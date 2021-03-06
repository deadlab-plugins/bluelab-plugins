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
//  SASFrame4.cpp
//  BL-SASViewer
//
//  Created by applematuer on 2/2/19.
//
//

#include <SASViewerProcess.h>

//#include <PartialsToFreqCepstrum.h>
//#include <PartialsToFreq5.h>
#include <PartialsToFreq6.h>

#include <FreqAdjustObj3.h>

#include <WavetableSynth.h>

#include <FftProcessObj16.h>

// Do not speed up...
#include <SinLUT.h>

#include <BLUtils.h>
#include <BLUtilsMath.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include <BLDebug.h>

#include <Scale.h>

#include <OnsetDetector.h>

#include "SASFrame4.h"


// Optim: optimizes a little (~5/15%)
SIN_LUT_CREATE(SAS_FRAME_SIN_LUT, 4096);

// 4096*64 (2MB)
// 2x slower than 4096
//SIN_LUT_CREATE(SAS_FRAME_SIN_LUT, 262144);

// Synthesis
#define SYNTH_MAX_NUM_PARTIALS 40 //20 //10 origin: 10, (40 for test bell)
#define SYNTH_AMP_COEFF 4.0
#define SYNTH_MIN_FREQ 50.0
#define SYNTH_DEAD_PARTIAL_DECREASE 0.25

// NOTE: if we don't smooth the color, this will make clips
// (because if a partial is missing, this will make a hole in the color)
//
// Avoids jumps in envelope
#define COLOR_SMOOTH_COEFF 0.5
// WARPING 0.9 improves "bowl"
#define WARPING_SMOOTH_COEFF 0.0 //0.9 //0.5
// NOTE: for the moment, smooting freq in only for debugging
#define FREQ_SMOOTH_COEFF 0.0

// Compute samples directly from tracked partials
#define COMPUTE_SAS_SAMPLES          1 // GOOD (best quality, but slow)
#define COMPUTE_SAS_FFT              0 // Perf gain x10 compared to COMPUTE_SAS_SAMPLES
#define COMPUTE_SAS_FFT_FREQ_ADJUST  1 // GOOD: Avoids a "reverb" phase effet as with COMPUTE_SAS_FFT
#define COMPUTE_SAS_SAMPLES_TABLE    0 // Perf gain ~x10 compared to COMPUTE_SAS_SAMPLES (with nearest + block buffers)
#define COMPUTE_SAS_SAMPLES_OVERLAP  0 // not very efficient (and sound is not good

#define DBG_DISABLE_WARPING 0 //1

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

// Interpolate partials to color in DB
// Better, for example with a single sine wave, at low freq
#define COLOR_DB_INTERP 1 // ??

// Set to 0 for optimization
//
// Origin: interpolate everything in db or mel scale
//
// Optim: this looks to work well when set to 0
// Perfs: 26% => 17%CPU
#define INTERP_RESCALE 0 // Origin: 1

// Origin: 1
#define FILL_ZERO_FIRST_LAST_VALUES 1

// Use OnsetDetector to avoid generating garbage harmonics when a transient appears
#define ENABLE_ONSET_DETECTION 1
#define DETECT_TRANSIENTS_ONSET 0 //1
#define ONSET_THRESHOLD 0.94
//#define ONSET_VALUE_THRESHOLD 0.0012
//#define ONSET_VALUE_THRESHOLD 0.06
//#define ONSET_VALUE_THRESHOLD 0.001 // Works ("bowl": takes the two peaks)
#define ONSET_VALUE_THRESHOLD 0.0025 // Works ("bowl": only take the main peak)

// Shift, so the onset peaks are in synch with mAmplitude 
#define ONSET_HISTORY_HACK 1
#define ONSET_HISTORY_HACK_SIZE 3

// Limit the maximum values that the detected warping can take
#define LIMIT_WARPING_MAX 1

#define DEBUG_DUMP_VALUES 0 //1

SASFrame4::SASPartial::SASPartial()
{
    mFreq = 0.0;
    mAmp = 0.0;
    mPhase = 0.0;
}

SASFrame4::SASPartial::SASPartial(const SASPartial &other)
{
    mFreq = other.mFreq;
    mAmp = other.mAmp;
    mPhase = other.mPhase;
}

SASFrame4::SASPartial::~SASPartial() {}

bool
SASFrame4::SASPartial::AmpLess(const SASPartial &p1, const SASPartial &p2)
{
    return (p1.mAmp < p2.mAmp);
}

SASFrame4::SASFrame4(int bufferSize, BL_FLOAT sampleRate, int overlapping)
{
    SIN_LUT_INIT(SAS_FRAME_SIN_LUT);
    
    mBufferSize = bufferSize;
    
    mSampleRate = sampleRate;
    mOverlapping = overlapping;
    
    mSynthMode = OSC;
    
    mAmplitude = 0.0;
    mPrevAmplitude = 0.0;
    
    mFrequency = 0.0;
    mPrevFrequency = -1.0;
    
    mAmpFactor = 1.0;
    mFreqFactor = 1.0;
    mColorFactor = 1.0;
    mWarpingFactor = 1.0;

    mScale = new Scale();
    
    mPartialsToFreq = new PartialsToFreq6(bufferSize, overlapping, 1, sampleRate);
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    int oversampling = 1;
    mFreqObj = new FreqAdjustObj3(bufferSize,
                                  overlapping, oversampling,
                                  sampleRate);
#endif

    mTableSynth = NULL;
#if COMPUTE_SAS_SAMPLES_TABLE
    BL_FLOAT minFreq = 20.0;
    mTableSynth = new WavetableSynth(bufferSize, overlapping,
                                     sampleRate, 1, minFreq);
#endif
    
    mMinAmpDB = -120.0;

    mSynthEvenPartials = true;
    mSynthOddPartials = true;

    mOnsetDetector = NULL;
#if ENABLE_ONSET_DETECTION
    mOnsetDetector = new OnsetDetector();
    mOnsetDetector->SetThreshold(ONSET_THRESHOLD);
#endif

#if DEBUG_DUMP_VALUES

    BLDebug::ResetFile("magns.txt");
    BLDebug::ResetFile("noise.txt");
    BLDebug::ResetFile("color.txt");
    BLDebug::ResetFile("warping.txt");
    
#if ENABLE_ONSET_DETECTION
    BLDebug::ResetFile("onset.txt");
#endif

    BLDebug::ResetFile("freq.txt");
    BLDebug::ResetFile("amp.txt");
    BLDebug::ResetFile("nump.txt");

    BLDebug::ResetFile("color-factor.txt");
#endif
}

SASFrame4::~SASFrame4()
{
    delete mPartialsToFreq;
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    delete mFreqObj;
#endif
    
#if COMPUTE_SAS_SAMPLES_TABLE
    delete mTableSynth;
#endif

    delete mScale;

#if ENABLE_ONSET_DETECTION
    delete mOnsetDetector;
#endif
}

void
SASFrame4::Reset(BL_FLOAT sampleRate)
{
    mPartials.clear();
    mPrevPartials.clear();
    
    mAmplitude = 0.0;
    
    mFrequency = 0.0;
    mPrevFrequency = -1.0;
    
    mSASPartials.clear();
    mPrevSASPartials.clear();
    
    mNoiseEnvelope.Resize(0);
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    mFreqObj->Reset(mBufferSize, mOverlapping, 1, sampleRate);
#endif
    
#if COMPUTE_SAS_SAMPLES_TABLE
    mTableSynth->Reset(sampleRate);
#endif

    mPartialsToFreq->Reset(mBufferSize, mOverlapping, 1, sampleRate);
}

