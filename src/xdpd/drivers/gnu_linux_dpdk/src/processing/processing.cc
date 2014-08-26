#include "processing.h"
#include <rofl/common/utils/c_logger.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>

#include "assert.h"
#include "../util/compiler_assert.h"
#include "../io/rx.h"
#include "../io/tx.h"

#include "../io/port_state.h"
#include "../io/iface_manager.h"
#include <rofl/datapath/pipeline/openflow/of_switch.h>


using namespace xdpd::gnu_linux_dpdk;

//
// Processing state 
//
static unsigned int current_core_index;
static unsigned int max_cores;
static rte_spinlock_t mutex;
core_tasks_t processing_core_tasks[RTE_MAX_LCORE];
unsigned int total_num_of_phy_ports = 0;
unsigned int total_num_of_pex_ports = 0;
unsigned int running_hash = 0;

static void processing_dump_cores_state(void){

#ifdef DEBUG
	unsigned int i;
	enum rte_lcore_role_t role;
	enum rte_lcore_state_t state;

	return;

	for(i=0; i < RTE_MAX_LCORE; ++i){
		role = rte_eal_lcore_role(i);
		state = rte_eal_get_lcore_state(i);
		
		ROFL_DEBUG(DRIVER_NAME"[processing] Core %u ROLE:", i);
		switch(role){
			case ROLE_RTE:
				ROFL_DEBUG(" RTE");
				break;
			case ROLE_OFF:
				ROFL_DEBUG(" OFF");
				break;
			default:
				assert(0);
				ROFL_DEBUG(" Unknown");
				break;
		}
		
		ROFL_DEBUG(" state:");
		switch(state){
			case WAIT:
				ROFL_DEBUG(" WAIT");
				break;
			case RUNNING:
				ROFL_DEBUG(" RUNNING");
				break;
			case FINISHED:
				ROFL_DEBUG(" FINISHED");
				break;
			default:
				assert(0);
				ROFL_DEBUG(" UNKNOWN");
				break;
		}
		ROFL_DEBUG("\n");
	}
#endif	
}

/*
* Initialize data structures for processing to work 
*/
rofl_result_t processing_init(void){

	unsigned int i;
	struct rte_config* config;
	enum rte_lcore_role_t role;

	//Cleanup
	memset(processing_core_tasks,0,sizeof(core_tasks_t)*RTE_MAX_LCORE);	

	//Init 
	current_core_index = 0;
	config = rte_eal_get_configuration();
	max_cores = config->lcore_count;
	rte_spinlock_init(&mutex);
		
	ROFL_DEBUG(DRIVER_NAME"[processing] Processing init: %u logical cores guessed from rte_eal_get_configuration(). Master is: %u\n", config->lcore_count, config->master_lcore);

	//Define available cores 
	for(i=0; i < RTE_MAX_LCORE; ++i){
		role = rte_eal_lcore_role(i);
		if(role == ROLE_RTE && i != config->master_lcore){
			processing_core_tasks[i].available = true;
			ROFL_DEBUG(DRIVER_NAME"[processing] Marking core %u as available\n",i);
		}
	}

	processing_dump_cores_state();	

	return ROFL_SUCCESS;
}


/*
* Destroy data structures for processing to work 
*/
rofl_result_t processing_destroy(void){

	unsigned int i;

	ROFL_DEBUG(DRIVER_NAME"[processing] Shutting down all active cores\n");
	
	//Stop all cores and wait for them to complete execution tasks
	for(i=0;i<RTE_MAX_LCORE;++i){
		if(processing_core_tasks[i].available && processing_core_tasks[i].active){
			ROFL_DEBUG(DRIVER_NAME"[processing] Shutting down active core %u\n",i);
			processing_core_tasks[i].active = false;
			//Join core
			rte_eal_wait_lcore(i);
		}
	}
	return ROFL_SUCCESS;
}

//Synchronization code
static void processing_wait_for_cores_to_sync(){

	unsigned int i;

	for(i=0;i<RTE_MAX_LCORE;++i){
		if(processing_core_tasks[i].active){
			while(processing_core_tasks[i].running_hash != running_hash);
		}	
	}
}

