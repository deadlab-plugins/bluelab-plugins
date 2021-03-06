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
//  FftConvolver6.cpp
//  Spatializer
//
//  Created by Pan on 20/11/17.
//
//

#include "Resampler2.h"
#include <Window.h>

#include <BLUtils.h>
#include <BLUtilsMath.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include "FftConvolver6.h"


// On Impulses, doesn't change anything !
// (and most costly when activated)
//
// On Spatializer, avoid aliasing
//
// Warning: this may override the maximum size of fft !
//

// PAD_FACTOR == 2: Non-cyclic technique, to avoid aliasing
// and to remove the need of overlap
#define PAD_FACTOR 2

// [NOT USED]
// Not working, not finished
// Use windowing to avoid ringing artifacts ("echo ghost")
#define USE_WINDOWING 0
#define USE_WINDOWING_FFT 0

// BAD
//
// Decrease less during normalization
// so the result will have almost the same volume as the input
//
// Set to 0, otherwise, on Protools, if we create a 88KHz project,
// and import a 44100Hz IR (by converting), the bounce will saturate
//
#define NORMALIZE_COEFF2 0 //1

// FIX: (Reason, Mac, clicks at startup)
// Avoid processing WDL_fft if data is all zero
// (would make problems)
#define FIX_FFT_ZERO 1


FftConvolver6::FftConvolver6(int bufferSize, bool normalize,
                             bool usePadFactor, bool normalizeResponses)
{
    mBufferSize = bufferSize;
    
    mNormalize = normalize;
    mUsePadFactor = usePadFactor;
    mNormalizeResponses = normalizeResponses;

#if !FIX_LATENCY
    mShift = mBufferSize;
    mNumResultReady = 0;
#endif

#if 0 //FIX_LATENCY
    // Latency fix
    mBufWasFull = false;
    mTotalInSamples = 0;
#endif
    
#if FIX_LATENCY
    // Latency fix 2
    mLatency = mBufferSize;
    mCurrentLatency = mLatency;
#endif
    
    Reset();
}

FftConvolver6::~FftConvolver6() {}

void
FftConvolver6::Init()
{
    // Init WDL FFT
    WDL_fft_init();
}

#if FIX_LATENCY
void
FftConvolver6::SetLatency(int latency)
{
    mLatency = latency;
    mCurrentLatency = mLatency;
}
#endif

void
FftConvolver6::Reset()
{
    //mSamplesBuf.Resize(0);
    mSamplesBuf.Clear();
    //mResultBuf.Resize(0);
    mResultBuf.Clear();
    
    //
    mPadFftResponse.Resize(0);
    mPadSampleResponse.Resize(0);;
    
    //mInit = true;
    
#if !FIX_LATENCY
    mOffsetResult = 0;
    mNumResultReady = 0;
#endif

#if 0//FIX_LATENCY
    // Latency fix
    mBufWasFull = false;
    mTotalInSamples = 0;
    
    mResultOut.Resize(0);
#endif
    
#if FIX_LATENCY
    // For latency fix 2
    mCurrentLatency = mLatency;
    
    //mResultOut.Resize(0);
    mResultOut.Clear();
#endif
}

void
FftConvolver6::Reset(int bufferSize)
{
    mBufferSize = bufferSize;
    
    Reset();
}

void
FftConvolver6::Flush()
{
    //mSamplesBuf.Resize(0);
    mSamplesBuf.Clear();
    //mResultBuf.Resize(0);
    mResultBuf.Clear();
    
    //BLUtils::FillAllZero(&mSamplesBuf);
    //BLUtils::FillAllZero(&mResultBuf);
    
#if FIX_LATENCY
    // For latency fix 2
    mCurrentLatency = mLatency;
    
    //mResultOut.Resize(0);
    mResultOut.Clear();
#endif

#if !FIX_LATENCY
    mOffsetResult = 0;
    mNumResultReady = 0;
#endif
    
    //mInit = true;
}

