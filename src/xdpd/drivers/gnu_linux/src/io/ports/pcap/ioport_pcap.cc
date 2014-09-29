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
//#include <pcap.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h> /* includes net/ethernet.h */
#include <net/if.h>
#include <sys/ioctl.h>

using namespace rofl;
using namespace xdpd::gnu_linux;


// Constructor
ioport_pcap::ioport_pcap(switch_port_t* of_ps, unsigned int num_queues) : ioport(of_ps, num_queues)
{
	descr = NULL;
}

//Destructor
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
	struct pcap_pkthdr *pcap_hdr;     /* pcap.h */
	int ret;

	datapacket_t *pkt;
	datapacketx86 *pkt_x86;

	//Check if we really have to read
	if(!of_port_state->up || of_port_state->drop_received){
		return NULL;
	}

	//packet = pcap_next(descr,&pcap_hdr);
	if(descr == NULL){
		return NULL;
	}

	if((ret = pcap_next_ex(descr, &pcap_hdr, &packet)) < 1){

		if(ret == 0){
			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] pcap_next_ex() = %i -> No packet read, timeout expired %s\n", of_port_state->name, ret, pcap_geterr(descr));
			return NULL;
		}else if (ret == -1){
			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] pcap_next_ex() = %i -> Error reading %s\n", of_port_state->name, ret, pcap_geterr(descr));
			return NULL;
		}

	}

	ROFL_DEBUG(DRIVER_NAME"[pcap] pcap_next_ex() Packet received\n");

	//Retrieve buffer from pool: this is a non-blocking call
	pkt = bufferpool::get_free_buffer_nonblocking();

	//Handle no free buffer
	if(!pkt){
		return NULL;
	}

	pkt_x86 = (datapacketx86*) pkt->platform_state;
	pkt_x86->init((uint8_t*)packet, pcap_hdr->len, of_port_state->attached_sw, get_port_no(), 0);

	return pkt;

}

unsigned int ioport_pcap::write(unsigned int q_id, unsigned int num_of_buckets){

	return 0;

}

