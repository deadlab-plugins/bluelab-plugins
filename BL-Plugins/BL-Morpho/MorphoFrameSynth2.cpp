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
 
#include <IdLinker.h>

#include <BLUtils.h>
#include <BLUtilsMath.h>

// Do not speed up...
#include <SinLUT.h>

#include <MorphoFrame7.h>

#include <BLDebug.h>

#include "MorphoFrameSynth2.h"

// Synthesis
#define SYNTH_AMP_COEFF 4.0
#define SYNTH_MIN_FREQ 50.0
#define SYNTH_DEAD_PARTIAL_DECREASE 0.25

#define SYNTH_AMP_COEFF_RESYNTH 3.0

// Set to 0 for optimization
//
// Origin: interpolate everything in db or mel scale
//
// Optim: this looks to work well when set to 0
// Perfs: 26% => 17%CPU
#define INTERP_RESCALE 0 // Origin: 1

// TEST: nearest interpolation, to be more quick
#define NEAREST_INTERP 0 //1

// Optim: optimizes a little (~5/15%)
SIN_LUT_CREATE(MORPHO_FRAME_SIN_LUT, 4096);

// 4096*64 (2MB)
// 2x slower than 4096
//SIN_LUT_CREATE(SAS_FRAME_SIN_LUT, 262144);

//
MorphoFrameSynth2::MorphoFrameSynth2(int bufferSize, int oversampling,
                               int freqRes, BL_FLOAT sampleRate)
{
    SIN_LUT_INIT(MORPHO_FRAME_SIN_LUT);

    mBufferSize = bufferSize;
    mOverlapping = oversampling;
    mFreqRes = freqRes;
    mSampleRate = sampleRate;
    
    //mSynthMode = RAW_PARTIALS;
    mSynthMode = SOURCE_PARTIALS;

    mMinAmpDB = -120.0;

    mSynthEvenPartials = true;
    mSynthOddPartials = true;

    mAmplitude = 0.0;
    mPrevAmplitude = 0.0;
    
    mFrequency = 0.0;
    mPrevFrequency = -1.0;
}

MorphoFrameSynth2::~MorphoFrameSynth2() {}

void
MorphoFrameSynth2::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    mAmplitude = 0.0;
    mPrevAmplitude = 0.0;
    
    mFrequency = 0.0;
    mPrevFrequency = -1.0;
}

void
MorphoFrameSynth2::Reset(int bufferSize, int oversampling,
                      int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mOverlapping = oversampling;
    mFreqRes = freqRes;
    mSampleRate = sampleRate;
    
    Reset(sampleRate);
}

void
MorphoFrameSynth2::SetMinAmpDB(BL_FLOAT ampDB)
{
    mMinAmpDB = ampDB;
}

void
MorphoFrameSynth2::SetSynthMode(enum SynthMode mode)
{
    mSynthMode = mode;
}

MorphoFrameSynth2::SynthMode
MorphoFrameSynth2::GetSynthMode() const
{
    return mSynthMode;
}

void
MorphoFrameSynth2::SetSynthEvenPartials(bool flag)
{
    mSynthEvenPartials = flag;
}

void
MorphoFrameSynth2::SetSynthOddPartials(bool flag)
{
    mSynthOddPartials = flag;
}

void
MorphoFrameSynth2::AddMorphoFrame(const MorphoFrame7 &frame)
{
    mPrevMorphoFrame = mMorphoFrame;
    mMorphoFrame = frame;

    mPrevAmplitude = mAmplitude;
    mAmplitude = mMorphoFrame.GetAmplitude();

    mPrevFrequency = mFrequency;
    mFrequency = mMorphoFrame.GetFrequency();
}

void
MorphoFrameSynth2::GetMorphoFrame(MorphoFrame7 *frame)
{
    *frame = mMorphoFrame;
}