void
SASFrame4::Reset(int bufferSize, int oversampling,
                 int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mOverlapping = oversampling;
    
    Reset(sampleRate);

#if ONSET_HISTORY_HACK
    mInputMagnsHistory.clear();
#endif
}

void
SASFrame4::SetMinAmpDB(BL_FLOAT ampDB)
{
    mMinAmpDB = ampDB;
}

void
SASFrame4::SetSynthMode(enum SynthMode mode)
{
    mSynthMode = mode;
}

void
SASFrame4::SetSynthEvenPartials(bool flag)
{
    mSynthEvenPartials = flag;
}

void
SASFrame4::SetSynthOddPartials(bool flag)
{
    mSynthOddPartials = flag;
}

void
SASFrame4::SetPartials(const vector<PartialTracker5::Partial> &partials)
{
    mPrevPartials = mPartials;
    mPartials = partials;
    
    // FIX: sorting by freq avoids big jumps in computed frequency when
    // id of a given partial changes.
    // (at least when the id of the first partial).
    sort(mPartials.begin(), mPartials.end(),
         PartialTracker5::Partial::FreqLess);
    
    mAmplitude = 0.0;
    
    mFrequency = 0.0;
    
    Compute();
}

void
SASFrame4::SetNoiseEnvelope(const WDL_TypedBuf<BL_FLOAT> &noiseEnv)
{
    mNoiseEnvelope = noiseEnv;
}

void
SASFrame4::GetNoiseEnvelope(WDL_TypedBuf<BL_FLOAT> *noiseEnv) const
{
    *noiseEnv = mNoiseEnvelope;
}

void
SASFrame4::SetInputData(const WDL_TypedBuf<BL_FLOAT> &magns,
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

BL_FLOAT
SASFrame4::GetAmplitude() const
{
    return mAmplitude*mAmpFactor;
}

BL_FLOAT
SASFrame4::GetFrequency() const
{
    return mFrequency*mFreqFactor;
}

void
SASFrame4::GetColor(WDL_TypedBuf<BL_FLOAT> *color) const
{
    *color = mColor;

    //BLUtils::MultValues(color, mColorFactor);
    ApplyColorFactor(color, mColorFactor);
}

void
SASFrame4::GetNormWarping(WDL_TypedBuf<BL_FLOAT> *warping) const
{
    *warping = mNormWarping;

    BLUtils::AddValues(warping, (BL_FLOAT)-1.0);
    BLUtils::MultValues(warping, mWarpingFactor);
    BLUtils::AddValues(warping, (BL_FLOAT)1.0);
}

void
SASFrame4::SetAmplitude(BL_FLOAT amp)
{
    mAmplitude = amp;
}

void
SASFrame4::SetFrequency(BL_FLOAT freq)
{
    mFrequency = freq;
}

void
SASFrame4::SetColor(const WDL_TypedBuf<BL_FLOAT> &color)
{
    mPrevColor = mColor;
    
    mColor = color;
}

void
SASFrame4::SetNormWarping(const WDL_TypedBuf<BL_FLOAT> &warping)
{
    mPrevNormWarping = mNormWarping;
    
    mNormWarping = warping;
}

void
SASFrame4::ComputeSamplesPost(WDL_TypedBuf<BL_FLOAT> *samples)
{
    ComputeSamplesPartials(samples);
}

void
SASFrame4::ComputeSamples(WDL_TypedBuf<BL_FLOAT> *samples)
{
    ComputeFftPartials(samples);
}

// TODO: teste this!
void
SASFrame4::ComputeFftPartials(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
        return;
    
    // Fft
    WDL_TypedBuf<BL_FLOAT> magns;
    BLUtils::ResizeFillZeros(&magns, mBufferSize/2);
    
    WDL_TypedBuf<BL_FLOAT> phases;
    BLUtils::ResizeFillZeros(&phases, mBufferSize/2);
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        if (i > SYNTH_MAX_NUM_PARTIALS)
            break;
        
        const PartialTracker5::Partial &partial = mPartials[i];
    
        BL_FLOAT freq = partial.mFreq;
        if (freq > mSampleRate*0.5)
            continue;
        
        if (freq < SYNTH_MIN_FREQ)
            continue;
        
        // Magn
        BL_FLOAT magn = partial.mAmp;
        BL_FLOAT phase = partial.mPhase;
            
        // Fill the fft
        BL_FLOAT binNum = freq/hzPerBin;
        binNum = bl_round(binNum);
        if (binNum < 0)
            binNum = 0;
        if (binNum > magns.GetSize() - 1)
            binNum = magns.GetSize() - 1;
                
        magns.Get()[(int)binNum] = magn;
        phases.Get()[(int)binNum] = phase;
            
        // Update the phases
        //int numSamples = samples->GetSize()/mOverlapping;
        //phase += numSamples*2.0*M_PI*freq/mSampleRate;
        //mSASPartials[partialIndex].mPhase = phase;
    }
    
    // Apply Ifft
    WDL_TypedBuf<WDL_FFT_COMPLEX> complexBuf;
    BLUtilsComp::MagnPhaseToComplex(&complexBuf, magns, phases);
    complexBuf.Resize(complexBuf.GetSize()*2);
    BLUtilsFft::FillSecondFftHalf(&complexBuf);
    
    // Ifft
    FftProcessObj16::FftToSamples(complexBuf, samples);
}

void
SASFrame4::ComputeSamplesResynthPost(WDL_TypedBuf<BL_FLOAT> *samples)
{
#if COMPUTE_SAS_SAMPLES
    if (mSynthMode == OSC)
        ComputeSamplesSAS(samples);
#endif
    
    // TEST: was tested to put it in ComputeSamples()
    // benefit from overlapping (result: no clicks),
    // and to take full nearest buffers
    // => but the sound was worse than ifft + freqObj
#if COMPUTE_SAS_SAMPLES_TABLE
    ComputeSamplesSASTable(samples);
    //ComputeSamplesSASTable2(samples);
#endif
}

void
SASFrame4::ComputeSamplesResynth(WDL_TypedBuf<BL_FLOAT> *samples)
{
#if COMPUTE_SAS_FFT
    ComputeFftSAS(samples);
#endif
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    if (mSynthMode == FFT)
    {
        ComputeFftSASFreqAdjust(samples);
    }
#endif
    
#if COMPUTE_SAS_SAMPLES_OVERLAP
    ComputeSamplesSASOverlap(samples);
#endif
}

// Directly use partials provided
void
SASFrame4::ComputeSamplesPartials(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
    {
        return;
    }
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        PartialTracker5::Partial partial;
        
        BL_FLOAT phase = 0.0;
        int prevPartialIdx = FindPrevPartialIdx(i);
        if (prevPartialIdx != -1)
            phase = mPrevPartials[prevPartialIdx].mPhase;
        
        for (int j = 0; j < samples->GetSize()/mOverlapping; j++)
        {
            // Get interpolated partial
            BL_FLOAT partialT = ((BL_FLOAT)j)/(samples->GetSize()/mOverlapping);
            GetPartial(&partial, i, partialT);
            
            BL_FLOAT freq = partial.mFreq;
            BL_FLOAT amp = partial.mAmp;
            
            BL_FLOAT samp = amp*std::sin(phase); // cos
            
            samp *= SYNTH_AMP_COEFF;
            
            if (freq >= SYNTH_MIN_FREQ)
                samples->Get()[j] += samp;
            
            phase += 2.0*M_PI*freq/mSampleRate;
        }
        
        mPartials[i].mPhase = phase;
    }
}

