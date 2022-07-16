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
//  PartialsToFreq8.h
//  BL-SASViewer
//
//  Created by applematuer on 2/25/19.
//
//

#ifndef __BL_SASViewer__PartialsToFreq8__
#define __BL_SASViewer__PartialsToFreq8__

#include <vector>
using namespace std;

#include <Partial2.h>

// PartialsToFreq: original code, moved from SASFrame to here
// PartialsToFreq2: improved algorithm
//
// PartialsToFreq3: custom algo, use freq diffs and sigma
//
// PartialsToFreq4: keep only the structure and moved the freq diff algo
// to a new class
// (will be useful if need filtering frequency)
//
// PartialsToFreq6: compute freq from chroma, and adjust to the right octave,
// depending on the partials freqs
//
// VERY GOOD! (this is a real improvement over the standardly used algorithm!)
// => the freq from chroma is really more smooth than the tracked partial data
// => and it represents the pitch rellay more accurately than partial tracking
class ChromagramObj;
class PartialsToFreq8
{
public:
    PartialsToFreq8(int bufferSize, int oversampling,
                    int freqRes, BL_FLOAT sampleRate);
    
    virtual ~PartialsToFreq8();

    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate);
    
    void SetHarmonicSoundFlag(bool flag);
    
    BL_FLOAT ComputeFrequency(const WDL_TypedBuf<BL_FLOAT> &magns,
                              const WDL_TypedBuf<BL_FLOAT> &phases,
                              const vector<Partial2> &partials);
    
protected:
    BL_FLOAT AdjustFreqToPartial(BL_FLOAT freq,
                                 const vector<Partial2> &partials);

    BL_FLOAT
    AdjustFreqToPartialOctave(BL_FLOAT freq,
                              const vector<Partial2> &partials);
    
    void ThresholdPartials(vector<Partial2> *partials);
    void ThresholdPartialsRelative(vector<Partial2> *partials);

    // From computed chroma freq, choose the best partial freq
    BL_FLOAT FindClosestPartialFreq(BL_FLOAT inFreq,
                                    const vector<Partial2> &partials);

    // Keep the chroma freq, but adjust to the right octave,
    // depending on the partials freqs
    //
    // VERY GOOD!
    // => the freq from chroma is really more smooth than the tracked partial data
    // => and it represents the pitch rellay more accurately than partial tracking
    BL_FLOAT FindBestOctave(BL_FLOAT inFreq,
                            const vector<Partial2> &partials);

    // Keep the chroma freq, but adjust to the right octave,
    // depending on roughtly detected partials (on the fly)
    //
    // VERY GOOD!
    // => the freq from chroma is really more smooth than the tracked partial data
    // => and it represents the pitch rellay more accurately than partial tracking
    BL_FLOAT FindBestOctave2(BL_FLOAT inFreq,
                             const WDL_TypedBuf<BL_FLOAT> &magns);
    
    //
    int mBufferSize;
    BL_FLOAT mSampleRate;

    ChromagramObj *mChromaObj;

private:
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0;
};

#endif /* defined(__BL_SASViewer__PartialsToFreq8__) */