void
MorphoFrameSynth2::ComputeSamples(WDL_TypedBuf<BL_FLOAT> *samples)
{
    UpdateMorphoData();

    bool onsetDetected = mMorphoFrame.GetOnsetDetected();
    
    if (!onsetDetected)
        // Generate samples only if not on an onset (transient)
    {
        if (mSynthMode == RAW_PARTIALS)
            ComputeSamplesPartialsRaw(samples);
        else if (mSynthMode == SOURCE_PARTIALS)
            ComputeSamplesPartialsSourceNorm(samples);
            //ComputeSamplesPartialsSource(samples);
        else if (mSynthMode == RESYNTH_PARTIALS)
            ComputeSamplesPartialsResynth(samples);
    }
    else
    {
        // Set the volumes to 0, for next step, to avoid click
        mPrevAmplitude = 0.0;
    }
}

// Directly use partials provided
void
MorphoFrameSynth2::ComputeSamplesPartialsRaw(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
    {
        return;
    }

    // Amp factor already applied on partials by MorphoFrame7::ApplyMorphoFactors() 
    //BL_FLOAT ampFactor = mMorphoFrame.GetAmpFactor();
    
    MorphoFrame7::MorphoPartial partial;
    BL_FLOAT twoPiSR = 2.0*M_PI/mSampleRate;
    for (int i = 0; i < mPartials.size(); i++)
    {
        BL_FLOAT phase = 0.0;

        int prevPartialIdx = mPartials[i].mLinkedId;
        
        if (prevPartialIdx != -1)
            phase = mPrevPartials[prevPartialIdx].mPhase;

        int numSamples = samples->GetSize()/mOverlapping;
        BL_FLOAT partialT = 0.0;
        BL_FLOAT partialTIncr = 1.0/numSamples;
        for (int j = 0; j < numSamples; j++)
        {
            // Get interpolated partial
            //BL_FLOAT partialT = ((BL_FLOAT)j)/numSamples;
            GetPartial(&partial, i, partialT);
            
            BL_FLOAT freq = partial.mFreq;
                
            BL_FLOAT amp = partial.mAmp;

            // Amp factor already applied on partials by MorphoFrame7::ApplyMorphoFactors() 
            //amp *= ampFactor;
            
            BL_FLOAT samp = amp*std::sin(phase); // cos
            
            samp *= SYNTH_AMP_COEFF;
            
            if (freq >= SYNTH_MIN_FREQ)
                samples->Get()[j] += samp;
            
            //phase += 2.0*M_PI*freq/mSampleRate;
            phase += twoPiSR*freq;

            partialT += partialTIncr;
        }
        
        mPartials[i].mPhase = phase;
    }
}

