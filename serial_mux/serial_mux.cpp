/* 
 * Picard Serial Multiplexer - main project file
 * 
 * Copyright (c) 2010, Dust Networks Inc. 
 */

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <list>
#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

#include <boost/thread.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
#include <boost/date_time/posix_time/posix_time.hpp>

#include "BoostLog.h"

#include "BoostClient.h"
#include "PicardBoost.h"
#include "BoostClientManager.h"
#include "BoostClientListener.h"

#include "Version.h"
#include "SerialMuxOptions.h"
#include "serial_mux.h"


#ifdef WIN32
// nt_service class from http://www.codeproject.com/KB/system/nt_service.aspx
#include "NTService/nt_service.h"
#endif

using namespace DustSerialMux;

#ifdef USE_PICARD_CLR
SerialPort^ connectSerial(String^ port /* serial port */ ) 
{
   // Create a new SerialPort object with default settings.
   SerialPort^ sp = gcnew SerialPort();

   // Allow the user to set the appropriate properties.
   sp->PortName = port;
   sp->BaudRate = DEFAULT_BAUD_RATE;
   //sp->Parity
   //sp->DataBits
   //sp->StopBits
   sp->Handshake = Handshake::None;

   // Set the read/write timeouts
   sp->ReadTimeout = 500;
   sp->WriteTimeout = 500;

   sp->Open();

   sp->DtrEnable = 1; // set CTS
   sp->RtsEnable = 0; // unset RTS (until we're ready to send)
   return sp;
}
#endif

boost::asio::io_service io_service;

CBasePicardIO*        gPicardIO = NULL;
CBoostClientListener* gListener = NULL;
CBoostClientManager*  gClientMgr = NULL;

// signal to handle waiting for main loop completion from service/daemon stop() operation
boost::mutex gMuxLoopMutex;
boost::condition_variable gMuxLoopComplete;


void resetConnection()
{
   // stop the listener
   CBoostLog::log(LOG_ALWAYS, "picard connection reset");

   if (gPicardIO) {
      // close Picard read loop
      CBoostLog::log("stopping Picard read loop");
      gPicardIO->reset();
      gPicardIO->stop();
   }
   if (gClientMgr) {
      // close client connections
      CBoostLog::log("closing client connections");
      gClientMgr->closeClients();
   }
   if (gListener) {
      // stop the listener
      CBoostLog::log("stopping listener");
      gListener->stop();
   }

   io_service.stop();
}


bool muxRunning = true;
SerialMuxOptions opts;