void
FftConvolver6::SetResponse(const WDL_TypedBuf<BL_FLOAT> *response)
{    
    if (response->GetSize() == 0)
        return;
    
    WDL_TypedBuf<BL_FLOAT> &copyResp = mTmpBuf2;
    copyResp = *response;
    
    ResizeImpulse(&copyResp);
    
    // As done in image analysis with normalized kernels
    //NormalizeResponse(&copyResp);
    
    mPadSampleResponse = copyResp;
    
    if (mUsePadFactor)
        // Pad the response
        BLUtils::ResizeFillZeros(&mPadSampleResponse, mPadSampleResponse.GetSize()*PAD_FACTOR);
    
    WDL_TypedBuf<BL_FLOAT> &respForFft = mTmpBuf3;
    respForFft = mPadSampleResponse;
    
#if USE_WINDOWING
    if (mWindow.GetSize() != respForFft.GetSize())
    {
        BL_FLOAT hanningFactor = 1.0;
        Window::MakeHanningPow(respForFft.GetSize(), hanningFactor, &mWindow);
    }
    
    Window::Apply(mWindow, &respForFft);
#endif

    // Compute fft
    ComputeFft(&respForFft, &mPadFftResponse, false);
    
#if USE_WINDOWING_FFT
    if (mWindow.GetSize() != mPadFftResponse.GetSize())
    {
        BL_FLOAT hanningFactor = 1.0;
        Window::MakeHanningPow(mPadFftResponse.GetSize(), hanningFactor, &mWindow);
    }
#endif
    
    if (mNormalizeResponses)
        NormalizeResponseFft(&mPadFftResponse);
    
    // Without that, the volume increases again after 5s with resp = 10s
#if 1
    // Reset the result buffer
    //BLUtils::FillAllZero(&mResultBuf);
    const WDL_TypedBuf<BL_FLOAT> &zero = mTmpBuf0;
    mTmpBuf0.Resize(mResultBuf.Available());
    BLUtils::FillAllZero(&mTmpBuf0);
    mResultBuf.SetFromBuf(0, mTmpBuf0.Get(), mTmpBuf0.GetSize());
#endif
}

void
FftConvolver6::GetResponse(WDL_TypedBuf<BL_FLOAT> *response)
{
    *response = mPadSampleResponse;
    
if (mUsePadFactor)
    // "unpad"
    response->Resize(response->GetSize()/PAD_FACTOR);
}

#if !FIX_LATENCY 
// Original method
void
FftConvolver6::ProcessOneBuffer(//const WDL_TypedBuf<BL_FLOAT> &samplesBuf,
                                //const WDL_TypedBuf<BL_FLOAT> *ioResultBuf,
                                const WDL_TypedFastQueue<BL_FLOAT> &samplesBuf,
                                const WDL_TypedFastQueue<BL_FLOAT> *ioResultBuf,
                                int offsetSamples, int offsetResult)
{
    /*padBuf.Resize(mBufferSize);
    for (int i = 0; i < mBufferSize; i++)
    {
    if (i + offsetSamples >= samplesBuf.GetSize())
    break;
    
    padBuf.Get()[i] = samplesBuf.Get()[i + offsetSamples];
    }*/
    
    WDL_TypedBuf<BL_FLOAT> &padBuf0 = mTmpBuf4;
    BLUtils::FastQueueToBuf(samplesBuf, &padBuf0, mBufferSize);
    
    // Take the size of the response if greater than buffer size
    int newSize;
    if (mUsePadFactor)
        newSize = MAX(mBufferSize*PAD_FACTOR, mPadFftResponse.GetSize());
    else
        newSize = MAX(mBufferSize, mPadFftResponse.GetSize());

    //BLUtils::ResizeFillZeros(&padBuf, newSize);
    
    WDL_TypedBuf<BL_FLOAT> *padBuf = &mTmpBuf19;
    padBuf->Resize(newSize);
    BLUtils::FillAllZero(padBuf);
    BLUtils::CopyBuf(padBuf, padBuf0);
    
#define DEBUG_BUFS 0
#if DEBUG_BUFS
    static int count = 0;
    count++;
    
    if (count == 1)
        BLDebug::DumpData("buf0.txt", padBuf);
#endif
    
#if USE_WINDOWING
    Window::Apply(mWindow, padBuf);
#endif
    
    WDL_TypedBuf<WDL_FFT_COMPLEX> &fftBuf = mTmpBuf5;
    ComputeFft(padBuf, &fftBuf, mNormalize);
    
#if DEBUG_BUFS
    if (count == 1)
        BLDebug::DumpData("resp.txt", mPadSampleResponse);
#endif
    
    // Apply modifications of the buffer
    ProcessFftBuffer(&fftBuf, mPadFftResponse);
    
    ComputeInverseFft(&fftBuf, padBuf);
    
#if DEBUG_BUFS
    if (count == 1)
        BLDebug::DumpData("buf1.txt", padBuf);
#endif

    WDL_TypedBuf<BL_FLOAT> &resultBuf = mTmpBuf20;

    BLUtils::FastQueueToBuf(*ioResultBuf, offsetResult,
                            resultBuf, padBuf->GetSize());
    
    BLUtils::AddValues(&resultBuf, *padBuf);

    ioResultBuf->SetFromBuf(offsetResult, resultBuf.Get(), resultBuf.GetSize());
    
    /*for (int i = 0; i < padBuf.GetSize(); i++)        
      {
      if (i + offsetResult >= ioResultBuf->GetSize())
      break;
      
      ioResultBuf->Get()[i + offsetResult] += padBuf.Get()[i];
      }*/
}
#endif

