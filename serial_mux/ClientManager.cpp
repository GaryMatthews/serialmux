/*
 * Copyright (c) 2010, Dust Networks, Inc. 
 */

#include "stdafx.h"
#include <assert.h>

using namespace System;
using namespace System::Threading;

using namespace System::Net;
using namespace System::Net::Sockets;

#include "ClientManager.h"
#include "BoostLog.h"
#include "CLRUtils.h"


namespace DustSerialMux {

   // public interface methods

   ArrayList^ CClients::getSockets() 
   {
      ArrayList^ sockets = gcnew ArrayList(m_clients.size());
      Clients::const_iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         sockets->Add((*iter)->getSocket());
      }
      return sockets;
   }

   IMuxClient* CClients::findClientBySocket(Socket^ sock) 
   {
      // look up the client that has the matching socket
      Clients::const_iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         if (sock == (*iter)->getSocket()) {
            return (*iter);
         }
      }
      return NULL;
   }

   void CClients::addClient(Socket^ clientSock)
   {
      // TODO: verify not already present
      CMuxClient* muxClient = new CMuxClient(clientSock, this);

      m_lock->WaitOne();
      m_clients.push_back(muxClient);
      m_lock->ReleaseMutex();
   }

   // called from the CInput thread
   void CClients::readMuxMessage(Socket^ sock) {
      IMuxClient* client = findClientBySocket(sock);
      if (client != NULL) {
         // read the next command
         m_activeClient = client;
         std::vector<Byte> data;
         int len = client->read(data);  
         if (len == 0) {
            // handle disconnect
            CBoostLog::log("Removing this client"); // TODO: log better description than 'this'
            removeClient(client);
         }
         m_activeClient = NULL;
      } else {
         CBoostLog::log("readClientCommand: UNEXPECTED: NULL client");
      }
   }

   // command callback from the client read
   void CClients::handleCommand(const CMuxMessage& command) {
      SClientCommand clientCmd = { m_activeClient, command };
      m_commands.push_back(clientCmd);
   }

   // called from the CInput thread
   void CClients::sendCommands(IPicardIO* picard) {
      // process the queue until it's empty
      while (!m_commands.empty()) {
         SClientCommand cmd = m_commands.front();
         m_commands.pop_front();
         if (cmd.client != NULL) {
            Socket^ sock = cmd.client->getSocket();
            IPEndPoint^ remote = safe_cast<IPEndPoint^>(sock->RemoteEndPoint);
            String^ remoteName = String::Format("{0}:{1}",
                                 remote->Address->ToString(),remote->Port.ToString());

            std::ostringstream prefix;
            prefix << "processing command " << cmd.command.type() << " from "
                   << convertToStdString(remoteName);
            CBoostLog::logDump(prefix.str(), cmd.command.m_data);
         }

         // filter out invalid commands
         if (cmd.command.type() <= NOTIFICATION) {
            ByteVector dummy;
            CMuxOutput resp(cmd.command.type(), 0 /* id */, ERR_INVALID_CMD, dummy);
            sendResponse(cmd.client, resp, "CClients::commandError");
            continue;
         }
         
         // if this is a subscribe, then use the union of all subscriptions
         if (cmd.command.type() == SUBSCRIBE && cmd.client != NULL) {
            cmd.client->setSubscription(vectorToFilter(cmd.command.m_data));
            bool changed = recomputeSubscribeFilter();
#if 0
            // optimization: only send subscribe if the filter union changes
            if (!changed) {
               // construct the response
               ByteVector dummy;
               CMuxOutput resp(cmd.command.type(), 0 /* id */, OK, dummy);
               sendResponse(cmd.client, resp, "CClients::commandComplete");
               cmd.client->commitFilter();
               continue;
            }
#endif
            // update the subscribe command with the complete filter
            filterToVector(m_filterUnion, cmd.command.m_data);
         }
         
         // keep the SClientCommand as state to know where to send the response
         m_currentCommand = cmd;

         // wait for the command to complete or timeout
         bool result = false;
         for (int i = 0; !result && i < m_retries; i++) {
            // send the command to Picard -- the last parameter is a flag indicating a retransmit
            picard->sendCommand(cmd.command, m_currentCommand.seq, i != 0);

            // wait for the command complete callback to set the semaphore
            result = m_inProgress->WaitOne(m_timeout);
            // boost::posix_time::ptime tout = boost::posix_time::microsecond_clock::local_time() + boost::posix_time::seconds(3);
            // m_inProgress.timed_wait(tout);
         }
         if (!result && cmd.client != NULL) {
            // handle command timeout
            if (cmd.command.type() == SUBSCRIBE) {
               // reset the filter union to its previous value
               m_filterUnion = m_prevfilter;
               cmd.client->resetFilter();
            }
            commandTimeout(cmd.client, cmd.command.type(), ERR_COMMAND_TIMEOUT);
         }
         m_currentCommand.clear();
      }
   }


   // -------------------------------------------------------
   // Client IO methods for handling data from Picard

   // handle command response from Picard
   void CClients::commandComplete(Byte cmdType, Byte seqNo, Byte respCode,
      const std::vector<Byte>& response) 
   {
      // validate the response
      uint8_t expectedSeq = m_currentCommand.seq;
      uint8_t expectedType = m_currentCommand.command.type();
      if (cmdType != expectedType || seqNo != expectedSeq) {
         std::ostringstream msg;
         msg << "CommandComplete error: invalid response type=" << (int)cmdType
             << " seq=" << (int)seqNo << ", expected " << (int)expectedType << ":" << (int)expectedSeq;
         
         CBoostLog::log(msg.str());
         return;
      }

      std::ostringstream prefix;
      prefix << "CClients::commandComplete: cmd=" << (int)cmdType
             << " resp=" << (int)respCode;
      CBoostLog::logDump(prefix.str(), response);

      if (cmdType == SUBSCRIBE && respCode != OK) {
         // reset the filter union to its previous value
         m_filterUnion = m_prevfilter;
      }

      m_lock->WaitOne();
      if (m_currentCommand.client != NULL) {
         // construct the response
         CMuxOutput resp(cmdType, 0 /* id */, respCode, response);
         sendResponse(m_currentCommand.client, resp, "CClients::commandComplete");
         // moved inProgress post outside client check because client can be
         // null when sending re-subscribe due to removed client

         if (cmdType == SUBSCRIBE) {
            // if subscribe, commit client filter
            m_currentCommand.client->commitFilter();
         }
      }
      m_inProgress->Release();
      // m_inProgress.post();
      m_lock->ReleaseMutex();
   }


   // handle notification from Picard

   void CClients::handleNotif(Byte notifType, const std::vector<Byte>& payload) 
   {
      std::ostringstream prefix;
      prefix << "CClients::handleNotif: type=" << (int)notifType;
      CBoostLog::logDump(prefix.str(), payload);
      m_lock->WaitOne();
      Clients::iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         if ((*iter)->isSubscribed(notifType)) {
            {
               Socket^ sock = (*iter)->getSocket();
               IPEndPoint^ remote = safe_cast<IPEndPoint^>(sock->RemoteEndPoint);
               String^ remoteName = String::Format("{0}:{1}",
                                    remote->Address->ToString(), remote->Port.ToString());

               std::ostringstream prefix;
               prefix << "CClients::handleNotif: sending to " << convertToStdString(remoteName);
               
               CBoostLog::logDump(prefix.str(), payload);                              
            }
            CMuxOutput notif(NOTIFICATION, 0 /* id */, notifType, payload);
            (*iter)->write(notif.serialize());
            // TODO: handle write failure / exception ?
         }
      }
      m_lock->ReleaseMutex();
   }


   // -------------------------------------------------------
   // internal (private) methods

   // removeClient is called from readMuxMessage
   void CClients::removeClient(IMuxClient* client) 
   {
      // Internal State:
      // By definition, client is the active client, but nothing was read, 
      // so there's no parsing to do and activeClient will be cleared on return
      // Also, since sendCommands is part of the read loop, the current command
      // state will not contain this client.

      if (m_currentCommand.client != NULL) {
         CBoostLog::log("Warning: current command is set while processing remove client");
      }

      m_lock->WaitOne();
      Clients::iterator iter = std::find(m_clients.begin(), m_clients.end(), client);
      if (iter != m_clients.end()) {
         removeClientCommands(client);
         m_clients.erase(iter);
         // update the subscription filter now that the client is gone
         {
            bool changed = recomputeSubscribeFilter();
            if (changed) {
               ByteVector payload(SUBSCRIBE_FILTER_LENGTH);
               filterToVector(m_filterUnion, payload);
               CMuxMessage command(SUBSCRIBE, payload);

               SClientCommand clientCmd = { NULL, command };
               m_commands.push_back(clientCmd);
            }
         }
         delete client; // deleting the client closes the socket
      }
      m_lock->ReleaseMutex();
   }

   // Returns: whether the overall filter union changed
   bool CClients::recomputeSubscribeFilter() 
   {
      bool changed = false;
      int filterUnion = 0;

      Clients::iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         int filter = (*iter)->getSubscription();
         filterUnion |= filter;
      }

      changed = (filterUnion != m_filterUnion);
      m_prevfilter = m_filterUnion; // save the filter in case there's an error 
      m_filterUnion = filterUnion;
      return changed;
   }

   // note: caller must lock
   void CClients::sendResponse(IMuxClient* client, const CMuxOutput& resp,
                               const std::string& sender)
   {
      if (client != NULL && client->getSocket()->Connected) {   
         {
            Socket^ sock = client->getSocket();
            IPEndPoint^ remote = safe_cast<IPEndPoint^>(sock->RemoteEndPoint);
            String^ remoteName = String::Format("{0}:{1}",
                                    remote->Address->ToString(), remote->Port.ToString());
            
            std::ostringstream msg;
            msg << sender << ": sending to " << convertToStdString(remoteName);
            CBoostLog::log(msg.str());
         }

         // send the serialized response
         client->write(resp.serialize());
         // TODO: handle write failure / exception ?
      }
   }

   void CClients::commandTimeout(IMuxClient* client, Byte cmdType, Byte respCode) 
   {
      std::ostringstream msg;
      msg << "CClients::commandTimeout: cmd=" << (int)cmdType 
          << " resp=" << (int)respCode;
      CBoostLog::log(msg.str());

      // construct the response
      std::vector<Byte> dummy;
      CMuxOutput resp(cmdType, 0 /* id */, respCode, dummy);

      m_lock->WaitOne(); // TODO: why lock access here?
      sendResponse(client, resp, "CClients::commandTimeout");
      m_lock->ReleaseMutex();
   }


   // Internal predicate to match a specific IMuxClient instance
   class isClientCommand {
   public:
      isClientCommand(IMuxClient* client) : m_client(client) { ; }
      bool operator() (const SClientCommand& value) { return value.client == m_client; }
   private:
      IMuxClient* m_client;
   };

   // remove commands from a particular client
   void CClients::removeClientCommands(IMuxClient* client) {
      m_commands.remove_if(isClientCommand(client));
   }

} // namespace DustSerialMux