int processing_core_process_packets(void* not_used){

	unsigned int i, l, core_id;
	int j;
	bool own_port;
	switch_port_t* port;
	port_queues_t* port_queues;	
        uint64_t diff_tsc, prev_tsc;
	struct rte_mbuf* pkt_burst[IO_IFACE_MAX_PKT_BURST]={0};
	core_tasks_t* tasks = &processing_core_tasks[rte_lcore_id()];

	//Time to drain in tics	
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * IO_BURST_TX_DRAIN_US;
	
	//Own core
	core_id = rte_lcore_id();  

	//Parsing and pipeline extra state
	datapacket_t pkt;
	datapacket_dpdk_t* pkt_state = create_datapacket_dpdk(&pkt);

	//Init values and assign
	pkt.platform_state = (platform_datapacket_state_t*)pkt_state;
	pkt_state->mbuf = NULL;

	//Set flag to active
	tasks->active = true;
	
	//Last drain tsc
	prev_tsc = 0;

	while(likely(tasks->active)){

		//Update running_hash
		tasks->running_hash = running_hash;

		//Calc diff
		diff_tsc = prev_tsc - rte_rdtsc();  

		//Drain TX if necessary	
		if(unlikely(diff_tsc > drain_tsc)){
		
			//Handle physical ports
			for(i=0, l=0; l<total_num_of_phy_ports && likely(i<PROCESSING_MAX_PORTS) ; ++i){
				
				if(!tasks->phy_ports[i].present)
					continue;
					
				l++;
	
				//make code readable
				port_queues = &tasks->phy_ports[i];
				
				//Check whether is our port (we have to also transmit TX queues)				
				own_port = (port_queues->core_id == core_id);
						
				//Flush (enqueue them in the RX/TX port lcore)
				for( j=(IO_IFACE_NUM_QUEUES-1); j >=0 ; j-- ){
					flush_port_queue_tx_burst(phy_port_mapping[i], i, &port_queues->tx_queues_burst[j], j);
					
					if(own_port)	
						transmit_port_queue_tx_burst(i, j, pkt_burst);
				}
			}

#ifdef GNU_LINUX_DPDK_ENABLE_PEX			
			//handle PEX ports
			for(i=0, l=0; l<total_num_of_pex_ports && likely(i<PROCESSING_MAX_PORTS) ; ++i)
			{	
				if(!tasks->pex_ports[i].present)
					continue;
					
				l++;
				
				if(pex_port_mapping[i]->type == PORT_TYPE_PEX_DPDK_KNI)
				{
					//make code readable
					port_queues = &tasks->pex_ports[i];
				
					//Check whether is our port (we have to also transmit TX queues)				
					own_port = (port_queues->core_id == core_id);
						
					flush_kni_pex_port_burst(pex_port_mapping[i], i, &port_queues->tx_queues_burst[0]);
				
					if(own_port)		
						transmit_kni_pex_port_burst(pex_port_mapping[i],i, pkt_burst);
				}
#ifdef ENABLE_DPDK_SECONDARY_SEMAPHORE
				else
				{
					assert(pex_port_mapping[i]->type == PORT_TYPE_PEX_DPDK_SECONDARY);
					flush_dpdk_pex_port(pex_port_mapping[i]);
				}				
#endif
			}
#endif
		}
		
		//Process RX
		for(i=0;i<tasks->num_of_rx_ports;++i)
		{
			port = tasks->port_list[i];
			if(likely(port != NULL) && likely(port->up)){ //This CAN happen while deschedulings
				//Process RX&pipeline 
				process_port_rx(core_id, port, pkt_burst, &pkt, pkt_state);
			}
		}
	}
	
	tasks->active = false;
	destroy_datapacket_dpdk(pkt_state);

	return (int)ROFL_SUCCESS; 
}


//
//Port scheduling
//