// Directly use partials provided, but also apply Morpho parameter to them
void
MorphoFrameSynth2::ComputeSamplesPartialsSourceNorm(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
    {
        return;
    }

    BL_FLOAT ampFactor = mMorphoFrame.GetAmpFactor();
    BL_FLOAT freqFactor = mMorphoFrame.GetFreqFactor();
    
    // For optim, precompute 1/color
    WDL_TypedBuf<BL_FLOAT> &colorInv = mTmpBuf1;
    //colorInv = mColor;
    mMorphoFrame.GetColor(&colorInv, false);

    // NOTE: this may be improved...
    BLUtils::ClipMin(&colorInv,  BL_EPS10); // Avoid having spurious color peaks
    BLUtils::ApplyInv(&colorInv);
    BLUtils::ClipMax(&colorInv,  1.0); // // Avoid having values line 1e+10!
    
    // Optim
    BL_FLOAT twoPiSR = 2.0*M_PI/mSampleRate;

    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT hzPerBinInv = 1.0/hzPerBin;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        int prevPartialIdx = mPartials[i].mLinkedId;

        BL_FLOAT phase = 0.0;
        if (prevPartialIdx != -1)
            phase = mPrevPartials[prevPartialIdx].mPhase;
        
        // Generate samples
        int numSamples = samples->GetSize()/mOverlapping;
        MorphoFrame7::MorphoPartial partial;
        for (int j = 0; j < numSamples; j++)
        {            
            // Get interpolated partial
            BL_FLOAT partialT = ((BL_FLOAT)j)/numSamples;
            GetPartial(&partial, i, partialT);
            
            BL_FLOAT binIdx = partial.mFreq*hzPerBinInv;
            
            // Apply Morpho parameters to current partial
            //

            // Cancel color(1)
            //BL_FLOAT colorFactor = mColorFactor;
            //mColorFactor = 1.0;
            //BL_FLOAT col0 = GetColor(mColor, binIdx);
            BL_FLOAT col0 = GetColor(colorInv, binIdx); // Optim
            //mColorFactor = colorFactor;
            //partial.mAmp /= col0;
            partial.mAmp *= col0;
 
            // Inverse warping
            // => the detected partials will then be aligned to harmonics after that
            //
            // (No need to revert additional warping after)            
            //BL_FLOAT warpFactor = mWarpingFactor;
            //mWarpingFactor = 1.0;
            BL_FLOAT w0 = GetWarping(mWarpingInv, binIdx);
            //mWarpingFactor = warpFactor;

            partial.mFreq *= w0;

            // Recompute bin idx
            binIdx = partial.mFreq*hzPerBinInv;
            
            // Apply warping
            BL_FLOAT w = GetWarping(mWarping, binIdx);
            partial.mFreq *= w;
            
            // Recompute bin idx
            binIdx = partial.mFreq*hzPerBinInv;
            
            // Apply color
            BL_FLOAT col = GetColor(mColor, binIdx);
            
            partial.mAmp *= col;

            // Amplitude
            partial.mAmp *= ampFactor;
                        
            // Freq factor (post)
            partial.mFreq *= freqFactor;
                
            //
            BL_FLOAT freq = partial.mFreq;
                
            BL_FLOAT amp = partial.mAmp;
            
            BL_FLOAT samp = amp*std::sin(phase);
            
            samp *= SYNTH_AMP_COEFF;
            
            if (freq >= SYNTH_MIN_FREQ)
                samples->Get()[j] += samp;
            
            phase += twoPiSR*freq;
        }
        
        mPartials[i].mPhase = phase;
    }

#if 0 // Not right to apply amplitude here.
    // Must apply it when mixing, source by source
    
    // At the end, apply amplitude
    //
    BL_FLOAT tStep = 1.0/(samples->GetSize() - 1);
    BL_FLOAT t = 0.0;
    for (int i = 0; i < samples->GetSize(); i++)
    {
        BL_FLOAT s = samples->Get()[i];
        //BL_FLOAT d = ampDenoms.Get()[i];
        
        //if (d < BL_EPS)
        //    continue;
        
        BL_FLOAT amp = GetAmp(mPrevAmplitude, mAmplitude, t);


        // Resynth the correct volume!
        BL_FLOAT a = amp*s;

        //a *= SYNTH_AMP_COEFF; // ??
        a *= SYNTH_AMP_COEFF_RESYNTH;
        
        samples->Get()[i] = a;
        
        t += tStep;
    }
    
    mPrevAmplitude = mAmplitude;
    
    mPrevMorphoPartials = mMorphoPartials;
#endif
}

