/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#include "PicardBoost.h"

#include "BoostLog.h"
#include "SerialMuxOptions.h"

#include "serial_mux.h"  // for resetConnection

#include <boost/date_time/posix_time/posix_time.hpp>

#ifndef WIN32
#include <termios.h>
#include <unistd.h>
#endif

using namespace boost::posix_time;

using namespace boost::asio;
using boost::asio::ip::udp;

namespace DustSerialMux {

   CPicardBoost_Serial::CPicardBoost_Serial(boost::asio::io_service& io_service, const std::string& port,
                                            int rtsDelay, bool hwFlowControl, int readTimeout)
      : m_io_service(io_service),
        m_rtsDelay(rtsDelay),
        m_hwFlowControl(hwFlowControl),
        m_readTimeout(readTimeout),
        m_serial(io_service, port),
        m_readLock(),
        m_readSem(),
        m_input(INPUT_BUFFER_LEN)
   {
      boost::system::error_code err;
      
      // set parameters
      m_serial.set_option(serial_port_base::baud_rate(DEFAULT_BAUD_RATE), err);
      m_serial.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none), err);
      m_serial.set_option(serial_port_base::character_size(8), err);
      m_serial.set_option(serial_port_base::parity(serial_port_base::parity::none), err);
      m_serial.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one), err);
