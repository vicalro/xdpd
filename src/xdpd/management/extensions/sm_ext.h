/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SWITCH_MANAGER_EXTENSIONS_H
#define SWITCH_MANAGER_EXTENSIONS_H

#include <stdbool.h>
#include <inttypes.h>

namespace xdpd {

/**
* @brief Logical Switch (LS) extensions management API.
*
* @ingroup cmm_mgmt
*/

class switch_manager_extensions {

public:
	//
	// IPv4 fragmentation
	//

	/**
	 * @brief Enable IPv4 fragmentation in the LSI with dpid
	 */
	static void enable_ipv4_fragmentation(const uint64_t dpid);

	/**
	 * @brief Disable IPv4 fragmentation in the LSI with dpid
	 */
	static void disable_ipv4_fragmentation(const uint64_t dpid);

	/**
	 * @brief Get IPv4 fragmentation filter status
	 */
	static bool ipv4_fragmentation_status(const uint64_t dpid);

	//
	// IPv4 reassembly
	//

	/**
	 * @brief Enable IPv4 reassembly filter in the LSI with dpid
	 */
	static void enable_ipv4_reassembly(const uint64_t dpid);

	/**
	 * @brief Enable IPv4 reassembly filter in the LSI with dpid
	 */
	static void disable_ipv4_reassembly(const uint64_t dpid);

	/**
	 * @brief Get IPv4 reassembly filter status
	 */
	static bool ipv4_reassembly_status(const uint64_t dpid);
};

}// namespace xdpd


#endif /* SWITCH_MANAGER_EXTENSIONS_H_ */
