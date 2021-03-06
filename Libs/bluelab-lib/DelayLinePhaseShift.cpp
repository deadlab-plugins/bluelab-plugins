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
//  DelayLinePhaseShift.cpp
//  BL-Bat
//
//  Created by applematuer on 12/14/19.
//
//

#include <BLUtils.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>

// For DBG_Test
#include <FftProcessObj16.h>

#include "DelayLinePhaseShift.h"

#define FIX_CLICK_FROM_ZERO 1


DelayLinePhaseShift::DelayLinePhaseShift(BL_FLOAT sampleRate, int bufferSize, int binNum,
                                         BL_FLOAT maxDelay, BL_FLOAT delay)
{
    mSampleRate = sampleRate;
    mBufferSize = bufferSize;
    
    mBinNum = binNum;
    
    mMaxDelay = maxDelay;
    mDelay = delay;
    
    Init();
}

DelayLinePhaseShift::~DelayLinePhaseShift() {}

void
DelayLinePhaseShift::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    Init();
}

WDL_FFT_COMPLEX
DelayLinePhaseShift::ProcessSample(WDL_FFT_COMPLEX sample)
{
    BL_FLOAT magn = COMP_MAGN(sample);
    BL_FLOAT phase = COMP_PHASE(sample);
    
    phase += mDPhase;
    
    WDL_FFT_COMPLEX result;
    MAGN_PHASE_COMP(magn, phase, result);
    
    return result;
}

void
DelayLinePhaseShift::Init()
{
    BL_FLOAT delay = mDelay + mMaxDelay/2.0;
    
    if (delay < 0.0)
    {
        // Error
    }
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;
    BL_FLOAT freq = mBinNum*hzPerBin;

    //BL_FLOAT T = 1.0/freq;
    //BL_FLOAT t = delay/T;
    
    BL_FLOAT t = delay*freq;
    
    mDPhase = t*2.0*M_PI;
}

void
DelayLinePhaseShift::DBG_Test(int bufferSize, BL_FLOAT sampleRate,
                              const WDL_TypedBuf<WDL_FFT_COMPLEX> &fftSamples)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> fftSamples0 = fftSamples;
    fftSamples0.Resize(fftSamples0.GetSize()*2);
    BLUtilsFft::FillSecondFftHalf(&fftSamples0);
    
    WDL_TypedBuf<BL_FLOAT> samples0;
    FftProcessObj16::FftToSamples(fftSamples0, &samples0);
    
    BLDebug::DumpData("samples0.txt", samples0);
    
    // Create the delay lines
#define DELAY 0.0005 // 0.5ms
    
    vector<DelayLinePhaseShift *> phaseShifts;
    phaseShifts.resize(bufferSize/2.0);
    for (int i = 0; i < bufferSize/2.0; i++)
    {
        DelayLinePhaseShift *phaseShift =
            new DelayLinePhaseShift(sampleRate, bufferSize, i, 0.0, DELAY);
        phaseShifts[i] = phaseShift;
    }
    
    // Apply the delay
    WDL_TypedBuf<WDL_FFT_COMPLEX> fftSamples1;
    fftSamples1.Resize(fftSamples.GetSize());
    
    for (int i = 0; i < bufferSize/2.0; i++)
    {
        WDL_FFT_COMPLEX val = fftSamples.Get()[i];
        WDL_FFT_COMPLEX val1 = phaseShifts[i]->ProcessSample(val);
        
        fftSamples1.Get()[i] = val1;
    }
    
    // Convert to samples and dump
    fftSamples1.Resize(fftSamples1.GetSize()*2);
    BLUtilsFft::FillSecondFftHalf(&fftSamples1);
    
    WDL_TypedBuf<BL_FLOAT> samples1;
    FftProcessObj16::FftToSamples(fftSamples1, &samples1);
    
    BLDebug::DumpData("samples1.txt", samples1);
    
    // Clean
    for (int i = 0; i < bufferSize/2.0; i++)
    {
        DelayLinePhaseShift *phaseShift = phaseShifts[i];
        delete phaseShift;
    }
}