#ifdef WIN32
      serial_port::native_type handle = m_serial.native();
      int result;
      result = EscapeCommFunction(handle, SETDTR);
      result = EscapeCommFunction(handle, CLRRTS);

      // TODO: instead of timeouts, rework read loop to use async reads
      COMMTIMEOUTS timeouts;
      GetCommTimeouts(handle, &timeouts);

      timeouts.ReadTotalTimeoutMultiplier = 0;
      timeouts.ReadTotalTimeoutConstant = readTimeout;
      SetCommTimeouts(handle, &timeouts);

      DCB dcb = {0};
      result = GetCommState(handle, &dcb);
 #endif
   }

   CPicardBoost_Serial::~CPicardBoost_Serial() { 
      m_serial.cancel();
   }

   void CPicardBoost_Serial::sendRaw(const ByteVector& data)
   {
      // locks should be handled at the sendCommand / sendAck methods

      // HDLC encode
      ByteVector encodedvec = encodeHDLC(data);

      // the current implementation does not support hardware flow control
#if 0
      if (m_hwFlowControl) {
         m_serial->RtsEnable = 1;
         // busy wait for DSR (or timeout)
         bool sendTimeout = false;
         ptime timeout = microsec_clock::local_time() + milliseconds(m_serial->WriteTimeout);
         while (m_serial->DsrHolding == 0) {
            ptime now = microsec_clock::local_time();
            if (now > timeout) {
               sendTimeout = true;
               break;
            }
            boost::this_thread::sleep(milliseconds(1)); 
         }
         if (sendTimeout) {
            // note: we detect the write failure when Acks are not received
            CBoostLog::log("Serial:Write error: no CTS received");
            return;
         }
      }
#endif
      
      try {
         size_t len = boost::asio::write(m_serial, boost::asio::buffer(encodedvec));
      }
      catch (const std::exception&) {
         CBoostLog::log("exception (Serial write)");
      }
      
      CBoostLog::logDump("Serial:Write", encodedvec);
      
      // the current implementation does not support hardware flow control
#if 0
      // wait before deasserting RTS
      // TODO: wait on EV_TXEMPTY
      // TODO: later, we may need to adjust the RTS delay based on data length
      if (m_hwFlowControl) {
         if (m_rtsDelay > 0) {
            boost::this_thread::sleep(milliseconds(m_rtsDelay));
         }
         m_serial->RtsEnable = 0;
      }
#endif
   }

   void CPicardBoost_Serial::handleRead(const boost::system::error_code& result,
                                        std::size_t bytes)
   {
      if (result) {
         // need to notify the semaphore - unless the async read was cancelled by the timeout
         if (result != boost::asio::error::operation_aborted) {
             m_readSem.notify_one();
         }
         return;
      }
      std::ostringstream msg;
      msg << "handleRead complete: len=" << bytes;
      CBoostLog::log(LOG_TRACE, msg.str());

      if (bytes > 0) { 
         CBoostLog::logDump("Serial Read data", ByteVector(m_input.begin(), m_input.begin() + bytes));
      }

      for (size_t i = 0; i < bytes; i++) {
          m_hdlc->addByte(m_input[i]);
      } // the HDLC parser calls frameComplete

      m_readLen = bytes;
      // set the read semaphore
      m_readSem.notify_one();
   }

   void CPicardBoost_Serial::read_async(const std::string& context, int timeout)
   {
      CBoostLog::log(LOG_TRACE, "Starting read(), async");
      bool portClosed = false;

      try {
         m_readLen = 0;
         
         m_serial.async_read_some(boost::asio::buffer(m_input),
                                  boost::bind(&CPicardBoost_Serial::handleRead, this,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
         {
            boost::unique_lock<boost::mutex> guard(m_readLock);
            boost::system_time const wtimeout=boost::get_system_time()+ boost::posix_time::milliseconds(timeout);
            m_readSem.timed_wait(guard, wtimeout);
            // the timeout should cancel the async read so we don't have multiple outstanding reads
            m_serial.cancel();
         }
         std::ostringstream msg;
         msg << "async read() complete: len=" << m_readLen;
         CBoostLog::log(LOG_TRACE, msg.str());
      }
      catch (const std::exception&) {
         CBoostLog::log("exception (Serial read)");
         portClosed = true;
      }

      if (portClosed) {
         // when the port is closed, we reset and hope it re-opens soon
         resetConnection();
      }      
   }

   // read 
   void CPicardBoost_Serial::read(const std::string& context, int timeout)
   {
      read_async(context, timeout);
   }   

   
   CPicardBoost_UDP::CPicardBoost_UDP(boost::asio::io_service& io_service, uint16_t port, int readTimeout)
      : m_io_service(io_service),
        m_endpoint(udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port)),
        m_socket(io_service, udp::v4()),
        m_readTimeout(readTimeout)
   {
      struct timeval recvtimeout = { m_readTimeout / 1000,
                                     (m_readTimeout % 1000) * 1000 };
      
      // not pretty, but easier than rewriting to use async methods
      // if/when we convert to async, we should use a deadline timer
      setsockopt(m_socket.native(), SOL_SOCKET, SO_RCVTIMEO,
                 (const char*)&recvtimeout, sizeof(struct timeval));
   }

   CPicardBoost_UDP::~CPicardBoost_UDP() { 
      // TODO: cleanup
   }

   void CPicardBoost_UDP::sendRaw(const ByteVector& data)
   {
      // insert a dummy byte in the front
      ByteVector withDummy(data.size() + 1);
      withDummy[0] = 0;
      copy(data.begin(), data.end(), withDummy.begin()+1);
      
      try {
         m_socket.send_to(boost::asio::buffer(withDummy), m_endpoint);
      }
      catch (const std::exception&) {
         CBoostLog::log("exception (UDP write)");
      }
      
      CBoostLog::logDump("UDP:Write (first byte excluded)", data);
   }

   // read 
   void CPicardBoost_UDP::read(const std::string& context, int timeout)
   {
      CBoostLog::log(LOG_TRACE, "Starting read()");

      try {
         ByteVector data(INPUT_BUFFER_LEN);
         
         size_t len = m_socket.receive_from(boost::asio::buffer(data, INPUT_BUFFER_LEN),
                                            m_endpoint);
         // limit input to actual len and strip the first byte
         ByteVector input(data.begin()+1, data.begin()+len);
         
         std::ostringstream prefix;
         prefix << "UDP:Read (" << context << ")";
         CBoostLog::logDump(prefix.str(), input);
         
         frameComplete(input);
      }
      catch (const std::exception& ex) {
         std::ostringstream msg;
         msg << "exception (UDP read) " << ex.what();
         CBoostLog::log(msg.str());
      }
   }

   
} // namespace DustSerialMux
