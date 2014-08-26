#ifndef EXPORTED_H
#define EXPORTED_H 

/**
* @file exported.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Exported APIs 
* 
*/



namespace xdpd{
namespace py_proxy{

//TODO: remove
	class hola{
		
	public:
		unsigned int py_sum_test(unsigned int x);
	};	
//TODO: remove


/**
* Exported 
*/

	class port_manager{

	public:
                static bool exists(std::string& port_name);
                static bool is_vlink(std::string& port_name);
                static std::string get_vlink_pair(std::string& port_name);
                static std::set<std::string> list_available_port_names(bool include_blacklisted=false);
        };

	class plugin_manager{


	public:
		/**
		* Get the list of plugins 
		*/
		//static std::vector<plugin*> get_plugins(void);


	};	


}; //end of xdpd py_proxy 
}; //end of xdpd namespace


#endif /* EXPORTED_H_ */


