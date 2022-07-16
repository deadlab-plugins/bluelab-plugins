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
//  MorphoFrame7.cpp
//  BL-Morpho
//
//  Created by applematuer on 2/2/19.
//
//

#include <BLUtils.h>
#include <BLUtilsMath.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include <BLDebug.h>

#include <Scale.h>

#include <IdLinker.h>

#include <Morpho_defs.h>

#include "MorphoFrame7.h"

MorphoFrame7::MorphoPartial::MorphoPartial()
{
    mFreq = 0.0;
    mAmp = 0.0;
    mPhase = 0.0;

    mLinkedId = -1;
    mState = Partial2::ALIVE;
    mWasAlive = false;

    mId = -1;
}

MorphoFrame7::MorphoPartial::MorphoPartial(const MorphoPartial &other)
{
    mFreq = other.mFreq;
    mAmp = other.mAmp;
    mPhase = other.mPhase;

    mLinkedId = other.mLinkedId;
    mState = other.mState;
    mWasAlive = other.mWasAlive;

    mId = other.mId;
}

MorphoFrame7::MorphoPartial::MorphoPartial(const Partial2 &other)
{
    mFreq = other.mFreq;
    mAmp = other.mAmp;
    mPhase = other.mPhase;

    mLinkedId = other.mLinkedId;
    mState = other.mState;
    mWasAlive = other.mWasAlive;

    mId = other.mId;
}

MorphoFrame7::MorphoPartial::~MorphoPartial() {}

bool
MorphoFrame7::MorphoPartial::AmpLess(const MorphoPartial &p1, const MorphoPartial &p2)
{
    return (p1.mAmp < p2.mAmp);
}

bool
MorphoFrame7::MorphoPartial::IdLess(const MorphoPartial &p1, const MorphoPartial &p2)
{
    return (p1.mId < p2.mId);
}

MorphoFrame7::MorphoFrame7()
{
    // Init
    mAmplitude = 1.0;
    mFrequency = 1.0;

    mAmpFactor = 1.0;
    mFreqFactor = 1.0;
    mColorFactor = 1.0;
    mWarpingFactor = 1.0;
    mNoiseFactor = 0.0;

    mOnsetDetected = false;
}

MorphoFrame7::MorphoFrame7(const MorphoFrame7 &other)
{
    mInputMagns = other.mInputMagns;
    mRawPartials = other.mRawPartials;
    mNormPartials = other.mNormPartials;
    
    mNoiseEnvelope = other.mNoiseEnvelope;
    mDenormNoiseEnvelope = other.mDenormNoiseEnvelope;

    mHarmoEnvelope = other.mHarmoEnvelope;
    
    mAmplitude = other.mAmplitude;
    mFrequency = other.mFrequency;
    
    mColor = other.mColor;    
    mColorRaw = other.mColorRaw;
    mColorProcessed = other.mColorProcessed;
    
    mWarping = other.mWarping;
    mWarpingInv = other.mWarpingInv;
    mWarpingProcessed = other.mWarpingProcessed;
    
    mPartials = other.mPartials;

    mOnsetDetected = other.mOnsetDetected;

    mAmpFactor = other.mAmpFactor;
    mFreqFactor = other.mFreqFactor;
    mColorFactor = other.mColorFactor;
    mWarpingFactor = other.mWarpingFactor;
    mNoiseFactor = other.mNoiseFactor;
}

MorphoFrame7::~MorphoFrame7() {}

void
MorphoFrame7::Reset()
{
    mInputMagns.Resize(0); //

    mRawPartials.clear(); //
    mNormPartials.clear(); //
    
    mNoiseEnvelope.Resize(0);
    mDenormNoiseEnvelope.Resize(0);

    mHarmoEnvelope.Resize(0);

    mAmplitude = 1.0;
    mFrequency = 1.0;
    
    mColor.Resize(0);
    mColorRaw.Resize(0);
    mColorProcessed.Resize(0);
 
    mWarping.Resize(0);
    mWarpingInv.Resize(0);
    mWarpingProcessed.Resize(0);

    mPartials.clear();

    //
    mAmpFactor = 1.0;
    mFreqFactor = 1.0;
    mColorFactor = 1.0;
    mWarpingFactor = 1.0;
    mNoiseFactor = 0.0;

    mOnsetDetected = false;
}

void
MorphoFrame7::SetInputMagns(const WDL_TypedBuf<BL_FLOAT> &magns)
{
    mInputMagns = magns;
}

