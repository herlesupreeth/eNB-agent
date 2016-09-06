# EmPOWER Agent project

The EmPOWER Agent (EMAge for friends) is an integrable piece of software which provides the 'slave' counterpart in the controller-agent communication of EmPOWER. EMAge is technology agnostic: this means that it does not depends on a particular implementation of LTE stack, but can be integrated with any custom software. Just a minimal modification of the stack itself is necessary in order to have the agent working (see documentation for more information).

### Compatibility
This software has been developed and tested for Linux.

### Pre-requisites
In order to successfully build EMAge you need:
* Linux standard build suite (GCC, LD, AR, etc...).
* Pthread library, necessary to handle multithreading.
* Google protocol buffer (protobuf).
* Protobuf-c, Google protocol buffer implementation for C.

### Build instructions
In order to use EMAge the necessary steps to do are:
* Clone this repository.
* Build the protocols used for network communication by invoking the `make` command inside the proto directory (will be built as a shared library).
* Install the proto library in your system by invoking the `make install` command. This will install both the library and the necessary headers.
* Build the agent by invoking the `make` command inside the agent directory (will be built as shared library).
* Install the agent library in your system by invoking the `make install` command. This will install both the library and the necessary headers.
* Include both the libraries when you are embedding the agent in your project.

### Run the agent
Take in account that, in order to run the agent, you must prepare a configuration file which has to be named `agent.conf` and placed under the `/etc/empower` directory. The syntax of such configuration file is, for the moment, outrageously simple, and just contains the necessary information to reach the controller. Such directory will be installed when the agent library is installed in your system, but probably you will need to change the default configuration (which points to the local machine).

### License
In the case you didn't get enough in the header and source files, let me say it again:
"Code is released under the Apache License, Version 2.0".