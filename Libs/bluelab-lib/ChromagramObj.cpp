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
 
#include <stdlib.h>

#include <BLUtils.h>
#include <BLUtilsMath.h>

#include <Window.h>

#include <FreqAdjustObj3.h>

#include <HistoMaskLine2.h>

#include "ChromagramObj.h"

// 6 seems a bit limit for performances
// test: in debug, set shaprness to 0
// => this slows down and crackles
//#define MIN_SMOOTH_DIVISOR 6.0

#define MIN_SMOOTH_DIVISOR 12.0
#define MAX_SMOOTH_DIVISOR 48.0

ChromagramObj::ChromagramObj(int bufferSize, int oversampling,
                             int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;
    
    mATune = 440.0;
    mSharpness = 0.0;

#if USE_FREQ_OBJ
    mFreqObj = new FreqAdjustObj3(bufferSize, oversampling, freqRes, sampleRate);
#endif
}

ChromagramObj::~ChromagramObj()
{
#if USE_FREQ_OBJ
    delete mFreqObj;
#endif
}

void
ChromagramObj::Reset(int bufferSize, int oversampling,
                     int freqRes, BL_FLOAT sampleRate)
{
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;
    
#if USE_FREQ_OBJ
    mFreqObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
#endif
}

void
ChromagramObj::SetATune(BL_FLOAT aTune)
{
    mATune = aTune;
}
    
void
ChromagramObj::SetSharpness(BL_FLOAT sharpness)
{
    mSharpness = sharpness;
    
    // Force re-creating the window
    mSmoothWin.Resize(0);
}

void
ChromagramObj::MagnsToChromaLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                                 const WDL_TypedBuf<BL_FLOAT> &phases,
                                 WDL_TypedBuf<BL_FLOAT> *chromaLine,
                                 HistoMaskLine2 *maskLine)
{
#if USE_FREQ_OBJ
    // Must update the freq obj at each step
    // (otherwise the detection will be very bad if mSpeedMod != 1)
    WDL_TypedBuf<BL_FLOAT> &realFreqs = mTmpBuf0;
    mFreqObj->ComputeRealFrequencies(phases, &realFreqs);
#endif

#if !USE_FREQ_OBJ
    MagnsToChromaLine(magns, chromaLine);
#else
    MagnsToChromaLineFreqs(magns, realFreqs, chromaLine);
#endif
}

void
ChromagramObj::MagnsToChromaLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                                 WDL_TypedBuf<BL_FLOAT> *chromaLine,
                                 HistoMaskLine2 *maskLine)
{
    // Corresponding to A 440
//#define C0_TONE 16.35160
    
    BL_FLOAT c0Freq = ComputeC0Freq(mATune);
    
    BL_FLOAT toneMult = std::pow(2.0, 1.0/12.0);
    
    chromaLine->Resize(magns.GetSize());
    BLUtils::FillAllZero(chromaLine);

    int chromaLineSize = chromaLine->GetSize();
    BL_FLOAT *chromaLineBuf = chromaLine->Get();

    int magnsSize = magns.GetSize();
    BL_FLOAT *magnsBuf = magns.Get();
    
    BL_FLOAT hzPerBin = mSampleRate/mBufferSize;

    BL_FLOAT c0FreqInv = 1.0/c0Freq;
    BL_FLOAT toneMultInv = 1.0/std::log(toneMult);
    BL_FLOAT twelveInv = 1.0/12.0;
    
    // Do not take 0Hz!
    //for (int i = 1; i < magns.GetSize(); i++)
    for (int i = 1; i < magnsSize; i++)
    {
        BL_FLOAT magnVal = magnsBuf[i];
        
        BL_FLOAT freq = i*hzPerBin;
        
        // See: https://pages.mtu.edu/~suits/NoteFreqCalcs.html
        //BL_FLOAT fRatio = freq / c0Freq;
        BL_FLOAT fRatio = freq*c0FreqInv;
        //BL_FLOAT tone = std::log(fRatio)/std::log(toneMult);
        BL_FLOAT tone = std::log(fRatio)*toneMultInv;
        
        // Shift by one (strange...)
        tone += 1.0;
        
        // Adjust to the axis labels
        tone -= 1.0/12.0;
        
        tone = fmod(tone, 12.0);
        
        //BL_FLOAT toneNorm = tone/12.0;
        BL_FLOAT toneNorm = tone*twelveInv;
        
        BL_FLOAT binNumF = toneNorm*chromaLineSize;
        
        int binNum = bl_round(binNumF);
        
        if ((binNum >= 0) && (binNum < chromaLineSize))
            //chromaLine->Get()[binNum] += magnVal;
            chromaLineBuf[binNum] += magnVal;

        if (maskLine != NULL)
        {
            if ((binNum >= 0) && (binNum < chromaLineSize))
                maskLine->AddValue(binNum, i);
        }
    }
    
    // Manage sharpness
    int divisor = 12;
    divisor = (1.0 - mSharpness)*MIN_SMOOTH_DIVISOR + mSharpness*MAX_SMOOTH_DIVISOR;
    if (divisor <= 0)
        divisor = 1;
    if (divisor > chromaLine->GetSize())
        divisor = chromaLine->GetSize();
    
    // Smooth the chroma line
    if (mSmoothWin.GetSize() != chromaLine->GetSize()/divisor)
    {
        int windowSize = chromaLine->GetSize()/divisor;
        Window::MakeHanning(windowSize, &mSmoothWin);
    }
    
    WDL_TypedBuf<BL_FLOAT> &smoothLine = mTmpBuf1;
    BLUtils::SmoothDataWin(&smoothLine, *chromaLine, mSmoothWin);
    
    *chromaLine = smoothLine;
}

