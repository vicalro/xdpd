#include "ipv4_filter.h"

#include "../io/bufferpool.h"
#include "../io/datapacketx86.h"
#include "../io/packet_classifiers/pktclassifier.h"

using namespace xdpd::gnu_linux;

#ifdef COMPILE_IPV4_FRAG_FILTER_SUPPORT

void gnu_linux_frag_ipv4_pkt(datapacket_t** pkt, unsigned int mps, unsigned int* nof, datapacket_t** frags){

	unsigned int pend_bytes, frag_common_len;
	datapacketx86* pack = (datapacketx86*)(*pkt)->platform_state;
	datapacketx86* frag_pack;
	classifier_state* cs = &pack->clas_state;

	//Original pointer to ip
	cpc_ipv4_hdr_t* ipv4 =  (cpc_ipv4_hdr_t*)get_ipv4_hdr(cs, 0);
	assert(ipv4 != NULL);

	//Check DF bit
	if(has_ipv4_MF_bit_set(ipv4)){
		//Output a nice trace and drop
		static unsigned int dropped = 0;

		if( unlikely((dropped++%LOG_SUPRESSION_ITER) == 0) ){
			ROFL_WARN(DRIVER_NAME"WARNING: %u IPv4 packets exceeding MPS of an interface were discarded due to DF bit set.\n", dropped);
		}

		//Packet is not an IPv4 packet => DROP
		bufferpool::release_buffer(*pkt);
	}

	//Calculate sizes
	frag_common_len = (uint8_t*)ipv4 - (uint8_t*)pack->get_buffer() + (ipv4->ihlvers&0x0F)*4;
	pend_bytes = pack->get_buffer_length() - mps;

	//Reset number of fragments and set first one
	*nof = 0;
	frags[(*nof)++] = *pkt;

	//Set size&total length for the fragment 0
	pack->set_buffer_length(mps);
	///TODO

	//Recalculate pkt0 checksum
	//TODO

	assert(pend_bytes > 0);

	while(pend_bytes){

		//Acquire new packet
		frags[(*nof)] = bufferpool::get_free_buffer_nonblocking();
		if(unlikely(!frags[(*nof)])){
			//No buffers => Congestion => drop all fragments
			goto FRAG_ERROR;
		}

		frag_pack = (datapacketx86*)(frags[(*nof)])->platform_state;

		//Initialize new pkt (do not classify)
		frag_pack->init(pack->get_buffer(), frag_common_len, pack->lsw, cs->port_in, 0, false, true);

		//Set queue
		frag_pack->output_queue = pack->output_queue;

		//Calculate number of bytes of this fragment


		//Copy fragment chunk
		//memcpy(&pack->get_buffer())
		//TODO

		//Adjust size
		//pack->set_buffer_length(frag_common_len+frag_chunk);

		//Calculate IPv4 checksum
		cs->calculate_checksums_in_sw = RECALCULATE_IPV4_CHECKSUM_IN_SW; 

		//Increment nof and continue
		(*nof)++;
		if(unlikely(*nof >= IPV4_MAX_FRAG))
			goto FRAG_ERROR;
	}

	pkt = NULL;
	return;

FRAG_ERROR:
	while(*nof)
		bufferpool::release_buffer(frags[--(*nof)]);
	pkt = NULL;
}




hal_result_t gnu_linux_enable_ipv4_frag_filter(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);
	
	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	//TODO: remove and call proper enable/disable
	ls_int->ipv4_frag_filter_status = true;	

	ROFL_INFO(DRIVER_NAME" IPv4 fragmentation filter ENABLED.\n");
	
	return HAL_SUCCESS;
}

hal_result_t gnu_linux_disable_ipv4_frag_filter(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);
	
	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	//TODO: remove and call proper enable/disable
	ls_int->ipv4_frag_filter_status = false;	
	
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


#endif


#ifdef COMPILE_IPV4_REAS_FILTER_SUPPORT

hal_result_t gnu_linux_enable_ipv4_reas_filter(const uint64_t dpid){
	
	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);
	
	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	//TODO: remove and call proper enable/disable
	ls_int->ipv4_reas_filter_status = true;	
	
	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter ENABLED.\n");

	return HAL_SUCCESS;
}

hal_result_t gnu_linux_disable_ipv4_reas_filter(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);
	
	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	//TODO: remove and call proper enable/disable
	ls_int->ipv4_reas_filter_status = false; 
	
	ROFL_INFO(DRIVER_NAME" IPv4 reassembly filter DISABLED.\n");

	return HAL_SUCCESS;
}

bool gnu_linux_ipv4_reas_filter_status(const uint64_t dpid){

	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);
	
	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	
	return ls_int->ipv4_reas_filter_status;
}

#endif
