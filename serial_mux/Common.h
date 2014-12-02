/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef Common_H_
#define Common_H_

#pragma once

#include <stdint.h>
#include <vector>
#include <string>

namespace DustSerialMux {
   
   enum ECommands {
      NULL_COMMAND   = 0,
      HELLO          = 1,
      HELLO_RESPONSE = 2,
      MGR_HELLO      = 3,
      NOTIFICATION   = 20,
      SUBSCRIBE      = 22,
   };

   enum EMuxCommands {
      MUX_HELLO = 1,
      MUX_INFO = 2,
   };

   enum ErrorCode {
      OK = 0,
      ERR_INVALID_CMD = 1,
      ERR_INVALID_ARG = 2,
      ERR_INVALID_AUTH = 3,
      ERR_UNSUPPORTED_VERSION = 4,
      ERR_COMMAND_TIMEOUT = 5,
   };

#define ARRAY_LEN(ary) (sizeof(ary)/sizeof(ary[0]))
   
   // types
   typedef std::vector<uint8_t> ByteVector;

   
   // validation for commands that can be forwarded to the Picard Serial API
   bool isPicardApiCommand(uint8_t type);

   ByteVector muxInfoPayload(uint8_t protocolVersion);
   
   
   // Common operations on the subscription filter
   
   struct SubscriptionParams {
      SubscriptionParams(int aFilter = 0, int aUnreliable = 0) 
         : filter(aFilter), unreliable(aUnreliable) 
      { ; }

      int filter;
      int unreliable;
   };

   const int SUBSCRIBE_PARAMS_LENGTH = 8;
   void filterToVector(SubscriptionParams filter, std::vector<uint8_t>& data);
   SubscriptionParams vectorToFilter(const std::vector<uint8_t>& data);

   inline bool subscribeFilterMatch(SubscriptionParams params, int notifType) { 
      return (params.filter & (1<<notifType)) != 0; 
   }


   class ISocket {
   public:
      virtual void close() = 0;

      virtual void setTimeout(int millisecs) = 0;

      virtual bool isConnected() = 0;
      
      virtual int receive(ByteVector& buffer) = 0;
      virtual int send(const ByteVector& buffer) = 0;

      virtual std::string getRemotePeer() = 0;
   };
}

#endif /* ! Common_H_ */
