from collections import OrderedDict
import re

#
# Protocols
#
#protocols = OrderedDict = 
protocols = OrderedDict(
	(
	("ETHERNET", 14), 
	("8023", 17), 
	("MPLS", 4), 
	("VLAN", 4), 
	("PPPOE", 20), 
	("PPP", 20), 
	("ARPV4", 28), 
	("ICMPV4", 4), 
	("IPV4", 20), 
	("ICMPV6", 4), 
	("ICMPV6_OPTS", 20), 
	("ICMPV6_OPTS_LLADR_SRC", 20), 
	("ICMPV6_OPTS_LLADR_TGT", 20), 
	("ICMPV6_OPTS_PREFIX_INFO", 20), 
	("IPV6", 40), 
	("TCP", 32), 
	("UDP", 8), 
	("SCTP", 12),
	("GTPU", 20)
	)
)

#
# Packet types (using a notation similar to scapy)
#
pkt_types = [ 
	"ETH\VLAN",

	#MPLS - no parsing beyond 	
	"ETH\VLAN/MPLS",
	
	#PPPOE/PPP We don't parse beyond that, for the moment
	"ETH\VLAN/PPPOE",
	"ETH\VLAN/PPPOE/PPP",

	#ARP	
	"ETH\VLAN/ARPV4",
	
	#ICMPs
	"ETH\VLAN/ICMPV4",
	"ETH\VLAN/ICMPV6",

	#IPv4	
	"ETH\VLAN/IPV4",
	"ETH\VLAN/IPV4/TCP",
	"ETH\VLAN/IPV4/UDP",
	"ETH\VLAN/IPV4/UDP/GTPU",
	"ETH\VLAN/IPV4/SCTP",

	#IPv6	
	"ETH\VLAN/IPV6",
	"ETH\VLAN/IPV6/TCP",
	"ETH\VLAN/IPV6/UDP",
	"ETH\VLAN/IPV6/UDP/GTPU",
	"ETH\VLAN/IPV6/SCTP",
]

#
# This is the unrolled version (options, mpls labels...) of the previous type
#
pkt_types_unrolled = [ ]

#Unroll parameters 
ipv4_max_options_words = 15 #15 Inclusive 15x4bytes=60bytes 
mpls_max_depth=16 #Maximum

##
## Helper Functions
##

def filter_pkt_type(pkt_type):
	#MPLS
	if "MPLS" in pkt_type:
		return pkt_type.split("_nlabels_")[0]
	
	if "IPV4" in pkt_type:
		return pkt_type.split("_noptions_")[0]
	
	return pkt_type

def unroll_pkt_types():
	for type__ in pkt_types:

		#Unroll ETH or ETH + VLAN
		unrolled_types=[]
		if "ETH\\" in type__:
			#Add ETHERNET+VLAN 
			tmp = type__.replace("ETH\\","ETHERNET/")
			unrolled_types.append(tmp)
			#Add 802.3+VLAN
			tmp = type__.replace("ETH\\","8023/")
			unrolled_types.append(tmp)
	
			#Add without optional
			tmp2 = type__.replace("ETH\\VLAN","ETHERNET")
			unrolled_types.append(tmp2)
			#Add 802.3
			tmp2 = type__.replace("ETH\\VLAN","8023")
			unrolled_types.append(tmp2)
			
		else:
			unrolled_types.append(type__)
			

		#Loop over all "unrolled types
		for type_ in unrolled_types:
			if "IPV4" in type_:
				for i in range(0,ipv4_max_options_words+1):
					pkt_types_unrolled.append(type_.replace("IPV4","IPV4_noptions_"+str(i)))
				continue
			if "MPLS" in type_:
				for i in range(1,mpls_max_depth+1):
					pkt_types_unrolled.append(type_.replace("MPLS","MPLS_nlabels_"+str(i)))
				continue
			pkt_types_unrolled.append(type_)

def sanitize_pkt_type(pkt_type):
	pkt_type = pkt_type.replace("/","_")
	return pkt_type	

def calc_inner_len_pkt_type(curr_len, pkt_type):
	""" Inner len in the offset of the inner most header for stackable headers (MPLS, VLAN)"""	
	if "MPLS" in pkt_type:
		#Return inner-most
		n_mpls_labels = int(pkt_type.split("_nlabels_")[1])
		curr_len += protocols[filter_pkt_type(pkt_type)]*(n_mpls_labels-1)
	return curr_len

def calc_len_pkt_type(pkt_type):
	""" Calculates the total len including options and other variable length headers depending on the unrolled pkt types"""	
	if "IPV4" in pkt_type:
		n_options = int(pkt_type.split("_noptions_")[1])
		return protocols[filter_pkt_type(pkt_type)] + (n_options*4) #1 option = 4 octets
	
	return protocols[filter_pkt_type(pkt_type)]	
##
## Main functions
##

def license(f):
	f.write("/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n")

def init_guard(f):
	f.write("#ifndef PKT_TYPES_H\n#define PKT_TYPES_H\n\n")
def end_guard(f):
	f.write("#endif //PKT_TYPES_H\n")

def comments(f):
	f.write("//This file has been autogenerated. DO NOT modify it\n\n")	
def protocols_enum(f):
	f.write("typedef enum protocol_types{\n")
	for proto in protocols:
		f.write("\tPT_PROTO_"+proto+",\n")
		
	f.write("\tPT_PROTO_MAX__\n}protocol_types_t;\n\n")

