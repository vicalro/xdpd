#include "sm_ext.h"

#include <rofl/common/utils/c_logger.h>
#include "../switch_manager.h"
#include "../system_manager.h"

using namespace rofl;
using namespace xdpd;

/**
* Extensions
*/

//
// IPv4 fragmentation
//
void switch_manager_extensions::enable_ipv4_fragmentation(const uint64_t dpid){

	openflow_switch* dp;
	std::string name;
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.enable_ipv4_frag_filter)
		throw eOfSmNotSupportedByDriver();

	pthread_mutex_lock(&switch_manager::mutex);

	dp = switch_manager::__get_switch_by_dpid(dpid);
	if(!dp)
		throw eOfSmDoesNotExist();
	name = dp->dpname;

	if(ops->ipv4_frag_reas.enable_ipv4_frag_filter(dpid) != HAL_SUCCESS)
		throw eOfSmGeneralError();

	pthread_mutex_unlock(&switch_manager::mutex);

	ROFL_INFO("[xdpd][switch_manager][extensions] IPv4 fragmentation filter ENABLED for LSI %s with dpid 0x%llx\n", name.c_str(), (long long unsigned)dpid);
}

void switch_manager_extensions::disable_ipv4_fragmentation(const uint64_t dpid){

	openflow_switch* dp;
	std::string name;
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.disable_ipv4_frag_filter)
		throw eOfSmNotSupportedByDriver();

	pthread_mutex_lock(&switch_manager::mutex);

	dp = switch_manager::__get_switch_by_dpid(dpid);
	if(!dp)
		throw eOfSmDoesNotExist();
	name = dp->dpname;

	if(ops->ipv4_frag_reas.disable_ipv4_frag_filter(dpid) != HAL_SUCCESS)
		throw eOfSmGeneralError();

	pthread_mutex_unlock(&switch_manager::mutex);

	ROFL_INFO("[xdpd][switch_manager][extensions] IPv4 fragmentation filter DISABLED for LSI %s with dpid 0x%llx\n", name.c_str(), (long long unsigned)dpid);
}

bool switch_manager_extensions::ipv4_fragmentation_status(const uint64_t dpid){
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.ipv4_frag_filter_status)
		throw eOfSmNotSupportedByDriver();

	return ops->ipv4_frag_reas.ipv4_frag_filter_status(dpid);
}

//
// IPv4 reassembly
//

void switch_manager_extensions::enable_ipv4_reassembly(const uint64_t dpid){

	openflow_switch* dp;
	std::string name;
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.enable_ipv4_reas_filter)
		throw eOfSmNotSupportedByDriver();

	pthread_mutex_lock(&switch_manager::mutex);

	dp = switch_manager::__get_switch_by_dpid(dpid);
	if(!dp)
		throw eOfSmDoesNotExist();
	name = dp->dpname;

	if(ops->ipv4_frag_reas.enable_ipv4_reas_filter(dpid) != HAL_SUCCESS)
		throw eOfSmGeneralError();

	pthread_mutex_unlock(&switch_manager::mutex);

	ROFL_INFO("[xdpd][switch_manager][extensions] IPv4 reassembly filter ENABLED for LSI %s with dpid 0x%llx\n", name.c_str(), (long long unsigned)dpid);

}

void switch_manager_extensions::disable_ipv4_reassembly_filter(const uint64_t dpid){

	openflow_switch* dp;
	std::string name;
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.disable_ipv4_reas_filter)
		throw eOfSmNotSupportedByDriver();

	pthread_mutex_lock(&switch_manager::mutex);

	dp = switch_manager::__get_switch_by_dpid(dpid);
	if(!dp)
		throw eOfSmDoesNotExist();
	name = dp->dpname;


	if(ops->ipv4_frag_reas.disable_ipv4_reas_filter(dpid) != HAL_SUCCESS)
		throw eOfSmGeneralError();

	pthread_mutex_unlock(&switch_manager::mutex);

	ROFL_INFO("[xdpd][switch_manager][extensions] IPv4 reassembly filter DISABLED for LSI %s with dpid 0x%llx\n", name.c_str(), (long long unsigned)dpid);
}

bool switch_manager_extensions::ipv4_reassembly_status(const uint64_t dpid){
	hal_extension_ops_t* ops = system_manager::__get_driver_hal_extension_ops();

	if(!ops->ipv4_frag_reas.ipv4_reas_filter_status)
		throw eOfSmNotSupportedByDriver();

	return ops->ipv4_frag_reas.ipv4_reas_filter_status(dpid);
}

