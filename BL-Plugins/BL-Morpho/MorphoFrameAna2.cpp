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
 
#include <MorphoFrame7.h>
#include <OnsetDetector.h>
#include <PartialsToFreq8.h>

#include <Window.h>

#include <PartialTracker7.h>

#include <BLUtils.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>

#include <IdLinker.h>

#include <MorphoFrameSynth2.h>

#include "MorphoFrameAna2.h"

#define MIN_NORM_AMP 1e-15

// GOOD: avoid many small zig-zags on the result lines
#define PROCESS_MUS_NOISE      1 // Working (but dos not remove all mus noise)
// 4 seems better than 2
#define HISTORY_SIZE_MUS_NOISE 4 //2 //4

// Use OnsetDetector to avoid generating garbage harmonics when a transient appears
#define ENABLE_ONSET_DETECTION 1
#define DETECT_TRANSIENTS_ONSET 0 //1
#define ONSET_THRESHOLD 0.94
//#define ONSET_VALUE_THRESHOLD 0.0012
//#define ONSET_VALUE_THRESHOLD 0.06
//#define ONSET_VALUE_THRESHOLD 0.001 // Works ("bowl": takes the two peaks)

// TODO: find a good method to get a threshold independent to amplitude etc.
//#define ONSET_VALUE_THRESHOLD 0.0025 // Works ("bowl": only take the main peak)
#define ONSET_VALUE_THRESHOLD 40.0 // New value that work

// Shift, so the onset peaks are in synch with mAmplitude 
#define ONSET_HISTORY_HACK 1
#define ONSET_HISTORY_HACK_SIZE 3

// NOTE: if we don't smooth the color, this will make clips
// (because if a partial is missing, this will make a hole in the color)
//
// Avoids jumps in envelope
#define COLOR_SMOOTH_COEFF 0.5
// WARPING 0.9 improves "bowl"
#define WARPING_SMOOTH_COEFF 0.0 //0.9 //0.5
// NOTE: for the moment, smooting freq in only for debugging
#define FREQ_SMOOTH_COEFF 0.0

// If a partial is missing, fill with zeroes around it
//
// At 0: low single partial => interpolated until the last frequencies
// Real signal => more smooth
//
// At 1: low single partial => interpolated locally, far frequencies set to 0
// Real signal: make some strange peaks at mid/high freqs
// Sine wave: very good, render only the analyzed input sine
//
//#define COLOR_CUT_MISSING_PARTIALS 1 // ORIGIN
#define COLOR_CUT_MISSING_PARTIALS 0 //1

// Origin: 1
#define FILL_ZERO_FIRST_LAST_VALUES_WARPING 1
#define FILL_ZERO_FIRST_LAST_VALUES_COLOR 0 //1

// Limit the maximum values that the detected warping can take
#define LIMIT_WARPING_MAX 1

// Smooth interpolation of warping envelope
#define WARP_ENVELOPE_USE_LAGRANGE_INTERP 1

// NOTE: Lagrange interp seems not good for color
// => it makes holes in "bowl" example, one the signal is converted
// in dB for displaying, and if adding additional values before
// interpolation this gives the same result as simple
// interpolation (no Lagrange)

// Smooth interpolation of color envelope?
#define COLOR_ENVELOPE_USE_LAGRANGE_INTERP 0 //1
#define LAGRANGE_MIN_NUM_COLOR_VALUES 16

MorphoFrameAna2::MorphoFrameAna2(int bufferSize, int oversampling,
                                 int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mOverlapping = oversampling;
    mFreqRes = freqRes;
    mSampleRate = sampleRate;
    
    mTimeSmoothNoiseCoeff = 0.5;

    mPrevFrequency = -1.0;

    mPartialsToFreq = new PartialsToFreq8(bufferSize, oversampling,
                                          freqRes, sampleRate);
    
    mOnsetDetector = NULL;
#if ENABLE_ONSET_DETECTION
    mOnsetDetector = new OnsetDetector();
    mOnsetDetector->SetThreshold(ONSET_THRESHOLD);
#endif
}