def packet_types_enum(f):
	
	f.write("typedef enum pkt_types{\n")
	for type_ in pkt_types_unrolled:
		f.write("\tPT_"+sanitize_pkt_type(type_)+",\n")
		
	f.write("\tPT_MAX__\n}pkt_types_t;\n\n")

def packet_offsets(f):
	
	f.write("const int protocol_offsets_bt[PT_MAX__][PT_PROTO_MAX__] = {")

	first_type = True 
	
	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in protocols:
		f.write(type_+",")
	f.write("}*/ \n")
	
	#Real rows
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in protocols:
			row.append(-1)
	
		for proto in type_.split("/"):
			row[ protocols.keys().index(filter_pkt_type(proto))] = calc_inner_len_pkt_type(len, proto) 
			len += calc_len_pkt_type(proto)

		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for proto_len in row: 
			if not first_proto:
				f.write(",")
			first_proto = False
			f.write(str(proto_len))

		f.write("}")
		
	f.write("\n};\n\n")
	
def parse_transitions(f):
	
	# Unroll protocols (only IPv4)
	# We need to transition based on IPv4 options
	unrolled_protocols = OrderedDict()
	
	for proto in protocols:
		if "IPV4" in proto:
			for i in range(0,ipv4_max_options_words+1):
				unrolled_protocols[proto.replace("IPV4","IPV4_noptions_"+str(i))] = 0
			continue
		unrolled_protocols[proto] = 0

	##
	## Unrolled protocols enum
	##
	f.write("//Only used in transitions (inside the MACRO )\n")
	f.write("typedef enum __unrolled_protocol_types{\n")
	for proto in unrolled_protocols:
		f.write("\t__UNROLLED_PT_PROTO_"+proto+",\n")
		
	f.write("\t__UNROLLED_PT_PROTO_MAX__\n}__unrolled_protocol_types_t;\n\n")


	##
	## Parse transitions
	##
	f.write("//Matrix of incremental classification steps. The only purpose of this matrix is to simplify parsing code, it is not essentially necessary, although _very_ desirable.\n")
	f.write("const int parse_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__] = {")

	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in unrolled_protocols:
		f.write(type_+",")
	f.write("}*/ \n")

	#Real rows
	first_type = True 
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in unrolled_protocols:
			new_type=""
			if "MPLS" in proto:
				#Special cases: MPLS
				if type_.split("MPLS_nlabels_").__len__() == 1:
					new_type=type_+"/"+proto+"_nlabels_1"	
				elif type_.split("MPLS_nlabels_").__len__() == 2:
					n_labels = int(type_.split("MPLS_nlabels_")[1])+1
					new_type=type_.split("MPLS_nlabels_")[0]+"MPLS_nlabels_"+str(n_labels)
				else:
					#No type
					new_type=type_+"/"+proto
			else:
				new_type=type_+"/"+proto

			if new_type in pkt_types_unrolled:
				row.append("PT_"+sanitize_pkt_type(new_type))
			else:
				row.append("0")
			#row.append("PT_"+sanitize_pkt_type(type_))
		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for next_proto in row: 
			if not first_proto:
				f.write(",")
			first_proto = False
			f.write(next_proto)

		f.write("}")
		
	f.write("\n};\n\n")

def get_hdr_macro(f):
	f.write("\n#define PT_GET_HDR(tmp, state, proto)\\\n\tdo{\\\n\t\ttmp = state->base + protocol_offsets_bt[ state->type ][ proto ];\\\n\t\tif(tmp < state->base )\\\n\t\t\ttmp = NULL;\\\n\t}while(0)\n\n")

def add_class_type_macro(f):
	#Add main Macro
	f.write("\n#define PT_CLASS_ADD_PROTO(state, PROTO_TYPE) do{\\\n")
	f.write("\tpkt_types_t next_header = (pkt_types_t)parse_transitions[state->type][ __UNROLLED_PT_PROTO_##PROTO_TYPE ];\\\n")
	f.write("\tif( unlikely(next_header == 0) ){ assert(0); return; }else{ state->type = next_header;  }\\\n")
	f.write("}while(0)\n")
	
	#Add IPv4 options macro
	f.write("\n#define PT_CLASS_ADD_IPV4_OPTIONS(state, NUM_OPTIONS) do{\\\n")
	f.write("\tswitch(NUM_OPTIONS){\\\n")
	for i in range(0,ipv4_max_options_words+1):
		f.write("\t\tcase "+str(i)+": PT_CLASS_ADD_PROTO(state, IPV4_noptions_"+str(i)+");break;\\\n")
	f.write("\t\tdefault: assert(0);\\\n")
	f.write("}}while(0)\n")
	
##
## Main function
##

def main():

	unroll_pkt_types()

	#Open file
	with open('../autogen_pkt_types.h', 'w') as f:	
		
		#License
		license(f)

		#Init guard
		init_guard(f)
		
		#Header comment
		comments(f)
	
		#Generate protocol enum
		protocols_enum(f)

		#Generate pkt types enum
		packet_types_enum(f)
	
		#Offsets in bytes
		packet_offsets(f)

		#Transitions
		parse_transitions(f)
		#mpls_assign_type_macro(f)

		#Macros
		get_hdr_macro(f)
		add_class_type_macro(f)	
		#End of guards
		end_guard(f)

if __name__ == "__main__":
	main()
