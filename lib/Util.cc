/**
 * @file Util.cc
 *
 * Implementation of miscellaneous utilities
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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <string>
#include <sys/time.h>
#include <pthread.h>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <tinyexpr.h>
#include <math.h>

#include "Util.hh"
#include "Trace.hh"



namespace Util {

#ifdef UTIL_THREADED
    static int randSeed = 0;

/**
 * Obtain a unique random seed for the current thread.
 * Deterministic if initRand has not beeen called: first
 * caller gets 0, next caller gets 1, etc.
 */
    static int getSeed()
    {
        if (randSeed != 0) {
            return randSeed * pthread_self();
        } else {
            static int count = 0;
            static pthread_mutex_t count_mutex;
            pthread_mutex_lock(&count_mutex);
            int ret = count++;
            pthread_mutex_unlock(&count_mutex);
            return ret;
        }
    }

/**
 * Use the system clock's usecs to generate a seed
 * (If initRand has not been called, then each thread
 * will always get the same sequence of random numbers).
 */
    void initRand()
    {
        struct timeval now;
        gettimeofday(&now, 0);
        randSeed = now.tv_usec;
    }

/**
 * Generate a random integer in [min, max] inclusive
 */
    int randInt(int min, int max) {
        typedef boost::mt19937 RNGType;
        typedef boost::variate_generator< RNGType, boost::uniform_int<> > VGENType;

        static int firstCall = true;
        static pthread_key_t rng_key;    // key for the random number generator
        static pthread_key_t vgen_key;   // key for the variate generator
    
        if (firstCall) {
            int ret = pthread_key_create(&rng_key, NULL);
            ABORT_IF(ret != 0, "pthread_key_create returned %d", ret);
            ret = pthread_key_create(&vgen_key, NULL);
            ABORT_IF(ret != 0, "pthread_key_create returned %d", ret);
            firstCall = false;
        }

        static boost::uniform_int<> distr(0, RAND_MAX - 1);

        RNGType *rng = (RNGType *) pthread_getspecific(rng_key);
        if (rng == NULL) {
            rng = new RNGType(getSeed());
            pthread_setspecific(rng_key, rng);
        
            VGENType *vgen = new VGENType(*rng, distr);
            pthread_setspecific(vgen_key, vgen);
        }
    
        VGENType *vgen = (VGENType *) pthread_getspecific(vgen_key);
        int ret = int(min + (max - min + 1) * 1.0 * (*vgen)() / RAND_MAX);

        return ret;
    }
#else                                           \
    // Note: this version of randInt is not thread-safe
    // because it uses random(3).
    
    /**
     * Initialize rand
     */
    void initRand()
    {
        // Use the system clock's usecs to randomize random(3)
        //
        struct timeval now;
        gettimeofday(&now, 0);
        srandom(now.tv_usec);
    }

    /**
     * Generate a random integer in [min, max] inclusive
     */
    int randInt(int min, int max)
    {
        return min + (max - min + 1) * 1.0 * random() / RAND_MAX;
    }