MorphoFrameAna2::~MorphoFrameAna2()
{
    delete mPartialsToFreq;
    
#if ENABLE_ONSET_DETECTION
    delete mOnsetDetector;
#endif
}

void
MorphoFrameAna2::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    Reset();
}
    
void
MorphoFrameAna2::Reset(int bufferSize, int oversampling,
                       int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mOverlapping = oversampling;
    mFreqRes = freqRes;
    mSampleRate = sampleRate;

    Reset();
}

void
MorphoFrameAna2::Reset()
{
    mInputMagns.Resize(0);
    mInputPhases.Resize(0);
    
    mSmoothWinNoise.Resize(0);

    mPrevNoiseEnvelope.Resize(0);
    // For ComputeMusicalNoise()
    mPrevNoiseMasks.unfreeze();
    mPrevNoiseMasks.clear();

    mPrevFrequency = -1.0;

    mPartialsToFreq->Reset(mBufferSize, mOverlapping, mFreqRes, mSampleRate);
    
#if ONSET_HISTORY_HACK
    mInputMagnsHistory.clear();
#endif
}

void
MorphoFrameAna2::SetTimeSmoothNoiseCoeff(BL_FLOAT coeff)
{
    mTimeSmoothNoiseCoeff = coeff;
}

void
MorphoFrameAna2::SetInputData(const WDL_TypedBuf<BL_FLOAT> &magns,
                              const WDL_TypedBuf<BL_FLOAT> &phases)
{
    mInputMagns = magns;
    mInputPhases = phases;

#if ONSET_HISTORY_HACK
    mInputMagnsHistory.push_back(mInputMagns);
    if (mInputMagnsHistory.size() > ONSET_HISTORY_HACK_SIZE)
        mInputMagnsHistory.pop_front();
#endif
}

void
MorphoFrameAna2::SetProcessedData(const WDL_TypedBuf<BL_FLOAT> &magns,
                                  const WDL_TypedBuf<BL_FLOAT> &phases)
{
    mProcessedMagns = magns;
    mProcessedPhases = phases;
}

void
MorphoFrameAna2::SetRawPartials(const vector<Partial2> &partials)
{
    mRawPartials = partials;
}

void
MorphoFrameAna2::SetPartials(const vector<Partial2> &partials)
{
    mPrevPartials = mPartials;
    mPartials = partials;

    // Should not be necessary
    //sort(mPrevPartials.begin(), mPrevPartials.end(), Partial2::IdLess);
    
    sort(mPartials.begin(), mPartials.end(), Partial2::IdLess);

    //LinkPartialsIdx(&mPrevPartials, &mPartials);
    IdLinker<Partial2, Partial2>::LinkIds(&mPrevPartials, &mPartials, false);
}

void
MorphoFrameAna2::Compute(MorphoFrame7 *frame)
{
    WDL_TypedBuf<BL_FLOAT> &noise = mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> &harmo = mTmpBuf8;
    ComputeNoiseEnvelope(&noise, &harmo);
    
    BL_FLOAT amp = ComputeAmplitude();
    BL_FLOAT freq = ComputeFrequency(mInputMagns);
    //BL_FLOAT freq = ComputeFrequency(harmo);

    WDL_TypedBuf<BL_FLOAT> &color = mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> &colorNorm = mTmpBuf7;
    ComputeColor(&color, &colorNorm, freq);

    WDL_TypedBuf<BL_FLOAT> &warping = mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> &warpingInv = mTmpBuf6;
    ComputeWarping(&warping, &warpingInv, freq);

    bool onsetDetected = ComputeOnset();

    // NOTE: not working, make artifacts (oscillations)
    // Cancel warping and color
    // To be used with MorphoFrameSynth::ComputeSamplesPartialsSource() instead of
    // MorphoFrameSynth::ComputeSamplesPartialsSourceNorm()
    //NormalizePartials(warpingInv, color);
    
    // TODO: denorm partials, for "source" synthesis
    
    //
    frame->SetNoiseEnvelope(noise);
    frame->SetHarmoEnvelope(harmo); // For displaying only
    
    frame->SetAmplitude(amp);
    frame->SetFrequency(freq);
    
    frame->SetColor(colorNorm);
    frame->SetColorRaw(color); // For displaying only

    frame->SetWarping(warping);
    frame->SetWarpingInv(warpingInv);

    frame->SetPartials(mPartials);

    frame->SetOnsetDetected(onsetDetected);
}

