#include "ioport_pcap.h"
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

//Added for pcap
#include <stdio.h>
//#include <string.h>
#include <stdlib.h>
#include <pcap.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h> /* includes net/ethernet.h */
#include <net/if.h>
#include <sys/ioctl.h>

using namespace rofl;
using namespace xdpd::gnu_linux;



// Constructor and destructor
ioport_pcap::ioport_pcap(switch_port_t* of_ps, unsigned int num_queues) : ioport(of_ps, num_queues)
{

	/* grab a device to peak into... */
	dev = pcap_lookupdev(errbuf);
	/* open the device for sniffing. */
	descr = pcap_open_live(dev,BUFSIZ,1,0,errbuf);

}

ioport_pcap::~ioport_pcap()
{
	pcap_close(descr);
}


//Read and write methods over port
void ioport_pcap::enqueue_packet(datapacket_t* pkt, unsigned int q_id){



}

// handle read
datapacket_t* ioport_pcap::read(){

	const u_char *packet;
	struct pcap_pkthdr pcap_hdr;     /* pcap.h */

	datapacket_t *pkt;
	datapacketx86 *pkt_x86;

	packet = pcap_next(descr,&pcap_hdr);

  	if(packet != NULL){

		//Retrieve buffer from pool: this is a non-blocking call
		pkt = bufferpool::get_free_buffer_nonblocking();
	
		//Handle no free buffer
		if(!pkt) {

			return NULL;
		}

		pkt_x86 = (datapacketx86*) pkt->platform_state;

		/* Fill packet
		#ifdef TP_STATUS_VLAN_VALID
		if(hdr->tp_status&TP_STATUS_VLAN_VALID){
		#else
		if(hdr->tp_vlan_tci != 0) {
       		#endif			
			//There is a VLAN
			fill_vlan_pkt(hdr, pkt_x86);	
		}else{*/
			// no vlan tag present
		//pkt_x86->init((uint8_t*)hdr + hdr->tp_mac, hdr->tp_len, of_port_state->attached_sw, get_port_no(), 0);
		//}
		pkt_x86->init((uint8_t*)packet, pcap_hdr->len, of_port_state->attached_sw, get_port_no(), 0);	
		//free pcap_hdr, packet


  	}else{
		return NULL;
  	}

	return pkt;

}

unsigned int ioport_pcap::write(unsigned int q_id, unsigned int num_of_buckets){
	





}

/*
*
* Enable and down port routines
*
*/
rofl_result_t ioport_pcap::up() {
	return ROFL_FAILURE;
}

rofl_result_t ioport_pcap::down() {
	return ROFL_FAILURE;
}