// Directly use partials provided, but also apply Morpho parameter to them
void
MorphoFrameSynth2::ComputeSamplesPartialsSource(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
    {
        return;
    }

    BL_FLOAT ampFactor = mMorphoFrame.GetAmpFactor();
    BL_FLOAT freqFactor = mMorphoFrame.GetFreqFactor();
    
    // Optim
    BL_FLOAT twoPiSR = 2.0*M_PI/mSampleRate;

    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT hzPerBinInv = 1.0/hzPerBin;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        int prevPartialIdx = mPartials[i].mLinkedId;

        BL_FLOAT phase = 0.0;
        if (prevPartialIdx != -1)
            phase = mPrevPartials[prevPartialIdx].mPhase;
        
        // Generate samples
        MorphoFrame7::MorphoPartial partial;
        int numSamples = samples->GetSize()/mOverlapping;
        for (int j = 0; j < numSamples; j++)
        {            
            // Get interpolated partial
            BL_FLOAT partialT = ((BL_FLOAT)j)/numSamples;
            GetPartial(&partial, i, partialT);
            
            BL_FLOAT binIdx = partial.mFreq*hzPerBinInv;
            
            // Apply Morpho parameters to current partial
            //
            
            // Apply warping
            BL_FLOAT w = GetWarping(mWarping, binIdx);
            partial.mFreq *= w;
            
            // Recompute bin idx
            binIdx = partial.mFreq*hzPerBinInv;
            
            // Apply color
            BL_FLOAT col = GetColor(mColor, binIdx);
            
            partial.mAmp *= col;

            // Amplitude
            partial.mAmp *= ampFactor;
                        
            // Freq factor (post)
            partial.mFreq *= freqFactor;
                
            //
            BL_FLOAT freq = partial.mFreq;
                
            BL_FLOAT amp = partial.mAmp;
            
            BL_FLOAT samp = amp*std::sin(phase);
            
            samp *= SYNTH_AMP_COEFF;
            
            if (freq >= SYNTH_MIN_FREQ)
                samples->Get()[j] += samp;
            
            phase += twoPiSR*freq;
        }
        
        mPartials[i].mPhase = phase;
    }
}

