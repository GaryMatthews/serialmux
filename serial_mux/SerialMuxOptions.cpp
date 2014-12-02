/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#include "SerialMuxOptions.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/program_options.hpp>
using namespace boost::program_options;

#include <boost/filesystem.hpp>

#include "Version.h"  // for command line version output

namespace DustSerialMux {

   // Convert hex string (two ASCII characters per byte) into binary data
   // Returns: length of the converted binary data
   // Returns: 0 if an error is encountered, i.e. if the string contains invalid chars
   //          or the output buffer is not large enough
   // outBuf is filled starting at the beginning of the buffer
   int hexToBin(const char* str, uint8_t * outBuf, int len)
   {
      int  idx      = 0,
         nn       = 0; 
      bool isOk     = true,
         wasDigit = false;
      memset(outBuf, 0, len);
      while (isspace(*str)) str++;
      while(isOk) {
         char c       = *str++;
         bool isDigit = isxdigit(c)!=0;
         isOk = wasDigit && (c==0 || isspace(c) || c=='-' || c==':') || isDigit;
         wasDigit =  isDigit;
         if (isOk) {
            if (!isDigit || nn==2) {
               idx++; 
               nn = 0;
            }
            if (isDigit) {
               isOk = idx < len;
               if (isOk) {
                  int numb = isdigit(c) ? c-'0' : tolower(c)-'a'+10;
                  outBuf[idx] = (outBuf[idx] << 4) + numb;
                  nn++;
               } 
            }
            else if (c!='-' && c!=':') {
               break;
            }
         } 
      }
      if (!isOk) {
         idx = 0;
      }
      return idx;
   }


   // parseConfiguration
   // Parse the command line and configuration file.
   // Sets values in options structure.
   // Throws exception on parse error
   // Returns: 0 if OK, 1 if caller should exit

   int parseConfiguration(struct SerialMuxOptions& options,
                          int argc, char* argv[], std::ostream& out)
   {
      std::string logLevel;
      
      // General options are allowed anywhere
      options_description g("General options");
      g.add_options()
         ("port,p",
          value<std::string>(&options.serialPort)->default_value(DEFAULT_SERIAL_PORT),
          "Picard port")
         ("listen,l",
          value<uint16_t>(&options.listenerPort)->default_value(DEFAULT_LISTENER_PORT),
          "Listener port")
         ("accept-anyhost", "Accept connections from any host (instead of localhost only)")
         ("rts-delay,d",
          value<int>(&options.rtsDelay)->default_value(DEFAULT_RTS_DELAY), "RTS delay")
         ("picard-timeout",
          value<int>(&options.picardTimeout)->default_value(DEFAULT_PICARD_TIMEOUT),
          "Picard command timeout")
         ("picard-retries",
          value<int>(&options.picardRetries)->default_value(DEFAULT_PICARD_RETRIES),
          "Picard command retries")
         ("read-timeout",
          value<int>(&options.readTimeout)->default_value(DEFAULT_READ_TIMEOUT),
          "Low-level read operation timeout")
         ("flow-control", "Use RTS flow control")
         ("log-level",
          value<std::string>(&logLevel),
          "Minimum level of messages to log")
         ("log-file",
          value<std::string>(&options.logFile)->default_value(DEFAULT_LOG_FILE),
          "Path to log file relative to executable")
         ("log-num-backups",
          value<int>(&options.numLogBackups)->default_value(DEFAULT_NUM_LOG_BACKUPS),
          "Number of log file backups to keep")
         ("log-max-size",
          value<int>(&options.maxLogSize)->default_value(DEFAULT_MAX_LOG_SIZE),
          "Maximum log file size before files are rotated")
         ("daemon", "Run as daemon")
         ("service-name",
          value<std::string>(&options.serviceName)->default_value(DEFAULT_SERVICE_NAME),
          "Name of the service to register as when running as daemon on Windows")
         ;

      // Command line options
      std::string dir;
      options_description cmdline_options;
      cmdline_options.add(g);

      cmdline_options.add_options()
         ("version,v", "Print the version string")
         ("help", "Print this help message")
         ("config,c", value<std::string>(&options.configFile), "Path to the configuration file")
         ("directory", value<std::string>(&dir), "Set the working directory")
         ;

      variables_map vm;

      try {
         store(command_line_parser(argc, argv).
            options(cmdline_options).run(), vm);
         notify(vm);
      }
      catch (const unknown_option& ex) {
         std::ostringstream msg;
         msg << "unknown option '" << ex.get_option_name() << "'";
         throw std::invalid_argument(msg.str());
      }


      if (vm.count("help")) {
         out << cmdline_options << std::endl;
         return 1;
      }

      if (vm.count("version")) {
         out << getVersionString() << std::endl;
         return 1;
      }

      if (vm.count("directory")) {
         // change the directory before trying to read the configuration file
         boost::filesystem::current_path(vm["directory"].as<std::string>());
      }

      // Config file options
      std::string authTokenStr;

      options_description conf_options;
      conf_options.add(g);
      conf_options.add_options()
         ("authToken", value<std::string>(&authTokenStr), "Authentication token")
         ("baud", value<uint32_t>(&options.baudRate), "Picard baud rate")
         ;

      // Read config file
      try {
         std::ifstream ifs(options.configFile.c_str());
         if (!ifs && vm.count("config")) {
            std::ostringstream msg;
            msg << "can not open config file: " << options.configFile;
            throw std::invalid_argument(msg.str());
         } 
         if (ifs.good()) {
            store(parse_config_file(ifs, conf_options), vm);
            notify(vm);
         }
      }
      catch (const unknown_option& ex) {
         std::ostringstream msg;
         msg << "unknown option '" << ex.get_option_name() << "' in " << options.configFile;
         throw std::invalid_argument(msg.str());
      }

      // * Parse option values

      // parse Picard port
      options.emulatorPort = atoi(options.serialPort.c_str());
      if (options.emulatorPort > 0 && options.emulatorPort < 65535) {
         options.useSerial = false;
      }
   
      // TODO: can we detect invalid port values?
   
      // parse Authentication Token
      if (vm.count("authToken")) {
         int result = hexToBin(authTokenStr.c_str(), options.authToken, AUTHENTICATION_LEN);
         if (result == 0) {
            std::ostringstream msg;
            msg << "invalid authentication token: " << authTokenStr;
            throw std::invalid_argument(msg.str());
         }
      }

      // check whether anyhost was specified
      if (vm.count("accept-anyhost")) {
         options.acceptAnyhost = true;
      }

      // check whether flow control was specified
      if (vm.count("flow-control")) {
         options.useFlowControl = true;
      }

      // check whether daemon mode was specified
      if (vm.count("daemon")) {
         options.runAsDaemon = true;
      }

      // parse the log level
      if (vm.count("log-level")) {
         options.logLevel = stringToEnum(logLevel);
      }

      return 0;
   }

} // namespace
