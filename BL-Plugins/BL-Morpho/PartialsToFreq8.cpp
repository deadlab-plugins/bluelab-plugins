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
 
//
//  PartialsToFreq8.cpp
//  BL-SASViewer
//
//  Created by applematuer on 2/25/19.
//
//

#include <algorithm>
using namespace std;

#include <BLUtils.h>
#include <BLUtilsMath.h>

#include <ChromagramObj.h>

#include <BLDebug.h>

#include "PartialsToFreq8.h"


#define MIN_AMP_DB -120.0


// Threshold
#define THRESHOLD_PARTIALS 0
// -40 : good for "oohoo"
// -60 : good for "bell"
#define AMP_THRESHOLD -60.0 //-80.0

// Threshold relative
#define THRESHOLD_PARTIALS_RELATIVE 0
// -40 : good for "oohoo"
// -60 : good for "bell"
#define AMP_THRESHOLD_RELATIVE 40.0

//
#define MAX_NUM_INTERVALS 1
#define NUM_OCTAVES_ADJUST 2
#define NUM_HARMO_GEN 4
#define NUM_OCTAVE_GEN 2

#define MIN_FREQ 20.0
#define PREV_PARTIAL_COEFF 0.75 

PartialsToFreq8::PartialsToFreq8(int bufferSize, int oversampling,
                                 int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;

    mChromaObj = new ChromagramObj(bufferSize, oversampling,
                                   freqRes, sampleRate);
    // Max smoothness
    mChromaObj->SetSharpness(1.0);
    // DEBUG: for the example "oohoo", tune in order to not have a cut due to modulo
    mChromaObj->SetATune(440.0);
    //mChromaObj->SetATune(698.46); // F (half one actave higher, to avoid cut)
}

PartialsToFreq8::~PartialsToFreq8()
{
    delete mChromaObj;
}

void
PartialsToFreq8::Reset(int bufferSize, int oversampling,
                       int freqRes, BL_FLOAT sampleRate)
{
    mChromaObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
}

void
PartialsToFreq8::SetHarmonicSoundFlag(bool flag) {}

BL_FLOAT
PartialsToFreq8::ComputeFrequency(const WDL_TypedBuf<BL_FLOAT> &magns,
                                  const WDL_TypedBuf<BL_FLOAT> &phases,
                                  const vector<Partial2> &partials)
{    
    vector<Partial2> partials0 = partials;

    WDL_TypedBuf<BL_FLOAT> &chromaLine = mTmpBuf0;
    mChromaObj->MagnsToChromaLine(magns, phases, &chromaLine);

    if (chromaLine.GetSize() == 0)
        return 0.0;
    
    //BL_FLOAT maxChroma = BLUtils::FindMaxValue(chromaLine);
    int maxChromaIdx = BLUtils::FindMaxIndex(chromaLine);
    BL_FLOAT maxChroma = ((BL_FLOAT)maxChromaIdx)/chromaLine.GetSize();
    
    BL_FLOAT freq0 = mChromaObj->ChromaToFreq(maxChroma, MIN_FREQ);
    
    // Find closest partial (not working well)
#if 0 //1
    BL_FLOAT freq = FindClosestPartialFreq(freq0, partials);
#endif
            
#if 0 //1 // Good, but if tracker loose tracking on lowest partial, this gives error
    BL_FLOAT freq = FindBestOctave(freq0, partials);
#endif

#if 1 // Good. Do not use prev tracked partials
    // This is a bit hackish, but looks efficient
    BL_FLOAT freq = FindBestOctave2(freq0, magns);
#endif

#if 0 //1 // TEST: Good!
    // Hedger trimmer: choose 100 (but this is still not perfect at all) 
    /*BL_FLOAT*/ freq = freq0;
    while(freq < 100.0/*200.0*//*100.0*//*300.0*/)
        freq *= 2.0;
#endif
    
    return freq;
}

// Take the principle that at least 1 partial has the correct frequency
// Adjust the found freq to the nearest partial
BL_FLOAT
PartialsToFreq8::AdjustFreqToPartial(BL_FLOAT freq,
                                     const vector<Partial2> &partials)
{
    BL_FLOAT bestFreq = 0.0;
    BL_FLOAT minDiff = BL_INF;
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &partial = partials[i];
        
        BL_FLOAT diff = std::fabs(partial.mFreq - freq);
        if (diff < minDiff)
        {
            minDiff = diff;
            bestFreq = partial.mFreq;
        }
    }
    
    return bestFreq;
}

BL_FLOAT
PartialsToFreq8::
AdjustFreqToPartialOctave(BL_FLOAT freq,
                          const vector<Partial2> &partials)
{    
    BL_FLOAT octaveCoeff = 1.0;
    
    BL_FLOAT bestFreq = 0.0;
    BL_FLOAT minDiff = BL_INF;
    for (int j = 0; j < NUM_OCTAVES_ADJUST; j++)
    {
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];
        
            BL_FLOAT pf = partial.mFreq*octaveCoeff;
            
            BL_FLOAT diff = std::fabs(pf - freq);
            if (diff < minDiff)
            {
                minDiff = diff;
                bestFreq = pf;
            }
        }
        
        octaveCoeff /= 2.0;
    }
    
    return bestFreq;
}