void
MorphoFrameSynth2::ComputeSamplesPartialsResynth(WDL_TypedBuf<BL_FLOAT> *samples)
{        
    BLUtils::FillAllZero(samples);
    
    // First time: initialize the partials
    if (mMorphoPartials.empty())
    {
        mMorphoPartials.resize(SYNTH_MAX_NUM_PARTIALS);
    }
    
    if (mPrevMorphoPartials.empty())
        mPrevMorphoPartials = mMorphoPartials;
    
    if (mPrevColor.GetSize() != mColor.GetSize())
        mPrevColor = mColor;
    
    if (mPrevWarping.GetSize() != mWarping.GetSize())
        mPrevWarping = mWarping;

    BL_FLOAT ampFactor = mMorphoFrame.GetAmpFactor();
    BL_FLOAT freqFactor = mMorphoFrame.GetFreqFactor();
    
    // For applying amplitude the correct way
    // Keep a denominator for each sample.
    //WDL_TypedBuf<BL_FLOAT> ampDenoms;
    //ampDenoms.Resize(samples->GetSize());
    //BLUtils::FillAllZero(&ampDenoms);
    
    // Optim
    BL_FLOAT phaseCoeff = 2.0*M_PI/mSampleRate;
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT hzPerBinInv = 1.0/hzPerBin;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT partialFreq = mFrequency*freqFactor;
    int partialIndex = 0;
    while((partialFreq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (partialIndex > 0)
        {
            if ((!mSynthEvenPartials) && (partialIndex % 2 == 0))
            {
                partialIndex++;
                partialFreq = mFrequency*freqFactor*(partialIndex + 1);
                
                continue;
            }

            if ((!mSynthOddPartials) && (partialIndex % 2 == 1))
            {
                partialIndex++;
                partialFreq = mFrequency*freqFactor*(partialIndex + 1);
                
                continue;
            }
        }
        
        if (partialFreq > SYNTH_MIN_FREQ)
        {
            // Current and prev partials
            MorphoFrame7::MorphoPartial &partial = mMorphoPartials[partialIndex];
            partial.mFreq = partialFreq;

            // Correct! (global amplitude will be applied later)
            partial.mAmp = 1.0;
            
            const MorphoFrame7::MorphoPartial &prevPartial = mPrevMorphoPartials[partialIndex];
            
            // Current phase
            BL_FLOAT phase = prevPartial.mPhase;
            
            // Bin param
            BL_FLOAT binIdx = partialFreq*hzPerBinInv;
            
            // Warping
            BL_FLOAT w0 = GetWarping(mPrevWarping, binIdx);
            BL_FLOAT w1 = GetWarping(mWarping, binIdx);
            
            // Color
            BL_FLOAT prevBinIdxc = w0*prevPartial.mFreq*hzPerBinInv;
            BL_FLOAT binIdxc = w1*partial.mFreq*hzPerBinInv;

            //
            BL_FLOAT col0 = GetColor(mPrevColor, prevBinIdxc);
            BL_FLOAT col1 = GetColor(mColor, binIdxc);
            
            if ((col0 < BL_EPS) && (col1 < BL_EPS))
            // Color will be 0, no need to synthetize samples
            {
                partialIndex++;
                partialFreq = mFrequency*freqFactor*(partialIndex + 1);
                
                // TODO: update phase!!
                
                continue;
            }
            
            // Loop
            //
            BL_FLOAT t = 0.0;
            BL_FLOAT tStep = 1.0/(samples->GetSize() - 1);
            for (int i = 0; i < samples->GetSize(); i++)
            {
                // Compute norm warping
                BL_FLOAT w = 1.0;
                if (binIdx < mWarping.GetSize() - 1)
                {
                    w = (1.0 - t)*w0 + t*w1;
                }
                
                // Freq
                BL_FLOAT freq = GetFreq(prevPartial.mFreq, partial.mFreq, t);
                
                // Warping
                freq *= w;
                
                // Color
                BL_FLOAT col = GetCol(col0, col1, t);

                // Sample
                
                // Not 100% perfect (partials les neat)
                //BL_FLOAT samp;
                //SIN_LUT_GET(Morpho_FRAME_SIN_LUT, samp, phase);
                
                // Better quality.
                // No "blurb" between frequencies
                BL_FLOAT samp = std::sin(phase);
                
                if (freq >= SYNTH_MIN_FREQ)
                {
                    // Generate samples only if not on an transient
                    samples->Get()[i] += samp*col;
                    //ampDenoms.Get()[i] += col;
                }
                
                t += tStep;
                phase += phaseCoeff*freq;
            }
            
            // Compute next phase
            mMorphoPartials[partialIndex].mPhase = phase;
        }
        
        partialIndex++;            
            
        partialFreq = mFrequency*freqFactor*(partialIndex + 1);
    }

    // At the end, apply amplitude
    //
    BL_FLOAT tStep = 1.0/(samples->GetSize() - 1);
    BL_FLOAT t = 0.0;
    for (int i = 0; i < samples->GetSize(); i++)
    {
        BL_FLOAT s = samples->Get()[i];
        //BL_FLOAT d = ampDenoms.Get()[i];
        
        //if (d < BL_EPS)
        //    continue;
        
        BL_FLOAT amp = GetAmp(mPrevAmplitude, mAmplitude, t);


        // Resynth the correct volume!
        BL_FLOAT a = amp*s;

        //a *= SYNTH_AMP_COEFF; // ??
        a *= SYNTH_AMP_COEFF_RESYNTH;
        
        samples->Get()[i] = a;
        
        t += tStep;
    }
    
    mPrevAmplitude = mAmplitude;
    
    mPrevMorphoPartials = mMorphoPartials;
}

BL_FLOAT
MorphoFrameSynth2::GetColor(const WDL_TypedBuf<BL_FLOAT> &color, BL_FLOAT binIdx)
{
#if NEAREST_INTERP
    // Quick method
    binIdx = bl_round(ninIdx);
    if (binIdx < color.GetSize() - 1)
    {
        BL_FLOAT col = color.Get()[(int)binIdx];
        
        return col;
    }
    return 0.0;
#endif
    
    BL_FLOAT col = 0.0;
    if (binIdx < color.GetSize() - 1)
    {
        BL_FLOAT t = binIdx - (int)binIdx;
        
        BL_FLOAT col0 = color.Get()[(int)binIdx];
        BL_FLOAT col1 = color.Get()[((int)binIdx) + 1];
        
        // Method 2: interpolate in dB
#if INTERP_RESCALE
        col0 = mScale->ApplyScale(Scale::DB, col0, mMinAmpDB, (BL_FLOAT)0.0);
        col1 = mScale->ApplyScale(Scale::DB, col1, mMinAmpDB, (BL_FLOAT)0.0);
#endif
        
        col = (1.0 - t)*col0 + t*col1;

#if INTERP_RESCALE
        col = mScale->ApplyScale(Scale::DB_INV, col, mMinAmpDB, (BL_FLOAT)0.0);
#endif
    }

#if 0 // No need anymore
    //col *= mColorFactor;
    col = ApplyColorFactor(col, mColorFactor);
#endif
    
    return col;
}

BL_FLOAT
MorphoFrameSynth2::GetWarping(const WDL_TypedBuf<BL_FLOAT> &warping,
                           BL_FLOAT binIdx)
{
#if NEAREST_INTERP
    BL_FLOAT warpingFactor = mMorphoFrame.GetWarpingFactor();
    
    // Quick method
    binIdx = bl_round(binIdx);
    if (binIdx < warping.GetSize() - 1)
    {
        BL_FLOAT w = warping.Get()[(int)binIdx];

        // Warping is center on 1
        w -= 1.0;
        w *= warpingFactor;
        w += 1.0;
        
        return w;
    }
    return 1.0;
#endif
    
    BL_FLOAT w = 0.0;
    if (binIdx < warping.GetSize() - 1)
    {
        BL_FLOAT t = binIdx - (int)binIdx;
        
        BL_FLOAT w0 = warping.Get()[(int)binIdx];
        BL_FLOAT w1 = warping.Get()[((int)binIdx) + 1];
        
        w = (1.0 - t)*w0 + t*w1;
    }

    // Warping is centered on 1
    // NOTE: no need anymore
    /*w -= 1.0;
      w *= mWarpingFactor;
      w += 1.0;
    */
    
    return w;
}

bool
MorphoFrameSynth2::FindPartial(BL_FLOAT freq)
{
#define FIND_COEFF 0.25
    
    BL_FLOAT step = mFrequency*FIND_COEFF;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        const MorphoFrame7::MorphoPartial &p = mPartials[i];
        
        if ((freq > p.mFreq - step) &&
            (freq < p.mFreq + step))
            // Found
            return true;
    }
    
    return false;
}

