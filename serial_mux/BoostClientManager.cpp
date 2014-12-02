/*
 * Copyright (c) 2010, Dust Networks, Inc. 
 */

#include "BoostClientManager.h"

#include "BoostLog.h"
#include "serial_mux.h"

#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace DustSerialMux {
   // max command payload (not including Serial API header)
   const int MAX_SERIAL_API_CMD_LEN = 128;
   
   // the destructor must be called after the Input and Output threads
   // are shut down to ensure nothing is accessing the client list. 
   CBoostClientManager::~CBoostClientManager()
   {
      // get rid of all clients and commands
   }
   
   void CBoostClientManager::closeClients()
   {
      boost::mutex::scoped_lock guard(m_lock);
      Clients::iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         (*iter)->close();
      }
      m_clients.clear();
   }

   void CBoostClientManager::stop()
   {
      m_isRunning = false;
      // make sure the command list is empty
      m_commands.clear();
   }   

   void CBoostClientManager::addClient(CBoostClient::pointer client)
   {
      // TODO: verify not already present
      boost::mutex::scoped_lock guard(m_lock);
      m_clients.push_back(client);
   }


   // command callback from the client read
   void CBoostClientManager::addCommand(CBoostClient::pointer client,
                                        const CMuxMessage& command)
   {
      m_commands.push(SClientCommand(client, command));
   }

   
   // main loop for processing commands
   void CBoostClientManager::commandLoop(IPicardIO* picard)
   {
      m_isRunning = true;
      
      // process the queue until it's empty
      while (m_isRunning) {
         SClientCommand cmd;
         bool good = m_commands.timedPop(cmd, 1);
         if (!good) {
            continue;
         }
         
         Clients::const_iterator iter = std::find(m_clients.begin(), m_clients.end(),
                                                  cmd.client);
         // note: we do have to process commands with NO client (e.g. resubscribes)
         if (iter == m_clients.end() && cmd.client) {
            continue;
         }
         
         if (cmd.client) {
            std::ostringstream prefix;
            prefix << "processing command " << (int)cmd.command.type() << " from "
                   << cmd.client->remoteName();
            CBoostLog::logDump(prefix.str(), cmd.command.m_data);
         }

         // handle commands that are not sent to Picard
         if (cmd.command.type() == MUX_INFO) {
            CMuxOutput resp(MUX_INFO, 0 /* id */, OK,
                            muxInfoPayload(cmd.client->getProtocolVersion()));
            sendResponse(cmd.client, resp, "CBoostClientManager");
            continue;
         }
         
         // filter out invalid commands
         if (!isPicardApiCommand(cmd.command.type()) ||
             cmd.command.size() > MAX_SERIAL_API_CMD_LEN) {
            ByteVector dummy;
            CMuxOutput resp(cmd.command.type(), 0 /* id */, ERR_INVALID_CMD, dummy);
            sendResponse(cmd.client, resp, "CBoostClientManager::commandError");
            continue;
         }
         
         // if this is a subscribe, then use the union of all subscriptions
         if (cmd.command.type() == SUBSCRIBE && cmd.client) {
            cmd.client->setSubscription(vectorToFilter(cmd.command.m_data));
            bool changed = recomputeSubscribeFilter();
#if 0
            // optimization: only send subscribe if the filter union changes
            if (!changed) {
               // construct the response
               ByteVector dummy;
               CMuxOutput resp(cmd.command.type(), 0 /* id */, OK, dummy);
               sendResponse(cmd.client, resp, "CBoostClientManager::commandComplete");
               cmd.client->commitFilter();
               continue;
            }
#endif
            // update the subscribe command with the complete filter
            // we rewrite the command data
            filterToVector(m_filterUnion, cmd.command.m_data);
         }
         
         // keep the SClientCommand as state to know where to send the response
         m_currentCommand = cmd;

         // wait for the command to complete or timeout
         EClientResult result = CLIENT_TIMEOUT;
         for (int i = 0; result == CLIENT_TIMEOUT && i < m_retries; i++) {
            // send the command to Picard -- the last parameter is a flag indicating a retransmit
            picard->sendCommand(cmd.command, m_currentCommand.seq, i != 0);

            // wait for the command complete callback to set the semaphore
            boost::unique_lock<boost::mutex> lock(m_inProgressMutex);
            m_inProgress.timed_wait(lock, boost::posix_time::milliseconds(m_timeout));
            result = m_currentCommand.result;
         }
         // in both the timeout and disconnect cases, we want to send a
         // timeout response to the client
         if (result != CLIENT_OK && cmd.client) {
            if (cmd.command.type() == SUBSCRIBE) {
               // reset the filter union to its previous value
               m_filterUnion = m_prevfilter;
               cmd.client->resetFilter();
            }
            commandTimeout(cmd.client, cmd.command.type(), ERR_COMMAND_TIMEOUT);
         }
         // reset the current command state (clear the client pointer)
         m_currentCommand = SClientCommand();
      }
   }


   // -------------------------------------------------------
   // Client IO methods for handling data from Picard

   // handle command response from Picard
   void CBoostClientManager::commandComplete(uint8_t cmdType, uint8_t seqNo, uint8_t respCode,
                                             const ByteVector& response) 
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
      prefix << "CBoostClientManager::commandComplete: cmd=" << (int)cmdType
             << " resp=" << (int)respCode;
      CBoostLog::logDump(prefix.str(), response);

      if (cmdType == SUBSCRIBE && respCode != OK) {
         // reset the filter union to its previous value
         m_filterUnion = m_prevfilter;
      }

      {
         boost::lock_guard<boost::mutex> lock(m_inProgressMutex);
         m_currentCommand.result = CLIENT_OK;
         
         if (m_currentCommand.client) {
            // construct the response
            CMuxOutput resp(cmdType, 0 /* id */, respCode, response);
            sendResponse(m_currentCommand.client, resp, "CBoostClientManager::commandComplete");
            // moved inProgress post outside client check because client can be
            // null when sending re-subscribe due to removed client
            
            if (cmdType == SUBSCRIBE) {
               // if subscribe, commit client filter
               m_currentCommand.client->commitFilter();
            }
         }
         m_inProgress.notify_all();
      }
   }


   // handle notification from Picard

   void CBoostClientManager::handleNotif(uint8_t notifType, const ByteVector& payload) 
   {
      std::ostringstream prefix;
      prefix << "CBoostClientManager::handleNotif: type=" << (int)notifType;
      CBoostLog::logDump(prefix.str(), payload);

      {
         boost::mutex::scoped_lock guard(m_lock);

         Clients::iterator iter;
         for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
            if ((*iter)->isSubscribed(notifType)) {
               {
                  std::ostringstream msg;
                  msg << "CBoostClientManager::handleNotif: sending to "
                      << (*iter)->remoteName();
                  CBoostLog::log(msg.str());
               }
               CMuxOutput notif(NOTIFICATION, 0 /* id */, notifType, payload);
               (*iter)->write(notif.serialize());
               // TODO: handle write failure / exception ?
            }
         }
      }
   }


   // -------------------------------------------------------
   // internal (private) methods

   // removeClient is called from readMuxMessage
   void CBoostClientManager::removeClient(CBoostClient::pointer client) 
   {
      // Internal State: removeClient is called:
      // 1. on a client read error (so current command should be empty), or 
      // 2. on disconnect.
      //
      // If current command is set, that means there's a client command in
      // progress while the system is disconnecting
      
      if (m_currentCommand.client) {
         CBoostLog::log("Warning: current command is set while processing remove client");
         // m_currentCommand will be cleared after a timeout response is
         // returned to the client
         m_currentCommand.result = CLIENT_DISCONNECT;
         m_inProgress.notify_all();
      }

      {
         boost::mutex::scoped_lock guard(m_lock);
         
         Clients::iterator iter = std::find(m_clients.begin(), m_clients.end(), client);
         if (iter != m_clients.end()) {
            m_clients.erase(iter);
            // update the subscription filter now that the client is gone
            {
               bool changed = recomputeSubscribeFilter();
               if (changed) {
                  ByteVector payload(SUBSCRIBE_PARAMS_LENGTH);
                  filterToVector(m_filterUnion, payload);
                  CMuxMessage command(SUBSCRIBE, payload);

                  addCommand(CBoostClient::pointer(), command);
               }
            }
         }
      }
   }

   // Returns: whether the overall filter union changed
   bool CBoostClientManager::recomputeSubscribeFilter() 
   {
      bool changed = false;
      SubscriptionParams newParamsUnion;

      Clients::iterator iter;
      for (iter = m_clients.begin(); iter != m_clients.end(); ++iter) {
         newParamsUnion.filter |= (*iter)->getSubscription();
         newParamsUnion.unreliable |= (*iter)->getUnreliable();
      }

      changed = (newParamsUnion.filter != m_filterUnion.filter) || 
         (newParamsUnion.unreliable != m_filterUnion.unreliable);
      m_prevfilter = m_filterUnion; // save the filter in case there's an error 
      m_filterUnion = newParamsUnion;

      if (changed) {
         std::ostringstream msg;
         msg << "updating filter from " << std::hex << m_prevfilter.filter 
            << " to " << m_filterUnion.filter << ", unreliable from " 
            << m_prevfilter.unreliable << " to " << m_filterUnion.unreliable;
         CBoostLog::log(msg.str());
      }

      return changed;
   }

   // note: caller must lock
   void CBoostClientManager::sendResponse(CBoostClient::pointer client,
                                          const CMuxOutput& resp,
                                          const std::string& sender)
   {
      if (client) {   
         {
            std::ostringstream msg;
            msg << sender << ": sending to " << client->remoteName();
            CBoostLog::log(msg.str());
         }

         // send the serialized response
         client->write(resp.serialize());
         // TODO: handle write failure / exception ?
      }
   }

   void CBoostClientManager::commandTimeout(CBoostClient::pointer client,
                                            uint8_t cmdType, uint8_t respCode) 
   {
      std::ostringstream msg;
      msg << "CBoostClientManager::commandTimeout: cmd=" << (int)cmdType 
          << " resp=" << (int)respCode;
      CBoostLog::log(msg.str());

      // construct the response
      ByteVector dummy;
      CMuxOutput resp(cmdType, 0 /* id */, respCode, dummy);

      sendResponse(client, resp, "CBoostClientManager::commandTimeout");

      // if Picard isn't responding, disconnect
      resetConnection();
   }

} // namespace DustSerialMux
