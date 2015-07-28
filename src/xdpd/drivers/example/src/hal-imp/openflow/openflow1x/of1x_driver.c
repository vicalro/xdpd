#include <rofl/datapath/hal/openflow/openflow1x/of1x_driver.h>
#include <rofl/common/utils/c_logger.h>
#include <rofl/datapath/pipeline/physical_switch.h>
#include <rofl/datapath/pipeline/openflow/of_switch.h>
#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_pipeline.h>
#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_flow_entry.h>
#include <rofl/datapath/pipeline/openflow/openflow1x/pipeline/of1x_statistics.h>

//Port config

/**
 * @name    hal_driver_of1x_set_port_drop_received_config
 * @brief   Instructs driver to modify port config state 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 			Datapath ID of the switch 
 * @param port_num		Port number 	
 * @param drop_received		Drop packets received
 */
hal_result_t hal_driver_of1x_set_port_drop_received_config(uint64_t dpid, unsigned int port_num, bool drop_received){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_set_port_no_flood_config
 * @brief   Instructs driver to modify port config state 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 			Datapath ID of the switch 
 * @param port_num		Port number 	
 * @param no_flood		No flood allowed in port
 */
hal_result_t hal_driver_of1x_set_port_no_flood_config(uint64_t dpid, unsigned int port_num, bool no_flood){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_set_port_forward_config
 * @brief   Instructs driver to modify port config state 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 			Datapath ID of the switch 
 * @param port_num		Port number 	
 * @param forward		Forward packets
 */
hal_result_t hal_driver_of1x_set_port_forward_config(uint64_t dpid, unsigned int port_num, bool forward){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_SUCCESS;
}
/**
 * @name    hal_driver_of1x_set_port_generate_packet_in_config
 * @brief   Instructs driver to modify port config state 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 			Datapath ID of the switch 
 * @param port_num		Port number 	
 * @param generate_packet_in	Generate packet in events for this port 
 */
hal_result_t hal_driver_of1x_set_port_generate_packet_in_config(uint64_t dpid, unsigned int port_num, bool generate_packet_in){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_set_port_advertise_config
 * @brief   Instructs driver to modify port advertise flags 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 			Datapath ID of the switch 
 * @param port_num		Port number 	
 * @param advertise		Bitmap advertised
 */
hal_result_t hal_driver_of1x_set_port_advertise_config(uint64_t dpid, unsigned int port_num, uint32_t advertise){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_set_pipeline_config
 * @brief   Instructs driver to process a PACKET_OUT event
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch 
 * @param flags		Capabilities bitmap (OF12_CAP_FLOW_STATS, OF12_CAP_TABLE_STATS, ...)
 * @param miss_send_len	OF MISS_SEND_LEN
 */
hal_result_t hal_driver_of1x_set_pipeline_config(uint64_t dpid, unsigned int flags, uint16_t miss_send_len){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_set_table_config
 * @brief   Instructs driver to set table configuration(default action)
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch
 * @param table_id	Table ID or 0xFF for all 
 * @param miss_send_len Table miss config	
 */
hal_result_t hal_driver_of1x_set_table_config(uint64_t dpid, unsigned int table_id, of1x_flow_table_miss_config_t config){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_process_packet_out
 * @brief   Instructs driver to process a PACKET_OUT event
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to process PACKET_OUT
 * @param buffer_id	Buffer ID
 * @param in_port 	Port IN
 * @param action_group 	Action group to apply
 * @param buffer		Pointer to the buffer
 * @param buffer_size	Buffer size
 */
hal_result_t hal_driver_of1x_process_packet_out(uint64_t dpid, uint32_t buffer_id, uint32_t in_port, of1x_action_group_t* action_group, uint8_t* buffer, uint32_t buffer_size)
{
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_SUCCESS;
}

/**
 * @name    hal_driver_of1x_process_flow_mod
 * @brief   Instructs driver to process a FLOW_MOD event
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the FLOW_MOD
 * @param table_id 	Table id to install the flowmod
 * @param flow_entry	Flow entry to be installed
 * @param buffer_id	Buffer ID
 * @param out_port 	Port to output
 * @param check_overlap	Check OVERLAP flag
 * @param check_counts	Check RESET_COUNTS flag
 */

hal_fm_result_t hal_driver_of1x_process_flow_mod_add(uint64_t dpid, uint8_t table_id, of1x_flow_entry_t** flow_entry, uint32_t buffer_id, bool check_overlap, bool reset_counts){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_FM_SUCCESS;
}

/**
 * @name    hal_driver_of1x_process_flow_mod_modify
 * @brief   Instructs driver to process a FLOW_MOD modify event
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the FLOW_MOD
 * @param table_id 	Table id from which to modify the flowmod
 * @param flow_entry	Flow entry 
 * @param buffer_id	Buffer ID
 * @param strictness 	Strictness (STRICT NON-STRICT)
 * @param check_counts	Check RESET_COUNTS flag
 */
hal_fm_result_t hal_driver_of1x_process_flow_mod_modify(uint64_t dpid, uint8_t table_id, of1x_flow_entry_t** flow_entry, uint32_t buffer_id, of1x_flow_removal_strictness_t strictness, bool reset_counts){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_FM_SUCCESS;
}


/**
 * @name    hal_driver_of1x_process_flow_mod_delete
 * @brief   Instructs driver to process a FLOW_MOD event
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the FLOW_MOD
 * @param table_id 	Table id to install the flowmod
 * @param flow_entry	Flow entry to be installed
 * @param out_port 	Out port that entry must include
 * @param out_group 	Out group that entry must include	
 * @param strictness 	Strictness (STRICT NON-STRICT)
 */
hal_fm_result_t hal_driver_of1x_process_flow_mod_delete(uint64_t dpid, uint8_t table_id, of1x_flow_entry_t* flow_entry, uint32_t out_port, uint32_t out_group, of1x_flow_removal_strictness_t strictness){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return HAL_FM_SUCCESS;
}

//
// Statistics
//

/**
 * @name    hal_driver_of1x_get_flow_stats
 * @brief   Recovers the flow stats given a set of matches 
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the FLOW_MOD
 * @param table_id 	Table id to get the flows of 
 * @param cookie	Cookie to be applied 
 * @param cookie_mask	Mask for the cookie
 * @param out_port 	Out port that entry must include
 * @param out_group 	Out group that entry must include	
 * @param matches	Matches
 */
of1x_stats_flow_msg_t* hal_driver_of1x_get_flow_stats(uint64_t dpid, uint8_t table_id, uint32_t cookie, uint32_t cookie_mask, uint32_t out_port, uint32_t out_group, of1x_match_group_t* matches){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return NULL; 
}

 
/**
 * @name    driver_of1x_get_flow_aggregate_stats
 * @brief   Recovers the aggregated flow stats given a set of matches 
 * @ingroup of1x_hal_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the FLOW_MOD
 * @param table_id 	Table id to get the flows of 
 * @param cookie	Cookie to be applied 
 * @param cookie_mask	Mask for the cookie
 * @param out_port 	Out port that entry must include
 * @param out_group 	Out group that entry must include	
 * @param matches	Matches
 */
of1x_stats_flow_aggregate_msg_t* hal_driver_of1x_get_flow_aggregate_stats(uint64_t dpid, uint8_t table_id, uint32_t cookie, uint32_t cookie_mask, uint32_t out_port, uint32_t out_group, of1x_match_group_t* matches){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);

	return NULL; 
} 
/**
 * @name    hal_driver_of1x_group_mod_add
 * @brief   Instructs driver to add a new GROUP
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the GROUP
 */
hal_gm_result_t hal_driver_of1x_group_mod_add(uint64_t dpid, of1x_group_type_t type, uint32_t id, of1x_bucket_list_t **buckets){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_GM_SUCCESS;
}

/**
 * @name    hal_driver_of1x_group_mod_modify
 * @brief   Instructs driver to modify the GROUP with identification ID
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the GROUP
 */
hal_gm_result_t hal_driver_of1x_group_mod_modify(uint64_t dpid, of1x_group_type_t type, uint32_t id, of1x_bucket_list_t **buckets){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_GM_SUCCESS;
}

/**
 * @name    hal_driver_of1x_group_mod_del
 * @brief   Instructs driver to delete the GROUP with identification ID
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch to install the GROUP
 */
hal_gm_result_t hal_driver_of1x_group_mod_delete(uint64_t dpid, uint32_t id){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return HAL_GM_SUCCESS;
}

/**
 * @ingroup core_of1x
 * Retrieves a copy of the group and bucket structure
 * @return of1x_stats_group_desc_msg_t instance that must be destroyed using of1x_destroy_group_desc_stats()
 */
of1x_stats_group_desc_msg_t *hal_driver_of1x_get_group_desc_stats(uint64_t dpid){
	
	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return NULL; 
}

/**
 * @name    hal_driver_of1x_get_group_stats
 * @brief   Instructs driver to fetch the GROUP statistics
 * @ingroup of1x_driver_async_event_processing
 *
 * @param dpid 		Datapath ID of the switch where the GROUP is
 */
of1x_stats_group_msg_t * hal_driver_of1x_get_group_stats(uint64_t dpid, uint32_t id){

	ROFL_INFO("["DRIVER_NAME"] calling %s()\n",__FUNCTION__);
	
	return NULL; 
}
