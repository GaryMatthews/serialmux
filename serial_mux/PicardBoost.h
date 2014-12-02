/*
 * Copyright (c) 2011, Dust Networks, Inc.
 */

#ifndef PicardBoost_H_
#define PicardBoost_H_

#pragma once


#include "BasePicard.h"

#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>


namespace DustSerialMux {

   // The output class 
   class CPicardBoost_Serial : public CBasePicardIO {
   public:
      CPicardBoost_Serial(boost::asio::io_service& io_service, const std::string& port,
                          int rtsDelay, bool hwFlowControl, int readTimeout);

      virtual ~CPicardBoost_Serial();

   protected:
      virtual void sendRaw(const ByteVector& data);
      
      virtual void read(const std::string& context, int timeout);

   private:
      boost::asio::io_service& m_io_service;
      
      // serial port options
      int m_rtsDelay; // millisecond delay before deasserting RTS
      bool m_hwFlowControl;
      int m_readTimeout; // millisecond timeout for read operations
      
      // serial port used for reading from Picard
      boost::asio::serial_port m_serial;
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
