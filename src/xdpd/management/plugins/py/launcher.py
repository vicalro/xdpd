
def launcher(args):
	import time
	import rest 

	print "[xdpd][py][test] Initializing..."
	
	rest.init()
	
	time.sleep(5)
	print "[xdpd][py][test] Exiting!!"
