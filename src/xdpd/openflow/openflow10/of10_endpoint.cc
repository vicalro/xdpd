/*
 * of10_endpoint.cc
 *
 *  Created on: 06.09.2013
 *      Author: andreas
 */

#include "of10_endpoint.h"

#include <rofl/datapath/hal/driver.h>
#include <rofl/common/utils/c_logger.h>
#include "of10_translation_utils.h"
#include "../../management/system_manager.h"

using namespace rofl;
using namespace xdpd;

/*
* Constructor and destructor
*/
of10_endpoint::of10_endpoint(
		openflow_switch* sw,
		int reconnect_start_timeout,
		const rofl::openflow::cofhello_elem_versionbitmap& versionbitmap,
		enum rofl::csocket::socket_type_t socket_type,
		cparams const& socket_params) throw (eOfSmErrorOnCreation) :
				of_endpoint(versionbitmap, socket_type, socket_params) {


	//Reference back to the sw
	this->sw = sw;

	//Connect to the main controller
	crofbase::add_ctl(rofl::cctlid(0), versionbitmap).connect(rofl::cauxid(0), socket_type, socket_params);
}


/*
*
* Handling endpoint messages routines
*
*/



void
of10_endpoint::handle_features_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_features_request& msg)
{
	logical_switch_port_t* ls_port;
	switch_port_snapshot_t* _port;
	uint32_t supported_actions;

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();
		
	uint32_t num_of_tables 	= 0;
	uint32_t num_of_buffers = 0;
	uint32_t capabilities 	= 0;

	num_of_tables 	= of10switch->pipeline.num_of_tables;
	num_of_buffers 	= of10switch->pipeline.num_of_buffers;
	capabilities 	= of10switch->pipeline.capabilities;

	// array of structures ofp_port
	rofl::openflow::cofports ports(ctl.get_version_negotiated());

	//we check all the positions in case there are empty slots
	for (unsigned int n = 1; n < of10switch->max_ports; n++){

		ls_port = &of10switch->logical_ports[n];
		_port = ls_port->port;

		if(_port!=NULL && ls_port->attachment_state!=LOGICAL_PORT_STATE_DETACHED){

			//Mapping of port state
			assert(n == _port->of_port_num);

			rofl::openflow::cofport port(ctl.get_version_negotiated());

			port.set_port_no(_port->of_port_num);
			port.set_hwaddr(cmacaddr(_port->hwaddr, OFP_ETH_ALEN));
			port.set_name(std::string(_port->name));

			uint32_t config = 0;
			if(!_port->up)
				config |= rofl::openflow10::OFPPC_PORT_DOWN;
			if(_port->drop_received)
				config |= rofl::openflow10::OFPPC_NO_RECV;
			if(_port->no_flood)
				config |= rofl::openflow10::OFPPC_NO_FLOOD;
			if(!_port->forward_packets)
				config |= rofl::openflow10::OFPPC_NO_FWD;
			if(!_port->of_generate_packet_in)
				config |= rofl::openflow10::OFPPC_NO_PACKET_IN;

			port.set_config(config);
			port.set_state(_port->state);
			port.set_curr(_port->curr);
			port.set_advertised(_port->advertised);
			port.set_supported(_port->supported);
			port.set_peer(_port->peer);

			ports.add_port(_port->of_port_num) = port;
		}
 	}

	//Recover supported actions
	supported_actions = of10_translation_utils::get_supported_actions(of10switch);
	
	//Destroy the snapshot
	//Warning: this MUST be before calling send_ method
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

	ctl.send_features_reply(
			auxid,
			msg.get_xid(),
			sw->dpid,
			num_of_buffers,	// n_buffers
			num_of_tables,	// n_tables
			capabilities,	// capabilities
			auxid.get_id(), //of13_aux_id
			supported_actions,
			ports);

}


void
of10_endpoint::handle_get_config_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_get_config_request& msg)
{
	uint16_t flags = 0x0;
	uint16_t miss_send_len = 0;
	
	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();
	
	flags = of10switch->pipeline.capabilities;
	miss_send_len = of10switch->pipeline.miss_send_len;

	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

	ctl.send_get_config_reply(auxid, msg.get_xid(), flags, miss_send_len);
}



void
of10_endpoint::handle_desc_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_desc_stats_request& msg)
{
	std::string mfr_desc(PACKAGE_NAME);
	std::string hw_desc(VERSION);
	std::string sw_desc(VERSION);

	rofl::openflow::cofdesc_stats_reply desc_stats(
			ctl.get_version_negotiated(),
			mfr_desc,
			hw_desc,
			sw_desc,
			system_manager::get_id(),
			system_manager::get_driver_description()
			);

	ctl.send_desc_stats_reply(auxid, msg.get_xid(), desc_stats);
}



