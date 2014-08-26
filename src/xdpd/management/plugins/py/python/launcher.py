
def launcher(args):
	import time
	import rest 

	print "[xdpd][py][test] Initializing..."
	
	try:
		import libxdpd_mgmt_py_native
		x = 1
		#print "--->"+str(libxdpd_mgmt_py_native.hola.__dict__)
		h = libxdpd_mgmt_py_native.hola()
		print "Hola module: "+str(h.__dict__)
		print "Result: "+str(h.py_sum_test(x))
			
		p = libxdpd_mgmt_py_native.port_manager()
		print "Instance : "+str(p.__dict__)
		port_name = "veth0"
		#print "Interface veth0"+ str(p.exists(port_name))
		print "Interfaces"+ str(p.list_available_port_names(False))
		
	except Exception as e:
		print "ERROR: "+str(e)
	
	rest.init()
	
	time.sleep(5)
	print "[xdpd][py][test] Exiting!!"
