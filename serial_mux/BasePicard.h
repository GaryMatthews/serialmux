/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef BasePicardIO_H_
#define BasePicardIO_H_

#pragma once


#include "PicardInterfaces.h"
#include "MuxMessageParser.h"
#include "HDLC.h"

#include <boost/thread/condition_variable.hpp>


namespace DustSerialMux {

   // list of known API versions -- the first entry is the version we request
   // if the Manager hasn't sent us a MgrHello
   const uint8_t KNOWN_API_PROTOCOL_VERSIONS[] = { 4, 3 };
   const int  PICARD_HELLO_INTERVAL = 6; // seconds


   /**
    * CPicardIO reads provides the IO interface for Picard (via a serial port or UDP)
    * It provides the IPicardIO interface for sending commands to Picard
    * It reads from Picard and generates callbacks to the IClientIO interface
    * 
    * CPicardIO is a Managed class because it inherits from CThread/IRunner
    *
    */
   // The output class 
   class CBasePicardIO : public IPicardIO, public IHDLCParser {
   public:
      static const int INPUT_BUFFER_LEN = 1024;

      CBasePicardIO();

      virtual ~CBasePicardIO();
      // cleanup should be called before restarting the read loop
      // in the serial mux main loop, we destroy and recreate all components
      void cleanup();
      
      void start() { m_isRunning = true; }
      void stop() { m_isRunning = false; }

      void registerCallback(IPicardCallback* handler);

      // -----------------------------------------------
      // base class interface methods

      virtual uint8_t sendCommand(const CMuxMessage& cmd, uint8_t& seqNo, bool retransmit);
      virtual void sendAck(uint8_t type, uint8_t seqNo);

      
      // read loop for input from Picard
      void threadMain();

      // wait for a connection from Picard
      bool waitForHello();

      // reset the waitForHello semaphore
      void reset();
      
      uint8_t getVersion() const { return m_protocolVersion; }

      // callback for complete messsage from Picard
      virtual void frameComplete(const ByteVector& packet);

   protected:
      bool checkProtocol(uint8_t version);
      
      void sendHello(uint8_t seqNo);

      virtual void sendRaw(const ByteVector& data) = 0;
      
      virtual void read(const std::string& context, int timeout) = 0;
      
      // handler for input from Picard
      IPicardCallback* m_callback;
      CHDLC* m_hdlc;

   private:
      bool m_isRunning;

      bool m_connected;
      boost::mutex m_connectMutex;
      boost::condition_variable m_connect;

      uint8_t m_protocolVersion;
      uint8_t m_seqNo;    // next sequence number to send
      uint8_t m_mgrSeqNo; // last sequence number received
      
   };

} // namespace DustSerialMux

#endif  /* ! BasePicardIO_H_ */
