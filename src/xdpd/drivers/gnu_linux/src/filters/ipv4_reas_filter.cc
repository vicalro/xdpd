#include <rofl.h>
#include <sys/queue.h>
#include <pthread.h>
#include "../config.h"
#include "../io/bufferpool.h"
#include "../io/datapacketx86.h"
#include "../io/packet_classifiers/pktclassifier.h"

#include "ipv4_filter.h"
#include "../util/khash.h"

//Make sure pipeline-imp are BEFORE _pp.h
//so that functions can be inlined
#include "../pipeline-imp/atomic_operations.h"
#include "../pipeline-imp/pthread_lock.h"

#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_pipeline_pp.h>
#include <rofl/datapath/pipeline/openflow/of_switch_pp.h>

using namespace xdpd::gnu_linux;


//Reassembly routines

#ifdef COMPILE_IPV4_REAS_FILTER_SUPPORT

static pthread_mutex_t reas_mutex = PTHREAD_MUTEX_INITIALIZER;

#define IPV4_REAS_HT_SIZE  2<<16
#define IPV4_REAS_FRAG_POOL_SIZE 1024*4

//Key struct
typedef struct ipv4_reas_key{
	uint32_t ip_dst;
	uint32_t id;
}ipv4_reas_key_t;
COMPILER_ASSERT(ipv4_reas_key_size, sizeof(ipv4_reas_key_t) == 8);

/**
* Inspired by the algorithm defined in RFC815
*/
#define IPV4_REAS_MAX_LAST 0xFFFFFFFF
struct ipv4_reas_hole{
	unsigned int start;
	unsigned int end;
};

//fwd decl
struct ipv4_reas_ht_entry;

typedef struct ipv4_reas_set{
	//Key
	struct ipv4_reas_key key;

	//Entry
	struct ipv4_reas_ht_entry *entry;

	//Holes
	unsigned int num_of_holes;
	struct ipv4_reas_hole holes[IPV4_MAX_FRAG];

	//Partially reassembled packet
	datapacket_t* pkt;

	//IPv4 offset from the beginning of the pkt
	cpc_ipv4_hdr_t* ipv4_hdr;

	//IPv4 payload offset from the beginning of the pkt
	unsigned int ipv4_payload_offset;

	//Last seen
	time_t last_seen;

	//Mutex for the set
	pthread_mutex_t mutex;

	//Linked list states
	TAILQ_ENTRY(ipv4_reas_set) __pool_ll;
	TAILQ_ENTRY(ipv4_reas_set) __bucket_ll;
}ipv4_reas_set_t;

//Fragment set pool
typedef struct ipv4_reas_set_pool{
	//Total number of frag sets
	unsigned int capacity;

	//Number of frag sets used
	unsigned int used;

	//Linked list of free sets
	TAILQ_HEAD(,ipv4_reas_set) free;

	//Rwlock
	pthread_mutex_t mutex;
}ipv4_reas_set_pool_t;


//Hashtable entry
typedef struct ipv4_reas_ht_entry{
	TAILQ_HEAD(,ipv4_reas_set) buckets;

	//Rwlock
	pthread_rwlock_t rwlock;
}ipv4_reas_ht_entry_t;

//Reassembly state
typedef struct ipv4_reas_state{
	//Fragment set pool
	ipv4_reas_set_pool_t pool;

	//Fragment hash table (pre-allocated)
	ipv4_reas_ht_entry_t frag_ht[IPV4_REAS_HT_SIZE];

	//Expirations linked list (FIFO)
	pthread_mutex_t mutex;
	TAILQ_HEAD(,ipv4_reas_set) to_expire;

	//Rwlock
	pthread_rwlock_t rwlock;
}ipv4_reas_state_t;