void
of10_endpoint::handle_table_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_table_stats_request& msg)
{
	unsigned int num_of_tables;
	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();
	
	num_of_tables = of10switch->pipeline.num_of_tables;
	
	//Reply to fill in
	rofl::openflow::coftablestatsarray tablestatsarray(ctl.get_version_negotiated());

	for (unsigned int n = 0; n < num_of_tables; n++) {

		uint8_t table_id = of10switch->pipeline.tables[n].number;

		//Main information
		tablestatsarray.set_table_stats(table_id).set_table_id(of10switch->pipeline.tables[n].number);
		tablestatsarray.set_table_stats(table_id).set_name(std::string(of10switch->pipeline.tables[n].name, strnlen(of10switch->pipeline.tables[n].name, OFP_MAX_TABLE_NAME_LEN)));

		//Capabilities
		tablestatsarray.set_table_stats(table_id).set_wildcards(of10_translation_utils::get_supported_wildcards(of10switch));

		//Other information
		tablestatsarray.set_table_stats(table_id).set_max_entries(of10switch->pipeline.tables[n].max_entries);
		tablestatsarray.set_table_stats(table_id).set_active_count(of10switch->pipeline.tables[n].num_of_entries);
		tablestatsarray.set_table_stats(table_id).set_lookup_count(of10switch->pipeline.tables[n].stats.s.counters.lookup_count);
		tablestatsarray.set_table_stats(table_id).set_matched_count(of10switch->pipeline.tables[n].stats.s.counters.matched_count);
	}

	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

	ctl.send_table_stats_reply(auxid, msg.get_xid(), tablestatsarray, false);

}



void
of10_endpoint::handle_port_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_port_stats_request& msg)
{
	switch_port_snapshot_t* port;
	uint32_t port_no = msg.get_port_stats().get_portno();

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();
	
	rofl::openflow::cofportstatsarray portstatsarray(ctl.get_version_negotiated());

	/*
	 *  send statistics for all ports
	 */
	if (rofl::openflow10::OFPP_ALL == port_no || rofl::openflow10::OFPP_NONE == port_no){

		//we check all the positions in case there are empty slots
		for (unsigned int n = 1; n < of10switch->max_ports; n++){

			port = of10switch->logical_ports[n].port;

			if((port != NULL) && (of10switch->logical_ports[n].attachment_state == LOGICAL_PORT_STATE_ATTACHED)){

				portstatsarray.set_port_stats(port->of_port_num).set_port_no(port->of_port_num);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_packets(port->stats.rx_packets);
				portstatsarray.set_port_stats(port->of_port_num).set_tx_packets(port->stats.tx_packets);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_bytes(port->stats.rx_bytes);
				portstatsarray.set_port_stats(port->of_port_num).set_tx_bytes(port->stats.tx_bytes);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_dropped(port->stats.rx_dropped);
				portstatsarray.set_port_stats(port->of_port_num).set_tx_dropped(port->stats.tx_dropped);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_errors(port->stats.rx_errors);
				portstatsarray.set_port_stats(port->of_port_num).set_tx_errors(port->stats.tx_errors);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_frame_err(port->stats.rx_frame_err);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_over_err(port->stats.rx_over_err);
				portstatsarray.set_port_stats(port->of_port_num).set_rx_crc_err(port->stats.rx_crc_err);
				portstatsarray.set_port_stats(port->of_port_num).set_collisions(port->stats.collisions);
			}
	 	}

	}else if(port_no < of10switch->max_ports){
		/*
		 * send statistics for only one port
		 */

		// search for the port with the specified port-number
		//we check all the positions in case there are empty slots
		port = of10switch->logical_ports[port_no].port;
		if( 	(port != NULL) &&
			(of10switch->logical_ports[port_no].attachment_state == LOGICAL_PORT_STATE_ATTACHED) &&
			(port->of_port_num == port_no)
		){

			//Mapping of port state
			portstatsarray.set_port_stats(port->of_port_num).set_port_no(port->of_port_num);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_packets(port->stats.rx_packets);
			portstatsarray.set_port_stats(port->of_port_num).set_tx_packets(port->stats.tx_packets);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_bytes(port->stats.rx_bytes);
			portstatsarray.set_port_stats(port->of_port_num).set_tx_bytes(port->stats.tx_bytes);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_dropped(port->stats.rx_dropped);
			portstatsarray.set_port_stats(port->of_port_num).set_tx_dropped(port->stats.tx_dropped);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_errors(port->stats.rx_errors);
			portstatsarray.set_port_stats(port->of_port_num).set_tx_errors(port->stats.tx_errors);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_frame_err(port->stats.rx_frame_err);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_over_err(port->stats.rx_over_err);
			portstatsarray.set_port_stats(port->of_port_num).set_rx_crc_err(port->stats.rx_crc_err);
			portstatsarray.set_port_stats(port->of_port_num).set_collisions(port->stats.collisions);
		}

		// if port_no was not found, body.memlen() is 0
	}else{
		//Unknown port
		ROFL_ERR("Got a port stats request for an unknown port: %u. Ignoring...\n",port_no);
	}

	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

	ctl.send_port_stats_reply(auxid, msg.get_xid(), portstatsarray, false);
}



