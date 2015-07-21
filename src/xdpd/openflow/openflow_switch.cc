#include "openflow_switch.h"
#include "../management/snapshots/controller_snapshot.h"

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
uint64_t openflow_switch::rpc_connect_to_ctl(enum rofl::csocket::socket_type_t socket_type, cparams const& socket_params){
	rofl::openflow::cofhello_elem_versionbitmap versionbitmap;
	versionbitmap.add_ofp_version(version);
	rofl::crofctl &ctl = endpoint->add_ctl(endpoint->get_idle_ctlid(), versionbitmap);
	ctl.connect(rofl::cauxid(0), socket_type, socket_params);
	
	return ctl.get_ctlid().get_ctlid();
}

void openflow_switch::rpc_disconnect_from_ctl(rofl::cctlid ctlid){
	//NOTE need to check for existance? if (endpoint->has_ctl(ctlid))
		endpoint->drop_ctl(ctlid);
}

void openflow_switch::rpc_list_ctls(std::list<rofl::cctlid> *ctls_list){
	endpoint->list_ctls(ctls_list);
}

void openflow_switch::get_controller_info(uint64_t ctl_id, controller_snapshot& ctl_info){

	// Get Controller
	rofl::cctlid ctlid(ctl_id);
	const rofl::crofctl &ctl = endpoint->get_ctl(ctlid);

	// Get info for snapshot (Role, Status, SSL, IP, port)
	ctl_info.id = ctl_id;
	// Role Master / Slave
	switch (ctl.get_role().get_role()){
		// WARNING the openflow version specific types should be abstracted from the API
		case rofl::openflow12::OFPCR_ROLE_EQUAL:
			ctl_info.role = controller_snapshot::CONTROLLER_MODE_EQUAL;
			break;
		case rofl::openflow12::OFPCR_ROLE_SLAVE:
			ctl_info.role = controller_snapshot::CONTROLLER_MODE_SLAVE;
			break;
		case rofl::openflow12::OFPCR_ROLE_MASTER:
			ctl_info.role = controller_snapshot::CONTROLLER_MODE_MASTER;
			break;
		default:
			//TODO error
			break;
	}

	// Status
	ctl_info.connected = ctl.is_established();

	// lets see what is in each connection.
	std::list<rofl::cauxid> conn_list = ctl.get_conn_index();
	for(std::list<rofl::cauxid>::iterator it = conn_list.begin(); it != conn_list.end(); it++){

		controller_conn_snapshot conn;
		
		conn.id = it->get_id();

		// Connection type plain/ssl
		rofl::csocket::socket_type_t stype = ctl.rofchan.get_conn(*it).get_rofsocket().get_socket().get_socket_type();
		switch (stype){
			case rofl::csocket::SOCKET_TYPE_UNKNOWN:
				conn.proto_type = controller_conn_snapshot::PROTOCOL_UNKNOWN;
				break;
			case rofl::csocket::SOCKET_TYPE_PLAIN:
				conn.proto_type = controller_conn_snapshot::PROTOCOL_PLAIN;
				break;
			case rofl::csocket::SOCKET_TYPE_OPENSSL:
				conn.proto_type = controller_conn_snapshot::PROTOCOL_SSL;
				break;
			default:
				assert(0);
				break;
		}

		rofl::cparams sparams = ctl.rofchan.get_conn(*it).get_rofsocket().get_socket().get_socket_params();

		// Hostname / IP address
		rofl::cparam hostname = sparams.get_param(rofl::csocket::PARAM_KEY_REMOTE_HOSTNAME);
		conn.ip = hostname.get_string();

		// Port
		conn.port = sparams.get_param(rofl::csocket::PARAM_KEY_REMOTE_PORT).get_uint();

		// Add connection to connection array
		ctl_info.conn_list.push_back(conn);

	}
}



