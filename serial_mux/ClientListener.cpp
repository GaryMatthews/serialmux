/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#include "StdAfx.h"
#include "ClientListener.h"
#include "BoostLog.h"

#include "CLRUtils.h"
using namespace System::Threading;

namespace DustSerialMux 
{
   const int MAX_AUTH_INPUT_LEN = 256;
   const int CLIENT_AUTH_READ_TIMEOUT = 1000; // in milliseconds

   const int EXPECTED_HELLO_LEN = 18;

   CClientListener::CClientListener(uint16_t port, ISimpleClientList& clients, const uint8_t* authToken)
      : m_listenerPort(port), 
        m_listenerAddr("127.0.0.1"),
        m_isListening(false),
        m_listener(nullptr),
        m_clients(clients)
   {
      // the expected auth array is fixed size (pre-allocated)
      std::copy(authToken, authToken + AUTHENTICATION_LEN, 
                m_expectedAuth);
   }

   CClientListener::~CClientListener(void)
   {
      stop();
   }


   void CClientListener::threadMain(void)
   {
      // start listening
      String^ addrStr = gcnew String(m_listenerAddr.c_str());
      IPAddress^ localAddr = IPAddress::Parse(addrStr);

      // retry until listener can be started
      while (!m_isListening) {
         try {
            m_listener = gcnew TcpListener(localAddr, m_listenerPort);
            m_listener->Start();

            m_isListening = true;
         } 
         catch (SocketException^) {
            std::ostringstream msg;
            msg << "Error: could not open listener on port " << m_listenerPort;
            CBoostLog::log(msg.str());
            Thread::Sleep(1000); // wait before retrying
         }
         // TODO: timeout if there are too many retries
      }
      std::ostringstream msg;
      msg << "Listener started on port " << m_listenerPort;
      CBoostLog::log(msg.str());

      while (m_isListening) {
         Socket^ clientSock = nullptr;
         try {
            clientSock = m_listener->AcceptSocket();
            {
               IPEndPoint^ remote = safe_cast<IPEndPoint^>(clientSock->RemoteEndPoint);
               String^ remoteName = String::Format("{0}:{1}",
                  remote->Address->ToString(), remote->Port.ToString());
               
               std::ostringstream msg;
               msg << "Listener accepting client from " << convertToStdString(remoteName);
               CBoostLog::log(msg.str());
            }
         }
         catch (SocketException^) {
            CBoostLog::log("Listener reset");
         }

         if (clientSock != nullptr) {
            bool doCleanup = false;

            // read the client's Hello message
            array<Byte>^ authBuffer = gcnew array<Byte>(MAX_AUTH_INPUT_LEN);
            clientSock->ReceiveTimeout = CLIENT_AUTH_READ_TIMEOUT;

            try {
               // we're assuming the whole Hello message comes at once
               // TODO: use a parser
               int readLen = clientSock->Receive(authBuffer);
               // TODO: dump the authBuffer
               CBoostLog::log("Client Hello");

               // parse the Hello message, authenticate
               int result = parseHello(authBuffer, readLen);
               // send Hello response
               array<Byte>^ resp = buildHelloResponse(result);
               clientSock->Send(resp);

               if (result == OK) {
                  // add client to list
                  m_clients.addClient(clientSock);
               } else {
                  doCleanup = true;
               }
            }
            catch (SocketException^) {
               CBoostLog::log("Authentication timeout");
               doCleanup = true;
            }
            catch (ObjectDisposedException^) {
               CBoostLog::log("Socket already closed");
            }

            if (doCleanup) {
               clientSock->Close();
               clientSock = nullptr;
            }
         }
      }
      // clean up the listener
      m_listener = nullptr;
   }

   // TODO: use ByteVectors
   int CClientListener::parseHello(array<Byte>^ data, int length) {
      int result = OK;
      int index = 9;

      // check type
      if (length == EXPECTED_HELLO_LEN && data[8] == MUX_HELLO) {
         
         // check version
         Byte protoVersion = data[index++];
         if (protoVersion != SERIAL_API_PROTOCOL_VERSION) {
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

   array<Byte>^ CClientListener::buildHelloResponse(int result) {
      std::vector<Byte> data(1); 
      data[0] = SERIAL_API_PROTOCOL_VERSION;
      CMuxOutput resp(HELLO, 0, result, data);
      std::vector<Byte> output = resp.serialize();
      array<Byte>^ helloResp = convertVectorToArray(output);
      return helloResp;
   }

} // namespace DustSerialMux
