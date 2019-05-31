/**
 * @file Gillespie.cc
 *
 * Implements Gillespie simulator
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

#include <vector>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
using std::string;
#include <unordered_map>
#include <libgen.h>

#include <format.h>
#include <tinyexpr.h>

#include "Gillespie.hh"
#include "Sched.hh"
#include "Trace.hh"

/**
 * Defined symbols
 */
std::unordered_map<string, string> defines;

static inline void dumpDefines()
{
    for (auto d : defines) {
        fmt::print("{} = {}\n", d.first, d.second);
    }
}    

/**
 * Constructor
 */
Gillespie::Gillespie(
    const char *gilFileName,
    void (*preIterFunc)(double time))
    : volume(0.0),
      runIdle(true),
      idleTick(0.3),
      preIterFunc(preIterFunc),
      twidth(9),
      mwidth(7)
{
    uint numLines = readGilFile(gilFileName);
    verify(gilFileName, numLines);
    // dumpDefines();
}        
    
static uint factorial(uint n)
{
    uint f = 1;
    for (uint i = 1; i <= n; i++) {
        f *= i;
    }
    return f;
}

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

/**
 * Substitute defined symbols in a vector of tokens
 * @param tokens Token vector
 * @param first First token to process
 * @param last Last token to process
 */
static void symSubst(
    std::vector<string> &tokens,
    size_t first = 0,
    size_t last = UINT_MAX)
{
    last = Util::min(last, tokens.size());

    for (uint i = first; i < last; i++) {
        auto sub = defines.find(tokens[i]);
        if (sub != defines.end()) {
            tokens[i] = sub->second;
        }
    }
}

/**
 * If str is a valid arithmetic expression, replace it with
 * the result of its evaluation.
 */
static void evalArith(std::string &str, string fname, uint lineNum)
{
    // TRACE_DEBUG("str before = %s", str.c_str());
    string errMsg;
    std::vector<string> tokens = Util::tokenize(str, " ", errMsg);
    if (!errMsg.empty()) {
        fail(fname, lineNum, "{}", errMsg);
    }
    symSubst(tokens);
    str = Util::untokenize(tokens);

    double val = te_interp(str.c_str(), 0);
    if (val != NAN) {
        str = std::to_string(val);
    }
    // TRACE_DEBUG("str after  = %s", str.c_str());
}

/**
 * Greatest reactant cardinality across all the reactions
 */
static uint maxK; // calculated by verify

/**
 * The number of distinct combinations of k elements that can be
 * chosen from a set of size n
 */

inline uint numCombinations(uint n, uint k)
{
    static uint maxN = 0;
    
    if (k == 1) {
        return n;
    } else {
        if (n > maxN) {
            Util::initBinom(maxN = 10 * n, maxK);
        }
        return Util::binom(n, k);
    }
}


/**
 * Calculate reaction probabilities (h and a values) for all
 * reactions.
 * @return: cumulative probability a0
 */
inline double Gillespie::calcReactProbs()
{
    double a0 = 0;
    for (auto& r : reactions) {
        if (r.isDirty) {
            r.h = 1.0;
            for (uint m = 0; m < molecules.size(); m++) {
                if (molecules[m].getCount() >= r.left[m]) {
                    for (uint n = 0; n < r.left[m]; n++) {
                        r.h *= numCombinations(molecules[m].getCount(),
                                               r.left[m]);
                    }
                }
            }
            if (isPossible(r)) {
                r.a = r.h * r.c * (1.0 - r.inhibition) ;
            } else {
                r.a = 0.0;
            }
            r.isDirty = false;
            r.recalc = true;
        } else {
            r.recalc = false;
        }
        a0 += r.a;
    }
    return a0;
}

