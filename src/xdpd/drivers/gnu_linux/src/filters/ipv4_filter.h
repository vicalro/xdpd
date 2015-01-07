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
#include "../config.h"

//C++ extern C
ROFL_BEGIN_DECLS

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

hal_result_t gnu_linux_enable_ipv4_frag_filter(const uint64_t dpid);
hal_result_t gnu_linux_disable_ipv4_frag_filter(const uint64_t dpid);
bool gnu_linux_ipv4_frag_filter_status(const uint64_t dpid);


hal_result_t gnu_linux_enable_ipv4_reas_filter(const uint64_t dpid);
hal_result_t gnu_linux_disable_ipv4_reas_filter(const uint64_t dpid);
bool gnu_linux_ipv4_reas_filter_status(const uint64_t dpid);


//C++ extern C
ROFL_END_DECLS


#endif /* GNU_LINUX_IPV4_FRAG_H_ */
