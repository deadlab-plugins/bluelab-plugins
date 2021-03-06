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
//  SASFrame5.h
//  BL-SASViewer
//
//  Created by applematuer on 2/2/19.
//
//

#ifndef __BL_SASViewer__SASFrame5__
#define __BL_SASViewer__SASFrame5__

#include <vector>
using namespace std;

#include <Partial.h>

// SASFrame2: from SASFrame
//
// Use PartialToFreq compute everything for
// frequency in a sperate object
//
// Use PartialToFreq2 (improved algorithm)
//
// SASFrame5: when restarted developing, in order to make SASSynth
// Try to improve frequency computation, using same thenique as in Chroma

//class PartialsToFreq5;
class PartialsToFreq7;
class FreqAdjustObj3;
class WavetableSynth;
class OnsetDetector;
class SASFrame5
{
public:
    enum SynthMode
    {
        // Resynth using fft
        FFT = 0,
        // Resynth using sines and samples
        OSC,
        // Use raw detected partials and synth using sine and samples
        RAW_PARTIALS
    };
    
    class SASPartial
    {
    public:
        SASPartial();
        
        SASPartial(const SASPartial &other);
        
        virtual ~SASPartial();
        
        static bool AmpLess(const SASPartial &p1, const SASPartial &p2);

    public:
        // Values are normalized, as provided by SASViewerProcess
        BL_FLOAT mFreq;
        BL_FLOAT mAmp; // Still used ?
        BL_FLOAT mPhase;
    };
    
    SASFrame5(int bufferSize, BL_FLOAT sampleRate,
              int overlapping, int freqRes);
    
    virtual ~SASFrame5();
    
