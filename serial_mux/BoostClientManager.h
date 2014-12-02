/*
 * Copyright (c) 2010, Dust Networks, Inc. 
 */

#ifndef BoostClientManager_H_
#define BoostClientManager_H_

#pragma once

#include <stdint.h>
#include <list>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "Common.h"

#include "BoostClient.h"
#include "PicardInterfaces.h"
#include "MuxMessageParser.h"

#include "SyncQueue.h"


namespace DustSerialMux {

   enum EClientResult {
      CLIENT_OK = 0,
      CLIENT_TIMEOUT,
      CLIENT_DISCONNECT,
   };
   
   struct SClientCommand {
      SClientCommand() 
         : client(), command(), seq(0), result(CLIENT_TIMEOUT)
      { ; }
      SClientCommand(CBoostClient::pointer inClient, const CMuxMessage& inCmd)
         : client(inClient), command(inCmd), seq(0), result(CLIENT_TIMEOUT)
      { ; }
      
      CBoostClient::pointer client;
      CMuxMessage    command; // command data from client
      uint8_t        seq;     // message sequence number
      EClientResult  result;  // did we get a Picard response?
   };


   // Client Manager 
   // contains list of active clients and the client command queue
   class CBoostClientManager : public ISimpleClientList,
                               public IPicardCallback
   {
      typedef std::list<CBoostClient::pointer> Clients;
      typedef CSyncQueue<SClientCommand> Commands;
   public:
      CBoostClientManager(int retries, int timeout) 
         : m_lock(), 
           m_inProgressMutex(), 
           m_inProgress(), 
           m_isRunning(false),
           // m_clients
           // m_commands
           m_filterUnion(),
           m_prevfilter(),
           m_currentCommand(),
           m_retries(retries),
           m_timeout(timeout)
      { 
      }

      virtual ~CBoostClientManager();
      
      // main loop for processing commands
      void commandLoop(IPicardIO* picard);

      void closeClients();

      void stop();

      // * ISimpleClientList 
      
      virtual void addClient(CBoostClient::pointer client);

      virtual void removeClient(CBoostClient::pointer client);

      virtual void addCommand(CBoostClient::pointer client, const CMuxMessage& cmd);

      // -------------------------------------------------------
      // methods for handling data from Picard

      virtual void commandComplete(uint8_t cmdType, uint8_t seqNo, uint8_t respCode,
                                   const ByteVector& response);

      virtual void handleNotif(uint8_t notifType, const ByteVector& payload);

   private:
      void commandTimeout(CBoostClient::pointer client, uint8_t cmdType, uint8_t respCode);
      void sendResponse(CBoostClient::pointer client, const CMuxOutput& resp,
                        const std::string& sender);

      bool recomputeSubscribeFilter();

      // lock access to the client list
      boost::mutex  m_lock;
      
      // wait while a command is in progress
      boost::mutex               m_inProgressMutex; 
      boost::condition_variable  m_inProgress; 

      bool m_isRunning;
      
      // client data structures
      Clients  m_clients;
      Commands m_commands;
      SubscriptionParams m_filterUnion; // union of all client subscriptions
      SubscriptionParams m_prevfilter;  // previous filter, used for resetting subscriptions on error

      // fields to hold temporary state
      SClientCommand  m_currentCommand; // current command sent to Picard

      int m_retries;  // number of times to retry a command to Picard
      int m_timeout;  // time to wait for a response from Picard
   };

} // namespace DustSerialMux


#endif  /* ! BoostClientManager_H_ */