void
MorphoFrameAna2::ComputeNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv,
                                      WDL_TypedBuf<BL_FLOAT> *harmoEnv)
{
    vector<Partial2> &partials = mTmpBuf4;
    //partials.resize(0);
    partials = mRawPartials;
    
    // Get all partials, or only alive ?
    //partials = mResult; // Not so good, makes noise peaks
    
    // NOTE: Must get the alive partials only,
    // otherwise we would get additional "garbage" partials,
    // that would corrupt the partial rectangle
    // and then compute incorrect noise peaks
    // (the state must be ALIVE, and not mWasAlive !)
    GetAlivePartials(&partials);

    WDL_TypedBuf<BL_FLOAT> &harmonicEnvelope = mTmpBuf0;
    //harmonicEnvelope = mCurrentMagns;
    //harmonicEnvelope = mInputMagns;
    harmonicEnvelope = mProcessedMagns;
    
    // Just in case
    for (int i = 0; i < DETECT_PARTIALS_START_INDEX; i++)
        harmonicEnvelope.Get()[i] = MIN_NORM_AMP;
    
    KeepOnlyPartials(partials, &harmonicEnvelope);

    *harmoEnv = harmonicEnvelope;
    
    // Compute harmonic envelope
    // (origin signal less noise)
    //*noiseEnv = mCurrentMagns;
    //*noiseEnv = mInputMagns;
    *noiseEnv = mProcessedMagns;
    
    BLUtils::SubstractValues(noiseEnv, harmonicEnvelope);
    
    // Because it is in dB
    BLUtils::AddValues(noiseEnv, (BL_FLOAT)0.0);
    
    BLUtils::ClipMin(noiseEnv, (BL_FLOAT)0.0);
    
    // Avoids interpolation from 0 to the first valid index
    // (could have made an artificial increasing slope in the low freqs)
    for (int i = 0; i < noiseEnv->GetSize(); i++)
    {
        BL_FLOAT val = noiseEnv->Get()[i];
        if (val > BL_EPS)
            // First value
        {
            int prevIdx = i - 1;
            if (prevIdx > 0)
            {
                noiseEnv->Get()[prevIdx] = BL_EPS;
            }
            
            break;
        }
    }
    
#if PROCESS_MUS_NOISE
    // Works well, but do not remove all musical noise
    ProcessMusicalNoise(noiseEnv);
#endif
    
    // Create an envelope
    // NOTE: good for "oohoo", not good for "alphabet A"
    BLUtils::FillMissingValues2(noiseEnv, false, (BL_FLOAT)0.0);

    // TimeSmooth
    BLUtils::Smooth(noiseEnv, &mPrevNoiseEnvelope, mTimeSmoothNoiseCoeff);
}

BL_FLOAT
MorphoFrameAna2::ComputeAmplitude()
{
    BL_FLOAT amplitude = 0.0;
       
    const vector<Partial2> &partials = mPartials;
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &p = partials[i];
        
        BL_FLOAT amp = p.mAmp;
        amplitude += amp;
    }
    
    return amplitude;
}

BL_FLOAT
MorphoFrameAna2::ComputeFrequency(const WDL_TypedBuf<BL_FLOAT> &magns)
{
    BL_FLOAT freq =
        mPartialsToFreq->ComputeFrequency(magns, mInputPhases, mPartials);
        
    // Smooth
    if (mPrevFrequency < 0.0)
        mPrevFrequency = freq;
    else
        BLUtils::Smooth(&freq, &mPrevFrequency, (BL_FLOAT)FREQ_SMOOTH_COEFF);

    return freq;
}