// Interpolate in amp
void
MorphoFrameSynth2::GetPartial(MorphoFrame7::MorphoPartial *result, int index, BL_FLOAT t)
{
    const MorphoFrame7::MorphoPartial &currentPartial = mPartials[index];

    int prevPartialIdx = currentPartial.mLinkedId;
        
    *result = currentPartial;
    
    // Manage decrease of dead partials
    //
    if ((currentPartial.mState == Partial2::DEAD) && currentPartial.mWasAlive)
    {
        // Decrease progressively the amplitude
        result->mAmp = 0.0;
        
        if (prevPartialIdx != -1)
            // Interpolate
        {
            BL_FLOAT t0 = t/SYNTH_DEAD_PARTIAL_DECREASE;
            if (t0 <= 1.0)
            {
                // Interpolate in amp
                const MorphoFrame7::MorphoPartial &prevPartial =
                    mPrevPartials[prevPartialIdx];
                
                BL_FLOAT amp = (1.0 - t0)*prevPartial.mAmp;
                result->mAmp = amp;
            }
        }
    }
    
    // Manage interpolation of freq and amp
    //
    if ((currentPartial.mState != Partial2::DEAD) && currentPartial.mWasAlive)
    {
        if (prevPartialIdx != -1)
        {
            const MorphoFrame7::MorphoPartial &prevPartial = mPrevPartials[prevPartialIdx];
                
            if (prevPartial.mState == Partial2::ALIVE)
            {
                result->mFreq = (1.0 - t)*prevPartial.mFreq + t*currentPartial.mFreq;
                
                BL_FLOAT amp = (1.0 - t)*prevPartial.mAmp + t*currentPartial.mAmp;
                result->mAmp = amp;
            }
        }
        else
        // New partial, fade in
        {
            // NOTE: this avoids vertical bars in the spectrogram when
            // a partial starts
                
            // Increase progressively the amplitude
                
            // Interpolate
            BL_FLOAT t0 = t/SYNTH_DEAD_PARTIAL_DECREASE;
            if (t0 > 1.0)
                t0 = 1.0;
            
            BL_FLOAT amp = t0*currentPartial.mAmp;
            result->mAmp = amp;
        }
    }
}

