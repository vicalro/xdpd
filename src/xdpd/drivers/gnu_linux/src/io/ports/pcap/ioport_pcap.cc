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
#include <stdlib.h>
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

	int rc;

	//Open pipe for output signaling on enqueue
	rc = pipe(notify_pipe);
	(void)rc; // todo use the value

	//Set non-blocking read/write in the pipe
	for(unsigned int i=0;i<2;i++){
		int flags = fcntl(notify_pipe[i], F_GETFL, 0);	///get current file status flags
		flags |= O_NONBLOCK;				//turn off blocking flag
		fcntl(notify_pipe[i], F_SETFL, flags);		//set up non-blocking read
	}
}

//Destructor
ioport_pcap::~ioport_pcap()
{
	if(descr != NULL)
		pcap_close(descr);
}

//Read and write methods over port
void ioport_pcap::enqueue_packet(datapacket_t* pkt, unsigned int q_id)
{


	unsigned int len;

	datapacketx86* pkt_x86 = (datapacketx86*) pkt->platform_state;
	len = pkt_x86->get_buffer_length();

	if ( likely(of_port_state->up) &&
		likely(of_port_state->forward_packets) &&
		likely(len >= MIN_PKT_LEN) ) {

		//Safe check for q_id
		if( unlikely(q_id >= get_num_of_queues()) ){
			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Packet(%p) trying to be enqueued in an invalid q_id: %u\n",  of_port_state->name, pkt, q_id);
			q_id = 0;
			bufferpool::release_buffer(pkt);
			assert(0);
		}
#ifndef IO_PCAP_BYPASS_TX
		//Store on queue and exit. This is NOT copying it to the pcap buffer
		if(output_queues[q_id]->non_blocking_write(pkt) != ROFL_SUCCESS){
			//TM_STAMP_STAGE(pkt, TM_SA5_FAILURE);

			ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Packet(%p) dropped. Congestion in output queue: %d\n",  of_port_state->name, pkt, q_id);
			//Drop packet
			bufferpool::release_buffer(pkt);

#ifndef IO_KERN_DONOT_CHANGE_SCHED
			//Force descheduling (prioritize TX)
			sched_yield();
#endif //IO_KERN_DONOT_CHANGE_SCHED
			return;
		}
		//TM_STAMP_STAGE(pkt, TM_SA5_SUCCESS);

		ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] Packet(%p) enqueued, buffer size: %d\n",  of_port_state->name, pkt, output_queues[q_id]->size());

		//WRITE to pipe
		const char c='a';
		int ret;
		ret = ::write(notify_pipe[WRITE],&c,sizeof(c));
		(void)ret; // todo use the value
#else
		//if(pcap_inject(descr,pkt_x86->get_buffer(),pkt_x86->get_buffer_length()) == -1)
		//	ROFL_ERR(DRIVER_NAME"[pcap:%s] ERROR while sending packets: %s. Size of the packet -> %i.\n", of_port_state->name, pcap_geterr(descr),pkt_x86->get_buffer_length());
		//int s;
		//s = pcap_inject(descr,pkt_x86->get_buffer(),pkt_x86->get_buffer_length());
		//ROFL_DEBUG(DRIVER_NAME"[pcap:%s] pcap_inject wrote: %d bytes\n",  of_port_state->name, s);
		pcap_inject(descr,pkt_x86->get_buffer(),pkt_x86->get_buffer_length());
		bufferpool::release_buffer(pkt);

#endif //IO_PCAP_BYPASS_TX

	} else {

		if(len < MIN_PKT_LEN){
			ROFL_ERR(DRIVER_NAME"[pcap:%s] ERROR: attempt to send invalid packet size for packet(%p) scheduled for queue %u. Packet size: %u\n", of_port_state->name, pkt, q_id, len);
			assert(0);
		}else{
			ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] dropped packet(%p) scheduled for queue %u\n", of_port_state->name, pkt, q_id);
		}

		//Drop packet
		bufferpool::release_buffer(pkt);
	}

}

inline void ioport_pcap::empty_pipe(){
	int ret;

	if(unlikely(deferred_drain == 0)){
		return;
	}
	//Just take deferred_drain from the pipe
	if(deferred_drain > IO_IFACE_RING_SLOTS){
		ret = ::read(notify_pipe[READ], draining_buffer, IO_IFACE_RING_SLOTS);
	}else{
		ret = ::read(notify_pipe[READ], draining_buffer, deferred_drain);
	}

	if(ret > 0){
		deferred_drain -= ret;

		if(unlikely( deferred_drain< 0 ) ){
			assert(0); //Desynchronized
			deferred_drain = 0;
		}
	}
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
			//ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] pcap_next_ex() = %i -> No packet read, timeout expired %s\n", of_port_state->name, ret, pcap_geterr(descr));
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
		of_port_state->stats.rx_dropped++;
		return NULL;
	}

	pkt_x86 = (datapacketx86*) pkt->platform_state;