void
MorphoFrameAna2::ComputeColor(WDL_TypedBuf<BL_FLOAT> *color,
                              WDL_TypedBuf<BL_FLOAT> *colorNorm,
                              BL_FLOAT freq)
{
    ComputeColorAux(color, freq);
    
    BLUtils::Smooth(color, &mPrevColor, (BL_FLOAT)COLOR_SMOOTH_COEFF);

    // Normalize the color
    
    // Fixed version => so the color max is 1
    // Normalize the color
    *colorNorm = *color;
    BL_FLOAT maxCol = BLUtils::ComputeMax(*colorNorm);
    if (maxCol > BL_EPS)
        BLUtils::MultValues(colorNorm, (BL_FLOAT)1.0/maxCol);
}

void
MorphoFrameAna2::ComputeColorAux(WDL_TypedBuf<BL_FLOAT> *color, BL_FLOAT freq)
{
    BL_FLOAT freq0 = freq;
    
    BL_FLOAT minColorValue = 0.0;
    BL_FLOAT undefinedValue = -1.0; // -300dB
    
    color->Resize(mBufferSize/2);
    
    if (freq < BL_EPS)
    {
        BLUtils::FillAllValue(color, minColorValue);
        
        return;
    }
    
    // Will interpolate values between the partials
    BLUtils::FillAllValue(color, undefinedValue);
    
    // Fix bounds at 0
    color->Get()[0] = minColorValue;
    color->Get()[mBufferSize/2 - 1] = minColorValue;
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    // Put the values we have
    for (int i = 0; i < mPartials.size(); i++)
    {
        const Partial2 &p = mPartials[i];
 
        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != Partial2::ALIVE)
            continue;
        
        BL_FLOAT idx = p.mFreq/hzPerBin;
        
        // TODO: make an interpolation, it is not so good to align to bins
        idx = bl_round(idx);
        
        BL_FLOAT amp = p.mAmp;
        
        if (((int)idx >= 0) && ((int)idx < color->GetSize()))
            color->Get()[(int)idx] = amp;

        // TEST: with this and sines example, the first partial has the right gain
        // Without this, the first synth partial amp is lower 
        if (i == 0)
            color->Get()[0] = amp;
    }
    
#if COLOR_CUT_MISSING_PARTIALS
    // Put some zeros when partials are missing
    while(freq < mSampleRate/2.0)
    {
        if (!FindPartial(freq))
        {
            BL_FLOAT idx = freq/hzPerBin;
            if (idx >= color->GetSize())
                break;
            
            color->Get()[(int)idx] = minColorValue;
        }
        
        freq += freq0;
    }
#endif

#if FILL_ZERO_FIRST_LAST_VALUES_COLOR
    // Avoid interpolating to the last partial value to 0
    // Would make color where there is no sound otherwise
    // (e.g example with just some sine waves is false)
    FillLastValues(color, mPartials, minColorValue);
#endif
    
    // Fill al the other value
    bool extendBounds = false;
#if !COLOR_ENVELOPE_USE_LAGRANGE_INTERP
    BLUtils::FillMissingValues(color, extendBounds, undefinedValue);
#else
    // Try to add intermediate values, to avoid too many oscillations
    // (not working, this leads to the same result as linear (no Lagrange)
    BLUtils::AddIntermediateValues(color, LAGRANGE_MIN_NUM_COLOR_VALUES,
                                   undefinedValue);
    BLUtils::FillMissingValuesLagrange(color, extendBounds, undefinedValue);
    // Lagrange oscillations can make the values to become negative sometimes
    BLUtils::ClipMin(color, 0.0);
#endif
}

