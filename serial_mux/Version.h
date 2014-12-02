#ifndef SerialMux_Version_H_
#define SerialMux_Version_H_

#include <stdint.h>
#include "Build.h"
enum {
   VER_MAJOR = 1,
   VER_MINOR = 1,
   VER_PATCH = 2,
};


struct SMuxVersion 
{
   uint8_t  major;
   uint8_t  minor;
   uint8_t  release;
   uint16_t build;
};

SMuxVersion getVersion();

const char* getVersionString();

#endif