/**
 * Run the Gillespie algorithm until
 * (a) stopTime is reached, or
 * (b) no more reactions are possible and runIdle is false, or
 * (c) the count of a specified molecule passes through a specified
 *     threshold (from either direction.)
 * 
 * @param plotInterval Time interval between molecule count outputs
 * @param stopTime Simulated time at which to stop unconditionally
 * @param monitorId Id of molecule to be monitored
 * @param threshold Stop when monitored molecule count reaches this value
 * @param monitorDelay Continue for this time interval after threshold
 *        reached
 * @param runIdle Keep running even if no reaction can run
 */
void Gillespie::run(
    double plotInterval,
    double stopTime,
    const char *monitorId,
    double threshold,
    double monitorDelay)
{
    // Initialize the random number generator
    //
    Util::initRand();

    // Print header line 
    string header = makeHeader(molecules);
    fmt::print("{}\n", header);

    // Is monitored molecule initially above or below threshold?
    //
    bool monitoring = false;
    uint monitorIndex = 0;
    bool monitorInitState = false;
    bool thresholdReached = false;

    if (monitorId != NULL) {
        monitoring = true;
        monitorIndex = moleculeIndex(monitorId);
        if (monitorIndex == UINT_MAX) {
            fmt::print("unknown molecule ({}) specified for monitoring",
                       monitorId);
            exit(1);
        }
        monitorInitState = (molecules[monitorIndex].getCount() > threshold);
        thresholdReached = false;
    }

    double plotTime = 0.0; // When to plot next 

    double t = 0.0;

    while (t <= stopTime ) {
        Sched::processEvents(t);

        // If the monitored molecule reached the threshold, arrange
        // to stop after the interval specified by monitorDdelay
        //
        if (monitoring && !thresholdReached) {
            if ((molecules[monitorIndex].getCount() == threshold) ||
                ((molecules[monitorIndex].getCount() > threshold) != 
                 monitorInitState)) 
            {
                stopTime = t + monitorDelay;
                thresholdReached = true;
            }
        }

        // Call the pre-iteration function, if one has been specified
        //
        if (preIterFunc != NULL) {
            (*preIterFunc)(t);
        }

        // Calculate reaction probabilities
        //
        // a0*dt is the probability that *any* reaction fires in the next
        // infinitesimal time interval dt
        //

        double a0 = calcReactProbs();

        // Roll the dice to determine which reaction (r) will happen next
        // and the time interval (tau) until it happens.
        //
        double r1 = 0.0, r2 = 0.0;
        double tau = 0.0;
        double sum = 0.0;
        int r = -1; // next reaction. -1 means none
        
        if (a0 != 0.0) {
            // At least one reaction is possible

            r1 = Util::randDouble(0.0, 1.0, true);
            r2 = Util::randDouble(0.0, a0, true);

            tau = 1.0 / a0 * log(1.0 / r1);
            if(tau == 0.0) { // should be impossible!
                fmt::print("a0={}, r1={}\n", a0, r1);
                TRACE_FATAL("Ouch!");
            }
                    

            sum = 0.0;

            for (r = 0; r < (int) reactions.size() - 1; r++) {
                if ((sum += reactions[r].a) >= r2) {
                    break;
                }
            }
        }

        if (r == -1) {
            // No reaction was possible
            if (runIdle) {
                tau = idleTick;
            }
        }

        // update t
        //
        t += tau;

        // Before updating the molecule counts, output the current counts
        // for any plot times that occurred during this simulation step.
        
        bool reactionPrinted = false;

        for (; plotTime <= t && plotTime <= stopTime; plotTime += plotInterval)
        {
            if (TRACE_DEBUG1_IS_ON && plotTime > 0.0) {
                fmt::print("{}\n", header);
            }

            fmt::print("{:{}.4f}", plotTime, twidth);
            for (uint m = 0; m < molecules.size(); m++) {
                fmt::print("{:{}}", molecules[m].getCount(), fwidths[m]);
            }
                
            if (!reactionPrinted) {
                if (Trace::getTraceLevel() == Trace::TRACE_Debug) {
                    if (r >= 0) {
                        fmt::print(" [{:{}}] {}",
                                   reactions[r].id,
                                   rwidth,
                                   reactions[r].formula);
                    } else {
                        fmt::print(" (no reaction)\n");
                    }
                }
                reactionPrinted = true;
            }
            
            fmt::print("\n");
                
            if (r >= 0) {
                if (TRACE_DEBUG1_IS_ON) {
                    fmt::print("-----------------------------------\n");
                    //          [r1](10.000,  0,    0.00)
                    fmt::print("{:{}}   c         h     a\n", "", rwidth + 3);
                    for (uint rr = 0; rr < reactions.size(); rr++) {
                        string s;

                        fmt::print("[{:{}}]{}({:7.3f},{:5},{:8.2f}) ", 
                                   reactions[rr].id,
                                   rwidth,
                                   reactions[rr].recalc ? '*' : ' ',
                                   reactions[rr].c,
                                   reactions[rr].h,
                                   reactions[rr].a);
                        bool firstReactant = true;
                        for (uint m = 0; m < molecules.size(); m++) {
                            if (reactions[rr].left[m] != 0) {
                                if (firstReactant) {
                                    firstReactant = false;
                                } else {
                                    s += "+ ";
                                }
                            
                                if (reactions[rr].left[m] > 1) {
                                    s += fmt::format("{} ",
                                                     reactions[rr].left[m]);
                                }
                                s += molecules[m].id + " ";
                            }
                        }
                        fmt::print("{:13} ---> ", s);
                        s = "";
                        bool firstProduct = true;
                        for (uint m = 0; m < molecules.size(); m++) {
                            if (reactions[rr].right[m] != 0) {
                                if (firstProduct) {
                                    firstProduct = false;
                                } else {
                                    s += "+ ";
                                }
                            
                                if (reactions[rr].right[m] > 1) {
                                    s += fmt::format("{} ",
                                                     reactions[rr].right[m]);
                                }
                                s += molecules[m].id + " ";
                            }
                        }
                        fmt::print("{}\n", s);
                    }
                    fmt::print("-----------------------------------\n");
                    fmt::print("R = [{:{}}] {}\n", 
                               reactions[r].id.c_str(),
                               rwidth,
                               reactions[r].formula.c_str());
                    fmt::print("===================================\n");
                }
            }
        }

        if (r != -1) {
            // A reaction happened: update molecule counts
            //
            for (uint m = 0; m < molecules.size(); m++) {
                int delta = -reactions[r].left[m] + reactions[r].right[m];
                if (delta != 0) {
                    molecules[m].setCount(molecules[m].getCount() + delta);

                    if (molecules[m].getCount() > 1000000) {
                        TRACE_INFO("Something fishy - about to dump core");
                        fflush(stdout);
                        kill(getpid(), SIGABRT);
                    }
                }
            }
        } else {
            // No reaction was possible
            //
            if (!runIdle) {
                // We don't run idle - stop
                break;
            }
        }
    }

    if (TRACE_DEBUG1_IS_ON) {
        fmt::print("t = {}.2f\n", t, twidth);
    }
}