    void Reset(BL_FLOAT sampleRate);
    
    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate);
        
    void SetMinAmpDB(BL_FLOAT ampDB);

    //
    //
    void SetSynthMode(enum SynthMode mode);
    SASFrame5::SynthMode GetSynthMode() const;
    
    void SetSynthEvenPartials(bool flag);
    void SetSynthOddPartials(bool flag);
    
    // De-normalized partials
    void SetPartials(const vector<Partial> &partials);
    
    void SetNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv);
    void GetNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv) const;

    // Keep input magns (will be used to compute frequency)
    void SetInputData(const WDL_TypedBuf<BL_FLOAT> &magns,
                      const WDL_TypedBuf<BL_FLOAT> &phases);
    
    // Get
    BL_FLOAT GetAmplitude() const;
    BL_FLOAT GetFrequency() const;
    void GetColor(WDL_TypedBuf<BL_FLOAT> *color) const;
    void GetNormWarping(WDL_TypedBuf<BL_FLOAT> *warping) const;
    
    // Set
    void SetAmplitude(BL_FLOAT amp);
    void SetFrequency(BL_FLOAT freq);
    void SetColor(const WDL_TypedBuf<BL_FLOAT> &color);
    void SetNormWarping(const WDL_TypedBuf<BL_FLOAT> &warping);
    
    // Compute directly from input partials
    void ComputeSamples(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesPost(WDL_TypedBuf<BL_FLOAT> *samples);
    
    // Compute by resynthesizing from color, warping etc.
    void ComputeSamplesResynth(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesResynthPost(WDL_TypedBuf<BL_FLOAT> *samples);
    
    void ComputeFftPartials(WDL_TypedBuf<BL_FLOAT> *samples);
    
    void SetAmpFactor(BL_FLOAT factor);
    void SetFreqFactor(BL_FLOAT factor);
    void SetColorFactor(BL_FLOAT factor);
    void SetWarpingFactor(BL_FLOAT factor);
    
    bool ComputeSamplesFlag();
    bool ComputeSamplesPostFlag();
    
    static void MixFrames(SASFrame5 *result,
                          const SASFrame5 &frame0,
                          const SASFrame5 &frame1,
                          BL_FLOAT t, bool mixFreq);
    
protected:
    void ComputeSamplesPartialsRAW(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesPartials(WDL_TypedBuf<BL_FLOAT> *samples);
    
    BL_FLOAT GetColor(const WDL_TypedBuf<BL_FLOAT> &color, BL_FLOAT binIdx);
    BL_FLOAT GetWarping(const WDL_TypedBuf<BL_FLOAT> &warping, BL_FLOAT binIdx);

    // Was "ComputeSamplesSAS7"
    void ComputeSamplesSAS(WDL_TypedBuf<BL_FLOAT> *samples);
    
    void ComputeFftSAS(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeFftSASFreqAdjust(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesSASTable(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesSASTable2(WDL_TypedBuf<BL_FLOAT> *samples); // Better interpolation
    void ComputeSamplesSASOverlap(WDL_TypedBuf<BL_FLOAT> *samples);
    
    void Compute();
    
    // Compute steps
    //
    void ComputeAmplitude();
    void ComputeFrequency();
    void ComputeColor();
    void ComputeColorAux();
    void ComputeNormWarping();
    void ComputeNormWarpingAux();
    // If inverse is true, then compute inverse warping
    void ComputeNormWarpingAux2(WDL_TypedBuf<BL_FLOAT> *warping,
                                bool inverse = false);
    
    // Simple version
    BL_FLOAT ApplyNormWarping(BL_FLOAT freq);
    BL_FLOAT ApplyColor(BL_FLOAT freq);

    // Versions to interpolate over time
    BL_FLOAT ApplyNormWarping(BL_FLOAT freq, BL_FLOAT t);
    BL_FLOAT ApplyColor(BL_FLOAT freq, BL_FLOAT t);
    
    bool FindPartial(BL_FLOAT freq);

    void GetPartial(Partial *result, int index, BL_FLOAT t);
    
    int FindPrevPartialIdx(int currentPartialIdx);
    // Optimized
    int FindPrevPartialIdxSorted(int currentPartialIdx);
    // Optimized 2
    int FindPrevPartialIdxSorted2(int currentPartialIdx);
    
    void GetSASPartial(SASPartial *result, int index, BL_FLOAT t);
    
    //
    BL_FLOAT GetFreq(BL_FLOAT freq0, BL_FLOAT freq1, BL_FLOAT t);
    BL_FLOAT GetAmp(BL_FLOAT amp0, BL_FLOAT amp1, BL_FLOAT t);
    BL_FLOAT GetCol(BL_FLOAT col0, BL_FLOAT col1, BL_FLOAT t);

    // Fill everything after the last partial with value
    void FillLastValues(WDL_TypedBuf<BL_FLOAT> *values,
                        const vector<Partial> &partials, BL_FLOAT val);

    // Fill everything bfore the first partial with value
    void FillFirstValues(WDL_TypedBuf<BL_FLOAT> *values,
                         const vector<Partial> &partials, BL_FLOAT val);

    static BL_FLOAT ApplyColorFactor(BL_FLOAT color, BL_FLOAT factor);
    static void ApplyColorFactor(WDL_TypedBuf<BL_FLOAT> *color, BL_FLOAT factor);
    
    void LinkPartialsIdx(vector<Partial> *partials0,
                         vector<Partial> *partials1);
        
    //
    SynthMode mSynthMode;
    bool mSynthEvenPartials;
    bool mSynthOddPartials;
    
    // Tracked partials
    BL_FLOAT mPrevAmplitude;
    
    // Not normalized
    vector<Partial> mPartials;
    vector<Partial> mPrevPartials;
    
    BL_FLOAT mAmplitude;
    
    BL_FLOAT mFrequency;
    // For smoothing
    BL_FLOAT mPrevFrequency;
    
    int mBufferSize;
    BL_FLOAT mSampleRate;
    int mOverlapping;
    int mFreqRes;
    
    WDL_TypedBuf<BL_FLOAT> mNoiseEnvelope;

    // Input signal, not processed
    WDL_TypedBuf<BL_FLOAT> mInputMagns;
    WDL_TypedBuf<BL_FLOAT> mInputPhases;

    // HACK
    deque<WDL_TypedBuf<BL_FLOAT> > mInputMagnsHistory;
    
    WDL_TypedBuf<BL_FLOAT> mColor;
    WDL_TypedBuf<BL_FLOAT> mNormWarping;
    WDL_TypedBuf<BL_FLOAT> mNormWarpingInv;
    
    // Must keep the prev values, to interpolate over time
    // when generating the samples
    WDL_TypedBuf<BL_FLOAT> mPrevColor;
    WDL_TypedBuf<BL_FLOAT> mPrevNormWarping;
    WDL_TypedBuf<BL_FLOAT> mPrevNormWarpingInv;
    
    vector<SASPartial> mSASPartials;
    vector<SASPartial> mPrevSASPartials;
    
    BL_FLOAT mAmpFactor;
    BL_FLOAT mFreqFactor;
    BL_FLOAT mColorFactor;
    BL_FLOAT mWarpingFactor;
    
    //PartialsToFreq5 *mPartialsToFreq;
    PartialsToFreq7 *mPartialsToFreq;
    
    FreqAdjustObj3 *mFreqObj;
    
    // For sample synth with table
    WavetableSynth *mTableSynth;
    
    BL_FLOAT mMinAmpDB;

    Scale *mScale;

    OnsetDetector *mOnsetDetector;

    struct PartialAux
    {
        BL_FLOAT mFreq;
        BL_FLOAT mWarping;
    };
};

#endif /* defined(__BL_SASViewer__SASFrame5__) */