#if FIX_LATENCY
// Method with latency correctly fixed
void
FftConvolver6::ProcessOneBuffer(const WDL_TypedFastQueue<BL_FLOAT> &samplesBuf,
                                WDL_TypedFastQueue<BL_FLOAT> *ioResultBuf)
{
    WDL_TypedBuf<BL_FLOAT> &padBuf0 = mTmpBuf6;
    
    /*for (int i = 0; i < mBufferSize; i++)
      {
      if (i >= samplesBuf.GetSize())
      break;
      
      padBuf.Get()[i] = samplesBuf.Get()[i];
      }*/

    BLUtils::FastQueueToBuf(samplesBuf, &padBuf0, mBufferSize);
    
    // Take the size of the response if greater than buffer size
    int newSize;
    if (mUsePadFactor)
        newSize = MAX(mBufferSize*PAD_FACTOR, mPadFftResponse.GetSize());
    else
        newSize = MAX(mBufferSize, mPadFftResponse.GetSize());
    
    //BLUtils::ResizeFillZeros(&padBuf, newSize);
    WDL_TypedBuf<BL_FLOAT> *padBuf = &mTmpBuf21;
    padBuf->Resize(newSize);
    BLUtils::FillAllZero(padBuf);
    BLUtils::CopyBuf(padBuf, padBuf0);
    
#if USE_WINDOWING
    Window::Apply(mWindow, padBuf);
#endif
    
    WDL_TypedBuf<WDL_FFT_COMPLEX> &fftBuf = mTmpBuf7;
    ComputeFft(padBuf, &fftBuf, mNormalize);
    
    // Apply modifications of the buffer
    ProcessFftBuffer(&fftBuf, mPadFftResponse);
    
    ComputeInverseFft(&fftBuf, padBuf);
    
    /*for (int i = 0; i < padBuf.GetSize(); i++)
      {
      if (i >= ioResultBuf->GetSize())
      break;
      
      ioResultBuf->Get()[i] += padBuf.Get()[i];
      }*/
    WDL_TypedBuf<BL_FLOAT> &resultBuf = mTmpBuf22;
    BLUtils::FastQueueToBuf(*ioResultBuf, &resultBuf, padBuf->GetSize());

    BLUtils::AddValues(&resultBuf, *padBuf);
    
    ioResultBuf->SetFromBuf(0, resultBuf.Get(), resultBuf.GetSize());
}
#endif

