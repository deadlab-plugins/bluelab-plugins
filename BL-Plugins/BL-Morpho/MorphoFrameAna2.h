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
 
#ifndef FRAME_ANA2_H
#define FRAME_ANA2_H

#include <bl_queue.h>

// SASFrameAna2: from SASFrameAna, for Morpho
class MorphoFrame7;
class PartialsToFreq8;
class OnsetDetector;
class MorphoFrameAna2
{
 public:
    MorphoFrameAna2(int bufferSize, int oversampling,
                 int freqRes, BL_FLOAT sampleRate);
    virtual ~MorphoFrameAna2();

    void Reset(BL_FLOAT sampleRate);
    
    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate);
    
    void Reset();

    //
    void SetTimeSmoothNoiseCoeff(BL_FLOAT coeff);
    
    // Keep input magns (will be used to compute frequency)
    void SetInputData(const WDL_TypedBuf<BL_FLOAT> &magns,
                      const WDL_TypedBuf<BL_FLOAT> &phases);

    // Processed data (will be used to compute noise envelope)
    void SetProcessedData(const WDL_TypedBuf<BL_FLOAT> &magns,
                          const WDL_TypedBuf<BL_FLOAT> &phases);
    
    // Non filtered partials
    void SetRawPartials(const vector<Partial2> &partials);
    
    // De-normalized partials
    void SetPartials(const vector<Partial2> &partials);

    void Compute(MorphoFrame7 *frame);
    
protected:
    // Compute steps
    //
    void ComputeNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                              WDL_TypedBuf<BL_FLOAT> *harmoEnv);
        
    BL_FLOAT ComputeAmplitude();
    BL_FLOAT ComputeFrequency(const WDL_TypedBuf<BL_FLOAT> &magns);
    void ComputeColor(WDL_TypedBuf<BL_FLOAT> *color,
                      WDL_TypedBuf<BL_FLOAT> *colorNorm,
                      BL_FLOAT freq);
    void ComputeColorAux(WDL_TypedBuf<BL_FLOAT> *color, BL_FLOAT freq);
    void ComputeWarping(WDL_TypedBuf<BL_FLOAT> *warping,
                        WDL_TypedBuf<BL_FLOAT> *warpingInv,
                        BL_FLOAT freq);

    // If inverse is true, then compute inverse warping
    void ComputeWarpingAux(WDL_TypedBuf<BL_FLOAT> *warping,
                           BL_FLOAT freq, bool inverse = false);

    bool ComputeOnset();

    void NormalizePartials(const WDL_TypedBuf<BL_FLOAT> &warpingInv,
                           const WDL_TypedBuf<BL_FLOAT> &color);
        
    void ProcessMusicalNoise(WDL_TypedBuf<BL_FLOAT> *noise);
    void SmoothNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noise);
    void TimeSmoothNoise(WDL_TypedBuf<BL_FLOAT> *noise);
    
    //
    
    // Fill everything after the last partial with value
    void FillLastValues(WDL_TypedBuf<BL_FLOAT> *values,
                        const vector<Partial2> &partials, BL_FLOAT val);

    // Fill everything bfore the first partial with value
    void FillFirstValues(WDL_TypedBuf<BL_FLOAT> *values,
                         const vector<Partial2> &partials, BL_FLOAT val);
    
    // Get the partials which are alive
    // (this avoid getting garbage partials that would never be associated)
    bool GetAlivePartials(vector<Partial2> *partials);

    void KeepOnlyPartials(const vector<Partial2> &partials,
                          WDL_TypedBuf<BL_FLOAT> *magns);

    //
    int mBufferSize;
    BL_FLOAT mSampleRate;
    int mOverlapping;
    int mFreqRes;
    
    // Input signal, not processed
    WDL_TypedBuf<BL_FLOAT> mInputMagns;
    WDL_TypedBuf<BL_FLOAT> mInputPhases;

    // Input signal, not processed
    WDL_TypedBuf<BL_FLOAT> mProcessedMagns;
    WDL_TypedBuf<BL_FLOAT> mProcessedPhases;
    
    // HACK
    deque<WDL_TypedBuf<BL_FLOAT> > mInputMagnsHistory;

    // Noise
    //
    WDL_TypedBuf<BL_FLOAT> mSmoothWinNoise;
    
    // For SmoothNoiseEnvelopeTime()
    BL_FLOAT mTimeSmoothNoiseCoeff;
    WDL_TypedBuf<BL_FLOAT> mPrevNoiseEnvelope;
    
    // For ComputeMusicalNoise()
    bl_queue<WDL_TypedBuf<BL_FLOAT> > mPrevNoiseMasks;

    vector<Partial2> mRawPartials;
    
    // Not normalized
    vector<Partial2> mPartials;
    vector<Partial2> mPrevPartials;

    //PartialsToFreq5 *mPartialsToFreq;
    PartialsToFreq8 *mPartialsToFreq;

    OnsetDetector *mOnsetDetector;

    // Used to compute frequency
    BL_FLOAT mPrevFrequency;

    // Must keep the prev values, to interpolate over time
    // when generating the samples
    WDL_TypedBuf<BL_FLOAT> mPrevColor;
    WDL_TypedBuf<BL_FLOAT> mPrevWarping;
    WDL_TypedBuf<BL_FLOAT> mPrevWarpingInv;
    
    //
    struct PartialAux
    {
        BL_FLOAT mFreq;
        BL_FLOAT mWarping;
    };

private:
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    vector<Partial2> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
};

#endif
