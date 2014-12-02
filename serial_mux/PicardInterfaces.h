/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef PicardInterfaces_H_
#define PicardInterfaces_H_


#pragma once

#include "Common.h"
#include "MuxMessageParser.h"


namespace DustSerialMux {

   /**
    * IPicardIO  provides the interface for sending commands to Picard
    */
   class IPicardIO {
   public:
      virtual uint8_t sendCommand(const CMuxMessage& cmd,
                                  uint8_t& seqNo, bool retransmit = false) = 0;

      virtual void sendAck(uint8_t type, uint8_t seqNo) = 0;
   };

   // ----------------------------------------------------------
   // Picard response/notification callback interface

   class IPicardCallback {
   public:
      virtual void commandComplete(uint8_t cmdType, uint8_t seqNo, uint8_t respCode, 
                                   const ByteVector& payload) = 0;

      virtual void handleNotif(uint8_t notifType, const ByteVector& notif) = 0;
   };

   
} // namespace DustSerialMux

#endif  /* ! PicardInterfaces_H_ */