#endif    

    /**
     * Create a random permutation of the integers min - max-1
     */
    vector<int> randPerm(int min, int max)
    {
        uint n = max - min;
        vector<int> v(n);
        for (uint i = 0; i < n; i++) {
            v[i] = min + i;
        }
        for (uint i = 0; i < n; i++) {
            int j = randInt(i, n - 1);
            swap(v[i], v[j]);
        }
        return v;
    }

    /**
     * Create a random set of n integers in the range [0, max] inclusive
     * May contain duplicates.
     */
    vector<int> randIntList(uint n, int max)
    {
        vector<int> v(n);
        for (uint i = 0; i < n; i++) {
            v[i] = randInt(0, max);
        }
        return v;
    }

    /**
     * Create a random set of n unique integers in the range [min, max-1]
     * No duplicates.
     */
    vector<int> randUniqueIntList(int n, int min, int max)
    {
        assert(n <= max - min);

        if (max - min < 100000) {
            // small range: permute [min, max-1] and return the initial n
            //
            vector<int> v = randPerm(min, max);
            v.resize(n);
            return v;
        } else {
            // Large range: pick random numbers from [min, max-1] until
            // we have n unique ones. (Only good if n << max.)
            //
            vector<int> v(n);
            for (int i = 0; i < n; ) {
                int m = randInt(min, max - 1);
                int j;
                for (j = 0; j < i; j++) {
                    if (v[j] == m) {
                        // already have it
                        break;
                    }
                }
                if (j == i) {
                    // didn't have it; add it
                    v[i++] = m;
                }
            }
            return v;
        }
    }

    /**
     * Generate a random double in the range [min, max] or (min, max)
     */
    double randDouble(double min, double max, bool open)
    {
        ABORT_IF(min >= max, "invalid interval: min=%g, max = %g", min, max);
        double r;
        uint count = 0;
        do {
            r = min + (max - min) * randInt(0, RAND_MAX - 1) / (RAND_MAX - 1);
            ABORT_IF(++count >1000, "min=%g, max = %g", min, max);
        } while (open && (r == min || r == max));
        return r;
    }

    /**
     * Create a random set of n doubles in the range [min, max] or (min, max)
     * May contain duplicates.
     */
    vector<double> randDoubleList(uint n, double min, double max, bool open)
    {
        vector<double> v(n);
        for (uint i = 0; i < n; i++) {
            v[i] = randDouble(min, max, open);
        }
        return v;
    }
    
    /**
     * Create a random set of n doubles from the provided domain
     * May contain duplicates.
     */
    vector<double> randDoubleList(uint n, vector<double> &domain)
    {
        vector<double> v(n);

        for (uint i = 0; i < n; i++) {
            v[i] = domain[randInt(0, domain.size() - 1)];
        }
        return v;
    }

    /**
     * Create a random set of n unique doubles from the provided domain
     * No duplicates.
     */
    vector<double> randUniqueDoubleList(uint n, vector<double> &domain)
    {
        assert(n <= domain.size());

        // permute [0, domain.size() - 1] and return the initial n
        //
        vector<int> indexes = randPerm(domain.size());
        vector<double> v(n);
        for (uint i = 0; i < n; i++) {
            v[i] = domain[indexes[i]];
        }
        return v;
    }

    /**
     * Create a string of length len where each character is
     * randomly selected from charset.
     * @param charset Set of characters to use
     * @param len Length of resulting string
     */
    string randStr(const char* charset, int len)
    {
        char buf[len+1];
        buf[len] = 0;
        for (int i = 0; i < len; i++) {
            buf[i] = charset[Util::randInt(0, strlen(charset) - 1)];
        }
        return buf;
    }

   /**
     * Print a usage message, optionally preceded by an error message.
     */
    void usage(const char *syntax, const char *fmt, ...)
    {
        if (fmt != NULL && strlen(fmt) > 0) {
            va_list ap;
            va_start(ap, fmt);
            fprintf(stderr, "ERROR: ");
            vfprintf(stderr, fmt, ap);
            fprintf(stderr, "\n");
            va_end(ap);
        }
        fprintf(stderr, "%s\n", syntax);
    }

   /**
     * Print a usage message, optionally preceded by an error message,
     * then exit with status EXIT_FAILURE.
     */
    void usageExit(const char *syntax, const char *fmt, ...)
    {
        if (fmt != NULL && strlen(fmt) > 0) {
            va_list ap;
            va_start(ap, fmt);
            fprintf(stderr, "ERROR: ");
            vfprintf(stderr, fmt, ap);
            fprintf(stderr, "\n");
            va_end(ap);
        }
        fprintf(stderr, "%s\n", syntax);
        exit(EXIT_FAILURE);
    }

    /**
     * Create a copy of a string with initial and final whitespace stripped
     */
    string wstrip(const string &s)
    {
        if (s.size() == 0) return s;
        const char *cs = s.c_str();
        const char *start;
        const char *end;
        for (start = cs; isspace(*start); start++);
        for (end = cs + strlen(cs) - 1; isspace(*end); end--);
        
        return s.substr(start - cs, end - start + 1);
    }

    string hms(uint seconds, bool showZeroHours)
    {
        uint hours = seconds / 3600;
        seconds %= 3600;
        uint minutes = seconds / 60;
        seconds %= 60;

        char buf[128];
        if ((hours == 0) && !showZeroHours) {
            sprintf(buf, "%02d:%02d", minutes, seconds);
        } else {
            sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
        }
        return buf;
    }

    string hmsm(uint seconds, int milliseconds, bool showZeroHours)
    {
        while (milliseconds < 0) {
            seconds -= 1;
            milliseconds += 1000;
        }

        uint hours = seconds / 3600;
        seconds %= 3600;
        uint minutes = seconds / 60;
        seconds %= 60;

        char buf[128];
        if ((hours == 0) && !showZeroHours) {
            sprintf(buf, "%02d:%02d.%03d",
                    minutes, seconds, milliseconds);
        } else {
            sprintf(buf, "%02d:%02d:%02d.%03d",
                    hours, minutes, seconds, milliseconds);
        }
        return buf;
    }

    char *chop(char *str)
    {
        uint len = strlen(str);
        if ((len > 0) && (str[len - 1] == '\n')) {
            str[len - 1] = 0;
        }
        return str;
    }

    char *tok(char *str, const char *sep, char **memptr, bool singleSep)
    {
        char *p = (str != NULL) ? str : *memptr;

        if (!singleSep) {
            //skip leading sep characters
            while (*p != '\0' && strchr(sep, *p) != NULL) {
                p++;
            }
        }

        if(*p == 0) { // end of string
            return NULL;
        }
    
        *memptr = strpbrk(p, sep);

        if ( *memptr ) {
            // found a sep: terminate the token and move memptr beyond it
            *(*memptr)++ = 0;
        } else {
            // No more sep: point memptr to terminating NUL, so that next
            // call will return NUL.
            *memptr = p + strlen(p);
        }
    
        return p;
    }

    int strToInt(string s, string &errMsg)
    {
        const char *ptr = s.c_str();
        char *endptr;
        int val = strtol(ptr, &endptr, 10);
        if (endptr == ptr) {
            errMsg = "Bad int";
        } else if (*endptr != 0) {
            errMsg = "Garbage after int";
        }
        return val;
    }

    uint strToUint(string s, string &errMsg)
    {
        const char *ptr = s.c_str();
        char *endptr;
        uint val = strtoul(ptr, &endptr, 10);
        // strtoul accepts negative numbers, so we check for '-'
        if (endptr == ptr || s.find('-') != s.npos) {
            errMsg = "Bad uint";
        } else if (*endptr != 0) {
            errMsg = "Garbage after uint";
        }
        return val;
    }

    double strToDouble(string s, string &errMsg)
    {
        const char *ptr = s.c_str();
        char *endptr;
        double val = strtod(ptr, &endptr);
        if (endptr == ptr) {
            errMsg = "Bad double";
        } else if (*endptr != 0) {
            errMsg = "Garbage after double";
        }
        return val;
    }

    double exprToDouble(string s, string &errMsg)
    {
        const char *ptr = s.c_str();
        double val = te_interp(ptr, 0);
        if (isnan(val)) {
            errMsg = "Bad expression";
        }
        return val;
    }

    bool strToBool(string s, string& errMsg)
    {
        bool val = false;

        if (Util::strCiEq(s, "true")) {
            val = true;
        } else if (Util::strCiEq(s, "false")) {
            val =  false;
        } else {
            errMsg = "Bad bool";
        }
        return val;
    }

    /**
     * Tokenize a string
     * @param str String to tokenize
     * @param sepChars Separator characters
     * @param quoteChars delimit string tokens
     * @param tokChars Single-character tokens (self-delimiting)
     * @singleSep If true, then consecutive separators separate empty tokens
     * @return The tokens
     */
    std::vector<string> tokenize(
        const char *str, 
        const char *sepChars, 
        const char *&errMsg,
        const char *quoteChars, 
        const char *tokenChars, 
        bool        singleSep)
    {
        #define ISSEP(c)   ((c != '\0') && (strchr(sepChars, c)   != NULL))
        #define ISQUOTE(c) ((c != '\0') && (strchr(quoteChars, c) != NULL))
        #define ISTOKEN(c) ((c != '\0') && (strchr(tokenChars, c) != NULL))
        #define QUOTING    (quoteChar != '\0')

        for (const char *p = sepChars; *p != '\0'; p++) {
            for (const char *q = tokenChars; *q != '\0'; q++) {
                ABORT_IF(*p == *q, "%c used both as separator and token", *p);
            }
        }

        errMsg = NULL; // so far, so good
        
        std::vector<string> tokens;
        const char *p;
        char quoteChar = '\0';

        string token;
        for (p = str; *p != '\0'; p++) {
            if (QUOTING) {
                if (*p == quoteChar) {
                    // stop quoting
                    quoteChar = '\0';
                } else {
                    // accumulate the character
                    token += *p;
                }
            } else if (ISQUOTE(*p)) {
                // start quoting
                quoteChar = *p;
            } else if (ISTOKEN(*p)) {
                // make token of what's been accumulated
                if (!token.empty()) {
                    tokens.push_back(token);
                    token = "";
                }
                // make a single-char token
                tokens.push_back(std::string(1, *p));
            } else if (ISSEP(*p)) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token = "";
                } else if (singleSep) {
                    // make empty token
                    tokens.push_back("");
                }
            } else {
                token += *p;
            }
        }
        if (QUOTING) {
            errMsg = "unclosed quote";
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }            
        
        return tokens;
    }


    /**
     * Check that command line options are correctly specified
     * @param optSpecs The option specifications
     */
    static void checkParseOptSpecs(std::vector<ParseOptSpec> optSpecs)
    {
        for (auto opt : optSpecs) {
            ABORT_IF(opt.optName != NULL && strchr(opt.optName, ' ') != NULL, "space in optName \"%s\"", opt.optName);
            ABORT_IF(opt.argName != NULL && strchr(opt.argName, ' ') != NULL, "space in argName \"%s\"", opt.argName);
        }
    }
    
    int parseOpts(int argc, char *argv[], std::vector<ParseOptSpec> optSpecs)
    {
        checkParseOptSpecs(optSpecs);
        
        uint numOpts = optSpecs.size();

        // Build the array to pass to getopt_long_only
        struct option longOptions[numOpts + 1];

        for (uint i = 0; i < numOpts; i++) {
            longOptions[i].name    = optSpecs[i].optName;
            longOptions[i].has_arg = (optSpecs[i].argType == OPTARG_NONE) ?
                no_argument : required_argument;
            longOptions[i].flag    = NULL;
            longOptions[i].val     = 0;
        }

        // Add a terminator entry
        longOptions[numOpts].name = NULL;
        longOptions[numOpts].has_arg = false;
        longOptions[numOpts].flag = NULL;
        longOptions[numOpts].val = 0;

        // Parse the options
        int opt; // matched option character
        int inx; // index in longOptions of matched option
        
        string errMsg;
        while (errMsg.empty() &&
               (opt = getopt_long_only(argc, argv, "", longOptions, &inx)) != -1) 
        {
            if (opt == '?') {
                return -1;
            } else if (opt != 0) {
                TRACE_FATAL("opt = %d", opt); // shouldn't happen
            }

            switch (optSpecs[inx].argType) {
                case OPTARG_NONE:
                    *((bool *) optSpecs[inx].argPtr) = true;
                    break;
                case OPTARG_STR:
                    *((char **) optSpecs[inx].argPtr) = optarg;
                    break;
                case OPTARG_INT:
                    *((int *) optSpecs[inx].argPtr) = Util::strToInt(optarg, errMsg);
                    break;
                case OPTARG_UINT:
                    *((uint *) optSpecs[inx].argPtr) = Util::strToUint(optarg, errMsg);
                    break;
                case OPTARG_DBLE:
                    *((double *) optSpecs[inx].argPtr) = Util::strToDouble(optarg, errMsg);
                    break;
                case OPTARG_EXPR:
                    *((double *) optSpecs[inx].argPtr) = Util::exprToDouble(optarg, errMsg);
                    break;
                default:
                    TRACE_FATAL("Unknown optarg type.");
            }
        }
        if (errMsg.empty()) {
            return 0;
        } else {
            fprintf(stderr, "%s: %s\n", optarg, errMsg.c_str());
            return -1;
        }
    }

    string parseOptsUsage(
        const char *pname, 
        std::vector<ParseOptSpec> optSpecs,
        bool vertical,
        std::vector<string> nonFlags)
    {
        string r = "Usage: ";
        r.append(pname);

        for (uint i = 0; i < optSpecs.size(); i++) {
            if (vertical) {
                r.append("\n        ");
            }
            r.append(" [-");
            r.append(optSpecs[i].optName);
            if (!Util::isBlank(optSpecs[i].argName)) {
                r.append(" ");
                r.append(optSpecs[i].argName);
            }
            r.append("]");
            if (vertical && (optSpecs[i].descr != NULL)) {
                r.append("\t// ");
                r.append(optSpecs[i].descr);
            };
        }
        for (uint i = 0; i < nonFlags.size(); i++) {
            if (vertical) {
                r.append("\n        ");
            }
            r.append(" ");
            r.append(nonFlags[i]);
        }            
        if (!vertical) {
            // print descriptions
            for (uint i = 0; i < optSpecs.size(); i++) {
                if (optSpecs[i].descr != NULL) {
                    r.append("\n        -");
                    r.append(optSpecs[i].optName);
                    r.append(": ");
                    r.append(optSpecs[i].descr);
                }
            }
        }
        return r;
    }

    // Binomial coefficient lookup table
    //
    static std::vector<std::vector<uint> > binomTable;

    void initBinom(uint newN, uint newK)
    {
        uint oldN = binomTable.size();
        uint oldK = (oldN == 0) ? 0 : binomTable[0].size();
        
        // reallocate the table
        binomTable.resize(newN + 1);
        for (uint n = 0; n <= oldN; n++) {
            binomTable[n].resize(newK + 1);
        }
        for (uint n = oldN + 1; n <= newN; n++) {
            binomTable[n] = std::vector<uint>(newK + 1);
        }

        // Initialize new [0][k] entries to 0
        for (uint k = oldK + 1; k <= newK; k++) binomTable[0][k] = 0;
        // Initialize new [n][0] entries to 1
        for (uint n = oldN; n <= newN; n++) binomTable[n][0] = 1;
        // Compute new column entries of old rows
        for (uint n = 1; n <= oldN; n++) {
            for (uint k = oldK + 1; k <= newK; k++) {
                binomTable[n][k] = binomTable[n-1][k-1] + binomTable[n-1][k];
            }
        }
        // Compute all entries in new rows
        for (uint n = oldN + 1; n <= newN; n++) {
            for (uint k = 1; k <= newK; k++) {
                binomTable[n][k] = binomTable[n-1][k-1] + binomTable[n-1][k];
            }
        }
    }

    uint binom(uint n, uint k)
    {
        if(n >= binomTable.size() || k >= binomTable[0].size()) {
            initBinom(n, k);
        }
        return binomTable[n][k];
    }
}
