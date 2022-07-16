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
 
#include <stdio.h>

#include <BLUtils.h>

#include <BLDebug.h>

#include "IPlug_include_in_plug_hdr.h"

#include "Partial2.h"

// Kalman
// "How much do we expect to our measurement vary"
//
// 200Hz (previously tested with 50Hz)
#define PT5_KF_E_MEA 0.01 // 200.0Hz
#define PT5_KF_E_EST PT5_KF_E_MEA

// "usually a small number between 0.001 and 1"
//
// If too low: predicted values move too slowly
// If too high: predicted values go straight
//
// 0.01: "oohoo" => fails when "EEAAooaa"
#define PT5_KF_Q 5.0 //2.0 // Was 1.0


unsigned long Partial2::mCurrentId = 0;

Partial2::Partial2()
: mKf(PT5_KF_E_MEA, PT5_KF_E_EST, PT5_KF_Q)
{
    mPeakIndex = 0;
    mLeftIndex = 0;
    mRightIndex = 0;
    
    mFreq = 0.0;
    mAmp = 0.0;    
    mPhase = 0.0;
    
    mState = ALIVE;
    
    mId = -1;
    mLinkedId = -1;
    
    mWasAlive = false;
    mZombieAge = 0;
    
    mAge = 0;
    
    mCookie = 0.0;

    mBinIdxF = 0.0;
    mAlpha0 = 0.0;
    mBeta0 = 0.0;
}

    
Partial2::Partial2(const Partial2 &other)
: mKf(other.mKf)
{
    mPeakIndex = other.mPeakIndex;
    mLeftIndex = other.mLeftIndex;
    mRightIndex = other.mRightIndex;
    
    mFreq = other.mFreq;
    mAmp = other.mAmp;
    
    mPhase = other.mPhase;
        
    mState = other.mState;;
        
    mId = other.mId;
    mLinkedId = other.mLinkedId;
    
    mWasAlive = other.mWasAlive;
    mZombieAge = other.mZombieAge;
    
    mAge = other.mAge;
    
    mCookie = other.mCookie;

    // QIFFT
    mBinIdxF = other.mBinIdxF;
    mAlpha0 = other.mAlpha0;
    mBeta0 = other.mBeta0;
}

Partial2::~Partial2() {}

void
Partial2::GenNewId()
{
    mId = mCurrentId++;
}
    
bool
Partial2::FreqLess(const Partial2 &p1, const Partial2 &p2)
{
    return (p1.mFreq < p2.mFreq);
}

bool
Partial2::AmpLess(const Partial2 &p1, const Partial2 &p2)
{
    return (p1.mAmp < p2.mAmp);
}

bool
Partial2::IdLess(const Partial2 &p1, const Partial2 &p2)
{
    return (p1.mId < p2.mId);
}

bool
Partial2::CookieLess(const Partial2 &p1, const Partial2 &p2)
{
    return (p1.mCookie < p2.mCookie);
}

void
Partial2::DBG_PrintPartials(const vector<Partial2> &partials)
{
    fprintf(stderr, "-------------------\n");
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &p = partials[i];
        
        fprintf(stderr, "id: %ld state: %d amp: %g freq: %g\n",
                p.mId, p.mState, p.mAmp, p.mFreq);
    }
}

void
Partial2::DBG_DumpPartials(const char *fileName,
                           const vector<Partial2> &partials, int size,
                           bool normFreq, bool db)
{
#define MIN_AMP_DB -120.0
    
    WDL_TypedBuf<BL_FLOAT> data;
    data.Resize(size);
    BLUtils::FillAllZero(&data);
    
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &p = partials[i];

        BL_FLOAT freq = p.mFreq;
        if (!normFreq)
            freq /= 22050.0; // Hard coded
        
        int idx = freq*size;
        if (idx >= size)
            continue;

        BL_FLOAT amp = p.mAmp;
        if (db)
            amp = (p.mAmp - MIN_AMP_DB)/(0.0 - MIN_AMP_DB);
        data.Get()[idx] = amp; //p.mAmp;
    }

    BLDebug::DumpData(fileName, data);
}