/*
*
* Enable and down port routines
*
*/
rofl_result_t ioport_pcap::up() {

	struct ifreq ifr;
	int sd, rc;
	struct ethtool_value eval;
	char errbuf[PCAP_ERRBUF_SIZE];
	errbuf[0] = '\0';
	int nonblock, status;

	ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Trying to bring up\n",of_port_state->name);

	if ((sd = socket(AF_PACKET, SOCK_RAW, 0)) < 0){
		return ROFL_FAILURE;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strcpy(ifr.ifr_name, of_port_state->name);

	if ((rc = ioctl(sd, SIOCGIFINDEX, &ifr)) < 0){
		return ROFL_FAILURE;
	}

	/*
	* Make sure we are disabling Generic and Large Receive Offload from the NIC.
	* This screws up the pcap
	*/

	//First retrieve the current gro setup, so that we can gently
	//inform the user we are going to disable (and not set it back)
	eval.cmd = ETHTOOL_GGRO;
	ifr.ifr_data = (caddr_t)&eval;
	eval.data = 0;//Make valgrind happy

	if (ioctl(sd, SIOCETHTOOL, &ifr) < 0) {
		ROFL_WARN(DRIVER_NAME"[pcap:%s] Unable to detect if the Generic Receive Offload (GRO) feature on the NIC is enabled or not. Please make sure it is disabled using ethtool or similar...\n", of_port_state->name);

	}else{
		//Show nice messages in debug mode
		if(eval.data == 0){
			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] GRO already disabled.\n", of_port_state->name);
		}else{
			//Do it
			eval.cmd = ETHTOOL_SGRO;
			eval.data = 0;
			ifr.ifr_data = (caddr_t)&eval;

			if (ioctl(sd, SIOCETHTOOL, &ifr) < 0) {
				ROFL_ERR(DRIVER_NAME"[pcap:%s] Could not disable Generic Receive Offload feature on the NIC. This can be potentially dangeros...be advised!\n",  of_port_state->name);
			}else{
				ROFL_DEBUG(DRIVER_NAME"[pcap:%s] GRO successfully disabled.\n", of_port_state->name);
			}

		}
	}

	//Now LRO
	eval.cmd = ETHTOOL_GFLAGS;
	ifr.ifr_data = (caddr_t)&eval;
	eval.data = 0;//Make valgrind happy

	if (ioctl(sd, SIOCETHTOOL, &ifr) < 0) {
		ROFL_WARN(DRIVER_NAME"[pcap:%s] Unable to detect if the Large Receive Offload (LRO) feature on the NIC is enabled or not. Please make sure it is disabled using ethtool or similar...\n", of_port_state->name);
	} else {
		if ((eval.data & ETH_FLAG_LRO) == 0) {
			//Show nice messages in debug mode
			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] LRO already disabled.\n", of_port_state->name);
		} else {
			//Do it
			eval.cmd = ETHTOOL_SFLAGS;
			eval.data = (eval.data & ~ETH_FLAG_LRO);
			ifr.ifr_data = (caddr_t)&eval;

			if (ioctl(sd, SIOCETHTOOL, &ifr) < 0){
				ROFL_ERR(DRIVER_NAME"[pcap:%s] Could not disable Large Receive Offload (LRO) feature on the NIC. This can be potentially dangeros...be advised!\n",  of_port_state->name);
			}else{
				ROFL_DEBUG(DRIVER_NAME"[pcap:%s] LRO successfully disabled.\n", of_port_state->name);
			}
		}
	}

	//Recover MTU
	memset((void*)&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, of_port_state->name, sizeof(ifr.ifr_name));

	if(ioctl(sd, SIOCGIFMTU, &ifr) < 0) {
		ROFL_ERR(DRIVER_NAME"[pcap:%s] Could not retreive MTU value from NIC. Default %u Max Packet Size(MPS) size will be used (%u total bytes). Packets exceeding this size will be DROPPED (Jumbo frames).\n",  of_port_state->name, (PORT_DEFAULT_PKT_SIZE-PORT_ETHER_LENGTH), PORT_DEFAULT_PKT_SIZE);
		mps = PORT_DEFAULT_PKT_SIZE;
	}else{
		mps = ifr.ifr_mtu+PORT_ETHER_LENGTH;
		ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Discovered Max Packet Size(MPS) of %u.\n",  of_port_state->name, mps);
	}

	//Recover flags
	if ((rc = ioctl(sd, SIOCGIFFLAGS, &ifr)) < 0){
		close(sd);
		return ROFL_FAILURE;
	}

	//descr = pcap_open_live(of_port_state->name,BUFSIZ,1,-1,errbuf);
	descr = pcap_create(of_port_state->name,errbuf);

	if(descr == NULL){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_create() %s\n", errbuf);
		assert(0);
		return ROFL_FAILURE;
	}

	/*
	if(pcap_set_buffer_size(descr,BUFSIZ) != 0)
	ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_set_buffer_size() %s\n", errbuf);
	*/
	if(pcap_set_promisc(descr,1) != 0){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_set_promisc() %s\n", errbuf);
	}
	if(pcap_set_timeout(descr,0) != 0){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_set_timeout() %s\n", errbuf);
	}

	//TODO
	//ifdef depending on libpcap version
	if(pcap_set_immediate_mode(descr,1) != 0)
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_set_immediate_mode() %s\n", errbuf);

	//Set state as up
	of_port_state->up = true;

	//Check if is up or not
	if (IFF_UP & ifr.ifr_flags){

		//Activate and non block
		status = pcap_activate(descr);
		if (status != 0){
			ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_activate() with status %i %s\n", status, pcap_geterr(descr));
			assert(0);
			return ROFL_FAILURE;
		}

		//Set non-blocking
		nonblock = pcap_setnonblock(descr,1,errbuf);

		if(nonblock != 0){
			ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_setnonblock()\n");
			return ROFL_FAILURE;
		}

		//Already up.. Silently skip
		close(sd);
		return ROFL_SUCCESS;
	}

	//Prevent race conditions with LINK/STATUS notification threads (bg)
	pthread_rwlock_wrlock(&rwlock);

	ifr.ifr_flags |= IFF_UP;
	if ((rc = ioctl(sd, SIOCSIFFLAGS, &ifr)) < 0){
		ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Unable to bring interface down via ioctl\n",of_port_state->name);
		close(sd);
		pthread_rwlock_unlock(&rwlock);
		return ROFL_FAILURE;
	}

	//Release mutex
	pthread_rwlock_unlock(&rwlock);

	//Activate and non block
	status = pcap_activate(descr);
	if (status != 0){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_activate() with status %i %s\n", status, pcap_geterr(descr));
		assert(0);
		return ROFL_FAILURE;
	}

	//Set non-blocking
	nonblock = pcap_setnonblock(descr,1,errbuf);

	if(nonblock != 0){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_setnonblock()\n");
		return ROFL_FAILURE;
	}
	close(sd);

	return ROFL_SUCCESS;

}

rofl_result_t ioport_pcap::down() {

	struct ifreq ifr;
	int sd, rc;

	ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] Trying to bring down\n",of_port_state->name);

	if ((sd = socket(AF_PACKET, SOCK_RAW, 0)) < 0) {
		return ROFL_FAILURE;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strcpy(ifr.ifr_name, of_port_state->name);

	if ((rc = ioctl(sd, SIOCGIFINDEX, &ifr)) < 0) {
		return ROFL_FAILURE;
	}

	if ((rc = ioctl(sd, SIOCGIFFLAGS, &ifr)) < 0) {
		close(sd);
		return ROFL_FAILURE;
	}

	of_port_state->up = false;

	if ( !(IFF_UP & ifr.ifr_flags) ) {
		close(sd);
		//Already down.. Silently skip
		return ROFL_SUCCESS;
	}

	//Prevent race conditions with LINK/STATUS notification threads (bg)
	pthread_rwlock_wrlock(&rwlock);

	ifr.ifr_flags &= ~IFF_UP;

	if ((rc = ioctl(sd, SIOCSIFFLAGS, &ifr)) < 0) {
		ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Unable to bring interface down via ioctl\n",of_port_state->name);
		close(sd);
		pthread_rwlock_unlock(&rwlock);
		return ROFL_FAILURE;
	}

	//Release mutex
	pthread_rwlock_unlock(&rwlock);

	close(sd);

	return ROFL_SUCCESS;


}