void
of10_endpoint::handle_flow_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_flow_stats_request& msg)
{
	of1x_stats_flow_msg_t* fp_msg = NULL;
	of1x_flow_entry_t* entry = NULL;

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();

	//Map the match structure from OpenFlow to of1x_packet_matches_t
	entry = of1x_init_flow_entry(false);

	try{
		of10_translation_utils::of10_map_flow_entry_matches(&ctl, msg.set_flow_stats().get_match(), sw, entry);
	}catch(...){
		of1x_destroy_flow_entry(entry);
	
		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
		
		throw rofl::eBadRequestBadStat();


	}

	//Ask the Forwarding Plane to process stats
	fp_msg = hal_driver_of1x_get_flow_stats(sw->dpid,
			msg.get_flow_stats().get_table_id(),
			0,
			0,
			of10_translation_utils::get_out_port(msg.get_flow_stats().get_out_port()),
			OF1X_GROUP_ANY,
			&entry->matches);

	if(!fp_msg){
		of1x_destroy_flow_entry(entry);

		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
	
		throw rofl::eBadRequestBadStat();
	}

	//Construct OF message
	of1x_stats_single_flow_msg_t *elem = fp_msg->flows_head;

	rofl::openflow::cofflowstatsarray flowstatsarray(ctl.get_version_negotiated());

	uint32_t flow_id = 0;

	try{
		for(elem = fp_msg->flows_head; elem; elem = elem->next){

			rofl::openflow::cofmatch match(rofl::openflow10::OFP_VERSION);
			of10_translation_utils::of1x_map_reverse_flow_entry_matches(elem->matches, match);

			rofl::openflow::cofactions actions(rofl::openflow10::OFP_VERSION);
			of10_translation_utils::of1x_map_reverse_flow_entry_actions((of1x_instruction_group_t*)(elem->inst_grp), actions, of10switch->pipeline.miss_send_len);

			flowstatsarray.set_flow_stats(flow_id).set_table_id(elem->table_id);
			flowstatsarray.set_flow_stats(flow_id).set_duration_sec(elem->duration_sec);
			flowstatsarray.set_flow_stats(flow_id).set_duration_nsec(elem->duration_nsec);
			flowstatsarray.set_flow_stats(flow_id).set_priority(elem->priority);
			flowstatsarray.set_flow_stats(flow_id).set_idle_timeout(elem->idle_timeout);
			flowstatsarray.set_flow_stats(flow_id).set_hard_timeout(elem->hard_timeout);
			flowstatsarray.set_flow_stats(flow_id).set_cookie(elem->cookie);
			flowstatsarray.set_flow_stats(flow_id).set_packet_count(elem->packet_count);
			flowstatsarray.set_flow_stats(flow_id).set_byte_count(elem->byte_count);
			flowstatsarray.set_flow_stats(flow_id).set_match() = match;
			flowstatsarray.set_flow_stats(flow_id).set_actions() = actions;

			flow_id++;

			// TODO: check this implicit assumption of always using a single instruction?
			// this should be an instruction of type OFPIT_APPLY_ACTIONS anyway
		}

		//Send message
		ctl.send_flow_stats_reply(auxid, msg.get_xid(), flowstatsarray);
	}catch(...){
		of1x_destroy_stats_flow_msg(fp_msg);
		of1x_destroy_flow_entry(entry);
	
		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
	
		throw;
	}
	//Destroy FP stats
	of1x_destroy_stats_flow_msg(fp_msg);
	of1x_destroy_flow_entry(entry);
	
	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
}



void
of10_endpoint::handle_aggregate_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_aggr_stats_request& msg)
{
	of1x_stats_flow_aggregate_msg_t* fp_msg;
	of1x_flow_entry_t* entry;

//	cmemory body(sizeof(struct ofp_flow_stats));
//	struct ofp_flow_stats *flow_stats = (struct ofp_flow_stats*)body.somem();

	//Map the match structure from OpenFlow to packet_matches_t
	entry = of1x_init_flow_entry(false);

	if(!entry)
		throw rofl::eBadRequestBadStat();

	try{
		of10_translation_utils::of10_map_flow_entry_matches(&ctl, msg.get_aggr_stats().get_match(), sw, entry);
	}catch(...){
		of1x_destroy_flow_entry(entry);
		throw rofl::eBadRequestBadStat();
	}

	//TODO check error while mapping

	//Ask the Forwarding Plane to process stats
	fp_msg = hal_driver_of1x_get_flow_aggregate_stats(sw->dpid,
					msg.get_aggr_stats().get_table_id(),
					0,
					0,
					of10_translation_utils::get_out_port(msg.get_aggr_stats().get_out_port()),
					OF1X_GROUP_ANY,
					&entry->matches);

	if(!fp_msg){
		of1x_destroy_flow_entry(entry);
		throw rofl::eBadRequestBadStat();
	}

	try{
		//Construct OF message
		ctl.send_aggr_stats_reply(
				auxid,
				msg.get_xid(),
				rofl::openflow::cofaggr_stats_reply(
					ctl.get_version_negotiated(),
					fp_msg->packet_count,
					fp_msg->byte_count,
					fp_msg->flow_count),
				false);
	}catch(...){
		of1x_destroy_stats_flow_aggregate_msg(fp_msg);
		of1x_destroy_flow_entry(entry);
		throw;
	}

	//Destroy FP stats
	of1x_destroy_stats_flow_aggregate_msg(fp_msg);
	of1x_destroy_flow_entry(entry);
}



