/*
* Copyright (c) 2010, Dust Networks, Inc.
*/

#include "StdAfx.h"
#include "PicardCLR.h"

#include "BoostLog.h"
#include "CLRUtils.h"

#include "serial_mux.h"    // for resetConnection

#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::posix_time;


namespace DustSerialMux {

   CPicardCLR_Serial::CPicardCLR_Serial(SerialPort^ device,
      int rtsDelay, bool hwFlowControl, int readTimeout)
      : m_serial(device), 
      m_rtsDelay(rtsDelay),
      m_hwFlowControl(hwFlowControl),
      m_readTimeout(readTimeout)
   { ; }

   CPicardCLR_Serial::~CPicardCLR_Serial() { 
      // TODO: cleanup
   }

   void CPicardCLR_Serial::sendRaw(const ByteVector& data)
   {
      // locks should be handled at the sendCommand / sendAck methods

      // HDLC encode
      ByteVector encodedvec = encodeHDLC(data);
      array<Byte>^ encodeddata = convertVectorToArray(encodedvec);
      bool portClosed = false;
      
      if (m_hwFlowControl) {
         m_serial->RtsEnable = 1;
         // busy wait for DSR (or timeout)
         bool sendTimeout = false;
         ptime timeout = microsec_clock::local_time() + milliseconds(m_serial->WriteTimeout);
         while (m_serial->DsrHolding == 0) {
            ptime now = microsec_clock::local_time();
            if (now > timeout) {
               sendTimeout = true;
               break;
            }
            boost::this_thread::sleep(milliseconds(1)); 
         }
         if (sendTimeout) {
            // note: we detect the write failure when Acks are not received
            CBoostLog::log("Serial:Write error: no CTS received");
            return;
         }
      }

      try {
         m_serial->Write(encodeddata, 0, encodeddata->Length);

         CBoostLog::logDump("Serial:Write", encodedvec);   
         
         // wait before deasserting RTS
         // TODO: later, we may need to adjust the RTS delay based on data length
         if (m_hwFlowControl) {
            if (m_rtsDelay > 0) {
               boost::this_thread::sleep(milliseconds(m_rtsDelay));
            }
            m_serial->RtsEnable = 0;
         }
      }
      catch (Exception^) {
         CBoostLog::log("Exception on write (Serial closed)");
         portClosed = true;
      }

      if (portClosed) {
         // when the port is closed, we reset and hope it re-opens soon
         resetConnection();
      }      
   }

   // read 
   void CPicardCLR_Serial::read(const std::string& context, int timeout)
   {
      CBoostLog::log("Starting read()");
      bool portClosed = false;

      try {
         m_serial->ReadTimeout = timeout;
         array<unsigned char>^ input = gcnew array<unsigned char>(INPUT_BUFFER_LEN);
         int len = m_serial->Read(input, 0, INPUT_BUFFER_LEN);

         std::ostringstream prefix;
         prefix << "Serial:Read (" << context << ")";
         CBoostLog::logDump(prefix.str(), convertArrayToVector(input, 0, len));

         for (int i = 0; i < len; i++) {
            m_hdlc->addByte(input[i]);
         }
         // the HDLC parser calls frameComplete
      }
      catch (TimeoutException^) {
         // CBoostLog::log("Timeout exception (Serial read)");
      }
      catch (Exception^) {
         CBoostLog::log("Invalid operation on read (Serial closed)");
         portClosed = true;
      }

      if (portClosed) {
         // when the port is closed, we reset and hope it re-opens soon
         resetConnection();
      }
   }   


   CPicardCLR_UDP::CPicardCLR_UDP(UdpClient^ device, int readTimeout)
      : m_outputDev(device),
        m_readTimeout(readTimeout)
   { ; }

   CPicardCLR_UDP::~CPicardCLR_UDP() { 
      // TODO: cleanup
   }

   void CPicardCLR_UDP::sendRaw(const ByteVector& data)
   {
      // TODO: is there a better solution than copying the whole array?
      // create a new buffer offset by one byte so there's a dummy byte in front
      int length = data.size();
      array<unsigned char>^ buf = gcnew array<unsigned char>(length+1);
      for (int i = 0; i < length; i++) { buf[i+1] = data[i]; }
      try {
         m_outputDev->Send(buf, length+1);
      }
      catch (Exception^) {
         CBoostLog::log("Socket exception (UDP write)");
      }
      CBoostLog::logDump("UDP:Write (first byte excluded)", data);
   }

   // read 
   void CPicardCLR_UDP::read(const std::string& context, int timeout)
   {
      CBoostLog::log("Starting read()");

      try {
         IPEndPoint^ remote;
         m_outputDev->Client->ReceiveTimeout = timeout; // milliseconds
         array<unsigned char>^ buf = m_outputDev->Receive(remote);

         // with UDP, there's no HDLC parser, so we call frameComplete directly
         // start at offset 1 to drop the dummy byte in front
         ByteVector data = convertArrayToVector(buf, 1, buf->Length-1);

         std::ostringstream prefix;
         prefix << "UDP:Read (" << context << ")";
         CBoostLog::logDump(prefix.str(), data);

         //boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
         //std::cout << "(" << now << ") : UDP read, len=" << buf->Length-1 << std::endl;

         frameComplete(data);
      }
      catch (Exception^) {
         CBoostLog::log("Socket exception (UDP read)");
      }
   }

} // namespace DustSerialMux
