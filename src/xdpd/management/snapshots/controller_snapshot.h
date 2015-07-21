/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONTROLLER_SNAPSHOT_H
#define CONTROLLER_SNAPSHOT_H 

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

	std::string get_proto_type_str(void) const {
		switch (this->proto_type){
			case rofl::csocket::SOCKET_TYPE_UNKNOWN:
                                return "unkown";
                        case rofl::csocket::SOCKET_TYPE_PLAIN:
                                return "plain";
                        case rofl::csocket::SOCKET_TYPE_OPENSSL:
                                return "ssl";
                        default:
				assert(0);
				return "type error";
		}
	}
 	//Dumping operator
	friend std::ostream& operator<<(std::ostream& os, const controller_conn_snapshot& c){
		os << "id: " << c.id << ", type: ";
		os << c.get_proto_type_str();
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
		CONTROLLER_MODE_EQUAL,
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

	std::string get_status_str(void) const {
		if (this->connected)
			return "established";
		else
			return "not-established";
	}

	std::string get_role_str(void) const {
		if (this->role == CONTROLLER_MODE_EQUAL)
			return "equal";
		else if (this->role == CONTROLLER_MODE_MASTER)
			return "master";
		else if (this->role == CONTROLLER_MODE_SLAVE)
			return "slave";
		else{
			assert(0);
			return "error";
		}
	}

 	//Dumping operator
	friend std::ostream& operator<<(std::ostream& os, const controller_snapshot& c)
	{
		//TODO: Improve output 
		os << "{ id: " << c.id << ", Channel status: " << c.get_status_str();
		os << ", Mode: " << c.get_role_str();
		os << "}";
		return os;
	}

};


}// namespace xdpd

#endif /* CTL_SNAPSHOT_H_ */