void
of10_endpoint::handle_queue_stats_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_queue_stats_request& pack)
{
	switch_port_snapshot_t* port = NULL;
	unsigned int portnum = pack.get_queue_stats().get_port_no();
	unsigned int queue_id = pack.get_queue_stats().get_queue_id();

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();

	if( ((portnum >= of10switch->max_ports) && (portnum != openflow10::OFPP_ALL)) || portnum == 0){
		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
		throw rofl::eBadRequestBadPort(); 	//Invalid port num
	}

	rofl::openflow::cofqueuestatsarray queuestatsarray(ctl.get_version_negotiated());

	/*
	* port num
	*/

	//we check all the positions in case there are empty slots
	for (unsigned int n = 1; n < of10switch->max_ports; n++){

		port = of10switch->logical_ports[n].port;

		if ( port == NULL || ( (rofl::openflow10::OFPP_ALL != portnum) && (port->of_port_num != portnum) ) )
			continue;


		if( of10switch->logical_ports[n].attachment_state == LOGICAL_PORT_STATE_ATTACHED /* && (port->of_port_num == portnum)*/){

			if (OFPQ_ALL == queue_id){

				for(unsigned int i=0; i<port->max_queues; i++){
					if(!port->queues[i].set)
						continue;

					queuestatsarray.set_queue_stats(port->of_port_num, i).set_port_no(port->of_port_num);
					queuestatsarray.set_queue_stats(port->of_port_num, i).set_queue_id(i);
					queuestatsarray.set_queue_stats(port->of_port_num, i).set_tx_bytes(port->queues[i].stats.tx_bytes);
					queuestatsarray.set_queue_stats(port->of_port_num, i).set_tx_packets(port->queues[i].stats.tx_packets);
					queuestatsarray.set_queue_stats(port->of_port_num, i).set_tx_errors(port->queues[i].stats.overrun);
				}

			} else {

				if(queue_id >= port->max_queues){
					//Destroy the snapshot
					of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

					throw rofl::eBadRequestBadPort(); 	//FIXME send a BadQueueId error
				}


				//Check if the queue is really in use
				if(port->queues[queue_id].set){
					//Set values

					queuestatsarray.set_queue_stats(portnum, queue_id).set_port_no(portnum);
					queuestatsarray.set_queue_stats(portnum, queue_id).set_queue_id(queue_id);
					queuestatsarray.set_queue_stats(portnum, queue_id).set_tx_bytes(port->queues[queue_id].stats.tx_bytes);
					queuestatsarray.set_queue_stats(portnum, queue_id).set_tx_packets(port->queues[queue_id].stats.tx_packets);
					queuestatsarray.set_queue_stats(portnum, queue_id).set_tx_errors(port->queues[queue_id].stats.overrun);
				}
			}
		}
	}


	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

	ctl.send_queue_stats_reply(
			auxid,
			pack.get_xid(),
			queuestatsarray,
			false);
}





void
of10_endpoint::handle_experimenter_stats_request(
		rofl::crofctl& ctl, 
		const rofl::cauxid& auxid, 
		rofl::openflow::cofmsg_experimenter_stats_request& msg)
{
	//TODO: when exp are supported
}



void
of10_endpoint::handle_packet_out(
		rofl::crofctl& ctl,
		const cauxid& auxid,
		rofl::openflow::cofmsg_packet_out& msg)
{
	of1x_action_group_t* action_group = of1x_init_action_group(NULL);

	try{
		of10_translation_utils::of1x_map_flow_entry_actions(&ctl, sw, msg.set_actions(), action_group, NULL); //TODO: is this OK always NULL?
	}catch(...){
		of1x_destroy_action_group(action_group);
		throw;
	}

	/* assumption: driver can handle all situations properly:
	 * - data and datalen both 0 and buffer_id != rofl::openflow10::OFP_NO_BUFFER
	 * - buffer_id == rofl::openflow10::OFP_NO_BUFFER and data and datalen both != 0
	 * - everything else is an error?
	 */
	if (HAL_FAILURE == hal_driver_of1x_process_packet_out(sw->dpid,
							msg.get_buffer_id(),
							msg.get_in_port(),
							action_group,
							msg.get_packet().soframe(), msg.get_packet().length())){
		// log error
		//FIXME: send error
	}

	of1x_destroy_action_group(action_group);
}