// Original method (buggy)
// BUG: When buffer size is too small (447), there are crackles in the result.
#if 0
bool
FftConvolver6::Process(BL_FLOAT *input, BL_FLOAT *output, int nFrames)
{    
    // Add the new samples
    mSamplesBuf.Add(input, nFrames);
    
    // Test if we have enough result sample to fill nFrames
    if (mNumResultReady >= nFrames)
    {
        if (output != NULL)
        // Copy the result
        {
            //if (mResultBuf.GetSize() >= nFrames)
            //    memcpy(output, mResultBuf.Get(), nFrames*sizeof(BL_FLOAT));
            if (mResultBuf.Available() >= nFrames)
            {
                mResultBuf.GetToBuf(0, output, nFrames);
            }
            // else there is an error somewhere !
        }
            
        // Consume the result already managed
        BLUtils::ConsumeLeft(&mResultBuf, nFrames);
        mNumResultReady -= nFrames;
            
        return true;
    }

    // Here, we have not yet enough result samples
    
    //if (mSamplesBuf.GetSize() < mBufferSize)
    if (mSamplesBuf.Available() < mBufferSize)
    {
        // And here, we have not yet enough samples to compute a result
        
        // Fill with zeros
        if (output != NULL)
            memset(output, 0, nFrames*sizeof(BL_FLOAT));
        
        return false;
    }
    
    // Fill with zero the future working area
    //BLUtils::AddZeros(&mResultBuf, mBufferSize);
    mResultBuf.Add(0, mBufferSize);

    int size = (mBufferSize > nFrames) ? mBufferSize : nFrames;
    
    // Even bigger if the response is bigger than the samples buffer
    int newSize = MAX(size, mPadFftResponse.GetSize());
    
    // Non-cyclic convolution !
    // We need more room !
    if (mUsePadFactor)
    {
        //if (mResultBuf.GetSize() < newSize*PAD_FACTOR)
        //    BLUtils::AddZeros(&mResultBuf, newSize*PAD_FACTOR);
        if (mResultBuf.Available() < newSize*PAD_FACTOR)
            mResultBuf.Add(0, newSize*PAD_FACTOR);
    }
    else
    {
        //if (mResultBuf.GetSize() < newSize)
        //    BLUtils::AddZeros(&mResultBuf, newSize);
        if (mResultBuf.Available() < newSize)
            mResultBuf.Add(0, newSize);
    }
    
    int offsetSamples = 0;
    int numProcessed = 0;
    //while (offsetSamples + mBufferSize <= mSamplesBuf.GetSize())
    while (offsetSamples + mBufferSize <= mSamplesBuf.Available())
    {
        ProcessOneBuffer(mSamplesBuf, &mResultBuf, offsetSamples, mOffsetResult);
        
        numProcessed += mShift;
        
        // Shift the offsets
        offsetSamples += mShift;
        mOffsetResult += mShift;
        
        // Stop if it remains too few samples to process
    }
    
    mNumResultReady += numProcessed;
    
    // Consume the processed samples
    BLUtils::ConsumeLeft(&mSamplesBuf, numProcessed);
    
    // We wait for having nFrames samples ready
    if (numProcessed < nFrames)
    {
        if (output != NULL)
            memset(output, 0, nFrames*sizeof(BL_FLOAT));
        
        return false;
    }
    
    // If we have enough result...
    
    // Copy the result
    if (output != NULL)
    {
        //if (mResultBuf.GetSize() >= nFrames)
        //    memcpy(output, mResultBuf.Get(), nFrames*sizeof(BL_FLOAT));
        if (mResultBuf.Available() >= nFrames)
            mResultBuf.GetToBuf(0, output, nFrames);
        // else there is an error somewhere
    }
    
    // Consume the result already managed
    BLUtils::ConsumeLeft(&mResultBuf, nFrames);
    
    mNumResultReady -= nFrames;
    
    // Offset is in the interval [0, mBufferSize]
    //mOffsetResult = mResultBuf.GetSize() % mBufferSize;
    mOffsetResult = mResultBuf.Available() % mBufferSize;
    
    return true;
}
#endif

#if !FIX_LATENCY
      // Worked, but made time shift with host buffer size 447 e.g
      // mOffsetResult is hard to understand

// More simple method (and fixed !)
// FIX: when buffer size is too small (447), there were crackles in the result.
bool
FftConvolver6::Process(BL_FLOAT *input, BL_FLOAT *output, int nFrames)
{
    // Add the new samples
    mSamplesBuf.Add(input, nFrames);
    
    // Fill with zero the future working area
    //BLUtils::AddZeros(&mResultBuf, mBufferSize);
    
    int size = (mBufferSize > nFrames) ? mBufferSize : nFrames;
    
    // Even bigger if the response is bigger than the samples buffer
    int newSize = MAX(size, mPadFftResponse.GetSize());
    
    // Non-cyclic convolution !
    // We need more room !
    if (mUsePadFactor)
    {
        //if (mResultBuf.GetSize() < newSize*PAD_FACTOR)
        //    BLUtils::AddZeros(&mResultBuf, newSize*PAD_FACTOR);
        if (mResultBuf.Available() < newSize*PAD_FACTOR)
            mResultBuf.Add(0, newSize*PAD_FACTOR);
    }
    else
    {
        //if (mResultBuf.GetSize() < newSize)
        //    BLUtils::AddZeros(&mResultBuf, newSize);
        if (mResultBuf.Available() < newSize)
            mResultBuf.Add(0, newSize);
    }
    
    int offsetSamples = 0;
    int numProcessed = 0;
    //while (offsetSamples + mBufferSize <= mSamplesBuf.GetSize())
    while (offsetSamples + mBufferSize <= mSamplesBuf.Available())
    {
        ProcessOneBuffer(mSamplesBuf, &mResultBuf, offsetSamples, mOffsetResult);
        
        numProcessed += mShift;
        
        // Shift the offsets
        offsetSamples += mShift;
        mOffsetResult += mShift;
        
        // Stop if it remains too few samples to process
    }
    
    mNumResultReady += numProcessed;
    
    // Consume the processed samples
    BLUtils::ConsumeLeft(&mSamplesBuf, numProcessed);
    
    // Test if we have enough result sample to fill nFrames
    if (mNumResultReady < nFrames)
        return false;
    
    // Here, we have enough redy result samples to fill the output buffer
    if (output != NULL)
    // Copy the result
    {
        //if (mResultBuf.GetSize() >= nFrames)
        //    memcpy(output, mResultBuf.Get(), nFrames*sizeof(BL_FLOAT));
        if (mResultBuf.Available() >= nFrames)
            mResultBuf.GetToBuf(0, output, nFrames);
        // else there is an error somewhere !
    }
        
    // Consume the result already managed
    BLUtils::ConsumeLeft(&mResultBuf, nFrames);
    mNumResultReady -= nFrames;
    
    // Offset is in the interval [0, mBufferSize]
    //mOffsetResult = mResultBuf.GetSize() % mBufferSize;
    mOffsetResult = mResultBuf.Available() % mBufferSize;
    
    return true;
}
#endif