void
MorphoFrame7::GetInputMagns(WDL_TypedBuf<BL_FLOAT> *magns,
                            bool applyFactor) const
{
    *magns = mInputMagns;

    if (applyFactor)
        BLUtils::MultValues(magns, mAmpFactor);
}

void
MorphoFrame7::SetRawPartials(const vector<Partial2> &partials)
{
    mRawPartials = partials;
}

void
MorphoFrame7::GetRawPartials(vector<Partial2> *partials, bool applyFactor) const
{
    *partials = mRawPartials;

    if (applyFactor)
        ApplyFactorsPartials(partials);
}

void
MorphoFrame7::SetNormPartials(const vector<Partial2> &partials)
{
    mNormPartials = partials;
}

void
MorphoFrame7::GetNormPartials(vector<Partial2> *partials,
                              bool applyFactor) const
{
    *partials = mNormPartials;

    if (applyFactor)
        ApplyFactorsPartials(partials);
}

void
MorphoFrame7::SetNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv)
{
    mNoiseEnvelope = noiseEnv;
}

void
MorphoFrame7::GetNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                               bool applyFactor) const
{
    *noiseEnv = mNoiseEnvelope;

    if (applyFactor)
    {
        BL_FLOAT noiseCoeff;
        BL_FLOAT harmoCoeff;
        BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                                  MAX_NOISE_COEFF);
        
        BLUtils::MultValues(noiseEnv, noiseCoeff);
        
        BLUtils::MultValues(noiseEnv, mAmpFactor); // Also apply global amp
    }
}

void
MorphoFrame7::SetDenormNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv)
{
    mDenormNoiseEnvelope = noiseEnv;
}

void
MorphoFrame7::GetDenormNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                                     bool applyFactor) const
{
    *noiseEnv = mDenormNoiseEnvelope;

    if (applyFactor)
    {
        BL_FLOAT noiseCoeff;
        BL_FLOAT harmoCoeff;
        BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                                  MAX_NOISE_COEFF);
        
        BLUtils::MultValues(noiseEnv, noiseCoeff);
        
        BLUtils::MultValues(noiseEnv, mAmpFactor); // Also apply global amp
    }
}

void
MorphoFrame7::SetHarmoEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv)
{
    mHarmoEnvelope = noiseEnv;
}

void
MorphoFrame7::GetHarmoEnvelope(WDL_TypedBuf<BL_FLOAT> *harmoEnv,
                               bool applyFactor) const
{
    *harmoEnv = mHarmoEnvelope;

    if (applyFactor)
    {
        BLUtils::MultValues(harmoEnv, mAmpFactor);

        BL_FLOAT noiseCoeff;
        BL_FLOAT harmoCoeff;
        BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                                  MAX_NOISE_COEFF);
        
        BLUtils::MultValues(harmoEnv, harmoCoeff);
    }
}

BL_FLOAT
MorphoFrame7::GetAmplitude(bool applyFactor) const
{
    BL_FLOAT result = mAmplitude;

    if (applyFactor)
    {
        result *= mAmpFactor;

        BL_FLOAT noiseCoeff;
        BL_FLOAT harmoCoeff;
        BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                                  MAX_NOISE_COEFF);
        
        result *= harmoCoeff;
    }

    return result;
}

BL_FLOAT
MorphoFrame7::GetFrequency(bool applyFactor) const
{
    BL_FLOAT result = mFrequency;

    if (applyFactor)
        result *= mFreqFactor;

    return result;
}

void
MorphoFrame7::GetColor(WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor) const
{
    GetColorAux(mColor, color, applyFactor);
}

void
MorphoFrame7::GetColorRaw(WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor) const
{
    GetColorAux(mColorRaw, color, applyFactor);
}

void
MorphoFrame7::GetColorAux(const WDL_TypedBuf<BL_FLOAT> &sourceColor,
                          WDL_TypedBuf<BL_FLOAT> *color, bool applyFactor) const
{
    *color = sourceColor;
    
    if (applyFactor)
    {
        // Sigmoid
        
        // mColorFactor is in [0, 2] here
        BL_FLOAT a = mColorFactor*0.5;

        // Reverse the effect of the color (so when at minimum, it is flat)
        a = 1.0 - a;
        
        if (a < BL_EPS)
            a = BL_EPS;
        if (a > 1.0 - BL_EPS)
            a = 1.0 - BL_EPS;

        BLUtilsMath::ApplySigmoid(color, a);
    }
}

