/**
 * @file Trace.hh
 *
 * Tracing utility
 * 
 * Copyright (c) 2016 - 2018, Peter Helfer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TRACE_HH
#define TRACE_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/**
 * Trace class
 */
class Trace
{
public:
    enum TraceLevel {
        TRACE_Flow,
        TRACE_Debug3,
        TRACE_Debug2,
        TRACE_Debug1,
        TRACE_Debug,
        TRACE_Info,
        TRACE_Warn,
        TRACE_Error,
        TRACE_Fatal,

        TRACE_Maxval
    };

    static void setTraceLevel(TraceLevel level)
    {
        traceLevel = level;
    }

    static bool setTraceLevel(const char *levelString)
    {
        for (uint i = 0; i < TRACE_Maxval; i++) {
            if (strcasecmp(levelString, traceLevelString((TraceLevel) i)) == 0) {
                setTraceLevel((TraceLevel) i);
                return true;
            }
        }
        fprintf(stderr, "Unknown trace level '%s'\n", levelString);
        return false;
    }

    static TraceLevel getTraceLevel()
    {
        return traceLevel;
    }

    static const char *getTraceLevelString()
    {
        return traceLevelString(traceLevel);
    }


#ifdef TRACE_ON
    #define TRACE(lvl, fmt, ...) \
        do { \
            if (lvl >= Trace::getTraceLevel()) { \
                Trace::trace(lvl, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
            } \
        } while(0);

    #define TRACE_FLOW_IS_ON    (Trace::TRACE_Flow   >= Trace::getTraceLevel())
    #define TRACE_DEBUG3_IS_ON  (Trace::TRACE_Debug3 >= Trace::getTraceLevel())
    #define TRACE_DEBUG2_IS_ON  (Trace::TRACE_Debug2 >= Trace::getTraceLevel())
    #define TRACE_DEBUG1_IS_ON  (Trace::TRACE_Debug1 >= Trace::getTraceLevel())
    #define TRACE_DEBUG_IS_ON   (Trace::TRACE_Debug  >= Trace::getTraceLevel())
    #define TRACE_INFO_IS_ON    (Trace::TRACE_Info   >= Trace::getTraceLevel())
    #define TRACE_WARN_IS_ON    (Trace::TRACE_Warn   >= Trace::getTraceLevel())
    #define TRACE_ERROR_IS_ON   (Trace::TRACE_Error  >= Trace::getTraceLevel())
#else
    #define TRACE(lvl, fmt, ...)
    #define TRACE_FLOW_IS_ON    false
    #define TRACE_DEBUG3_IS_ON  false
    #define TRACE_DEBUG2_IS_ON  false
    #define TRACE_DEBUG1_IS_ON  false
    #define TRACE_DEBUG_IS_ON   false
    #define TRACE_INFO_IS_ON    false
    #define TRACE_WARN_IS_ON    false
    #define TRACE_ERROR_IS_ON   false
#endif

#define TRACE_FLOW(fmt, ...)  TRACE(Trace::TRACE_Flow,    fmt, ##__VA_ARGS__)
#define TRACE_DEBUG3(fmt, ...) TRACE(Trace::TRACE_Debug3, fmt, ##__VA_ARGS__)
#define TRACE_DEBUG2(fmt, ...) TRACE(Trace::TRACE_Debug2, fmt, ##__VA_ARGS__)
#define TRACE_DEBUG1(fmt, ...) TRACE(Trace::TRACE_Debug1, fmt, ##__VA_ARGS__)
#define TRACE_DEBUG(fmt, ...) TRACE(Trace::TRACE_Debug,   fmt, ##__VA_ARGS__)
#define TRACE_INFO(fmt, ...)  TRACE(Trace::TRACE_Info,    fmt, ##__VA_ARGS__)
#define TRACE_WARN(fmt, ...)  TRACE(Trace::TRACE_Warn,    fmt, ##__VA_ARGS__)
#define TRACE_ERROR(fmt, ...) TRACE(Trace::TRACE_Error,   fmt, ##__VA_ARGS__)
#define TRACE_FATAL(fmt, ...)                                           \
    do {                                                                \
        Trace::trace(Trace::TRACE_Fatal, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
        abort(); \
    } while (0)
// TRACE_FATAL will always abort the process, even when compiled without TRACE_ON

/**
  * Abort if a condition is true. Unlike assert(3), ABORT_IF cannot be
  * disabled by a compile option like -DNDEBUG.
  * @param cond The condition
  * @param fmt printf-style format string for error message
  * @param ... Optional parameters values for error message
  */
#define ABORT_IF(cond, fmt, ...) \
    if (cond) {                  \
        TRACE_DEBUG("Aborting because: %s", #cond);     \
        TRACE_FATAL(fmt, ##__VA_ARGS__);    \
    }

#define ABORT_UNLESS(cond, fmt, ...) \
    ABORT_IF(!(cond), fmt, ##__VA_ARGS__)

#define TRACE_ENTER()   do { TRACE_FLOW("-->"); Trace::incrIndent(); } while (0);
#define TRACE_EXIT()    do { Trace::decrIndent(); TRACE_FLOW("<--"); } while (0);
#define TRACE_RETURN(x) do { TRACE_EXIT(); return x; } while (0)
     
    static void trace(TraceLevel lvl,
                      const char *file,
                      int line,
                      const char *func,
                      const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        FILE *dest = lvl >= TRACE_Warn ? stderr : stdout;
        fprintf(dest, "%s%s %s[%d] %s(): ",
                indentStr(), traceLevelString(lvl), file, line, func);
        vfprintf(dest, fmt, ap);
        fprintf(dest, "\n");
        va_end(ap);
    }
    
    static void incrIndent()
    {
        if (indentLevel < MAXINDENT) indentLevel++;
    }

        static void decrIndent()
    {
        if (indentLevel > 0) indentLevel--;
    }
    
    static const char *traceLevelString(TraceLevel lvl)
    {
        switch (lvl) {
            case TRACE_Flow:   return "FLOW";
            case TRACE_Debug3: return "DEBUG3";
            case TRACE_Debug2: return "DEBUG2";
            case TRACE_Debug1: return "DEBUG1";
            case TRACE_Debug:  return "DEBUG";
            case TRACE_Info:   return "INFO";
            case TRACE_Warn:   return "WARN";
            case TRACE_Error:  return "ERROR";
            case TRACE_Fatal:  return "FATAL";
            default:           return "UNKNOWN TRACE LEVEL";
        }
    }

private:
    static TraceLevel traceLevel;
    static uint indentLevel;
    
    enum { MAXINDENT = 128 };
    
    static const char *indentStr()
    {
        if (traceLevel <= TRACE_Flow) {
            static char buf[MAXINDENT + 1];
            static bool firstTime = true;
            if (firstTime) {
                for (uint i = 0; i < MAXINDENT; i++) {
                    buf[i] = ' ';
                }
                buf[MAXINDENT] = 0;
                firstTime = false;
            }
            return buf + MAXINDENT - indentLevel;
        } else {
            return "";
        }
    }
};

#endif // TRACE_HH