void Gillespie::Reaction::parseFormula(string fname, uint lineNum)
{
    left = std::vector<uint>(g.molecules.size(), 0);
    right = std::vector<uint>(g.molecules.size(), 0);

    string errMsg;
    std::vector<string> tokens = Util::tokenize(formula, " ", errMsg);
    if (!errMsg.empty()) {
        fail(fname, lineNum, "{}", errMsg);
    }

    enum { LEFT = 0, RIGHT = 1 } side = LEFT;
    enum { COUNT, MOLECULE, OPERATOR } expect = COUNT;
    uint count = 0;
    for (auto& token : tokens) {
        switch (expect) {
            case COUNT:
                if (Util::isDigitsOnly(token)) {
                    count = strtoul(token.c_str(), NULL, 10);
                    if (count != 0) {
                        expect = MOLECULE;
                    } else {
                        expect = OPERATOR; // lone 0 means 'nothing'
                    }
                    break;
                } else {
                    count = 1;
                    // fall thru to MOLECULE
                }
            case MOLECULE: {
                uint m = g.moleculeIndex(token);
                if (m < g.molecules.size()) {
                    if (side == LEFT) {
                        left[m] = count;
                    } else {
                        right[m] = count;
                    }
                    expect = OPERATOR;
                } else {
                    fail(fname, lineNum,
                         "Unknown molecule: {}", token);
                }
                break;
            }
            case OPERATOR:
                if (side == LEFT && token == "--->") {
                    side = RIGHT;
                    expect = COUNT;
                    break;
                } else if (token == "+") {
                    expect = COUNT;
                    break;
                } else {
                    fail(fname, lineNum,
                         "Expected operator, got '{}'", token);
                }
                break;
        }
    }

    if (side != RIGHT || expect != OPERATOR) {
        fail(fname, lineNum, "Incomplete reaction formula");
    }

    // c = prod(mj!)/pow(v, n-1) * k
    //
    // Calculate the stochastic reaction constant c by multiplying the
    // deterministic (concentration-based) reaction rate k by the product of
    // the factorials of the cardinalities of the reactants mj and dividing
    // by the volume v raised to the number n of reactant molecules minus 1.
    // 
    // Why divide by V^(n-1)?
    // The n is because the concentration-based formulation involves a V for
    // each reactant concentration (X/V), and the -1 is because the k
    // number represents reaction rate per unit volume).
    //
    // Why multiply by prod(mj!)?
    //
    // The number of unique combinations of X and Y is X*Y, but combinations
    // of X and X is X * (X-1) / 2 = (approx) x^2 / 2), and of 3 xs is
    // X^3/3!, etc. See Gillespie 1977.
    //
    // For details see my see my document gillespie.docx in
    //  C:/neuroscience/memory_reconsolidation/cellular_recons_paper
    //
    uint n = 0; // number of reactant molecules
    uint p = 1; // product of factorials of reactant cardinalities
    for (auto& m : left) {
        n += m;
        p *= factorial(m);
    }
    c = p * k / pow(g.volume, n - 1);
}