void
MorphoFrame7::GetColorProcessed(WDL_TypedBuf<BL_FLOAT> *color,
                                bool applyFactor) const
{
    *color = mColorProcessed;

    if (applyFactor)
    {
        // Sigmoid
        BL_FLOAT a = mColorFactor*0.5;

        // Reverse the effect of the color (so when at minimum, it is flat)
        a = 1.0 - a;
        
        if (a < BL_EPS)
            a = BL_EPS;
        if (a > 1.0 - BL_EPS)
            a = 1.0 - BL_EPS;
        
        BLUtilsMath::ApplySigmoid(color, a);
    }
}

void
MorphoFrame7::GetWarping(WDL_TypedBuf<BL_FLOAT> *warping,
                         bool applyFactor) const
{
    *warping = mWarping;

    if (applyFactor)
    {
        BLUtils::AddValues(warping, (BL_FLOAT)-1.0);
        BLUtils::MultValues(warping, mWarpingFactor);
        BLUtils::AddValues(warping, (BL_FLOAT)1.0);
    }
}

void
MorphoFrame7::GetWarpingInv(WDL_TypedBuf<BL_FLOAT> *warpingInv,
                            bool applyFactor) const
{
    *warpingInv = mWarpingInv;

    if (applyFactor)
    {
        BLUtils::AddValues(warpingInv, (BL_FLOAT)-1.0);
        BLUtils::MultValues(warpingInv, mWarpingFactor);
        BLUtils::AddValues(warpingInv, (BL_FLOAT)1.0);
    }
}

void
MorphoFrame7::GetWarpingProcessed(WDL_TypedBuf<BL_FLOAT> *warping,
                                  bool applyFactor) const
{
    *warping = mWarpingProcessed;

    if (applyFactor)
    {
        BLUtils::AddValues(warping, (BL_FLOAT)-1.0);
        BLUtils::MultValues(warping, mWarpingFactor);
        BLUtils::AddValues(warping, (BL_FLOAT)1.0);
    }
}

void
MorphoFrame7::SetAmplitude(BL_FLOAT amp)
{
    mAmplitude = amp;
}

void
MorphoFrame7::SetFrequency(BL_FLOAT freq)
{
    mFrequency = freq;
}

void
MorphoFrame7::SetColor(const WDL_TypedBuf<BL_FLOAT> &color)
{    
    mColor = color;
}

void
MorphoFrame7::SetColorRaw(const WDL_TypedBuf<BL_FLOAT> &color)
{    
    mColorRaw = color;
}

void
MorphoFrame7::SetColorProcessed(const WDL_TypedBuf<BL_FLOAT> &color)
{    
    mColorProcessed = color;
}

void
MorphoFrame7::SetWarping(const WDL_TypedBuf<BL_FLOAT> &warping)
{    
    mWarping = warping;
}

void
MorphoFrame7::SetWarpingInv(const WDL_TypedBuf<BL_FLOAT> &warpingInv)
{    
    mWarpingInv = warpingInv;
}

void
MorphoFrame7::SetWarpingProcessed(const WDL_TypedBuf<BL_FLOAT> &warping)
{    
    mWarpingProcessed = warping;
}

void
MorphoFrame7::SetPartials(const vector<Partial2> &partials)
{
    mPartials.resize(partials.size());
    
    // Convert
    for (int i = 0; i < mPartials.size(); i++)
        mPartials[i] = partials[i];
}

void
MorphoFrame7::SetPartials(const vector<MorphoPartial> &partials)
{
    mPartials = partials;
}

void
MorphoFrame7::GetPartials(vector<MorphoPartial> *partials,
                          bool applyFactor) const
{
    *partials = mPartials;

    if (applyFactor)
        ApplyFactorsPartials(partials);
}

void
MorphoFrame7::SetOnsetDetected(bool flag)
{
    mOnsetDetected = flag;
}

bool
MorphoFrame7::GetOnsetDetected() const
{
    return mOnsetDetected;
}

void
MorphoFrame7::SetAmpFactor(BL_FLOAT factor)
{
    mAmpFactor = factor;
}

BL_FLOAT
MorphoFrame7::GetAmpFactor() const
{
    return mAmpFactor;
}

void
MorphoFrame7::SetFreqFactor(BL_FLOAT factor)
{
    mFreqFactor = factor;
}

BL_FLOAT
MorphoFrame7::GetFreqFactor() const
{
    return mFreqFactor;
}

void
MorphoFrame7::SetColorFactor(BL_FLOAT factor)
{
    mColorFactor = factor;
}

