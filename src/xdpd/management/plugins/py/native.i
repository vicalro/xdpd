 %module example
 %{
 /* Includes the header in the wrapper code */
 #include "exported.h"
 %}
 
 /* Parse the header file to generate wrappers */
 %include "exported.h"
