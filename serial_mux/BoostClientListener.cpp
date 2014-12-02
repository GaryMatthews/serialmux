/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#include "BoostClientListener.h"

#include "BoostLog.h"


namespace DustSerialMux 
{

   CBoostClientListener::CBoostClientListener(boost::asio::io_service& io_svc,
                                              uint16_t port, bool useLocalhost,
                                              ISimpleClientList& clients,
                                              const uint8_t* authToken,
                                              uint8_t protocolVersion)
      : m_listenerPort(port), 
        m_isListening(false),
        m_clients(clients),
        m_protocolVersion(protocolVersion),
        m_io_service(io_svc)
   {
      if (useLocalhost) {
         m_listenerEndpoint = tcp::endpoint(boost::asio::ip::address_v4::loopback(), m_listenerPort);
      }
      else {
         m_listenerEndpoint = tcp::endpoint(tcp::v4(), m_listenerPort);
      }
      m_acceptor = new tcp::acceptor(m_io_service, m_listenerEndpoint);

      // the expected auth array is fixed size (pre-allocated)
      std::copy(authToken, authToken + AUTHENTICATION_LEN, 
                m_expectedAuth);
   }

   CBoostClientListener::~CBoostClientListener(void)
   {
      delete m_acceptor;
   }

   void CBoostClientListener::stop() 
   {
      m_isListening = false;
      if (m_acceptor) {
         boost::system::error_code err;
         m_acceptor->close(err);
         if (err) {
            std::ostringstream msg;
            msg << "Error while stopping listener: " << err;
            CBoostLog::log(msg.str());
         }
      }
   }
   
   // boost asio methods
   
   void CBoostClientListener::handleAccept(CBoostClient::pointer new_connection,
                                           const boost::system::error_code& error)
   {
      if (!error)
      {
         new_connection->start();
         asyncListen();
      }
   }
   
   void CBoostClientListener::asyncListen()
   {
      CBoostLog::log("listening for connections");

      CBoostClient::pointer new_connection =
         CBoostClient::create(m_acceptor->get_io_service(), m_clients,
                              m_expectedAuth, m_protocolVersion);

      m_acceptor->async_accept(new_connection->socket(),
                               boost::bind(&CBoostClientListener::handleAccept, this,
                                           new_connection,
                                           boost::asio::placeholders::error));
   }
   
} // namespace DustSerialMux
