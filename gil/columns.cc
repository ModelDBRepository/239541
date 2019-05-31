/**
 * @file columns.cc
 *
 * Copy selected columns from input to output
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

#include <unistd.h>
#include <vector>
#include <string>
using std::string;
#include <format.h>
#include "Util.hh"
#include "Trace.hh"

// A few abbreviations

const int NONE = Util::OPTARG_NONE;
const int INT  = Util::OPTARG_INT;
const int UINT = Util::OPTARG_UINT;
const int DBLE = Util::OPTARG_DBLE;
const int STR  = Util::OPTARG_STR;

bool   help            = false;
const char *traceLevel = "warn";

static const char *fname    = NULL;  // default is stdin
static const char *sepChars = " \t"; // input file separator chars
static const char *osep = "\t";      // output separator

/**
 * Print error message and exit
 * @param file File name
 * @param line Line number
 * @param format fmt::print format string
 * @param args Additional arguments to fmt::print
 */
template <typename... T>
void fail(string file, uint line, const char *format, const T & ... args)
{
    fmt::print(stderr, "File {}, line {}: ", file, line);
    fmt::print(stderr, format, args...);
    fmt::print(stderr, "\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    char *pname = argv[0];

    // Process command line args

    std::vector<Util::ParseOptSpec> optSpecs = {
        { "file",     STR,  &fname,                 "file_name" },
        { "sep",      STR,  &sepChars,              "input_separator_chars" },
        { "osep",     STR,  &sepChars,              "output_separator_string" },
        { "t",        STR,  &traceLevel,            "trace_level" },
        { "help",     NONE, &help,                  }};

    if (parseOpts(argc, argv, optSpecs) != 0 ||
        optind == argc ||
        !Trace::setTraceLevel(traceLevel) ||
        help) 
    {
        std::vector<string> nonFlags = { "column_name [column_name ...]" };
        Util::usageExit(
            parseOptsUsage(
                pname, optSpecs, true,
                nonFlags).c_str(), NULL);
    }

    std::vector<string> selectedColumns;
    while (optind < argc) {
        selectedColumns.push_back(argv[optind++]);
    }

    // Open the input file
    //
    FILE *fp = stdin;
    if (fname == NULL) {
        fname = "<stdin>"; // for diagnostics only
    } else {
        fp = fopen(fname, "r");
        if (fp == NULL) {
            perror(fname);
            exit(errno);
        }
    } 
    
    // Parse the header line
    //
    const uint LINELEN = 2048;
    char line[LINELEN];
    uint lineNum = 1;
    if (fgets(line, LINELEN, fp) == NULL) {
        fmt::print(stderr, "{}: failed to read header line\n", fname);
        exit(errno);
    }
    Util::chop(line);
    string errMsg;
    std::vector<string> headers = Util::tokenize(line, sepChars, errMsg);
    if (!errMsg.empty()) {
        fail(fname, lineNum, "{}", errMsg);
    }

    // Determine which columns to copy
    //
    std::vector<uint> columnNumbers;
    for (auto c : selectedColumns) {
        uint i = 0;
        for (; i < headers.size(); i++) {
            if (Util::strCiEq(c, headers[i])) {
                columnNumbers.push_back(i);
                break;
            }
        }
        if (i == headers.size()) {
            fail(fname, lineNum, "{}: column not found", c);
        }
    }

    // Copy the selected columns of the header line
    //
    for (uint i = 0; i < columnNumbers.size(); i++) {
        fmt::print("{}", headers[columnNumbers[i]]);
        if (i < columnNumbers.size() - 1) {
            fmt::print("{}", osep);
        }
    }
    fmt::print("\n");
                   
    // Read the rest of the file and copy the
    // selected columns in the specified order
    //
    while (fgets(line, LINELEN, fp) != NULL) {
        Util::chop(line);
        lineNum++;
        std::vector<string> tokens = Util::tokenize(line, sepChars, errMsg);
        if (!errMsg.empty()) {
            fail(fname, lineNum, "{}", errMsg);
        }
        if (tokens.size() != headers.size()) {
            fail(fname, lineNum, "Expected {} columns, found {}",
                 headers.size(), tokens.size());
        }
        for (uint i = 0; i < columnNumbers.size(); i++) {
            fmt::print("{}", tokens[columnNumbers[i]]);
            if (i < columnNumbers.size() - 1) {
                fmt::print("{}", osep);
            }
        }
        fmt::print("\n");
    }
}