// main loop for Serial Mux
void serial_mux_loop()
{
   // allow an external stop command
   while (muxRunning) {
      io_service.reset();
      
      // connect to Picard
#ifdef USE_PICARD_CLR
      SerialPort^ sp = nullptr;
      UdpClient^ picardSim = nullptr;

      try {
         if (opts.useSerial) {
            String^ serialDevice = gcnew String(opts.serialPort.c_str());
            sp = connectSerial(serialDevice);
            std::ostringstream msg;
            msg << "Connected to serial port " << opts.serialPort;
            CBoostLog::log(LOG_ALWAYS, msg.str());
            gPicardIO = new CPicardCLR_Serial(sp, opts.rtsDelay, opts.useFlowControl, opts.readTimeout);
         } else {
            picardSim = gcnew UdpClient();
            picardSim->Connect("127.0.0.1", opts.emulatorPort);
            gPicardIO = new CPicardCLR_UDP(picardSim, opts.readTimeout);
         }
      }
      catch (Exception^) {
         CBoostLog::log("error: can not open a connection to Picard, retrying");
         boost::this_thread::sleep(boost::posix_time::seconds(1));
         continue;
      }
#else
      try {
         if (opts.useSerial) {
            gPicardIO = new CPicardBoost_Serial(io_service, opts.serialPort,
                                                opts.rtsDelay, opts.useFlowControl, opts.readTimeout);
            std::ostringstream msg;
            msg << "Connected to serial port " << opts.serialPort;
            CBoostLog::log(LOG_ALWAYS, msg.str());
         }
         else {
            gPicardIO = new CPicardBoost_UDP(io_service, opts.emulatorPort, opts.readTimeout);
         }
      }
      catch (const std::exception& ex) {
         std::ostringstream msg;
         msg << "error: can not open a connection to Picard, retrying" << ex.what();
         CBoostLog::log(msg.str());
         boost::this_thread::sleep(boost::posix_time::seconds(1));
         continue;
      }
#endif

      // create the client manager
      gClientMgr = new CBoostClientManager(opts.picardRetries, opts.picardTimeout);
      gPicardIO->registerCallback(gClientMgr);

      // start output
      gPicardIO->start();
      boost::thread picardThread(&CBasePicardIO::threadMain, gPicardIO);
#ifdef WIN32
      SetThreadPriority(picardThread.native_handle(), THREAD_PRIORITY_HIGHEST);
#endif
      // wait for Hello response from Picard
      bool picardReady = gPicardIO->waitForHello();

      if (picardReady) {
         // start command processing thread
         boost::thread clientThread(&CBoostClientManager::commandLoop,
                                    gClientMgr, gPicardIO);
         
         // start listening
         gListener = new CBoostClientListener(io_service, opts.listenerPort, !opts.acceptAnyhost,
                                              *gClientMgr, opts.authToken, gPicardIO->getVersion());
         gListener->asyncListen();
         
         io_service.run();
         
         CBoostLog::log("stopping components");
         
         io_service.reset();
         
         // close command processing thread
         gPicardIO->registerCallback(NULL); // disable callbacks from Picard output
         gClientMgr->stop();
         clientThread.join();
      }
      
      // shutdown output
      gPicardIO->stop();
      picardThread.join();

      CBoostLog::log("deleting components");

      delete gClientMgr;
      gClientMgr = NULL;

      delete gListener;
      gListener = NULL;

      delete gPicardIO;
      gPicardIO = NULL;

#ifdef USE_PICARD_CLR
      // close connection to Picard
      CBoostLog::log("closing Picard connection");
      if (opts.useSerial) {
         sp->Close();
      } else {
         picardSim->Close();
      }
#endif
   }
   
   gMuxLoopComplete.notify_one();   
}


#ifdef WIN32
// start the mux main loop when run as service
void WINAPI serial_mux_start(DWORD argc_, char_* argv_[])
{
   // start the mux main loop
   serial_mux_loop();
}
#endif

// stop the mux main loop when run as daemon/service
void serial_mux_stop()
{
   try {
      muxRunning = false;
      resetConnection();
   }
   catch (std::exception& ex) {
      CBoostLog::log("serial_mux_stop: exception");
      CBoostLog::log(ex.what());
   }

   // wait for the mux loop to complete before returning
   boost::unique_lock<boost::mutex> guard(gMuxLoopMutex);
   gMuxLoopComplete.wait(guard);
}


int main(int argc, char* argv[])
{
   // parse command line
   int result = 0;
   try {
      result = parseConfiguration(opts, argc, argv, std::cout);
   }
   catch (const std::exception& ex) {
      std::cerr << "error: " << ex.what() << std::endl;
      return 1;
   }
   // handle special options like --help and --version where we should exit immediately
   if (result != 0) {
      return 0;
   }

   // use the log as a lock file to indicate another serial mux is running here
   try {
      CBoostLog::getInstance().openLog(opts.logFile, opts.numLogBackups, opts.maxLogSize, opts.logLevel);
      // Print the header
      CBoostLog::log(LOG_ALWAYS, "*** Starting Serial API Multiplexer ***");
   }
   catch (const std::exception&) {
      cerr << "error: can not open log file, another Serial Mux must be running" << endl;
      return 1;
   }

   cout << getVersionString() << endl;
   CBoostLog::log(LOG_ALWAYS, getVersionString());
         
   if (opts.runAsDaemon) {
#ifdef WIN32
      win::nt_service& nts = win::nt_service::instance(opts.serviceName.c_str());

      // register the service main loop 
      nts.register_service_main( serial_mux_start );
      // configure the service to accept stop controls
      nts.register_control_handler( SERVICE_CONTROL_STOP, serial_mux_stop );
      // configure the service to accept shutdown controls
      nts.register_control_handler( SERVICE_CONTROL_SHUTDOWN, serial_mux_stop );

      nts.start();
#else
      cerr << "Running as daemon not implemented" << endl;
      result = 1;
#endif
   } else {
      // go directly to the main loop
      serial_mux_loop();
   }

   CBoostLog::log(LOG_ALWAYS, "serial_mux shutting down"); 

   return result;
}

