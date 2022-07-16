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

#include <cmath>
#include <algorithm>
using namespace std;

#include <BLUtils.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>

#include <Hungarian.h>

#include "PartialFilterAMFM2.h"

// 3 is often good
//#define MAX_ZOMBIE_AGE 3 //2 //5 //2
// 5 avoids changing id when partial crossing
//#define MAX_ZOMBIE_AGE 5
#define MAX_ZOMBIE_AGE 1 // TEST NEW

// Must keep history size >= 3, for FixPartialsCrossing
#define PARTIALS_HISTORY_SIZE 3 //2


#define EXTRAPOLATE_KALMAN 0 // 1
// Propagate dead and zombies with alpha0 and beta0
// Problem: at partial crossing, alpha0 (for amp) sometimes has big values 
#define EXTRAPOLATE_AMFM 0 //1

// NOTE: when using time smoothing on magns, it created fake amp dirivatives (alpha0)
// So the following algorithms won't be optimal
// But finally the result looks good with time smooth

#define ASSOC_SIMPLE_AMFM 1 //0 //1 // ORIGIN
// Result almost similar to ASSOC_SIMPLE_AMFM
#define ASSOC_SIMPLE_NERI 0 //1 //0 // NOTE: don't forget the hack zetaA = 50.0
#define ASSOC_HUNGARIAN_AMFM 0 //1
#define ASSOC_HUNGARIAN_NERI 0 //1

#define RESCALE_HZ 0 //1

#define FIX_PARTIAL_CROSSING 1 //0 //1

#define DISCARD_BIG_JUMPS 1 //0

#define DISCARD_OPPOSITE_DIRECTION 0 //1

#define OPTIM_TRAPEZOID_AREA 1 // 0

#define OPTIM_SAMPLES_SYNTH_SORTED_VEC 1 // 0

// WIP...
#define HARD_OPTIM 1 //0 //1

PartialFilterAMFM2::PartialFilterAMFM2(int bufferSize, BL_FLOAT sampleRate)
{    
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;

    mNeriDelta = 0.2;
}

PartialFilterAMFM2::~PartialFilterAMFM2() {}

void
PartialFilterAMFM2::Reset(int bufferSize, BL_FLOAT sampleRate)
{    
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;
    
    mPartials.clear();
}
           
void
PartialFilterAMFM2::FilterPartials(vector<Partial2> *partials)
{
#if RESCALE_HZ
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &p = (*partials)[i];
        p.mFreq *= mSampleRate*0.5;
        p.mBeta0 *= mSampleRate*0.5;
    }
#endif
    
    //DBG_PrintPartials(*partials);
    
    mPartials.push_front(*partials);
    
    while(mPartials.size() > PARTIALS_HISTORY_SIZE)
        mPartials.pop_back();
    
    partials->clear();
    
    if (mPartials.empty())
        return;
    
    if (mPartials.size() == 1)
        // Assigne ids to the first series of partials
    {
        for (int j = 0; j < mPartials[0].size(); j++)
        {
            Partial2 &currentPartial = mPartials[0][j];
            currentPartial.GenNewId();
        }
        
        // Not enough partials to filter, need 2 series
        return;
    }
    
    if (mPartials.size() < 2)
        return;
    
    const vector<Partial2> &prevPartials = mPartials[1];
    vector<Partial2> &currentPartials = mTmpPartials0;
    currentPartials = mPartials[0];
    
    // Partials that was not associated at the end
    vector<Partial2> &remainingCurrentPartials = mTmpPartials1;
    remainingCurrentPartials.resize(0);

    // DEBUG
    //DBG_DumpPartials("prev.txt", prevPartials, mBufferSize);
    //DBG_DumpPartials("cur.txt", currentPartials, mBufferSize);

    //DBG_PrintPartials(prevPartials);
        
#if ASSOC_SIMPLE_AMFM
    AssociatePartialsAMFM(prevPartials, &currentPartials, &remainingCurrentPartials);
    //AssociatePartialsAMFMSimple(prevPartials, &currentPartials,
    //                            &remainingCurrentPartials);
#endif

#if ASSOC_SIMPLE_NERI
    AssociatePartialsNeri(prevPartials, &currentPartials,
                          &remainingCurrentPartials);
#endif
    
#if ASSOC_HUNGARIAN_AMFM
    AssociatePartialsHungarianAMFM(prevPartials, &currentPartials,
                                   &remainingCurrentPartials);
#endif

#if ASSOC_HUNGARIAN_NERI
    AssociatePartialsHungarianNeri(prevPartials, &currentPartials,
                                   &remainingCurrentPartials);
#endif
    
    vector<Partial2> &deadZombiePartials = mTmpPartials7;
    ComputeZombieDeadPartials(prevPartials, currentPartials, &deadZombiePartials);
    
    // Add zombie and dead partial
    for (int i = 0; i < deadZombiePartials.size(); i++)
        currentPartials.push_back(deadZombiePartials[i]);

#if FIX_PARTIAL_CROSSING
    if (mPartials.size() >= 3)
    {
#if OPTIM_SAMPLES_SYNTH_SORTED_VEC
        sort(mPartials[1].begin(), mPartials[1].end(), Partial2::IdLess);
        sort(mPartials[2].begin(), mPartials[2].end(), Partial2::IdLess);
#endif
    
        FixPartialsCrossing(mPartials[2], mPartials[1], &currentPartials);
    }
#endif
    
    // At the end, there remains the partial that have not been matched
    //
    // Add them at to the history for next time
    //
    for (int i = 0; i < remainingCurrentPartials.size(); i++)
    {
        Partial2 p = remainingCurrentPartials[i];
        
        p.GenNewId();
        
        currentPartials.push_back(p);
    }

    // Then sort the new partials by frequency
    sort(currentPartials.begin(), currentPartials.end(), Partial2::FreqLess);
    
    //
    // Update: add the partials to the history
    // (except the dead ones)
    mPartials[0].clear();
    for (int i = 0; i < currentPartials.size(); i++)
    {
        const Partial2 &currentPartial = currentPartials[i];

#if 0 // ORIGIN
        // TEST: do not skip the dead partials:
        // they will be used for fade out !
        //if (currentPartial.mState != Partial2::DEAD)
        mPartials[0].push_back(currentPartial);
#endif
#if 1
        if (currentPartial.mState != Partial2::DEAD)
            mPartials[0].push_back(currentPartial);
#endif
    }

    *partials = mPartials[0];

