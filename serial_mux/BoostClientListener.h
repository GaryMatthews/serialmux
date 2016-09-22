/*
 * Copyright (c) 2010, Dust Networks, Inc. All rights reserved.
 */

#ifndef BoostClientListener_H_
#define BoostClientListener_H_

#pragma once


#include <stdint.h>
#include <string>

#include "Common.h"

#include "BoostClient.h"
#include "MuxMessageParser.h"
#include "SerialMuxOptions.h"  // for AUTHENTICATION_LEN

#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;


namespace DustSerialMux 
{
   
   class CBoostClientListener 
   {
   public:
      CBoostClientListener(boost::asio::io_service& io_svc, uint16_t port, bool useLocalhost,
                           ISimpleClientList& clients, const uint8_t* authToken, uint8_t protocolVersion);

      virtual ~CBoostClientListener();

      void asyncListen();

      void stop();

      uint16_t getPort() const { return m_listenerPort; }

      void set_protocolVersion(uint8_t version) {m_protocolVersion = version;}

   private:
      void handleAccept(CBoostClient::pointer new_connection,
                        const boost::system::error_code& error);

      uint16_t              m_listenerPort;
      bool                  m_isListening;

      ISimpleClientList&    m_clients;
      unsigned char         m_expectedAuth[AUTHENTICATION_LEN];
      uint8_t               m_protocolVersion;
      
      boost::asio::io_service& m_io_service;
      tcp::endpoint         m_listenerEndpoint;
      tcp::acceptor*        m_acceptor;
   };

} // namespace DustSerialMux

#endif  /* ! BoostClientListener_H_ */