BL_FLOAT
MorphoFrame7::GetColorFactor() const
{
    return mColorFactor;
}

void
MorphoFrame7::SetWarpingFactor(BL_FLOAT factor)
{
    mWarpingFactor = factor;
}

BL_FLOAT
MorphoFrame7::GetWarpingFactor() const
{
    return mWarpingFactor;
}

void
MorphoFrame7::SetNoiseFactor(BL_FLOAT factor)
{
    mNoiseFactor = factor;
}

BL_FLOAT
MorphoFrame7::GetNoiseFactor() const
{
    return mNoiseFactor;
}

void
MorphoFrame7::ApplyMorphoFactors(BL_FLOAT ampFactor,
                                 BL_FLOAT pitchFactor,
                                 BL_FLOAT colorFactor,
                                 BL_FLOAT warpingFactor,
                                 BL_FLOAT noiseFactor)
{
    mAmpFactor *= ampFactor;
    mFreqFactor *= pitchFactor;
    mColorFactor *= colorFactor;
    mWarpingFactor *= warpingFactor;
    
    BLUtils::ScaleMixParam(&mNoiseFactor, noiseFactor);
}

void
MorphoFrame7::ApplyAndResetFactors()
{
    // Amp
    BL_FLOAT amp = GetAmplitude();
    SetAmplitude(amp);
    
    BL_FLOAT freq = GetFrequency();    
    SetFrequency(freq);
    
    // Color
    WDL_TypedBuf<BL_FLOAT> color;
    GetColor(&color);
    SetColor(color);

    // Color Raw (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorRaw;
    GetColorRaw(&colorRaw);
    SetColorRaw(colorRaw);

    // Color processed (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorProcessed;
    GetColorProcessed(&colorProcessed);
    SetColorProcessed(colorProcessed);
    
    // Warping
    WDL_TypedBuf<BL_FLOAT> warp;
    GetWarping(&warp);
    SetWarping(warp);
    
    // Noise envelope
    WDL_TypedBuf<BL_FLOAT> noise;
    GetNoiseEnvelope(&noise);
    SetNoiseEnvelope(noise);

    // Denorm noise envelope
    WDL_TypedBuf<BL_FLOAT> noiseDenorm;
    GetDenormNoiseEnvelope(&noiseDenorm);
    SetDenormNoiseEnvelope(noiseDenorm);
    
    ApplyFactorsPartials(&mPartials);

    // Reset
    mAmplitude = 1.0;
    mFrequency = 1.0;

    mAmpFactor = 1.0;
    mFreqFactor = 1.0;
    mColorFactor = 1.0;
    mWarpingFactor = 1.0;
    mNoiseFactor = 0.0; // Maybe will affect the noise/harmo mix during synth??
}