#if FIX_LATENCY
// Even more simple method
bool
FftConvolver6::Process(BL_FLOAT *input, BL_FLOAT *output, int nFrames)
{
    // Add the new samples
    mSamplesBuf.Add(input, nFrames);
    
#if 0 // Disabled, for latency fix 2
    // For latency fix
    mTotalInSamples += nFrames;
    
    // Added for FftConvolver6/FIX_LATENCY
    //
    // If mTotalInSamples is exactly mBufferSize, and
    // nFrames is mBufferSize, we must skip once, to take care of
    // declared plugin latency
    //
    // (otherwise, with all at 1024, we would have the result directly,
    // not corresponding to the annouced latency of the plugin)
    //
    if (mTotalInSamples <= mBufferSize)
    {
        // For the moment, keep accumulating, to manage latency well
        if (output != NULL)
        {
            memset(output, 0, nFrames*sizeof(BL_FLOAT));
        }
        
        return false;
    }
#endif
    
    int size = (mBufferSize > nFrames) ? mBufferSize : nFrames;
    
    // Even bigger if the response is bigger than the samples buffer
    int newSize = MAX(size, mPadFftResponse.GetSize());
    
    // Non-cyclic convolution !
    // We need more room !
    if (mUsePadFactor)
    {
        //if (mResultBuf.GetSize() < newSize*PAD_FACTOR)
        //    BLUtils::AddZeros(&mResultBuf, newSize*PAD_FACTOR);
        if (mResultBuf.Available() < newSize*PAD_FACTOR)
            mResultBuf.Add(0, newSize*PAD_FACTOR);
    }
    else
    {
        //if (mResultBuf.GetSize() < newSize)
        //    BLUtils::AddZeros(&mResultBuf, newSize);
        if (mResultBuf.Available() < newSize)
            mResultBuf.Add(0, newSize);
    }
    
    // BUG:
    // Crackles: Spatializer, block size 1024, 44100Hz, width=100%
    //while (mSamplesBuf.GetSize() >/*=*/ mBufferSize)
    
    // FIX:
    // No crackles
    //while (mSamplesBuf.GetSize() > mBufferSize)
    while (mSamplesBuf.Available() > mBufferSize)
    {
        // Version for FIX_LATENCY
        ProcessOneBuffer(mSamplesBuf, &mResultBuf);

        WDL_TypedBuf<BL_FLOAT> &buf = mTmpBuf1;
        buf.Resize(mBufferSize);
        mResultBuf.GetToBuf(0, buf.Get(), mBufferSize);
        
        //mResultOut.Add(mResultBuf.Get(), mBufferSize);
        mResultOut.Add(buf.Get(), buf.GetSize());
        
        // Consume the processed samples
        BLUtils::ConsumeLeft(&mSamplesBuf, mBufferSize);
        
        BLUtils::ConsumeLeft(&mResultBuf, mBufferSize);
        
        // Stop if it remains too few samples to process
    }
    
    WDL_TypedBuf<BL_FLOAT> &resultBuf = mTmpBuf8;
    bool res = GetResult(&resultBuf, nFrames);
    
    if (res)
    {
        memcpy(output, resultBuf.Get(), nFrames*sizeof(BL_FLOAT));
    }
    else
        // Result is not ready, fill the output with zeros
        // (to avoid undefined numbers sent to the host)
        //
        // NOTE: added for Rebalance
        //
    {
        if (output != NULL)
        {
            BLUtils::FillAllZero(output, nFrames);
        }
    }
    
    return res;
}
#endif

bool
FftConvolver6::Process(const WDL_TypedBuf<BL_FLOAT> &inMagns,
                       const WDL_TypedBuf<BL_FLOAT> &inPhases,
                       WDL_TypedBuf<BL_FLOAT> *resultMagns,
                       WDL_TypedBuf<BL_FLOAT> *resultPhases)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> &ioBuffer = mTmpBuf9;
    BLUtilsComp::MagnPhaseToComplex(&ioBuffer, inMagns, inPhases);
    
    ProcessFftBuffer2(&ioBuffer, mPadFftResponse);
    
    BLUtilsComp::ComplexToMagnPhase(resultMagns, resultPhases, ioBuffer);
    
    return true;
}

