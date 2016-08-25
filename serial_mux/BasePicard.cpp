/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#include "BasePicard.h"

#include "Common.h"
#include "BoostLog.h"
#include "HDLC.h"

#include "serial_mux.h"    // for resetConnection

#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::posix_time;


namespace DustSerialMux {

   const int SERIAL_API_HEADER_LEN = 4;
   const int READ_TIMEOUT = 1000; // milliseconds to wait for read completion

   CBasePicardIO::CBasePicardIO()
      : m_isRunning(false),
        m_connected(false),
        m_connectMutex(),
        m_connect(),
        m_callback(NULL),
        m_hdlc(NULL),
        m_protocolVersion(0),
        m_seqNo(0),
        m_mgrSeqNo(0)
   { ; }

   CBasePicardIO::~CBasePicardIO()
   { 
      cleanup();
   }

   // cleanup should be called before restarting the read loop
   void CBasePicardIO::cleanup()
   {
      m_connected = false;
      
      delete m_hdlc; 
      m_hdlc = NULL;
   }

   // reset anyone waiting for a Picard connection
   void CBasePicardIO::reset()
   {
      m_connect.notify_all();
   }

   void CBasePicardIO::registerCallback(IPicardCallback* handler)
   {
      m_callback = handler;
   }

   // -----------------------------------------------
   // base class interface methods

   /**
    * Send a command to Picard 
    */
   uint8_t CBasePicardIO::sendCommand(const CMuxMessage& cmd, uint8_t& seqNo, bool retransmit) 
   {
      // TODO: lock b/c we access a member variable (seqNo)
      
      uint8_t cmdLen = cmd.size() & 0xFF;
      std::vector<uint8_t> buf(cmdLen + SERIAL_API_HEADER_LEN);
      
      // prefix the control byte
      uint8_t control = 2; // Request (DATA) | RELIABLE
      size_t index = 0;
      buf[index++] = control;    // control
      buf[index++] = cmd.type(); // type
      buf[index++] = m_seqNo;
      buf[index++] = cmdLen;     // length
      // copy the command to the temporary buffer
      for (int i = 0; i < cmdLen; i++) {
         buf[index++] = cmd.m_data[i];
      }

      if (retransmit) {
         std::ostringstream msg;
         msg << "sendCmd: retransmit seq=" << (int)m_seqNo;
         CBoostLog::log(msg.str());
      }

      // set the sequence number for the caller before we write 
      // in case the response comes back *really* fast
      seqNo = m_seqNo; 
      // note: we shouldn't get any other commands until this one has responded

      sendRaw(buf);
      return m_seqNo;
   }

   void CBasePicardIO::sendAck(uint8_t type, uint8_t seqNo)
   {
      // TODO: lock b/c we access a member variable (mgrSeq)
      
      std::vector<uint8_t> buf;

      // prefix the control byte
      uint8_t control = 3;  // Response (ACK) | RELIABLE
      buf.push_back(control);
      buf.push_back(type);   // type
      buf.push_back(seqNo);  // sequence number
      buf.push_back(1);      // length
      
      buf.push_back(0);      // response code
      m_mgrSeqNo = seqNo+1;  // next expected sequence number

      sendRaw(buf);
   }

   // Returns: whether or not there is a connection
   bool CBasePicardIO::waitForHello() {
      if (!m_connected) {
         boost::unique_lock<boost::mutex> lock(m_connectMutex);
         m_connect.wait(lock);
      }
      return m_connected;
   }

   // -----------------------------------------------
   // private methods

   bool CBasePicardIO::checkProtocol(uint8_t version)
   {
      // check for a known protocol version
      bool knownProtoVersion = false;
      for (int i = 0; i < ARRAY_LEN(KNOWN_API_PROTOCOL_VERSIONS); i++) {
         if (version == KNOWN_API_PROTOCOL_VERSIONS[i]) {
            knownProtoVersion = true;
            break;
         }
      }

      if (!knownProtoVersion) {
         std::ostringstream msg;
         msg << "mgrHello: unknown protocol version=" << (int)version;
         CBoostLog::log(msg.str());
      }

      return knownProtoVersion;
   }
   
   void CBasePicardIO::sendHello(uint8_t seqNo)
   {
      // the protocol version to send is either the version that we get from
      // the Manager or the first (preferred) in the list of known versions
      uint8_t requestedVersion = m_protocolVersion;
      if (requestedVersion == 0) {
         requestedVersion = KNOWN_API_PROTOCOL_VERSIONS[0];
      }
      
      std::vector<uint8_t> buf;

      // prefix the control byte
      uint8_t control = 0;  // Request (DATA) | UNRELIABLE
      buf.push_back(control);
      buf.push_back(HELLO);
      buf.push_back(m_seqNo);  // sequence number
      buf.push_back(3);        // length
      buf.push_back(requestedVersion);
      buf.push_back(m_seqNo);  // (client) sequence number
      buf.push_back(0);        // reserved (mode)
      //m_seqNo++;
      sendRaw(buf);
   }

