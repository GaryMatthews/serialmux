/* 
 * Picard Serial Multiplexer
 * 
 * Copyright (c) 2011, Dust Networks Inc. 
 */

#include "Version.h"
#include <stdio.h>

SMuxVersion getVersion()
{
   SMuxVersion version;
   version.major = VER_MAJOR;
   version.minor = VER_MINOR;
   version.release = VER_PATCH;
   version.build = VER_BUILD;

   return version;
}

const char* getVersionString() 
{
   static char version[64];
   SMuxVersion v = getVersion();
#ifdef _WIN32
   _snprintf_s(version, sizeof(version), "Serial Mux v%d.%d.%d (build %d)", 
               v.major, v.minor, v.release, v.build);
#else
   snprintf(version, sizeof(version), "Serial Mux v%d.%d.%d (build %d)", 
            v.major, v.minor, v.release, v.build);
#endif
   return version;
}

