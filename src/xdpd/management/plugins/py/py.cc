#include "py.h"
#include <sstream>
#include <rofl/common/utils/c_logger.h>
#include "../../plugin_manager.h"
#include "../../switch_manager.h"

//Include python header
#include <python2.7/Python.h>
#include "exported.h"

using namespace xdpd;

#define PLUGIN_NAME "py_plugin" 
#include "exported.h"

unsigned int xdpd::py_proxy::hola::py_sum_test(unsigned int x){
	return x+1;
}
	
const std::string py::LAUNCHER_FUNC="launcher";
py* py::inst;

//Launch a Python script
void* __launch_py_script(void* __mod_id){
	
	py* py_plugin;
	PyGILState_STATE gstate;
	PyObject *strret, *mymod, *strfunc, *strargs;
	unsigned int mod_id = ((uint8_t*)__mod_id) - ((uint8_t*)0x0);

	//Recover python plugin instance
	py_plugin = py::get_inst();
	
	ROFL_DEBUG("[xdpd]["PLUGIN_NAME"] Trampoline for '%s' (%u)\n", py_plugin->get_module_name(mod_id).c_str(), mod_id);

	//Recover Python interpreter state
	gstate = PyGILState_Ensure();


	mymod = PyImport_ImportModule(py_plugin->get_module_name(mod_id).c_str());
	if(!mymod) PyErr_Print();

	strfunc = PyObject_GetAttrString(mymod, py::LAUNCHER_FUNC.c_str());
	if(!strfunc) PyErr_Print();
	
	strargs = Py_BuildValue("(s)", "XXX");
	strret = PyEval_CallObject(strfunc, strargs);
	if(!strret) PyErr_Print();
	
	Py_CLEAR(strargs);

	//Release	
	PyGILState_Release (gstate);

	return NULL;
}

//Plugin init
void py::init(){

	//Initialize 
	if(py::inst == NULL){
		py::inst = this; 
		this->num_of_modules = 0;
	}else{
		//Can never happen
		throw ePyAlreadyInitialized();
	}

	//Initialize Python interpreter
	Py_Initialize();    	
	PySys_SetPath((char*)"/home/marc/BISDN/code/github/xdpd//src/xdpd/management/plugins/py:/home/marc/BISDN/code/github/xdpd//src/xdpd/management/plugins/py/python"); // before ..

	//Init threads
	PyEval_InitThreads();
	PyEval_ReleaseThread(PyThreadState_Get());

	/* TODO remove	*/

	//std::string module = "helloworld";
	//launch(module);

	std::string module = "launcher";
	launch(module);

	/* TODO remove	*/

}

//Plugin init
py::~py(){

	for(std::map<std::string, pthread_t>::iterator it=python_threads.begin(); it != python_threads.end() ;it++){
	
		//TODO give some mercy but kill 

		//Now wait
		pthread_join(it->second, NULL);	
	}
	
	//Destroy Python interpreter
	Py_Finalize();
}

void py::launch(std::string& module){

	pthread_t thread;
	unsigned int num;	

	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Loading '%s'\n", module.c_str());
		
	//Store module info 
	module_names[num_of_modules] = module;
	python_threads[module] = thread;	
	
	//Increment
	num = num_of_modules;
	num_of_modules++;

	//Create a thread
	if(pthread_create( &thread, NULL, __launch_py_script, ((uint8_t*)0x0)+num ) < 0){
		ROFL_INFO("[xdpd]["PLUGIN_NAME"] ERROR: unable launch python module. Reason: unable to create thread for '%s'\n", module.c_str());
		assert(0);	
		throw ePyThread(); 
	}

	ROFL_INFO("[xdpd]["PLUGIN_NAME"] '%s' loaded!\n", module.c_str());
}


/*
* TODO
*/

//Events; print nice traces
void py::notify_port_added(const switch_port_snapshot_t* port_snapshot){
}
	
void py::notify_port_attached(const switch_port_snapshot_t* port_snapshot){
}	

void py::notify_port_status_changed(const switch_port_snapshot_t* port_snapshot){
}
		
void py::notify_port_detached(const switch_port_snapshot_t* port_snapshot){
}	

void py::notify_port_deleted(const switch_port_snapshot_t* port_snapshot){
}
	
void py::notify_monitoring_state_changed(const monitoring_snapshot_state_t* monitoring_snapshot){
}

