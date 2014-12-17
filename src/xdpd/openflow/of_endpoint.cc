#include "of_endpoint.h"
#include "../management/system_manager.h"

using namespace rofl;
using namespace xdpd;

of_endpoint::of_endpoint(rofl::openflow::cofhello_elem_versionbitmap const& versionbitmap) : 
		crofbase(versionbitmap, system_manager::__get_ciosrv_tid()), 
		sw(NULL) {

};