#if RESCALE_HZ
    BL_FLOAT coeff = 1.0/(mSampleRate*0.5);
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &p = (*partials)[i];
        p.mFreq *= coeff;
        p.mBeta0 *= coeff;

        //#if !EXTRAPOLATE_KALMAN
        //p.mPredictedFreq = p.mFreq;
        //#endif
    }
#endif
}

void
PartialFilterAMFM2::SetNeriDelta(BL_FLOAT delta)
{
    mNeriDelta = delta;
}

void
PartialFilterAMFM2::
AssociatePartialsAMFMSimple(const vector<Partial2> &prevPartials,
                            vector<Partial2> *currentPartials,
                            vector<Partial2> *remainingCurrentPartials)
{
    // Quick optimization (avoid long freezing)
    // (later, will use hungarian)
    //#define MAX_NUM_ITER 5
    
    // Sometimes need more than 5 (and less than 10)
    // When threshold is near 1%
    // (Sometimes it never solves totally and would lead to infinite num iters)
#define MAX_NUM_ITER 10
    
    // Sort current partials and prev partials by increasing frequency
    sort(currentPartials->begin(), currentPartials->end(), Partial2::FreqLess);
    
    vector<Partial2> &prevPartials0 = mTmpPartials5;
    prevPartials0 = prevPartials;

#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
    sort(prevPartials0.begin(), prevPartials0.end(), Partial2::FreqLess);
#else
    sort(prevPartials0.begin(), prevPartials0.end(), Partial2::IdLess);
#endif
    
    // Associated partials
    bool stopFlag = true;
    int numIters = 0;
    do {
        stopFlag = true;

        numIters++;
        
        for (int i = 0; i < prevPartials0.size(); i++)
        {
            const Partial2 &prevPartial = prevPartials0[i];
            
            // Check if the link is already done
            if (((int)prevPartial.mId != -1) &&
                (FindPartialById(*currentPartials, (int)prevPartial.mId) != -1))
                // Already linked
                continue;
            
            for (int j = 0; j < currentPartials->size(); j++)
            {
                Partial2 &currentPartial = (*currentPartials)[j];
                
                if (currentPartial.mId == prevPartial.mId)
                    continue;
                
                BL_FLOAT LA = ComputeLA(prevPartial, currentPartial);
                BL_FLOAT LF = ComputeLF(prevPartial, currentPartial);

                bool discard = false;
#if DISCARD_BIG_JUMPS
                discard = CheckDiscardBigJump(prevPartial, currentPartial);
#endif

#if DISCARD_OPPOSITE_DIRECTION
                discard = CheckDiscardOppositeDirection(prevPartial, currentPartial);
#endif
                
                // As is the paper
                if ((LA > 0.5) && (LF > 0.5) &&
                    // Avoid big jumps or similar
                    !discard)
                     
                    // Associate!
                {
                    // Current partial already has an id
                    bool mustFight0 = (currentPartial.mId != -1);

                    int fight1Idx = FindPartialById(*currentPartials,
                                                    (int)prevPartial.mId);
                    // Prev partial already has some association with the current id
                    bool mustFight1 = (fight1Idx != -1);
                        
                    if (!mustFight0 && !mustFight1)
                    {
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;
                        
#if EXTRAPOLATE_KALMAN
                        currentPartial.mKf = prevPartial.mKf; //
#endif
                        
                        stopFlag = false;
                        
                        continue;
                    }
                        
                    // Fight!
                    //
                    
                    // Find the previous link for case 0
#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
                    int otherPrevIdx =
                        FindPartialById(prevPartials0, (int)currentPartial.mId);
#else
                    int otherPrevIdx =
                        FindPartialByIdSorted(prevPartials0, currentPartial);
#endif
                    
                    // Find prev partial
                    Partial2 prevPartialFight =
                        mustFight0 ? prevPartials0[otherPrevIdx] : prevPartial;
                    // Find current partial
                    Partial2 currentPartialFight =
                        mustFight0 ? currentPartial : (*currentPartials)[fight1Idx];

                    // Compute scores
                    BL_FLOAT otherLA =
                        ComputeLA(prevPartialFight, currentPartialFight);
                    BL_FLOAT otherLF =
                        ComputeLF(prevPartialFight, currentPartialFight);
                    
                    // Joint likelihood
                    BL_FLOAT j0 = LA*LF;
                    BL_FLOAT j1 = otherLA*otherLF;
                    if (j0 > j1)
                        // Current partial won
                    {
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;
                        
#if EXTRAPOLATE_KALMAN
                        currentPartial.mKf = prevPartial.mKf; //
#endif

                        // Disconnect for case 1
                        if (mustFight1)
                            (*currentPartials)[fight1Idx].mId = -1;
                        stopFlag = false;
                    }
                    else
                        // Other partial won
                    {
                        // Just keep it like it is!
                    }
                }
            }
        }

        // Quick optimization
        if (numIters > MAX_NUM_ITER)
            break;
        
    } while (!stopFlag);
    
    // Update partials
    vector<Partial2> &newPartials = mTmpPartials6;
    newPartials.resize(0);
    
    for (int j = 0; j < currentPartials->size(); j++)
    {
        Partial2 &currentPartial = (*currentPartials)[j];

        if (currentPartial.mId != -1)
        {
            currentPartial.mState = Partial2::ALIVE;
            currentPartial.mWasAlive = true;
    
            // Increment age
            currentPartial.mAge = currentPartial.mAge + 1;

#if 0 // No need
            
#if EXTRAPOLATE_KALMAN
            ExtrapolatePartialKalman(&currentPartial);
#endif
            
#if EXTRAPOLATE_AMFM
            ExtrapolatePartialAMFM(&currentPartial);
#endif

#endif
            
            newPartials.push_back(currentPartial);
        }
    }

    // NOTE: sometimes would need an "infinite number of iterations"..
    
    // Add the remaining partials
    remainingCurrentPartials->clear();
    for (int i = 0; i < currentPartials->size(); i++)
    {
        const Partial2 &p = (*currentPartials)[i];
        if (p.mId == -1)
            remainingCurrentPartials->push_back(p);
    }
    
    // Update current partials
    *currentPartials = newPartials;
}