void
PartialsToFreq8::ThresholdPartials(vector<Partial2> *partials)
{
#if 0
    vector<Partial2> result;
    
    for (int i = 0; i < partials->size(); i++)
    {
        const Partial2 &partial = (*partials)[i];
        
        if (partial.mAmpDB > AMP_THRESHOLD)
            result.push_back(partial);
    }
    
    *partials = result;
#endif
}

void
PartialsToFreq8::ThresholdPartialsRelative(vector<Partial2> *partials)
{
#if 0
    vector<Partial2> result;
    
    // Find the maximum amp
    BL_FLOAT maxAmp = MIN_AMP_DB;
    for (int i = 0; i < partials->size(); i++)
    {
        const Partial2 &partial = (*partials)[i];
        
        if (partial.mAmpDB > maxAmp)
            maxAmp = partial.mAmpDB;
            
    }
    
    // Threshold compared to the maximum peak
    BL_FLOAT ampThrs = maxAmp - AMP_THRESHOLD_RELATIVE;
    for (int i = 0; i < partials->size(); i++)
    {
        const Partial2 &partial = (*partials)[i];
        
        if (partial.mAmpDB > ampThrs)
            result.push_back(partial);
    }
    
    *partials = result;
#endif
}

// Uunsed?
BL_FLOAT
PartialsToFreq8::
FindClosestPartialFreq(BL_FLOAT refFreq0,
                       const vector<Partial2> &partials)
{
    if (partials.empty())
        return 0.0;

#if 0 //1
    // Get min partial freq
    BL_FLOAT minFreq = BL_INF;
    for (int i = 0; i < partials.size(); i++)
    {
        if (partials[i].mFreq < minFreq)
            minFreq = partials[i].mFreq;
    }

    return minFreq;
#endif
    
    // Get max partial freq
    BL_FLOAT maxFreq = 0.0;
    for (int i = 0; i < partials.size(); i++)
    {
        if (partials[i].mFreq > maxFreq)
            maxFreq = partials[i].mFreq;
    }

    // Go one octave uppper, just to be sure to not miss value
    maxFreq *= 2.0;
    
    // Find closest partial freq
    int bestPartialIdx = 0;
    BL_FLOAT bestDiff = BL_INF;
    for (int i = 0; i < partials.size(); i++)
    {
        BL_FLOAT pf = partials[i].mFreq;
       
        BL_FLOAT f = refFreq0;
        int octave = 1;
        while(f < maxFreq)
        {
            BL_FLOAT diff = std::fabs(f - pf)/octave;
            //BL_FLOAT diff = std::fabs(f - pf);

            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestPartialIdx = i;
            }
            
            f *= 2.0;
            octave++;
        }
    }

    BL_FLOAT result = partials[bestPartialIdx].mFreq;

    return result;
}

// Take inFreq, and rise it to the best upper octave
// Take previously tracked partials as reference (may be risky)
BL_FLOAT
PartialsToFreq8::FindBestOctave(BL_FLOAT inFreq,
                                const vector<Partial2> &partials)
{
    vector<Partial2> partials0 = partials;
    sort(partials0.begin(), partials0.end(), Partial2::FreqLess);

    // Compute min partial freq
    BL_FLOAT minFreq = inFreq;
    int minIdx = -1;
    for (int i = 0; i < partials0.size(); i++)
    {
        const Partial2 &p = partials0[i];
        if (p.mFreq >= MIN_FREQ)
        {
            minFreq = p.mFreq;
            minIdx = i;

            break;
        }
    }
    
    BL_FLOAT freq = inFreq;
    while(freq < minFreq*PREV_PARTIAL_COEFF)
        freq *= 2.0;

    return freq;
}

// Take inFreq, and rise it to the best upper octave
// Detect on the fly the first big partial (hackish, but more secure)
BL_FLOAT
PartialsToFreq8::FindBestOctave2(BL_FLOAT inFreq,
                                 const WDL_TypedBuf<BL_FLOAT> &magns)
{
#if 0 // Very naive method, does not work well
    // Compute max amplitude
    // And consider this is the peak of the main partial
    int maxFreqIdx = BLUtils::FindMaxIndex(magns);
    BL_FLOAT hzPerBin =  mSampleRate/mBufferSize;
    BL_FLOAT minFreq = maxFreqIdx*hzPerBin;
#endif

#if 1
    BL_FLOAT minFreq = -1.0;

    // Find the first partial (kind of left border)
    BL_FLOAT maxMagn = BLUtils::FindMaxValue(magns);
    maxMagn = BLUtils::AmpToDB(maxMagn);
    for (int i = 0; i < magns.GetSize(); i++)
    {
        BL_FLOAT m = magns.Get()[i];
        // Threshold: 2 times smaller (in dB, this is reversed)
        if (BLUtils::AmpToDB(m) > maxMagn*2.0)
            // We have found the beginning of the first peak
        {
            // Now, find the center of the peak
            for (int j = i; j < magns.GetSize(); j++)
            {
                if (j < magns.GetSize() - 1)
                {
                    if (magns.Get()[j + 1] < magns.Get()[j])
                        // We start descending, we should be at the top of the peak
                    {
                        BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
                        minFreq = j*hzPerBin;
                        
                        break;
                    }
                }   
            }
        }

        if (minFreq > 0.0)
            // Peak found
            break;
    }

    if (minFreq < 0.0)
        return inFreq;
#endif
    
    BL_FLOAT freq = inFreq;
    while(freq < minFreq*PREV_PARTIAL_COEFF)
        freq *= 2.0;

    return freq;
}
