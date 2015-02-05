#include <rofl/datapath/pipeline/openflow/of_switch_pp.h>
#include "ip_far.h"

#include "../processing/ls_internal_state.h"


namespace xdpd{
namespace gnu_linux_dpdk{

#ifdef COMPILE_IP_FRAG_FILTER_SUPPORT
hal_result_t gnu_linux_dpdk_enable_ip_frag_filter(const uint64_t dpid){
	
	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	ls_int->ip_frag_filter_status = true;

	ROFL_INFO(DRIVER_NAME" IP fragmentation filter ENABLED.\n");
	
	return HAL_SUCCESS;
}
hal_result_t gnu_linux_dpdk_disable_ip_frag_filter(const uint64_t dpid){
	
	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;
	ls_int->ip_frag_filter_status = false;
	//WARNING __sync_synchronize();

	ROFL_INFO(DRIVER_NAME" IP fragmentation filter DISABLED.\n");
	
	return HAL_SUCCESS;
}
bool gnu_linux_dpdk_ip_frag_filter_status(const uint64_t dpid){
	
	of_switch_t* sw = physical_switch_get_logical_switch_by_dpid(dpid);

	if(!sw)
		return HAL_FAILURE;

	switch_platform_state_t* ls_int =  (switch_platform_state_t*)sw->platform_state;

	return ls_int->ip_frag_filter_status;
}

#endif

hal_result_t gnu_linux_dpdk_enable_ip_reas_filter(const uint64_t dpid){
	return HAL_SUCCESS;
}
hal_result_t gnu_linux_dpdk_disable_ip_reas_filter(const uint64_t dpid){
	return HAL_SUCCESS;
}
bool gnu_linux_dpdk_ip_reas_filter_status(const uint64_t dpid){
	return true;
}

} // namespace xdpd
} // namespace gnu_linux_dpdk

/*


#ifdef COMPILE_IP_REAS_FILTER_SUPPORT

hal_result_t gnu_linux_dpdk_enable_ip_reas_filter(const uint64_t dpid){
	
}


hal_result_t gnu_linux_dpdk_disable_ip_reas_filter(const uint64_t dpid){
	
}


hal_result_t gnu_linux_dpdk_ip_reas_filter_status(const uint64_t dpid){
	
}

#endif
*/