void
PartialFilterAMFM2::
AssociatePartialsAMFM(const vector<Partial2> &prevPartials,
                      vector<Partial2> *currentPartials,
                      vector<Partial2> *remainingCurrentPartials)
{    
    // Quick optimization (avoid long freezing)
    // (later, will use hungarian)
    //#define MAX_NUM_ITER 5
    
    // Sometimes need more than 5 (and less than 10)
    // When threshold is near 1%
    // (Sometimes it never solves totally and would lead to infinite num iters)
#define MAX_NUM_ITER 10 //4
    // DEBUG: 4
    
    // Problem: we miss the highest freqs if != 2048
#define NUM_STEPS_LOOKUP 8 //4 //2048 // 128 //4
    // DEBUG: 4

#if HARD_OPTIM
#define MAX_NUM_ITER 4
    
    // Problem: we miss the highest freqs if != 2048
#define NUM_STEPS_LOOKUP 4
#endif
    
    // Sort current partials and prev partials by increasing frequency
    sort(currentPartials->begin(), currentPartials->end(), Partial2::FreqLess);
    
    vector<Partial2> &prevPartials0 = mTmpPartials5;
    prevPartials0 = prevPartials;

    sort(prevPartials0.begin(), prevPartials0.end(), Partial2::FreqLess);

    // Reset the links
    for (int i = 0; i < prevPartials0.size(); i++)
        prevPartials0[i].mLinkedId = -1;
    for (int i = 0; i < currentPartials->size(); i++)
    {
        (*currentPartials)[i].mLinkedId = -1;

        // Just in case
        (*currentPartials)[i].mId = -1;
    }
    
    // Associated partials
    bool stopFlag = true;
    int numIters = 0;
    
    do {
        stopFlag = true;

        numIters++;
        
        for (int i = 0; i < prevPartials0.size(); i++)
        {
            Partial2 &prevPartial = prevPartials0[i];

            // Check if the link is already done
            if (((int)prevPartial.mId != -1) &&
                (prevPartial.mLinkedId != -1) &&
                ((*currentPartials)[prevPartial.mLinkedId].mLinkedId == i))
                // Already linked
                continue;
            
            int nearestFreqId =
                FindNearestFreqId(*currentPartials, prevPartial.mFreq, i);
            
            for (int j = nearestFreqId - NUM_STEPS_LOOKUP/2;
                 j < nearestFreqId + NUM_STEPS_LOOKUP/2; j++)
            {
                if ((j < 0) || (j >= currentPartials->size()))
                    continue;
                
                Partial2 &currentPartial = (*currentPartials)[j];
                
                if (currentPartial.mId == prevPartial.mId)
                    continue;
                
#if DISCARD_BIG_JUMPS
                bool discard = CheckDiscardBigJump(prevPartial, currentPartial);
                if (discard)
                    continue;
#endif
                
                BL_FLOAT LA = ComputeLA(prevPartial, currentPartial);
                BL_FLOAT LF = ComputeLF(prevPartial, currentPartial);
                
                // As is the paper
                if ((LA > 0.5) && (LF > 0.5))
                    // Associate!
                {
                    // Current partial already has an id
                    bool mustFight0 = (currentPartial.mId != -1);

                    int fight1Idx = prevPartial.mLinkedId;
                    
                    // Prev partial already has some association with the current id
                    bool mustFight1 = (fight1Idx != -1);
                        
                    if (!mustFight0 && !mustFight1)
                    {
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;

                        currentPartial.mLinkedId = i;
                        prevPartial.mLinkedId = j;
                            
                        stopFlag = false;
                        
                        continue;
                    }
                    
                    // Fight!
                    //
                    
                    // Find the previous link for case 0
                    int otherPrevIdx = currentPartial.mLinkedId;

                    if ((otherPrevIdx == -1) && mustFight0)
                        continue;

                    if ((fight1Idx == -1) && !mustFight0)
                        continue;
                    
                    // Find prev partial
                    Partial2 prevPartialFight =
                        mustFight0 ? prevPartials0[otherPrevIdx] : prevPartial;
                    // Find current partial
                    Partial2 currentPartialFight =
                        mustFight0 ? currentPartial : (*currentPartials)[fight1Idx];

                    // Compute scores
                    BL_FLOAT otherLA =
                        ComputeLA(prevPartialFight, currentPartialFight);
                    BL_FLOAT otherLF =
                        ComputeLF(prevPartialFight, currentPartialFight);
                    
                    // Joint likelihood
                    BL_FLOAT j0 = LA*LF;
                    BL_FLOAT j1 = otherLA*otherLF;
                    if (j0 > j1)
                        // Current partial won
                    {
                        // Disconnect for case 1
                        if (mustFight1)
                        {
                            int prevPartialIdx =
                                (*currentPartials)[fight1Idx].mLinkedId;
                            if (prevPartialIdx != -1)
                                prevPartials0[prevPartialIdx].mLinkedId = -1;
                            
                            (*currentPartials)[fight1Idx].mId = -1;
                            (*currentPartials)[fight1Idx].mLinkedId = -1;
                        }
                        
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;

                        currentPartial.mLinkedId = i;
                        prevPartial.mLinkedId = j;
                        
                        stopFlag = false;
                    }
                    else
                        // Other partial won
                    {
                        // Just keep it like it is!
                    }
                }
            }
        }

        // Quick optimization
        if (numIters > MAX_NUM_ITER)
            break;
        
    } while (!stopFlag);
    
    // Update partials
    vector<Partial2> &newPartials = mTmpPartials6;
    newPartials.resize(0);
    
    for (int j = 0; j < currentPartials->size(); j++)
    {
        Partial2 &currentPartial = (*currentPartials)[j];

        if (currentPartial.mId != -1)
        {
            currentPartial.mState = Partial2::ALIVE;
            currentPartial.mWasAlive = true;
    
            // Increment age
            currentPartial.mAge = currentPartial.mAge + 1;
            
            newPartials.push_back(currentPartial);
        }
    }
    
    // NOTE: sometimes would need an "infinite number of iterations"..
    
    // Add the remaining partials
    remainingCurrentPartials->clear();
    for (int i = 0; i < currentPartials->size(); i++)
    {
        const Partial2 &p = (*currentPartials)[i];
        if (p.mId == -1)
            remainingCurrentPartials->push_back(p);
    }
    
    // Update current partials
    *currentPartials = newPartials;
}

