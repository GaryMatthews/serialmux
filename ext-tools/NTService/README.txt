* Overview 

The NT Service code provides an interface for starting and stopping a service
under Windows NT (including XP or later) using the WIN32 API (not the .NET APIs).

The NT Service code was obtained from 
http://www.codeproject.com/KB/system/nt_service.aspx.

The code is distributed under the terms of the Code Project Open License, see
LICENSE.txt. There are no requirements to distribute the associated source
code or a specific copyright notice with any work that includes this code. 


* Usage

The nt_service class expects:
* to call the main loop from within the service start code. 
* a stop function to be registered that shuts down the main loop.

The nt_service class expects the signature of the main loop function to
imitate a Windows main loop (which is not platform independent). Platform
independent code may want to wrap the declaration of the main loop function as
shown below in order to provide different function signatures on different
platforms.


#ifdef WIN32
#include "nt_service.h"

#define DECLARE_MAIN_LOOP(func) void WINAPI func(DWORD argc_, char_* argv_[])
#define CALL_MAIN_LOOP(func) func(0, NULL)

#else
#define DECLARE_MAIN_LOOP(func) void func()
#define CALL_MAIN_LOOP(func) func()
#endif


// stop the main loop
void main_loop_stop()
{
   doStop();
}

DECLARE_MAIN_LOOP(main_loop)
{
   // allow an external stop command
   while (isRunning) {
     doProcessing();
   }
}

main(...) 
{
    ...

#ifdef WIN32
    // only create the service object if we should run as a service 
    
    win::nt_service& nts = win::nt_service::instance(L"MyService");

    // register the service main loop 
    nts.register_service_main( main_loop );
    // configure the service to accept stop controls
    nts.register_control_handler( SERVICE_CONTROL_STOP, main_loop_stop );
    // configure the service to accept shutdown controls
    nts.register_control_handler( SERVICE_CONTROL_SHUTDOWN, main_loop_stop );

    nts.start();
#else
    if (runAsDaemon) doDaemonize();

    // go directly to the main loop
    CALL_MAIN_LOOP(main_loop);

#endif

    ...
}
