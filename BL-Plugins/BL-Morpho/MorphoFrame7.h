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
//  MorphoFrame7.h
//  BL-Morpho
//
//  Created by applematuer on 2/2/19.
//
//

#ifndef MORPHO_FRAME_H
#define MORPHO_FRAME_H

#include <vector>
using namespace std;

#include <Partial2.h>

#include "IPlug_include_in_plug_hdr.h"

// SASFrame2: from SASFrame
//
// Use PartialToFreq compute everything for
// frequency in a sperate object
//
// Use PartialToFreq2 (improved algorithm)
//
// SASFrame5: when restarted developing, in order to make SASSynth
// Try to improve frequency computation, using same thenique as in Chroma

// SASFrame6: keep only the data. Processing is now in SASFrameAna and SASFrameSynth
// MorphoFrame7: for Morpho
class MorphoFrame7
{
public:
    class MorphoPartial
    {
    public:
        MorphoPartial();
        
        MorphoPartial(const MorphoPartial &other);
        MorphoPartial(const Partial2 &other);
        
        virtual ~MorphoPartial();
        
        static bool AmpLess(const MorphoPartial &p1, const MorphoPartial &p2);

        static bool IdLess(const MorphoPartial &p1, const MorphoPartial &p2);

    public:
        BL_FLOAT mFreq;
        BL_FLOAT mAmp;
        BL_FLOAT mPhase;

        int mLinkedId;
        enum Partial2::State mState;
        bool mWasAlive;

        int mId;
    };
    
    MorphoFrame7();
    MorphoFrame7(const MorphoFrame7 &other);
    
    virtual ~MorphoFrame7();
    
    void Reset();
    
    // Detection data
    void SetInputMagns(const WDL_TypedBuf<BL_FLOAT> &magns);
    void GetInputMagns(WDL_TypedBuf<BL_FLOAT> *magns,
                       bool applyFactor = true) const;

    void SetRawPartials(const vector<Partial2> &partials);
    void GetRawPartials(vector<Partial2> *partials,
                        bool applyFactor = true) const;

    void SetNormPartials(const vector<Partial2> &partials);
    void GetNormPartials(vector<Partial2> *partials,
                         bool applyFactor = true) const;
    
    // Params
    //
    void SetNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv);
    void GetNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                          bool applyFactor = true) const;

    // Only for displaying
    void SetHarmoEnvelope(const WDL_TypedBuf<BL_FLOAT> &harmoEnv);
    void GetHarmoEnvelope(WDL_TypedBuf<BL_FLOAT> *harmoEnv,
                          bool applyFactor = true) const;
    
    void SetDenormNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv);
    void GetDenormNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                                bool applyFactor = true) const;
    
    void SetAmplitude(BL_FLOAT amp);
    BL_FLOAT GetAmplitude(bool applyFactor = true) const;

    void SetFrequency(BL_FLOAT freq);
    BL_FLOAT GetFrequency(bool applyFactor = true) const;

    void SetColor(const WDL_TypedBuf<BL_FLOAT> &color);
    void GetColor(WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor = true) const;

    // Only for displaying
    void SetColorRaw(const WDL_TypedBuf<BL_FLOAT> &color);
    void GetColorRaw(WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor = true) const;

    // Only for displaying
    void SetColorProcessed(const WDL_TypedBuf<BL_FLOAT> &color);
    void GetColorProcessed(WDL_TypedBuf<BL_FLOAT> *color,
                           bool applyFactor = true) const;
    
    void SetWarping(const WDL_TypedBuf<BL_FLOAT> &warping);
    void GetWarping(WDL_TypedBuf<BL_FLOAT> *warping, bool applyFactor = true) const;

    void SetWarpingInv(const WDL_TypedBuf<BL_FLOAT> &warpingInv);
    void GetWarpingInv(WDL_TypedBuf<BL_FLOAT> *warpingInv,
                       bool applyFactor = true) const;

    // Only for displaying
    void SetWarpingProcessed(const WDL_TypedBuf<BL_FLOAT> &warping);
    void GetWarpingProcessed(WDL_TypedBuf<BL_FLOAT> *warping,
                             bool applyFactor = true) const;
    
    // For "raw"
    void SetPartials(const vector<Partial2> &partials);
    void SetPartials(const vector<MorphoPartial> &partials);
    void GetPartials(vector<MorphoPartial> *partials,
                     bool applyFactor = true) const;

    // Onsets
    void SetOnsetDetected(bool flag);
    bool GetOnsetDetected() const;
        
    // Factors
    void SetAmpFactor(BL_FLOAT factor);
    BL_FLOAT GetAmpFactor() const;
        
    void SetFreqFactor(BL_FLOAT factor);
    BL_FLOAT GetFreqFactor() const;
    
    void SetColorFactor(BL_FLOAT factor);
    BL_FLOAT GetColorFactor() const;
    
    void SetWarpingFactor(BL_FLOAT factor);
    BL_FLOAT GetWarpingFactor() const;

    void SetNoiseFactor(BL_FLOAT factor);
    BL_FLOAT GetNoiseFactor() const;

    //
    void ApplyMorphoFactors(BL_FLOAT ampFactor,
                            BL_FLOAT pitchFactor,
                            BL_FLOAT colorFactor,
                            BL_FLOAT warpingFactor,
                            BL_FLOAT noiseFactor);

    // Apply the current factors to the data and reset the factors
    void ApplyAndResetFactors();
    
    //
    static void MixFrames(MorphoFrame7 *result,
                          const MorphoFrame7 &frame0,
                          const MorphoFrame7 &frame1,
                          BL_FLOAT t, bool mixFreq);
    
    static void MultFrame(MorphoFrame7 *frame, BL_FLOAT t, bool multFreq);
    static void MultFrameFactors(MorphoFrame7 *frame, BL_FLOAT t, bool multFreq);
    static void AddFrames(MorphoFrame7 *frame0,
                          const MorphoFrame7 &frame1, bool addFreq);

    static void AddAllPartials(MorphoFrame7 *frame0, const MorphoFrame7 &frame1);
    static void ApplyFreqFactorPartials(MorphoFrame7 *frame);
    static void ApplyAmpFactorPartials(MorphoFrame7 *frame);
    static void ApplyAmplitudePartials(MorphoFrame7 *frame); // HACK
    
