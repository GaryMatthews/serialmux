/*
 * Copyright (c) 2010, Dust Networks Inc. 
 */
#pragma once

#ifndef HDLC_H_
#define HDLC_H_

/*
 * HDLC Parser and Generator
 */
#include <stdint.h>
#include <vector>


/**
 * HDLC packet generator
 */
std::vector<unsigned char> encodeHDLC(const std::vector<uint8_t>& src);

/**
 * HDLC FCS calculation
 */
uint16_t computeFCS16(const std::vector<uint8_t>& data);

// TODO: needs a better name
class IHDLCParser {
public:
   virtual void frameComplete(const std::vector<uint8_t>& packet) = 0;
};


// TODO: I don't know how to make this part of the CHDLC class
//enum ParseState {
//   HDLC_PACKET_COMPLETE,
//   HDLC_DATA,
//   HDLC_ESCAPE,
//};
//#define HDLC_PADDING         0x7E
//#define HDLC_ESCCHAR         0x7D
//#define HDLC_XORBYTE         0x20
//#define CRC_MAGIC_NUMBER     0xF0B8

class CHDLC {
   enum ParseState {
      HDLC_PACKET_COMPLETE,
      HDLC_DATA,
      HDLC_ESCAPE,
   };

public:
   CHDLC(int inputLength, IHDLCParser* handler) 
      : m_buffer(inputLength),
        m_handler(handler)
   { reset(); }

   void addByte(uint8_t b);

private:
   bool validateChecksum(uint16_t frameFcs);
   void append(uint8_t byte);
   void callback();
   void reset();

   IHDLCParser* m_handler;

   ParseState   m_state;
   std::vector<uint8_t> m_buffer;
   uint32_t     m_runningFCS;
};


#endif /* ! HDLC_H_ */
