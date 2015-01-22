/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GNU_LINUX_IPV4_FRAG_H
#define GNU_LINUX_IPV4_FRAG_H

#include <stdio.h>
#include <rofl.h>
#include <rofl/datapath/hal/extensions/frag.h>
#include <rofl/datapath/hal/driver.h>
#include <rofl/common/utils/c_logger.h>
#include <rofl/datapath/hal/cmm.h>
#include <rofl/datapath/pipeline/platform/memory.h>
#include <rofl/datapath/pipeline/physical_switch.h>
#include <rofl/datapath/pipeline/openflow/openflow1x/of1x_switch.h>

#include "../processing/ls_internal_state.h"
#include "../io/packet_classifiers/pktclassifier.h"
#include "../config.h"

//C++ extern C
ROFL_BEGIN_DECLS

static inline void gnu_linux_ipv4_set_offset(cpc_ipv4_hdr_t* ipv4, uint16_t val){
	*((uint16_t*)ipv4->offset_flags) |= HTONB16(val&0x1FFF);
}

static inline uint16_t gnu_linux_ipv4_get_offset(cpc_ipv4_hdr_t* ipv4){
	return NTOHB16((*(uint16_t*)ipv4->offset_flags))&0x1FFF;
}


/**
* @brief Fragment IPv4 packet
*
* It attempts to fragment an IPv4 packet, according to the MPS, and places the
* fragments in the frag array.
*
* The caller MUST ensure the packet is an IPv4 packet before calling this function
*
* On success, the fragments are placed in the frags array, and nof is set to the
* number of fragments.
*
* On failure, nof is set to 0.
*
*/
void gnu_linux_frag_ipv4_pkt(datapacket_t** pkt, unsigned int mps, unsigned int* nof, datapacket_t** frags);

//mgmt routines
hal_result_t gnu_linux_enable_ipv4_frag_filter(const uint64_t dpid);
hal_result_t gnu_linux_disable_ipv4_frag_filter(const uint64_t dpid);
bool gnu_linux_ipv4_frag_filter_status(const uint64_t dpid);


/**
* @brief Reasemble IPv4 packet
*
* On success the reasembled, already classified packet is returned. On partial
* reasembly, NULL is returned, and *pkt is also set to NULL.
*
* The implementation is optimized for *ordered* reassemblies.
*/
datapacket_t* gnu_linux_reas_ipv4_pkt(of_switch_t* sw, datapacket_t** pkt, cpc_ipv4_hdr_t* ipv4);

//mgmt routines
hal_result_t gnu_linux_enable_ipv4_reas_filter(const uint64_t dpid);
hal_result_t gnu_linux_disable_ipv4_reas_filter(const uint64_t dpid);
bool gnu_linux_ipv4_reas_filter_status(const uint64_t dpid);

//C++ extern C
ROFL_END_DECLS


#endif /* GNU_LINUX_IPV4_FRAG_H_ */