void
MorphoFrame7::MixFrames(MorphoFrame7 *result,
                        const MorphoFrame7 &frame0,
                        const MorphoFrame7 &frame1,
                        BL_FLOAT t, bool mixFreq)
{
    // Amp
    BL_FLOAT amp0 = frame0.GetAmplitude();
    BL_FLOAT amp1 = frame1.GetAmplitude();
    BL_FLOAT resultAmp = (1.0 - t)*amp0 + t*amp1;
    result->SetAmplitude(resultAmp);
    
    // Freq
    if (mixFreq)
    {
        BL_FLOAT freq0 = frame0.GetFrequency();
        BL_FLOAT freq1 = frame1.GetFrequency();
        BL_FLOAT resultFreq = (1.0 - t)*freq0 + t*freq1;
        
        result->SetFrequency(resultFreq);
    }
    else
    {
        BL_FLOAT freq0 = frame0.GetFrequency();
        result->SetFrequency(freq0);
    }
    
    // Color
    WDL_TypedBuf<BL_FLOAT> color0;
    frame0.GetColor(&color0);
    
    WDL_TypedBuf<BL_FLOAT> color1;
    frame1.GetColor(&color1);
    
    WDL_TypedBuf<BL_FLOAT> resultColor;
    BLUtils::Interp(&resultColor, &color0, &color1, t);
    
    result->SetColor(resultColor);

    // Color Raw (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorRaw0;
    frame0.GetColorRaw(&colorRaw0);
    
    WDL_TypedBuf<BL_FLOAT> colorRaw1;
    frame1.GetColorRaw(&colorRaw1);
    
    WDL_TypedBuf<BL_FLOAT> resultColorRaw;
    BLUtils::Interp(&resultColorRaw, &colorRaw0, &colorRaw1, t);
    
    result->SetColorRaw(resultColorRaw);

    // Color processed (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorProcessed0;
    frame0.GetColorProcessed(&colorProcessed0);
    
    WDL_TypedBuf<BL_FLOAT> colorProcessed1;
    frame1.GetColorProcessed(&colorProcessed1);
    
    WDL_TypedBuf<BL_FLOAT> resultColorProcessed;
    BLUtils::Interp(&resultColorProcessed, &colorProcessed0, &colorProcessed1, t);
    
    result->SetColorProcessed(resultColorProcessed);
    
    // Warping
    WDL_TypedBuf<BL_FLOAT> warp0;
    frame0.GetWarping(&warp0);
    
    WDL_TypedBuf<BL_FLOAT> warp1;
    frame1.GetWarping(&warp1);
    
    WDL_TypedBuf<BL_FLOAT> resultWarping;
    BLUtils::Interp(&resultWarping, &warp0, &warp1, t);
    
    result->SetWarping(resultWarping);

    // Warping inv
    WDL_TypedBuf<BL_FLOAT> warpInv0;
    frame0.GetWarpingInv(&warpInv0);
    
    WDL_TypedBuf<BL_FLOAT> warpInv1;
    frame1.GetWarpingInv(&warpInv1);
    
    WDL_TypedBuf<BL_FLOAT> resultWarpingInv;
    BLUtils::Interp(&resultWarpingInv, &warpInv0, &warpInv1, t);
    
    result->SetWarpingInv(resultWarpingInv);

    // Warping processed
    WDL_TypedBuf<BL_FLOAT> warpProcessed0;
    frame0.GetWarpingProcessed(&warpProcessed0);
    
    WDL_TypedBuf<BL_FLOAT> warpProcessed1;
    frame1.GetWarpingProcessed(&warpProcessed1);
    
    WDL_TypedBuf<BL_FLOAT> resultWarpingProcessed;
    BLUtils::Interp(&resultWarpingProcessed, &warpProcessed0, &warpProcessed1, t);
    
    result->SetWarpingProcessed(resultWarpingProcessed);
    
    // Noise envelope
    WDL_TypedBuf<BL_FLOAT> noise0;
    frame0.GetNoiseEnvelope(&noise0);
    
    WDL_TypedBuf<BL_FLOAT> noise1;
    frame1.GetNoiseEnvelope(&noise1);
    
    WDL_TypedBuf<BL_FLOAT> resultNoise;
    BLUtils::Interp(&resultNoise, &noise0, &noise1, t);
    
    result->SetNoiseEnvelope(resultNoise);

    // Denorm noise envelope
    WDL_TypedBuf<BL_FLOAT> noiseDenorm0;
    frame0.GetDenormNoiseEnvelope(&noiseDenorm0);
    
    WDL_TypedBuf<BL_FLOAT> noiseDenorm1;
    frame1.GetDenormNoiseEnvelope(&noiseDenorm1);
    
    WDL_TypedBuf<BL_FLOAT> resultNoiseDenorm;
    BLUtils::Interp(&resultNoiseDenorm, &noiseDenorm0, &noiseDenorm1, t);
    
    result->SetDenormNoiseEnvelope(resultNoiseDenorm);
    
    // Partials
    result->mRawPartials = frame0.mRawPartials;
    MixPartials(&result->mRawPartials, frame1.mRawPartials, t);

    result->mNormPartials = frame0.mNormPartials;
    MixPartials(&result->mNormPartials, frame1.mNormPartials, t);

    MixPartials(&result->mPartials, frame1.mPartials, t);
    
    // Harmonic envelope...
}