void
MorphoFrameAna2::ComputeWarping(WDL_TypedBuf<BL_FLOAT> *warping,
                                WDL_TypedBuf<BL_FLOAT> *warpingInv,
                                BL_FLOAT freq)
{
    // Normal warping
    //    
    ComputeWarpingAux(warping, freq);                      
    BLUtils::Smooth(warping, &mPrevWarping, (BL_FLOAT)WARPING_SMOOTH_COEFF);
        
    // Inverse warping
    //
    ComputeWarpingAux(warpingInv, freq, true); 
    BLUtils::Smooth(warpingInv, &mPrevWarpingInv, (BL_FLOAT)WARPING_SMOOTH_COEFF);
}

bool
MorphoFrameAna2::ComputeOnset()
{
    bool onsetDetected = false;
#if ENABLE_ONSET_DETECTION
#if !ONSET_HISTORY_HACK
    mOnsetDetector->Detect(mInputMagns); // Origin
#else
    mOnsetDetector->Detect(mInputMagnsHistory[0]);
#endif
    
    BL_FLOAT onsetValue = mOnsetDetector->GetCurrentOnsetValue();

#if 0 //1
    BLDebug::AppendValue("onset.txt", onsetValue);
#endif
    
#if DETECT_TRANSIENTS_ONSET
    onsetDetected = (onsetValue > ONSET_VALUE_THRESHOLD);
#endif
    
#endif

    return onsetDetected;
}

void
MorphoFrameAna2::NormalizePartials(const WDL_TypedBuf<BL_FLOAT> &warpingInv,
                                   const WDL_TypedBuf<BL_FLOAT> &color)
{
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT hzPerBinInv = 1.0/hzPerBin;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        Partial2 &p = mPartials[i];

        BL_FLOAT binIdx = p.mFreq*hzPerBinInv;

        // NOTE: should invert color or warping first?
        
        // Normalize amp
        BL_FLOAT c0 = BLUtils::GetLinerp(color, binIdx);
        p.mAmp /= c0;

        // Normalize warping
        BL_FLOAT w0 = BLUtils::GetLinerp(warpingInv, binIdx);
        p.mFreq *= w0;
        
        // Recompute bin idx
        //binIdx = p.mFreq*hzPerBinInv;
        //BL_FLOAT c0 = BLUtils::GetLinerp(color, binIdx);
        //p.mAmp /= c0;
    }
}