rofl_result_t
of10_endpoint::process_packet_in(
		uint8_t table_id,
		uint8_t reason,
		uint32_t in_port,
		uint32_t buffer_id,
		uint64_t cookie,
		uint8_t* pkt_buffer,
		uint32_t buf_len,
		uint16_t total_len,
		packet_matches_t* matches)
{
	try {
		//Transform matches
		rofl::openflow::cofmatch match(rofl::openflow10::OFP_VERSION);
		of10_translation_utils::of1x_map_reverse_packet_matches(matches, match);

		size_t len = (total_len < buf_len) ? total_len : buf_len;

		rofl::crofbase::send_packet_in_message(
				cauxid(0),
				buffer_id,
				total_len,
				reason,
				table_id,
				cookie,
				in_port, // OF1.0 only
				match,
				pkt_buffer, len);

		return ROFL_SUCCESS;

	} catch (rofl::eRofBaseNotConnected& e) {

		return ROFL_FAILURE;

	} catch (rofl::eRofBaseCongested& e) {

		return ROFL_FAILURE;

	} catch (...) {

	}

	return ROFL_FAILURE;
}

/*
* Port async notifications processing
*/

rofl_result_t of10_endpoint::notify_port_attached(const switch_port_snapshot_t* port){

	try {
		uint32_t config=0x0;

		//Compose port config
		if(!port->up) config |= rofl::openflow10::OFPPC_PORT_DOWN;
		if(!port->of_generate_packet_in) config |= rofl::openflow10::OFPPC_NO_PACKET_IN;
		if(!port->forward_packets) config |= rofl::openflow10::OFPPC_NO_FWD;
		if(port->drop_received) config |= rofl::openflow10::OFPPC_NO_RECV;


		rofl::openflow::cofport ofport(rofl::openflow10::OFP_VERSION);
		ofport.set_port_no(port->of_port_num);
		ofport.set_hwaddr(cmacaddr((uint8_t*)port->hwaddr, OFP_ETH_ALEN));
		ofport.set_name(std::string(port->name));
		ofport.set_config(config);
		ofport.set_state(port->state&0x1); //Only first bit is relevant
		ofport.set_curr(port->curr);
		ofport.set_advertised(port->advertised);
		ofport.set_supported(port->supported);
		ofport.set_peer(port->peer);
		//ofport.set_curr_speed(of10_translation_utils::get_port_speed_kb(port->curr_speed));
		//ofport.set_max_speed(of10_translation_utils::get_port_speed_kb(port->curr_max_speed));

		//Send message
		rofl::crofbase::send_port_status_message(cauxid(0), rofl::openflow10::OFPPR_ADD, ofport);

		return ROFL_SUCCESS;

	} catch (...) {

		return ROFL_FAILURE;
	}

}

rofl_result_t of10_endpoint::notify_port_detached(const switch_port_snapshot_t* port){

	try {
		uint32_t config=0x0;

		//Compose port config
		if(!port->up) config |= rofl::openflow10::OFPPC_PORT_DOWN;
		if(!port->of_generate_packet_in) config |= rofl::openflow10::OFPPC_NO_PACKET_IN;
		if(!port->forward_packets) config |= rofl::openflow10::OFPPC_NO_FWD;
		if(port->drop_received) config |= rofl::openflow10::OFPPC_NO_RECV;

		rofl::openflow::cofport ofport(rofl::openflow10::OFP_VERSION);
		ofport.set_port_no(port->of_port_num);
		ofport.set_hwaddr(cmacaddr((uint8_t*)port->hwaddr, OFP_ETH_ALEN));
		ofport.set_name(std::string(port->name));
		ofport.set_config(config);
		ofport.set_state(port->state&0x1); //Only first bit is relevant
		ofport.set_curr(port->curr);
		ofport.set_advertised(port->advertised);
		ofport.set_supported(port->supported);
		ofport.set_peer(port->peer);
		//ofport.set_curr_speed(of10_translation_utils::get_port_speed_kb(port->curr_speed));
		//ofport.set_max_speed(of10_translation_utils::get_port_speed_kb(port->curr_max_speed));

		//Send message
		rofl::crofbase::send_port_status_message(cauxid(0), rofl::openflow10::OFPPR_DELETE, ofport);

		return ROFL_SUCCESS;


	} catch (...) {

		return ROFL_FAILURE;
	}

}

rofl_result_t of10_endpoint::notify_port_status_changed(const switch_port_snapshot_t* port){

	try {
		uint32_t config=0x0;

		//Compose port config
		if(!port->up) config |= rofl::openflow10::OFPPC_PORT_DOWN;
		if(!port->of_generate_packet_in) config |= rofl::openflow10::OFPPC_NO_PACKET_IN;
		if(!port->forward_packets) config |= rofl::openflow10::OFPPC_NO_FWD;
		if(port->drop_received) config |= rofl::openflow10::OFPPC_NO_RECV;

		//Notify OF controller
		rofl::openflow::cofport ofport(rofl::openflow10::OFP_VERSION);
		ofport.set_port_no(port->of_port_num);
		ofport.set_hwaddr(cmacaddr((uint8_t*)port->hwaddr, OFP_ETH_ALEN));
		ofport.set_name(std::string(port->name));
		ofport.set_config(config);
		ofport.set_state(port->state&0x1); //Only first bit is relevant
		ofport.set_curr(port->curr);
		ofport.set_advertised(port->advertised);
		ofport.set_supported(port->supported);
		ofport.set_peer(port->peer);
		//ofport.set_curr_speed(of10_translation_utils::get_port_speed_kb(port->curr_speed));
		//ofport.set_max_speed(of10_translation_utils::get_port_speed_kb(port->curr_max_speed));

		//Send message
		send_port_status_message(cauxid(0), rofl::openflow10::OFPPR_MODIFY, ofport);

		return ROFL_SUCCESS; // ignore this notification

	} catch (...) {

		return ROFL_FAILURE;
	}

}