/**
 * Retrieve molecule index by id
 */
uint Gillespie::moleculeIndex(string id)
{
    for (uint i = 0; i < molecules.size(); i++) {
        if (Util::strCiEq(id, molecules[i].id)) {
            return i;
        }
    }
    return UINT_MAX;
}

/**
 * Retrieve reaction index by id
 */
uint Gillespie::reactionIndex(string id)
{
    for (uint i = 0; i < reactions.size(); i++) {
        if (Util::strCiEq(id, reactions[i].id)) {
            return i;
        }
    }
    return UINT_MAX;
}


/**
 * Expand a wildcard (*) in a reaction ID
 */
static std::unordered_map<string, uint> rNumbers;

void Gillespie::expandReactionWildcard(string &id)
{
    size_t pos = id.find('*');
    if (pos != string::npos) {
        if (rNumbers.find(id) == rNumbers.end()) {
            rNumbers[id] = 0;
        }
        string newId;
        do {
            uint n = rNumbers[id];
            rNumbers[id] = n + 1;

            newId = id.substr(0, pos) + 
                std::to_string(n) +
                id.substr(pos + 1);
        } while (reactionIndex(newId) < reactions.size());
        id = newId;
    }
}

/**
 * Remove # and everything that follows it from line
 */
static void stripComment(char *line)
{
    char *p;
    if ((p = strchr(line, '#')) != NULL) {
        *p = '\0';
    }
}

/**
 * Divide a string into tokens
 * @param s The string to tokenize
 * @param fname File name, for error messages
 * @param lineNum Line number, for error messages
 */