// Problem: when incorrect partials are briefly detected, they affect warping a lot
// Solution: take each theorical synth partials, and find the closest detected
// partial to compute the norma warping (and ignore other partials)
// => a lot more robust for low thresholds, when we have many partials
//
// NOTE: this is not still perfect if we briefly loose the tracking
void
MorphoFrameAna2::ComputeWarpingAux(WDL_TypedBuf<BL_FLOAT> *warping,
                                   BL_FLOAT freq, bool inverse)
{
#define MIN_WARPING_VAL 0.8
#define MAX_WARPING_VAL 1.25
    
    // Init

    int maxNumPartials = SYNTH_MAX_NUM_PARTIALS;
    //BL_FLOAT minFreq = 20.0;
    //int maxNumPartials = mSampleRate*0.5/minFreq;
    
    vector<PartialAux> theoricalPartials;
    theoricalPartials.resize(maxNumPartials);
    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        theoricalPartials[i].mFreq = (i + 1)*freq;
        theoricalPartials[i].mWarping = -1.0;
    }
    
    // Compute best match
    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        PartialAux &pa = theoricalPartials[i];
        
        for (int j = 0; j < mPartials.size(); j++)
        {
            const Partial2 &p = mPartials[j];
            
            // Do no add to warping if dead or zombie
            if (p.mState != Partial2::ALIVE)
                continue;

            // GOOD!
            // mFrequency is the step...
            // Take half the step! Over half the step,
            // this is another theorical partial which will be interesting
            if (p.mFreq < pa.mFreq - freq*0.5)
                continue;
            if (p.mFreq > pa.mFreq + freq*0.5)
                // Can not break, because now partials can be sorted by id
                // instead of by freq
                //break;
                continue;
            
            BL_FLOAT w = p.mFreq/pa.mFreq;
            
#if LIMIT_WARPING_MAX
            // Discard too high warping values,
            // that can come to short invalid partials
            // spread randomly
            if ((w < MIN_WARPING_VAL) || (w > MAX_WARPING_VAL))
                continue;
#endif

            // Take the smallest warping
            // (i.e the closest partial)
            if (pa.mWarping < 0.0)
                pa.mWarping = w;
            else
            {
                // Compute the ral "smallest" warping value
                BL_FLOAT wNorm = (w < 1.0) ? 1.0/w : w;
                BL_FLOAT paNorm = (pa.mWarping < 1.0) ? 1.0/pa.mWarping : pa.mWarping;
                
                // Keep smallest warping
                if (wNorm < paNorm)
                    pa.mWarping = w;
            }
        }
    }

    // Fill missing partials values
    // (in case no matching detected partial was found for a given theorical partial)
    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        PartialAux &pa = theoricalPartials[i];

        if (pa.mWarping < 0.0)
            // Not defined
        {
            BL_FLOAT leftWarp = -1.0;
            int leftIdx = -1;
            for (int j = i - 1; j >= 0; j--)
            {
                const PartialAux &paL = theoricalPartials[j];
                if (paL.mWarping > 0.0)
                {
                    leftWarp = paL.mWarping;
                    leftIdx = j;
                    break;
                }
            }

            BL_FLOAT rightWarp = -1.0;
            int rightIdx = -1;
            for (int j = i + 1; j < theoricalPartials.size(); j++)
            {
                const PartialAux &paR = theoricalPartials[j];
                if (paR.mWarping > 0.0)
                {
                    rightWarp = paR.mWarping;
                    rightIdx = j;
                    break;
                }
            }

            if ((leftWarp > 0.0) && (rightWarp > 0.0))
            {
                BL_FLOAT t = (i - leftIdx)/(rightIdx - leftIdx);
                BL_FLOAT w = (1.0 - t)*leftWarp + t*rightWarp;

                pa.mWarping = w;
            }
        }
    }
    
    // Fill the warping envelope
    //
    
    warping->Resize(mBufferSize/2);
    
    if (freq < BL_EPS)
    {
        BLUtils::FillAllValue(warping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Will interpolate values between the partials
    BL_FLOAT undefinedValue = -1.0;
    BLUtils::FillAllValue(warping, undefinedValue);
    
    // Fix bounds at 1
    warping->Get()[0] = 1.0;
    warping->Get()[mBufferSize/2 - 1] = 1.0;
    
    if (mPartials.size() < 2)
    {
        BLUtils::FillAllValue(warping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Fundamental frequency
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;

    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        PartialAux &pa = theoricalPartials[i];
        
        BL_FLOAT idx = pa.mFreq/hzPerBin;
        BL_FLOAT w = pa.mWarping;

        // Fix missing values in the thorical partials here
        if (w < 0.0)
            w = 1.0;
        
        if (inverse)
        {
            if (w > BL_EPS)
            {
                idx *= w;
            
                w = 1.0/w;
            }
        }
        
        // TODO: make an interpolation ?
        idx = bl_round(idx);
        
        if ((idx > 0) && (idx < warping->GetSize()))
            warping->Get()[(int)idx] = w;
    }

    // Do not do this, no need because the undefined theorical partials
    // will now have a warping value of 1 assigned 
#if FILL_ZERO_FIRST_LAST_VALUES_WARPING
    // Keep the first partial warping of reference is chroma-compute freq
    // Avoid warping the first partial
    //FillFirstValues(warping, mPartials, 1.0);
    
    // NEW
    FillLastValues(warping, mPartials, 1.0);
#endif

    // Fill all the other value
    bool extendBounds = false;
#if !WARP_ENVELOPE_USE_LAGRANGE_INTERP
    BLUtils::FillMissingValues(warping, extendBounds, undefinedValue);
#else
    BLUtils::FillMissingValuesLagrange(warping, extendBounds, undefinedValue);

    // Sometimes Lagrange can make very big oscillations
    // like negative warping, leading later to negative bin index, making crash
    BLUtils::ClipMinMax(warping,
                        (BL_FLOAT)MIN_WARPING_VAL, (BL_FLOAT)MAX_WARPING_VAL);
#endif
}

// Supress musical noise in the raw noise (not filled)
void
MorphoFrameAna2::ProcessMusicalNoise(WDL_TypedBuf<BL_FLOAT> *noise)
{
    // Must choose bigger value than 1e-15
    // (otherwise the threshold won't work)
#define MUS_NOISE_EPS 1e-8
    
    // Better with an history
    // => suppress only spots that are in zone where partials are erased
    
    // If history is small => remove more spots (but unwanted ones)
    // If history too big => keep some spots that should have been erased
    if (mPrevNoiseMasks.size() < HISTORY_SIZE_MUS_NOISE)
    {
        mPrevNoiseMasks.push_back(*noise);

        if (mPrevNoiseMasks.size() == HISTORY_SIZE_MUS_NOISE)
            mPrevNoiseMasks.freeze();

        //if (mPrevNoiseMasks.size() > HISTORY_SIZE_MUS_NOISE)
        //    mPrevNoiseMasks.pop_front();
        
        return;
    }
    
    WDL_TypedBuf<BL_FLOAT> &noiseCopy = mTmpBuf2;
    noiseCopy = *noise;
    
    // Search for begin of first isle: values with zero borders
    //
    int startIdx = 0;
    while (startIdx < noise->GetSize())
    {
        BL_FLOAT val = noise->Get()[startIdx];
        if (val < MUS_NOISE_EPS)
            // Zero
        {
            break;
        }
        
        startIdx++;
    }
    
    // Loop to search for isles
    //
    while(startIdx < noise->GetSize())
    {
        // Find "isles" in the current noise
        int startIdxIsle = startIdx;
        while (startIdxIsle < noise->GetSize())
        {
            BL_FLOAT val = noise->Get()[startIdxIsle];
            if (val > MUS_NOISE_EPS)
                // One
            {
                // Start of isle found
                break;
            }
            
            startIdxIsle++;
        }
        
        // Search for isles: values with zero borders
        int endIdxIsle = startIdxIsle;
        while (endIdxIsle < noise->GetSize())
        {
            BL_FLOAT val = noise->Get()[endIdxIsle];
            if (val < MUS_NOISE_EPS)
                // Zero
            {
                // End of isle found
                
                // Adjust the index to the last zero value
                if (endIdxIsle > startIdxIsle)
                    endIdxIsle--;
                
                break;
            }
            
            endIdxIsle++;
        }
        
        // Check that the prev mask is all zero
        // at in front of the isle
        bool prevMaskZero = true;
        for (int i = 0; i < mPrevNoiseMasks.size(); i++)
        {
            const WDL_TypedBuf<BL_FLOAT> &mask = mPrevNoiseMasks[i];
            
            for (int j = startIdxIsle; j <= endIdxIsle; j++)
            {
                if (j >= mask.GetSize())
                    break;
                
                BL_FLOAT prevVal = mask.Get()[j];
                if (prevVal > MUS_NOISE_EPS)
                {
                    prevMaskZero = false;
                
                    break;
                }
            }
            
            if (!prevMaskZero)
                break;
        }
        
        if (prevMaskZero)
            // We have a real isle
        {
            // TODO: check isle size ?
            
            // Earse the isle
            for (int i = startIdxIsle; i <= endIdxIsle; i++)
            {
                if (i >= noise->GetSize())
                    break;
                
                noise->Get()[i] = 0.0;
            }
        }
        
        startIdx = endIdxIsle + 1;
    }
    
    // Fill the history at the end
    // (to avoid having processing the current noise as history
    //mPrevNoiseMasks.push_back(noiseCopy);
    //if (mPrevNoiseMasks.size() > HISTORY_SIZE_MUS_NOISE)
    //    mPrevNoiseMasks.pop_front();
    mPrevNoiseMasks.push_pop(noiseCopy);
}

void
MorphoFrameAna2::SmoothNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noise)
{
#define NOISE_SMOOTH_WIN_SIZE 27 //9
    
    // Get a smoothed version of the magns
    if (mSmoothWinNoise.GetSize() != NOISE_SMOOTH_WIN_SIZE)
    {
        //Window::MakeHanning(NOISE_SMOOTH_WIN_SIZE, &mSmoothWinNoise);
        
        // Works well too
        //
        // See: https://en.wikipedia.org/wiki/Window_function
        //
        BL_FLOAT sigma = 0.1;
        Window::MakeGaussian2(NOISE_SMOOTH_WIN_SIZE, sigma, &mSmoothWinNoise);
    }
    
    WDL_TypedBuf<BL_FLOAT> &smoothNoise = mTmpBuf3;
    BLUtils::SmoothDataWin(&smoothNoise, *noise, mSmoothWinNoise);
    
    *noise = smoothNoise;
}

void
MorphoFrameAna2::FillLastValues(WDL_TypedBuf<BL_FLOAT> *values,
                                const vector<Partial2> &partials, BL_FLOAT val)
{    
    // First, find the last bin index
    int maxIdx = -1;
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &p = partials[i];
        
        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != Partial2::ALIVE)
            continue;
            
        //if (p.mRightIndex > maxIdx)
        //    maxIdx = p.mRightIndex;
        if (p.mPeakIndex > maxIdx)
            maxIdx = p.mPeakIndex;
    }
    
    // Then fill with zeros after this index
    if (maxIdx > 0)
    {
        for (int i = maxIdx + 1; i < values->GetSize(); i++)
        {
            values->Get()[i] = val;
        }
    }
}