void
MorphoFrame7::MultFrame(MorphoFrame7 *frame, BL_FLOAT t, bool multFreq)
{
    // Amp
    BL_FLOAT amp = frame->GetAmplitude();
    amp *= t;
    frame->SetAmplitude(amp);
    
    // Freq
    if (multFreq)
    {
        BL_FLOAT freq = frame->GetFrequency();
        freq *= t; 
        frame->SetFrequency(freq);
    }
    
    // Color
    WDL_TypedBuf<BL_FLOAT> color;
    frame->GetColor(&color, false);
    BLUtils::MultValues(&color, t);
    frame->SetColor(color);

    // Color Raw (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorRaw;
    frame->GetColorRaw(&colorRaw, false);
    BLUtils::MultValues(&colorRaw, t);
    frame->SetColorRaw(colorRaw);

    // Color processed (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorProcessed;
    frame->GetColorProcessed(&colorProcessed, false);
    BLUtils::MultValues(&colorProcessed, t);
    frame->SetColorProcessed(colorProcessed);
    
    // Warping
    WDL_TypedBuf<BL_FLOAT> warping;
    frame->GetWarping(&warping, false);
    BLUtils::MultValues(&warping, t);
    frame->SetWarping(warping);
        
    // Noise envelope
    WDL_TypedBuf<BL_FLOAT> noise;
    frame->GetNoiseEnvelope(&noise, false);
    BLUtils::MultValues(&noise, t);
    frame->SetNoiseEnvelope(noise);

    // Denorm noise envelope
    WDL_TypedBuf<BL_FLOAT> noiseDenorm;
    frame->GetDenormNoiseEnvelope(&noiseDenorm, false);
    BLUtils::MultValues(&noiseDenorm, t);
    frame->SetDenormNoiseEnvelope(noiseDenorm);
    
    // Harmonic envelope....
}

void
MorphoFrame7::MultFrameFactors(MorphoFrame7 *frame, BL_FLOAT t, bool multFreq)
{
    frame->mAmpFactor *= t;

    if (multFreq)
        frame->mFreqFactor *= t;
    
    frame->mColorFactor *= t;
    frame->mWarpingFactor *= t;
    frame->mNoiseFactor *= t;
}
    
void
MorphoFrame7::AddFrames(MorphoFrame7 *frame0, const MorphoFrame7 &frame1,
                        bool addFreq)
{
    // Amp
    BL_FLOAT amp0 = frame0->GetAmplitude();
    BL_FLOAT amp1 = frame1.GetAmplitude();
    BL_FLOAT resultAmp = amp0 + amp1;
    frame0->SetAmplitude(resultAmp);
    
    // Freq
    if (addFreq)
    {
        BL_FLOAT freq0 = frame0->GetFrequency();
        BL_FLOAT freq1 = frame1.GetFrequency();
        BL_FLOAT resultFreq = freq0 + freq1;
        
        frame0->SetFrequency(resultFreq);
    }
    
    // Color
    WDL_TypedBuf<BL_FLOAT> color0;
    frame0->GetColor(&color0);
    
    WDL_TypedBuf<BL_FLOAT> color1;
    frame1.GetColor(&color1);
    
    WDL_TypedBuf<BL_FLOAT> resultColor;
    BLUtils::AddValues(&resultColor, color0, color1);
    frame0->SetColor(resultColor);
    
    // Color Raw (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorRaw0;
    frame0->GetColorRaw(&colorRaw0);
    
    WDL_TypedBuf<BL_FLOAT> colorRaw1;
    frame1.GetColorRaw(&colorRaw1);
    
    WDL_TypedBuf<BL_FLOAT> resultColorRaw;
    BLUtils::AddValues(&resultColorRaw, colorRaw0, colorRaw1);
    frame0->SetColorRaw(resultColorRaw);

    // Color processed (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorProcessed0;
    frame0->GetColorProcessed(&colorProcessed0);
    
    WDL_TypedBuf<BL_FLOAT> colorProcessed1;
    frame1.GetColorProcessed(&colorProcessed1);
    
    WDL_TypedBuf<BL_FLOAT> resultColorProcessed;
    BLUtils::AddValues(&resultColorProcessed, colorProcessed0, colorProcessed1);
    frame0->SetColorProcessed(resultColorProcessed);
    
    // Warping
    WDL_TypedBuf<BL_FLOAT> warp0;
    frame0->GetWarping(&warp0);
    
    WDL_TypedBuf<BL_FLOAT> warp1;
    frame1.GetWarping(&warp1);
    
    WDL_TypedBuf<BL_FLOAT> resultWarping;
    BLUtils::AddValues(&resultWarping, warp0, warp1);
    frame0->SetWarping(resultWarping);
    
    // Noise envelope
    WDL_TypedBuf<BL_FLOAT> noise0;
    frame0->GetNoiseEnvelope(&noise0);
    
    WDL_TypedBuf<BL_FLOAT> noise1;
    frame1.GetNoiseEnvelope(&noise1);
    
    WDL_TypedBuf<BL_FLOAT> resultNoise;
    BLUtils::AddValues(&resultNoise, noise0, noise1);
    frame0->SetNoiseEnvelope(resultNoise);

    // Denorm noise envelope
    WDL_TypedBuf<BL_FLOAT> noiseDenorm0;
    frame0->GetDenormNoiseEnvelope(&noiseDenorm0);
    
    WDL_TypedBuf<BL_FLOAT> noiseDenorm1;
    frame1.GetDenormNoiseEnvelope(&noiseDenorm1);
    
    WDL_TypedBuf<BL_FLOAT> resultNoiseDenorm;
    BLUtils::AddValues(&resultNoiseDenorm, noiseDenorm0, noiseDenorm1);
    frame0->SetDenormNoiseEnvelope(resultNoiseDenorm);

    // Partials
    AddPartials(&frame0->mPartials, frame1.mPartials);
    AddPartials(&frame0->mRawPartials, frame1.mRawPartials);
    AddPartials(&frame0->mNormPartials, frame1.mNormPartials);
    
    // Harmonic envelope...
}