//Internals
static ipv4_reas_state_t* gnu_linux_init_ipv4_reas(){

	unsigned int i;
	ipv4_reas_state_t* state;
	ipv4_reas_ht_entry_t* entry;
	ipv4_reas_set_t* set;

	state = (ipv4_reas_state_t*)malloc(sizeof(ipv4_reas_state_t));

	if(!state){
		goto REAS_INIT_END;
	}

	//Initialize pool
	state->pool.capacity = IPV4_REAS_FRAG_POOL_SIZE;
	state->pool.used = 0;
	TAILQ_INIT(&state->pool.free);
	for(i=0;i<IPV4_REAS_FRAG_POOL_SIZE;i++){
		set = (ipv4_reas_set_t*)malloc(sizeof(ipv4_reas_set_t));
		if(unlikely(!set)){
			//TODO: unwind
			free(state);
			state = NULL;
			goto REAS_INIT_END;
		}

		//Init mutex
		pthread_mutex_init(&set->mutex, NULL);

		//Add to the list
		TAILQ_INSERT_TAIL(&state->pool.free, set, __pool_ll);
	}
	pthread_mutex_init(&state->pool.mutex, NULL);

	//Initialize HT
	for(i=0;i<IPV4_REAS_HT_SIZE;i++){
		entry = &state->frag_ht[i];
		pthread_rwlock_init(&entry->rwlock, NULL);
		TAILQ_INIT(&entry->buckets);
	}

	//Initilize tailq for expirations
	TAILQ_INIT(&state->to_expire);

	//rwlock
	pthread_rwlock_init(&state->rwlock, NULL);

	//mutex
	pthread_mutex_init(&state->mutex, NULL);

REAS_INIT_END:
	return state;
}

static void gnu_linux_destroy_ipv4_reas(ipv4_reas_state_t* state){

	ipv4_reas_set_t* set;

	//Wait for all possible pending-fragmentation threads using the
	//infrastructure. Note that no more threads will be able to use it,
	//since status is already set to false
	pthread_rwlock_wrlock(&state->rwlock);

	//Destroy all sets in to_expire (used)
	while (!TAILQ_EMPTY(&state->to_expire)) {
		set = TAILQ_FIRST(&state->to_expire);
		TAILQ_REMOVE(&state->to_expire, set, __pool_ll);
		free(set);
	}

	//Destroy remaning sets in the pool
	while (!TAILQ_EMPTY(&state->pool.free)) {
		set = TAILQ_FIRST(&state->pool.free);
		TAILQ_REMOVE(&state->pool.free, set, __pool_ll);
		free(set);
	}
	free(state);
}

//hash algorithm (fnv1a)
#define FRAG_HASH_FNV_MASK_16 0xFFFF
#define FRAG_HASH_FNV_PRIME 0x01000193 //16777619
#define FRAG_HASH_FNV_SEED 0x811C9DC5 //2166136261


__attribute__ ((always_inline)) static inline uint32_t frag_hash_fnv1a(unsigned char c, uint32_t hash){
	return (c ^ hash) * FRAG_HASH_FNV_PRIME;
}
/// hash a single byte
__attribute__ ((always_inline)) static inline uint16_t frag_hash(const char* key){

	uint32_t hash = FRAG_HASH_FNV_SEED;

	hash = frag_hash_fnv1a( key[0], hash);
	hash = frag_hash_fnv1a( key[1], hash);
	hash = frag_hash_fnv1a( key[2], hash);
	hash = frag_hash_fnv1a( key[3], hash);
	hash = frag_hash_fnv1a( key[4], hash);
	hash = frag_hash_fnv1a( key[5], hash);
	hash = frag_hash_fnv1a( key[6], hash);
	hash = frag_hash_fnv1a( key[7], hash);

	//Fold
	return  (hash>>16) ^ (hash & FRAG_HASH_FNV_MASK_16);
}