   // handle a complete message from Picard 
   void CBasePicardIO::frameComplete(const std::vector<uint8_t>& frame)
   {
      if (frame.size() > SERIAL_API_HEADER_LEN) {
         // parse the Serial API frame
         uint8_t control = frame[0];
         uint8_t type    = frame[1];
         uint8_t seqNo   = frame[2];
         uint8_t len     = frame[3];

         // check payload length
         if (len + SERIAL_API_HEADER_LEN != frame.size()) {
            return;
         }

         // handle input based on command type
         if (type == HELLO_RESPONSE && len >= 5) {
            //struct  spl_helloRsp {
            //   uint8_t  successCode;
            //   uint8_t  version;
            //   uint8_t  mgrSeqNo;
            //   uint8_t  cliSeqNo;
            //   uint8_t  mode;     // reserved for compatibility, must be 0
            //}
            // control = 0, type = HELLO_RESP, success code = OK
            uint8_t successCode = frame[SERIAL_API_HEADER_LEN];
            uint8_t version     = frame[SERIAL_API_HEADER_LEN + 1];
            uint8_t mgrSeqNo    = frame[SERIAL_API_HEADER_LEN + 2];
            uint8_t cliSeqNo    = frame[SERIAL_API_HEADER_LEN + 3];

            // record the protocol version if we recognize it (with any successCode)
            if (checkProtocol(version)) {
               m_protocolVersion = version;
            }
            if (control == 0 && successCode == 0) {
               // handle sequence numbers
               m_mgrSeqNo = mgrSeqNo;
               m_seqNo = cliSeqNo + 1; // increment our sequence number
               
               if (!m_connected) {
                  boost::lock_guard<boost::mutex> guard(m_connectMutex);
                  m_connected = true;
                  m_connect.notify_all();
               }
            } else {
               CBoostLog::log("Bad Hello Response");
            }
         }
         else if (type == MGR_HELLO && len >= 2) {
            //struct  spl_mgrHello {
            //   uint8_t  version;
            //   uint8_t  mode;     // reserved for compatibility, must be 0
            //}
            // record the protocol version if we recognize it
            if (checkProtocol(frame[SERIAL_API_HEADER_LEN])) {
               m_protocolVersion = frame[SERIAL_API_HEADER_LEN];
            }
            // reset when a MGR_HELLO is received, i.e. Picard has reset
            if (m_connected) {
               resetConnection();
               // Don't clear m_connected. We want everything to be torn down and restarted.
               // If m_connected is false, the read loop might send a new hello
            }
         }
         else if (type == NOTIFICATION && (control & 0x1) == 0 && len > 1) {
            // send ack if this notification is reliable, send ack before notif callback 
            bool isReliable = (control & 0x2);
            if (isReliable) {
               sendAck(type, seqNo);
            }

            // notif type is the first byte after the header
            uint8_t notifType = frame[SERIAL_API_HEADER_LEN];
            // notif payload is the rest
            std::vector<uint8_t> notif(frame.begin()+SERIAL_API_HEADER_LEN+1, frame.end());

            // we expect seqNo = m_mgrSeqNo + 1
            // always send notifications if unreliable, otherwise (if reliable),
            // filter out duplicates, but don't validate sequence number
            if (m_callback != NULL && (!isReliable || (m_mgrSeqNo != seqNo))) {
               m_callback->handleNotif(notifType, notif);
            }
            m_mgrSeqNo = seqNo;
         }
         // ensure this is a command response with command type in the right range
         // control == 3 means Response (ACK) | RELIABLE
         else if (type > NOTIFICATION && len >= 1 && control == 3) {
            // update the next expected sequence number
            m_seqNo = seqNo + 1;
            // response code is the first byte after the header
            uint8_t respCode = frame[SERIAL_API_HEADER_LEN];
            // response payload is the rest
            std::vector<uint8_t> payload(frame.begin()+SERIAL_API_HEADER_LEN+1, frame.end());
            if (m_callback != NULL) {
               m_callback->commandComplete(type, seqNo, respCode, payload);
            }
         }
      }
   }

   void CBasePicardIO::threadMain() 
   {
      // start a new parser
      m_hdlc = new CHDLC(INPUT_BUFFER_LEN, this);
      // init the hello to something in the past
      ptime lastHello = second_clock::universal_time() - seconds(2*PICARD_HELLO_INTERVAL);
      try {
         while (m_isRunning) {
            ptime now = second_clock::universal_time();
            if (!m_connected && (now - lastHello) > seconds(PICARD_HELLO_INTERVAL)) {
               // note: send should catch exceptions
               sendHello(0);
               lastHello = second_clock::universal_time();
            }
            // the PicardIO main loop always reads
            read("read loop", READ_TIMEOUT);
         }
      }
      catch (const std::exception& ex) {
         std::ostringstream msg;
         msg << "IO exception (Picard read loop): " << ex.what();
         CBoostLog::log(msg.str());
      }
   }

} // namespace DustSerialMux