long
PartialFilterAMFM2::FindNearestFreqId(const vector<Partial2> &partials,
                                     BL_FLOAT freq, int index)
{
    //if (index >= partials.size())
    //    return -1;

    if (index > partials.size() - 1)
        index = partials.size() - 1; 
    
    if (partials[index].mFreq < freq)
    {
        for (int i = index; i < partials.size(); i++)
        {
            if (partials[i].mFreq > freq)
            {
                BL_FLOAT d20 = partials[i].mFreq - freq;
                BL_FLOAT d21 = freq - partials[i - 1].mFreq;
                
                if (d20 < d21)
                    return i;
                else
                    return (i - 1);
            }
        }
    }
    else if (partials[index].mFreq > freq)
    {
        for (int i = index; i >= 0; i--)
        {
            if (partials[i].mFreq < freq)
            {
                BL_FLOAT d20 = freq - partials[i].mFreq;
                BL_FLOAT d21 = partials[i + 1].mFreq - freq;
                
                if (d20 < d21)
                    return i;
                else
                    return (i + 1);
            }
        }
    }

    // Index corresponds exactly to the lookup frequency
    return index;
}

void
PartialFilterAMFM2::
AssociatePartialsNeri(const vector<Partial2> &prevPartials,
                      vector<Partial2> *currentPartials,
                      vector<Partial2> *remainingCurrentPartials)
{
    // Parameters
    //BL_FLOAT delta = 0.2;
    BL_FLOAT delta = mNeriDelta;
    BL_FLOAT zetaF = 50.0; // in Hz
    BL_FLOAT zetaA = 15; // in dB

#if !RESCALE_HZ
    zetaF *= 1.0/(mSampleRate*0.5);
#endif
    
    // These can't be <= 0
    if (zetaF < 1e-1)
        zetaF = 1e-1;
    if (zetaA < 1e-1)
        zetaA = 1e-1;
 
    //Convert to log from dB
    zetaA = zetaA/20*log(10);
    
    // Quick optimization (avoid long freezing)
    // (later, will use hungarian)
    //#define MAX_NUM_ITER 5
    
    // Sometimes need more than 5 (and less than 10)
    // When threshold is near 1%
    // (Sometimes it never solves totally and would lead to infinite num iters)
#define MAX_NUM_ITER 10
    
    // Sort current partials and prev partials by increasing frequency
    sort(currentPartials->begin(), currentPartials->end(), Partial2::FreqLess);
    
    vector<Partial2> &prevPartials0 = mTmpPartials5;
    prevPartials0 = prevPartials;

#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
    sort(prevPartials0.begin(), prevPartials0.end(), Partial2::FreqLess);
#else
    sort(prevPartials0.begin(), prevPartials0.end(), Partial2::IdLess);
#endif
    
    // Associated partials
    bool stopFlag = true;
    int numIters = 0;
    do {
        stopFlag = true;

        numIters++;
        
        for (int i = 0; i < prevPartials0.size(); i++)
        {
            const Partial2 &prevPartial = prevPartials0[i];
            
            // Check if the link is already done
            if (((int)prevPartial.mId != -1) &&
                (FindPartialById(*currentPartials, (int)prevPartial.mId) != -1))
                // Already linked
                continue;
            
            for (int j = 0; j < currentPartials->size(); j++)
            {
                Partial2 &currentPartial = (*currentPartials)[j];
                
                if (currentPartial.mId == prevPartial.mId)
                    continue;

                // Compute current score
                BL_FLOAT A;
                BL_FLOAT B;
                ComputeCostNeri(prevPartial, currentPartial,
                                delta, zetaF, zetaA,
                                &A, &B);

                BL_FLOAT cost = MIN(A, B);
                
                bool discard = false;
#if DISCARD_BIG_JUMPS
                discard = CheckDiscardBigJump(prevPartial, currentPartial);
#endif

#if DISCARD_OPPOSITE_DIRECTION
                discard = CheckDiscardOppositeDirection(prevPartial, currentPartial);
#endif
                
                // Avoid big jumps or similar
                if (!discard)
                    // Associate!
                {
                    // Current partial already has an id
                    bool mustFight0 = (currentPartial.mId != -1);

                    int fight1Idx = FindPartialById(*currentPartials,
                                                    (int)prevPartial.mId);
                    // Prev partial already has some association with the current id
                    bool mustFight1 = (fight1Idx != -1);
                        
                    if (!mustFight0 && !mustFight1)
                    {
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;
                        
#if EXTRAPOLATE_KALMAN
                        currentPartial.mKf = prevPartial.mKf; //
#endif
                        
                        stopFlag = false;
                        
                        continue;
                    }
                        
                    // Fight!
                    //
                    
                    // Find the previous link for case 0
#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
                    int otherPrevIdx =
                        FindPartialById(prevPartials0, (int)currentPartial.mId);
#else
                    int otherPrevIdx =
                        FindPartialByIdSorted(prevPartials0, currentPartial);
#endif
                    
                    // Find prev partial
                    Partial2 prevPartialFight =
                        mustFight0 ? prevPartials0[otherPrevIdx] : prevPartial;
                    // Find current partial
                    Partial2 currentPartialFight =
                        mustFight0 ? currentPartial : (*currentPartials)[fight1Idx];

                    // Compute other score
                    BL_FLOAT otherA;
                    BL_FLOAT otherB;
                    ComputeCostNeri(prevPartialFight, currentPartialFight,
                                    delta, zetaF, zetaA,
                                    &otherA, &otherB);
                    
                    BL_FLOAT otherCost = MIN(otherA, otherB);
                    
                    if (cost < otherCost)
                    //if ((A < otherA) && (B < otherB))
                        // Current partial won
                    {
                        currentPartial.mId = prevPartial.mId;
                        currentPartial.mAge = prevPartial.mAge;
                        
#if EXTRAPOLATE_KALMAN
                        currentPartial.mKf = prevPartial.mKf; //
#endif

                        // Disconnect for case 1
                        if (mustFight1)
                            (*currentPartials)[fight1Idx].mId = -1;
                        stopFlag = false;
                    }
                    else
                        // Other partial won
                    {
                        // Just keep it like it is!
                    }
                }
            }
        }

        // Quick optimization
        if (numIters > MAX_NUM_ITER)
            break;
        
    } while (!stopFlag);
    
    // Update partials
    vector<Partial2> &newPartials = mTmpPartials6;
    newPartials.resize(0);
    
    for (int j = 0; j < currentPartials->size(); j++)
    {
        Partial2 &currentPartial = (*currentPartials)[j];

        if (currentPartial.mId != -1)
        {
            currentPartial.mState = Partial2::ALIVE;
            currentPartial.mWasAlive = true;
    
            // Increment age
            currentPartial.mAge = currentPartial.mAge + 1;

#if 0 // No need
            
#if EXTRAPOLATE_KALMAN
            ExtrapolatePartialKalman(&currentPartial);
#endif
            
#if EXTRAPOLATE_AMFM
            ExtrapolatePartialAMFM(&currentPartial);
#endif

#endif
            
            newPartials.push_back(currentPartial);
        }
    }

    // NOTE: sometimes would need an "infinite number of iterations"..
    
    // Add the remaining partials
    remainingCurrentPartials->clear();
    for (int i = 0; i < currentPartials->size(); i++)
    {
        const Partial2 &p = (*currentPartials)[i];
        if (p.mId == -1)
            remainingCurrentPartials->push_back(p);
    }
    
    // Update current partials
    *currentPartials = newPartials;
}

