#include <rofl.h>
#include "../io/bufferpool.h"
#include "../io/datapacketx86.h"
#include "../io/packet_classifiers/pktclassifier.h"

//Make sure pipeline-imp are BEFORE _pp.h
//so that functions can be inlined
#include "../pipeline-imp/atomic_operations.h"
#include "../pipeline-imp/pthread_lock.h"
#include "../pipeline-imp/packet.h"

#include "ipv4_filter.h"

#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_pipeline_pp.h>
#include <rofl/datapath/pipeline/openflow/of_switch_pp.h>

//Extern C
ROFL_BEGIN_DECLS

//Checksums calculator
extern void calculate_checksums_in_software(datapacket_t* pkt);

//Extern C
ROFL_END_DECLS

using namespace xdpd::gnu_linux;

#ifdef COMPILE_IPV4_FRAG_FILTER_SUPPORT

hal_result_t gnu_linux_enable_ipv4_frag_filter(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	ls_int->ipv4_frag_filter_status = true;

	ROFL_INFO(DRIVER_NAME" IPv4 fragmentation filter ENABLED.\n");

	return HAL_SUCCESS;
}

hal_result_t gnu_linux_disable_ipv4_frag_filter(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	ls_int->ipv4_frag_filter_status = false;
	__sync_synchronize();

	ROFL_INFO(DRIVER_NAME" IPv4 fragmentation filter DISABLED.\n");

	return HAL_SUCCESS;
}

bool gnu_linux_ipv4_frag_filter_status(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	return ls_int->ipv4_frag_filter_status;
}

void gnu_linux_frag_ipv4_pkt(datapacket_t** pkt, unsigned int mps, unsigned int* nof, datapacket_t** frags){

	unsigned int i, payload_proc_len, payload_total_len, frag_common_len, ipv4_len;
	int frag_chunk;
	datapacketx86* pack = (datapacketx86*)(*pkt)->platform_state;
	datapacketx86* frag_pack;
	classifier_state* cs = &pack->clas_state;
	classifier_state* frag_cs;

	//Original pointer to ip
	cpc_ipv4_hdr_t* ipv4 =  (cpc_ipv4_hdr_t*)get_ipv4_hdr(cs, 0);
	assert(ipv4 != NULL);

	ROFL_DEBUG(DRIVER_NAME"[ipv4_frag_filter] Starting to fragment packet %p, MPS: %u, total length:%u.\n", *pkt, mps, pack->get_buffer_length());

	//Reset number of fragments and set first one
	*nof = 0;

	//Check DF bit
	if(has_ipv4_DF_bit_set(ipv4)){
		//Output a nice trace and drop
		static unsigned int dropped = 0;

		if( unlikely((dropped++%LOG_SUPRESSION_ITER) == 0) ){
			ROFL_WARN(DRIVER_NAME"WARNING: %u IPv4 packets exceeding MPS of an interface were discarded due to DF bit set.\n", dropped);
		}

#ifdef ASSERT_PKT_EXCEEDS_MTU_DF
		assert(0);
#endif

		//Packet is not an IPv4 packet => DROP
		bufferpool::release_buffer(*pkt);
		goto FRAG_ERROR;
	}

	//Calculate sizes
	ipv4_len =  (ipv4->ihlvers&0x0F)*4;
	frag_common_len = (uint8_t*)ipv4 - (uint8_t*)pack->get_buffer() + ipv4_len;
	payload_total_len = pack->get_buffer_length()-frag_common_len;
	payload_proc_len = 0;

	ROFL_DEBUG(DRIVER_NAME"[ipv4_frag_filter] Packet %p, Ethernet+IPv4 length:%u.\n", *pkt, frag_common_len);
	do{
		if(*nof == 0){
			frags[(*nof)] = *pkt;
		}else{
			frags[(*nof)] = bufferpool::get_free_buffer_nonblocking();
		}
		if(unlikely(!frags[(*nof)])){
			//No buffers => Congestion => drop all fragments
			(*nof)++;
			goto FRAG_ERROR;
		}
		//Helpers
		frag_pack = (datapacketx86*)(frags[(*nof)])->platform_state;
		frag_cs = &frag_pack->clas_state;

		*frag_cs =  *cs;
		if(*nof){
			//Initialize new pkt (do not classify)
			frag_pack->init(pack->get_buffer(), frag_common_len, pack->lsw, cs->port_in, 0, false, true);

			//Set queue
			frag_pack->output_queue = pack->output_queue;
		}

		//Calculate number of bytes of this fragment(rounded to power of 8)
		frag_chunk = (mps-frag_common_len);
		if(payload_total_len < (payload_proc_len+frag_chunk))
			frag_chunk = payload_total_len-payload_proc_len;
		else
			frag_chunk &= ~(0x7);

		//Copy fragment chunk
		if(*nof)
			memcpy(frag_pack->get_buffer()+frag_common_len, pack->get_buffer()+frag_common_len+payload_proc_len, frag_chunk);

		//Readjust size and IPv4 header
		frag_pack->set_buffer_length(frag_common_len+frag_chunk);
		ipv4 = (cpc_ipv4_hdr_t*)get_ipv4_hdr(frag_cs, 0);
		if((frag_chunk+payload_proc_len) != payload_total_len)
			set_ipv4_MF_bit(ipv4);
		else
			clear_ipv4_MF_bit(ipv4);

		//Set fragment total length
		ipv4->length = HTONB16(ipv4_len+frag_chunk);

		gnu_linux_ipv4_set_offset(ipv4, gnu_linux_ipv4_get_offset(ipv4) + payload_proc_len/8);

		ROFL_DEBUG(DRIVER_NAME"[ipv4_frag_filter] Generating fragment for pkt %p num: %u(%p), pkt length: %u, frag payload chunk: %u, processed: %u, frame starts at %p\n", *pkt, *nof, frags[(*nof)], frag_pack->get_buffer_length(), frag_chunk, payload_proc_len, frag_pack->get_buffer());

		assert(memcmp(pack->get_buffer(), frag_pack->get_buffer(), 14) == 0);

		//Mark to calculate the checksum
		frag_cs->calculate_checksums_in_sw = 0;
		set_recalculate_checksum(frag_cs, RECALCULATE_IPV4_CHECKSUM_IN_SW);

		//Decrement pending bytes
		payload_proc_len += frag_chunk;

		//Increment nof and continue
		(*nof)++;
		if(unlikely(*nof >= IPV4_MAX_FRAG))
			goto FRAG_ERROR;

	}while(payload_total_len > payload_proc_len);

	assert(payload_proc_len == payload_total_len);

	//Now that all fragments had no errors
	//recalculate checksums
	for(i=0;i<*nof;++i)
		calculate_checksums_in_software(frags[i]);

	ROFL_DEBUG(DRIVER_NAME"[ipv4_frag_filter] Successful fragmentation for pkt: %p. Resulting number of fragments: %u\n", *pkt, *nof);
#ifdef DEBUG
	//Cleanup and leave
	pkt = NULL;
#endif

	return;

FRAG_ERROR:
	while(*nof)
		bufferpool::release_buffer(frags[--(*nof)]);
#ifdef DEBUG
	pkt = NULL;
#endif
}

#endif //COMPILE_IPV4_FRAG_FILTER_SUPPORT
