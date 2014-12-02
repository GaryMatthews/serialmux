#include "nt_service.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using win::nt_service;
using std::wofstream;
using std::ios_base;

const wchar_t * logfile = L"C:\\output.txt";

void WINAPI my_service_main(DWORD argc, char_* argv[])
{
	try
	{
		static int i = 0;

		wofstream fout(logfile,ios_base::app);
        if ( fout.is_open() ){
			fout << "Service main: counting: [" << ++i <<"]\n";
            fout.close();
        }
        Sleep(5000);			
	}
	catch(...)
	{
		nt_service::stop(-1);
	}
}
void my_init_fcn(void)
{
	try
	{
		wofstream fout(logfile,ios_base::app);
        if ( fout.is_open() ){
			fout << "Service Initializing...\n";
            fout.close();
        }
	}
	catch(...)
	{
		nt_service::stop(-1);
	}
}
void my_shutdown_fcn(void)
{
	Beep(1000,1000);
}

void wmain(DWORD argc, LPWSTR* argv)
{
	// creates an access point to the instance of the service framework
	nt_service&  service = nt_service::instance(L"test_service");

	// register "my_service_main" to be executed as the service main method 
	service.register_service_main( my_service_main );

	// register "my_init_fcn" as initialization fcn
	service.register_init_function( my_init_fcn );
	
	// config the service to accept stop controls. Do nothing when it happens
	service.accept_control( SERVICE_ACCEPT_STOP );

	// config the service to accept shutdown controls and do something when receive it 
	service.register_control_handler( SERVICE_CONTROL_SHUTDOWN, my_shutdown_fcn );
		
	service.start();
}