void
PartialFilterAMFM2::
AssociatePartialsHungarianAMFM(const vector<Partial2> &prevPartials,
                               vector<Partial2> *currentPartials,
                               vector<Partial2> *remainingCurrentPartials)
{
#define HUNGARIAN_INF BL_INF
    //#define HUNGARIAN_INF 1.0 //10.0

    // ORIGIN: 0
    // => looks quite similar with or without...
#define FORCE_SQUARE_MATRIX 0 //1
    
#if FORCE_SQUARE_MATRIX
    int maxDim = (prevPartials.size() > currentPartials->size()) ?
        prevPartials.size() : currentPartials->size();

    // Init cost matrix (MxN)
    vector<vector<BL_FLOAT> > costMatrix;
    costMatrix.resize(maxDim);
    for (int i = 0; i < costMatrix.size(); i++)
        costMatrix[i].resize(maxDim);
#else 
    // Init cost matrix (MxN)
    vector<vector<BL_FLOAT> > costMatrix;
    costMatrix.resize(prevPartials.size());
    for (int i = 0; i < costMatrix.size(); i++)
        costMatrix[i].resize(currentPartials->size());
#endif
    
#if FORCE_SQUARE_MATRIX
    // Fill with dummy values
    for (int i = 0; i < costMatrix.size(); i++)
        for (int j = 0; j < costMatrix[i].size(); j++)
            costMatrix[i][j] = 0.0;
#endif
    
    // Fill the cost matrix
    for (int i = 0; i < costMatrix.size(); i++)
    {
        for (int j = 0; j < costMatrix[i].size(); j++)
        {
#if FORCE_SQUARE_MATRIX
            if (i >= prevPartials.size())
                continue;
            if (j >= currentPartials->size())
                continue;
#endif
            
            BL_FLOAT LA = ComputeLA(prevPartials[i], (*currentPartials)[j]);
            BL_FLOAT LF = ComputeLF(prevPartials[i], (*currentPartials)[j]);
            
            bool discard = false;
#if DISCARD_BIG_JUMPS
            discard = CheckDiscardBigJump(prevPartials[i], (*currentPartials)[j]);
#endif

#if 1 // Check discard?
            if ((LA < 0.5) || (LF < 0.5) ||
                // Avoid big jumps or similar
                discard)
                costMatrix[i][j] = HUNGARIAN_INF; 
            else
#endif
                costMatrix[i][j] = 1.0 - LA*LF;
        }
    }

#if 0 //1 // DEBUG
    BLDebug::ResetFile("matrix.txt");
    for (int i = 0; i < costMatrix.size(); i++)
    {
        for (int j = 0; j < costMatrix[i].size(); j++)
        {
            BL_FLOAT val = costMatrix[i][j];
            
            BLDebug::AppendValue("matrix.txt", val);
        }
    }
#endif
    
    // Solve
    HungarianAlgorithm HungAlgo;
	vector<int> assignment;
	BL_FLOAT cost = HungAlgo.Solve(costMatrix, assignment);
    
    for (int i = 0; i < assignment.size(); i++)
    {
        int a = assignment[i];

#if FORCE_SQUARE_MATRIX
        if (i >= prevPartials.size())
            continue;
        if (a >= currentPartials->size())
            continue;
#endif
        
        // If num prev > num current, there will be some unassigned partials
        // (int this case, assignment is -1)
        if ((a != -1) && (prevPartials[i].mId != -1))
            (*currentPartials)[a].mId = prevPartials[i].mId;
    }

    vector<Partial2> newPartials;
    
    // Add the remaining partials
    remainingCurrentPartials->clear();
    for (int i = 0; i < currentPartials->size(); i++)
    {
        Partial2 &p = (*currentPartials)[i];
        if (p.mId != -1)
        {
            p.mState = Partial2::ALIVE;
            p.mWasAlive = true;
    
            // Increment age
            p.mAge = p.mAge + 1;
            
            newPartials.push_back(p);
        }
        else          
            remainingCurrentPartials->push_back(p);
    }

    *currentPartials = newPartials;
}