// ComputeSamplesSAS2
// Optim
// Gain (by suppressing loops): 74 => 57ms (~20%)
//
// ComputeSamplesSAS3: avoid tiny clicks (not audible)
//
// ComputeSamplesSAS4: optimize more
//
// ComputeSamplesSAS5: optimize more
//
// ComputeSamplesSAS6: Refact and try to debug
//
// ComputeSamplesSAS7: copy (new)
void
SASFrame4::ComputeSamplesSAS(WDL_TypedBuf<BL_FLOAT> *samples)
{
#if DEBUG_DUMP_VALUES
    BL_FLOAT avgInputMagns = BLUtils::ComputeAvg(mInputMagns);
    BLDebug::AppendValue("magns.txt", avgInputMagns);

    BL_FLOAT avgNoiseEnv = BLUtils::ComputeAvg(mNoiseEnvelope);
    BLDebug::AppendValue("noise.txt", avgNoiseEnv);

    BL_FLOAT avgColor = BLUtils::ComputeAvg(mColor);
    BLDebug::AppendValue("color.txt", avgColor);

    BL_FLOAT avgWarping = BLUtils::ComputeAvg(mNormWarping);
    BLDebug::AppendValue("warping.txt", avgWarping);
    
    BLDebug::AppendValue("freq.txt", mFrequency);
    BLDebug::AppendValue("amp.txt", mAmplitude);
    BLDebug::AppendValue("nump.txt", (BL_FLOAT)mPartials.size());

    BLDebug::AppendValue("color-factor.txt", mColorFactor);
#endif
    
    bool transientDetected = false;
#if ENABLE_ONSET_DETECTION
#if !ONSET_HISTORY_HACK
    mOnsetDetector->Detect(mInputMagns); // Origin
#else
    mOnsetDetector->Detect(mInputMagnsHistory[0]);
#endif
    
    BL_FLOAT onsetValue = mOnsetDetector->GetCurrentOnsetValue();

#if DEBUG_DUMP_VALUES
    BLDebug::AppendValue("onset.txt", onsetValue);
#endif
    
#if DETECT_TRANSIENTS_ONSET
    transientDetected = (onsetValue > ONSET_VALUE_THRESHOLD);
#endif
    
#endif
    
    BLUtils::FillAllZero(samples);
    
    // First time: initialize the partials
    if (mSASPartials.empty())
    {
        mSASPartials.resize(SYNTH_MAX_NUM_PARTIALS);
    }
    
    if (mPrevSASPartials.empty())
        mPrevSASPartials = mSASPartials;
    
    if (mPrevColor.GetSize() != mColor.GetSize())
        mPrevColor = mColor;
    
    if (mPrevNormWarping.GetSize() != mNormWarping.GetSize())
        mPrevNormWarping = mNormWarping;
    
    // For applying amplitude the correct way
    // Keep a denominator for each sample.
    WDL_TypedBuf<BL_FLOAT> ampDenoms;
    ampDenoms.Resize(samples->GetSize());
    BLUtils::FillAllZero(&ampDenoms);
    
    // Optim
    BL_FLOAT phaseCoeff = 2.0*M_PI/mSampleRate;
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT hzPerBinInv = 1.0/hzPerBin;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT partialFreq = mFrequency*mFreqFactor;
    int partialIndex = 0;
    while((partialFreq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (partialIndex > 0)
        {
            if ((!mSynthEvenPartials) && (partialIndex % 2 == 0))
            {
                partialIndex++;
                partialFreq = mFrequency*mFreqFactor*(partialIndex + 1);
                
                continue;
            }

            if ((!mSynthOddPartials) && (partialIndex % 2 == 1))
            {
                partialIndex++;
                partialFreq = mFrequency*mFreqFactor*(partialIndex + 1);
                
                continue;
            }
        }
        
        if (partialFreq > SYNTH_MIN_FREQ)
        {
            // Current and prev partials
            SASPartial &partial = mSASPartials[partialIndex];
            partial.mFreq = partialFreq;
            partial.mAmp = mAmplitude;
            
            const SASPartial &prevPartial = mPrevSASPartials[partialIndex];
            
            // Current phase
            BL_FLOAT phase = prevPartial.mPhase;
            
            // Bin param
            BL_FLOAT binIdx = partialFreq*hzPerBinInv;
            
            // Warping
            BL_FLOAT w0 = GetWarping(mPrevNormWarping, binIdx);
            BL_FLOAT w1 = GetWarping(mNormWarping, binIdx);
            
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
                partialFreq = mFrequency*mFreqFactor*(partialIndex + 1);
                
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
                if (binIdx < mNormWarping.GetSize() - 1)
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
                //SIN_LUT_GET(SAS_FRAME_SIN_LUT, samp, phase);
                
                // Better quality.
                // No "blurb" between frequencies
                BL_FLOAT samp = std::sin(phase);
                
                if (freq >= SYNTH_MIN_FREQ)
                {
                    // Generate samples only if not on an transient
                    if (!transientDetected)
                    {
                        samples->Get()[i] += samp*col;
                        ampDenoms.Get()[i] += col;
                    }
                }
                
                t += tStep;
                phase += phaseCoeff*freq;
            }
            
            // Compute next phase
            mSASPartials[partialIndex].mPhase = phase;
        }
        
        partialIndex++;            
            
        partialFreq = mFrequency*mFreqFactor*(partialIndex + 1);
    }
    
    // At the end, apply amplitude
    //
    BL_FLOAT tStep = 1.0/(samples->GetSize() - 1);
    BL_FLOAT t = 0.0;
    for (int i = 0; i < samples->GetSize(); i++)
    {
        BL_FLOAT s = samples->Get()[i];
        BL_FLOAT d = ampDenoms.Get()[i];
        
        if (d < BL_EPS)
            continue;
        
        BL_FLOAT amp = GetAmp(mPrevAmplitude, mAmplitude, t);
        
        BL_FLOAT a = amp*(s/d);
        
        samples->Get()[i] = a;
        
        t += tStep;
    }
    
    mPrevAmplitude = mAmplitude;

    if (transientDetected)
    {
        // Set the volumes to 0, for next step, to avoid click
        mPrevAmplitude = 0.0;
    }
    
    mPrevSASPartials = mSASPartials;
}

BL_FLOAT
SASFrame4::GetColor(const WDL_TypedBuf<BL_FLOAT> &color, BL_FLOAT binIdx)
{
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

    //col *= mColorFactor;
    col = ApplyColorFactor(col, mColorFactor);
    
    return col;
}

BL_FLOAT
SASFrame4::GetWarping(const WDL_TypedBuf<BL_FLOAT> &warping,
                      BL_FLOAT binIdx)
{
    BL_FLOAT w = 0.0;
    if (binIdx < warping.GetSize() - 1)
    {
        BL_FLOAT t = binIdx - (int)binIdx;
        
        BL_FLOAT w0 = warping.Get()[(int)binIdx];
        BL_FLOAT w1 = warping.Get()[((int)binIdx) + 1];
        
        w = (1.0 - t)*w0 + t*w1;
    }

    // Warping is center on 1
    w -= 1.0;
    
    w *= mWarpingFactor;

    w += 1.0;
    
    return w;
}

