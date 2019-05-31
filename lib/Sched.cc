/**
 * @file Sched.cc
 *
 * Imlpementation of event scheduler
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

#include <string>
#include "Sched.hh"

namespace Sched {
    enum EventDataType {
        NONE,
        UINT,
        DBLE,
        VOID_PTR
    };

    struct EventData {
        union {
            uint   u;
            double d;
            void   *v;
        };

        EventData() {}
        EventData(uint u)   : u(u) {}
        EventData(double d) : d(d) {}
        EventData(void *v)  : v(v) {}
    };

    union Callback {
        NoneCallback n;
        UintCallback u;
        DbleCallback d;
        VoidPtrCallback v;
    };

    /**
     * A scheduled event
     */
    struct Event {
        double        time;
        EventDataType type;
        Callback      cb;
        EventData     data;
        Event         *next;

        Event(
            double time,
            EventDataType type,
            Callback cb,
            EventData data)
            : time(time),
              type(type),
              cb(cb), 
              data(data), 
              next(NULL)
        {}
    };

    /**
     * List of scheduled events.
     */
    static Event *nextEvent = NULL;

    /**
     * Schedule an event
     * @param time Time for which event will be scheduled
     * @param cb Callback function
     * @param data Event Data
     */
    void scheduleEvent(
        double time, 
        EventDataType type,
        Callback cb,
        EventData data)
    {
        Event *newEv = new Event(time, type, cb, data);

        // Find event after which to insert new event
        //
        Event *prevEv = nextEvent; 
        while (prevEv != NULL && 
               prevEv->next != NULL
               && prevEv->next->time <= time)
        {
            prevEv = prevEv->next;
        }

        // Insert it
        //
        if (prevEv == NULL) {
            newEv->next = nextEvent;
            nextEvent = newEv;
        } else {
            newEv->next = prevEv->next;
            prevEv->next = newEv;
        }
    }

    void scheduleEvent(
        double time, 
        NoneCallback ncb)
    {
        Callback cb;
        cb.n = ncb;
        scheduleEvent(time, NONE, cb, EventData());
    }

    void scheduleEvent(
        double time, 
        UintCallback ucb,
        uint data)
    {
        Callback cb;
        cb.u = ucb;
        EventData d;
        d.u = data;
        scheduleEvent(time, UINT, cb, d);
    }

    void scheduleEvent(
        double time, 
        DbleCallback dcb,
        double data)
    {
        Callback cb;
        cb.d = dcb;
        EventData d;
        d.d = data;
        scheduleEvent(time, DBLE, cb, d);
    }

    void scheduleEvent(
        double time, 
        VoidPtrCallback vcb,
        void *data)
    {
        Callback cb;
        cb.v = vcb;
        EventData d;
        d.v = data;
        scheduleEvent(time, VOID_PTR, cb, d);
    }
            
    /**
     * Clear all scheduled events
     */
    void clearEvents()
    {
        for (Event *ev = nextEvent; ev != NULL; ev = ev->next) {
            delete ev;
        }
        nextEvent = NULL;
    }

    /**
     * Process a scheduled event.
     */
    static void processEvent(Event *event, double now)
    {
        switch(event->type) {
            case NONE:
                event->cb.n(event->time, now);
                break;
            case UINT:
                event->cb.u(event->time, now, event->data.u);
                break;
            case DBLE:
                event->cb.d(event->time, now, event->data.d);
                break;
            case VOID_PTR:
                event->cb.v(event->time, now, event->data.v);
                break;
            default:
                abort();
        }
    }

    /**
     * Process all events scheduled at or before the specified time
     */
    void processEvents(double now)
    {
        while (nextEvent != NULL && nextEvent->time <= now) {
            Event *ev = nextEvent;
            processEvent(ev, now);
            nextEvent = ev->next;
            delete ev;
        }
    }
}
