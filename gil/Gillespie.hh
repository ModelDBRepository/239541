/**
 * @file Gillespie.hh
 *
 * Gillespie simulator
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

#ifndef GILLESPIE
#define GILLESPIE

#include <limits.h>
#include <float.h>

#include "Trace.hh"

class Gillespie {
public:
    /**
     * Constructor
     */
    Gillespie(
        const char *gilFileName,
        void (*preIterFunc)(double time) = NULL);
    
    /**
     * Print molecule info
     */
    void printMolecules();

    /**
     * Print reaction info
     */
    void printReactions();

    double calcReactProbs();
    
    /**
     * Run the Gillespie algorithm until (a) stopTime is reached, or (b) no
     * more reactions are possible, or (c) the count of a specified molecule
     * passes through a specified threshold (from either direction.)
     * 
     * @param plotInterval Time interval between molecule count outputs
     * @param stopTime Simulated time at which to stop unconditionally
     * @param monitorId Id of molecule to be monitored
     * @param threshold Stop after monitored molecule count reaches this
     *        value
     * @param monitorDelay Continue for this time interval after threshold
     *        reached
     * @param runIdle Keep running even if no reaction can run
     * @param idleTick Length of time step while idling
     */
    void run(
        double plotInterval,
        double stopTime,
        const char *monitorId = NULL,
        double threshold = -DBL_MAX,
        double monitorDelay = 0.0);

    /**
     * Member accessors
     */
    void setMwidth(uint w) { mwidth = w; }
    void setTwidth(uint w) { twidth = w; }
    void setRwidth(uint w) { rwidth = w; }
    void setMoleculeCount(uint id, uint count)
    {
        ABORT_IF(id > molecules.size(), "Invalid molecule id");
        molecules[id].setCount(count);
    }
    void setReactionInhibition(uint id, double inhibition)
    {
        ABORT_IF(id > reactions.size(), "Invalid reaction id");
        reactions[id].inhibition = inhibition;
    }
    
private:
    class Reaction;
    struct Molecule {
        string id;
        string description;

        // Reactions in which this molecule is a reactant
        //
        std::vector<uint> downstreamReactions;

        // Constructor
        //
        Molecule(Gillespie &g, string id, uint count, string description)
            : id(id),
              description(description),
              g(g),
              count(count)
        {}

        // Assignment operator
        //
        const Molecule &operator=(const Molecule &other)
        {
            return other;
        }

        // Copy constructor
        //
        Molecule(const Molecule &other)
            : id(other.id),
              description(other.description),
              g(other.g),
              count(other.count)
        {}

        void setCount(uint value)
        {
            count = value;
            for (auto r : downstreamReactions) {
                g.reactions[r].isDirty = true;
            }
        }

        uint getCount() { return count; }
    private:
        Gillespie &g;
        uint count;
    };

    class Reaction {
    public:
        string id;
        string formula;
        double k;                 // reaction rate (as used in
                                  // deterministic rate reaction)
        string description;
        double inhibition;        // (1.0 - inhibition) multiplies c to
                                  // yield effective reaction constant;
        std::vector<uint> left;   // number of each molecule 0..n
                                  // on left side
        std::vector<uint> right;  // number of each molecule 0..n
                                  // on right side

        uint h;                   // number of available reactant
                                  // combinations

        double a;                 // a*dt=prob that this reaction
                                  // happens in dt

        double c;                 // reaction constant

        bool isDirty;             // h and a need to be recalculated
        bool recalc;              // h and a were recalculated in last iteration
                                  // - used for debug printout only.
        
        // Constructor
        //
        Reaction(
            Gillespie &g,
            string id,
            string formula,
            double k,
            string description)
            : id(id),
              formula(formula),
              k(k),
              description(description),
              inhibition(0.0),
              isDirty(true),
              recalc(true),
              g(g)
        {}

        // Assignment operator
        //
        const Reaction &operator=(const Reaction &other)
        {
            return other;
        }

        // Copy constructor
        //
        Reaction(const Reaction &other)
            : id (other.id),
              formula(other.formula),
              k(other.k),
              description(other.description),
              inhibition(other.inhibition),
              left(other.left),
              right(other.right),
              h(other.h),
              a(other.a),
              c(other.c),
              isDirty(other.isDirty),
              recalc(other.recalc),
              g(other.g)
        {}
        
        /**
         * Parse the chemical formula into vectors of molecule
         * counts on the and right side of the reaction, and
         * calculate the stochastic reaction constant c.
         */
        void parseFormula(string fname, uint lineNum);

    private:
        Gillespie &g;
    };

    /*
     * Read system from .gil file
     * @param fp FILE pointer
     * @return Number of lines read
     */
    uint readGilFile(const char *fname);

    /**
     * Verify and post-process data read from .gil file(s)
     */
    void verify(const char *fname, uint numLines);

    /**
     * Expand a wildcard (*) in a reaction ID
     */
    void expandReactionWildcard(string &id);

    /**
     * Check if there are enough molecules for reaction r to happen
     * @param r Reaction
     */
    bool isPossible(Reaction &r);

    /**
     * Make a header line for the output
     * @param molecules Array of molecules
     */
    string makeHeader(const std::vector<Molecule> &molecules);

    double volume; // containment volume
    bool runIdle;  // whether to keep running when no reactions are possible
    double idleTick; // time step size while idling;
    std::vector<Molecule> molecules;
    std::vector<Reaction> reactions;
    void (*preIterFunc)(double time);
    uint twidth; // width of time field in output
    uint mwidth; // minimum width of molecule count field in output
    uint rwidth; // width of reaction name field in output
    std::vector<uint> fwidths; // field widths in output

    /**
     * Retrieve molecule index by id
     */
    uint moleculeIndex(string id);
    
    /**
     * Retrieve reaction index by id
     */
    uint reactionIndex(string id);
};

#endif