BL_FLOAT
MorphoFrameSynth2::GetFreq(BL_FLOAT freq0, BL_FLOAT freq1, BL_FLOAT t)
{
#if INTERP_RESCALE
    // Mel scale
    BL_FLOAT maxFreq = mSampleRate*0.5;
    
    freq0 = mScale->ApplyScale(Scale::MEL, freq0/maxFreq, (BL_FLOAT)0.0, maxFreq);
    freq1 = mScale->ApplyScale(Scale::MEL, freq1/maxFreq, (BL_FLOAT)0.0, maxFreq);
#endif
    
    BL_FLOAT freq = (1.0 - t)*freq0 + t*freq1;

#if INTERP_RESCALE
    freq = mScale->ApplyScale(Scale::MEL_INV, freq, (BL_FLOAT)0.0, maxFreq);    
    freq *= maxFreq;
#endif

    //freq *= mFreqFactor;
    
    return freq;
}

BL_FLOAT
MorphoFrameSynth2::GetAmp(BL_FLOAT amp0, BL_FLOAT amp1, BL_FLOAT t)
{
    BL_FLOAT ampFactor = mMorphoFrame.GetAmpFactor();
    
#if INTERP_RESCALE
    // If amp is not already in dB)
    
    amp0 = mScale->ApplyScale(Scale::DB, amp0, mMinAmpDB, (BL_FLOAT)0.0);
    amp1 = mScale->ApplyScale(Scale::DB, amp1, mMinAmpDB, (BL_FLOAT)0.0);
#endif
    
    BL_FLOAT amp = (1.0 - t)*amp0 + t*amp1;

#if INTERP_RESCALE
    amp = mScale->ApplyScale(Scale::DB_INV, amp, mMinAmpDB, (BL_FLOAT)0.0);
#endif

    amp *= ampFactor;
    
    return amp;
}

BL_FLOAT
MorphoFrameSynth2::GetCol(BL_FLOAT col0, BL_FLOAT col1, BL_FLOAT t)
{
#if INTERP_RESCALE
    // Method 2: (if col is not already in dB)
    col0 = mScale->ApplyScale(Scale::DB, col0, mMinAmpDB, (BL_FLOAT)0.0);
    col1 = mScale->ApplyScale(Scale::DB, col1, mMinAmpDB, (BL_FLOAT)0.0);
#endif
    
    BL_FLOAT col = (1.0 - t)*col0 + t*col1;

#if INTERP_RESCALE
    col = mScale->ApplyScale(Scale::DB_INV, col, mMinAmpDB, (BL_FLOAT)0.0);
#endif
    
    return col;
}

void
MorphoFrameSynth2::UpdateMorphoData()
{
    // For Morpho, frames can be provided not consecutive
    // (due to mix params, play speed etc.)
    // So need to recompute linked ids
    sort(mPartials.begin(), mPartials.end(), MorphoFrame7::MorphoPartial::IdLess);
    
    // Partials
    mPrevPartials = mPartials;
    mMorphoFrame.GetPartials(&mPartials);

    // Added for Morpho
    IdLinker<MorphoFrame7::MorphoPartial,
             MorphoFrame7::MorphoPartial>::LinkIds(&mPrevPartials, &mPartials, false);
    
    //
    mPrevColor = mColor;
    mMorphoFrame.GetColor(&mColor);

    mPrevWarping = mWarping;
    mMorphoFrame.GetWarping(&mWarping);

    mMorphoFrame.GetWarpingInv(&mWarpingInv);
}