static ipv4_reas_set_t* get_or_init_frag_set(ipv4_reas_state_t* state, cpc_ipv4_hdr_t* ipv4){
	struct ipv4_reas_key key;
	uint16_t ht_index;
	ipv4_reas_ht_entry_t* entry;
	ipv4_reas_set_t *set;

	//Compose key
	key.ip_dst = *get_ipv4_dst(ipv4);
	key.id = *get_ipv4_ident(ipv4);

	//Hash it
	ht_index = frag_hash((const char*)&key);

	//Recover
	entry = &state->frag_ht[ht_index];

	pthread_rwlock_rdlock(&entry->rwlock);
	//Look for the right bucket
	TAILQ_FOREACH(set, &entry->buckets, __bucket_ll) {
		if( (*(uint64_t*)&set->key) == (*((uint64_t*)&key))){
			pthread_mutex_lock(&set->mutex);
			break;
		}
	}
	pthread_rwlock_unlock(&entry->rwlock);

	if(!set){
		//Allocate a new set
		pthread_mutex_lock(&state->pool.mutex);
		set = TAILQ_FIRST(&state->pool.free);
		if(likely(set != NULL)){
			//Remove from the pool, and quickly release
			TAILQ_REMOVE( &state->pool.free, set, __pool_ll);
			pthread_mutex_unlock(&state->pool.mutex);

			//Add it to the HT entry
			pthread_rwlock_wrlock(&entry->rwlock);
			TAILQ_INSERT_TAIL(&entry->buckets, set, __bucket_ll);
			pthread_rwlock_unlock(&entry->rwlock);

			//Mark set as being used
			pthread_mutex_lock(&set->mutex);
			set->num_of_holes = 0;
			set->pkt = NULL;
			set->key = key;
			set->entry = entry;
		}else{
			pthread_mutex_unlock(&state->pool.mutex);
		}

	}
	return set;
}

static void release_frag_set(ipv4_reas_state_t* state, ipv4_reas_set_t* set){
	//Remove from the HT
	pthread_rwlock_wrlock(&set->entry->rwlock);
	TAILQ_REMOVE(&set->entry->buckets, set, __bucket_ll);
	pthread_rwlock_unlock(&set->entry->rwlock);

	//Remove from expire list
	pthread_mutex_lock(&state->mutex);
	TAILQ_REMOVE(&state->to_expire, set, __pool_ll);
	pthread_mutex_unlock(&state->mutex);

	//Put it back to the pool
	pthread_mutex_lock(&state->pool.mutex);
	TAILQ_INSERT_TAIL(&state->pool.free, set, __pool_ll);
	pthread_mutex_unlock(&state->pool.mutex);
}

//API calls
hal_result_t gnu_linux_enable_ipv4_reas_filter(const uint64_t dpid){

	hal_result_t ret = HAL_FAILURE;

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return ret;
	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	pthread_mutex_lock(&reas_mutex);

	if(ls_int->ipv4_reas_filter_status){
		ROFL_INFO(DRIVER_NAME" IPv4 reassembly system already running!\n");
		assert(0);
		ret = HAL_SUCCESS;
		goto IPV4_REAS_ENABLE_END;
	}

	assert(ls_int->reas_state == NULL);

	//Init reas state
	ls_int->reas_state = gnu_linux_init_ipv4_reas();
	if(unlikely(!ls_int->reas_state)){
		ROFL_INFO(DRIVER_NAME" IPv4 reassembly system could not be initialized; out of memory?.\n");
		assert(0);
		ret = HAL_SUCCESS;
		goto IPV4_REAS_ENABLE_END;
	}

	ls_int->ipv4_reas_filter_status = true;

	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter ENABLED.\n");
	ret = HAL_SUCCESS;

IPV4_REAS_ENABLE_END:
	pthread_mutex_unlock(&reas_mutex);

	return ret;
}

hal_result_t gnu_linux_disable_ipv4_reas_filter(const uint64_t dpid){

	hal_result_t ret = HAL_FAILURE;

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return ret;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	pthread_mutex_lock(&reas_mutex);

	if(!ls_int->ipv4_reas_filter_status){
		ROFL_INFO(DRIVER_NAME" IPv4 reassembly system already disabled!\n");
		assert(0);
		ret = HAL_SUCCESS;
		goto IPV4_REAS_DISABLE_END;
	}

	ls_int->ipv4_reas_filter_status = false;
	gnu_linux_destroy_ipv4_reas(ls_int->reas_state);
	ls_int->reas_state = NULL;

	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter DISABLED.\n");
	ret = HAL_SUCCESS;

IPV4_REAS_DISABLE_END:
	pthread_mutex_unlock(&reas_mutex);

	return ret;
}

bool gnu_linux_ipv4_reas_filter_status(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	return ls_int->ipv4_reas_filter_status;
}