void
MorphoFrameAna2::FillFirstValues(WDL_TypedBuf<BL_FLOAT> *values,
                                 const vector<Partial2> &partials, BL_FLOAT val)
{
    // First, find the last bin idex
    int minIdx = values->GetSize() - 1;
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &p = partials[i];

        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != Partial2::ALIVE)
            continue;
        
        //if (p.mLeftIndex < minIdx)
        //    minIdx = p.mLeftIndex;
        if (p.mRightIndex < minIdx)
            minIdx = p.mRightIndex;
    }
    
    // Then fill with zeros after this index
    if (minIdx < values->GetSize() - 1)
    {
        for (int i = 0; i <= minIdx; i++)
        {
            values->Get()[i] = val;
        }
    }            
}

// For noise envelope extraction, the
// state must be ALIVE, and not mWasAlive
bool
MorphoFrameAna2::GetAlivePartials(vector<Partial2> *partials)
{
    if (mPartials.empty())
        return false;
    
    partials->clear();
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        const Partial2 &p = mPartials[i];
        if (p.mState == Partial2::ALIVE)
        {
            partials->push_back(p);
        }
    }
    
    return true;
}

void
MorphoFrameAna2::KeepOnlyPartials(const vector<Partial2> &partials,
                                  WDL_TypedBuf<BL_FLOAT> *magns)
{
    WDL_TypedBuf<BL_FLOAT> &result = mTmpBuf5;
    result.Resize(magns->GetSize());
    BLUtils::FillAllZero(&result);
                   
    for (int i = 0; i < partials.size(); i++)
    {
        const Partial2 &partial = partials[i];
        
        int minIdx = partial.mLeftIndex;
        if (minIdx >= magns->GetSize())
            continue;
        
        int maxIdx = partial.mRightIndex;
        if (maxIdx >= magns->GetSize())
            continue;
        
        for (int j = minIdx; j <= maxIdx; j++)
        {
            BL_FLOAT val = magns->Get()[j];
            result.Get()[j] = val;
        }
    }
                           
    *magns = result;
}
