#pragma once

#include <string>

#include "Common.h"

#include "ClientIO.h"
#include "MuxMessageParser.h"
#include "SerialMuxOptions.h"  // for AUTHENTICATION_LEN

#include <vcclr.h>
using namespace System;
using namespace System::Net;

namespace DustSerialMux 
{
   class CClientListener 
   {
   public:
      CClientListener(uint16_t port, ISimpleClientList& clients, const uint8_t* authToken);

      virtual ~CClientListener();

      virtual void threadMain();

      // validate the Hello
      int parseHello(array<Byte>^ data, int length);

      array<Byte>^ buildHelloResponse(int result);

      void stop() {
         m_isListening = false;
         if (m_listener) {
            m_listener->Stop();
         }
      }

      uint16_t getPort() const { return m_listenerPort; }

   private:
      uint16_t              m_listenerPort;
      std::string           m_listenerAddr;
      bool                  m_isListening;
      gcroot<TcpListener^>  m_listener;
      ISimpleClientList&    m_clients;
      unsigned char         m_expectedAuth[AUTHENTICATION_LEN];
   };

} // namespace DustSerialMux
