/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#ifndef SerialMuxOptions_H_
#define SerialMuxOptions_H_

#pragma once

#include <stdint.h>

#include <string>
#include <iostream>

#include "BoostLog.h"  // for log level parameter

namespace DustSerialMux {

   // * Default Configuration Values

   // Serial parameters
   const char   DEFAULT_SERIAL_PORT[] = "COM1";
   const uint32_t DEFAULT_BAUD_RATE = 115200;
   const int    DEFAULT_RTS_DELAY = 5; // milliseconds to delay deasserting RTS after a write
   const bool   DEFAULT_FLOW_CONTROL = false;

   const bool   DEFAULT_TO_SERIAL = true;

   const int DEFAULT_PICARD_TIMEOUT = 3000;  // command timeout (how long to wait for
                                             // a response from Picard), milliseconds
   const int DEFAULT_PICARD_RETRIES = 2; // number of times to retry the command to Picard

   const int DEFAULT_READ_TIMEOUT = 1000; // millisecond timeout for read operation
   
   // Command line defaults
   const uint16_t DEFAULT_LISTENER_PORT = 9900;
   const uint16_t DEFAULT_EMULATOR_PORT = 60000;
   const bool     DEFAULT_ACCEPT_ANYHOST = false;

   const char   DEFAULT_CONFIG_FILE[] = "serial_mux.cfg";

   // The authentication token is configurable
   const int AUTHENTICATION_LEN = 8;
   const uint8_t DEFAULT_AUTHENTICATION[] = { 48, 49, 50, 51, 52, 53, 54, 55 };

   // Run as daemon / service
   const bool DEFAULT_RUN_AS_DAEMON = false;
   const char DEFAULT_SERVICE_NAME[] = "SerialMux";
   
   // Log configuration
   const char DEFAULT_LOG_FILE[] = "serial_mux.log";
   const int  DEFAULT_NUM_LOG_BACKUPS = 5;
   const int  DEFAULT_MAX_LOG_SIZE = 1000000; // max log size in bytes
   const LogLevel  DEFAULT_LOG_LEVEL = LOG_ERROR;  // see BoostLog.h
   
   /** 
    * Serial Mux Options structure
    */

   struct SerialMuxOptions
   {
      // configuration file
      std::string  configFile;
      // Picard connection
      bool         useSerial;
      std::string  serialPort;
      // Serial port parameters
      uint32_t     baudRate;
      bool         useFlowControl;
      int          rtsDelay;
      // Emulator parameters
      uint16_t     emulatorPort;
      // Mux client parameters
      uint16_t     listenerPort;
      bool         acceptAnyhost;
      uint8_t      authToken[AUTHENTICATION_LEN];
      // Picard protocol
      int          picardTimeout;
      int          picardRetries;
      int          readTimeout;  // TODO: should this match the higher-level command timeout ?
      // Run as daemon / service
      bool         runAsDaemon;
      std::string  serviceName;
      
      // log parameters
      LogLevel     logLevel;
      std::string  logFile;
      int          numLogBackups;
      int          maxLogSize;

      SerialMuxOptions() 
         : configFile(DEFAULT_CONFIG_FILE),
           useSerial(DEFAULT_TO_SERIAL),
           serialPort(DEFAULT_SERIAL_PORT),
           baudRate(DEFAULT_BAUD_RATE),
           useFlowControl(DEFAULT_FLOW_CONTROL),
           rtsDelay(DEFAULT_RTS_DELAY),
           emulatorPort(DEFAULT_EMULATOR_PORT),
           listenerPort(DEFAULT_LISTENER_PORT),
           acceptAnyhost(DEFAULT_ACCEPT_ANYHOST),
           picardTimeout(DEFAULT_PICARD_TIMEOUT),
           picardRetries(DEFAULT_PICARD_RETRIES),
           readTimeout(DEFAULT_READ_TIMEOUT),
           runAsDaemon(DEFAULT_RUN_AS_DAEMON),
           serviceName(DEFAULT_SERVICE_NAME),
           logLevel(DEFAULT_LOG_LEVEL),
           logFile(DEFAULT_LOG_FILE),
           numLogBackups(DEFAULT_NUM_LOG_BACKUPS),
           maxLogSize(DEFAULT_MAX_LOG_SIZE)
      { 
         std::copy(DEFAULT_AUTHENTICATION, DEFAULT_AUTHENTICATION + AUTHENTICATION_LEN,
            authToken);
      }
   };


   /** 
    * parseConfiguration
    * Parse the command line and configuration file.
    * Sets values in options structure.
    * Throws exception on parse error
    * Returns: 0 if OK, 1 if caller should exit
    */
   int parseConfiguration(struct SerialMuxOptions& options,
                          int argc, char* argv[], std::ostream& out);

} // namespace

#endif /* ! SerialMuxOptions_H_ */
