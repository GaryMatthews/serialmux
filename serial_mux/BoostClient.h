/*
 * Copyright (c) 2010, Dust Networks, Inc. All rights reserved.
 */

#ifndef BoostClient_H_
#define BoostClient_H_

#pragma once


#include <string>

#include "Common.h"

#include "MuxMessageParser.h"
#include "SerialMuxOptions.h"  // for AUTHENTICATION_LEN

#include "Subscriber.h"

#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;


namespace DustSerialMux 
{
   const int CLIENT_AUTH_READ_TIMEOUT = 1000; // in milliseconds

   const int EXPECTED_HELLO_LEN = 9;

   
   // TODO BoostClientManager requires:
   // isConnected() - called before write(), could be eliminated
   // ISubscription
   // id string (host:port) for logging
   // write()
   

   class ISimpleClientList;

   
   class CBoostClient : public boost::enable_shared_from_this<CBoostClient>,
                        public ICommandCallback, public CSubscriber
   {
   public:
      typedef boost::shared_ptr<CBoostClient> pointer;
      
      static pointer create(boost::asio::io_service& io_service, 
                            ISimpleClientList& clientMgr,
                            const uint8_t* authToken,
                            uint8_t protocolVersion)
      {
         return pointer(new CBoostClient(io_service, clientMgr,
                                         authToken, protocolVersion));
      }

      virtual ~CBoostClient();

      void close();
      
      // used by acceptor in ClientListener
      tcp::socket& socket()
      {
         return m_socket;
      }
      
      // * interface required by ClientManager

      std::string remoteName();

      boost::system::error_code write(const ByteVector& msg);

      bool isInitialized() const 
      {
         return m_initState == AUTHENTICATED;
      }

      virtual void handleCommand(const CMuxMessage& command);

      void handle_read(const boost::system::error_code& error,
                       size_t len);

      void handleAuthTimeout(const boost::system::error_code& error);
      
      void start();

      uint8_t getProtocolVersion() const { return m_protocolVersion; }
      
   private:
      enum InitState {
         WAITING,
         AUTHENTICATED,
         BAD_INIT,
         CLOSED
      };
      
      CBoostClient(boost::asio::io_service& io_service,
                   ISimpleClientList& clientMgr,
                   const uint8_t* authToken,
                   uint8_t protocolVersion)
       : m_socket(io_service),
         m_initState(WAITING),
         m_parser((ICommandCallback*)this),
         m_clientMgr(clientMgr),
         m_expectedAuth(authToken),
         m_protocolVersion(protocolVersion),
         m_input(256),
         m_authTimeout(io_service)
      { ; }

      bool badInit() const {
         return m_initState == BAD_INIT;
      }

      void asyncRead();      

      int parseHello(const ByteVector& data, int length);
      ByteVector buildHelloResponse(int result);
      
      tcp::socket m_socket;
      InitState   m_initState;
      CMuxParser  m_parser;
      ByteVector  m_input;
 
      ISimpleClientList& m_clientMgr;
      const uint8_t *    m_expectedAuth;
      uint8_t            m_protocolVersion;
      
      std::string m_name;
      boost::asio::deadline_timer m_authTimeout;
   };

   
   // The ClientListener only needs a way to add new clients to the client manager 
   class ISimpleClientList {
   public:
      virtual void addClient(CBoostClient::pointer client) = 0;

      virtual void removeClient(CBoostClient::pointer client) = 0;

      virtual void addCommand(CBoostClient::pointer client,
                              const CMuxMessage& cmd) = 0;
   };

   
} // namespace

#endif  /* ! BoostClient_H_ */