// Compute score like in the paper
void
PartialFilterAMFM2::
AssociatePartialsHungarianNeri(const vector<Partial2> &prevPartials,
                               vector<Partial2> *currentPartials,
                               vector<Partial2> *remainingCurrentPartials)
{
    // Parameters
    /*const*/ BL_FLOAT delta = 0.2;
    /*const*/ BL_FLOAT zetaF = 50.0; // in Hz
    /*const*/ BL_FLOAT zetaA = 15; // in dB

#if !RESCALE_HZ
    zetaF *= 1.0/(mSampleRate*0.5);
#endif
    
    // These can't be <= 0
    if (zetaF < 1e-1)
        zetaF = 1e-1;
    if (zetaA < 1e-1)
        zetaA = 1e-1;
 
    //Convert to log from dB
    zetaA = zetaA/20*log(10);

    
    // Init cost matrix (MxN)
    vector<vector<BL_FLOAT> > costMatrix;
    costMatrix.resize(prevPartials.size());
    for (int i = 0; i < costMatrix.size(); i++)
        costMatrix[i].resize(currentPartials->size());
    
    // Fill the cost matrix
    for (int i = 0; i < costMatrix.size(); i++)
    {
        for (int j = 0; j < costMatrix[i].size(); j++)
        {
            BL_FLOAT A;
            BL_FLOAT B;
            ComputeCostNeri(prevPartials[i], (*currentPartials)[j],
                            delta, zetaF, zetaA,
                            &A, &B);

            costMatrix[i][j] = (A < B) ? A : B;
        }
    }

    // Solve
    HungarianAlgorithm HungAlgo;
	vector<int> assignment;
	BL_FLOAT cost = HungAlgo.Solve(costMatrix, assignment);
    
    for (int i = 0; i < assignment.size(); i++)
    {
        int a = assignment[i];
       
        // If num prev > num current, there will be some unassigned partials
        // (int this case, assignment is -1)
        if ((a != -1) && 
            (prevPartials[i].mId != -1))
            (*currentPartials)[a].mId = prevPartials[i].mId;
    }

    vector<Partial2> newPartials;
    
    // Add the remaining partials
    remainingCurrentPartials->clear();
    for (int i = 0; i < currentPartials->size(); i++)
    {
        Partial2 &p = (*currentPartials)[i];
        if (p.mId != -1)
        {
            p.mState = Partial2::ALIVE;
            p.mWasAlive = true;
    
            // Increment age
            p.mAge = p.mAge + 1;
            
            newPartials.push_back(p);
        }
        else          
            remainingCurrentPartials->push_back(p);
    }

    *currentPartials = newPartials;
}

void
PartialFilterAMFM2::ComputeZombieDeadPartials(const vector<Partial2> &prevPartials,
                                             const vector<Partial2> &currentPartials,
                                             vector<Partial2> *zombieDeadPartials)
{
    zombieDeadPartials->clear();
    
    // Add the new zombie and dead partials
    for (int i = 0; i < prevPartials.size(); i++)
    {
        const Partial2 &prevPartial = prevPartials[i];

        bool found = false;
        for (int j = 0; j < currentPartials.size(); j++)
        {
            const Partial2 &currentPartial = currentPartials[j];
            
            if (currentPartial.mId == prevPartial.mId)
            {
                found = true;
                
                break;
            }
        }

        if (!found)
        {            
            if (prevPartial.mState == Partial2::ALIVE)
            {
                // We set zombie for 1 frame only
                Partial2 newPartial = prevPartial;
                newPartial.mState = Partial2::ZOMBIE;
                newPartial.mZombieAge = 0;

#if 1 // Good, extrapolate zombies
                
#if EXTRAPOLATE_KALMAN
                ExtrapolatePartialKalman(&newPartial);
#endif

#if EXTRAPOLATE_AMFM
                ExtrapolatePartialAMFM(&newPartial);
#endif

#endif
                if (newPartial.mZombieAge < MAX_ZOMBIE_AGE)
                    zombieDeadPartials->push_back(newPartial);
            }
            else if (prevPartial.mState == Partial2::ZOMBIE)
            {
                Partial2 newPartial = prevPartial;
                newPartial.mZombieAge++;
                
                if (newPartial.mZombieAge >= MAX_ZOMBIE_AGE)
                {
                    newPartial.mState = Partial2::DEAD;
                }
                else
                {
#if 1
                
#if EXTRAPOLATE_KALMAN
                    ExtrapolatePartialKalman(&newPartial);
#endif

#if EXTRAPOLATE_AMFM
                    ExtrapolatePartialAMFM(&newPartial);
#endif
                
#endif
                }
                
                zombieDeadPartials->push_back(newPartial);
            }
            
            // If DEAD, do not add, forget it
        }
    }
}