void
of10_endpoint::handle_barrier_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_barrier_request& pack)
{
	//Since we are not queuing messages currently
	ctl.send_barrier_reply(auxid, pack.get_xid());
}



void
of10_endpoint::handle_flow_mod(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_flow_mod& msg)
{
	switch (msg.get_flowmod().get_command()) {
		case rofl::openflow10::OFPFC_ADD: {
				flow_mod_add(ctl, msg);
			} break;

		case rofl::openflow10::OFPFC_MODIFY: {
				flow_mod_modify(ctl, msg, false);
			} break;

		case rofl::openflow10::OFPFC_MODIFY_STRICT: {
				flow_mod_modify(ctl, msg, true);
			} break;

		case rofl::openflow10::OFPFC_DELETE: {
				flow_mod_delete(ctl, msg, false);
			} break;

		case rofl::openflow10::OFPFC_DELETE_STRICT: {
				flow_mod_delete(ctl, msg, true);
			} break;

		default:
			throw rofl::eFlowModBadCommand();
	}
}



void
of10_endpoint::flow_mod_add(
		rofl::crofctl& ctl,
		rofl::openflow::cofmsg_flow_mod& msg)
{
	uint8_t table_id = msg.get_flowmod().get_table_id();
	hal_result_t res;
	of1x_flow_entry_t *entry=NULL;

	// sanity check: table for table-id must exist
	if ( (table_id > sw->num_of_tables) && (table_id != openflow10::OFPTT_ALL) )
	{
		ROFL_DEBUG("of10_endpoint(%s)::flow_mod_add() "
				"invalid table-id:%d in flow-mod command",
				sw->dpname.c_str(), msg.get_flowmod().get_table_id());
	
		assert(0);
		return;
	}

	try{
		entry = of10_translation_utils::of1x_map_flow_entry(&ctl, &msg, sw);
	}catch(...){
		ROFL_DEBUG("of10_endpoint(%s)::flow_mod_add() "
				"unable to create flow-entry", sw->dpname.c_str());
		assert(0);
		return;
	}

	if(!entry) {
		assert(0);//Just for safety, but shall never reach this
		return;
	}

	if (HAL_SUCCESS != (res = hal_driver_of1x_process_flow_mod_add(sw->dpid,
								msg.get_flowmod().get_table_id(),
								&entry,
								msg.get_flowmod().get_buffer_id(),
								msg.get_flowmod().get_flags() & rofl::openflow10::OFPFF_CHECK_OVERLAP,
								false /*OFPFF_RESET_COUNTS is not defined for OpenFlow 1.0*/))){

		if(entry){
			ROFL_DEBUG("Error inserting the flowmod\n");
			of1x_destroy_flow_entry(entry);
		}else{
			ROFL_DEBUG("Flowmod inserted, but buffer was expired/invalid\n");
		}

		if(res == HAL_FM_OVERLAP_FAILURE)
			throw rofl::eFlowModOverlap();
		else
			throw rofl::eFlowModTableFull();
	}
}



void
of10_endpoint::flow_mod_modify(
		rofl::crofctl& ctl,
		rofl::openflow::cofmsg_flow_mod& pack,
		bool strict)
{
	of1x_flow_entry_t *entry=NULL;

	// sanity check: table for table-id must exist
	if (pack.get_flowmod().get_table_id() > sw->num_of_tables)
	{
		ROFL_DEBUG("of10_endpoint(%s)::flow_mod_modify() "
				"invalid table-id:%d in flow-mod command",
				sw->dpname.c_str(), pack.get_flowmod().get_table_id());

		assert(0);
		return;
	}

	try{
		entry = of10_translation_utils::of1x_map_flow_entry(&ctl, &pack, sw);
	}catch(...){
		ROFL_DEBUG("of10_endpoint(%s)::flow_mod_modify() "
				"unable to attempt to modify flow-entry", sw->dpname.c_str());
		assert(0);
		return;
	}

	if(!entry) {
		assert(0);//Just for safety, but shall never reach this
		return;
	}

	of1x_flow_removal_strictness_t strictness = (strict) ? STRICT : NOT_STRICT;


	if(HAL_SUCCESS != hal_driver_of1x_process_flow_mod_modify(sw->dpid,
								pack.get_flowmod().get_table_id(),
								&entry,
								pack.get_flowmod().get_buffer_id(),
								strictness,
								false /*OFPFF_RESET_COUNTS is not defined for OpenFlow 1.0*/)){
		if(entry){
			ROFL_DEBUG("Error modifying the flowmod\n");
			of1x_destroy_flow_entry(entry);
		}else{
			ROFL_DEBUG("Flowmod inserted, but buffer was expired/invalid\n");
		}
	}

}



