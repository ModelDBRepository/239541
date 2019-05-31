/**
 * Mat - Add, mul, div, min, max or average matrices read from files, or
 *       compute standard deviation or standard error. Files contain equal
 *       numbers of rows and cols of float or int numbers, optionally with a
 *       header row.
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

#include <stdlib.h>
#include <unistd.h>
#include <set>

#include <format.h>
#include "Util.hh"



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

static bool hasIndex = false;
static const char *sepChars = " \t";
static const char *prefix = "";
static const char *suffix = "";

/**
 * Adorn each token in line with prefix and suffix
 */
string adorn(string line)
{
    string errMsg;
    std::vector<string> tokens =
        Util::tokenize(line, sepChars, errMsg);
    ABORT_IF(!errMsg.empty(), "%s", errMsg.c_str());

    string r;
    bool first = true;
    for (auto tok : tokens) {
        if (first) {
            first = false;
        } else {
            r += sepChars[0];
        }
        r += prefix;
        r += tok;
        r += suffix;
     }

    return r;
}


int main(int argc, char *argv[])
{
    char *pname = argv[0];
    bool help = false;
    bool hasHdr = false;
    bool chkHdr = false;

    // Process command line args
    //
    std::vector<Util::ParseOptSpec> optSpecs = {
        {"hdr",     Util::OPTARG_NONE, &hasHdr,   "", "files have header row"},
        {"chkHdr",  Util::OPTARG_NONE, &chkHdr,   "", "headers must match"},
        {"index",   Util::OPTARG_NONE, &hasIndex, "", "files have index column"},
        {"sep",     Util::OPTARG_STR,  &sepChars, "separator_chars"},
        {"prefix",  Util::OPTARG_STR,  &prefix,   "output_header_prefix"},
        {"suffix",  Util::OPTARG_STR,  &suffix,   "output_header_suffix"},
        {"help",    Util::OPTARG_NONE, &help,     }};

    std::vector<string> nonFlags =
        { "{add|sub|mul|div|min|max|avg|stdevp|stdevs|sterr} <file> ..." };
    if (Util::parseOpts(argc, argv, optSpecs) != 0 ||
        optind > argc - 2 || help)
    {
        Util::usage(
            parseOptsUsage(pname, optSpecs, true, nonFlags).c_str(), NULL);
        exit(EXIT_FAILURE);
    }

    uint nCols = 0;
    uint nRows = 0;

    // The first non-flag arg is the operation
    string opStr = argv[optind++];
    Util::Operation op;
    if      (Util::strCiEq(opStr, "ADD"))    { op = Util::ADD; }
    else if (Util::strCiEq(opStr, "SUB"))    { op = Util::SUB; }
    else if (Util::strCiEq(opStr, "MUL"))    { op = Util::MUL; }
    else if (Util::strCiEq(opStr, "DIV"))    { op = Util::DIV; }
    else if (Util::strCiEq(opStr, "MIN"))    { op = Util::MIN; }
    else if (Util::strCiEq(opStr, "MAX"))    { op = Util::MAX; }
    else if (Util::strCiEq(opStr, "AVG"))    { op = Util::AVG; }
    else if (Util::strCiEq(opStr, "STDEVP")) { op = Util::STDEVP; }
    else if (Util::strCiEq(opStr, "STDEVS")) { op = Util::STDEVS; }
    else if (Util::strCiEq(opStr, "STERR"))  { op = Util::STERR; }
    else {
        Util::usage(
            parseOptsUsage(pname, optSpecs, false, nonFlags).c_str(), NULL);
        exit(EXIT_FAILURE);
    }

    // The remaining args are file names. Process them one by one
    // and build the result matrix as we go. For STDEV and STDERR,
    // also build sqsum on the fly.

    uint numFiles = argc - optind;
    string firstFile;
    string hdr;

    vector<vector<double>> firstMat;
    vector<vector<double>> result;
    vector<vector<double>> sqsum;

    while (optind < argc) {
        const char *fname = argv[optind++];
        if (firstFile.empty()) {
            firstFile = fname;
        }
        FILE *fp = fopen(fname, "r");
        if (fp == NULL) {
            perror(fname);
            return 1;
        }

        vector<vector<double>> mat;

        uint lineNum = 0;
        char line[1024];

        // Read a matrix of doubles from the file

        while ((fgets(line, sizeof(line), fp) != NULL)) {
            Util::chop(line);
            if (++lineNum == 1) {
                if (hasHdr) {
                    // This is a header line
                    if (hdr.empty()) {
                        hdr = line;
                        fmt::print("{}\n", adorn(hdr));
                    } else {
                        ABORT_IF(chkHdr && (hdr != line),
                                 "%s and %s have different headers",
                                 firstFile.c_str(), fname);
                    }
                    continue;
                }
            }
            string errMsg;
            std::vector<string> tokens =
                Util::tokenize(line, sepChars, errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum, "{}", errMsg);
            }

            if (tokens.size() == 0) {
                fail(fname, lineNum, "empty line", errMsg);
            }

            if (nCols == 0) {
                nCols = tokens.size();
            } else if (tokens.size() != nCols) {
                fail(fname, lineNum, "Expected {} tokens, found {}",
                     nCols, tokens.size());
            }

            vector<double> row;
            for (auto tok : tokens) {
            
                size_t sz;
                try {
                    row.push_back(std::stod(tok, &sz));
                } catch (std::invalid_argument) {
                    fail(fname, lineNum, "Bad double [{}]", tok);
                }
            }
            mat.push_back(row);
        }
    
        fclose(fp);

        if (nRows == 0) {
            // First file read
            nRows = mat.size();
            result = mat;
            firstMat = mat;
            sqsum = Util::matrixSquare(mat);
        } else {
            if (mat.size() != nRows) {
                fail(fname, lineNum, "Expected {} rows, found {}",
                     nRows, mat.size());
            }

            if (hasIndex) {
                for (uint r = 0; r < nRows; r++) {
                    if (mat[r][0] != firstMat[r][0]) {
                        fail(fname, hasHdr ? r+2 : r+1, "Index differs from file {}", firstFile);
                    }
                }
            }
            
            switch (op) {
                case Util::STDEVP:
                case Util::STDEVS:
                case Util::STERR:
                    sqsum = Util::matrixAdd(sqsum, Util::matrixSquare(mat));
                    // fall thru
                case Util::ADD:
                case Util::AVG:
                    result = Util::matrixAdd(result, mat);
                    break;
                case Util::SUB:
                    result = Util::matrixSub(result, mat);
                    break;
                case Util::MUL:
                    result = Util::matrixMul(result, mat);
                    break;
                case Util::DIV:
                    result = Util::matrixDiv(result, mat);
                    break;
                case Util::MIN:
                    result = Util::matrixMin(result, mat);
                    break;
                case Util::MAX:
                    result = Util::matrixMax(result, mat);
                    break;
                default:
                    TRACE_FATAL("Bad operation");
            }
        }
    }
    switch (op) {
        case Util::AVG:
            for (auto &row : result) {
                for (auto &elem : row) {
                    elem /= numFiles;
                }
            }
            break;
        case Util::STDEVP:
        case Util::STDEVS:
        case Util::STERR:
            // For standard deviation or error, calculate the standard
            // deviation of the sample
            for (uint r = 0; r < nRows; r++) {
                for (uint c = 0; c < nCols; c++) {
                    // stdev = sqrt(sum-of-squares/n - (square-of-sum/n)^2)
                    result[r][c] = sqrt(sqsum[r][c] / numFiles -
                                        pow((result[r][c] / numFiles), 2.0));
                }
            }
            // For sample statistics, apply Bessel's correction
            if ((numFiles > 1) && (op == Util::STDEVS || op == Util::STERR)) {
                result = Util::matrixMul(
                    result, sqrt(1. * numFiles/(numFiles - 1)));
            }
            // For standard error, divide by sqrt of sample size
            if (op == Util::STERR) {
                result = Util::matrixDiv(result, sqrt(numFiles));
            }
            break;
        default:
            // no special processing
            ;
    }

    if (hasIndex) {
        for (uint r = 0; r < nRows; r++) {
            result[r][0] = firstMat[r][0];
        }
    }        

    fmt::print(Util::matrixToStr(result));
}