#ifndef IO_PCAP_BYPASS_TX
	pkt_x86->init((uint8_t*)packet, pcap_hdr->len, of_port_state->attached_sw, get_port_no(), 0);
#else
 pkt_x86->init((uint8_t*)packet, pcap_hdr->len, of_port_state->attached_sw, get_port_no(), 0, true, false);
#endif

	ROFL_DEBUG(DRIVER_NAME"[pcap:%s] Size of the packet -> %i.\n", of_port_state->name,pkt_x86->get_buffer_length());

	//Increment statistics&return
	of_port_state->stats.rx_packets++;
	of_port_state->stats.rx_bytes += pkt_x86->get_buffer_length();

	return pkt;

}

unsigned int ioport_pcap::write(unsigned int q_id, unsigned int num_of_buckets){

	//const u_char *packet;
	//struct pcap_pkthdr *pcap_hdr;
	datapacket_t *pkt;
	datapacketx86 *pkt_x86;
	unsigned int cnt = 0;
	int tx_bytes_local = 0;

	circular_queue<datapacket_t>* queue = output_queues[q_id];

	// read available packets from incoming buffer
	for ( ; 0 < num_of_buckets; --num_of_buckets ) {

		//Check
		if(queue->size() == 0){
			ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] no packet left in output_queue %u left, %u buckets left\n",
					of_port_state->name,
					q_id,
					num_of_buckets);
			break;
		}

		//Retrieve an empty slot in the TX ring
		//hdr = tx->get_free_slot();

		//Skip, TX is full
		//if(!hdr)
		//	break;

		//Retrieve the buffer
		pkt = queue->non_blocking_read();

		if(!pkt){
			ROFL_ERR(DRIVER_NAME"[pcap:%s] A packet has been discarded due to race condition on the output queue. Are you really running the TX group with a single thread? output_queue %u left, %u buckets left\n",
				of_port_state->name,
				q_id,
				num_of_buckets);

			assert(0);
			break;
		}

		//TM_STAMP_STAGE(pkt, TM_SA6);

		pkt_x86 = (datapacketx86*) pkt->platform_state;

		if(unlikely(pkt_x86->get_buffer_length() > mps)){
			//This should NEVER happen
			ROFL_ERR(DRIVER_NAME"[pcap:%s] Packet length above the Max Packet Size (MPS). Packet length: %u, MPS %u.. discarding\n", of_port_state->name, pkt_x86->get_buffer_length(), mps);
			assert(0);

			//Return buffer to the pool
			bufferpool::release_buffer(pkt);

			//Increment errors
			of_port_state->queues[q_id].stats.overrun++;
			of_port_state->stats.tx_dropped++;

			deferred_drain++;
			continue;
		}

		//TM_STAMP_STAGE(pkt, TM_SA7);

		//Return buffer to the pool
		bufferpool::release_buffer(pkt);

		//tx_bytes_local += hdr->tp_len;
		cnt++;
		deferred_drain++;
	}

	//Increment stats and return
	if (likely(cnt > 0)) {

		ROFL_DEBUG_VERBOSE(DRIVER_NAME"[pcap:%s] schedule %u packet(s) to be sent\n", __FUNCTION__, cnt);
		int sent = pcap_inject(descr,pkt_x86->get_buffer(),pkt_x86->get_buffer_length());
		ROFL_DEBUG(DRIVER_NAME"[pcap:%s] pcap_inject wrote: %d bytes\n",  of_port_state->name, sent);

		//int sent = pcap_sendpacket(descr,pkt_x86->get_buffer(),pkt_x86->get_buffer_length());
		// send packets in TX
		//if(unlikely(tx->send() != ROFL_SUCCESS)){
   	if (sent == -1){

			ROFL_ERR(DRIVER_NAME"[pcap:%s] ERROR while sending packets: %s. Size of the packet -> %i.\n", of_port_state->name, pcap_geterr(descr),pkt_x86->get_buffer_length());
			assert(0);
			of_port_state->stats.tx_errors += cnt;
			of_port_state->queues[q_id].stats.overrun += cnt;

		}

		//Increment statistics
		of_port_state->stats.tx_packets += cnt;
		of_port_state->stats.tx_bytes += tx_bytes_local;
		of_port_state->queues[q_id].stats.tx_packets += cnt;
		of_port_state->queues[q_id].stats.tx_bytes += tx_bytes_local;

	}

	//Empty reading pipe (batch)
	empty_pipe();

	// return not used buckets
	return num_of_buckets;

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

/* pcap_open_live = pcap_create + pcap_set_options + pcap_activate */
/* descr = pcap_open_live(of_port_state->name,BUFSIZ,1,-1,errbuf); */

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
/* Check if libpcap supports the immediate mode. Bypass libpcap bug of reading after version 1.5.0 */
#ifdef PCAP_SET_IMMEDIATE_MODE
	if(pcap_set_immediate_mode(descr,1) != 0){
		ROFL_ERR(DRIVER_NAME"[pcap] Error pcap_set_immediate_mode() %s\n", errbuf);
	}
#endif
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
