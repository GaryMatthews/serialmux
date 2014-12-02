/*
* Copyright (c) 2010, Dust Networks, Inc. 
*/

#pragma once

#include "stdafx.h"
#include <assert.h>

using namespace System;

using namespace System::Threading;

using namespace System::Net;
using namespace System::Net::Sockets;

#include <list>
#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

#include "Common.h"

#include "ClientIO.h"
#include "PicardInterfaces.h"
#include "MuxMessageParser.h"


namespace DustSerialMux {
   struct SClientCommand {
      IMuxClient* client;
      CMuxMessage command;
      uint8_t     seq; // message sequence number
      void clear() { client = NULL; command.clear(); seq = 0; }
   };


   // Client Manager 
   // contains list of active clients and the client command queue
   class CClients : public IClientList, public IPicardCallback, public ICommandCallback {
      typedef std::list<IMuxClient*> Clients;
      typedef std::list<SClientCommand> Commands;
   public:
      CClients(int retries, int timeout) 
         : m_lock(gcnew Mutex), 
           m_inProgress(gcnew Semaphore(0, 1)), 
           // m_clients
           // m_commands
           m_filterUnion(0),
           m_activeClient(NULL),
           m_retries(retries),
           m_timeout(timeout)
      { 
         m_currentCommand.clear(); 
      }

      void close() {
         // get rid of all commands
         m_commands.clear();
         // TODO: get rid of all clients

      }

      virtual ArrayList^ getSockets();

      virtual IMuxClient* findClientBySocket(Socket^ sock); 

      virtual void addClient(Socket^ clientSock);

      // called from the CInput thread
      virtual void readMuxMessage(Socket^ sock);

      // command callback from the client read
      virtual void handleCommand(const CMuxMessage& command);

      // called from the CInput thread
      virtual void sendCommands(IPicardIO* picard);

      // -------------------------------------------------------
      // methods for handling data from Picard

      virtual void commandComplete(Byte cmdType, Byte seqNo, Byte respCode,
                                   const std::vector<Byte>& response);

      virtual void handleNotif(Byte notifType, const std::vector<Byte>& payload);

   private:
      void commandTimeout(IMuxClient* client, Byte cmdType, Byte respCode);
      void sendResponse(IMuxClient* client, const CMuxOutput& resp,
                        const std::string& sender);

      bool recomputeSubscribeFilter();

      void removeClient(IMuxClient* client);

      // remove commands from a particular client
      void removeClientCommands(IMuxClient* client);

      gcroot<Mutex^>     m_lock; // lock access to the client list
      gcroot<Semaphore^> m_inProgress;
      // client data structures
      Clients  m_clients;
      Commands m_commands;
      int m_filterUnion; // union of all client subscriptions
      int m_prevfilter;  // previous filter, used for resetting subscriptions on error
      // fields to hold temporary state
      IMuxClient*     m_activeClient;   // client associated with the current input message
      SClientCommand  m_currentCommand; // current command sent to Picard
      int m_retries;  // number of times to retry a command to Picard
      int m_timeout;  // time to wait for a response from Picard
   };

} // namespace DustSerialMux
