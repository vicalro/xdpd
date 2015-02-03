#ifndef CONFIG_OPENFLOW_FILTERS_PLUGIN_H
#define CONFIG_OPENFLOW_FILTERS_PLUGIN_H

#include <vector>
#include <libconfig.h++>
#include "../config.h"

/**
* @file filters.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Parse Filters hierarchy
*
*/

namespace xdpd {

class filters_scope:public scope {

public:
	filters_scope(scope* parent);

	//Setup filters
	void setup_filters(uint64_t& dpid);

	//Scope name
	static const std::string SCOPE_NAME;
protected:
	virtual void post_validate(libconfig::Setting& setting, bool dry_run);

	//Flags
	bool ipv4_frag_enabled;
	bool ipv4_reas_enabled;
};

}// namespace xdpd

#endif /* CONFIG_OPENFLOW_FILTERS_PLUGIN_H_ */
