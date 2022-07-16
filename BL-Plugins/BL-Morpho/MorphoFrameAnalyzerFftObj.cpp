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
//  StereoVizProcess5.cpp
//  BL-PitchShift
//
//  Created by Pan on 20/04/18.
//
//

#include <BLUtils.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include <BLDebug.h>

#include <MorphoFrame7.h>
#include <MorphoFrameAna2.h>

#include <PartialTracker8.h>
#include <QIFFT.h> // For empirical coeffs

#include <IdLinker.h>

#include "MorphoFrameAnalyzerFftObj.h"


MorphoFrameAnalyzerFftObj::MorphoFrameAnalyzerFftObj(int bufferSize,
                                                     BL_FLOAT overlapping,
                                                     BL_FLOAT oversampling,
                                                     BL_FLOAT sampleRate,
                                                     bool storeDetectDataInFrames)
: ProcessObj(bufferSize)
{
    mOverlapping = overlapping;
    mFreqRes = oversampling;
    
    mSampleRate = sampleRate;

    mStoreDetectDataInFrames = storeDetectDataInFrames;
    
    mPartialTracker = new PartialTracker8(bufferSize, sampleRate, overlapping);
    
    mMorphoFrameAna = new MorphoFrameAna2(bufferSize, overlapping, 1, sampleRate);
}

MorphoFrameAnalyzerFftObj::~MorphoFrameAnalyzerFftObj()
{
    delete mPartialTracker;
    delete mMorphoFrameAna;
}

void
MorphoFrameAnalyzerFftObj::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;

    mMorphoFrameAna->Reset(mBufferSize, mOverlapping, mFreqRes, sampleRate);
}

void
MorphoFrameAnalyzerFftObj::
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
    
    mPartialTracker->SetData(magns, phases);
    
    // DetectPartials
    mPartialTracker->DetectPartials();

    // Try to provide the first partials, even is they are not yet filtered
    mPartialTracker->GetRawPartials(&mCurrentRawPartials);

    //mPartialTracker->ExtractNoiseEnvelope();
    mMorphoFrameAna->SetRawPartials(mCurrentRawPartials);
 
    mPartialTracker->FilterPartials();
        
    //
    mPartialTracker->GetPreProcessedMagns(&mCurrentMagns);
            
    //
    mMorphoFrameAna->SetInputData(magns, phases); // Good for pitch detection
    mMorphoFrameAna->SetProcessedData(mCurrentMagns, phases); // Good for noise envelope
    
    if (mPartialTracker != NULL)
    {
        mPartialTracker->GetPartials(&mCurrentNormPartials);

        vector<Partial2> &denormPartials = mTmpBuf5;
        denormPartials = mCurrentNormPartials;
        mPartialTracker->DenormPartials(&denormPartials);
        
        mMorphoFrameAna->SetPartials(denormPartials);

        MorphoFrame7 &frame = mTmpBuf6;
        mMorphoFrameAna->Compute(&frame);
        
        // Get and apply the noise envelope
        WDL_TypedBuf<BL_FLOAT> &noise = mTmpBuf4;
        frame.GetNoiseEnvelope(&noise, false);
        
        mPartialTracker->DenormData(&noise);

        frame.SetDenormNoiseEnvelope(noise);

        if (mStoreDetectDataInFrames)
        {
            frame.SetInputMagns(mCurrentMagns); // magns?
            frame.SetRawPartials(mCurrentRawPartials);
            frame.SetNormPartials(mCurrentNormPartials);
        }

        // Color processed
        WDL_TypedBuf<BL_FLOAT> &colorProcessed = mTmpBuf7;
        frame.GetColorRaw(&colorProcessed);
        BL_FLOAT amplitude = frame.GetAmplitude();
        BLUtils::MultValues(&colorProcessed, amplitude);
        mPartialTracker->PreProcessDataXY(&colorProcessed);
        frame.SetColorProcessed(colorProcessed); // For displaying only

        // Warping processed
        WDL_TypedBuf<BL_FLOAT> &warpingProcessed = mTmpBuf8;
        frame.GetWarping(&warpingProcessed);
        if (mPartialTracker != NULL)
            mPartialTracker->PreProcessDataX(&warpingProcessed);
        frame.SetWarpingProcessed(warpingProcessed);
        
        mCurrentMorphoFrames.push_back(frame);
    }
    
    // No need to resynth
}

void
MorphoFrameAnalyzerFftObj::GetMorphoFrames(vector<MorphoFrame7> *frames)
{
    *frames = mCurrentMorphoFrames;
    mCurrentMorphoFrames.clear();
}

void
MorphoFrameAnalyzerFftObj::SetThreshold(BL_FLOAT threshold)
{
    // Was in %, convert it to normalized
    threshold = threshold*0.01;
    
    mPartialTracker->SetThreshold(threshold);
}

void
MorphoFrameAnalyzerFftObj::SetThreshold2(BL_FLOAT threshold2)
{
    // Was in %, convert it to normalized
    threshold2 = threshold2*0.01;
    
    mPartialTracker->SetThreshold2(threshold2);
}

void
MorphoFrameAnalyzerFftObj::SetTimeSmoothCoeff(BL_FLOAT coeff)
{
    // Was in %, convert it to normalized
    coeff = coeff*0.01;
    
    if (mPartialTracker != NULL)
        mPartialTracker->SetTimeSmoothCoeff(coeff);
}

void
MorphoFrameAnalyzerFftObj::SetTimeSmoothNoiseCoeff(BL_FLOAT coeff)
{
    if (mPartialTracker != NULL)
        mMorphoFrameAna->SetTimeSmoothNoiseCoeff(coeff);
}

void
MorphoFrameAnalyzerFftObj::SetNeriDelta(BL_FLOAT delta)
{
    if (mPartialTracker != NULL)
        mPartialTracker->SetNeriDelta(delta);
}