#if 0 //FIX_LATENCY
// From FftProcessObj15 (exactly the same code)

// Problem: with block size > buffer size, there is still a shift
// (and the code is not very clear)
bool
FftConvolver6::GetResult(WDL_TypedBuf<BL_FLOAT> *output, int numRequested)
{
    // Get the computed result
    //int numOutSamples = mResultOut.GetSize();
    int numOutSamples = mResultOut.Available();
    
    // Latency fix
    //
    // Special case:
    // - first time we have enough data
    // - and nFrames <= BUFFER_SIZE
    //
    // NOTE: for nFrames > BUFFER_SIZE, we will have latency, impossible to fix
    //
    if (!mBufWasFull &&
        (numRequested < mBufferSize))
    {
        if (numOutSamples == 0)
            // Nothing processed yet
        {
            // For the moment, keep accumulating, to manage latency well
            if (output != NULL)
            {
                BLUtils::ResizeFillZeros(output, numRequested);
            }
            
            return false;
        }
        
        // FIX: fixes auval crash
        if (mTotalInSamples < mBufferSize)
        {
            // For the moment, keep accumulating, to manage latency well
            if (output != NULL)
            {
                BLUtils::ResizeFillZeros(output, numRequested);
            }
            
            return false;
        }
        
        // We just have computed something
        //
        // This means we have got more than 2048 samples in input
        // We have reached the latency limit (2048), so we need to start outputing
        //
        // Take care of returning only a part of the result
        // to manage latency well
        //
        
        // Compute the number of samples to send to output
        int remaining = mTotalInSamples - mBufferSize;
        
        if (remaining > numRequested)
            // Just in case
            remaining = numRequested;
        
        // Compute the starting buffer position where to output samples
        int numZeros = numRequested - remaining;
        
        // get only the necessary samples
        WDL_TypedBuf<BL_FLOAT> &buf = mTmpBuf10;
        GetResultOutBuffer(&buf, remaining);
        
        if (output != NULL)
        {
            output->Resize(numRequested);
            BLUtils::FillAllZero(output);
            
            // Write to output, the correct number of samples,
            // and at the correct position
            for (int i = 0; i < remaining; i++)
            {
                output->Get()[numZeros + i] = buf.Get()[i];
            }
        }
        
        mBufWasFull = true;
        
        return true;
    }
    
    // We have enough out data to provide
    //
    if (numRequested <= numOutSamples)
    {
        if (output != NULL)
        {
            GetResultOutBuffer(output, numRequested);
        }
        
        return true;
    }
    
    // We don't have enough data yet
    if (output != NULL)
    {
        BLUtils::ResizeFillZeros(output, numRequested);
    }
    
    return false;
}
#endif

#if FIX_LATENCY
// Latency fix 2
//
// More clear fix
// (and manages block size > buffer size)
bool
FftConvolver6::GetResult(WDL_TypedBuf<BL_FLOAT> *output, int numRequested)
{
    // Get the computed result
    //int numOutSamples = mResultOut.GetSize();
    int numOutSamples = mResultOut.Available();
    
    // We have big latency, output only zeros
    if (mCurrentLatency >= numRequested)
    {
        // outputs zeros
        if (output != NULL)
        {
            output->Resize(numRequested);
            BLUtils::FillAllZero(output);
        }
        
        mCurrentLatency -= numRequested;
        if (mCurrentLatency < 0)
            mCurrentLatency = 0;
        
        return false;
    }
    
    // We have no latency, output result normally
    if (mCurrentLatency == 0)
    {
        if (numRequested <= numOutSamples) // just in case
        {
            WDL_TypedBuf<BL_FLOAT> &buf = mTmpBuf11;
            GetResultOutBuffer(&buf, numRequested);
            
            if (output != NULL)
            {
                *output = buf;
            }
            
            return true;
        }
    }
    
    // We have medium latency, outputs zeros and a part of the result
    
    int numRequestedLat = numRequested - mCurrentLatency;
    
    // Just in cases
    if (numRequestedLat < 0)
        numRequestedLat = 0;
    
    if (output != NULL)
    {
        output->Resize(numRequested);
        BLUtils::FillAllZero(output);
        
        if (numRequestedLat <= numOutSamples) // Just in case
        {
            // get only the necessary samples
            WDL_TypedBuf<BL_FLOAT> &buf = mTmpBuf12;
            GetResultOutBuffer(&buf, numRequestedLat);
            
            if (output != NULL)
            {
                // Write to output, the correct number of samples,
                // and at the correct position
                for (int i = 0; i < numRequestedLat; i++)
                {
                    output->Get()[mCurrentLatency + i] = buf.Get()[i];
                }
            }
            
            mCurrentLatency = 0;
            
            return true;
        }
    }
    
    return false;
}
#endif

