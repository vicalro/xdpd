#include "openflow_switch.h"

using namespace rofl;
using namespace xdpd;

openflow_switch::openflow_switch(const uint64_t dpid, const std::string &dpname, const of_version_t version, unsigned int num_of_tables) :
		endpoint(NULL),
		dpid(dpid),
		dpname(dpname),
		version(version),
		num_of_tables(num_of_tables)
{

}

/*
* Port notfications. Process them directly in the endpoint
*/
rofl_result_t openflow_switch::notify_port_attached(const switch_port_t* port){
	return endpoint->notify_port_attached(port);
}
rofl_result_t openflow_switch::notify_port_detached(const switch_port_t* port){
	return endpoint->notify_port_detached(port);
}
rofl_result_t openflow_switch::notify_port_status_changed(const switch_port_t* port){
	return endpoint->notify_port_status_changed(port);
}


/*
* Connecting and disconnecting from a controller entity
*/
void openflow_switch::rpc_connect_to_ctl(enum rofl::csocket::socket_type_t socket_type, cparams const& socket_params){
	rofl::openflow::cofhello_elem_versionbitmap versionbitmap;
	versionbitmap.add_ofp_version(version);
	endpoint->add_ctl(endpoint->get_idle_ctlid(), versionbitmap).connect(rofl::cauxid(0), socket_type, socket_params);
}

void openflow_switch::rpc_disconnect_from_ctl(rofl::cctlid ctlid){
	// TODO check for a valid ID
	//if (endpoint->has_ctl(ctlid)){
		endpoint->drop_ctl(ctlid);
	//}
	//else {
	//	throw eOfSmNotFound;
	//}
}