/*
* Schedule port. Shedule port to an available core (RR)
*/
rofl_result_t processing_schedule_port(switch_port_t* port){

	unsigned int i, index, *num_of_ports;
	unsigned int port_id;

	
	rte_spinlock_lock(&mutex);

	switch(port->type){
		case PORT_TYPE_PHYSICAL: 
			if(total_num_of_phy_ports == PROCESSING_MAX_PORTS){
				ROFL_ERR(DRIVER_NAME"[processing] Reached already PROCESSING_MAX_PORTS(%u). All cores are full. No available port slots\n", PROCESSING_MAX_PORTS);
				rte_spinlock_unlock(&mutex);
				return ROFL_FAILURE;
			}
			break;
		case PORT_TYPE_PEX_DPDK_SECONDARY:	
		case PORT_TYPE_PEX_DPDK_KNI:
			if(total_num_of_pex_ports == PROCESSING_MAX_PORTS){
					ROFL_ERR(DRIVER_NAME"[processing] Reached already PROCESSING_MAX_PORTS(%u). All cores are full. No available port slots\n", PROCESSING_MAX_PORTS);
					rte_spinlock_unlock(&mutex);
					return ROFL_FAILURE;
			}
			break;
	
		default: assert(0);
			return ROFL_FAILURE;
	}
	//Select core
	for(current_core_index++, index=current_core_index;;){
		if( processing_core_tasks[current_core_index].available == true && processing_core_tasks[current_core_index].num_of_rx_ports != PROCESSING_MAX_PORTS_PER_CORE )
			break;

		//Circular increment
		if(current_core_index+1 == RTE_MAX_LCORE)
			current_core_index=0; 
		else
			current_core_index++;
	
		//We've already checked all positions. No core free. Return
		if(current_core_index == index){
			//All full 
			ROFL_ERR(DRIVER_NAME"[processing] All cores are full. No available port slots\n");
			assert(0);		
			rte_spinlock_unlock(&mutex);
			return ROFL_FAILURE;
		}
	}

	ROFL_DEBUG(DRIVER_NAME"[processing] Selected core %u for scheduling port %s(%p)\n", current_core_index, port->name, port); 

	num_of_ports = &processing_core_tasks[current_core_index].num_of_rx_ports;

	//Assign port and exit
	if(processing_core_tasks[current_core_index].port_list[*num_of_ports] != NULL){
		ROFL_ERR(DRIVER_NAME"[processing] Corrupted state on the core task list\n");
		assert(0);
		rte_spinlock_unlock(&mutex);
		return ROFL_FAILURE;
	}

	switch(port->type){
		case PORT_TYPE_PHYSICAL: 
		{
			dpdk_port_state_t* port_state = (dpdk_port_state_t*)port->platform_port_state;	
			//FIXME: check if already scheduled
			if( iface_manager_set_queues(port, current_core_index, port_state->port_id) != ROFL_SUCCESS){
				assert(0);
				return ROFL_FAILURE;
			}

			//Store attachment info (back reference)
			port_state->core_id = current_core_index; 
			port_state->core_port_slot = *num_of_ports;
		
			
			port_id = port_state->port_id;

			//Increment total counter
			total_num_of_phy_ports++;
			
			port_state->scheduled = true;
		}
			break;
		case PORT_TYPE_PEX_DPDK_SECONDARY:	
		{
			pex_port_state_dpdk_t* port_state = (pex_port_state_dpdk_t*)port->platform_port_state;

			//Store attachment info (back reference)
			port_state->core_id = current_core_index; 
			port_state->core_port_slot = *num_of_ports;

			port_id = port_state->pex_id;

			//Increment total counter
			total_num_of_pex_ports++;
				
			port_state->scheduled = true;
		}

			break;
		case PORT_TYPE_PEX_DPDK_KNI:
		{
			pex_port_state_kni_t* port_state = (pex_port_state_kni_t*)port->platform_port_state;

			//Store attachment info (back reference)
			port_state->core_id = current_core_index; 
			port_state->core_port_slot = *num_of_ports;

			port_id = port_state->pex_id;
		
			//Increment total counter
			total_num_of_pex_ports++;
			
			port_state->scheduled = true;
		}

			break;
	
		default: assert(0);
			return ROFL_FAILURE;
	}

	processing_core_tasks[current_core_index].port_list[*num_of_ports] = port;
	(*num_of_ports)++;
	
	index = current_core_index;

	//Mark port as present (and scheduled) on all cores (TX)
	for(i=0;i<RTE_MAX_LCORE;++i){

		switch(port->type){
			case PORT_TYPE_PHYSICAL: 
				processing_core_tasks[i].phy_ports[port_id].present = true;
				processing_core_tasks[i].phy_ports[port_id].core_id = index;
				break;
				
			case PORT_TYPE_PEX_DPDK_SECONDARY:	
			case PORT_TYPE_PEX_DPDK_KNI:
				processing_core_tasks[i].pex_ports[port_id].present = true;
				processing_core_tasks[i].pex_ports[port_id].core_id = index;
				break;
		
			default: assert(0);
				return ROFL_FAILURE;
		}
	}


	//Increment the hash counter
	running_hash++;
	
	rte_spinlock_unlock(&mutex);

	if(!processing_core_tasks[index].active){
		if(rte_eal_get_lcore_state(index) != WAIT){
			assert(0);
			rte_panic("Core status corrupted!");
		}
		
		ROFL_DEBUG(DRIVER_NAME"[processing] Launching core %u due to scheduling action of port %p\n", index, port);

		//Launch
		ROFL_DEBUG_VERBOSE("Pre-launching core %u due to scheduling action of port %p\n", index, port);
		if( rte_eal_remote_launch(processing_core_process_packets, NULL, index) < 0)
			rte_panic("Unable to launch core %u! Status was NOT wait (race-condition?)", index);
		ROFL_DEBUG_VERBOSE("Post-launching core %u due to scheduling action of port %p\n", index, port);
	}
	
	return ROFL_SUCCESS;
}

