#include "ioport_pf_ring.h"
#include <sched.h>
#include "../../bufferpool.h"
#include "../../datapacketx86.h"
#include "../../../util/likely.h"
#include "../../iomanager.h"

#include <linux/ethtool.h>
#include <rofl/common/utils/c_logger.h>
#include <rofl/common/protocols/fetherframe.h>
#include <rofl/common/protocols/fvlanframe.h>

//Profiling
#include "../../../util/time_measurements.h"
#include "../../../config.h"

using namespace rofl;
using namespace xdpd::gnu_linux;

//Constructor and destructor
ioport_pf_ring::ioport_pf_ring(
		switch_port_t* of_ps,
		unsigned int num_queues) :
			ioport(of_ps, num_queues)
{


}


ioport_pf_ring::~ioport_pf_ring()
{

}

//Read and write methods over port
void ioport_pf_ring::enqueue_packet(datapacket_t* pkt, unsigned int q_id){

}


// handle read
datapacket_t* ioport_pf_ring::read(){

	return NULL;
}

unsigned int ioport_pf_ring::write(unsigned int q_id, unsigned int num_of_buckets){
	//Unnecessary
	
	return 0;
}

/*
*
* Enable and down port routines
*
*/
rofl_result_t ioport_pf_ring::up() {
	return ROFL_FAILURE;
}

rofl_result_t ioport_pf_ring::down() {
	return ROFL_FAILURE;
}
