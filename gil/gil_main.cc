/**
 * gil_main.cc
 *
 * gil main program
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "Trace.hh"
#include "Util.hh"
#include "Gillespie.hh"

const uint UNSPECIFIED = UINT_MAX;

// A few abbreviations

const int NONE = Util::OPTARG_NONE;
const int INT  = Util::OPTARG_INT;
const int UINT = Util::OPTARG_UINT;
const int DBLE = Util::OPTARG_DBLE;
const int STR  = Util::OPTARG_STR;

// Execution control parameters
double stopTime        = 1.0;
char   *monitorId      = NULL;
double monitorThresh   = -DBL_MAX;
double monitorDelay    = 0.0;
uint   numPlotPoints   = 1000;
bool   help            = false;
bool   verbose         = false;
const char *traceLevel = "warn";

int main(int argc, char *argv[])
{
    char *pname = argv[0];

    std::vector<Util::ParseOptSpec> optSpecs = {
        // Execution control
        { "stop",     DBLE, &stopTime,      "stopTime"                            },
        { "mid",      STR,  &monitorId,     "monitorId"                           },
        { "mthresh",  DBLE, &monitorThresh, "monitorThreshold"                    },
        { "mdelay",   DBLE, &monitorDelay,  "monitorDelay"                        },
        // Output control
        { "npp",      UINT, &numPlotPoints, "numPlotPoints", "(use 0 for 'all')"  },
        { "t",        STR,  &traceLevel,    "traceLevel"                          },
        { "verbose",  NONE, &verbose,       "",              "print formulas"     },
        { "help",     NONE, &help,          "",                                   }};

    if (Util::parseOpts(argc, argv, optSpecs) != 0 ||
        optind != argc - 1 ||
        help) 
    {
        std::vector<string>nonFlags = { "<fileName>" };
        Util::usage(parseOptsUsage(pname, optSpecs, true, nonFlags).c_str(), NULL);
        fprintf(stderr, "Note:\n"
                "When <monitorThreshold> is specified, the simulation will run until the\n"
                "<monitorId> molecule passes through <monitorThreshold> (in\n"
                "either direction), and then continue for <monitorDelay> ticks\n"
                "or until <stopTime> is reached, whichever happens first.\n");
	exit(EXIT_FAILURE);
    }

    const char *fname = argv[optind];

    if (!Trace::setTraceLevel(traceLevel)) {
        Util::usageExit(parseOptsUsage(pname, optSpecs, true).c_str(), NULL);
    }
    Gillespie g(fname);
    if (verbose) {
        g.printMolecules();
        putchar('\n');
        g.printReactions();
        putchar('\n');
    }
    double plotInterval = 0.0;
    if (numPlotPoints != 0) { // 0 means "all"
        plotInterval = stopTime / numPlotPoints;
    }
    g.run(plotInterval, stopTime, monitorId, monitorThresh, monitorDelay);
}