// Simple fix for partial crossing error
void
PartialFilterAMFM2::FixPartialsCrossing(const vector<Partial2> &partials0,
                                       const vector<Partial2> &partials1,
                                       vector<Partial2> *partials2)
{
#if HARD_OPTIM
    return;
#endif
    
    // Tmp optimization
#define MIN_PARTIAL_AGE 5

#if RESCALE_HZ
#define MAX_SWAP_FREQ 100.0
#else
#define MAX_SWAP_FREQ 100.0/(mSampleRate*0.5)
#endif
    
    const vector<Partial2> partials2Copy = *partials2;
        
    Partial2 p0[3];
    Partial2 p1[3];
    
    for (int i = 0; i < partials2Copy.size(); i++)
    {
        p0[2] = partials2Copy[i];
        if (p0[2].mId == -1)
            continue;

        // Tmp optim
        if (p0[2].mAge < MIN_PARTIAL_AGE)
            continue;

#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
        int idx01 = FindPartialById(partials1, p0[2].mId);
#else
        int idx01 = FindPartialByIdSorted(partials1, p0[2]);
#endif
        
        if (idx01 == -1)
            continue;
        p0[1] = partials1[idx01];

#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
        int idx00 = FindPartialById(partials0, p0[2].mId);
#else
        int idx00 = FindPartialByIdSorted(partials0, p0[2]);
#endif
        
        if (idx00 == -1)
            continue;
        p0[0] = partials0[idx00];

        //
        for (int j = i + 1; j < partials2Copy.size(); j++)
        {
            p1[2] = partials2Copy[j];
            if (p1[2].mId == -1)
                continue;

            // Try to avoid very messy results
            //if (std::fabs(p1[2].mFreq - p0[2].mFreq) > MAX_SWAP_FREQ)
            //    continue;
            if ((p1[2].mFreq - p0[2].mFreq > MAX_SWAP_FREQ) ||
                (p0[2].mFreq - p1[2].mFreq > MAX_SWAP_FREQ))
                continue;
 
#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
            int idx11 = FindPartialById(partials1, p1[2].mId);
#else
            int idx11 = FindPartialByIdSorted(partials1, p1[2]);
#endif
            
            if (idx11 == -1)
                continue;
            p1[1] = partials1[idx11];

#if !OPTIM_SAMPLES_SYNTH_SORTED_VEC
            int idx10 = FindPartialById(partials0, p1[2].mId);
#else
            int idx10 = FindPartialByIdSorted(partials0, p1[2]);
#endif
            
            if (idx10 == -1)
                continue;
            p1[0] = partials0[idx10];

            // Extrapolated values
            BL_FLOAT extraP0 = p0[1].mFreq + (p0[1].mFreq - p0[0].mFreq);
            BL_FLOAT extraP1 = p1[1].mFreq + (p1[1].mFreq - p1[0].mFreq);

            // Check if extrapolated points intersect
            BL_FLOAT extraSeg0[2][2] = { { p0[1].mFreq, 0.0 }, { extraP0, 1.0 } };
            BL_FLOAT extraSeg1[2][2] = { { p1[1].mFreq, 0.0 }, { extraP1, 1.0 } };
            bool extraIntersect = BLUtilsMath::SegSegIntersect2(extraSeg0, extraSeg1);

            // Check if real points intersect
            BL_FLOAT seg0[2][2] = { { p0[1].mFreq, 0.0 }, { p0[2].mFreq, 1.0 } };
            BL_FLOAT seg1[2][2] = { { p1[1].mFreq, 0.0 }, { p1[2].mFreq, 1.0 } };
            bool intersect = BLUtilsMath::SegSegIntersect2(seg0, seg1);
            
            if (intersect != extraIntersect)
            {   
                int tmpId = p0[2].mId;
                p0[2].mId = p1[2].mId;
                p1[2].mId = tmpId;
                
                (*partials2)[i].mId = p0[2].mId;
                (*partials2)[j].mId = p1[2].mId;

                // ??
                break;
            }
        }
    }
}

int
PartialFilterAMFM2::FindPartialById(const vector<Partial2> &partials, int idx)
{
    long numPartials = partials.size();
    for (int i = 0; i < numPartials; i++)
    {
        const Partial2 &partial = partials[i];
        
        if (partial.mId == idx)
            return i;
    }
    
    return -1;
}

int
PartialFilterAMFM2::FindPartialByIdSorted(const vector<Partial2> &partials,
                                         const Partial2 &refPartial)
{
    vector<Partial2> &partials0 = (vector<Partial2> &)partials;
    
    // Find the corresponding prev partial
    vector<Partial2>::iterator it =
        lower_bound(partials0.begin(), partials0.end(), refPartial, Partial2::IdLess);
    
    if (it != partials0.end() && (*it).mId == refPartial.mId)
    {
        // We found the element!
        return (it - partials0.begin());
    }

    // Not found
    return -1;
}

// Compute amplitude likelihood
// (increase when the penality decrease)
BL_FLOAT
PartialFilterAMFM2::ComputeLA(const Partial2 &prevPartial,
                             const Partial2 &currentPartial)
{
#if !OPTIM_TRAPEZOID_AREA
    // Use general polygon
    BL_FLOAT x[4] = { 0.0, 1.0, 1.0, 0.0 };
    BL_FLOAT y[4] = { prevPartial.mAmp,
                      prevPartial.mAmp + prevPartial.mAlpha0,
                      currentPartial.mAmp,
                      currentPartial.mAmp - currentPartial.mAlpha0 };
                        
    BL_FLOAT area = BLUtilsMath::PolygonArea(x, y, 4);
#else
    BL_FLOAT a =
        fabs(prevPartial.mAmp - (currentPartial.mAmp - currentPartial.mAlpha0));
    BL_FLOAT b =
        fabs(currentPartial.mAmp - (prevPartial.mAmp + prevPartial.mAlpha0));
    BL_FLOAT h = 1.0;
    BL_FLOAT area = BLUtilsMath::TrapezoidArea(a, b, h);
#endif
    
    // u
    BL_FLOAT denom = sqrt(currentPartial.mAmp*prevPartial.mAmp);
    BL_FLOAT ua = 0.0;
    if (denom > BL_EPS)
        ua = area/denom;
    
    // Likelihood
    BL_FLOAT LA = 1.0/(1.0 + ua);
    
    return LA;
}

// Compute frequency likelihood
// (increase when the penality decrease)
BL_FLOAT
PartialFilterAMFM2::ComputeLF(const Partial2 &prevPartial,
                             const Partial2 &currentPartial)
{
#if !OPTIM_TRAPEZOID_AREA
    // General polygon
    BL_FLOAT x[4] = { 0.0, 1.0, 1.0, 0.0 };
    BL_FLOAT y[4] = { prevPartial.mFreq,
                      prevPartial.mFreq + prevPartial.mBeta0,
                      currentPartial.mFreq,
                      currentPartial.mFreq - currentPartial.mBeta0 };
    
    BL_FLOAT area = BLUtilsMath::PolygonArea(x, y, 4);
#else
    BL_FLOAT a =
        fabs(prevPartial.mFreq - (currentPartial.mFreq - currentPartial.mBeta0));
    BL_FLOAT b =
        fabs(currentPartial.mFreq - (prevPartial.mFreq + prevPartial.mBeta0));
    BL_FLOAT h = 1.0;
    BL_FLOAT area = BLUtilsMath::TrapezoidArea(a, b, h);
#endif
    
    // u
    BL_FLOAT denom = sqrt(currentPartial.mFreq*prevPartial.mFreq);
    BL_FLOAT uf = 0.0;
    if (denom > BL_EPS)
        uf = area/denom;
    
    // Likelihood
    BL_FLOAT LF = 1.0/(1.0 + uf);
        
    return LF;
}

