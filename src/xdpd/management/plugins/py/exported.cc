#include "../../plugin_manager.h"
#include "../../port_manager.h"
#include "../../switch_manager.h"
#include "exported.h"


/*
std::vector<plugin*> xdpd::py_proxy::plugin_manager::get_plugins(void){
	return xdpd::plugin_manager::get_plugins();
}*/	


//Port manager
bool xdpd::py_proxy::port_manager::exists(std::string& port_name){
        return port_manager::exists(port_name);
}

bool xdpd::py_proxy::port_manager::is_vlink(std::string& port_name){
        return port_manager::is_vlink(port_name);
}

std::string xdpd::py_proxy::port_manager::get_vlink_pair(std::string& port_name){
        return port_manager::get_vlink_pair(port_name);
}
std::set<std::string> xdpd::py_proxy::port_manager::list_available_port_names(bool include_blacklisted){
	fprintf(stderr,"Hey");
        return xdpd::port_manager::list_available_port_names(include_blacklisted);
}
