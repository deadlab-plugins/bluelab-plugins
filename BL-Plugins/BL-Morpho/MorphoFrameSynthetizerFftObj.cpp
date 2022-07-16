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
 
#include <BLUtils.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include <MorphoFrame7.h>
#include <MorphoFrameSynth2.h>

#include <BLDebug.h>

#include <Morpho_defs.h>

#include "MorphoFrameSynthetizerFftObj.h"

MorphoFrameSynthetizerFftObj::MorphoFrameSynthetizerFftObj(int bufferSize,
                                                     BL_FLOAT overlapping,
                                                     BL_FLOAT oversampling,
                                                     BL_FLOAT sampleRate)
: ProcessObj(bufferSize)
{
    mOverlapping = overlapping;
    mFreqRes = oversampling;
    mSampleRate = sampleRate;

    mMorphoFrameSynth = new MorphoFrameSynth2(bufferSize, overlapping, 1, sampleRate);
    
    mMorphoFrameSynth->SetMinAmpDB(MIN_AMP_DB);
}
    
MorphoFrameSynthetizerFftObj::~MorphoFrameSynthetizerFftObj()
{
    delete mMorphoFrameSynth;
}
    
void
MorphoFrameSynthetizerFftObj::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    mMorphoFrameSynth->Reset(mSampleRate);
}

void
MorphoFrameSynthetizerFftObj::
ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                 const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> &fftSamples0 = mTmpBuf0;
    fftSamples0 = *ioBuffer;
    
    // Take half of the complexes
    WDL_TypedBuf<WDL_FFT_COMPLEX> &fftSamples = mTmpBuf1;
    BLUtils::TakeHalf(fftSamples0, &fftSamples);

    // Need to compute magns and phases here for later mMorphoFrameAna->SetInputData()
    WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf3;
    BLUtilsComp::ComplexToMagnPhase(&magns, &phases, fftSamples);

    // Get current frame
    MorphoFrame7 frame;
    mMorphoFrameSynth->GetMorphoFrame(&frame);
    
    // Get and apply the noise envelope
    WDL_TypedBuf<BL_FLOAT> &noise = mTmpBuf4;
    frame.GetDenormNoiseEnvelope(&noise);

#if 0
    BL_FLOAT noiseCoeff = frame.GetNoiseFactor();
    BLUtils::MultValues(&noise, noiseCoeff);
#endif
    
    magns = noise;
    
    // For noise envelope
    BLUtilsComp::MagnPhaseToComplex(&fftSamples, magns, phases);
    BLUtilsFft::FillSecondFftHalf(fftSamples, ioBuffer);
}

void
MorphoFrameSynthetizerFftObj::ProcessSamplesPost(WDL_TypedBuf<BL_FLOAT> *ioBuffer)
{    
    // Create a separate buffer for samples synthesis from partials
    // (because it doesn't use overlap)
    WDL_TypedBuf<BL_FLOAT> samplesBuffer;
    BLUtils::ResizeFillZeros(&samplesBuffer, ioBuffer->GetSize());
        
    // Compute the samples from partials
    mMorphoFrameSynth->ComputeSamples(&samplesBuffer);
    
#if 0
    MorphoFrame7 frame;
    mMorphoFrameSynth->GetMorphoFrame(&frame);
    BL_FLOAT noiseCoeff = frame.GetNoiseFactor();
    BL_FLOAT harmoCoeff = 1.0 - noiseCoeff*0.5;
    
    BLUtils::MultValues(&samplesBuffer, harmoCoeff);
#endif
    
    // ioBuffer may already contain noise
    BLUtils::AddValues(ioBuffer, samplesBuffer);
}
    
void
MorphoFrameSynthetizerFftObj::AddMorphoFrame(const MorphoFrame7 &frame)
{
    mMorphoFrameSynth->AddMorphoFrame(frame);
}

void
MorphoFrameSynthetizerFftObj::SetSynthMode(MorphoFrameSynth2::SynthMode mode)
{
    mMorphoFrameSynth->SetSynthMode(mode);
}

void
MorphoFrameSynthetizerFftObj::SetSynthEvenPartials(bool flag)
{
    mMorphoFrameSynth->SetSynthEvenPartials(flag);
}

void
MorphoFrameSynthetizerFftObj::SetSynthOddPartials(bool flag)
{
    mMorphoFrameSynth->SetSynthOddPartials(flag);
}
