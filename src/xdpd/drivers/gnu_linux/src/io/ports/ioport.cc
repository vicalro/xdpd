/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ioport.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <rofl/common/utils/c_logger.h>
#include "../bufferpool.h"
#include "../datapacketx86.h"
#include "../packet_classifiers/pktclassifier.h"
#include "../../filters/ipv4_filter.h"

using namespace xdpd::gnu_linux;

//Constructor and destructor
ioport::ioport(switch_port_t* of_ps, unsigned int q_num){

	//Output queues
	num_of_queues = q_num;

	//of_port_state
	of_port_state = of_ps;

	//Maximum packet size
	mps = NO_MPS;

	//Copy MAC address
	memcpy(mac, of_ps->hwaddr, ETHER_MAC_LEN);

	//Initialize input queue
	input_queue = new circular_queue<datapacket_t>(IO_IFACE_RING_SLOTS);

	for(int i=0;i<IO_IFACE_NUM_QUEUES;++i)
		output_queues[i] = new circular_queue<datapacket_t>(IO_IFACE_RING_SLOTS);

	//Initalize pthread rwlock
	if(pthread_rwlock_init(&rwlock, NULL) < 0){
		//Can never happen...
		ROFL_ERR(DRIVER_NAME" Unable to initialize ioport's rwlock\n");
		assert(0);
	}
}
ioport::~ioport(){
	//Drain queues first
	drain_queues();

	delete input_queue;
	for(int i=0;i<IO_IFACE_NUM_QUEUES;++i)
		delete output_queues[i];
}

/**
* Wipes output and input queues
*/
void ioport::drain_queues(){
	datapacket_t* pkt;

	//Drain input queue
	while( ( pkt = input_queue->non_blocking_read() ) != NULL){
		bufferpool::release_buffer(pkt);
	}

	for(int i=0;i<IO_IFACE_NUM_QUEUES;++i){
		while( ( pkt = output_queues[i]->non_blocking_read() ) != NULL){
			bufferpool::release_buffer(pkt);
		}
	}
}

/**
 * Sets the port receiving behaviour. This MUST change the of_port_state appropiately
 */
rofl_result_t ioport::set_drop_received_config(bool drop_received){
	of_port_state->drop_received = drop_received;
	return ROFL_SUCCESS;
}

/**
 * Sets the port flood output behaviour. This MUST change the of_port_state appropiately
 */
rofl_result_t ioport::set_no_flood_config(bool no_flood){
	of_port_state->no_flood = no_flood;
	return ROFL_SUCCESS;
}

/**
 * Sets the port output behaviour. This MUST change the of_port_state appropiately
 */
rofl_result_t ioport::set_forward_config(bool forward_packets){
	of_port_state->forward_packets = forward_packets;
	return ROFL_SUCCESS;
}

/**
 * Sets the port Openflow specific behaviour for non matching packets (PACKET_IN). This MUST change the of_port_state appropiately
 */
rofl_result_t ioport::set_generate_packet_in_config(bool generate_pkt_in){
	of_port_state->of_generate_packet_in = generate_pkt_in;
	return ROFL_SUCCESS;
}

/**
 * Sets the port advertised features. This MUST change the of_port_state appropiately
 */
rofl_result_t ioport::set_advertise_config(uint32_t advertised){
	of_port_state->advertised = (port_features_t)advertised;
	return ROFL_SUCCESS;
}

#ifdef COMPILE_IPV4_FRAG_FILTER_SUPPORT
void ioport::enqueue_packet(datapacket_t* pkt, unsigned int q_id){
	datapacketx86* pack = (datapacketx86*)pkt->platform_state;

	if(unlikely(pack->get_buffer_length() > mps)){
		unsigned int i, nof;
		datapacket_t* frags[IPV4_MAX_FRAG];

		//Check if is an IPv4 packet
		if(unlikely(get_ipv4_hdr(&pack->clas_state, 0) == NULL)){
			static unsigned int dropped = 0;

			if( unlikely((dropped++%LOG_SUPRESSION_ITER) == 0) ){
				ROFL_ERR(DRIVER_NAME"[ioport:%s] ERROR: %u packets were discarded due to excessive packet size. Interface MPS %u, size curr. pkt being discarded: %u\n", of_port_state->name, dropped, mps, pack->get_buffer_length());
			}

			//Packet is not an IPv4 packet => DROP
			bufferpool::release_buffer(pkt);
		}

		//Call the fragmentation library
		gnu_linux_frag_ipv4_pkt(&pkt, mps, &nof, frags);

		//Enqueue all pkts
		for(i=0; i<nof; ++i){
			enqueue_packet__(frags[i], q_id);
		}
	}else{
		enqueue_packet__(pkt, q_id);
	}
}
#endif