void
MorphoFrame7::AddAllPartials(MorphoFrame7 *frame0, const MorphoFrame7 &frame1)
{
    AddPartials(&frame0->mPartials, frame1.mPartials);
    AddPartials(&frame0->mRawPartials, frame1.mRawPartials);
    AddPartials(&frame0->mNormPartials, frame1.mNormPartials);
}

void
MorphoFrame7::ApplyFreqFactorPartials(MorphoFrame7 *frame)
{
    frame->ApplyFreqFactorPartials(&frame->mPartials);
    frame->ApplyFreqFactorPartials(&frame->mRawPartials);
    frame->ApplyFreqFactorPartials(&frame->mNormPartials);
}

void
MorphoFrame7::ApplyAmpFactorPartials(MorphoFrame7 *frame)
{
    frame->ApplyAmpFactorPartials(&frame->mPartials);
    frame->ApplyAmpFactorPartials(&frame->mRawPartials);
    frame->ApplyAmpFactorPartials(&frame->mNormPartials);
}

void
MorphoFrame7::ApplyAmplitudePartials(MorphoFrame7 *frame)
{
    frame->ApplyAmplitudePartials(&frame->mPartials);
    frame->ApplyAmplitudePartials(&frame->mRawPartials);
    frame->ApplyAmplitudePartials(&frame->mNormPartials);
}

void
MorphoFrame7::ApplyFactorsPartials(vector<Partial2> *partials) const
{
    BL_FLOAT noiseCoeff;
    BL_FLOAT harmoCoeff;
    BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                              MAX_NOISE_COEFF);
        
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &partial = (*partials)[i];
        partial.mAmp *= mAmpFactor;
        partial.mFreq *= mFreqFactor;

        partial.mAmp *= harmoCoeff;
    }
}

void
MorphoFrame7::ApplyFactorsPartials(vector<MorphoPartial> *partials) const
{
    BL_FLOAT noiseCoeff;
    BL_FLOAT harmoCoeff;
    BLUtils::MixParamToCoeffs(mNoiseFactor, &harmoCoeff, &noiseCoeff,
                              MAX_NOISE_COEFF);
      
    for (int i = 0; i < partials->size(); i++)
    {
        MorphoPartial &partial = (*partials)[i];
        partial.mAmp *= mAmpFactor;
        partial.mFreq *= mFreqFactor;

        partial.mAmp *= harmoCoeff;
    }
}

void
MorphoFrame7::ApplyFreqFactorPartials(vector<Partial2> *partials) const
{    
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &partial = (*partials)[i];
        partial.mFreq *= mFreqFactor;
    }
}

void
MorphoFrame7::ApplyFreqFactorPartials(vector<MorphoPartial> *partials) const
{
    for (int i = 0; i < partials->size(); i++)
    {
        MorphoPartial &partial = (*partials)[i];
        partial.mFreq *= mFreqFactor;
    }
}

void
MorphoFrame7::ApplyAmpFactorPartials(vector<Partial2> *partials) const
{    
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &partial = (*partials)[i];
        partial.mAmp *= mAmpFactor;
    }
}

void
MorphoFrame7::ApplyAmpFactorPartials(vector<MorphoPartial> *partials) const
{
    for (int i = 0; i < partials->size(); i++)
    {
        MorphoPartial &partial = (*partials)[i];
        partial.mAmp *= mAmpFactor;
    }
}

void
MorphoFrame7::ApplyAmplitudePartials(vector<Partial2> *partials) const
{    
    for (int i = 0; i < partials->size(); i++)
    {
        Partial2 &partial = (*partials)[i];
        partial.mAmp *= mAmplitude*mAmpFactor;
    }
}

