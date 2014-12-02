/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#ifndef MuxMessageParser_H_
#define MuxMessageParser_H_

#pragma once

#include <stdint.h>
#include <vector>


namespace DustSerialMux {

   const uint8_t MAGIC_TOKEN[] = { 0xa7, 0x40, 0xa0, 0xf5 };

   typedef std::vector<uint8_t> ByteVector;

   /**
    * MuxMessage is a structure for passing around parsed responses or 
    * notifications received from the Mux
    */
   class CMuxMessage
   {
      static const int MAX_COMMAND_LEN = 256;
   public:
      CMuxMessage() : m_type(0), m_id(0) { ; }
      explicit CMuxMessage(uint8_t inType, const ByteVector& inData)
         : m_type(inType), m_data(inData.size()), m_id(0)
      {
         std::copy(inData.begin(), inData.end(), m_data.begin());
      }

      explicit CMuxMessage(const ByteVector& data);

      ByteVector serialize() const;

      void clear() { m_type = 0; m_data.clear(); m_id = 0; }

      uint16_t size() const { return m_data.size(); }
      uint8_t  type() const { return m_type; }
      uint16_t id()   const { return m_id; }

      ByteVector  m_data;
   private:
      uint8_t m_type;
      uint16_t m_id;
   };


   /**
    * MuxOutput turns a typed output payload into a serialized byte stream
    * TODO: not sure if it's worth using a class for this
    */
   class CMuxOutput {
   public:
      CMuxOutput(uint8_t cmdType, uint16_t id, uint8_t prefix, ByteVector payload);

      ByteVector serialize() const;
   private:
      ByteVector  m_data;
   };

   /**
    * CommandCallback is the interface that receives a message parsed from 
    * the Mux input stream
    */
   class ICommandCallback {
   public:
      virtual void handleCommand(const CMuxMessage& command) = 0;
   };

   /** 
    * MuxParser parses data read from the Mux input and calls 
    * the CommandCallback when a complete message is received. 
    */
   class CMuxParser
   {
      static const int COMMAND_SIZE = 128;

   public:
      CMuxParser(ICommandCallback* handler);

      void read(const ByteVector& input);

   private:
      bool parse();
      void callback(const ByteVector& command);
         
      ICommandCallback* m_handler;

      ByteVector m_partialInput;
   };

};

#endif /* ! MuxMessageParser_H_ */
