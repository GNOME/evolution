Camel is currently (conceptualy) separated in four parts:

* the session handling 
* the storage mechanism.
* the (mime) message handling.
* some general utilities class/functions.



* Session handling 
------------------

(This is not gnome session managing related)
CamelSession is an object used to store some parameters on a user
basis. This can be a permanent (fs based) or volatile 
(ram only) storage depending on user preferences. 
The session object is, for example, responsible for 
remembering authentication datas during application lifetime.
It is also responsible for selecting and loading providers 
corresponding to protocols. In the case where only one 
provider exists for a given protocol, the task is trivial, 
but when multiple providers exist for a given protocol, the
user can choose their prefered one. Given its relationship
with providers, the session object is also used to instanciate
a store given an URL.

Associated Classes:
  CamelSession 
    implementation: 5%

Associated Files:
  camel-provider.[ch]
    implementation: 2.5% (a struct in camel-provider.h)


* the storage mechanism.
------------------------

The storage mechanism is  mainly represented by 
the Store class and the Folder class. 
* the (mime) message handling.
* some general utilities class/functions.

