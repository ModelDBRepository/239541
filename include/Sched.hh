/**
 * @file Sched.hh
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

#ifndef SCHED_HH
#define SCHED_HH

#include "Util.hh"

/**
 * Event scheduler
 */
namespace Sched {
    /**
     * Callback function signatures
     */
    typedef void (*NoneCallback)(double scheduledTime,
                                 double currentTime);

    typedef void (*UintCallback)(double scheduledTime,
                                 double currentTime,
                                 uint data);

    typedef void (*DbleCallback)(double scheduledTime,
                                 double currentTime,
                                 double data);

    typedef void (*VoidPtrCallback)(double scheduledTime,
                                 double currentTime,
                                 void *data);
    /**
     * Schedule an event
     * @param time Time for which event will be scheduled
     * @param cb Callback function
     * @param data Will be passed as parameter to cb
     */
    void scheduleEvent(
        double time, 
        NoneCallback cb);

    void scheduleEvent(
        double time, 
        UintCallback cb,
        uint data);

    void scheduleEvent(
        double time, 
        DbleCallback cb,
        double data);

    void scheduleEvent(
        double time, 
        VoidPtrCallback cb,
        void *data);

    /**
     * Clear all scheduled events
     */
    void clearEvents();

    /**
     * Process all events scheduled to run at or before the specified time
     */
    void processEvents(double time);
};

#endif
