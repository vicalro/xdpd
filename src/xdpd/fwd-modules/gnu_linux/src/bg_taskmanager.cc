#include "bg_taskmanager.h"

#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
// Maybe needs to be in hal.h
#include <rofl/common/utils/c_logger.h>
#include <rofl/datapath/pipeline/physical_switch.h>
#include <rofl/datapath/afa/fwd_module.h>
#include <rofl/datapath/afa/cmm.h>
#include "io/datapacket_storage_c_wrapper.h"
#include "io/bufferpool_c_wrapper.h"
#include "processing/ls_internal_state.h"
#include "io/pktin_dispatcher.h"
#include "io/iomanager.h"
#include "util/time_utils.h"

//Local static variable for background manager thread
static pthread_t bg_thread;
static bool bg_continue_execution = true;

/**
 * This piece of code is meant to manage a thread that does:
 * 
 * - the expiration of the flow entries.
 * - the update the status of the ports
 * - purge old buffers in the buffer storage of a logical switch(pkt-in) 
 * - more?
 */

/**
 * @name prepare_event_socket
 * @brief creates socket and binds it to the NETLINK events
 */
int prepare_event_socket()
{
	int sock;
	struct sockaddr_nl addr;

	if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1){
		ROFL_ERR("Couldn't open NETLINK_ROUTE socket, errno(%d): %s\n", errno, strerror(errno));
		return -1;
	}

	if(fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0){
		// handle error
		ROFL_ERR("Error fcntl, errno(%d): %s\n", errno, strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTNLGRP_LINK;//RTMGRP_IPV4_IFADDR;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1){
		ROFL_ERR("couldn't bind, errno(%d): %s\n", errno, strerror(errno));
		return -1;
	}

	return sock;
}

/**
 * @name update_port_status
 */
rofl_result_t update_port_status(char * name){
	int skfd;
	struct ethtool_value edata;
	struct ifreq ifr;
	switch_port_t *port;
	bool last_link_status;
	port = fwd_module_get_port_by_name(name);
	
	memset(&edata,0,sizeof(edata));//Make valgrind happy
	
	if(port == NULL){
		ROFL_ERR("Error port with name %s not found\n",name);
		return ROFL_FAILURE;
	}
	
	if (( skfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ){
		ROFL_ERR("Socket call error, errno(%d): %s\n", errno, strerror(errno));
		return ROFL_FAILURE;
	}
	
	edata.cmd = ETHTOOL_GLINK;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name)-1);
	
	ifr.ifr_data = (char *) &edata;
	if (ioctl(skfd, SIOCETHTOOL, &ifr) == -1){
		ROFL_ERR("ETHTOOL_GLINK failed, errno(%d): %s\n", errno, strerror(errno));
		return ROFL_FAILURE;
	}

	ROFL_DEBUG("[bg] Interface %s link is %s\n", name,(edata.data ? "up" : "down"));
	
	last_link_status = port->up;

	if (edata.data){
		port->up = true;
		//TODO call iomanager to reset mmap
		//if(iomanager::bring_port_up((ioport*)port->platform_port_state)!=ROFL_SUCCESS)
			//return ROFL_FAILURE;
	}else{
		port->up = false;
		//TODO call iomanager to reset mmap
		//if(iomanager::bring_port_down((ioport*)port->platform_port_state)!=ROFL_SUCCESS)
			//return ROFL_FAILURE;
	}
	
	//port_status message needs to be created if the port id attached to switch
	if(port->attached_sw != NULL && last_link_status != port->up){
		//TODO check for both status changed: LINK and ADMIN STATE, and only send 1 message
		cmm_notify_port_status_changed(port);
	}
	
	return ROFL_SUCCESS;
}

/*
*  TODO: probably this belongs to iface_utils
*/

