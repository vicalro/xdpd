/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONTROLLER_SNAPSHOT_H
#define CONTROLLER_SNAPSHOT_H 

//#include <iostream> 
//#include <string> 
//#include <list> 
//#include <rofl_datapath.h>
//#include <rofl/common/caddress.h>
//#include <rofl/datapath/pipeline/switch_port.h>

/**
* @file ctl_snapshot.h 
* @author Victor Alvarez<victor.alvarez (at) bisdn.de>
*
* @brief C++ Controller snapshot
*/

namespace xdpd {

class controller_conn_snapshot {
public:

	uint64_t id;

	typedef enum {
		PROTOCOL_UNKNOWN,
		PROTOCOL_PLAIN,
		PROTOCOL_SSL,
	}protocol_type_t;

	/**
	* Protocol used: Plain/SSL
	*/
	protocol_type_t proto_type;
	
	/**
	* IP
	*/
	std::string ip;

	/**
	* Port
	*/
	uint64_t port;

 	//Dumping operator
	friend std::ostream& operator<<(std::ostream& os, const controller_conn_snapshot& c){
		os << "id: " << c.id << ", type: ";
		switch (c.proto_type){
			case rofl::csocket::SOCKET_TYPE_UNKNOWN:
                                os << "Unkown";
                                break;
                        case rofl::csocket::SOCKET_TYPE_PLAIN:
                                os << "Plain";
                                break;
                        case rofl::csocket::SOCKET_TYPE_OPENSSL:
                                os << "SSL";
                                break;
                        default:
				assert(0);
				os << "type error";
                                break;

		}
		os << ", IP: " << c.ip << ", port: " << c.port;
		return os;
	}
};

/**
* @brief C++ controller snapshot 
* @ingroup cmm_mgmt
*/
class controller_snapshot {

public:	

	uint64_t id;

	// Controller Mode
	typedef enum {
		CONTROLLER_MODE_MASTER,
		CONTROLLER_MODE_SLAVE
	}controller_role_t;

	/**
	* Role ( Master / slave )
	*/
	controller_role_t role;

	/**
	* Status of the controller
	*/
	bool connected;

	/**
	* List of connections
	*/
	std::list<controller_conn_snapshot> conn_list;

 	//Dumping operator
	friend std::ostream& operator<<(std::ostream& os, const controller_snapshot& c)
	{
		//TODO: Improve output 
		os << "{ id: " << c.id << ", Channel status: ";
		if (c.connected){
			os << " Established";
		}else{
			os << " Not established";
		}

		os << ", Mode: ";
		if (c.role == CONTROLLER_MODE_MASTER){
			os << "Master";
		}else{
			os << "Slave";
		}
		os << "}";
		return os;
	}

};


}// namespace xdpd

#endif /* CTL_SNAPSHOT_H_ */
