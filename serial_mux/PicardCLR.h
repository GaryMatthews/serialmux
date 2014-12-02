/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef PicardCLR_H_
#define PicardCLR_H_

#pragma once


#include "BasePicard.h"

#include <vcclr.h>
using namespace System;
//using namespace System::Threading;
using namespace System::IO;
using namespace System::IO::Ports;
using namespace System::Net;
using namespace System::Net::Sockets;


namespace DustSerialMux {

   /**
    * OBSOLETE -- 
    * CPicardIO reads provides the IO interface for Picard (via a serial port or UDP)
    * It provides the IPicardIO interface for sending commands to Picard
    * It reads from Picard and generates callbacks to the IClientIO interface
    * 
    */
   // The output class 
   class CPicardCLR_Serial : public CBasePicardIO {
   public:
      CPicardCLR_Serial(SerialPort^ device,
                        int rtsDelay, bool hwFlowControl, int readTimeout);

      virtual ~CPicardCLR_Serial();

   protected:
      virtual void sendRaw(const std::vector<Byte>& data);
      
      virtual void read(const std::string& context, int timeout);

   private:
      // serial port options
      int m_rtsDelay; // millisecond delay before deasserting RTS
      bool m_hwFlowControl;
      int m_readTimeout; // millisecond timeout for read operations
      
      // port (serial or UDP) used for reading from Picard
      gcroot<SerialPort^> m_serial;
   };

   class CPicardCLR_UDP : public CBasePicardIO {
   public:
      CPicardCLR_UDP(UdpClient^ device, int readTimeout);

      virtual ~CPicardCLR_UDP();

   protected:
      virtual void sendRaw(const std::vector<Byte>& data);
      
      virtual void read(const std::string& context, int timeout);

   private:
      int m_readTimeout; // millisecond timeout for read operations
      
      // port (serial or UDP) used for reading from Picard
      gcroot<UdpClient^> m_outputDev;
   };
   
} // namespace DustSerialMux

#endif  /* ! PicardCLR_H_ */