static int read_netlink_socket(int fd, char *buf, int seq_num, int pid){

	struct nlmsghdr *nlh;
	int read_len = 0, msg_len = 0;

	do {
		/* Recieve response from the kernel */
		if ((read_len = recv(fd, buf, MAX_NL_MESSAGE_HEADER - msg_len, 0)) < 0) {
		    return -1;
		}

		nlh = (struct nlmsghdr *) buf;

		/* Check if the header is valid */
		if ((NLMSG_OK(nlh, (unsigned int)read_len) == 0)
		    || (nlh->nlmsg_type == NLMSG_ERROR)) {
		    return -1;
		}

		/* Check if the its the last message */
		if (nlh->nlmsg_type == NLMSG_DONE) {
		    break;
		} else {
		/* Else move the pointer to buffer appropriately */
		    buf += read_len;
		    msg_len += read_len;
		}

		/* Check if its a multi part message */
		if ((nlh->nlmsg_flags & NLM_F_MULTI) == 0) {
		   /* return if its not */
		    break;
		}
	} while ((nlh->nlmsg_seq != (unsigned int)seq_num) || (nlh->nlmsg_pid != (unsigned int)pid));
	
	return msg_len;
}

/**
 * @name read_netlink_message
 * @brief once we have received a NL message, this function reads the content and 
 * calls a function to update the port structure
 * @param fd file descriptor where the message has been received
 */
static rofl_result_t read_netlink_message(int fd){
	
	int len;
	struct nlmsghdr *nlh;
	char recv_buffer[MAX_NL_MESSAGE_HEADER];
	nlh = (struct nlmsghdr *)recv_buffer;
	
	struct ifinfomsg *ifi;
	char name[IFNAMSIZ];
	
	len = read_netlink_socket(fd,(char*)nlh, 0, getpid());
	if(len == -1)
		return ROFL_FAILURE; 

	for (; NLMSG_OK(nlh, (unsigned int)len); nlh = NLMSG_NEXT(nlh, len)) {

	    if (nlh->nlmsg_type == RTM_NEWLINK){
				ifi = (struct ifinfomsg *) NLMSG_DATA(nlh);
				
				if_indextoname(ifi->ifi_index, name);
				
				ROFL_DEBUG_VERBOSE("interface changed status\n",name);
				
				// HERE change the status to the port structure
				if(update_port_status(name)!=ROFL_SUCCESS)
					return ROFL_FAILURE;
	    }else{
				ROFL_ERR("Received a different RTM message type\n");
				return ROFL_FAILURE;
	    }
	}
	
	return ROFL_SUCCESS;
}

/**
 * @name process_timeouts
 * @brief checks if its time to process timeouts (flow entries and pool of buffers)
 * @param psw physical switch (where all the logical switches are)
 */
int process_timeouts()
{
	datapacket_t* pkt;
	unsigned int i, max_switches;
	struct timeval now;
	of_switch_t** logical_switches;
	static struct timeval last_time_entries_checked={0,0}, last_time_pool_checked={0,0};
	gettimeofday(&now,NULL);

	//Retrieve the logical switches list
	logical_switches = physical_switch_get_logical_switches(&max_switches);
	
	if(get_time_difference_ms(&now, &last_time_entries_checked)>=LSW_TIMER_SLOT_MS)
	{
#ifdef DEBUG
		static int dummy = 0;
#endif

		//TIMERS FLOW ENTRIES
		for(i=0; i<max_switches; i++)
		{

			if(logical_switches[i] != NULL){
				of_process_pipeline_tables_timeout_expirations(logical_switches[i]);
				
#ifdef DEBUG
				if(dummy%20 == 0)
					of1x_full_dump_switch((of1x_switch_t*)logical_switches[i]);
#endif
			}
		}
			
#ifdef DEBUG
		dummy++;
		//ROFL_DEBUG_VERBOSE("Checking flow entries expirations %lu:%lu\n",now.tv_sec,now.tv_usec);
#endif
		last_time_entries_checked = now;
	}
	
	if(get_time_difference_ms(&now, &last_time_pool_checked)>=LSW_TIMER_BUFFER_POOL_MS){
		uint32_t buffer_id;
		datapacket_store_handle dps=NULL;
		
		for(i=0; i<max_switches; i++){

			if(logical_switches[i] != NULL){

				//Recover storage pointer
				dps =(datapacket_store_handle) ( (struct logical_switch_internals*) logical_switches[i]->platform_state)->storage;
				//Loop until the oldest expired packet is taken out
				while(datapacket_storage_oldest_packet_needs_expiration_wrapper(dps,&buffer_id)){

					ROFL_DEBUG_VERBOSE("Trying to erase a datapacket from storage: %u\n", buffer_id);

					if( (pkt = datapacket_storage_get_packet_wrapper(dps,buffer_id) ) == NULL ){
						ROFL_DEBUG_VERBOSE("Error in get_packet_wrapper %u\n", buffer_id);
					}else{
						ROFL_DEBUG_VERBOSE("Datapacket expired correctly %u\n", buffer_id);
						//Return buffer to bufferpool
						bufferpool_release_buffer_wrapper(pkt);
					}
				}
			}
		}
		
#ifdef DEBUG
		//ROFL_ERR("Checking pool buffers expirations %lu:%lu\n",now.tv_sec,now.tv_usec);
#endif
		last_time_pool_checked = now;
	}
	
	return ROFL_SUCCESS;
}