#if USE_FREQ_OBJ
void
ChromagramObj::MagnsToChromaLineFreqs(const WDL_TypedBuf<BL_FLOAT> &magns,
                                      const WDL_TypedBuf<BL_FLOAT> &realFreqs,
                                      WDL_TypedBuf<BL_FLOAT> *chromaLine,
                                      HistoMaskLine2 *maskLine)
{
    BL_FLOAT c0Freq = ComputeC0Freq(mATune);
    
    BL_FLOAT toneMult = std::pow(2.0, 1.0/12.0);
    
    // Optim
    BL_FLOAT logToneMult = std::log(toneMult);
    BL_FLOAT logToneMultInv = 1.0/logToneMult;
    
    chromaLine->Resize(magns.GetSize());
    BLUtils::FillAllZero(chromaLine);

    BL_FLOAT c0FreqInv = 1.0/c0Freq;
    BL_FLOAT inv12 = 1.0/12.0;
    
    int magnsSize = magns.GetSize();
    BL_FLOAT *magnsData = magns.Get();
    
    BL_FLOAT *realFreqsData = realFreqs.Get();

    int chromaLineSize = chromaLine->GetSize();
    BL_FLOAT *chromaLineBuf = chromaLine->Get();
    
    // Do not take 0Hz!
    //for (int i = 1; i < magns.GetSize(); i++)
    for (int i = 1; i < magnsSize; i++)
    {
        //BL_FLOAT magnVal = magns.Get()[i];
        //BL_FLOAT freq = realFreqs.Get()[i];
        BL_FLOAT magnVal = magnsData[i];
        BL_FLOAT freq = realFreqsData[i];
        
        // See: https://pages.mtu.edu/~suits/NoteFreqCalcs.html
        //BL_FLOAT fRatio = freq / c0Freq;
        BL_FLOAT fRatio = freq*c0FreqInv;
        
        BL_FLOAT tone = std::log(fRatio)*logToneMultInv;
        
        // Shift (strange ?)
        tone += 0.5;
        
        tone = fmod(tone, 12.0);
        
        //BL_FLOAT toneNorm = tone/12.0;
        BL_FLOAT toneNorm = tone*inv12;
        
        //BL_FLOAT binNumF = toneNorm*chromaLine->GetSize();
        BL_FLOAT binNumF = toneNorm*chromaLineSize;
        
        int binNum = bl_round(binNumF);
        
        //if ((binNum >= 0) && (binNum < chromaLine->GetSize()))
        if ((binNum >= 0) && (binNum < chromaLineSize))
            chromaLineBuf[binNum] += magnVal;

        if (maskLine != NULL)
        {
            //if ((binNum >= 0) && (binNum < chromaLine->GetSize()))
            if ((binNum >= 0) && (binNum < chromaLineSize))
                maskLine->AddValue(binNum, i);
        }
    }
    
    // Initial, leaks a lot
    int divisor = 12;
    divisor = (1.0 - mSharpness)*MIN_SMOOTH_DIVISOR + mSharpness*MAX_SMOOTH_DIVISOR;
    if (divisor <= 0)
        divisor = 1;
    if (divisor > chromaLine->GetSize())
        divisor = chromaLine->GetSize();
    
    // Smooth the chroma line
    //if (mSmoothWin.GetSize() == 0)
    if (mSmoothWin.GetSize() != chromaLine->GetSize()/divisor)
    {
        int windowSize = chromaLine->GetSize()/divisor;
        Window::MakeHanning(windowSize, &mSmoothWin);
    }
    
    // Smooth
    WDL_TypedBuf<BL_FLOAT> &smoothLine = mTmpBuf2;
    BLUtils::SmoothDataWin(&smoothLine, *chromaLine, mSmoothWin);
    *chromaLine = smoothLine;
}
#endif

BL_FLOAT
ChromagramObj::ChromaToFreq(BL_FLOAT chromaVal, BL_FLOAT minFreq) const
{    
    BL_FLOAT c0Freq = ComputeC0Freq(mATune);

    BL_FLOAT toneMult = std::pow(2.0, 1.0/12.0);

    // Inverse computation done in chromagram
    BL_FLOAT freq0 = c0Freq*exp((chromaVal*12.0 - 0.5)*log(toneMult));

    BL_FLOAT freq = freq0;
    while(freq < minFreq)
        freq *= 2.0;

    return freq;
}

BL_FLOAT
ChromagramObj::FreqToChroma(BL_FLOAT freq, BL_FLOAT aTune)
{
    BL_FLOAT c0Freq = ComputeC0Freq(aTune);

    BL_FLOAT toneMult = std::pow(2.0, 1.0/12.0);

    BL_FLOAT chromaVal = 0.5 + (1.0/12.0)*log(freq/c0Freq)/log(toneMult);

    // Bound to [0, 1]
    chromaVal = chromaVal - (int)chromaVal;
    
    return chromaVal;
}

BL_FLOAT
ChromagramObj::ComputeC0Freq(BL_FLOAT aTune)
{
    BL_FLOAT AMinus1 = aTune/32.0;
    
    BL_FLOAT toneMult = std::pow(2.0, 1.0/12.0);
    
    BL_FLOAT ASharpMinus1 = AMinus1*toneMult;
    BL_FLOAT BMinus1 = ASharpMinus1*toneMult;
    
    BL_FLOAT C0 = BMinus1*toneMult;
    
    return C0;
}
