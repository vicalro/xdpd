/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _IP_FAR_H_
#define _IP_FAR_H_

#include <rte_config.h>
#include <rte_ethdev.h>
#include <rofl.h>
#include <rofl/datapath/hal/cmm.h>

//Bug in DPDK
#ifdef __cplusplus
extern "C" { 
#endif

#include <rte_ip_frag.h>

#ifdef __cplusplus
} //extern "C"
#endif

#include "../io/dpdk_datapacket.h"
#include "../io/port_state.h"

extern struct rte_mempool *pool_direct;
extern struct rte_mempool *pool_indirect;

namespace xdpd{
namespace gnu_linux_dpdk{

hal_result_t gnu_linux_dpdk_enable_ip_frag_filter(const uint64_t dpid);
hal_result_t gnu_linux_dpdk_disable_ip_frag_filter(const uint64_t dpid);
bool gnu_linux_dpdk_ip_frag_filter_status(const uint64_t dpid);
hal_result_t gnu_linux_dpdk_enable_ip_reas_filter(const uint64_t dpid);
hal_result_t gnu_linux_dpdk_disable_ip_reas_filter(const uint64_t dpid);
bool gnu_linux_dpdk_ip_reas_filter_status(const uint64_t dpid);

static inline
int32_t gnu_linux_dpdk_frag_ip_packet(switch_port_t* port, datapacket_t* pkt, struct rte_mbuf **pkts_out){
	uint16_t mtu_size;
	unsigned int port_id = ((dpdk_port_state_t*)port->platform_port_state)->port_id;
	datapacket_dpdk_t * dpkt = (datapacket_dpdk_t*)pkt->platform_state;
	
	uint16_t nb_pkts_out = RTE_LIBRTE_IP_FRAG_MAX_FRAG;
	int32_t num_fragments=0;
	
	if( rte_eth_dev_get_mtu( port_id , &mtu_size) < 0 ){
		//invalid port -> ERROR
	}
	
	if(get_buffer_length_dpdk(dpkt) > mtu_size){
		void* ipv4 = get_ipv4_hdr( &(((datapacket_dpdk_t*)pkt->platform_state)->clas_state), 0);
		if (ipv4){
			num_fragments = rte_ipv4_fragment_packet( dpkt->mbuf, pkts_out, nb_pkts_out, mtu_size, pool_direct, pool_indirect);
		}
		// TODO handle errors!
		void* ipv6 = get_ipv6_hdr( &(((datapacket_dpdk_t*)pkt->platform_state)->clas_state), 0);
		if (ipv6){
			num_fragments = rte_ipv6_fragment_packet( dpkt->mbuf, pkts_out, nb_pkts_out, mtu_size, pool_direct, pool_indirect);
		}
		
	} else {
		pkts_out[0] = dpkt->mbuf;
		num_fragments = 1;
	}
	
	return num_fragments;
}

/*
static inline
int32_t gnu_linux_dpdk_frag_ip_packet(unsigned int port_id, struct rte_mbuf* mbuf_in, struct rte_mbuf **pkts_out){
	uint16_t mtu_size;
	//pointers to mbuf should be allocated. Mbufs are allocated in the function.
	
	uint16_t nb_pkts_out = RTE_LIBRTE_IP_FRAG_MAX_FRAG;
	int32_t num_fragments=0;
	
	if( rte_eth_dev_get_mtu( port_id , &mtu_size) < 0 ){
		//invalid port -> ERROR
	}
	if(rte_pktmbuf_pkt_len(mbuf_in) > mtu_size){
		if (mbuf_in->ol_flags & PKT_RX_IPV4_HDR){
			num_fragments = rte_ipv4_fragment_packet( mbuf_in, pkts_out, nb_pkts_out, mtu_size, pool_direct, pool_indirect);
		}
		// TODO handle errors!
		if (mbuf_in->ol_flags & PKT_RX_IPV6_HDR){
			num_fragments = rte_ipv6_fragment_packet( mbuf_in, pkts_out, nb_pkts_out, mtu_size, pool_direct, pool_indirect);
		}
		
	}
	return num_fragments;
}

static inline
int32_t gnu_linux_dpdk_frag_burst(unsigned int port_id, int32_t len_in, struct rte_mbuf **burst_in, struct rte_mbuf **burst_out){
	struct rte_mbuf *pkts_out[RTE_LIBRTE_IP_FRAG_MAX_FRAG];
	int i,j, len_out=0, num_fragments;
	
	for(i=0; i<len_in; i++){
		num_fragments = gnu_linux_dpdk_frag_ip_packet(port_id, burst_in[i], pkts_out);
		
		if(num_fragments<0){
			//error
			return num_fragments;
		}
		
		if(num_fragments>0){
			for(j=0; j<num_fragments; j++){
				burst_out[len_out] = pkts_out[j];
				len_out++;
			}
		}else{
			burst_out[len_out] = burst_in[i];
			len_out++;
		}
	}
	return len_out;
}
*/
static inline
void reassembly_ip_packet(){
	
}

}// namespace xdpd{
}// namespace gnu_linux_dpdk{

#endif //_IP_FAR_H_