static std::vector<std::string> tokenizeString(
    std::string s,
    std::string fname,
    uint lineNum)
{
    string errMsg;
    std::vector<std::string> tokens = Util::tokenize(
        s, " \t", errMsg, "'\"");
    if (!errMsg.empty()) {
        fail(fname, lineNum, "{}", errMsg);
    }
    return tokens;
}

/**
 * struct used to schedule setCount event
 */
struct SetCountData {
    Gillespie *g;
    uint m;
    uint count;
    string comment;
    SetCountData(Gillespie *g, uint m, uint count, string comment)
        : g(g), m(m), count(count), comment(comment) {}
};

/**
 * Set a molecule count to a specified value
 * This function is called from the event scheduler.
 */
static void setCount(
    double stime, 
    double now, 
    SetCountData *data)
{
    data->g->setMoleculeCount(data->m, data->count);
    delete data;
}


/**
 * struct used to schedule setInhib event
 */
struct SetInhibData {
    Gillespie *g;
    uint r;
    double level;
    string comment;
    SetInhibData(Gillespie *g, uint r, double level, string comment)
        : g(g), r(r), level(level), comment(comment) {}
};

/**
 * Set a reaction's inhibition level to a specified value
 * This function is called from the event scheduler.
 */
static void setInhib(
    double stime, 
    double now, 
    SetInhibData *data)
{
    data->g->setReactionInhibition(data->r, data->level);
    delete data;
}

static uint checkParams(
    string directive,
    std::vector<string> tokens,
    uint min,
    uint max,
    string fname,
    uint lineNum)
{
    uint n = tokens.size();
    if (n < min || n > max) {
        fail(fname, lineNum, 
             "\"{}\" requires min {}, max {} parameters, found {}",
             directive,
             min, max,
             tokens.size());
    }
    return n;
}

/*
 * Read system from .gil file
 * @param fname File name
 * @erturn Number of lines read
 */
