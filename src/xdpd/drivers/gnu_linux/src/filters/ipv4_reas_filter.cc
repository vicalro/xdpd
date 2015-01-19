#include <rofl.h>
#include <sys/queue.h>
#include "../io/bufferpool.h"
#include "../io/datapacketx86.h"
#include "../io/packet_classifiers/pktclassifier.h"

#include "ipv4_filter.h"
#include "../util/khash.h"

//Make sure pipeline-imp are BEFORE _pp.h
//so that functions can be inlined
#include "../pipeline-imp/atomic_operations.h"
#include "../pipeline-imp/pthread_lock.h"
#include "../pipeline-imp/packet_inline.h"

#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_pipeline_pp.h>
#include <rofl/datapath/pipeline/openflow/of_switch_pp.h>

using namespace xdpd::gnu_linux;


//Reassembly routines

#ifdef COMPILE_IPV4_REAS_FILTER_SUPPORT

//Serialize enable/disable operations
static pthread_mutex_t reas_mutex = PTHREAD_MUTEX_INIT;

struct ipv4_reas_set{

	//Number of fragments
	unsigned int num_of_frags;

	//Fragments
	datapacket_t* [IPV4_MAX_FRAG];

	//Last seen
	time_t last_seen;

	//Linked lis state
	TAILQ_ENTRY(ipv4_reas_set) __ll;
}

//Fragment set pool
struct ipv4_reas_set_pool{

	//Total number of frag sets
	unsigned int capacity;

	//Number of frag sets used
	unsigned int used;

	//Linked list of free sets
	TAILQ_HEAD(,ipv4_reas_set) free;

	//Rwlock
	pthread_rwlock_t rwlock;
}ipv4_reas_set_pool_t;

//Key struct
struct ipv4_reas_key{
	uint32_t ip_dst;
	uint32_t id;
	char _null; //Null terminated to use as string
}ipv4_reas_key_t;

//Fragment set hash table type definition
//Using STR with null terminated string
KHASH_MAP_INIT_STR(ipv4_reas_set, ipv4_reas_key_t);


//Reassembly state
struct ipv4_reas_state{

	//Is system enabled
	bool enabled;

	//Fragment set pool
	ipv4_reas_set_pool_t pool;

	//Fragment hash table
	khash_t(ipv4_reas_set) *frags;

	//Expirations linked list (FIFO)
	TAILQ_HEAD(, ipv4_reas_set_pool) to_expire;
}ipv4_reas_state_t;

hal_result_t gnu_linux_enable_ipv4_reas_filter(const uint64_t dpid){

	hal_result_t ret = HAL_FAILURE;

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return ret;

	pthread_mutex_lock(&reas_mutex);

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	if(ls_int->reas_state != NULL && ls_int->reas_state->enabled){
		ROFL_INFO(DRIVER_NAME" IPv4 reassembly system critical error; corrupted state?.\n");
		assert(0); //Corrupted state
		goto IPV4_REAS_ENABLE_ERROR;
	}


	ls_int->ipv4_reas_filter_status = true;

	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter ENABLED.\n");

	pthread_mutex_unlock(&reas_mutex);

IPV4_REAS_ENABLE_ERROR:
	pthread_mutex_unlock(&reas_mutex);

	return ret;
}

hal_result_t gnu_linux_disable_ipv4_reas_filter(const uint64_t dpid){

	hal_result_t ret = HAL_FAILURE;

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return ret;

	pthread_mutex_lock(&reas_mutex);

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	ls_int->ipv4_reas_filter_status = false;

	if(ls_int->reas_state == NULL){
	if(ls_int->reas_state == NULL || !ls_int->reas_state->enabled){
		ROFL_INFO(DRIVER_NAME" IPv4 reassembly system critical error; corrupted state?.\n");
		assert(0); //Corrupted state
		goto IPV4_REAS_DISABLE_ERROR;
	}

	//Destroy (take write lock)


	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter DISABLED.\n");

	ret = HAL_SUCCESS;

IPV4_REAS_DISABLE_ERROR:
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


datapacket_t* gnu_linux_reas_ipv4_pkt(datapacket_t** pkt){

	//Throw

	*pkt = NULL;

	return NULL;
}



#endif //COMPILE_IPV4_REAS_FILTER_SUPPORT