#if 0
/*
* Schedule PEX port. Shedule PEX port to an available core (RR)
*/
rofl_result_t processing_schedule_pex_port(switch_port_t* port)
{
	unsigned int i, index, *num_of_ports;	

	if(port->type != PORT_TYPE_PEX_DPDK_SECONDARY && port->type != PORT_TYPE_PEX_DPDK_KNI)
	{
		assert(0);
		return ROFL_FAILURE;
	}

	rte_spinlock_lock(&mutex);

	if(total_num_of_pex_ports == PROCESSING_MAX_PORTS)
	{
		ROFL_ERR(DRIVER_NAME"[processing] Reached already PROCESSING_MAX_PORTS(%u). All cores are full. No available port slots\n", PROCESSING_MAX_PORTS);
		rte_spinlock_unlock(&mutex);
		return ROFL_FAILURE;
	}

	//Select core
	for(current_core_index++, index=current_core_index;;)
	{
		if( processing_core_tasks[current_core_index].available == true && processing_core_tasks[current_core_index].num_of_rx_ports != PROCESSING_MAX_PORTS_PER_CORE )
			break;

		//Circular increment
		if(current_core_index+1 == RTE_MAX_LCORE)
			current_core_index=0; 
		else
			current_core_index++;
	
		//We've already checked all positions. No core free. Return
		if(current_core_index == index)
		{
			//All full 
			ROFL_ERR(DRIVER_NAME"[processing] All cores are full. No available port slots\n");
			assert(0);		
			rte_spinlock_unlock(&mutex);
			return ROFL_FAILURE;
		}
	}

	ROFL_DEBUG(DRIVER_NAME"[processing] Selected core %u for scheduling port %s(%p)\n", current_core_index, port->name, port); 

	num_of_ports = &processing_core_tasks[current_core_index].num_of_rx_ports;

	//Assign port and exit
	if(processing_core_tasks[current_core_index].port_list[*num_of_ports] != NULL)
	{
		ROFL_ERR(DRIVER_NAME"[processing] Corrupted state on the core task list\n");
		assert(0);
		rte_spinlock_unlock(&mutex);
		return ROFL_FAILURE;
	}

	assert(port != NULL);
	
	processing_core_tasks[current_core_index].port_list[*num_of_ports] = port;
	(*num_of_ports)++;
	
	index = current_core_index;

	if(port->type == PORT_TYPE_PEX_DPDK_SECONDARY)
	{
		pex_port_state_dpdk_t* port_state = (pex_port_state_dpdk_t*)port->platform_port_state;

		//Store attachment info (back reference)
		port_state->core_id = current_core_index; 
		port_state->core_port_slot = *num_of_ports;

		//Mark port as present (and scheduled) on all cores (TX)
		for(i=0;i<RTE_MAX_LCORE;++i)
		{
			processing_core_tasks[i].pex_ports[port_state->pex_id].present = true;
			processing_core_tasks[i].pex_ports[port_state->pex_id].core_id = index;
		}
	}
	else
	{
		pex_port_state_kni_t* port_state = (pex_port_state_kni_t*)port->platform_port_state;

		//Store attachment info (back reference)
		port_state->core_id = current_core_index; 
		port_state->core_port_slot = *num_of_ports;

		//Mark port as present (and scheduled) on all cores (TX)
		for(i=0;i<RTE_MAX_LCORE;++i)
		{
			processing_core_tasks[i].pex_ports[port_state->pex_id].present = true;
			processing_core_tasks[i].pex_ports[port_state->pex_id].core_id = index;
		}
	}

	//Increment total counter
	total_num_of_pex_ports++;
	
	//Increment the hash counter
	running_hash++;
	
	rte_spinlock_unlock(&mutex);

	if(!processing_core_tasks[index].active)
	{
		if(rte_eal_get_lcore_state(index) != WAIT)
		{
			assert(0);
			rte_panic("Core status corrupted!");
		}
		
		ROFL_DEBUG(DRIVER_NAME"[processing] Launching core %u due to scheduling action of port %p\n", index, port);

		//Launch
		ROFL_DEBUG_VERBOSE("Pre-launching core %u due to scheduling action of port %p\n", index, port);
		if( rte_eal_remote_launch(processing_core_process_packets, NULL, index) < 0)
			rte_panic("Unable to launch core %u! Status was NOT wait (race-condition?)", index);
		ROFL_DEBUG_VERBOSE("Post-launching core %u due to scheduling action of port %p\n", index, port);
	}
	
	if(port->type == PORT_TYPE_PEX_DPDK_SECONDARY)
	{
		pex_port_state_dpdk_t* port_state = (pex_port_state_dpdk_t*)port->platform_port_state;
		port_state->scheduled = true;
	}
	else
	{
		pex_port_state_kni_t* port_state = (pex_port_state_kni_t*)port->platform_port_state;
		port_state->scheduled = true;
	}
	
	return ROFL_SUCCESS;
}

