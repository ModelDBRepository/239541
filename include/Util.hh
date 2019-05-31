/**
 * @file Util.hh
 *
 * Miscellaneous utilities
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

#ifndef UTIL_HH
#define UTIL_HH

#include <glob.h>
#include <vector>
using std::vector;
#include <string.h>
#include <string>
#include <bitset>
using std::string;
#include <format.h>

#include <Trace.hh>

namespace Util {
    /**
     * Test for oddness or evenness
     */
    inline bool isOdd(uint i)  { return i % 2 == 1; }
    inline bool isEven(uint i) { return i % 2 == 0; }

    /**
     * Test if a string consists of digits only
     */
    inline bool isDigitsOnly(const std::string & s)
    {
        for (uint i = 0; i < s.size(); i++) {
            if (!isdigit(s[i])) return false;
        }
        return true;
    }    

    /**
     * Initialize rand
     */
    void initRand();

    /**
     * Generate a random integer in [min, max] inclusive
     */
    int randInt(int min, int max);
    
    /**
     * Generate a random double in [min, max] or (min, max)
     * @param min Lower interval endpoint
     * @param max Upper interval endpoint
     * @param open If true sample from (min, max), else [min, max]
     */
    double randDouble(double min, double max, bool open = false);

    /**
     * Create a random permutation of the integers min ... max-1
     */
    vector<int> randPerm(int min, int max);
    
    /**
     * Create a random permutation of the integers 0 ... n-1
     */
    inline vector<int> randPerm(int n) {
        return randPerm(0, n);
    }
    
    /**
     * Create a random set of n integers in the range [0, max-1]
     * May contain duplicates.
     */
    vector<int> randIntList(uint n, int max);

    /**
     * Create a random set of n unique integers in the range [min, max-1]
     * No duplicates.
     */
    vector<int> randUniqueIntList(int n, int min, int max);

    /**
     * Create a random set of n unique integers in the range [0, max-1]
     * No duplicates.
     */
    inline vector<int> randUniqueIntList(int n, int max) {
        return randUniqueIntList(n, 0, max);
    }

    /**
     * Create a random set of n doubles in the range [min, max] or (min, max)
     * May contain duplicates.
     * @param min Lower interval endpoint
     * @param max Upper interval endpoint
     * @param open If true sample from (min, max), else [min, max]
     */
    vector<double> randDoubleList(uint n, double min, double max, bool open = false );

    /**
     * Create a random set of n doubles from the provided domain
     * May contain duplicates.
     */
    vector<double> randDoubleList(uint n, vector<double> &domain);

    /**
     * Create a random set of n unique doubles from the provided domain
     * No duplicates.
     */
    vector<double> randUniqueDoubleList(uint n, vector<double> &domain);

    /**
     * Create a binary string representation of an integer with
     * a specified number of bits, e.g. 13, 5 --> "01101".
     * @param value The number to represent
     * @param len Number of bits, default is the number of bits in T
     * @param zeroChar character for representing a zero bit
     * @param oneChar character for representing a one bit
     */
    template<class T> std::string intToBinStr(
        T i,
        uint width = 0,
        char zero = char('0'),
        char one = char('1'))
    {
        uint w = (width != 0) ? width : 8 * sizeof(i);
        std::string s = std::bitset<8 * sizeof(i)>(i).to_string(zero, one);
        return s.substr(s.length() - w);
    }
    /*
     Less C++ style version 
    inline string intToBinStr(int value, int len, char zeroChar, char oneChar)
    {
        char buf[len+1];
        buf[len] = 0;
        for (int i = 0; i < len; i++) {
            buf[len - i - 1] = ((value >> i) & 1) ? oneChar : zeroChar;
        }
        return buf;
    }
    */

    /**
     * Create a string of length len where each character is
     * randomly selected from charset.
     * @param charset Set of characters to use
     * @param len Length of resulting string
     */
    string randStr(const char* charset, int len);

    /**
     * Print a usage message, optionally preceded by an error message.
     * @param syntax Syntax message
     * @param fmt ... printf-style error message
     */
    void usage(const char *syntax, const char *fmt, ...);

    /**
     * Print a usage message, optionally preceded by an error message,
     * then exit with status EXIT_FAILURE.
     * @param syntax Syntax message
     * @param fmt ... printf-style error message
     */
    void usageExit(const char *syntax, const char *fmt, ...);

    /**
     * return the min or max of two or three values
     * TODO: rewrite with variadic template
     */
    template<class T> T     min(const T &a, const T &b) { return (a < b) ? a : b; }
    template<class T> T     max(const T &a, const T &b) { return (a > b) ? a : b; }

    template<class T> T     min(const T &a, const T &b, const T &c) {
        return (a < b) ? min(a, c) : min(b, c);
    }
    template<class T> T     max(const T &a, const T &b, const T &c) {
        return (a > b) ? max(a, c) : max(b, c);
    }
    
    /**
     * If value is within [min:max] return it, otherwise return
     * the nearest of min or max.
     */
    template<class T> T bracket(const T value, const T min, const T max)
    {
        return (value < min) ? min : (value > max) ? max : value;
    }

    /**
     * Confine a variable to a range
     */
    template<class T> void confine(T &val, const T &min, const T &max)
    {
        if (val < min) {
            val = min;
        } else if (val > max) {
            val = max;
        }
    }

    /**
     * Swap the values of two variables
     */
    template<class T> void swap(T &a, T &b) { T t = a; a = b; b = t; }

    /**
     * Create a copy of a string with initial and final whitespace stripped
     */
    string wstrip(const string &s);

    /**
     * Whether a string consist entirely of whitespace
     */
    inline bool isBlank(const string &s) { return wstrip(s).size() == 0; }
    inline bool isBlank(const char *s) {return s == NULL || isBlank(string(s)); }

    /**
     * Case-independent string compare
     */
    inline int strCiCmp(const string &s1, const string &s2)
    {
        return strcasecmp(s1.c_str(), s2.c_str());
    }

    /**
     * Case-independent string equality test
     */
    inline bool strCiEq(const string &s1, const string &s2)
    {
        return strCiCmp(s1, s2) == 0;
    }

    /**
     * Case-dependent string equality test
     */
    inline bool strEq(const string &s1, const string &s2)
    {
        return strcmp(s1.c_str(), s2.c_str()) == 0;
    }

    /**
     * Chop off terminating linefeed of C string (in place)
     */
    char *chop(char *str);

    /**
     * Plural suffix for printf formats
     */
    inline const char *plural(int i)
    {
        return (i == 1) ? "" : "s";
    }

    /**
     * Plural form for printf formats
     */
    inline const char *plural(int i, const char *sing, const char *plur)
    {
        return (i == 1) ? sing : plur;
    }

    /**
     * double compare function, suitable for qsort
     */
    inline int compareDoubles(const void* d1, const void* d2)
    {
        double diff = *((double *)d1) - *((double *)d2);
        return (diff > 0.0) ? 1 : (diff < 0.0) ? -1 : 0;
    }

    /**
     * Generate [hh:]mm:ss representation of a time interval.
     * @param seconds Time interval in seconds
     * @param showZeroHours whether to show zero hours
     */
    string hms(uint seconds, bool showZeroHours = true);

    /**
     * Generate [hh:]mm:ss.sss representation of a time interval.
     * @param seconds Time interval in seconds
     * @param milliseconds Additional milliseconds (negative is ok)
     * @param showZeroHours whether to show zero hours
     */
    string hmsm(uint seconds, int milliseconds, bool showZeroHours);

    /**
     * Tokenize a string: if str != NULL return pointer to first token,
     * NUL-terminated. If str == NULL, return next token from string being
     * parsed.
     * 
     * If singleSep is true, then consecutive separator characters separate
     * empty tokens. Default behavior is like strtok: strtok treats two or
     * more consecutive separator characters as a single separator.
     *
     * Unlike strtok, is reentrant and threadsafe.
     *
     * Like strtok, destroys str by overwriting the separators with NULs.
     * 
     * @param str String to tokenize
     * @param sep Separator
     * @param memptr Memory pointer provided by (and not used by) the context
     * @singleSep If true, then consecutive separators separate empty tokens
     * @return Next token, or NULL if no more tokens
     */
    char *tok(char *str, const char *sep, char **memptr, bool singleSep = false);

    /**
     * Convert string to int
     * @param errMsg will be non-empty if there's a problem
     */
    int strToInt(string s, string &ErrMsg);

    /**
     * Convert string to uint
     * @param errMsg will be non-empty if there's a problem
     */
    uint strToUint(string s, string &ErrMsg);

    /**
     * Convert string to double
     * @param errMsg will be non-empty if there's a problem
     */
    double strToDouble(string s, string &errMsg);
    
    /**
     * Convert arithmetic expression to double
     * @param errMsg will be non-empty if there's a problem
     */
    double exprToDouble(string s, string &errMsg);
    
    /**
     * Convert string to bool
     * @param errMsg will be non-empty if there's a problem
     */
    bool strToBool(string s, string &errMsg);
    
    /**
     * Tokenize a c-string
     * @param str String to tokenize
     * @param sepChars Separator characters
     * @errMsg set to non-NULL if error encountered
     * @param quoteChars delimit string tokens
     * @param tokChars Single-character tokens (self-delimiting)
     * @singleSep If true, then consecutive separators separate empty tokens
     * @return The tokens
     */
    std::vector<string> tokenize(
        const char *str, 
        const char *sepChars, 
        const char *&errMsg,
        const char *quoteChars = "",
        const char *tokChars = "", 
        bool        singleSep = false);
    
    /**
     * Tokenize a std::string
     * @param str String to tokenize
     * @param sepChars Separator characters
     * @errMsg set to non-empty if error encountered
     * @param quoteChars delimit string tokens
     * @param tokChars Single-character tokens (self-delimiting)
     * @singleSep If true, then consecutive separators separate empty tokens
     * @return The tokens
     */
    inline std::vector<string> tokenize(
        const string str, 
        const string sepChars, 
        string       &errMsg,
        const string quoteChars = "",
        const string tokChars = "", 
        bool         singleSep = false)
    {
        const char *err = NULL;
        std::vector<string> tokens = tokenize(str.c_str(),
                                              sepChars.c_str(), 
                                              err,
                                              quoteChars.c_str(), 
                                              tokChars.c_str(),
                                              singleSep);
        if (err != NULL) {
            errMsg = err;
        }
        return tokens;
    }

    /**
     * Concatenate tokens, separated by sep
     * @param tokens The tokens
     * @param sep Separator string
     */
    inline string untokenize(std::vector<string> tokens, string sep = " ")
    {
        uint n = tokens.size();
        string result;
        for (uint i = 0; i < n -1; i++) {
            result += tokens[i];
            result += sep;
        }
        result += tokens[n-1];
        return result;
    }

    /**
     * std::string wrapper around glob(3): find pathnames matching a pattern
     * @param pat A pathname pattern as specified in glob(7).
     */
    inline std::vector<std::string> glob(const string& pat){
        glob_t glob_result;
        glob(pat.c_str(), GLOB_TILDE, NULL, &glob_result);
        std::vector<string> ret;
        for(unsigned int i = 0; i < glob_result.gl_pathc; ++i){
            ret.push_back(string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
        return ret;
    }

    /**
     * Option argument types, for use when calling parseOpts. The end comments
     * indicate the type of variable that the corresponding argPtr must point to
     * for each value of optArgType.
     */
    const int OPTARG_NONE = 0;	// bool
    const int OPTARG_STR  = 1;	// char *
    const int OPTARG_INT  = 2;	// int
    const int OPTARG_UINT = 3;	// uint
    const int OPTARG_DBLE = 4;	// double
    const int OPTARG_EXPR = 5;	// arith expr

    /**
     * Option specification for parseOpts and parseOptsUsage
     * 
     * For options that don't take an argument, argType should be set
     * to OPTARG_NONE and argPtr should point to a bool, which will be
     * set to true if the option is present.
     * 
     * argName is used by parseOptsUsage, but not by parseOpts
     */
    typedef struct {
        const char *optName;   // option name as specified on cmdline
        int         argType;   // option argument type
        void       *argPtr;    // pointer to variable that will receive arg value
        const char *argName;   // arg value name, used for usage message only
        const char *descr;     // Description, used for usage message only
    } ParseOptSpec;
    
    /**
     * Parse command line options, using getopt_long_only (3)
     *
     * Options may be single- or multi-character, and may (in either case) be
     * specified by either a single or a double dash. For example, if the
     * options "v" and "verbosity" are both specified, than any of -v, --v,
     * -verbosity and --verbosity will work, and the argument, when present, may
     * be separated from the option name either by whitespace or by a =.
     * Furthermore, long option names may be abbreviated, as long as the
     * abbreviation is unique, so "-verb 5" will also work, unless there is
     * another option with a name that begins with 'verb'.
     *
     * Concatenation of single-character options (like ps -eax) is not supported.
     * 
     * @param argc The argument count.
     * @param argv The argument vector.
     * @param optSpecs Option specifiers
     * @return 0 if all went well, -1 otherwise 
     */
    int parseOpts(int argc, char *argv[], std::vector<ParseOptSpec> optSpecs);

    /**
     * Generate a "usage" message given a program name and a vector of
     * parseOpts option specifications.
     * 
     * @param pname The program name
     * @param optSpecs Option specifiers
     * @param vertical If true, display option list vertically
     * @param nonFlags Cmd line args other than flags
     * @return 0 if all went well, -1 otherwise
     */
    string parseOptsUsage(
        const char *pname,
        std::vector<ParseOptSpec> optSpecs,
        bool vertical = false,
        std::vector<string> nonFlags = std::vector<string>());

    // Initialize binomial coefficient table to specified dimensions.
    // Note: if not called explicitly, then binom() will call it as needed.
    //
    void initBinom(uint N, uint K);

    // Look up a binomial coefficient
    //
    uint binom(uint n, uint k);


    /**
     * Quick fmtlib formats for printing out matrices
     * (printf equivalents in comments)
     */
    // TODO: width and precision as arguments
    inline const char *tableFmt(double)       { return "{:8.2f}"; } // "%#6.2f"
    inline const char *tableFmt(int)          { return "{:6d}"; }   // "%6d"
    inline const char *tableFmt(uint)         { return "{:6d}"; }   // "%6d"
    inline const char *tableFmt(const char *) { return "{}"; }      // "%s"

    /**
     * String representation of a matrix of numbers or C strings
     */
    template<class T> string matrixToStr(vector<vector<T>> &m)
    {
        string s;
        for (uint i = 0; i < m.size(); i++) {
            for (uint j = 0; j < m[0].size(); j++) {
                T &val = m[i][j];
                string valStr = fmt::format(tableFmt(val), val);
                s += valStr;
            }
            s += "\n";
        }
        return s;
    }

    /**
     * Element-wise vector and matrix operations
     */
    enum Operation {
        ADD,
        SUB,
        MUL,
        DIV,
        MIN,
        MAX,
        AVG,
        STDEVP,
        STDEVS,
        STERR
    };

    /**
     * Apply an operation to corresponding elements of two vectors
     */
    template<class T> vector<T> vectorOp(
        vector<T> v1,
        vector<T> v2,
        Operation op)
    {
        ABORT_IF(v1.size() != v2.size(), "vectors must be of equal size");

        vector<T> r;
        for (uint i = 0; i < v1.size(); i++) {
            T val;
            switch (op) {
                case ADD: val = v1[i] + v2[i]; break;
                case SUB: val = v1[i] - v2[i]; break;
                case MUL: val = v1[i] * v2[i]; break;
                case DIV: val = v1[i] / v2[i]; break;
                case MIN: val = min(v1[i], v2[i]); break;
                case MAX: val = max(v1[i], v2[i]); break;
                default: TRACE_FATAL("Invalid operation");
            }
            r.push_back(val);
        }
        return r;
    }

    /**
     * Apply an operation with a scalar to each element of a vector
     */
    template<class T> vector<T> vectorOp(
        vector<T> v,
        T a,
        Operation op)
    {
        vector<T> r;
        for (uint i = 0; i < v.size(); i++) {
            T val;
            switch (op) {
                case ADD: val = v[i] + a; break;
                case SUB: val = v[i] - a; break;
                case MUL: val = v[i] * a; break;
                case DIV: val = v[i] / a; break;
                case MIN: val = min(v[i], a); break;
                case MAX: val = max(v[i], a); break;
                default: TRACE_FATAL("Invalid operation");
            }
            r.push_back(val);
        }
        return r;
    }

    template<class T, class U> vector<T> vectorAdd(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, ADD);
    }

    template<class T, class U> vector<T> vectorSub(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, SUB);
    }

    template<class T, class U> vector<T> vectorMul(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, MUL);
    }

    template<class T> vector<T> vectorSquare(
        vector<T> v)
    {
        return vectorMul(v, v);
    }

    template<class T, class U> vector<T> vectorDiv(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, DIV);
    }

    template<class T, class U> vector<T> vectorMin(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, MIN);
    }

    template<class T, class U> vector<T> vectorMax(
        vector<T> v,
        U a)
    {
        return vectorOp(v, a, MAX);
    }

    /**
     * Apply an operation to corresponding elements of two matrices
     */
    template<class T> vector<vector<T>> matrixOp(
        vector<vector<T>> m1,
        vector<vector<T>> m2,
        Operation op)
    {
        ABORT_IF(m1.size() != m2.size(), "matrices must be equal size");
        vector<vector<T>> r;
        for (uint i = 0; i < m1.size(); i++) {
            vector<T> row;
            switch (op) {
                case ADD: row = vectorAdd(m1[i], m2[i]); break;
                case SUB: row = vectorSub(m1[i], m2[i]); break;
                case MUL: row = vectorMul(m1[i], m2[i]); break;
                case DIV: row = vectorDiv(m1[i], m2[i]); break;
                case MIN: row = vectorMin(m1[i], m2[i]); break;
                case MAX: row = vectorMax(m1[i], m2[i]); break;
                default: TRACE_FATAL("Invalid operation");
            }
            r.push_back(row);
        }
        return r;
    }

    /**
     * Apply an operation with a scalar to each element of a matrix
     */
    template<class T> vector<vector<T>> matrixOp(
        vector<vector<T>> m,
        T a,
        Operation op)
    {
        vector<vector<T>> r;
        for (uint i = 0; i < m.size(); i++) {
            vector<T> row;
            switch (op) {
                case ADD: row = vectorAdd(m[i], a); break;
                case SUB: row = vectorSub(m[i], a); break;
                case MUL: row = vectorMul(m[i], a); break;
                case DIV: row = vectorDiv(m[i], a); break;
                case MIN: row = vectorMin(m[i], a); break;
                case MAX: row = vectorMax(m[i], a); break;
                default: TRACE_FATAL("Invalid operation");
            }
            r.push_back(row);
        }
        return r;
    }
    
    template<class T, class U> vector<vector<T>> matrixAdd(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, ADD);
    }

    template<class T, class U> vector<vector<T>> matrixSub(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, SUB);
    }

    template<class T, class U> vector<vector<T>> matrixMul(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, MUL);
    }

    template<class T> vector<vector<T>> matrixSquare(
        vector<vector<T>> m)
    {
        return matrixMul(m, m);
    }

    template<class T, class U> vector<vector<T>> matrixDiv(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, DIV);
    }

    template<class T, class U> vector<vector<T>> matrixMin(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, MIN);
    }

    template<class T, class U> vector<vector<T>> matrixMax(
        vector<vector<T>> m,
        U a)
    {
        return matrixOp(m, a, MAX);
    }
}    

/**
 * Number of elements in a statically allocated array
 */
#define NUM_ELEM(x) (sizeof(x) / sizeof(x[0]))

/**
 * Test if pointer or object is of a specified class
 * (I wonder if there's a way to do this with templates?)
 */
#define IS_PTR_TO(pointer, type) (dynamic_cast<type *>(pointer) != NULL)
#define IS_OF_CLASS(obj, type)   (dynamic_cast<type *>(&obj) != NULL)

#endif // UTIL_HH
