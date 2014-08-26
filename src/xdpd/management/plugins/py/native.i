%module libxdpd_mgmt_py_native 
 %{

 /* Include std */
 #include <vector> 
 #include <string> 
 #include <set> 

 /* Includes the header in the wrapper code */
 #include "exported.h"

 using namespace xdpd;
 using namespace xdpd::py_proxy;

 %}
 
 /* Parse the header file to generate wrappers */
 %include "exported.h"
