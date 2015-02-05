/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LS_INTERNAL_STATE_H_
#define LS_INTERNAL_STATE_H_

#include <stdbool.h>
#include "../config.h"
#include "../io/datapacket_storage.h"

/**
* @file ls_internal_state.h
* @author Tobias Jungel<tobias.jungel (at) bisdn.de>
* @author Marc Sune<marc.sune (at) bisdn.de>
* @author Victor Alvarez<victor.alvarez (at) bisdn.de>
* @brief Implements the internal (platform state) logical switch
* state
*/

//fwd decl
struct ipv4_reas_state;

namespace xdpd {
namespace gnu_linux_dpdk {

typedef struct switch_platform_state {
	
	//Packet storage pointer
	xdpd::gnu_linux::datapacket_storage* storage;

	//IPv4 fragmentation filter
	bool ip_frag_filter_status;

	//IPv4 reassembly filter on
	bool ip_reas_filter_status;

	//Reassembly state
	//struct ipv4_reas_state* reas_state;
}switch_platform_state_t;

}// namespace xdpd::gnu_linux
}// namespace xdpd

#endif /* LS_INTERNAL_STATE_H_ */
