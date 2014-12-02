/*
 * Copyright (c) 2010, Dust Networks, Inc. All rights reserved.
 */

#include "BoostClient.h"

#include "BoostLog.h"

namespace DustSerialMux 
{
   const int AUTH_TIMEOUT = 2; // seconds

   // * interface required by ClientManager

   CBoostClient::~CBoostClient()
   {
      std::ostringstream msg;
      msg << "closing client";
      if (!m_name.empty()) { msg << " " << m_name; }
      CBoostLog::log(msg.str());
   }

   void CBoostClient::close()
   {
      m_authTimeout.cancel(); // just in case
      if (m_socket.is_open()) {
         m_socket.shutdown(tcp::socket::shutdown_both);
         m_socket.close();
      }
      // update the state to indicated the socket has been closed
      m_initState = CLOSED;
   }
   
   
   std::string CBoostClient::remoteName()
   {
      if (m_name.empty()) {
         std::ostringstream name;
         name << m_socket.remote_endpoint().address() << ":"
              << m_socket.remote_endpoint().port();
         m_name = name.str();
      }
      return m_name;
   }
   
   boost::system::error_code CBoostClient::write(const ByteVector& msg) 
   {
      {
         std::ostringstream logmsg;
         logmsg << "write to " << remoteName() << ": ";
         CBoostLog::logDump(logmsg.str(), msg);
      }
      boost::system::error_code err;
      boost::asio::write(m_socket, boost::asio::buffer(msg),
                         boost::asio::transfer_all(), err);
      return err; 
   }

   // * async I/O handlers
   
   void CBoostClient::handleCommand(const CMuxMessage& command)
   {
      if (isInitialized()) {
         CMuxOutput respcmd(MUX_INFO, 0, ERR_INVALID_CMD, ByteVector());
         ByteVector resp = respcmd.serialize();

         // once initialized, all commands should be processed through the
         // Client Manager queue so that order is maintained. 
         
         // add the command to the client manager's queue
         m_clientMgr.addCommand(shared_from_this(), command);
      }
      else if (m_initState == WAITING && command.type() == MUX_HELLO) {
         // TODO: update parseHello to accept MuxMessage
         int helloResult = parseHello(command.m_data, command.size());
         ByteVector resp = buildHelloResponse(helloResult);

         // no need to do an async write, this is tiny
         boost::system::error_code err = write(resp);

         // handle the post-write Hello actions

         // cancel the auth timeout
         m_authTimeout.cancel();
         
         if (helloResult == OK) {
            m_initState = AUTHENTICATED;
            // register the client with the ClientManager
            m_clientMgr.addClient(shared_from_this());
         } else {
            // close the connection if the Hello response is an error
            m_initState = BAD_INIT;
            m_socket.close();
            CBoostLog::log("client hello error. closing connection");
            // note: the client isn't added yet, so no need to remove it
         }
      }
      // until the client sends a Hello message, input is discarded
   }
   
   void CBoostClient::handle_read(const boost::system::error_code& error,
                                  size_t len)
   {
      if (!error && len > 0) {
         // limit input buffer to len bytes
         ByteVector data(m_input.begin(), m_input.begin()+len);

         {
            std::ostringstream msg;
            msg << "read from " << remoteName() << ": ";
            CBoostLog::logDump(msg.str(), data);
         }

         m_parser.read(data);

         if (!badInit())
            asyncRead();
      }

      // on error, remove the client
      if (error) {
         if (m_initState == AUTHENTICATED) {
            m_clientMgr.removeClient(shared_from_this());
         }
         {
            std::ostringstream msg;
            // don't use remoteName() to avoid errors on shutdown
            msg << "client " << m_name << " read error: " << error;
            CBoostLog::log(msg.str());
         }
      }
   }

   void CBoostClient::handleAuthTimeout(const boost::system::error_code& error)
   {
      if (!error) {
         {
            std::ostringstream msg;
            msg << "client authentication timeout from " << remoteName();
            CBoostLog::log(msg.str());
         }

         m_initState = BAD_INIT;
         close();
         // note: the client isn't added yet, so no need to remove it      
      }
   }
   
   void CBoostClient::start()
   {
      {
         std::ostringstream msg;
         msg << "client connection from " << remoteName();
         CBoostLog::log(msg.str());
      }

      // first time: start an auth timeout timer
      m_authTimeout.expires_from_now(boost::posix_time::seconds(AUTH_TIMEOUT));
      m_authTimeout.async_wait(boost::bind(&CBoostClient::handleAuthTimeout, this, 
                                           boost::asio::placeholders::error));
      
      asyncRead();
   }
   
   
   void CBoostClient::asyncRead() 
   {
      m_socket.async_read_some(boost::asio::buffer(m_input),
                               boost::bind(&CBoostClient::handle_read, shared_from_this(),
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
   }
   

   int CBoostClient::parseHello(const ByteVector& data, int length)
   {
      int result = OK;
      int index = 0;
      
      // check type
      if (length == EXPECTED_HELLO_LEN) {
         
         // check version
         uint8_t protoVersion = data[index++];
         if (protoVersion != m_protocolVersion) {
            result = ERR_UNSUPPORTED_VERSION;
         }
         
         // check authentication
         for (int i = 0; i < AUTHENTICATION_LEN; i++) {
            if (m_expectedAuth[i] != data[index + i]) {
               result = ERR_INVALID_AUTH;
            }
         }
         
      } else {
         result = ERR_INVALID_CMD;
      }
      return result;
   }
   
   ByteVector CBoostClient::buildHelloResponse(int result) 
   {
      ByteVector data(1); 
      data[0] = m_protocolVersion;
      CMuxOutput resp(MUX_HELLO, 0, result, data);
      return resp.serialize();
   }
   
} // namespace

