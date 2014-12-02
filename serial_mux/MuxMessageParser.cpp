/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#include "MuxMessageParser.h"

#include <iterator>
#include <algorithm>

using namespace DustSerialMux;


CMuxMessage::CMuxMessage(const ByteVector& data)
   : m_type(0), m_id(0), m_data(data.size() >= 3 ? data.size() - 3 : 0)
{
   if (data.size() >= 3) {
      m_id = ((uint16_t)data[0] << 8) | data[1];
      m_type = data[2];
      std::copy(data.begin() + 3, data.end(), m_data.begin());
   }
}

ByteVector CMuxMessage::serialize() const 
{
   ByteVector output;
   // magic token
   std::copy(MAGIC_TOKEN, MAGIC_TOKEN + sizeof(MAGIC_TOKEN), std::back_inserter(output));
   // length
   uint16_t len = m_data.size() + 3;
   output.push_back((len & 0xFF00) >> 8);
   output.push_back(len & 0xFF);
   // id
   output.push_back((id() & 0xFF00) >> 8);
   output.push_back(id() & 0xFF);
   // type
   output.push_back(type());
   // payload
   std::copy(m_data.begin(), m_data.end(), std::back_inserter(output));
   return output;
}


// -------------------------------------------------------------
// Mux Response 

CMuxOutput::CMuxOutput(uint8_t cmdType, uint16_t id, uint8_t prefix, ByteVector payload)
   : m_data(payload.size() + 4)
{
   int index = 0;
   // id
   m_data[index++] = (id & 0xFF00) >> 8;
   m_data[index++] = (id & 0xFF);
   // type
   m_data[index++] = cmdType;
   // the prefix is the response code or notification type
   m_data[index++] = prefix;
   // payload
   std::copy(payload.begin(), payload.end(), m_data.begin() + index);
}

ByteVector CMuxOutput::serialize() const 
{
   ByteVector output;
   // magic token
   std::copy(MAGIC_TOKEN, MAGIC_TOKEN + sizeof(MAGIC_TOKEN), std::back_inserter(output));
   // length
   uint16_t len = m_data.size();
   output.push_back((len & 0xFF00) >> 8);
   output.push_back(len & 0xFF);
   // id + type + response code + payload
   std::copy(m_data.begin(), m_data.end(), std::back_inserter(output));
   return output;
}


// -------------------------------------------------------------
// Command Parser

CMuxParser::CMuxParser(ICommandCallback* handler) 
  : m_handler(handler),
    m_partialInput(COMMAND_SIZE)
{
   // intentionally blank
}


/**
 * Command parser read/parse methods
 */

void CMuxParser::read(const ByteVector& input)
{
   // first, copy all of the new input
   std::copy(input.begin(), input.end(), back_inserter(m_partialInput));
   // then, parse everything in the unparsed input buffer
   while (parse()) ; // parse until no more commands are found
}

bool CMuxParser::parse()
{
   // scan for the first magic token
   ByteVector::iterator iter;
   iter = std::search(m_partialInput.begin(), m_partialInput.end(), 
                      MAGIC_TOKEN, MAGIC_TOKEN + sizeof(uint32_t));

   if (iter != m_partialInput.end()) {
      iter += sizeof(uint32_t);
      // if there's not enough data to read the length, wait for the next input
      if (m_partialInput.end() - iter < sizeof(uint16_t)) {
         return false;
      }

      int expectedLength = (*iter++)*256;
      expectedLength += (*iter++);

      // TODO: verify the length
      
      // check whether we have a whole command
      int remaining = (m_partialInput.end() - iter);
      if (remaining >= expectedLength) {
         ByteVector cmdData(expectedLength);
         ByteVector::iterator endIter = iter + expectedLength;
         std::copy(iter, endIter, cmdData.begin());
         callback(cmdData);
         m_partialInput.erase(m_partialInput.begin(), endIter);
         return true;
      }
   } else if (m_partialInput.size() > 3) {
      // if no token is found, discard all but the last three bytes
      m_partialInput.erase(m_partialInput.begin(), m_partialInput.end()-3);
   }
   return false;
}

void CMuxParser::callback(const ByteVector& command)
{
   // call the handler
   if (m_handler) {
      CMuxMessage cmd(command); 
      m_handler->handleCommand(cmd);
   }
}
