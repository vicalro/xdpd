#include "py.h"
#include <sstream>
#include <rofl/common/utils/c_logger.h>
#include "../../plugin_manager.h"
#include "../../switch_manager.h"

//Include python header
#include <python2.7/Python.h>

using namespace xdpd;

#define PLUGIN_NAME "py_plugin" 

void py::init(){

	PyObject *strret, *mymod, *strfunc, *strargs;

	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Loading...\n");

	Py_Initialize();    	
	PySys_SetPath((char*)"/home/marc/BISDN/code/github/xdpd//src/xdpd/management/plugins/py/"); // before ..
	mymod = PyImport_ImportModule("helloworld");


	strfunc = PyObject_GetAttrString(mymod, "myfunc");
	strargs = Py_BuildValue("(s)", "XXX");
	strret = PyEval_CallObject(strfunc, strargs);

	(void)strret;

	Py_Finalize();
	
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Loaded!\n");
}

//Events; print nice traces
void py::notify_port_added(const switch_port_snapshot_t* port_snapshot){
		if(port_snapshot->is_attached_to_sw)
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been ADDED to the system and attached to the switch with DPID: 0x%"PRIx64":%u. Administrative status: %s, link detected: %s\n", port_snapshot->name, port_snapshot->attached_sw_dpid, port_snapshot->of_port_num, (port_snapshot->up)? "UP":"DOWN", ((port_snapshot->state & PORT_STATE_LINK_DOWN) > 0)? "NO":"YES");
		else
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been ADDED to the system. Administrative status: %s, link detected: %s\n", port_snapshot->name, (port_snapshot->up)? "UP":"DOWN", ((port_snapshot->state & PORT_STATE_LINK_DOWN) > 0)? "NO":"YES");
}
	
void py::notify_port_attached(const switch_port_snapshot_t* port_snapshot){
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been ATTACHED to the switch with DPID: 0x%"PRIx64":%u.\n", port_snapshot->name, port_snapshot->attached_sw_dpid, port_snapshot->of_port_num);
}	

void py::notify_port_status_changed(const switch_port_snapshot_t* port_snapshot){
		if(port_snapshot->is_attached_to_sw)
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s attached to the switch with DPID: 0x%"PRIx64":%u has CHANGED ITS STATUS. Administrative status: %s, link detected: %s\n", port_snapshot->name, port_snapshot->attached_sw_dpid, port_snapshot->of_port_num, (port_snapshot->up)? "UP":"DOWN", ((port_snapshot->state & PORT_STATE_LINK_DOWN) > 0)? "NO":"YES");
		else
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has CHANGED ITS STATUS. Administrative status: %s, link detected: %s\n", port_snapshot->name, (port_snapshot->up)? "UP":"DOWN", ((port_snapshot->state & PORT_STATE_LINK_DOWN) > 0)? "NO":"YES");
}
		
void py::notify_port_detached(const switch_port_snapshot_t* port_snapshot){
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been DETACHED from the switch with DPID: 0x%"PRIx64":%u.\n", port_snapshot->name, port_snapshot->attached_sw_dpid, port_snapshot->of_port_num);
}	

void py::notify_port_deleted(const switch_port_snapshot_t* port_snapshot){
		if(port_snapshot->is_attached_to_sw)
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been REMOVED from the system and detached from the switch with DPID: 0x%"PRIx64":%u.\n", port_snapshot->name, port_snapshot->attached_sw_dpid, port_snapshot->of_port_num);
		else
			ROFL_INFO("[xdpd]["PLUGIN_NAME"] The port %s has been REMOVED from the system.\n", port_snapshot->name);
}
	
void py::notify_monitoring_state_changed(const monitoring_snapshot_state_t* monitoring_snapshot){
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Got an event of MONITORING STATE CHANGE\n");
}



