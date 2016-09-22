/*
 * Copyright (c) 2011, Dust Networks, Inc.
 */

#ifndef PicardBoost_H_
#define PicardBoost_H_

#pragma once


#include "BasePicard.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>


namespace DustSerialMux {

   // The output class 
   class CPicardBoost_Serial : public CBasePicardIO {
   public:
      CPicardBoost_Serial(boost::asio::io_service& io_service, const std::string& port,
                          int rtsDelay, bool hwFlowControl, int readTimeout);

      virtual ~CPicardBoost_Serial();

      // Callbacks 
      void handleRead(const boost::system::error_code& result, std::size_t bytes);
      void handleReadTimeout(const boost::system::error_code& result);

   protected:
      virtual void sendRaw(const ByteVector& data);
      
      virtual void read(const std::string& context, int timeout);

   private:
      void read_async(const std::string& context, int timeout);

      boost::asio::io_service& m_io_service;
      
      // serial port options
      int m_rtsDelay; // millisecond delay before deasserting RTS
      bool m_hwFlowControl;
      int m_readTimeout; // millisecond timeout for read operations
      size_t m_readLen;  // bytes read
      
      // serial port used for reading from Picard
      boost::asio::serial_port m_serial;
      boost::mutex m_readLock;
      boost::condition_variable m_readSem;
      bool m_worker_is_done;
   };

   class CPicardBoost_UDP : public CBasePicardIO {
   public:
      CPicardBoost_UDP(boost::asio::io_service& io_service, uint16_t port, int readTimeout);

      virtual ~CPicardBoost_UDP();

   protected:
      virtual void sendRaw(const ByteVector& data);
      
      virtual void read(const std::string& context, int timeout);

   private:
      boost::asio::io_service& m_io_service;
      boost::asio::ip::udp::endpoint m_endpoint;
      boost::asio::ip::udp::socket m_socket;
      
      int m_readTimeout; // millisecond timeout for read operations
      
   };
   
} // namespace DustSerialMux

#endif  /* ! PicardBoost_H_ */