protected:
    void GetColorAux(const WDL_TypedBuf<BL_FLOAT> &sourceColor,
                     WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor = true) const;

    void ApplyFactorsPartials(vector<Partial2> *partials) const;
    void ApplyFactorsPartials(vector<MorphoPartial> *partials) const;

    void ApplyFreqFactorPartials(vector<Partial2> *partials) const; 
    void ApplyFreqFactorPartials(vector<MorphoPartial> *partials) const;

    void ApplyAmpFactorPartials(vector<Partial2> *partials) const; 
    void ApplyAmpFactorPartials(vector<MorphoPartial> *partials) const;

    void ApplyAmplitudePartials(vector<Partial2> *partials) const; 
    void ApplyAmplitudePartials(vector<MorphoPartial> *partials) const;
    
    static void MixPartials(vector<Partial2> *partials0,
                            const vector<Partial2> &partials1,
                            BL_FLOAT t);
    static void MixPartials(vector<MorphoPartial> *partials0,
                            const vector<MorphoPartial> &partials1In,
                            BL_FLOAT t);

    static void AddPartials(vector<Partial2> *partials0,
                            const vector<Partial2> &partials1);
    static void AddPartials(vector<MorphoPartial> *partials0,
                            const vector<MorphoPartial> &partials1);
    
    // Detection data
    //
    WDL_TypedBuf<BL_FLOAT> mInputMagns;
    vector<Partial2> mRawPartials;
    vector<Partial2> mNormPartials;
    
    // Data
    //
    WDL_TypedBuf<BL_FLOAT> mNoiseEnvelope;
    WDL_TypedBuf<BL_FLOAT> mDenormNoiseEnvelope;

    WDL_TypedBuf<BL_FLOAT> mHarmoEnvelope; // Only for displaying
    
    BL_FLOAT mAmplitude;
    BL_FLOAT mFrequency;
    
    WDL_TypedBuf<BL_FLOAT> mColor;    
    WDL_TypedBuf<BL_FLOAT> mColorRaw; // Only for displaying
    WDL_TypedBuf<BL_FLOAT> mColorProcessed; // Only for displaying
    
    WDL_TypedBuf<BL_FLOAT> mWarping;
    WDL_TypedBuf<BL_FLOAT> mWarpingInv;
    WDL_TypedBuf<BL_FLOAT> mWarpingProcessed; // Only for displaying
    
    vector<MorphoPartial> mPartials;

    bool mOnsetDetected;
    
    BL_FLOAT mAmpFactor;
    BL_FLOAT mFreqFactor;
    BL_FLOAT mColorFactor;
    BL_FLOAT mWarpingFactor;
    BL_FLOAT mNoiseFactor;
};

#endif