uint Gillespie::readGilFile(const char *fname)
{
    static bool overrideAllowed = false;

    const char *suffix = ".gil";
    FILE *fp = fopen(fname, "r");

    // If file named fname doesn't exist, and fname doesn't
    // end in suffix, then generously append suffix and try again
    //
    if (fp == NULL && errno == ENOENT && 
        strstr(fname, suffix) != fname + strlen(fname) - strlen(suffix)) 
    {
        static char buf[1024];
        strcpy(buf, fname);
        strcat(buf, suffix);
        fname = buf;
        fp = fopen(fname, "r");
    }

    if (fp == NULL) {
        perror(fname);
        exit(errno);
    }

    string errMsg;
    char line[1000];

    uint lineNum;
    for (lineNum = 1;
         fgets(line, sizeof(line), fp) != NULL;
         lineNum++) 
    {
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }
        stripComment(line);
        if (Util::isBlank(line)) continue;

        char *colonPos = strchr(line, ':');
        if (colonPos == NULL) {
            fail(fname, lineNum, "Bad directive (no colon)");
        }
        string directive =
            Util::wstrip(string(line).substr(0, colonPos - line));
        std::vector<string> tokens =
            tokenizeString(Util::wstrip(colonPos + 1), fname, lineNum);

        if (Util::strCiEq(directive, "include")) {
            checkParams("include", tokens, 1, 1, fname, lineNum);
            string path = tokens[0];
            if (path[0] != '/') {
                // Relative path. Prepend directory path of current file.
                //
                char fnameCopy[strlen(fname)+1];
                strcpy(fnameCopy, fname);
                path = string(dirname(fnameCopy)) + '/' + path;
            }
            readGilFile(path.c_str());
        } else if (Util::strCiEq(directive, "define")) {
            symSubst(tokens, 1);
            checkParams("define", tokens, 2, 2, fname, lineNum);
            if (defines.find(tokens[0]) != defines.end()) {
                fail(fname, lineNum, 
                     "redefinition: {}", tokens[0]);
            }
            evalArith(tokens[1], fname, lineNum);
            defines.insert(std::make_pair(tokens[0], tokens[1]));
        } else if (Util::strCiEq(directive, "volume")) {
            symSubst(tokens);
            checkParams("volume", tokens, 1, 1, fname, lineNum);
            volume = std::stod(tokens[0]);
        } else if (Util::strCiEq(directive, "runIdle")) {
            symSubst(tokens);
            checkParams("runIdle", tokens, 1, 1, fname, lineNum);
            runIdle = Util::strToBool(tokens[0], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum, "{}: {}", errMsg, line);
            }
        } else if (Util::strCiEq(directive, "idleTick")) {
            symSubst(tokens);
            checkParams("idleTick", tokens, 1, 1, fname, lineNum);
            idleTick = std::stod(tokens[0]);
        } else if (Util::strCiEq(directive, "molecule")) {
            symSubst(tokens, 1);
            uint nParams =
                checkParams("molecule", tokens, 2, 3, fname, lineNum);
            Molecule m(*this,
                       tokens[0],
                       Util::strToUint(tokens[1], errMsg),
                       nParams == 3 ? tokens[2] : "");
            if (!errMsg.empty()) {
                fail(fname, lineNum,
                     "{}: {}", errMsg, tokens[1]);
            }
            uint pos = moleculeIndex(tokens[0]);
            if (pos < molecules.size()) {
                if (overrideAllowed) {
                    molecules[pos] = m;
                } else {
                    fail(fname, lineNum,
                         "Duplicate molecule id: {}", tokens[0]);
                }
            } else {
#if 1
                molecules.push_back(m);
#else
                molecules.push_back(*this,
                                    Molecule(
                                        tokens[0],
                                        Util::strToUint(tokens[1], errMsg),
                                        nParams == 3 ? tokens[2] : ""));
#endif
            }
        } else if (Util::strCiEq(directive, "reaction")) {
            symSubst(tokens, 1);
            uint nParams =
                checkParams("reaction", tokens, 3, 4, fname, lineNum);
            expandReactionWildcard(tokens[0]);
            Reaction r = Reaction(
                *this,
                tokens[0],
                tokens[1],
                Util::strToDouble(tokens[2], errMsg),
                nParams == 4 ? tokens[3] : "");
            if (!errMsg.empty()) {
                fail(fname, lineNum, "{}: {}",
                     errMsg, 
                     tokens[2]);
            }

            r.parseFormula(fname, lineNum);
            uint pos = reactionIndex(tokens[0]);
            if (pos < reactions.size()) {
                if (overrideAllowed) {
                    reactions[pos] = r;
                } else {
                    fail(fname, lineNum,
                         "Duplicate reaction id: {}", tokens[0]);
                }
            } else {
                reactions.push_back(r);
            }
        } else if (Util::strCiEq(directive, "setcount")) {
            symSubst(tokens, 1);
            uint nParams =
                checkParams("setcount", tokens, 3, 4, fname, lineNum);
            string &id = tokens[0];
            uint m = moleculeIndex(id);
            if (m >= molecules.size()) {
                fail(fname, lineNum, "unknown molecule: {}", id);
            }
            double time = Util::strToDouble(tokens[1], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum,
                     "{}: {}", errMsg, tokens[1]);
            }
            uint count = Util::strToUint(tokens[2], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum,
                     "{}: {}", errMsg, tokens[2]);
            }
            string comment = nParams == 4 ? tokens[3] : "";

            SetCountData *sc = new SetCountData(this, m, count, comment);
            Sched::scheduleEvent(
                time,
                (Sched::VoidPtrCallback) setCount,
                sc);
        } else if (Util::strCiEq(directive, "setInhib")) {
            symSubst(tokens, 1);
            uint nParams =
                checkParams("setInhib", tokens, 3, 4, fname, lineNum);
            string &id = tokens[0];
            uint r = reactionIndex(id);
            if (r >= reactions.size()) {
                fail(fname, lineNum, "unknown reaction: {}", id);
            }

            double time = Util::strToDouble(tokens[1], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum,
                     "{}: {}", errMsg, tokens[1]);
            }
            double level = Util::strToDouble(tokens[2], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum, "{}: {}", errMsg, tokens[2]);
            }
            string comment = nParams == 4 ? tokens[3] : "";

            if (level < 0.0 || level > 1.0) {
                fail(fname, lineNum,
                     "Invalid inhibition level ({}), "
                     "must be between 0.0 and 1.0", tokens[2]);
            }
            
            SetInhibData *md = new SetInhibData(this, r, level, comment);
            Sched::scheduleEvent(
                time,
                (Sched::VoidPtrCallback) setInhib,
                md);
        } else if (Util::strCiEq(directive, "allowOverride")) {
            symSubst(tokens);
            checkParams("allowOverride", tokens, 1, 1, fname, lineNum);
            overrideAllowed = Util::strToBool(tokens[0], errMsg);
            if (!errMsg.empty()) {
                fail(fname, lineNum, "{}: {}", errMsg, line);
            }
        } else {
            fail(fname, lineNum, "Unknown directive: {}", directive);
        }
    }
    return lineNum;
}