#endif

/*
* Deschedule port to a core 
*/
rofl_result_t processing_deschedule_port(switch_port_t* port){

	unsigned int i, j, k, len;
	bool* scheduled;
	unsigned int* core_id, *port_id, *core_port_slot;

	switch(port->type){
		case PORT_TYPE_PHYSICAL:
		{ 
			dpdk_port_state_t* port_state = (dpdk_port_state_t*)port->platform_port_state;	
			scheduled = &port_state->scheduled;
			core_id = &port_state->core_id;	
			core_port_slot = &port_state->core_port_slot;	
			port_id = &port_state->port_id;	
		}	
			break;
		case PORT_TYPE_PEX_DPDK_SECONDARY:
		{
			pex_port_state_dpdk_t* port_state = (pex_port_state_dpdk_t*)port->platform_port_state;	
			scheduled = &port_state->scheduled;	
			core_id = &port_state->core_id;
			core_port_slot = &port_state->core_port_slot;	
			port_id = &port_state->pex_id;	

		}
			break;	
		case PORT_TYPE_PEX_DPDK_KNI:
		{
			pex_port_state_kni_t* port_state = (pex_port_state_kni_t*)port->platform_port_state;	
		
			scheduled = &port_state->scheduled;	
			core_id = &port_state->core_id;	
			core_port_slot = &port_state->core_port_slot;	
			port_id = &port_state->pex_id;	

		}
			
			break;
	
		default: assert(0);
			return ROFL_FAILURE;
	}
	
	if(*scheduled == false){
		ROFL_ERR(DRIVER_NAME"[processing] Tyring to descheduled an unscheduled port\n");
		assert(0);
		return ROFL_FAILURE;
	}
	
	core_tasks_t* core_task = &processing_core_tasks[*core_id];

	rte_spinlock_lock(&mutex);

	//This loop copies from descheduled port, all the rest of the ports
	//one up, so that list of ports is contiguous (0...N-1)
	for(i=*core_port_slot; i<core_task->num_of_rx_ports; i++){
		core_task->port_list[i] = core_task->port_list[i+1];
		if(core_task->port_list[i]){
			switch(core_task->port_list[i]->type){
				case PORT_TYPE_PHYSICAL: 
					((dpdk_port_state_t*)core_task->port_list[i]->platform_port_state)->core_port_slot = i;
					break;
				case PORT_TYPE_PEX_DPDK_SECONDARY:	
					((pex_port_state_dpdk_t*)core_task->port_list[i]->platform_port_state)->core_port_slot = i;
					break;
				case PORT_TYPE_PEX_DPDK_KNI:
					((pex_port_state_kni_t*)core_task->port_list[i]->platform_port_state)->core_port_slot = i;
					break;
				default: assert(0); //Can never happen
					return ROFL_FAILURE;
			}
		}
	}
	
	//Decrement counter
	core_task->num_of_rx_ports--;

	//There are no more ports, so simply stop core
	if(core_task->num_of_rx_ports == 0){
		if(rte_eal_get_lcore_state(*core_id) != RUNNING){
			ROFL_ERR(DRIVER_NAME"[processing] Corrupted state; port was marked as active, but EAL informs it was not running..\n");
			assert(0);
			
		}
		
		ROFL_DEBUG(DRIVER_NAME"[processing] Shutting down core %u, since port list is empty\n",i);
		
		core_task->active = false;
		
		//Wait for core to stop
		rte_eal_wait_lcore(*core_id);
	}

	switch(port->type){
		case PORT_TYPE_PHYSICAL: 
			//Decrement total counter
			total_num_of_phy_ports--;
			break;
		case PORT_TYPE_PEX_DPDK_SECONDARY:	
		case PORT_TYPE_PEX_DPDK_KNI:
			//Decrement total counter
			total_num_of_pex_ports--;
			break;
		
		default: assert(0); //Can never happen
			return ROFL_FAILURE;
	}
	
	
	//Mark port as NOT present anymore (descheduled) on all cores (TX)
	for(i=0;i<RTE_MAX_LCORE;++i){

		switch(port->type){
			case PORT_TYPE_PHYSICAL: 
				processing_core_tasks[i].phy_ports[*port_id].present = false;
				processing_core_tasks[i].phy_ports[*port_id].core_id = 0xFFFFFFFF;
				break;
				
			case PORT_TYPE_PEX_DPDK_SECONDARY:
			case PORT_TYPE_PEX_DPDK_KNI:
				processing_core_tasks[i].pex_ports[*port_id].present = false;
				processing_core_tasks[i].pex_ports[*port_id].core_id = 0xFFFFFFFF;
				break;
		
			default: assert(0);
				return ROFL_FAILURE;
		}
	}

	//Increment the hash counter
	running_hash++;
	
	//Wait for all the active cores to sync
	processing_wait_for_cores_to_sync();

	//Now we are realising any pending tx mbuf	
	for(i=0;i<RTE_MAX_LCORE;++i){

		switch(port->type){
			case PORT_TYPE_PHYSICAL: 
				for(j=0;j<IO_IFACE_NUM_QUEUES;++j){
					struct mbuf_burst pkt_burst = processing_core_tasks[i].phy_ports[*port_id].tx_queues_burst[j];
					len = pkt_burst.len;
					for(k=0;k<len;k++){
						rte_pktmbuf_free(pkt_burst.burst[k]);
					}
				}
				break;
				
			case PORT_TYPE_PEX_DPDK_SECONDARY:
			case PORT_TYPE_PEX_DPDK_KNI:
				for(j=0;j<IO_IFACE_NUM_QUEUES;++j){
					struct mbuf_burst pkt_burst = processing_core_tasks[i].pex_ports[*port_id].tx_queues_burst[j];
					len = pkt_burst.len;
					for(k=0;k<len;k++){
						rte_pktmbuf_free(pkt_burst.burst[k]);
					}
				}
				break;
		
			default: assert(0);
				return ROFL_FAILURE;
		}
	}

	rte_spinlock_unlock(&mutex);	
	
	*scheduled = false;

	return ROFL_SUCCESS;
}
#if 0
/*
* Deschedule PEX port to a core 
*/
rofl_result_t processing_deschedule_pex_port(switch_port_t* port)
{
	unsigned int i;
	
	assert(port->type == PORT_TYPE_PEX_DPDK_SECONDARY || port->type == PORT_TYPE_PEX_DPDK_KNI);
	
	if(port->type == PORT_TYPE_PEX_DPDK_SECONDARY)
	{	
		pex_port_state_dpdk_t* port_state = (pex_port_state_dpdk_t*)port->platform_port_state;	
		
		core_tasks_t* core_task = &processing_core_tasks[port_state->core_id];

		if(port_state->scheduled == false){
			ROFL_ERR(DRIVER_NAME"[processing] Tyring to descheduled an unscheduled PEX port\n");
			assert(0);
			return ROFL_FAILURE;
		}

		rte_spinlock_lock(&mutex);

		//This loop copies from descheduled port, all the rest of the ports
		//one up, so that list of ports is contiguous (0...N-1)

		assert(core_task->num_of_rx_ports != 0);

		for(i=(core_task->num_of_rx_ports-1); i > port_state->core_port_slot; i--)
			core_task->port_list[i-1] = core_task->port_list[i];	
	
		//Cleanup the last position
		core_task->num_of_rx_ports--;
		core_task->port_list[core_task->num_of_rx_ports] = NULL;

		//There are no more ports, so simply stop core
		if(core_task->num_of_rx_ports == 0){
			if(rte_eal_get_lcore_state(port_state->core_id) != RUNNING){
				ROFL_ERR(DRIVER_NAME"[processing] Corrupted state; port was marked as active, but EAL informs it was not running..\n");
				assert(0);
			
			}
		
			ROFL_DEBUG(DRIVER_NAME"[processing] Shutting down core %u, since port list is empty\n",i);
		
			core_task->active = false;
		
			//Wait for core to stop
			rte_eal_wait_lcore(port_state->core_id);
		}
	
		//Decrement total counter
		total_num_of_pex_ports--;
	
		//Mark PEX port as NOT present anymore (descheduled) on all cores (TX)
		for(i=0;i<RTE_MAX_LCORE;++i){
			processing_core_tasks[i].pex_ports[port_state->pex_id].present = false;
			processing_core_tasks[i].pex_ports[port_state->pex_id].core_id = 0xFFFFFFFF;
		}

		//Increment the hash counter
		running_hash++;
		
		//Wait for all the active cores to sync
		processing_wait_for_cores_to_sync();

		rte_spinlock_unlock(&mutex);	
	
		port_state->scheduled = false;
	}
	else	//if(port->type == PORT_TYPE_PEX_KNI)
	{
	
		pex_port_state_kni_t* port_state = (pex_port_state_kni_t*)port->platform_port_state;	

		core_tasks_t* core_task = &processing_core_tasks[port_state->core_id];

		if(port_state->scheduled == false){
			ROFL_ERR(DRIVER_NAME"[processing] Tyring to descheduled an unscheduled PEX port\n");
			assert(0);
			return ROFL_FAILURE;
		}

		rte_spinlock_lock(&mutex);

		//This loop copies from descheduled port, all the rest of the ports
		//one up, so that list of ports is contiguous (0...N-1)

		assert(core_task->num_of_rx_ports != 0);

		for(i=(core_task->num_of_rx_ports-1); i > port_state->core_port_slot; i--)
			core_task->port_list[i-1] = core_task->port_list[i];	
	
		//Cleanup the last position
		core_task->num_of_rx_ports--;
		core_task->port_list[core_task->num_of_rx_ports] = NULL;

		//There are no more ports, so simply stop core
		if(core_task->num_of_rx_ports == 0){
			if(rte_eal_get_lcore_state(port_state->core_id) != RUNNING){
				ROFL_ERR(DRIVER_NAME"[processing] Corrupted state; port was marked as active, but EAL informs it was not running..\n");
				assert(0);
			
			}
		
			ROFL_DEBUG(DRIVER_NAME"[processing] Shutting down core %u, since port list is empty\n",i);
		
			core_task->active = false;
		
			//Wait for core to stop
			rte_eal_wait_lcore(port_state->core_id);
		}
	
		//Decrement total counter
		total_num_of_pex_ports--;
	
		//Mark PEX port as NOT present anymore (descheduled) on all cores (TX)
		for(i=0;i<RTE_MAX_LCORE;++i){
			processing_core_tasks[i].pex_ports[port_state->pex_id].present = false;
			processing_core_tasks[i].pex_ports[port_state->pex_id].core_id = 0xFFFFFFFF;
		}

		//Increment the hash counter
		running_hash++;
		
		//Wait for all the active cores to sync
		processing_wait_for_cores_to_sync();

		rte_spinlock_unlock(&mutex);	
	
		port_state->scheduled = false;
	}

	return ROFL_SUCCESS;
}
#endif

/*
* Dump core state
*/
void processing_dump_core_state(void){

	unsigned int i,j;
	core_tasks_t* core_task;
	
	for(i=0;i<max_cores;++i){
		core_task = &processing_core_tasks[i];
		if(!core_task->available)
			continue;

		//Print basic info	
		ROFL_DEBUG(DRIVER_NAME"[processing] Core: %u ",i);
		
		if(!core_task->active)
			ROFL_DEBUG("IN");
		ROFL_DEBUG("ACTIVE port-list:[");
	
		for(j=0;j<core_task->num_of_rx_ports;++j){
			if(core_task->port_list[j] == NULL){
				ROFL_DEBUG("error_NULL,");
				continue;
			}
			ROFL_DEBUG("%s,",core_task->port_list[j]->name);
		}
		ROFL_DEBUG("]\n");
	}
}



