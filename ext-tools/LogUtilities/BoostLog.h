/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

/*
 * BoostLog creates a thread for writing log messages without delaying the main process.
 * If you need to use the Boost thread library as a DLL for compatibility with the .NET CLR
 * you must define BOOST_THREAD_USE_DLL before including BoostLog.h
 */

#pragma once

#ifndef BoostLog_H_
#define BoostLog_H_

#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/thread.hpp>

#include "SyncQueue.h"


enum LogLevel {
   LOG_TRACE = 0,
   LOG_INFO,
   LOG_WARNING,
   LOG_ERROR,
   LOG_FATAL,
   LOG_ALWAYS,  // special level to force output to the log
};


const char LOG_FIELD_SEPARATOR[] = ",";


LogLevel stringToEnum(std::string levelStr);

class CBoostLog {
public: 

   // Write a log message to the static logger instance
   // Defaults to an INFO-level message.
   static void log(/* LogLevel msgLevel = INFO, */ const std::string& msg);

   // Write a log message to the static logger instance
   static void log(LogLevel msgLevel, const std::string& msg);

   // Dump a data buffer to the static logger instance
   // Defaults to a TRACE-level message.
   static void logDump(/* LogLevel msgLevel = TRACE, */ const std::string& prefix, 
                       const std::vector<uint8_t>& data);

   static void logDump(LogLevel msgLevel, const std::string& prefix, 
                       const std::vector<uint8_t>& data, int startIndex = 0, int length = -1);


   static CBoostLog& getInstance();


   // Constructor takes no parameters to make handling the static instance easier
   CBoostLog();
   // Destructor stops the logging thread and closes the log file
   ~CBoostLog();

   // openLog() should be called to initialize the logger before writing any
   // log messages. openLog causes the log files to be rotated, opened and
   // the logging thread started. 
   void openLog(const std::string& logfile, int numBackups, int maxLogSize,
                LogLevel outLevel = LOG_INFO);

   // The start() and stop() method allow control of the logging thread.
   // Generally, you shouldn't need to call these methods unless you need
   // to interactively start and stop log output.
   
   void start();

   void stop();

   // Write a message with the specified log level
   void logMsg(LogLevel msgLevel, const std::string& msg);

private:
   // Returns: whether the rotate operation succeeded
   bool rotateLogs();
   void openFile(std::ios_base::openmode = std::ios_base::out);

   void logFromQueue(); // log thread

   void writeMsg(const std::string& msg);


   int            m_logLevel;
   int            m_numBackups;
   int            m_maxLogSize;
   std::string    m_filename;
   std::ofstream* m_log;
   boost::mutex   m_lock;

   bool           m_isRunning;
   boost::thread* m_logger;
   CSyncQueue<std::string> m_msgQueue;
};


#endif /* ! BoostLog_H_ */
