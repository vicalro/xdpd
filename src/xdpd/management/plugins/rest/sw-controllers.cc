// Copyright(c) 2014	Barnstormer Softworks, Ltd.

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>


#include "server/request.hpp"
#include "server/reply.hpp"
#include "json_spirit/json_spirit.h"

#include "delete-controllers.h"
#include "get-controllers.h"
#include "put-controllers.h"

#include <rofl/common/utils/c_logger.h>

#include "../../port_manager.h"
#include "../../plugin_manager.h"
#include "../../switch_manager.h"
#include "../../system_manager.h"

namespace xdpd{
namespace controllers{
namespace get{

void list_lsis(const http::server::request &req,
						http::server::reply &rep,
						boost::cmatch& grps){
	//Prepare object
	json_spirit::Object dps;

	std::list<std::string> datapaths =
					switch_manager::list_sw_names();

	json_spirit::Value pa(datapaths.begin(), datapaths.end());
	dps.push_back(json_spirit::Pair("lsis", pa));

	rep.content = json_spirit::write(dps, true);
}

void lsi_detail(const http::server::request &req,
						http::server::reply &rep,
						boost::cmatch& grps){
	json_spirit::Object lsi;
	std::string lsi_name = std::string(grps[1]);

	//Check if it exists;
	if(!switch_manager::exists_by_name(lsi_name)){
		//Throw 404
		std::stringstream ss;
		ss<<"Invalid lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	//Get the snapshot
	uint64_t dpid = switch_manager::get_switch_dpid(lsi_name);
	openflow_switch_snapshot snapshot;

	switch_manager::get_switch_info(dpid, snapshot);

	//Fill in general information
	lsi.push_back(json_spirit::Pair("name", lsi_name));
	lsi.push_back(json_spirit::Pair("dpid", dpid));

	std::string of_version;
	switch(snapshot.version){
		case OF_VERSION_10: of_version = "1.0";
			break;
		case OF_VERSION_12: of_version = "1.2";
			break;
		case OF_VERSION_13: of_version = "1.3";
			break;
		default: of_version = "invalid";
			break;
	}

	lsi.push_back(json_spirit::Pair("of_version", of_version));
	lsi.push_back(json_spirit::Pair("num-of-tables", (int)snapshot.num_of_tables));
	lsi.push_back(json_spirit::Pair("miss-send-len", (int)snapshot.miss_send_len));
	lsi.push_back(json_spirit::Pair("num-of-buffers", (int)snapshot.num_of_buffers));

	//Attached ports
	json_spirit::Object ports;
	std::map<unsigned int, std::string>::const_iterator port_it;
	for(port_it = snapshot.ports.begin(); port_it != snapshot.ports.end(); ++port_it){
		std::stringstream ss;
		ss <<  port_it->first;
		ports.push_back(json_spirit::Pair(ss.str(), port_it->second));
	}
	lsi.push_back(json_spirit::Pair("attached-ports", ports));

	//Loop over the tables
	json_spirit::Object tables;
	std::list<openflow_switch_table_snapshot>::const_iterator it;
	for(it = snapshot.tables.begin(); it != snapshot.tables.end(); ++it){
		json_spirit::Object t;
		std::stringstream ss;
		//Generics
		t.push_back(json_spirit::Pair("number", (int)it->number));
		ss <<  it->number;

		//TODO: convert this into a string
		t.push_back(json_spirit::Pair("matching-algorithm", (int)it->matching_algorithm));

		t.push_back(json_spirit::Pair("num-of-entries", (uint64_t)it->num_of_entries));
		t.push_back(json_spirit::Pair("max-entries", (uint64_t)it->max_entries));

		std::string miss;
		switch(it->default_action){
			case OF1X_TABLE_MISS_CONTROLLER: miss = "controller";
				break;
			case OF1X_TABLE_MISS_CONTINUE: miss = "continue";
				break;
			case OF1X_TABLE_MISS_DROP: miss = "drop";
				break;
			default: miss = "unknown";
				break;
		}
		t.push_back(json_spirit::Pair("table-miss", miss));

		//TODO: capabilities

		//Statistics
		json_spirit::Object s;
		s.push_back(json_spirit::Pair("pkts-looked-up", it->stats_lookup));
		s.push_back(json_spirit::Pair("pkts-matched", it->stats_matched));
		t.push_back(json_spirit::Pair("statistics", s));

		tables.push_back(json_spirit::Pair(ss.str(), t));
	}
	lsi.push_back(json_spirit::Pair("tables", tables));

	//Group table
	json_spirit::Object gtable;
	gtable.push_back(json_spirit::Pair("num-of-groups", (uint64_t)snapshot.group_table.num_of_entries));
	//TODO: config
	lsi.push_back(json_spirit::Pair("group-table", gtable));

	rep.content = json_spirit::write(lsi, true);
}

void lsi_table_flows(const http::server::request &req,
						http::server::reply &rep,
						boost::cmatch& grps){
	json_spirit::Object wrap;
	json_spirit::Object table;
	std::string lsi_name = std::string(grps[1]);
	std::string tid = std::string(grps[2]);

	//Check if it exists;
	if(!switch_manager::exists_by_name(lsi_name)){
		//Throw 404
		std::stringstream ss;
		ss<<"Invalid lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	//Check if table exists
	std::istringstream reader(tid);
	unsigned int id;
	reader >> id;

	uint64_t dpid = switch_manager::get_switch_dpid(lsi_name);
	openflow_switch_snapshot snapshot;
	switch_manager::get_switch_info(dpid, snapshot);

	//Check if table id is valid
	if(snapshot.num_of_tables <= id){
		//Throw 404
		std::stringstream ss;
		ss << "Invalid table id: '"<< id << "' for lsi: '"<< lsi_name <<"'. Valid tables: 0-"<<snapshot.num_of_tables-1;
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	//Get flows
	std::list<flow_entry_snapshot> flows;
	std::list<std::string> flows_str;
	switch_manager::get_switch_table_flows(dpid, id, flows);
	std::list<flow_entry_snapshot>::const_iterator it;
	for(it=flows.begin(); it != flows.end(); ++it){
		std::stringstream ss;
		ss << *it;
		flows_str.push_back(ss.str());
	}

	table.push_back(json_spirit::Pair("id", (uint64_t)id));
	json_spirit::Value flows_(flows_str.begin(), flows_str.end());
	table.push_back(json_spirit::Pair("flows", flows_));

	//Wrap
	wrap.push_back(json_spirit::Pair("table", table));
	rep.content = json_spirit::write(wrap, true);
}

void lsi_groups(const http::server::request &req,
						http::server::reply &rep,
						boost::cmatch& grps){
	json_spirit::Object wrap;
	json_spirit::Object table;
	std::string lsi_name = std::string(grps[1]);

	//Check if it exists;
	if(!switch_manager::exists_by_name(lsi_name)){
		//Throw 404
		std::stringstream ss;
		ss<<"Invalid lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	uint64_t dpid = switch_manager::get_switch_dpid(lsi_name);

	//Get flows
	std::list<openflow_group_mod_snapshot> group_mods;
	std::list<std::string> groups_str;
	switch_manager::get_switch_group_mods(dpid, group_mods);
	std::list<openflow_group_mod_snapshot>::const_iterator it;
	for(it=group_mods.begin(); it != group_mods.end(); ++it){
		std::stringstream ss;
		ss << *it;
		groups_str.push_back(ss.str());
	}

	json_spirit::Value groups_(groups_str.begin(), groups_str.end());
	table.push_back(json_spirit::Pair("groups", groups_));

	//Wrap
	wrap.push_back(json_spirit::Pair("group-table", table));
	rep.content = json_spirit::write(wrap, true);
}

void list_ctls(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){
	std::stringstream ss;
	std::string lsi_name;
	uint64_t dpid;
	std::list<rofl::cctlid> list;
	json_spirit::Object table;
	
	//Perform security checks
        if(!authorised(req,rep)) return;

	lsi_name = std::string(grps[1]);

	// Get dpid
	try{
		dpid = switch_manager::get_switch_dpid(lsi_name);
	}catch(...){
		ss<<"Unable to find a dpid for lsi " << lsi_name;
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}
	
	try{
		ss<<"Listing controllers from: " << lsi_name;
		switch_manager::rpc_list_ctls(dpid, &list);
	}catch(...){
		ss<<"Unable to get list of ctls from lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::internal_server_error;
		return;
	}

	//Return data
	std::list<std::string> list_str;
	for(std::list<rofl::cctlid>::iterator it = list.begin(); it != list.end(); it++){
		std::stringstream ss_tmp;
		ss_tmp << it->get_ctlid();
		list_str.push_back(ss_tmp.str());
	}

	json_spirit::Value ids_(list_str.begin(), list_str.end());
	table.push_back(json_spirit::Pair("ids", ids_));
	rep.content = json_spirit::write(table, true);
}

void show_ctl(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){
	std::stringstream ss;
	std::string lsi_name; //, ctl_id;
	uint64_t dpid, ctl_id;
	controller_snapshot ctl_info;
	json_spirit::Object wrap, list;
	
	//Perform security checks
        if(!authorised(req,rep)) return;

	lsi_name = std::string(grps[1]);
	ctl_id = atoi(std::string(grps[2]).c_str());


	// Get dpid
	try{
		dpid = switch_manager::get_switch_dpid(lsi_name);
	}catch(...){
		ss<<"Unable to find a dpid for lsi " << lsi_name;
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}
	
	try{
		ss<<"Showing Controller info: " << lsi_name;
		switch_manager::get_controller_info(dpid, ctl_id, ctl_info);
	}catch(...){
		ss<<"Unable to get Controller info for lsi " << lsi_name;
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}	

	std::list<std::string> conn_str;
	std::list<controller_conn_snapshot>::const_iterator it;
	for(it=ctl_info.conn_list.begin(); it != ctl_info.conn_list.end(); ++it){
		std::stringstream ss;
		ss << *it;
		conn_str.push_back(ss.str());
	}

	std::stringstream ctl_ss;
	ctl_ss << ctl_info;

	list.push_back(json_spirit::Pair("info", ctl_ss.str()));
	json_spirit::Value conns_(conn_str.begin(), conn_str.end());
	list.push_back(json_spirit::Pair("Connections", conns_));

	//Return data
	wrap.push_back(json_spirit::Pair("Controller", list));
	rep.content = json_spirit::write(wrap, true);
}

} //namespace get


namespace put{

void create_lsi(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){

	//Perform security checks
	if(!authorised(req,rep)) return;

	of_version_t ver = OF_VERSION_10;
	uint64_t dpid = 0;
	std::string lsi_name = "";
	unsigned int num_of_tables = 0;
	int ma_list[OF1X_MAX_FLOWTABLES] = { 0 };
	int reconnect_start_time = 1;
	enum rofl::csocket::socket_type_t socket_type = rofl::csocket::SOCKET_TYPE_PLAIN;
	rofl::cparams socket_params = rofl::csocket::get_default_params(socket_type);
	//bool pirl_enabled = true;
	//unsigned int pirl_rate;

	if(req.content.compare("") == 0){
		//Something went wrong
		std::stringstream ss;
		ss<<"Unable to create lsi; missing lsi description";
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}

	//Parse
	try{
		//Parse the input
		json_spirit::Value val;

		//First object must be "lsi"
		json_spirit::read(req.content, val);

		json_spirit::Object obj = val.get_obj();
		obj = json_spirit::find_value(obj, "lsi").get_obj();

		//Fill in parameters
		lsi_name = json_spirit::find_value(obj, "name").get_str();
		dpid = json_spirit::find_value(obj, "dpid").get_uint64();
		double ver_float = json_spirit::find_value(obj, "of-version").get_real();
		if( ver_float == 1.0 )
			ver = OF_VERSION_10;
		else if( ver_float == 1.2 )
			ver = OF_VERSION_12;
		else if( ver_float == 1.3)
			ver = OF_VERSION_13;
		else
			throw "Invalid OF version";

		num_of_tables = json_spirit::find_value(obj, "number-of-tables").get_int();

		//Parse matching algorithms if defined
		try{
			std::list<std::string> available_algorithms = switch_manager::list_matching_algorithms(ver);
			std::list<std::string>::iterator it;
			json_spirit::Array mas = json_spirit::find_value(obj, "matching-algorithms").get_array();

			if(num_of_tables != mas.size())
				throw -1;

			for( unsigned int i = 0; i < mas.size(); ++i ){
				it = std::find(available_algorithms.begin(), available_algorithms.end(), mas[i].get_str());

				if(it == available_algorithms.end())
					throw -1;

				ma_list[i] = std::distance(available_algorithms.begin(), it);
			}
		}catch(int& e){
			throw e;
		}catch(...){

		}
	}catch(...){
		//Something went wrong
		std::stringstream ss;
		ss<<"Unable to parse arguments for create lsi";
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}

	//Check if LSI exists;
	if(switch_manager::exists_by_name(lsi_name)){
		//Throw 404
		std::stringstream ss;
		ss<<"lsi '"<<lsi_name<<"' already exists";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}
	//Check if LSI exists (dpid;
	if(switch_manager::exists(dpid)){
		//Throw 404
		std::stringstream ss;
		ss<<"lsi with dpid '"<<dpid<<"' already exists";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	//Create
	try{

		switch_manager::create_switch(ver, dpid, lsi_name, num_of_tables, ma_list, reconnect_start_time, socket_type, socket_params);
	}catch(...){
		//Something went wrong
		std::stringstream ss;
		ss<<"Unable to create lsi '"<<lsi_name<<"', request: {"<<req.content<<"}";
		rep.content = ss.str();
		rep.status = http::server::reply::internal_server_error;
		return;
	}

	//There is no need to return anything
}


void add_ctl(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){

	//call switch_manager to add a new controller to the LSI
	// get IP & port from the req parameter (json?)
	// check IP & port (if there is already a controller there, send error message)
	std::stringstream ss;
	std::string proto, ip, port;
	enum rofl::csocket::socket_type_t socket_type;
	rofl::cparams socket_params;
	json_spirit::Object table;
	uint64_t assigned_id;
	
	//Perform security checks
        if(!authorised(req,rep)) return;

	std::string lsi_name = std::string(grps[1]);

	//Check if LSI exists;
        if(!switch_manager::exists_by_name(lsi_name)){
                //Throw 404
                std::stringstream ss;
                ss<<"Invalid lsi '"<<lsi_name<<"'";
                rep.content = ss.str();
                rep.status = http::server::reply::not_found;
                return;
        }

	// Get dpid TODO check for failure
	uint64_t dpid = switch_manager::get_switch_dpid(lsi_name);
	
	// Parse data (protocol, IP & port)
	try{
		//Parse the input
		json_spirit::Value val;

		//First object must be "lsi"
		json_spirit::read(req.content, val);

		json_spirit::Object obj = val.get_obj();
		obj = json_spirit::find_value(obj, "ctl").get_obj();

		//Fill in parameters
		proto = json_spirit::find_value(obj, "proto").get_str();
		ip = json_spirit::find_value(obj, "ip").get_str();
		port = json_spirit::find_value(obj, "port").get_str();

	}catch(...){
		//Something went wrong
		ss<<"Unable to parse arguments for add controller";
		rep.content = ss.str();
		rep.status = http::server::reply::bad_request;
		return;
	}


	//fprintf(stderr,"found proto: %s\n ip %s\n port %s\n", proto.c_str(), ip.c_str(), port.c_str());
	//TODO Check parameters received (proto, ip & port)
	if ( proto == "tcp" ){
		socket_type = rofl::csocket::SOCKET_TYPE_PLAIN;
	} else {
		//Other types not supported
		// TODO add SOCKET_TYPE_OPENSSL
		return;
	}
	socket_params = rofl::csocket::get_default_params(socket_type);
	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_HOSTNAME) = ip;
	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_PORT) = port;

	try{
		ss<<"Adding Controller: " << lsi_name << " : " << proto;
		assigned_id = switch_manager::rpc_connect_to_ctl(dpid, socket_type, socket_params);
	}catch(...){
		//Something went wrong
                ss<<"Unable to add controller to lsi '"<<lsi_name<<"'";
                rep.content = ss.str();
                rep.status = http::server::reply::internal_server_error;
                return;
	}
	
	//Return assigned ID
	table.push_back(json_spirit::Pair("assigned id", assigned_id));
	rep.content = json_spirit::write(table, true);
}

} //namespace put

namespace delete_{

void destroy_switch(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){

	//Perform security checks
	if(!authorised(req,rep)) return;

	std::string lsi_name = std::string(grps[1]);

	//Check if LSI exists;
	if(!switch_manager::exists_by_name(lsi_name)){
		//Throw 404
		std::stringstream ss;
		ss<<"Invalid lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::not_found;
		return;
	}

	//Get dpid
	uint64_t dpid = switch_manager::get_switch_dpid(lsi_name);

	//Destroy it
	try{
		switch_manager::destroy_switch(dpid);
	}catch(...){
		//Something went wrong
		std::stringstream ss;
		ss<<"Unable to destroy lsi '"<<lsi_name<<"'";
		rep.content = ss.str();
		rep.status = http::server::reply::internal_server_error;
		return;
	}
}

void rem_ctl(const http::server::request &req, http::server::reply &rep, boost::cmatch& grps){

	std::stringstream ss;
	std::string lsi_name;
	std::string ctlid_str;
	uint64_t dpid, ctl_id;

	//Perform security checks
	if(!authorised(req,rep)) return;	

	lsi_name = std::string(grps[1]);
	ctlid_str = std::string(grps[2]);

	//Check if LSI exists;
        if(!switch_manager::exists_by_name(lsi_name)){
                //Throw 404
                std::stringstream ss;
                ss<<"Invalid lsi '"<<lsi_name<<"'";
                rep.content = ss.str();
                rep.status = http::server::reply::not_found;
                return;
        }

	// Get dpid
	dpid = switch_manager::get_switch_dpid(lsi_name);

	// get Ctlid
	ctl_id = atoi(ctlid_str.c_str());
	rofl::cctlid ctlid(ctl_id);

	try{
		ss<<"Removing Controller: " << lsi_name << " : " << ctlid_str;
		switch_manager::rpc_disconnect_from_ctl(dpid, ctlid);
	}catch(...){
		//Something went wrong
                std::stringstream ss;
                ss<<"Unable to remove controller from lsi '"<<lsi_name<<"'";
                rep.content = ss.str();
                rep.status = http::server::reply::internal_server_error;
                return;
	}
}

} //namespace delete
} //namespace controllers
} //namespace xdpd