/**
 * Verify and post-process data read from .gil file(s)
 * @param fname For error reportage
 * @param lineNum For error reportage
 */
void Gillespie::verify(const char *fname, uint lineNum)
{
    if (molecules.size() == 0) {
        fail(fname, lineNum, "No molecules specified");
    }
    if (reactions.size() == 0) {
        fail(fname, lineNum, "No reactions specified");
    }
    if (volume == 0) {
        fail(fname, lineNum, "volume not specified");
    }

    // Find the max cardinality for any reactant in any reaction
    //
    for (auto r : reactions) {
        for (auto m : r.left) {
            if (m > maxK) {
                maxK = m;
            }
        }
    }

    // Determine max reaction ID length
    //
    rwidth = 0;
    for (auto r : reactions) {
        rwidth = Util::max(rwidth, (uint) r.id.size());
    }

    // For each molecule, create a vector of reactions in
    // which it is a reactant.
    //
    for (uint r = 0; r < reactions.size(); r++) {
        for (uint m = 0; m < molecules.size(); m++) {
            if (reactions[r].left[m] != 0) {
                molecules[m].downstreamReactions.push_back(r);
            }
        }
    }
}


/**
 * Check if there are enough molecules for reaction r to happen
 */
bool Gillespie::isPossible(Reaction &r)
{
    for (uint m = 0; m < r.left.size(); m++) {
        if (r.left[m] > molecules[m].getCount()) {
            return false;
        }
    }
    return true;
}

/**
 * Make a header line for the output
 * @param molecules Array of molecules
 */
string Gillespie::makeHeader(
    const std::vector<Molecule> &molecules)
{
    fwidths.resize(molecules.size());

    string s = fmt::format("{:>{}}", "t", twidth);
    for (uint m = 0; m < molecules.size(); m++) {
        fwidths[m] = Util::max(mwidth, (uint) molecules[m].id.size() + 1);
        s += fmt::format( "{:>{}}", molecules[m].id.c_str(), fwidths[m]);
    }
    return s;
}

void Gillespie::printMolecules()
{
    fmt::print("Molecule Count Description                               Reactions\n");
    for (auto m : molecules) {
        fmt::print("{:9}{:7} {:40}{:3}: ", 
                   m.id, m.getCount(), m.description,
                   m.downstreamReactions.size());
        for (uint r : m.downstreamReactions) {
            fmt::print("{} ", reactions[r].id);
        }
        fmt::print("\n");
    }
}

void Gillespie::printReactions()
{
    fmt::print("{:{}.{}} Formula                      k     Description\n",
           "Reaction", rwidth, rwidth);
    for (auto r : reactions) {
        fmt::print("{:{}}{:25} {:8.3f} {}\n", 
                   r.id,
                   rwidth + 1,
                   r.formula,
                   r.k,
                   r.description);
    }
}
