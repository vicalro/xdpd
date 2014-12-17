#include "ipv4_filter.h"

using namespace xdpd::gnu_linux;

#ifdef COMPILE_IPV4_FRAG_FILTER_SUPPORT 

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
