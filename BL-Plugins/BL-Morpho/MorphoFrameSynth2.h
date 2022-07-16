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
 
#ifndef MORPHO_FRAME_SYNTH2_H
#define MORPHO_FRAME_SYNTH2_H

#include <BLTypes.h>

#include <MorphoFrame7.h>
#include <Partial2.h>

#include "IPlug_include_in_plug_hdr.h"

#define SYNTH_MAX_NUM_PARTIALS 40 //20 //10 origin: 10, (40 for test bell)

class MorphoFrameSynth2
{
 public:
    enum SynthMode
    {
        // Use raw detected partials and synth using sine and samples
        RAW_PARTIALS = 0,
        // Source partials, denormalized (we re-apply sas params on them later 
        SOURCE_PARTIALS,
        // Resynth using sines and samples
        RESYNTH_PARTIALS
    };

    MorphoFrameSynth2(int bufferSize, int oversampling,
                  int freqRes, BL_FLOAT sampleRate);
    virtual ~MorphoFrameSynth2();

    void Reset(BL_FLOAT sampleRate);
    
    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate);

    void SetMinAmpDB(BL_FLOAT ampDB);

    //
    void SetSynthMode(enum SynthMode mode);
    MorphoFrameSynth2::SynthMode GetSynthMode() const;

    void SetSynthEvenPartials(bool flag);
    void SetSynthOddPartials(bool flag);
    
    //
    void AddMorphoFrame(const MorphoFrame7 &frame);
    // Get the current Morpho frame
    void GetMorphoFrame(MorphoFrame7 *frame);
    
    void ComputeSamples(WDL_TypedBuf<BL_FLOAT> *samples);
    
protected:
    // Keep it for debugging
    void ComputeSamplesPartialsRaw(WDL_TypedBuf<BL_FLOAT> *samples);
     // TODO: delete this
    void ComputeSamplesPartialsSourceNorm(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesPartialsSource(WDL_TypedBuf<BL_FLOAT> *samples);
    void ComputeSamplesPartialsResynth(WDL_TypedBuf<BL_FLOAT> *samples);

    BL_FLOAT GetColor(const WDL_TypedBuf<BL_FLOAT> &color, BL_FLOAT binIdx);
    BL_FLOAT GetWarping(const WDL_TypedBuf<BL_FLOAT> &warping, BL_FLOAT binIdx);

    bool FindPartial(BL_FLOAT freq);
    void GetPartial(MorphoFrame7::MorphoPartial *result, int index, BL_FLOAT t);
    
    //
    BL_FLOAT GetFreq(BL_FLOAT freq0, BL_FLOAT freq1, BL_FLOAT t);
    BL_FLOAT GetAmp(BL_FLOAT amp0, BL_FLOAT amp1, BL_FLOAT t);
    BL_FLOAT GetCol(BL_FLOAT col0, BL_FLOAT col1, BL_FLOAT t);

    void UpdateMorphoData();
        
    //
    vector<MorphoFrame7::MorphoPartial> mMorphoPartials;
    vector<MorphoFrame7::MorphoPartial> mPrevMorphoPartials;

    // For "raw" and "source"
    vector<MorphoFrame7::MorphoPartial> mPartials;
    vector<MorphoFrame7::MorphoPartial> mPrevPartials;
    
    //
    int mBufferSize;
    BL_FLOAT mSampleRate;
    int mOverlapping;
    int mFreqRes;

    //
    BL_FLOAT mMinAmpDB;
    
    //
    SynthMode mSynthMode;
    bool mSynthEvenPartials;
    bool mSynthOddPartials;
    
    //
    BL_FLOAT mAmplitude;
    BL_FLOAT mPrevAmplitude;
    
    BL_FLOAT mFrequency;
    // For smoothing
    BL_FLOAT mPrevFrequency;

    WDL_TypedBuf<BL_FLOAT> mColor;
    WDL_TypedBuf<BL_FLOAT> mPrevColor;
    
    WDL_TypedBuf<BL_FLOAT> mWarping;
    WDL_TypedBuf<BL_FLOAT> mPrevWarping;
    
    WDL_TypedBuf<BL_FLOAT> mWarpingInv;

    //
    MorphoFrame7 mMorphoFrame;
    MorphoFrame7 mPrevMorphoFrame;

private:
    vector<Partial2> mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
};

#endif