void
of10_endpoint::flow_mod_delete(
		rofl::crofctl& ctl,
		rofl::openflow::cofmsg_flow_mod& pack,
		bool strict) //throw (eOfSmPipelineBadTableId)
{

	of1x_flow_entry_t *entry=NULL;

	try{
		entry = of10_translation_utils::of1x_map_flow_entry(&ctl, &pack, sw);
	}catch(...){
		ROFL_DEBUG("of10_endpoint(%s)::flow_mod_delete() "
				"unable to attempt to remove flow-entry", sw->dpname.c_str());
		assert(0);
		return;
	}

	if(!entry) {
		assert(0);//Just for safety, but shall never reach this
		return;
	}


	of1x_flow_removal_strictness_t strictness = (strict) ? STRICT : NOT_STRICT;

	if(HAL_SUCCESS != hal_driver_of1x_process_flow_mod_delete(sw->dpid,
								pack.get_flowmod().get_table_id(),
								entry,
								of10_translation_utils::get_out_port(pack.get_flowmod().get_out_port()),
								OF1X_GROUP_ANY,
								strictness)) {
		ROFL_DEBUG("Error deleting flowmod\n");
	}

	//Always delete entry
	of1x_destroy_flow_entry(entry);

}


rofl_result_t
of10_endpoint::process_flow_removed(
		uint8_t reason,
		of1x_flow_entry *entry)
{
	try {
		rofl::openflow::cofmatch match(rofl::openflow10::OFP_VERSION);
		uint32_t sec,nsec;

		of10_translation_utils::of1x_map_reverse_flow_entry_matches(entry->matches.head, match);

		//get duration of the flow mod
		of1x_stats_flow_get_duration(entry, &sec, &nsec);


		rofl::crofbase::send_flow_removed_message(
				cauxid(0),
				match,
				entry->cookie,
				entry->priority,
				reason,
				entry->table->number,
				sec,
				nsec,
				entry->timer_info.idle_timeout,
				entry->timer_info.hard_timeout,
				entry->stats.s.counters.packet_count,
				entry->stats.s.counters.byte_count);

		return ROFL_SUCCESS;

	} catch (...) {

		return ROFL_FAILURE;
	}

}





void
of10_endpoint::handle_table_mod(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_table_mod& msg)
{

	/*
	 * the parameters defined in the pipeline OF1X_TABLE_...
	 * match those defined by the OF1.2 specification.
	 * This may change in the future for other versions, so map
	 * the OF official numbers to the ones used by the pipeline.
	 *
	 * at least we map network byte order to host byte order here ...
	 */
	of1x_flow_table_miss_config_t config = OF1X_TABLE_MISS_CONTROLLER; //Default

	/*
	 * OpenFlow 1.0 does not define struct ofp_table_mod.
	 */
#if 0
	if (msg->get_config() == rofl::openflow10::OFPTC_TABLE_MISS_CONTINUE){
		config = OF1X_TABLE_MISS_CONTINUE;
	}else if (msg->get_config() == rofl::openflow10::OFPTC_TABLE_MISS_CONTROLLER){
		config = OF1X_TABLE_MISS_CONTROLLER;
	}else if (msg->get_config() == rofl::openflow10::OFPTC_TABLE_MISS_DROP){
		config = OF1X_TABLE_MISS_DROP;
	}
#endif
	if( HAL_FAILURE == hal_driver_of1x_set_table_config(sw->dpid, msg.get_table_id(), config) ){
		//TODO: treat exception
	}
}



