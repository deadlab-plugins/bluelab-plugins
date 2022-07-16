/* Copyright (C) 2022 Nicolas Dittlo <deadlab.plugins@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this software; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef PARTIAL2_H
#define PARTIAL2_H

#include <vector>
using namespace std;

#include <SimpleKalmanFilter.h>

#include <BLTypes.h>

class Partial2
{
 public:
    enum State
    {
        ALIVE = 0,
        ZOMBIE,
        DEAD
    };
        
    Partial2();
        
    Partial2(const Partial2 &other);
        
    virtual ~Partial2();
        
    void GenNewId();
        
    //
    static bool FreqLess(const Partial2 &p1, const Partial2 &p2);
        
    static bool AmpLess(const Partial2 &p1, const Partial2 &p2);
        
    static bool IdLess(const Partial2 &p1, const Partial2 &p2);
        
    static bool CookieLess(const Partial2 &p1, const Partial2 &p2);

    // Debug
    static void DBG_PrintPartials(const vector<Partial2> &partials);
    static void DBG_DumpPartials(const char *fileName,
                                 const vector<Partial2> &partials, int size,
                                 bool normFreq = true, bool db = true);
    
 public:
    int mPeakIndex;
    int mLeftIndex;
    int mRightIndex;
        
    // When detecting and filtering, mFreq and mAmp are "scaled and normalized"
    // After processing, we can compute the real frequencies in Hz and amp in dB.
    BL_FLOAT mFreq;
    union{
        // Inside PartialTracker6
        BL_FLOAT mAmp;
            
        // After, outside PartialTracker6, if external classes need amp in dB
        // Need to call DenormPartials() then PartialsAmpToAmpDB()
        BL_FLOAT mAmpDB;
    };
    BL_FLOAT mPhase;

    // Partial id
    long mId;
    // Index of the linked partial in the current array (for associating)
    long mLinkedId;
    
    enum State mState;
        
    bool mWasAlive;
    long mZombieAge;
        
    long mAge;
        
    // All-purpose field
    BL_FLOAT mCookie;

    // Used e.g for PartialFilterMarchand
    SimpleKalmanFilter mKf;

    // QIFFT
    BL_FLOAT mBinIdxF;
    BL_FLOAT mAlpha0;
    BL_FLOAT mBeta0;

    // amp + alpha0, summed with the correct scales
    BL_FLOAT mAmpAlpha0; // Only for displaying
    
 protected:
    static unsigned long mCurrentId;
};

#endif