void
SASFrame4::ComputeSamplesSASOverlap(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
        return;
    
    BL_FLOAT amp0 = GetAmplitude();
    BL_FLOAT freq0 = GetFrequency();
    
    freq0 *= mFreqFactor;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT freq = freq0;
    int partialIndex = 0;
    while((freq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (freq > SYNTH_MIN_FREQ)
        {
            freq = ApplyNormWarping(freq);
            BL_FLOAT col = ApplyColor(freq);
            
            if (mSASPartials.size() <= partialIndex)
            {
                SASPartial newPartial;
                newPartial.mFreq = freq;
                newPartial.mAmp = amp0*col;
                
                mSASPartials.push_back(newPartial);
            }
            else
            {
                SASPartial &partial = mSASPartials[partialIndex];
                partial.mFreq = freq;
                partial.mAmp = amp0*col;
            }
            
            BL_FLOAT phase = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                phase = mPrevSASPartials[partialIndex].mPhase;
            
            SASPartial partial = mSASPartials[partialIndex];
            for (int i = 0; i < samples->GetSize(); i++)
            {
                BL_FLOAT freq2 = partial.mFreq;
                
                BL_FLOAT amp = partial.mAmp;
                
                BL_FLOAT samp = std::sin(phase);
                
                samp *= amp;
                samp *= SYNTH_AMP_COEFF;
                
                if (freq2 >= SYNTH_MIN_FREQ)
                    samples->Get()[i] += samp;
                
                phase += 2.0*M_PI*freq2/mSampleRate;
            }
            
            mSASPartials[partialIndex].mPhase =/*+=*/ phase;
        }
        
        partialIndex++;
        freq = freq0*(partialIndex + 1);
    }
    
    mPrevSASPartials = mSASPartials;
}

void
SASFrame4::ComputeFftSAS(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
        return;
    
    BL_FLOAT amp0 = GetAmplitude();
    BL_FLOAT freq0 = GetFrequency();
    
    freq0 *= mFreqFactor;
    
    // Fft
    WDL_TypedBuf<BL_FLOAT> magns;
    BLUtils::ResizeFillZeros(&magns, mBufferSize/2);
    
    WDL_TypedBuf<BL_FLOAT> phases;
    BLUtils::ResizeFillZeros(&phases, mBufferSize/2);
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT freq = freq0;
    int partialIndex = 0;
    while((freq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (freq > SYNTH_MIN_FREQ)
        {
            if (mSASPartials.size() <= partialIndex)
            {
                SASPartial newPartial;
                newPartial.mFreq = freq;
                newPartial.mAmp = amp0;
                
                mSASPartials.push_back(newPartial);
            }
            else
            {
                SASPartial &partial = mSASPartials[partialIndex];
                partial.mFreq = freq;
                partial.mAmp = amp0;
            }
            
            // Freq
            BL_FLOAT freq2 = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                freq2 = mPrevSASPartials[partialIndex].mFreq;
            freq2 = ApplyNormWarping(freq2);
                
            // Magn
            BL_FLOAT magn = 0.0;
            if (partialIndex < mPrevSASPartials.size())
            {
                magn = mPrevSASPartials[partialIndex].mAmp;
                //magn = magn; // ??
            }
            
            // Color
            BL_FLOAT col = ApplyColor(freq2);
            magn *= col;
            
            // Phase
            BL_FLOAT phase = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                phase = mPrevSASPartials[partialIndex].mPhase;
            
            // Fill the fft
            if (freq2 >= SYNTH_MIN_FREQ)
            {
                BL_FLOAT binNum = freq2/hzPerBin;
                binNum = bl_round(binNum);
                if (binNum < 0)
                    binNum = 0;
                if (binNum > magns.GetSize() - 1)
                    binNum = magns.GetSize() - 1;
                
                magns.Get()[(int)binNum] = magn;
                phases.Get()[(int)binNum] = phase;
            }
            
            // Update the phases
            int numSamples = samples->GetSize()/mOverlapping;
            phase += numSamples*2.0*M_PI*freq2/mSampleRate;
            mSASPartials[partialIndex].mPhase = phase;
        }
                    
        partialIndex++;
        freq = freq0*(partialIndex + 1);
    }
    
    // Apply Ifft
    WDL_TypedBuf<WDL_FFT_COMPLEX> complexBuf;
    BLUtilsComp::MagnPhaseToComplex(&complexBuf, magns, phases);
    complexBuf.Resize(complexBuf.GetSize()*2);
    BLUtilsFft::FillSecondFftHalf(&complexBuf);
    
    // Ifft
    FftProcessObj16::FftToSamples(complexBuf, samples);
    
    mPrevSASPartials = mSASPartials;
}

void
SASFrame4::ComputeFftSASFreqAdjust(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    
    BLUtils::FillAllZero(samples);
    
    BL_FLOAT amp0 = GetAmplitude();
    BL_FLOAT freq0 = GetFrequency();
    
    //freq0 *= mFreqFactor;
    
    // Fft
    WDL_TypedBuf<BL_FLOAT> magns;
    BLUtils::ResizeFillZeros(&magns, mBufferSize/2);
    
    WDL_TypedBuf<BL_FLOAT> phases;
    BLUtils::ResizeFillZeros(&phases, mBufferSize/2);
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    WDL_TypedBuf<BL_FLOAT> realFreqs;
    BLUtils::ResizeFillZeros(&realFreqs, mBufferSize/2);
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT freq = freq0;
    int partialIndex = 0;
    while((freq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (freq > SYNTH_MIN_FREQ)
        {
            if (mSASPartials.size() <= partialIndex)
            {
                SASPartial newPartial;
                newPartial.mFreq = freq;
                newPartial.mAmp = amp0;
                
                mSASPartials.push_back(newPartial);
            }
            else
            {
                SASPartial &partial = mSASPartials[partialIndex];
                partial.mFreq = freq;
                partial.mAmp = amp0;
            }
            
            // Freq
            BL_FLOAT freq2 = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                freq2 = mPrevSASPartials[partialIndex].mFreq;
            freq2 = ApplyNormWarping(freq2);
            
            // Magn
            BL_FLOAT magn = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                magn = mPrevSASPartials[partialIndex].mAmp;
            
            // Color
            BL_FLOAT col = ApplyColor(freq2);
            magn *= col;
            
            // Phase
            BL_FLOAT phase = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                phase = mPrevSASPartials[partialIndex].mPhase;
            
            // Fill the fft
            if (freq2 >= SYNTH_MIN_FREQ)
            {
                BL_FLOAT binNum = freq2/hzPerBin;
                binNum = bl_round(binNum);
                if (binNum < 0)
                    binNum = 0;
                if (binNum > magns.GetSize() - 1)
                    binNum = magns.GetSize() - 1;
                
                magns.Get()[(int)binNum] = magn;
                phases.Get()[(int)binNum] = phase;
                
                // Set real freq
                realFreqs.Get()[(int)binNum] = freq2;
            }
            
            // Update the phases
            int numSamples = samples->GetSize()/mOverlapping;
            phase += numSamples*2.0*M_PI*freq2/mSampleRate;
            if (partialIndex < mSASPartials.size()) // Tmp fix...
                mSASPartials[partialIndex].mPhase = phase;
        }
        
        partialIndex++;
        freq = freq0*(partialIndex + 1);
    }
    
    // Set adjusted phases
    WDL_TypedBuf<BL_FLOAT> phasesAdjust;
    mFreqObj->ComputePhases(&phasesAdjust, realFreqs);
    for (int i = 0; i < realFreqs.GetSize(); i++)
    {
        phases.Get()[i] = phasesAdjust.Get()[i];
    }
        
    // Apply Ifft
    WDL_TypedBuf<WDL_FFT_COMPLEX> complexBuf;
    BLUtilsComp::MagnPhaseToComplex(&complexBuf, magns, phases);
    complexBuf.Resize(complexBuf.GetSize()*2);
    BLUtilsFft::FillSecondFftHalf(&complexBuf);
    
    // Ifft
    FftProcessObj16::FftToSamples(complexBuf, samples);
    
    mPrevSASPartials = mSASPartials;
}

void
SASFrame4::ComputeSamplesSASTable(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
        return;
    
    BL_FLOAT amp0 = GetAmplitude();
    BL_FLOAT freq0 = GetFrequency();
    
    freq0 *= mFreqFactor;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT freq = freq0;
    int partialIndex = 0;
    while((freq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (freq > SYNTH_MIN_FREQ)
        {
            if (mSASPartials.size() <= partialIndex)
            {
                SASPartial newPartial;
                newPartial.mFreq = freq;
                newPartial.mAmp = amp0;
                
                mSASPartials.push_back(newPartial);
            }
            else
            {
                SASPartial &partial = mSASPartials[partialIndex];
                partial.mFreq = freq;
                partial.mAmp = amp0;
            }
            
            // Freq
            BL_FLOAT freq2 = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                freq2 = mPrevSASPartials[partialIndex].mFreq;
            freq2 = ApplyNormWarping(freq2);
            
            // Magn
            BL_FLOAT amp = 0.0;
            if (partialIndex < mPrevSASPartials.size())
            {
                amp = mPrevSASPartials[partialIndex].mAmp;
            }
            
            // Color
            BL_FLOAT col = ApplyColor(freq2);
            amp *= col;
            
            amp *= SYNTH_AMP_COEFF;
            
            // Phase
            BL_FLOAT phase = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                phase = mPrevSASPartials[partialIndex].mPhase;
            
            // Fill the fft
            if (freq2 >= SYNTH_MIN_FREQ)
            {
                WDL_TypedBuf<BL_FLOAT> freqBuffer;
                freqBuffer.Resize(mBufferSize/mOverlapping);
                mTableSynth->GetSamplesLinear(&freqBuffer, freq2, amp);
                
                BLUtils::AddValues(samples, freqBuffer);
            }
            
            // Update the phases
            int numSamples = samples->GetSize()/mOverlapping;
            phase += numSamples*2.0*M_PI*freq2/mSampleRate;
            mSASPartials[partialIndex].mPhase = phase;
        }
        
        partialIndex++;
        freq = freq0*(partialIndex + 1);
    }
    
    mPrevSASPartials = mSASPartials;
    
    mTableSynth->NextBuffer();
}

void
SASFrame4::ComputeSamplesSASTable2(WDL_TypedBuf<BL_FLOAT> *samples)
{
    samples->Resize(mBufferSize);
    BLUtils::FillAllZero(samples);
    
    if (mPartials.empty())
        return;
    
    BL_FLOAT amp0 = GetAmplitude();
    BL_FLOAT freq0 = GetFrequency();
    
    //freq0 *= mFreqFactor;
    
    // Sort partials by amplitude, in order to play the highest amplitudes ?
    BL_FLOAT freq = freq0;
    int partialIndex = 0;
    while((freq < mSampleRate/2.0) && (partialIndex < SYNTH_MAX_NUM_PARTIALS))
    {
        if (freq > SYNTH_MIN_FREQ)
        {
            if (mSASPartials.size() <= partialIndex)
            {
                SASPartial newPartial;
                newPartial.mFreq = freq;
                newPartial.mAmp = amp0;
                
                mSASPartials.push_back(newPartial);
            }
            else
            {
                SASPartial &partial = mSASPartials[partialIndex];
                partial.mFreq = freq;
                partial.mAmp = amp0;
            }
            
            SASPartial partial;
            for (int i = 0; i < samples->GetSize()/mOverlapping; i++)
            {
                // Get interpolated partial
                BL_FLOAT partialT = ((BL_FLOAT)i)/(samples->GetSize()/mOverlapping);
                GetSASPartial(&partial, partialIndex, partialT);
                
                BL_FLOAT freq2 = partial.mFreq;
                
                freq2 = ApplyNormWarping(freq2, partialT);
                BL_FLOAT col = ApplyColor(freq2, partialT);
                
                BL_FLOAT amp = partial.mAmp;
                amp  *= col;
                
                amp *= SYNTH_AMP_COEFF;
                
                // Get from the wavetable
                if (freq2 >= SYNTH_MIN_FREQ)
                {
                    BL_FLOAT samp = mTableSynth->GetSampleNearest(i, freq2, amp);
                    samples->Get()[i] += samp;
                }
            }
            
            // Phase
            BL_FLOAT phase = 0.0;
            if (partialIndex < mPrevSASPartials.size())
                phase = mPrevSASPartials[partialIndex].mPhase;
            
            // Update the phases
            int numSamples = samples->GetSize()/mOverlapping;
            phase += numSamples*2.0*M_PI*freq/mSampleRate;
            mSASPartials[partialIndex].mPhase = phase;
        }
        
        partialIndex++;
        freq = freq0*(partialIndex + 1);
    }
    
    mPrevSASPartials = mSASPartials;
    
    mTableSynth->NextBuffer();
}

void
SASFrame4::SetAmpFactor(BL_FLOAT factor)
{
    mAmpFactor = factor;
}

void
SASFrame4::SetFreqFactor(BL_FLOAT factor)
{
    mFreqFactor = factor;
}

void
SASFrame4::SetColorFactor(BL_FLOAT factor)
{
    mColorFactor = factor;
}

void
SASFrame4::SetWarpingFactor(BL_FLOAT factor)
{
    mWarpingFactor = factor;
}

bool
SASFrame4::ComputeSamplesFlag()
{
#if COMPUTE_SAS_SAMPLES
    if (mSynthMode == OSC)
        return false;
#endif

#if COMPUTE_SAS_FFT
    return true;
#endif
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    if (mSynthMode == FFT)
        return true;
#endif
    
#if COMPUTE_SAS_SAMPLES_TABLE
    return false;
#endif
    
#if COMPUTE_SAS_SAMPLES_OVERLAP
    return true;
#endif
    
    return false;
}

bool
SASFrame4::ComputeSamplesPostFlag()
{
#if COMPUTE_SAS_SAMPLES
    if (mSynthMode == OSC)
        return true;
#endif
    
#if COMPUTE_SAS_FFT
    return false;
#endif
    
#if COMPUTE_SAS_FFT_FREQ_ADJUST
    if (mSynthMode == FFT)
        return false;
#endif
    
#if COMPUTE_SAS_SAMPLES_TABLE
    return true;
#endif

#if COMPUTE_SAS_SAMPLES_OVERLAP
    return false;
#endif
    
    return false;
}

void
SASFrame4::Compute()
{
    ComputeAmplitude();
    ComputeFrequency();
    ComputeColor();
    ComputeNormWarping();
}

void
SASFrame4::ComputeAmplitude()
{
    // Amp must not be in dB, but direct!
    //mPrevAmplitude = mAmplitude;
    mAmplitude = 0.0;

    const vector<PartialTracker5::Partial> &partials = mPartials;
    
    BL_FLOAT amplitude = 0.0;
    for (int i = 0; i < partials.size(); i++)
    {
        const PartialTracker5::Partial &p = partials[i];
        
        BL_FLOAT amp = p.mAmp;
        amplitude += amp;
    }
    
    mAmplitude = amplitude;
}

void
SASFrame4::ComputeFrequency()
{
    BL_FLOAT freq =
        mPartialsToFreq->ComputeFrequency(mInputMagns, mInputPhases, mPartials);
    
    mFrequency = freq;
    
    // Smooth
    if (mPrevFrequency < 0.0)
        mPrevFrequency = freq;
    else
    {
        freq = (1.0 - FREQ_SMOOTH_COEFF)*freq + FREQ_SMOOTH_COEFF*mPrevFrequency;
        
        mPrevFrequency = freq;
        mFrequency = freq;
    }
}

void
SASFrame4::ComputeColor()
{
    mPrevColor = mColor;
    
    WDL_TypedBuf<BL_FLOAT> prevColor = mColor;
    
    ComputeColorAux();
    
    if (prevColor.GetSize() != mColor.GetSize())
        return;
    
    // Smooth
    for (int i = 0; i < mColor.GetSize(); i++)
    {
        BL_FLOAT col = mColor.Get()[i];
        BL_FLOAT prevCol = prevColor.Get()[i];
        
        BL_FLOAT result = COLOR_SMOOTH_COEFF*prevCol + (1.0 - COLOR_SMOOTH_COEFF)*col;
        
        mColor.Get()[i] = result;
    }
}

void
SASFrame4::ComputeColorAux()
{
#if !COLOR_DB_INTERP
    BL_FLOAT minColorValue = 0.0;
    BL_FLOAT undefinedValue = -1.0; // -300dB
#else
    BL_FLOAT minColorValue = mMinAmpDB;
    BL_FLOAT undefinedValue = -300.0; // -300dB
#endif
    
    mColor.Resize(mBufferSize/2);
    
    if (mFrequency < BL_EPS)
    {
        BLUtils::FillAllValue(&mColor, minColorValue);
        
        return;
    }
    
    // Will interpolate values between the partials
    BLUtils::FillAllValue(&mColor, undefinedValue);
    
    // Fix bounds at 0
    mColor.Get()[0] = minColorValue;
    mColor.Get()[mBufferSize/2 - 1] = minColorValue;
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    // Put the values we have
    for (int i = 0; i < mPartials.size(); i++)
    {
        const PartialTracker5::Partial &p = mPartials[i];
 
        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != PartialTracker5::Partial::ALIVE)
            continue;
        
        BL_FLOAT idx = p.mFreq/hzPerBin;
        
        // TODO: make an interpolation, it is not so good to align to bins
        idx = bl_round(idx);
        
        BL_FLOAT amp = p.mAmp;
        
#if COLOR_DB_INTERP
        amp = mScale->ApplyScale(Scale::DB, amp, mMinAmpDB, (BL_FLOAT)0.0);
#endif
        
        if (((int)idx >= 0) && ((int)idx < mColor.GetSize()))
            mColor.Get()[(int)idx] = amp;

        // TEST: with this and sines example, the first partial has the right gain
        // Without this, the first synth partial amp is lower 
        if (i == 0)
            mColor.Get()[0] = amp;
    }
    
#if COLOR_CUT_MISSING_PARTIALS
    // Put some zeros when partials are missing
    BL_FLOAT freq = mFrequency;
    while(freq < mSampleRate/2.0)
    {
        if (!FindPartial(freq))
        {
            BL_FLOAT idx = freq/hzPerBin;
            if (idx >= mColor.GetSize())
                break;
            
            mColor.Get()[(int)idx] = minColorValue;
        }
        
        freq += mFrequency;
    }
#endif

#if FILL_ZERO_FIRST_LAST_VALUES
    // Avoid interpolating to the last partial value to 0
    // Would make color where ther eis no sound otherwise
    // (e.g example with just some sine waves is false)
    FillLastValues(&mColor, mPartials, minColorValue);
#endif
    
    // Fill al the other value
    bool extendBounds = false;
    BLUtils::FillMissingValues(&mColor, extendBounds, undefinedValue);
    
#if COLOR_DB_INTERP
    for (int i = 0; i < mColor.GetSize(); i++)
    {
        BL_FLOAT c = mColor.Get()[i];
        
        c = mScale->ApplyScale(Scale::DB_INV, c, mMinAmpDB, (BL_FLOAT)0.0);
     
        mColor.Get()[i] = c;
    }
#endif
    
    // Normalize the color (maybe not necessary)
    BL_FLOAT amplitude = mAmplitude;
    if (amplitude > 0.0)
    {
        BL_FLOAT coeff = 1.0/amplitude;
        
        BLUtils::MultValues(&mColor, coeff);
    }
}

void
SASFrame4::ComputeNormWarping()
{
    mPrevNormWarping = mNormWarping;
    
    WDL_TypedBuf<BL_FLOAT> prevWarping = mNormWarping;
    
    //ComputeNormWarpingAux();
    ComputeNormWarpingAux2();
    
    if (prevWarping.GetSize() != mNormWarping.GetSize())
        return;

    // Smooth
    for (int i = 0; i < mNormWarping.GetSize(); i++)
    {
        BL_FLOAT warp = mNormWarping.Get()[i];
        BL_FLOAT prevWarp = prevWarping.Get()[i];
        
        BL_FLOAT result = WARPING_SMOOTH_COEFF*prevWarp +
            (1.0 - WARPING_SMOOTH_COEFF)*warp;
        
        mNormWarping.Get()[i] = result;
    }
}

void
SASFrame4::ComputeNormWarpingAux()
{
    mNormWarping.Resize(mBufferSize/2);
    
    if (mFrequency < BL_EPS)
    {
        BLUtils::FillAllValue(&mNormWarping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Will interpolate values between the partials
    BL_FLOAT undefinedValue = -1.0;
    BLUtils::FillAllValue(&mNormWarping, undefinedValue);
    
    // Fix bounds at 1
    mNormWarping.Get()[0] = 1.0;
    mNormWarping.Get()[mBufferSize/2 - 1] = 1.0;
    
    if (mPartials.size() < 2)
    {
        BLUtils::FillAllValue(&mNormWarping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Fundamental frequency
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;

    // Adjust on chroma-computed frequency
    BL_FLOAT freq0 = mFrequency;

    // Adjust of first pratial frequency
    //
    // NOTE: This is different, is it better?
    // Set to 0 for good Vox Oooh!
#if 0 //1 // Use first partial frequency, instead of mFrequency
    // mFrequency has been gotten from chroma feature, and for inharmonic sound,
    // it may be e abit different than the first partial freq
    if (!mPartials.empty())
        freq0 = mPartials[0].mFreq;
#endif
    
#if 1 // Must adjust the also on the first partial, if ref freq is chroma-computed
    // Put the values we have
    for (int i = 0; i < mPartials.size(); i++)
#else
    // Skip the first partial, so it will always have warping=1 
    for (int i = 1; i < mPartials.size(); i++)
#endif
    {
        const PartialTracker5::Partial &p = mPartials[i];
    
        // Do no add to warping if dead or zombie
        if (p.mState != PartialTracker5::Partial::ALIVE)
            continue;
        
        BL_FLOAT freq = mPartials[i].mFreq;
        BL_FLOAT freq1 = BLUtilsMath::FindNearestHarmonic(freq, freq0);
        
        BL_FLOAT normWarp = freq/freq1;

#if LIMIT_WARPING_MAX
        // Discard too high warping values, that can come to short invalid partials
        // spread randomly
        if ((normWarp < 0.8) || (normWarp > 1.25))
            continue;
#endif
        
        //BL_FLOAT idx = p.mFreq/hzPerBin;
        BL_FLOAT idx = freq1/hzPerBin;
        // TODO: make an interpolation, it is not so good to align to bins
        idx = bl_round(idx);
        
        if ((idx > 0) && (idx < mNormWarping.GetSize()))
            mNormWarping.Get()[(int)idx] = normWarp;
    }

#if FILL_ZERO_FIRST_LAST_VALUES
#if 0 // Keep the first partial warping of reference is chroma-compute freq
    // Avoid warping the first partial
    FillFirstValues(&mNormWarping, mPartials, 1.0);
#endif
    
    // NEW
    FillLastValues(&mNormWarping, mPartials, 1.0);
#endif
    
    // Fill all the other value
    bool extendBounds = false;
    BLUtils::FillMissingValues(&mNormWarping, extendBounds, undefinedValue);
}

// Problem: when incorrect partials are briefly detected, they affect warping a lot
// Solution: take each theorical synth partials, and find the closest detected
// partial to compute the norma warping (and ignore other partials)
// => a lot more robust for low thresholds, when we have many partials
//
// NOTE: this is not still perfect if we briefly loose the tracking
void
SASFrame4::ComputeNormWarpingAux2()
{
    // Init

    int maxNumPartials = SYNTH_MAX_NUM_PARTIALS;
    //BL_FLOAT minFreq = 20.0;
    //int maxNumPartials = mSampleRate*0.5/minFreq;
    
    vector<PartialAux> theoricalPartials;
    theoricalPartials.resize(maxNumPartials);
    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        theoricalPartials[i].mFreq = (i + 1)*mFrequency;
        theoricalPartials[i].mWarping = -1.0;
        theoricalPartials[i].mPartialAge = -1;
    }
    
    // Compute best match
    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        PartialAux &pa = theoricalPartials[i];
        
        for (int j = 0; j < mPartials.size(); j++)
        {
            const PartialTracker5::Partial &p = mPartials[j];

            // Do no add to warping if dead or zombie
            if (p.mState != PartialTracker5::Partial::ALIVE)
                continue;
        
            if (p.mFreq < pa.mFreq*0.5)
                continue;
            if (p.mFreq > pa.mFreq*2.0)
                break;

            BL_FLOAT w = p.mFreq/pa.mFreq;
            
#if LIMIT_WARPING_MAX
            // Discard too high warping values,
            // that can come to short invalid partials
            // spread randomly
            if ((w < 0.8) || (w > 1.25))
                continue;
#endif

#if 1
            // Keep smallest warping
            if ((pa.mWarping < 0.0) ||
                (std::fabs(w - 1.0) < std::fabs(pa.mWarping - 1.0)))
                pa.mWarping = w;
#endif

#if 0 // Bad if we briefly loose the tracking of a long partial
            // Keep biggest age, then smallest warping
            if (pa.mWarping < 0.0)
            {
                if (p.mAge >= pa.mPartialAge)
                {
                    if ((p.mAge > pa.mPartialAge) ||
                        (std::fabs(w - 1.0) < std::fabs(pa.mWarping - 1.0)))
                    {
                        pa.mWarping = w;
                        pa.mPartialAge = p.mAge;
                    }
                }
            }
#endif
        }
    }

    // Fill missing partials values
    // (in case no matchin detected partial was found for a given theorical partial)
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
    
    mNormWarping.Resize(mBufferSize/2);
    
    if (mFrequency < BL_EPS)
    {
        BLUtils::FillAllValue(&mNormWarping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Will interpolate values between the partials
    BL_FLOAT undefinedValue = -1.0;
    BLUtils::FillAllValue(&mNormWarping, undefinedValue);
    
    // Fix bounds at 1
    mNormWarping.Get()[0] = 1.0;
    mNormWarping.Get()[mBufferSize/2 - 1] = 1.0;
    
    if (mPartials.size() < 2)
    {
        BLUtils::FillAllValue(&mNormWarping, (BL_FLOAT)1.0);
        
        return;
    }
    
    // Fundamental frequency
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;

    for (int i = 0; i < theoricalPartials.size(); i++)
    {
        PartialAux &pa = theoricalPartials[i];
        
        BL_FLOAT idx = pa.mFreq/hzPerBin;
        // TODO: make an interpolation ?
        idx = bl_round(idx);
        
        if ((idx > 0) && (idx < mNormWarping.GetSize()))
            mNormWarping.Get()[(int)idx] = pa.mWarping;
    }

#if FILL_ZERO_FIRST_LAST_VALUES
#if 0 // Keep the first partial warping of reference is chroma-compute freq
    // Avoid warping the first partial
    FillFirstValues(&mNormWarping, mPartials, 1.0);
#endif
    
    // NEW
    FillLastValues(&mNormWarping, mPartials, 1.0);
#endif
    
    // Fill all the other value
    bool extendBounds = false;
    BLUtils::FillMissingValues(&mNormWarping, extendBounds, undefinedValue);
}

BL_FLOAT
SASFrame4::ApplyNormWarping(BL_FLOAT freq)
{
#if DBG_DISABLE_WARPING
    return freq;
#endif
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    
    // TODO: use linerp instead of nearest
    BL_FLOAT idx = freq/hzPerBin;
    idx = bl_round(idx);
    
    if (idx > mNormWarping.GetSize())
        return freq;
    
    BL_FLOAT w = mNormWarping.Get()[(int)idx];
    
    BL_FLOAT result = freq*w;
    
    return result;
}

BL_FLOAT
SASFrame4::ApplyColor(BL_FLOAT freq)
{
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
 
    // TODO: use linerp instead of nearest
    // TODO: use BL_FLOAT and bl_round as above
    int idx = freq/hzPerBin;
    
    if (idx > mColor.GetSize())
        return 0.0;
    
    BL_FLOAT result = mColor.Get()[idx];
    
    return result;
}

BL_FLOAT
SASFrame4::ApplyNormWarping(BL_FLOAT freq, BL_FLOAT t)
{
#if DBG_DISABLE_WARPING
    return freq;
#endif
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT idx = freq/hzPerBin;
    idx = bl_round(idx);
    if (idx > mNormWarping.GetSize())
        return freq;
    
    BL_FLOAT w0 = mPrevNormWarping.Get()[(int)idx];
    BL_FLOAT w1 = mNormWarping.Get()[(int)idx];
    
    BL_FLOAT w = (1.0 - t)*w0 + t*w1;
    
    BL_FLOAT result = freq*w;
    
    return result;
}

BL_FLOAT
SASFrame4::ApplyColor(BL_FLOAT freq, BL_FLOAT t)
{
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    int idx = freq/hzPerBin;
    if (idx > mColor.GetSize())
        return 0.0;
    
    BL_FLOAT col0 = mPrevColor.Get()[idx];
    BL_FLOAT col1 = mColor.Get()[idx];
    
    BL_FLOAT col = (1.0 - t)*col0 + t*col1;
    
    return col;
}

bool
SASFrame4::FindPartial(BL_FLOAT freq)
{
#define FIND_COEFF 0.25
    
    BL_FLOAT step = mFrequency*FIND_COEFF;
    
    for (int i = 0; i < mPartials.size(); i++)
    {
        const PartialTracker5::Partial &p = mPartials[i];
        
        if ((freq > p.mFreq - step) &&
            (freq < p.mFreq + step))
            // Found
            return true;
    }
    
    return false;
}

// Interpolate in amp
void
SASFrame4::GetPartial(PartialTracker5::Partial *result, int index, BL_FLOAT t)
{
    const PartialTracker5::Partial &currentPartial = mPartials[index];
    
    int prevPartialIdx = FindPrevPartialIdx(index);
    
    *result = currentPartial;
    
    // Manage decrease of dead partials
    //
    if ((currentPartial.mState == PartialTracker5::Partial::DEAD) &&
        currentPartial.mWasAlive)
    {
        // Decrease progressively the amplitude
        result->mAmp = 0.0;
        
        if (prevPartialIdx != -1)
            // Interpolate
        {
            const PartialTracker5::Partial &prevPartial =
                mPrevPartials[prevPartialIdx];
            
            BL_FLOAT t0 = t/SYNTH_DEAD_PARTIAL_DECREASE;
            if (t0 <= 1.0)
            {
                // Interpolate in amp
                BL_FLOAT amp = (1.0 - t0)*prevPartial.mAmp;
                result->mAmp = amp;
            }
        }
    }
    
    // Manage interpolation of freq and amp
    //
    if ((currentPartial.mState != PartialTracker5::Partial::DEAD) &&
        currentPartial.mWasAlive)
    {
        if (prevPartialIdx != -1)
        {
            const PartialTracker5::Partial &prevPartial =
                mPrevPartials[prevPartialIdx];
                
            if (prevPartial.mState == PartialTracker5::Partial::ALIVE)
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

int
SASFrame4::FindPrevPartialIdx(int currentPartialIdx)
{
    if (currentPartialIdx >= mPartials.size())
        return -1;
    
    const PartialTracker5::Partial &currentPartial = mPartials[currentPartialIdx];
    
    // Find the corresponding prev partial
    int prevPartialIdx = -1;
    for (int i = 0; i < mPrevPartials.size(); i++)
    {
        const PartialTracker5::Partial &prevPartial = mPrevPartials[i];
        if (prevPartial.mId == currentPartial.mId)
        {
            prevPartialIdx = i;
        
            // Found
            break;
        }
    }
    
    return prevPartialIdx;
}

void
SASFrame4::GetSASPartial(SASPartial *result, int index, BL_FLOAT t)
{
    const SASPartial &currentPartial = mSASPartials[index];
    *result = currentPartial;
    
    // Manage interpolation of freq and amp
    //
    if (index < mPrevSASPartials.size())
    {
        const SASPartial &prevPartial = mPrevSASPartials[index];
        
        result->mFreq = (1.0 - t)*prevPartial.mFreq + t*currentPartial.mFreq;
        result->mAmp = (1.0 - t)*prevPartial.mAmp + t*currentPartial.mAmp;
    }
    else
        // New partial, fade in
    {
        // NOTE: this avoids vertical bars in the spectrogram when a partial starts
            
        // Increase progressively the amplitude
            
        // Interpolate
        BL_FLOAT t0 = t/SYNTH_DEAD_PARTIAL_DECREASE;
        if (t0 > 1.0)
            t0 = 1.0;
            
        result->mAmp = t0*currentPartial.mAmp;
    }
}

void
SASFrame4::MixFrames(SASFrame4 *result,
                     const SASFrame4 &frame0,
                     const SASFrame4 &frame1,
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
    
    // Warping
    WDL_TypedBuf<BL_FLOAT> warp0;
    frame0.GetNormWarping(&warp0);
    
    WDL_TypedBuf<BL_FLOAT> warp1;
    frame1.GetNormWarping(&warp1);
    
    WDL_TypedBuf<BL_FLOAT> resultWarping;
    BLUtils::Interp(&resultWarping, &warp0, &warp1, t);
    
    result->SetNormWarping(resultWarping);
    
    // Noise envelope
    WDL_TypedBuf<BL_FLOAT> noise0;
    frame0.GetNoiseEnvelope(&noise0);
    
    WDL_TypedBuf<BL_FLOAT> noise1;
    frame1.GetNoiseEnvelope(&noise1);
    
    WDL_TypedBuf<BL_FLOAT> resultNoise;
    BLUtils::Interp(&resultNoise, &noise0, &noise1, t);
    
    result->SetNoiseEnvelope(resultNoise);
}

BL_FLOAT
SASFrame4::GetFreq(BL_FLOAT freq0, BL_FLOAT freq1, BL_FLOAT t)
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
SASFrame4::GetAmp(BL_FLOAT amp0, BL_FLOAT amp1, BL_FLOAT t)
{
#if INTERP_RESCALE
    // If amp is not already in dB)
    
    amp0 = mScale->ApplyScale(Scale::DB, amp0, mMinAmpDB, (BL_FLOAT)0.0);
    amp1 = mScale->ApplyScale(Scale::DB, amp1, mMinAmpDB, (BL_FLOAT)0.0);
#endif
    
    BL_FLOAT amp = (1.0 - t)*amp0 + t*amp1;

#if INTERP_RESCALE
    amp = mScale->ApplyScale(Scale::DB_INV, amp, mMinAmpDB, (BL_FLOAT)0.0);
#endif

    amp *= mAmpFactor;
    
    return amp;
}

BL_FLOAT
SASFrame4::GetCol(BL_FLOAT col0, BL_FLOAT col1, BL_FLOAT t)
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
SASFrame4::FillLastValues(WDL_TypedBuf<BL_FLOAT> *values,
                          const vector<PartialTracker5::Partial> &partials,
                          BL_FLOAT val)
{
    // First, find the last bin index
    int maxIdx = -1;
    for (int i = 0; i < partials.size(); i++)
    {
        const PartialTracker5::Partial &p = partials[i];
        
        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != PartialTracker5::Partial::ALIVE)
            continue;
        
        if (p.mRightIndex > maxIdx)
            maxIdx = p.mRightIndex;
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
SASFrame4::FillFirstValues(WDL_TypedBuf<BL_FLOAT> *values,
                           const vector<PartialTracker5::Partial> &partials,
                           BL_FLOAT val)
{
    // First, find the last bin idex
    int minIdx = values->GetSize() - 1;
    for (int i = 0; i < partials.size(); i++)
    {
        const PartialTracker5::Partial &p = partials[i];

        // Dead or zombie: do not use for color enveloppe
        // (this is important !)
        if (p.mState != PartialTracker5::Partial::ALIVE)
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

BL_FLOAT
SASFrame4::ApplyColorFactor(BL_FLOAT color, BL_FLOAT factor)
{
#if 0
    // Naive
    BL_FLOAT res = color*factor;
#endif

#if 1
    // Sigmoid
    BL_FLOAT a = factor*0.5;
    if (a < BL_EPS)
        a = BL_EPS;
    if (a > 1.0 - BL_EPS)
        a = 1.0 - BL_EPS;

    BL_FLOAT res = BLUtilsMath::ApplySigmoid(color, a);
#endif
        
    return res;
}

void
SASFrame4::ApplyColorFactor(WDL_TypedBuf<BL_FLOAT> *color, BL_FLOAT factor)
{
    for (int i = 0; i < color->GetSize(); i++)
    {
        BL_FLOAT col = color->Get()[i];
        col = ApplyColorFactor(col, factor);
        color->Get()[i] = col;
    }
}
