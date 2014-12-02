/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef CLRUtils_H_
#define CLRUtils_H_

#pragma once

#include <stdint.h>
#include <vector>
#include <string>

#include <vcclr.h>
using namespace System;
using namespace System::Net;
using namespace System::Net::Sockets;
using namespace System::Threading;

#include "Common.h"  // for ByteVector

namespace DustSerialMux {
   // Copy the input data to the vector starting at offset for dataLength bytes
   ByteVector convertArrayToVector(array<Byte>^ ary, int offset, int dataLength);

   array<Byte>^ convertVectorToArray(const ByteVector& vec);

   std::string convertToStdString(String^ in);

   class CLRSocket : public ISocket
   {
   public:
      CLRSocket(Socket^ s);
      virtual ~CLRSocket();
      
      virtual void close();

      virtual void setTimeout(int millisecs);

      virtual bool isConnected();

      virtual int receive(ByteVector& buffer);
      virtual int send(const ByteVector& buffer);

      virtual std::string getRemotePeer();

      // temporary? access to internal socket
      Socket^ getInternal() { return m_sock; }

   private:
      gcroot<Socket^> m_sock;
   };
   
} // namespace

#endif
