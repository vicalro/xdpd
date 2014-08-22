#ifndef PY_PLUGIN_H
#define PY_PLUGIN_H 

#include <rofl/common/ciosrv.h>
#include "../../plugin_manager.h"

/**
* @file py.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Exposes the C++ APIs as python calls via SWIG
* 
*/

namespace xdpd {

/**
* @brief Dummy management plugin py
* @ingroup cmm_mgmt_plugins
*/
class py:public plugin{
	
public:
	virtual void init(void);

	virtual std::string get_name(void){
		//Use code-name
		return std::string("py");
	};

	virtual void notify_port_added(const switch_port_snapshot_t* port_snapshot);
	virtual void notify_port_attached(const switch_port_snapshot_t* port_snapshot);
	virtual void notify_port_status_changed(const switch_port_snapshot_t* port_snapshot);
	virtual void notify_port_detached(const switch_port_snapshot_t* port_snapshot);
	virtual void notify_port_deleted(const switch_port_snapshot_t* port_snapshot);
	
	virtual void notify_monitoring_state_changed(const monitoring_snapshot_state_t* monitoring_snapshot);

private:
	

};

}// namespace xdpd 

#endif /* PY_PLUGIN_H_ */