void
MorphoFrame7::ApplyAmplitudePartials(vector<MorphoPartial> *partials) const
{
    for (int i = 0; i < partials->size(); i++)
    {
        MorphoPartial &partial = (*partials)[i];
        partial.mAmp *= mAmplitude*mAmpFactor;
    }
}

void
MorphoFrame7::MixPartials(vector<Partial2> *partials0,
                          const vector<Partial2> &partials1In,
                          BL_FLOAT t)
{
    vector<Partial2> partials1 = partials1In;
        
    // Added for Morpho
    IdLinker<Partial2, Partial2>::LinkIds(partials0, &partials1, true);

    for (int i = 0; i < partials1.size(); i++)
    {
        const Partial2 &p1 = partials1[i];
        if (p1.mLinkedId == -1)
        {
            partials0->push_back(p1);
            
            continue;
        }

        Partial2 &p0 = (*partials0)[i];
        
        p0.mFreq = (1.0 - t)*p0.mFreq + t*p1.mFreq;
        p0.mAmp = (1.0 - t)*p0.mAmp + t*p1.mAmp;
        p0.mPhase = (1.0 - t)*p0.mPhase + t*p1.mPhase;
    }
}

void
MorphoFrame7::MixPartials(vector<MorphoPartial> *partials0,
                          const vector<MorphoPartial> &partials1In,
                          BL_FLOAT t)
{
    vector<MorphoPartial> partials1 = partials1In;
        
    // Added for Morpho
    IdLinker<MorphoPartial, MorphoPartial>::LinkIds(partials0, &partials1, true);

    for (int i = 0; i < partials1.size(); i++)
    {
        const MorphoPartial &p1 = partials1[i];
        if (p1.mLinkedId == -1)
        {
            partials0->push_back(p1);
            
            continue;
        }

        MorphoPartial &p0 = (*partials0)[i];
        
        p0.mFreq = (1.0 - t)*p0.mFreq + t*p1.mFreq;
        p0.mAmp = (1.0 - t)*p0.mAmp + t*p1.mAmp;
        p0.mPhase = (1.0 - t)*p0.mPhase + t*p1.mPhase;
    }
}

void
MorphoFrame7::AddPartials(vector<Partial2> *partials0,
                          const vector<Partial2> &partials1)
{
    if (partials0->empty())
    {
        *partials0 = partials1;

        return;
    }

    if (partials1.empty())
        return;

    int maxId0 = 0;
    for (int i = 0; i < partials0->size(); i++)
    {
        int id = (*partials0)[i].mId;
        if (id > maxId0)
            maxId0 = id;
    }

    // Guarantee that the new ids are "aligned" when num partials in frame0 change
    int idOffset =
        BLUtilsMath::RoundToNextMultiple(maxId0 + PARTIAL_ID_FRAME_OFFSET,
                                         PARTIAL_ID_FRAME_OFFSET);

    int partials0I = partials0->size();
    
    int newSize = partials0->size() + partials1.size();
    partials0->resize(newSize);
    for (int i = 0; i < partials1.size(); i++)
    {
        Partial2 &p0 = (*partials0)[partials0I++];
        const Partial2 &p1 = partials1[i];
        p0 = p1;
        
        if (p0.mId >= 0)
            p0.mId += idOffset;
    }
}

void
MorphoFrame7::AddPartials(vector<MorphoPartial> *partials0,
                          const vector<MorphoPartial> &partials1)
{
    if (partials0->empty())
    {
        *partials0 = partials1;

        return;
    }

    if (partials1.empty())
        return;
    
    int maxId0 = 0;
    for (int i = 0; i < partials0->size(); i++)
    {
        int id = (*partials0)[i].mId;
        if (id > maxId0)
            maxId0 = id;
    }

    // Guarantee that the new ids are "aligned" when num partials in frame0 change
    int idOffset =
        BLUtilsMath::RoundToNextMultiple(maxId0 + PARTIAL_ID_FRAME_OFFSET,
                                         PARTIAL_ID_FRAME_OFFSET);
 
    int partials0I = partials0->size();
    
    int newSize = partials0->size() + partials1.size();
    partials0->resize(newSize);
    for (int i = 0; i < partials1.size(); i++)
    {
        MorphoPartial &p0 = (*partials0)[partials0I++];
        const MorphoPartial &p1 = partials1[i];
        p0 = p1;
        
        if (p0.mId >= 0)
            p0.mId += idOffset;
    }
}
