// log_tests.cpp : Logging test cases
//

#include <vector>
#include <iostream>

#include "BoostLog.h"

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/algorithm/string.hpp>


const char LOG_BASE[] = "boost_test.log";

struct LoggingFixture {

   static void clearLogs(const std::string& baseName) {
      boost::filesystem::remove(baseName);
      for (int i = 0; i < 5; ++i) {
         std::ostringstream f;
         f << baseName << "." << i; 
         boost::filesystem::remove(f.str());
      }
   }

   LoggingFixture() 
   {
      clearLogs(LOG_BASE);
      logger.openLog(LOG_BASE, 3, 5000);
   }

   ~LoggingFixture() {
      logger.stop();
   }

   CBoostLog logger;
};


BOOST_AUTO_TEST_CASE(logInit)
{
   // remove all previous logs
   LoggingFixture::clearLogs(LOG_BASE);

   // create a log file
   {
      CBoostLog logger;
      logger.openLog(LOG_BASE, 3, 5000);
   }
   BOOST_CHECK_EQUAL(boost::filesystem::exists(LOG_BASE), true);
   BOOST_CHECK_EQUAL(boost::filesystem::exists("boost_test.log.1"), false);

   // create another file, check for startup log rotation
   {
      CBoostLog logger;
      logger.openLog(LOG_BASE, 3, 5000);
   }
   BOOST_CHECK_EQUAL(boost::filesystem::exists(LOG_BASE), true);
   BOOST_CHECK_EQUAL(boost::filesystem::exists("boost_test.log.1"), true);
}

BOOST_AUTO_TEST_CASE(rotateLogs)
{
   // remove all previous logs
   LoggingFixture::clearLogs(LOG_BASE);

   const std::string ROTATED_LOG = std::string(LOG_BASE) + ".1";

   // create a log file
   CBoostLog logger;
   logger.openLog(LOG_BASE, 3, 100);
   
   BOOST_CHECK_EQUAL(boost::filesystem::exists(LOG_BASE), true);
   BOOST_CHECK_EQUAL(boost::filesystem::exists(ROTATED_LOG), false);
   
   logger.logMsg(LOG_INFO, "this is a log message. lalalalalalalalalalala");
   logger.logMsg(LOG_INFO, "this is a second log message. lalalalalalalalalalala");
   logger.logMsg(LOG_INFO, "this is a third log message. lalalalalalalalalalala");
   logger.logMsg(LOG_INFO, "this is a fourth log message. lalalalalalalalalalala");

   logger.stop();

   // verify the log files were rotated because of max size
   BOOST_CHECK_EQUAL(boost::filesystem::exists(ROTATED_LOG), true);

}


std::string lastLine(const std::string& path)
{
   std::ifstream input(path.c_str());
   char line[256];
   std::string result;
   while (!input.eof()) {
      input.getline(line, 256);
      if (strlen(line) > 0) {
         result = line;
      }
   }
   return result;
}

BOOST_AUTO_TEST_CASE(logLevels)
{
   // remove all previous logs
   LoggingFixture::clearLogs(LOG_BASE);

   // create a log file
   CBoostLog logger;
   logger.openLog(LOG_BASE, 3, 5000, LOG_WARNING);
   
   logger.logMsg(LOG_ERROR, "this is an ERROR message.");
   logger.logMsg(LOG_WARNING, "this is a WARNING message.");
   logger.logMsg(LOG_INFO, "this is an INFO message.");
   logger.logMsg(LOG_TRACE, "this is a TRACE message.");

   logger.stop();

   // verify WARNING and ERROR messages are present
   {   
      std::string lastMsg = lastLine(LOG_BASE);
      BOOST_CHECK(lastMsg.find("this is a WARNING message") > 0);
   }
}



BOOST_FIXTURE_TEST_SUITE(Logger, LoggingFixture);


BOOST_AUTO_TEST_CASE(logMessages)
{
   // requires Fixture setup

   logger.logMsg(LOG_INFO, "this is a log message");
   // stop the logger to verify the output
   logger.stop();

   {   
      std::string lastMsg = lastLine(LOG_BASE);
      std::vector<std::string> strs;
      boost::split(strs, lastMsg, boost::is_any_of(","));
      BOOST_CHECK_EQUAL(strs.size(), 2);
      if (strs.size() > 1) {
         BOOST_CHECK_EQUAL(strs[1], "this is a log message");
      }
   }

#if 0
   // (re)start the logger with TRACE output
   logger.openLog(LOG_BASE, 3, 5000, LOG_TRACE);

   std::vector<uint8_t> data;
   data.push_back(1);
   data.push_back(2);
   data.push_back(4);
   data.push_back(8);
   data.push_back(15);
   logger.logDump("Some data", data);

   // stop the logger to verify the output
   logger.stop();

   {   
      std::string lastMsg = lastLine(LOG_BASE);
      BOOST_CHECK_EQUAL(lastMsg, std::string("1 2 4 8 f "));  // trailing space
   }
#endif
}


BOOST_AUTO_TEST_CASE(multipleLoggers)
{
   // requires Fixture setup

   CBoostLog secondlog;
   secondlog.openLog("boost_second.log", 3, 5000, LOG_INFO);
   
   logger.logMsg(LOG_INFO, "this is a log message");

   secondlog.logMsg(LOG_INFO, "this message goes to the second log");
   
   logger.logMsg(LOG_INFO, "this is a log message");

   secondlog.logMsg(LOG_INFO, "this message goes to the second log");

   
   // stop the loggers to verify the output
   logger.stop();
   secondlog.stop();
   
   {   
      std::string lastMsg = lastLine(LOG_BASE);
      std::vector<std::string> strs;
      boost::split(strs, lastMsg, boost::is_any_of(","));
      BOOST_CHECK_EQUAL(strs.size(), 2);
      if (strs.size() > 1) {
         BOOST_CHECK_EQUAL(strs[1], "this is a log message");
      }
   }

   {   
      std::string lastMsg = lastLine("boost_second.log");
      std::vector<std::string> strs;
      boost::split(strs, lastMsg, boost::is_any_of(","));
      BOOST_CHECK_EQUAL(strs.size(), 2);
      if (strs.size() > 1) {
         BOOST_CHECK_EQUAL(strs[1], "this message goes to the second log");
      }
   }
}



BOOST_AUTO_TEST_SUITE_END();