// See: https://github.com/jundsp/Fast-Partial-Tracking
void
PartialFilterAMFM2::ComputeCostNeri(const Partial2 &prevPartial,
                                    const Partial2 &currentPartial,
                                    BL_FLOAT delta, BL_FLOAT zetaF, BL_FLOAT zetaA,
                                    BL_FLOAT *A, BL_FLOAT *B)
{
    // Debug/Hack: set a very big value for zetaA...
    
    //zetaA = 5.0; // TEST
    //zetaA = 10.0; // TEST
    zetaA = 50.0; // TEST => this way detections begin to be good
    
    /*fprintf(stderr, "freq: %g zetaF: %g\n", currentPartial.mFreq, zetaF);
      fprintf(stderr, "amp: %g zetaA: %g\n", currentPartial.mAmp, zetaA);
      fprintf(stderr, "\n");
    */
    
    BL_FLOAT deltaF =
        (currentPartial.mFreq - currentPartial.mBeta0*0.5) -
        (prevPartial.mFreq + prevPartial.mBeta0*0.5);
    BL_FLOAT deltaA = (currentPartial.mAmp - currentPartial.mAlpha0*0.5) -
        (prevPartial.mAmp + prevPartial.mAlpha0*0.5);
    
#if 0
    // Like in paper (there is a mistake here! log of negative values => NaN 
    BL_FLOAT denom = (2.0*log(delta - 2.0) - 2.0*log(delta - 1.0));    
    BL_FLOAT sigmaF2 = zetaF*zetaF/denom;
    BL_FLOAT sigmaA2 = zetaA*zetaA/denom;
#endif

#if 1
    // Like in github
    BL_FLOAT coeff = log((delta - 1.0)/(delta - 2.0));
    BL_FLOAT sigmaF2 = -zetaF*zetaF*coeff;
    BL_FLOAT sigmaA2 = -zetaA*zetaA*coeff;
#endif

#if 0 //1
    // Like in paper
    *A = 1.0 - exp(-deltaF*deltaF/(2.0*sigmaF2) - deltaA*deltaA/(2.0*sigmaA2));
#endif

#if 1 //0
    // Like in github
    *A = 1.0 - exp(-deltaF*deltaF/sigmaF2 - deltaA*deltaA/sigmaA2);
#endif
    
    *B = 1.0 - (1.0 - delta)*(*A);
}   

// Extrapolate the partial with alpha0 and beta0
void
PartialFilterAMFM2::ExtrapolatePartialAMFM(Partial2 *p)
{
    // Amp is normalized
    p->mAmp += p->mAlpha0;
    if (p->mAmp < 0.0)
        p->mAmp = 0.0;
    if (p->mAmp > 1.0)
        p->mAmp = 1.0;

    // Freq is real freq
    p->mFreq += p->mBeta0;
    
#if RESCALE_HZ
    if (p->mFreq < 0.0)
        p->mFreq = 0.0;
    if (p->mFreq > mSampleRate*0.5)
        p->mFreq = mSampleRate*0.5;
#else
    if (p->mFreq < 0.0)
        p->mFreq = 0.0;
    if (p->mFreq > 1.0)
        p->mFreq = 1.0;
#endif
}

void
PartialFilterAMFM2::ExtrapolatePartialKalman(Partial2 *p)
{
#if RESCALE_HZ
    p->mFreq /= mSampleRate*0.5;
#endif
    
    p->mFreq = p->mKf.updateEstimate(p->mFreq);

#if RESCALE_HZ
    p->mFreq *= mSampleRate*0.5;
#endif
}

bool
PartialFilterAMFM2::CheckDiscardBigJump(const Partial2 &prevPartial,
                                       const Partial2 &currentPartial)
{
#define BIG_JUMP_COEFF 16.0 //4.0 //16.0 //4.0 //2.0

    BL_FLOAT oneBinEps = 1.0/mBufferSize;
#if RESCALE_HZ
    oneBinEps *= mSampleRate*0.5;
#endif

    // Check if partials are very close
    // (in this case, it sould keep the same id, even if beta0 is very small)
    if (std::fabs(prevPartial.mFreq - currentPartial.mFreq) <
        oneBinEps*BIG_JUMP_COEFF)
        return false;
        
    // Extrapoled frequency, from prev partial
    BL_FLOAT extraFreq0 = prevPartial.mFreq + prevPartial.mBeta0;
    bool flag0 = (currentPartial.mFreq > extraFreq0 +
                  BIG_JUMP_COEFF*(extraFreq0 - prevPartial.mFreq));


    bool flag1 = (currentPartial.mFreq < extraFreq0 -
                  BIG_JUMP_COEFF*(extraFreq0 - prevPartial.mFreq));

    // Extrapolated, from current partial
    BL_FLOAT extraFreq1 = currentPartial.mFreq - currentPartial.mBeta0;
    bool flag2 = (prevPartial.mFreq > extraFreq1 +
                  BIG_JUMP_COEFF*(extraFreq1 - currentPartial.mFreq));

    bool flag3 = (prevPartial.mFreq < extraFreq1 -
                  BIG_JUMP_COEFF*(extraFreq1 - currentPartial.mFreq));

    // Use flags "&&" to mix cases
    // e.g to give a chance to a case where prev beta0 is almost 0,
    // but current beta0 has a significant value.
    if (flag0 && flag3)
        return true;
    
    if (flag1 && flag2)
        return true;
    
    return false;
}

bool
PartialFilterAMFM2::CheckDiscardOppositeDirection(const Partial2 &prevPartial,
                                                 const Partial2 &currentPartial)
{
    if ((prevPartial.mFreq < currentPartial.mFreq) &&
        (prevPartial.mBeta0 < 0.0) && (currentPartial.mBeta0 > 0.0))
        return true;
    if ((prevPartial.mFreq > currentPartial.mFreq) &&
        (prevPartial.mBeta0 > 0.0) && (currentPartial.mBeta0 < 0.0))
        return true;

    if ((prevPartial.mAmp < currentPartial.mAmp) &&
        (prevPartial.mAlpha0 < 0.0) && (currentPartial.mAlpha0 > 0.0))
        return true;
    if ((prevPartial.mAmp > currentPartial.mAmp) &&
        (prevPartial.mAlpha0 > 0.0) && (currentPartial.mAlpha0 < 0.0))
        return true;
    
    return false;
}