#if FIX_LATENCY
// From FftProcessObj15 (exactly the same code)
void
FftConvolver6::GetResultOutBuffer(WDL_TypedBuf<BL_FLOAT> *output,
                                  int numRequested)
{
    //int size = mResultOut.GetSize();
    int size = mResultOut.Available();
    
    // Should not happen
    if (size < numRequested)
        return;
    
    //
    output->Resize(numRequested);
    
    // Copy the result
    //memcpy(output->Get(), mResultOut.Get(), numRequested*sizeof(BL_FLOAT));
    mResultOut.GetToBuf(0, output->Get(), numRequested);
    
    // Resize down, skipping left
    BLUtils::ConsumeLeft(&mResultOut, numRequested);
}

#endif

void
FftConvolver6::ResampleImpulse(WDL_TypedBuf<BL_FLOAT> *impulseResponse,
                               BL_FLOAT sampleRate, BL_FLOAT respSampleRate)
{
    if (impulseResponse->GetSize() == 0)
        return;
    
    if (respSampleRate != sampleRate)
        // We have to resample the impulse
    {
        if (impulseResponse->GetSize() > 0)
        {
            Resampler2 resampler(sampleRate, respSampleRate);

            WDL_TypedBuf<BL_FLOAT> newImpulse;
            resampler.Resample(impulseResponse, &newImpulse);
            
            ResizeImpulse(&newImpulse);
            
            *impulseResponse = newImpulse;
        }
    }
}

void
FftConvolver6::ResampleImpulse2(WDL_TypedBuf<BL_FLOAT> *impulseResponse,
                                BL_FLOAT sampleRate, BL_FLOAT respSampleRate,
                                bool resizeToNextPowerOfTwo)
{
    if (impulseResponse->GetSize() == 0)
        return;
    
    if (respSampleRate != sampleRate)
        // We have to resample the impulse
    {
        if (impulseResponse->GetSize() > 0)
        {
            Resampler2 resampler(sampleRate, respSampleRate);
            
            WDL_TypedBuf<BL_FLOAT> newImpulse;
            resampler.Resample2(impulseResponse, &newImpulse);
            
            if (resizeToNextPowerOfTwo)
                ResizeImpulse(&newImpulse);
            
            *impulseResponse = newImpulse;
        }
    }
}

void
FftConvolver6::ResizeImpulse(WDL_TypedBuf<BL_FLOAT> *impulseResponse)
{
    int respSize = impulseResponse->GetSize();
    int newSize = BLUtilsMath::NextPowerOfTwo(respSize);
    int diff = newSize - impulseResponse->GetSize();
        
    impulseResponse->Resize(newSize);
        
    // Fill with zeros if we have grown
    for (int j = 0; j < diff; j++)
    {
        int index = newSize - j - 1;
        if (index < 0)
            continue;
        if (index > impulseResponse->GetSize())
            continue;
        
        impulseResponse->Get()[index] = 0.0;
    }
}

void
FftConvolver6::ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer0,
                                const WDL_TypedBuf<WDL_FFT_COMPLEX> &response)
{
    //BLUtils::TakeHalf(ioBuffer);
    WDL_TypedBuf<WDL_FFT_COMPLEX> &ioBuffer = mTmpBuf23;
    BLUtils::TakeHalf(*ioBuffer0, &ioBuffer);
    
    for (int i = 0; i < ioBuffer.GetSize(); i++)
    {
        if (i >= ioBuffer.GetSize())
            break;
        WDL_FFT_COMPLEX sigComp = ioBuffer.Get()[i];
        
        if (i >= response.GetSize())
            break;
        WDL_FFT_COMPLEX respComp = response.Get()[i];
        
        // Pointwise multiplication of two complex
        WDL_FFT_COMPLEX res;
        res.re = sigComp.re*respComp.re - sigComp.im*respComp.im;
        res.im = sigComp.im*respComp.re + sigComp.re*respComp.im;
        
        ioBuffer.Get()[i] = res;
    }
    
    //BLUtils::ResizeFillZeros(ioBuffer, ioBuffer->GetSize()*2);
    BLUtils::SetBuf(ioBuffer0, ioBuffer);
    
    BLUtilsFft::FillSecondFftHalf(ioBuffer0);
}

