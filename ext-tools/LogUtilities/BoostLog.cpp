/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#include "BoostLog.h"

#include <iomanip>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


const int LOG_TIMEOUT = 1;

// note: we intentionally pass the string parameter by value so we can convert it to lowercase
LogLevel stringToEnum(std::string levelStr)
{
   LogLevel result = LOG_ERROR;
   // convert to lowercase for string comparisons
   boost::algorithm::to_lower(levelStr);
   if (levelStr == "trace") {
      result = LOG_TRACE;
   } else if (levelStr == "info") {
      result = LOG_INFO;
   } else if (levelStr == "warning") {
      result = LOG_WARNING;
   } 
   return result;
}

void CBoostLog::log(const std::string& msg) 
{ 
   CBoostLog::log(LOG_INFO, msg); 
}

void CBoostLog::log(LogLevel msgLevel, const std::string& msg) 
{ 
   getInstance().logMsg(msgLevel, msg); 
}

void CBoostLog::logDump(const std::string& prefix, const std::vector<uint8_t>& data) 
{
   logDump(LOG_TRACE, prefix, data);
}

void CBoostLog::logDump(LogLevel msgLevel, const std::string& prefix, 
                        const std::vector<uint8_t>& data, int startIndex, int length) 
{
   std::ostringstream output;
   if (length == -1) { length = data.size() - startIndex; }

   output << prefix << " [len=" << std::dec << length << "]:\n";
   for (size_t i = startIndex; i < length; i++) {
      output << std::hex << (int)data[i] << " ";
   }
   getInstance().logMsg(msgLevel, output.str());
}


CBoostLog& CBoostLog::getInstance() 
{
   static CBoostLog me;
   return me;
}



CBoostLog::CBoostLog()
   : m_logLevel(-1), 
     m_numBackups(1), 
     m_maxLogSize(1000000), 
     m_filename(), 
     m_log(NULL), 
     m_isRunning(false),
     m_logger(NULL)
{
   ;
}
CBoostLog::~CBoostLog() { stop(); delete m_log; }

void CBoostLog::openLog(const std::string& logfile, int numBackups,
                        int maxLogSize, LogLevel outLevel)
{
   {
      boost::mutex::scoped_lock guard(m_lock);
      delete m_log; m_log = NULL; // if there's an active log, close it

      m_logLevel = outLevel;
      m_filename = logfile;
      m_maxLogSize = maxLogSize;
      m_numBackups = numBackups;

      rotateLogs();

      // Open the log file (for appending if it exists)
      openFile(std::ios_base::app);
   }
   start();
}

void CBoostLog::start()
{
   if (!m_isRunning) {
      m_isRunning = true;
      m_logger = new boost::thread(&CBoostLog::logFromQueue, this);
   }
}

void CBoostLog::stop()
{
   m_isRunning = false;
   if (m_logger) {
      m_logger->join();
      delete m_logger;
      m_logger = NULL;
   }
}

void CBoostLog::logFromQueue()
{
   while (m_isRunning) {
      std::string msg;
      bool hasMsg = m_msgQueue.timedPop(msg, LOG_TIMEOUT);
      if (hasMsg) {
         writeMsg(msg);
      }
   }
   // make sure the queue is empty before quitting
   while (!m_msgQueue.empty()) {
      std::string msg;
      m_msgQueue.timedPop(msg, LOG_TIMEOUT);
      writeMsg(msg);
   }
}

void CBoostLog::logMsg(LogLevel msgLevel, const std::string& msg)
{
   boost::mutex::scoped_lock guard(m_lock);
   boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();

   // filter by severity
   if (msgLevel >= m_logLevel || msgLevel == LOG_ALWAYS) {
      std::ostringstream logmsg;
      logmsg << now << LOG_FIELD_SEPARATOR << msg;
      m_msgQueue.push(logmsg.str());
   }
}

void CBoostLog::writeMsg(const std::string& msg)
{
   bool rotateOk = true;

   // rotate the logs if the log file gets too big
   if (m_maxLogSize > 0 && 
      boost::filesystem::file_size(m_filename) > m_maxLogSize) {
         delete m_log; m_log = NULL;
         rotateOk = rotateLogs();
   }

   // note: always create a new file when we rotate for size
   if (!m_log) { 
      openFile(); 
   }

   if (m_log) {
      if (!rotateOk) {
         (*m_log) << "Log file could not be rotated, truncated instead." << std::endl;
      }
      (*m_log) << msg << std::endl;
   }
}

void CBoostLog::openFile(std::ios_base::openmode mode)
{
   try {
      // Open the log file
      m_log = new std::ofstream(m_filename.c_str(), mode);
   }
   catch (const std::exception&) {
      // not much we can do here, except try again later
   }
}

// note: if numBackups is 0, there's no rotation and we should append to
// an existing log file
bool CBoostLog::rotateLogs()
{
   bool result = true;
   try {
      for (int i = m_numBackups; i > 0; --i) {
         std::ostringstream backup_file;
         backup_file << m_filename << "." << i;
         std::string backup = backup_file.str();

         std::ostringstream prev_file;
         prev_file << m_filename;
         if (i-1 > 0) {
            prev_file << "." << i-1;
         }
         std::string previous = prev_file.str();

         // Ensure that the backup log file does not exist.
         if (boost::filesystem::exists(backup))
            boost::filesystem::remove(backup);

         // Move aside the existing log file.
         if (boost::filesystem::exists(previous))
            boost::filesystem::rename(previous, backup);
      }
   }
   catch (const boost::filesystem::filesystem_error&) {
      result = false;
   }
   return result;
}