datapacket_t* gnu_linux_reas_ipv4_pkt(of_switch_t* sw, datapacket_t** pkt, cpc_ipv4_hdr_t* ipv4){

	ipv4_reas_set_t* set;
	datapacket_t* reas_pkt=NULL;
	uint8_t *dst, *src;
	unsigned int chunk_len, chunk_offset, i;

	datapacketx86* pack_reas;
	datapacketx86* pack = (datapacketx86*)(*pkt)->platform_state;
	switch_platform_state_t* ls_int = (switch_platform_state_t*)sw->platform_state;

	pthread_rwlock_rdlock(&ls_int->reas_state->rwlock);

	//Get the fragment for the hash
	set = get_or_init_frag_set(ls_int->reas_state, ipv4);
	if(unlikely(!set)){
		//Out of sets
		assert(0);
		goto IPV4_REAS_END;
	}

	///Check if we are the first ones
	if(!set->pkt){
		//We will hold the entire pkt
		set->pkt = *pkt;

		//Set common elements of the fragments
		set->ipv4_payload_offset = ((uint8_t*)ipv4 + (ipv4->ihlvers&0x0F)*4) - pack->get_buffer();
		set->ipv4_hdr = ipv4;

		//Recover chunk length
		chunk_len = pack->get_buffer_length() - set->ipv4_payload_offset;
		chunk_offset = (gnu_linux_ipv4_get_offset(ipv4)*8);

		ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Starting packet %p reassembly total length:%u, chunk length: %u offset: %u.\n", *pkt, pack->get_buffer_length(), chunk_len, chunk_offset);
		//Set holes and memcpy
		if(unlikely(gnu_linux_ipv4_get_offset(ipv4) != 0)){
			//Move chunk in the unlikely case we are not the first
			//fragment (reorderings)
			src = pack->get_buffer() + set->ipv4_payload_offset;
			dst = src + (gnu_linux_ipv4_get_offset(ipv4)*8);
			memcpy(dst, src, chunk_len);

			if(has_ipv4_MF_bit_set(ipv4)){
				//Not the last fragment
				// start->offset
				set->holes[0].start = 0;
				set->holes[0].end = chunk_offset;

				// offset+chunk_frag->END
				set->holes[1].start = chunk_offset+chunk_len;
				set->holes[1].end = IPV4_REAS_MAX_LAST;

				set->num_of_holes = 2;
			}else{
				//Create a single hole start->(END-offset)
				set->holes[0].start = 0;
				set->holes[0].end = IPV4_REAS_MAX_LAST;

				set->num_of_holes = 1;
			}
		}else{
			assert(has_ipv4_MF_bit_set(ipv4) == true);
			set->holes[0].start = chunk_len;
			set->holes[0].end = IPV4_REAS_MAX_LAST;
			set->num_of_holes = 1;
		}

		//Set offset to 0
		gnu_linux_ipv4_set_offset(ipv4, 0);
	}else{
		//Recover chunk length
		chunk_len = pack->get_buffer_length() - set->ipv4_payload_offset;
		chunk_offset = (gnu_linux_ipv4_get_offset(ipv4)*8);
		pack_reas = (datapacketx86*)set->pkt->platform_state;

		ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Assembling fragment for packet %p (fragment: %p) total length:%u, chunk length: %u offset: %u.\n", set->pkt, *pkt, pack->get_buffer_length(), chunk_len, chunk_offset);
		//Find fragment
		for(i=0;i<set->num_of_holes;i++){
			if(set->holes[i].start <= chunk_offset &&
				set->holes[i].end > (chunk_offset+chunk_len))
				break;
		}

		if(unlikely(i == IPV4_MAX_FRAG)){
			//Abnormal fragment
#ifdef ASSERT_PKT_IPV4_REAS_ABNORMAL
			assert(0);
#endif
			//Cleanup
			bufferpool::release_buffer(*pkt);
			bufferpool::release_buffer(set->pkt);
			pthread_mutex_unlock(&set->mutex);
			release_frag_set(ls_int->reas_state, set);

			//Remove for the expirations list
			pthread_mutex_lock(&ls_int->reas_state->mutex);
			TAILQ_REMOVE(&ls_int->reas_state->to_expire, set, __pool_ll);
			pthread_mutex_unlock(&ls_int->reas_state->mutex);

			goto IPV4_REAS_END;
		}

		//Check if it is the next consecutive fragment
		if(likely(i==0)){
			//Add length to the packet
			pack_reas->set_buffer_length(pack_reas->get_buffer_length()+chunk_len);

			//Memcpy
			src = pack->get_buffer() + set->ipv4_payload_offset;
			dst = pack_reas->get_buffer()+set->ipv4_payload_offset+chunk_offset;
			memcpy(dst, src, chunk_len);

			//Hole state
			if(has_ipv4_MF_bit_set(ipv4)){
				//Readjust hole
				set->holes[i].start = set->ipv4_payload_offset+chunk_offset;
			}else{
				//last fragment
				set->num_of_holes--;
			}
		}else{
			assert(0);
		}

		//Add chunk len to the total length
		set->ipv4_hdr->length = HTONB16((NTOHB16(set->ipv4_hdr->length) + chunk_len));

		//Drop partial fragment
		bufferpool::release_buffer(*pkt);
	}

	//If it is the last fragments
	if(!set->num_of_holes){
		//Set the reas_pkt
		reas_pkt = set->pkt;

		//Return set
		release_frag_set(ls_int->reas_state, set);
		pthread_mutex_unlock(&set->mutex);

		//Reclassify the packet
		classify_packet(&pack_reas->clas_state, pack_reas->get_buffer(), pack_reas->get_buffer_length(), pack_reas->clas_state.port_in, 0);

		set_recalculate_checksum(&pack_reas->clas_state, RECALCULATE_IPV4_CHECKSUM_IN_SW);
		ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Successfully reassembled packet %p total length:%u.\n", reas_pkt, pack_reas->get_buffer_length());
		goto IPV4_REAS_END;
	}else{
		//Reeschedule set for expirations
		set->last_seen = time(NULL);

		//Remove for the expirations list and ad to tail
		pthread_mutex_lock(&ls_int->reas_state->mutex);
		TAILQ_REMOVE(&ls_int->reas_state->to_expire, set, __pool_ll);
		TAILQ_INSERT_TAIL(&ls_int->reas_state->to_expire, set, __pool_ll);
		pthread_mutex_unlock(&ls_int->reas_state->mutex);
	}
	pthread_mutex_unlock(&set->mutex);

IPV4_REAS_END:
	pthread_rwlock_unlock(&ls_int->reas_state->rwlock);
	*pkt = NULL;
	return reas_pkt;
}