// For using with already frequential signal
void
FftConvolver6::ProcessFftBuffer2(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                                 const WDL_TypedBuf<WDL_FFT_COMPLEX> &response)
{
    for (int i = 0; i < ioBuffer->GetSize(); i++)
    {
        if (i >= ioBuffer->GetSize())
            break;
        WDL_FFT_COMPLEX sigComp = ioBuffer->Get()[i];
        
        if (i >= response.GetSize())
            break;
        WDL_FFT_COMPLEX respComp = response.Get()[i];
        
        // Pointwise multiplication of two complex
        WDL_FFT_COMPLEX res;
        res.re = sigComp.re*respComp.re - sigComp.im*respComp.im;
        res.im = sigComp.im*respComp.re + sigComp.re*respComp.im;
        
        ioBuffer->Get()[i] = res;
    }
}

void
FftConvolver6::ComputeFft(const WDL_TypedBuf<BL_FLOAT> *samples,
                          WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples,
                          bool normalize)
{
    if (fftSamples->GetSize() != samples->GetSize())
        fftSamples->Resize(samples->GetSize());
    
    BL_FLOAT normCoeff = 1.0;
    if (normalize)
        normCoeff = 1.0/fftSamples->GetSize();
    
    // Fill the fft buf
    for (int i = 0; i < fftSamples->GetSize(); i++)
    {
        // No need to divide by mBufferSize if we ponderate analysis hanning window
        fftSamples->Get()[i].re = normCoeff*samples->Get()[i];
        fftSamples->Get()[i].im = 0.0;
    }
    
#if FIX_FFT_ZERO
    if (BLUtils::IsAllZero(*samples))
    {
        BLUtils::FillAllZero(fftSamples);
        
        return;
    }
#endif
    
    // Do the fft
    // Do it on the window but also in following the empty space, to capture remaining waves
    WDL_fft(fftSamples->Get(), fftSamples->GetSize(), false);
    
    // Sort the fft buffer
    WDL_TypedBuf<WDL_FFT_COMPLEX> &tmpFftBuf = mTmpBuf15;
    tmpFftBuf = *fftSamples;
    
    for (int i = 0; i < fftSamples->GetSize(); i++)
    {
        int k = WDL_fft_permute(fftSamples->GetSize(), i);
        
        // Error somewhere...
        if ((k < 0) || (k >= tmpFftBuf.GetSize()))
            break;
        
        fftSamples->Get()[i].re = tmpFftBuf.Get()[k].re;
        fftSamples->Get()[i].im = tmpFftBuf.Get()[k].im;
    }
}

void
FftConvolver6::ComputeInverseFft(const WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples,
                                 WDL_TypedBuf<BL_FLOAT> *samples)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> &tmpFftBuf = mTmpBuf16;
    tmpFftBuf = *fftSamples;
    
    for (int i = 0; i < fftSamples->GetSize(); i++)
    {
        int k = WDL_fft_permute(fftSamples->GetSize(), i);
        
        // Error somewhere...
        if ((k < 0) || (k >= tmpFftBuf.GetSize()))
            break;
        
        fftSamples->Get()[k].re = tmpFftBuf.Get()[i].re;
        fftSamples->Get()[k].im = tmpFftBuf.Get()[i].im;
    }
    
    // Should not do this step when not necessary (for example for transients)
    
#if FIX_FFT_ZERO
    if (BLUtilsComp::IsAllZeroComp(*fftSamples))
    {
        BLUtils::FillAllZero(samples);
        
        return;
    }
#endif

    // Do the ifft
    WDL_fft(fftSamples->Get(), fftSamples->GetSize(), true);
    
    for (int i = 0; i < fftSamples->GetSize(); i++)
        samples->Get()[i] = fftSamples->Get()[i].re;
}

void
FftConvolver6::NormalizeResponseFft(WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples)
{
    // As done in image analysis with normalized kernels
    // Multiply by a coefficient so the sum becomes one
    
    // We can normalize the response as samples or as fft, this is the same
    // as fft is conservative for multiplication by a factor
    
    WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf17;
    WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf18;
    BLUtilsComp::ComplexToMagnPhase(&magns, &phases, *fftSamples);
    
    // Take sum(samples) and not sum(abs(samples)) !
    BL_FLOAT sum = BLUtils::ComputeSum(magns);
    
    int fftSize = fftSamples->GetSize();
    
#if !NORMALIZE_COEFF2
    BL_FLOAT coeff = 0.0;
    if (sum > 0.0)
        coeff = 0.125*fftSize/sum;
#endif
    
#if NORMALIZE_COEFF2
    // Coeff 0.5 seems good after fixes in Impulses
    // (gain factor depending on sample rate, and squared gain factor
    BL_FLOAT coeff = 1.0;
    if (sum > 0.0)
        coeff = 0.5*fftSize/sum;
#endif
    
    BLUtils::MultValues(&magns, coeff);
    
    BLUtilsComp::MagnPhaseToComplex(fftSamples, magns, phases);
}