/**
 * @name x86_background_tasks_thread
 * @brief contents the infinite loop checking for ports and timeouts
 */
void* x86_background_tasks_routine(void* param)
{
	int i, efd, events_socket,nfds;
	struct epoll_event event_list[MAX_EPOLL_EVENTS], epe_port;

	// program an epoll that listents to the file descriptors of the ports with a
	// timeout that makes us check 
	
	memset(event_list,0,sizeof(event_list));
	memset(&epe_port,0,sizeof(epe_port));
	
	if((events_socket=prepare_event_socket())<0){
		exit(ROFL_FAILURE);
	}
	
	efd = epoll_create1(0);

	if(efd == -1){
		ROFL_ERR("Error in epoll_create1, errno(%d): %s\n", errno, strerror(errno) );
		return NULL;
	}

	//Add netlink	
	epe_port.data.fd = events_socket;
	epe_port.events = EPOLLIN | EPOLLET;
	
	if(epoll_ctl(efd,EPOLL_CTL_ADD,events_socket,&epe_port)==-1){
		ROFL_ERR("Error in epoll_ctl, errno(%d): %s\n", errno, strerror(errno));
		return NULL;
	}

	//Add PKT_IN
	init_packetin_pipe();

	epe_port.data.fd = get_packet_in_read_fd();
	epe_port.events = EPOLLIN | EPOLLET;
	
	if(epoll_ctl(efd,EPOLL_CTL_ADD, epe_port.data.fd, &epe_port)==-1){
		ROFL_ERR("Error in epoll_ctl, errno(%d): %s\n", errno, strerror(errno));
		return NULL;
	}
	
	while(bg_continue_execution){
		
		//Throttle
		nfds = epoll_wait(efd, event_list, MAX_EPOLL_EVENTS, LSW_TIMER_SLOT_MS/*timeout needs TBD somewhere else*/);


		if(nfds==-1){
			//ROFL_DEBUG("Epoll Failed\n");
			continue;
		}

		//Check for events
		for(i=0;i<nfds;i++){

			if( (event_list[i].events & EPOLLERR) || (event_list[i].events & EPOLLHUP)/*||(event_list[i].events & EPOLLIN)*/){
				//error on this fd
				//ROFL_ERR("Error in file descriptor\n");
				close(event_list[i].data.fd); //fd gets removed automatically from efd's
				continue;
			}else{
				//Is netlink or packet-in subsystem
				if(get_packet_in_read_fd() == event_list[i].data.fd){
					//PKT_IN
					process_packet_ins();	
				}else{
					//Netlink message
					read_netlink_message(event_list[i].data.fd);
				}
			}
		}
		
		//check timers expiration 
		process_timeouts();
	}

	//Cleanup packet-in
	destroy_packetin_pipe();
	
	//Cleanup epoll fd
	close(efd);
	
	//Printing some information
	ROFL_DEBUG("[bg] Finishing thread execution\n"); 

	//Exit
	pthread_exit(NULL);	
}

/**
 * launches the main thread
 */
rofl_result_t launch_background_tasks_manager()
{
	//Set flag
	bg_continue_execution = true;

	if(pthread_create(&bg_thread, NULL, x86_background_tasks_routine,NULL)<0){
		ROFL_ERR("pthread_create failed, errno(%d): %s\n", errno, strerror(errno));
		return ROFL_FAILURE;
	}
	return ROFL_SUCCESS;
}

rofl_result_t stop_background_tasks_manager()
{
	bg_continue_execution = false;
	pthread_join(bg_thread,NULL);
	return ROFL_SUCCESS;
}