void
of10_endpoint::handle_port_mod(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_port_mod& msg)
{
	uint32_t config, mask, advertise;
	uint16_t port_num;

	config 		= msg.get_config();
	mask 		= msg.get_mask();
	advertise 	= msg.get_advertise();
	port_num 	= (uint16_t)msg.get_port_no();

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();

	//Check if port_num FLOOD
	//TODO: Inspect if this is right. Spec does not clearly define if this should be supported or not
	if( (port_num != rofl::openflow10::OFPP_ALL) && (port_num > rofl::openflow10::OFPP_MAX) ){
		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
		throw rofl::ePortModBadPort();
	}

	// check for existence of port with id port_num
	switch_port_snapshot_t* port = NULL;
	bool port_found = false;
	for(unsigned int n = 1; n < of10switch->max_ports; n++){
		port = of10switch->logical_ports[n].port;
		if ((0 != port) && (port->of_port_num == (uint32_t)port_num)) {
			port_found = true;
			break;
		}
	}
	if (not port_found){
		//Destroy the snapshot
		of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
		throw rofl::eBadRequestBadPort();
	}


	//Drop received
	if( mask &  rofl::openflow10::OFPPC_NO_RECV )
		if( HAL_FAILURE == hal_driver_of1x_set_port_drop_received_config(sw->dpid, port_num, config & rofl::openflow10::OFPPC_NO_RECV ) ){
			//Destroy the snapshot
			of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
			throw rofl::ePortModBase();
		}
	//No forward
	if( mask &  rofl::openflow10::OFPPC_NO_FWD )
		if( HAL_FAILURE == hal_driver_of1x_set_port_forward_config(sw->dpid, port_num, !(config & rofl::openflow10::OFPPC_NO_FWD) ) ){
			//Destroy the snapshot
			of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
			throw rofl::ePortModBase();
		}

	//No flood
	if( mask &  rofl::openflow10::OFPPC_NO_FLOOD )
	{
		if( HAL_FAILURE == hal_driver_of1x_set_port_no_flood_config(sw->dpid, port_num, config & rofl::openflow10::OFPPC_NO_FLOOD ) ){
			//Destroy the snapshot
			of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
			throw rofl::ePortModBase();
		}
	}

	//No packet in
	if( mask &  rofl::openflow10::OFPPC_NO_PACKET_IN )
		if( HAL_FAILURE == hal_driver_of1x_set_port_generate_packet_in_config(sw->dpid, port_num, !(config & rofl::openflow10::OFPPC_NO_PACKET_IN) ) ){
			//Destroy the snapshot
			of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
			throw rofl::ePortModBase();
		}

	//Advertised
	if( advertise )
		if( HAL_FAILURE == hal_driver_of1x_set_port_advertise_config(sw->dpid, port_num, advertise)  ){
			//Destroy the snapshot
			of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
			throw rofl::ePortModBase();
		}

	//Port admin down //TODO: evaluate if we can directly call hal_driver_enable_port_by_num instead
	if( mask &  rofl::openflow10::OFPPC_PORT_DOWN ){
		if( (config & rofl::openflow10::OFPPC_PORT_DOWN)  ){
			//Disable port
			if( HAL_FAILURE == hal_driver_bring_port_down_by_num(sw->dpid, port_num) ){
				//Destroy the snapshot
				of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
				throw rofl::ePortModBase();
			}
		}else{
			if( HAL_FAILURE == hal_driver_bring_port_up_by_num(sw->dpid, port_num) ){
				//Destroy the snapshot
				of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
				throw rofl::ePortModBase();
			}
		}
	}
	
	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);

#if 0
	/*
	 * in case of an error, use one of these exceptions:
	 */
	throw rofl::ePortModBadAdvertise();
	throw rofl::ePortModBadConfig();
	throw rofl::ePortModBadHwAddr();
	throw rofl::ePortModBadPort();
#endif
}



void
of10_endpoint::handle_set_config(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_set_config& msg)
{

	//Instruct the driver to process the set config
	if(HAL_FAILURE == hal_driver_of1x_set_pipeline_config(sw->dpid, msg.get_flags(), msg.get_miss_send_len())){
		throw rofl::eSwitchConfigBadFlags();
	}
}



void
of10_endpoint::handle_queue_get_config_request(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_queue_get_config_request& pack)
{
	switch_port_snapshot_t* port;
	unsigned int portnum = pack.get_port_no();

	of1x_switch_snapshot_t* of10switch = (of1x_switch_snapshot_t*)hal_driver_get_switch_snapshot_by_dpid(sw->dpid);

	if(!of10switch)
		throw rofl::eRofBase();

	rofl::openflow::cofpacket_queues queues(ctl.get_version_negotiated());

	//we check all the positions in case there are empty slots
	for(unsigned int n = 1; n < of10switch->max_ports; n++){

		port = of10switch->logical_ports[n].port;

		if(port == NULL)
			continue;

		if (of10switch->logical_ports[n].attachment_state != LOGICAL_PORT_STATE_ATTACHED)
			continue;

		if ((rofl::openflow10::OFPP_ALL != portnum) && (port->of_port_num != portnum))
			continue;

		for(unsigned int i=0; i<port->max_queues; i++){
			if(!port->queues[i].set)
				continue;
			queues.set_pqueue(portnum, port->queues[i].id).set_queue_props().
					add_queue_prop_min_rate().set_min_rate(port->queues[i].min_rate);
		}
	}
	
	//Destroy the snapshot
	of_switch_destroy_snapshot((of_switch_snapshot_t*)of10switch);
	
	//Send reply
	ctl.send_queue_get_config_reply(auxid, pack.get_xid(), pack.get_port_no(), queues);
}



void
of10_endpoint::handle_experimenter_message(
		rofl::crofctl& ctl,
		const rofl::cauxid& auxid,
		rofl::openflow::cofmsg_experimenter& pack)
{
	// TODO
}



void
of10_endpoint::handle_ctl_attached(crofctl *ctrl)
{
	std::stringstream sstr; sstr << ctrl->get_peer_addr(rofl::cauxid(0));
	ROFL_INFO("[sw: %s]Controller %s:%u is in CONNECTED state. \n", sw->dpname.c_str() , sstr.str().c_str()); //FIXME: add role
}



void
of10_endpoint::handle_ctl_detached(crofctl *ctrl)
{
	std::stringstream sstr; sstr << ctrl->get_peer_addr(rofl::cauxid(0));
	ROFL_INFO("[sw: %s] Controller %s:%u has DISCONNECTED. \n", sw->dpname.c_str() ,sstr.str().c_str()); //FIXME: add role

}