void gnu_linux_reas_ipv4_expire_frag_sets(of_switch_t* sw){

	unsigned int expired = 0;
	ipv4_reas_set_t *it;
	time_t now = time(NULL);
	double diff;

	//Prevent disabling of the reas filter while expiration execution
	pthread_mutex_lock(&reas_mutex);

	//State
	ipv4_reas_state_t* state = ((switch_platform_state_t*)sw->platform_state)->reas_state;

	if(unlikely(state == NULL))
		goto IP_FRAG_EXPIRE_END;

	ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Performing partially reassembled packet expirations for LSI %s.\n", sw->name);
	//Read lock
	pthread_rwlock_rdlock(&state->rwlock);
	pthread_mutex_lock(&state->mutex);
	TAILQ_FOREACH(it, &state->to_expire, __pool_ll) {
		//Try to lock,
		if(pthread_mutex_trylock(&it->mutex) != 0){
			//Being used, skip
			continue;
		}
		//Check if it expired
		diff = difftime(now,it->last_seen);
		if(unlikely(diff >= IPV4_REAS_FRAG_TIMEOUT_S)){
			ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Partially reassembled packet %p (curr. computed total length:%u) expired.\n", it->pkt, ((datapacketx86*)it->pkt->platform_state)->get_buffer_length());
			//Release frag
			assert(it->pkt);
			bufferpool::release_buffer(it->pkt);
			release_frag_set(state, it);
			pthread_mutex_unlock(&it->mutex);
			expired++;
		}else{
			//All other sets are newer, break
			pthread_mutex_unlock(&it->mutex);
			break;
		}
	}
	pthread_mutex_unlock(&state->mutex);
	pthread_rwlock_unlock(&state->rwlock);
IP_FRAG_EXPIRE_END:
	pthread_mutex_unlock(&reas_mutex);
	ROFL_DEBUG(DRIVER_NAME"[ipv4_reas_filter] Partially reassembled packets expired for LSI '%s':%u.\n", sw->name, expired);
}

#endif //COMPILE_IPV4_REAS_FILTER_SUPPORT
