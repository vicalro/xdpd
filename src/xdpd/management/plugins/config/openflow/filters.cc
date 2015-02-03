#include "filters.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <algorithm>
#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_pipeline.h>
#include "../../../switch_manager.h"

using namespace xdpd;
using namespace rofl;

//Constants
#define FILTER_IPV4_FRAG "ipv4-frag-enabled"
#define FILTER_IPV4_REAS "ipv4-reas-enabled"
// [+] Add more here...


const std::string filters_scope::SCOPE_NAME = "filters";

filters_scope::filters_scope(scope* parent):scope(SCOPE_NAME, parent, false){

	//Register available filter extensions
	register_parameter(FILTER_IPV4_FRAG, false);
	register_parameter(FILTER_IPV4_REAS, false);

	// [+] Add more here...
}

void filters_scope::post_validate(libconfig::Setting& setting, bool dry_run){
	
	if(!dry_run){
		//IPv4 fragmentation
		if(setting.exists(FILTER_IPV4_FRAG) && setting[FILTER_IPV4_FRAG])
			ipv4_frag_enabled = true;
		else
			ipv4_frag_enabled = false;
		//IPv4 reassembly
		if(setting.exists(FILTER_IPV4_REAS) && setting[FILTER_IPV4_REAS])
			ipv4_reas_enabled = true;
		else
			ipv4_reas_enabled = false;

		// [+] Add more here...
	}
}

void filters_scope::setup_filters(uint64_t& dpid){

	//IPv4 fragmentation
	if(ipv4_frag_enabled)
		switch_manager::extensions.enable_ipv4_fragmentation(dpid);
	//IPv4 reassembly
	if(ipv4_reas_enabled)
		switch_manager::extensions.enable_ipv4_reassembly(dpid);

	// [+] Add more here...
}
