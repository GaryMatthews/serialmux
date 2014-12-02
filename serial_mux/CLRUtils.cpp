/*
 * Copyright (c) 2010, Dust Networks, Inc. 
 */

#include "stdafx.h"
#include <msclr\marshal_cppstd.h> // to avoid compile errors make sure this is before std C++ headers

#include "CLRUtils.h"


namespace DustSerialMux {
   
   // Copy the input data to the vector starting at offset for dataLength bytes
   std::vector<Byte> convertArrayToVector(array<Byte>^ ary, int offset, int dataLength) {
      std::vector<Byte> data;
      for (int i = 0; i < dataLength; i++) { Byte b = ary[offset+i]; data.push_back(b); }
      return data;
   }

   // Copy the vector to a new array
   array<Byte>^ convertVectorToArray(const std::vector<Byte>& vec) {
      array<Byte>^ ary = gcnew array<Byte>(vec.size());
      // copy the output data to the temporary buffer
      for (size_t i = 0; i < vec.size(); i++) { ary[i] = vec[i]; }
      return ary;
   }

   std::string convertToStdString(String^ in)
   {
      msclr::interop::marshal_context context;
      return context.marshal_as<std::string>(in);
   }

   
   CLRSocket::CLRSocket(Socket^ s)
      : m_sock(s)
   {
      ;
   }

   CLRSocket::~CLRSocket() 
   {
      m_sock = nullptr;
   }
   
   void CLRSocket::close()
   {
      m_sock->Close();
   }

   void CLRSocket::setTimeout(int millisecs)
   {
      m_sock->ReceiveTimeout = millisecs;
   }

   bool CLRSocket::isConnected() 
   {
      return m_sock->Connected;
   }
   
   int CLRSocket::receive(ByteVector& buffer)
   {
      array<Byte>^ clrBuffer = gcnew array<Byte>(buffer.capacity());
      int readLen = 0;

      // TODO: catch CLR exceptions
      
      readLen = m_sock->Receive(clrBuffer);
      // copy the socket data to the input vector
      buffer = convertArrayToVector(clrBuffer, 0, readLen);
      
      return readLen;
   }

   int CLRSocket::send(const ByteVector& buffer)
   {
      array<Byte>^ clrBuffer = convertVectorToArray(buffer);
      int sentLen = m_sock->Send(clrBuffer);
      return sentLen;
   }

   std::string CLRSocket::getRemotePeer()
   {
      IPEndPoint^ remote = safe_cast<IPEndPoint^>(m_sock->RemoteEndPoint);
      String^ remoteName = String::Format("{0}:{1}",
                           remote->Address->ToString(), remote->Port.ToString());

      return convertToStdString(remoteName);
   }

   
}
