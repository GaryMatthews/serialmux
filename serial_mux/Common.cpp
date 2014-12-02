/*
 * Copyright (c) 2010, Dust Networks, Inc. 
 */


#include "Common.h"
#include "Version.h"

namespace DustSerialMux {

   bool isPicardApiCommand(uint8_t type)
   {
      return type > NOTIFICATION;
   }
   
   ByteVector muxInfoPayload(uint8_t protocolVersion) 
   {
      SMuxVersion v = getVersion();

      ByteVector data;
      data.push_back(protocolVersion);
      data.push_back(v.major);
      data.push_back(v.minor);
      data.push_back(v.release);
      data.push_back((v.build >> 8) & 0xFF);
      data.push_back(v.build & 0xFF);

      return data;
   }   

   
   void filterToVector(SubscriptionParams params, std::vector<uint8_t>& data)
   {
      data.reserve(SUBSCRIBE_PARAMS_LENGTH);
      data[0] = (params.filter >> 24) & 0xFF;
      data[1] = (params.filter >> 16) & 0xFF;
      data[2] = (params.filter >> 8) & 0xFF;
      data[3] = params.filter & 0xFF;
      data[4] = (params.unreliable >> 24) & 0xFF;
      data[5] = (params.unreliable >> 16) & 0xFF;
      data[6] = (params.unreliable >> 8) & 0xFF;
      data[7] = params.unreliable & 0xFF;
   }
   SubscriptionParams vectorToFilter(const std::vector<uint8_t>& data)
   {
      SubscriptionParams params;
      params.filter = ((data[0] << 24) & 0xFF000000) | 
         ((data[1] << 16) & 0xFF0000) | 
         ((data[2] << 8) & 0xFF00) | 
         (data[3] & 0xFF); 
      if (data.size() >= 8) {
         params.unreliable = ((data[4] << 24) & 0xFF000000) | 
            ((data[5] << 16) & 0xFF0000) | 
            ((data[6] << 8) & 0xFF00) | 
            (data[7] & 0xFF);
      } 
      return params;
   }

} 
