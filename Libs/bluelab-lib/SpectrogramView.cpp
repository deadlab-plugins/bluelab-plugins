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
//  SpectrogramView.cpp
//  BL-Ghost
//
//  Created by Pan on 02/06/18.
//
//
#ifdef IGRAPHICS_NANOVG

#include <BLSpectrogram4.h>
#include <FftProcessObj16.h>
#include <Resampler2.h>
#include <BLUtils.h>

#include "SpectrogramView.h"

#define MIN_ZOOM 1.0
//#define MAX_ZOOM 20.0 // ORIGIN
//#define MAX_ZOOM 45.0
//#define MAX_ZOOM 200.0
//#define MAX_ZOOM 400.0
#define MAX_ZOOM 800.0

// Works quite well with 1
// (make some light diffrence, but really better
// than a black band !)
#define NUM_BUFFERS_PREFILL 1

// Without, we have no vertical artifact lines (FIXED)
// With, we can zoom more with real zoomed data displayed
#define USE_FLOAT_STEP 1

// Process every buffers or use steps
//
// If set to 0, we have vertical lines on some files
// (FIXED => overlapping=1)
#define FULL_STEP_PROCESS 0

// Fix when zooming a lot, the dsplay disappeared and the time axis has bugs
// (this is because the time duration was negative)
#define FIX_BIG_ZOOM 1

// Experimental, not tested
#define FIX_BIG_ZOOM2 0 //1

SpectrogramView::SpectrogramView(BLSpectrogram4 *spectrogram,
                                 FftProcessObj16 *fftObj,
                                 int maxNumCols,
                                 BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                                 BL_FLOAT sampleRate)
{
    mSpectrogram = spectrogram;
    mFftObj = fftObj;
    
    mMaxNumCols = maxNumCols;
    
    mViewBarPos = 0.5;
    
    mStartDataPos = 0.0;
    mEndDataPos = 0.0;
    
    // Warning: y is reversed
    mBounds[0] = x0;
    mBounds[1] = y0;
    mBounds[2] = x1;
    mBounds[3] = y1;
    
    mSampleRate = sampleRate;
    
    mChannels = NULL;

    mSelectionActive = false;
    for (int i = 0; i < 4; i++)
        mSelection[i] = 0.0;
    
    Reset();
}

SpectrogramView::~SpectrogramView() {}

void
SpectrogramView::Reset()
{
    mZoomFactor = 1.0;
    
    mAbsZoomFactor = 1.0;
    
    mTranslation = 0.0;
}

void
SpectrogramView::Resize(int prevWidth, int prevHeight,
                        int newWidth, int newHeight)
{
    mSelection[0] = ((BL_FLOAT)mSelection[0]*prevWidth)/newWidth;
    mSelection[1] = ((BL_FLOAT)mSelection[1]*prevHeight)/newHeight;
    mSelection[2] = ((BL_FLOAT)mSelection[2]*prevWidth)/newWidth;
    mSelection[3] = ((BL_FLOAT)mSelection[3]*prevHeight)/newHeight;
}

void
SpectrogramView::SetData(vector<WDL_TypedBuf<BL_FLOAT> > *channels)
{
    mChannels = channels;
    
    mStartDataPos = 0.0;
    mEndDataPos = 0.0;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    if (!channels->empty())
    {
        long channelSize = (*channels)[0].GetSize();
        mEndDataPos = ((BL_FLOAT)channelSize)/bufferSize;
    }
    
    //UpdateSpectrogramData();
}

void
SpectrogramView::SetViewBarPosition(BL_FLOAT pos)
{
    if ((mChannels == NULL) || mChannels->empty())
      return;
    
    mViewBarPos = pos;
}

void
SpectrogramView::SetViewSelection(BL_FLOAT x0, BL_FLOAT y0,
                                  BL_FLOAT x1, BL_FLOAT y1)
{
    mSelection[0] = (x0 - mBounds[0])/(mBounds[2] - mBounds[0]);
    mSelection[1] = (y0 - mBounds[1])/(mBounds[3] - mBounds[1]);
    mSelection[2] = (x1 - mBounds[0])/(mBounds[2] - mBounds[0]);
    mSelection[3] = (y1 - mBounds[1])/(mBounds[3] - mBounds[1]);
    
    // Hack
    // Avoid out of bounds selection(due to graph + miniview)
    // TODO: manage coordinates better
#if 1
    if (mSelection[1] < 0.0)
        mSelection[1] = 0.0;
    if (mSelection[3] > 1.0)
        mSelection[3] = 1.0;
#endif
    
    mSelectionActive = true;
}

void
SpectrogramView::GetViewSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                  BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = mSelection[0]*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y0 = mSelection[1]*(mBounds[3] - mBounds[1]) + mBounds[1]; // 3 <=> 1
    *x1 = mSelection[2]*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y1 = mSelection[3]*(mBounds[3] - mBounds[1]) + mBounds[1];
    
    // ???
    mSelectionActive = true;
}

// Not checked !
// Should do (1.0 - mBounds[3]) for offset, asjust below
void
SpectrogramView::ViewToDataRef(BL_FLOAT *x0, BL_FLOAT *y0,
                               BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = (*x0 - mBounds[0])/(mBounds[2] - mBounds[0]);
    *y0 = (*y0 - mBounds[1])/(mBounds[3] - mBounds[1]);
    *x1 = (*x1 - mBounds[0])/(mBounds[2] - mBounds[0]);
    *y1 = (*y1 - mBounds[1])/(mBounds[3] - mBounds[1]);
}

// GOOD !
void
SpectrogramView::DataToViewRef(BL_FLOAT *x0, BL_FLOAT *y0,
                               BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = *x0*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y0 = *y0*(mBounds[3] - mBounds[1]) + 1.0 - mBounds[3];
    *x1 = *x1*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y1 = *y1*(mBounds[3] - mBounds[1]) + 1.0 - mBounds[3];
}


void
SpectrogramView::ClearViewSelection()
{
    mSelectionActive = false;
}

// Not used
void
SpectrogramView::GetViewDataBounds(BL_FLOAT *startDataPos, BL_FLOAT *endDataPos)
{
    //int bufferSize = mFftObj->GetBufferSize();
    
    *startDataPos = mStartDataPos;
    *endDataPos = mEndDataPos;
}

void
SpectrogramView::GetViewDataBounds2(BL_FLOAT *startDataPos, BL_FLOAT *endDataPos,
                                    BL_FLOAT minNormX, BL_FLOAT maxNormX)
{
    if ((mChannels == NULL) || mChannels->empty())
    {
        *startDataPos = 0;
        *endDataPos = 0;
        
        return;
    }
    
    long channelSize = (*mChannels)[0].GetSize();
    if (channelSize == 0)
    {
        *startDataPos = 0;
        *endDataPos = 0;
        
        return;
    }
    
    int bufferSize = mFftObj->GetBufferSize();
    
    *startDataPos = minNormX*((BL_FLOAT)channelSize)/bufferSize;
    // NOTE: with -1, makes negative time interval if we zoom a lot
#if !FIX_BIG_ZOOM
    *endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize - 1;
#else
    *endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize;;
#endif
}

bool
SpectrogramView::GetDataSelection(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1)
{
    if (!mSelectionActive)
        return false;
    
    if ((mChannels == NULL) || mChannels->empty())
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    *x0 = mStartDataPos + mSelection[0]*(mEndDataPos - mStartDataPos);
    
    // Warning, y is reversed !
    *y0 = (1.0 - mSelection[3])*bufferSize/2.0;
    
    *x1 = mStartDataPos + mSelection[2]*(mEndDataPos - mStartDataPos);
    
    // Warning, y is reversed !
    *y1 = (1.0 - mSelection[1])*bufferSize/2.0;
    
    return true;
}

bool
SpectrogramView::GetDataSelection2(BL_FLOAT *x0, BL_FLOAT *y0,
                                   BL_FLOAT *x1, BL_FLOAT *y1,
                                   BL_FLOAT minNormX, BL_FLOAT maxNormX)
{
    if (!mSelectionActive)
        return false;
    
    if ((mChannels == NULL) || mChannels->empty())
        return false;
    
    long channelSize = (*mChannels)[0].GetSize();
    if (channelSize == 0)
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    /*long*/ BL_FLOAT startDataPos = minNormX*((BL_FLOAT)channelSize)/bufferSize;

#if !FIX_BIG_ZOOM2
    /*long*/ BL_FLOAT endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize - 1;
#else
    /*long*/ BL_FLOAT endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize;
#endif
    
    *x0 = startDataPos + mSelection[0]*(endDataPos - startDataPos + 1);
    
    // Warning, y is reversed !
    *y0 = (1.0 - mSelection[3])*bufferSize/2.0;
    
    *x1 = startDataPos + mSelection[2]*(endDataPos - startDataPos + 1);
    
    // Warning, y is reversed !
    *y1 = (1.0 - mSelection[1])*bufferSize/2.0;
    
    return true;
}

// TEST
void
SpectrogramView::SetDataSelection2(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                                   BL_FLOAT minNormX, BL_FLOAT maxNormX)
{
    //if (!mSelectionActive)
     //   return false;
    
    if ((mChannels == NULL) || mChannels->empty())
        return;
    
    long channelSize = (*mChannels)[0].GetSize();
    if (channelSize == 0)
        return;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    BL_FLOAT /*long*/ startDataPos = minNormX*((BL_FLOAT)channelSize)/bufferSize;

#if !FIX_BIG_ZOOM2
    BL_FLOAT /*long*/ endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize - 1;
#else
    BL_FLOAT /*long*/ endDataPos = maxNormX*((BL_FLOAT)channelSize)/bufferSize;
#endif
    
    mSelection[0] = (x0 - startDataPos)/(endDataPos - startDataPos + 1);
    
    // Warning, y is reversed !
    mSelection[3] = -y0/(bufferSize/2.0) + 1.0;
    
    mSelection[2] = (x1 - startDataPos)/(endDataPos - startDataPos + 1);
    
    // Warning, y is reversed !
    mSelection[1] = -y1/(bufferSize/2.0) + 1.0;
}

bool
SpectrogramView::GetNormDataSelection(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1)
{    
    if ((mChannels == NULL) || mChannels->empty())
        return false;
    
    bool selectionActive = mSelectionActive;
    
    // To force getting the result
    mSelectionActive = true;
    
    BL_FLOAT dataSelection[4];
    bool res = GetDataSelection(&dataSelection[0], &dataSelection[1],
                                &dataSelection[2], &dataSelection[3]);
    
    mSelectionActive = selectionActive;
    
    if (!res)
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    int channelSize = (*mChannels)[0].GetSize();
    *x0 = ((BL_FLOAT)dataSelection[0]*bufferSize)/channelSize;
    *y0 = ((BL_FLOAT)dataSelection[1])/(bufferSize/2.0);

    *x1 = ((BL_FLOAT)dataSelection[2]*bufferSize)/channelSize;
    *y1 = ((BL_FLOAT)dataSelection[3])/(bufferSize/2.0);
    
    return true;
}

bool
SpectrogramView::UpdateZoomFactor(BL_FLOAT zoomChange)
{
    if ((mChannels == NULL) || mChannels->empty())
        return false;
    
    // Avoid zooming too much
    BL_FLOAT prevAbsZoomFactor = mAbsZoomFactor;
    mAbsZoomFactor *= zoomChange;
    
    long channelSize = (*mChannels)[0].GetSize();
    BL_FLOAT maxZoom = MAX_ZOOM*((BL_FLOAT)channelSize)/100000.0;
    
    if ((mAbsZoomFactor < MIN_ZOOM) || (mAbsZoomFactor > maxZoom/*MAX_ZOOM*/))
    {
        mAbsZoomFactor = prevAbsZoomFactor;
        
        return false;
    }
    
    // Else the zoom is reasonable, set it
    mZoomFactor *= zoomChange;
    
    return true;
}

BL_FLOAT
SpectrogramView::GetZoomFactor()
{
    return mZoomFactor;
}

BL_FLOAT
SpectrogramView::GetAbsZoomFactor()
{
    return mAbsZoomFactor;
}

BL_FLOAT
SpectrogramView::GetNormZoom()
{
    if ((mChannels == NULL) || mChannels->empty())
        return 0.0;
    
    long channelSize = (*mChannels)[0].GetSize();
    BL_FLOAT maxZoom = MAX_ZOOM*((BL_FLOAT)channelSize)/100000.0;

    BL_FLOAT result = (mAbsZoomFactor - MIN_ZOOM)/(maxZoom - MIN_ZOOM);
    
    return result;
}

void
SpectrogramView::Translate(BL_FLOAT tX)
{
    mTranslation += tX;
}

BL_FLOAT
SpectrogramView::GetTranslation()
{
    return mTranslation;
}

// Use step
void
SpectrogramView::UpdateSpectrogramData(BL_FLOAT minNormX, BL_FLOAT maxNormX)
{
    //BL_FLOAT dataZoomFactor = 1.0/mZoomFactor;
    
    BL_FLOAT sampleRate = mSpectrogram->GetSampleRate();
    mSpectrogram->Reset(sampleRate);
    
    if ((mChannels == NULL) || mChannels->empty())
        return;
    
    long channelSize = (*mChannels)[0].GetSize();
    if (channelSize == 0)
        return;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    //BL_FLOAT viewNumLines = mEndDataPos - mStartDataPos + 1.0;
    
    //BL_FLOAT dataBarPos = mStartDataPos + mViewBarPos*viewNumLines;
    
    // Recompute data pos
    
#if 0 // Old
    mStartDataPos = dataBarPos - mViewBarPos*dataZoomFactor*viewNumLines;
    mEndDataPos = dataBarPos + (1.0 - mViewBarPos)*dataZoomFactor*viewNumLines; // - 1.0;
#endif
    
#if 1 // New
    mStartDataPos = ((BL_FLOAT)minNormX*channelSize)/bufferSize;
    mEndDataPos = ((BL_FLOAT)maxNormX*channelSize)/bufferSize; // - 1.0;
#endif
    
    // FIX: avoid consuming a lot of memory when zooming at the maximum,
    // then de-zooming at the maximum
    //
    // FIX: also fix the problem of refresh when zooming directly at the maximum
    // (before, we had to dezoom a little to have a good refresh
    BL_FLOAT viewNumLines = mEndDataPos - mStartDataPos + 1.0;
    
#if 0
    // Use a flag to correct both in case of overdraw bounds
    bool fullView = false;
    
    if (mStartDataPos < 0)
    {
        fullView = true;
    }
    
    if (mEndDataPos > ((BL_FLOAT)channelSize)/bufferSize - 1.0)
    {
        fullView = true;
    }
    
    if (fullView)
    {
        mStartDataPos = 0;
        mEndDataPos = ((BL_FLOAT)channelSize)/bufferSize;
    }
#endif
    
    int overlapping = mFftObj->GetOverlapping();
    
    // TODO: manage stereo
    // (for the moment, we manage only mono)
    
    vector<WDL_TypedBuf<BL_FLOAT> > dummySc;
    //vector<WDL_TypedBuf<BL_FLOAT> > dummyOut;
    
    int numChunks = viewNumLines*overlapping;
    if (viewNumLines < 1)
        numChunks = 1;
    
#if !USE_FLOAT_STEP
    int step = numChunks/mMaxNumCols;
    if (step == 0)
        step = 1;
#else
    BL_FLOAT step = ((BL_FLOAT)numChunks)/mMaxNumCols;
#endif
    
#if FULL_STEP_PROCESS
    step = 1.0;
#endif
    
    // If step is greater than 1, then force overlapping to 1
    // This will avoid vertical clear bars in the spectrogram
    // and will save much useless computation
    int prevOverlap = mFftObj->GetOverlapping();
    int prevFreqRes = mFftObj->GetFreqRes();
    BL_FLOAT prevSampleRate = mFftObj->GetSampleRate();
    bool overlapForce = false;
    
    // The magic is here !
    if (step > 1.0)
    {
        overlapForce = true;
        
        mFftObj->Reset(bufferSize, 1, prevFreqRes, prevSampleRate);
        
        step /= prevOverlap;
    }
    
    if (step < 1.0)
        step = 1.0;
    
    // Fill start pos
    int pos = mStartDataPos*bufferSize;
    
    // Pre-fill the beginning
    // (try to begin to fill the fft buffer, to avoid black border on the left)
    int preFillPos = pos - step*NUM_BUFFERS_PREFILL*bufferSize;
    if (preFillPos < 0)
        preFillPos = 0;
    int maxI = ((BL_FLOAT)(pos - preFillPos))/bufferSize;
    
    for (int i = 0; i < maxI; i++)
    {
        WDL_TypedBuf<BL_FLOAT> chunk;
        if ((preFillPos >= 0) &&
            (preFillPos < (*mChannels)[0].GetSize() - bufferSize))
        {
            // In bounds
            chunk.Add(&(*mChannels)[0].Get()[preFillPos], bufferSize);
        }
        else
        {
            // Out of bounds
            BLUtils::ResizeFillZeros(&chunk, bufferSize);
        }
        
        vector<WDL_TypedBuf<BL_FLOAT> > in;
        in.resize(1);
        in[0] = chunk;
        
        mFftObj->Process(in, dummySc, NULL);
        
        preFillPos += bufferSize;
    }

    // Empty the spectrogram
    // So here, the fft buffer is pre-filled with the previous data
    // and the spectrogram is ready
    mSpectrogram->Reset(sampleRate);
    
    // Fill normally
    while(pos < mEndDataPos*bufferSize)
    {
        WDL_TypedBuf<BL_FLOAT> chunk;
        if ((pos >= 0) && (pos < (*mChannels)[0].GetSize() - bufferSize))
        {
            // In bounds
            chunk.Add(&(*mChannels)[0].Get()[pos], bufferSize);
        }
        else
        {
            // Out of bounds
            BLUtils::ResizeFillZeros(&chunk, bufferSize);
        }
        
        // Fill the rest with zeros if the buffer is too short
        if (chunk.GetSize() < bufferSize)
            // Should not happen
            BLUtils::ResizeFillZeros(&chunk, bufferSize);
        
        vector<WDL_TypedBuf<BL_FLOAT> > in;
        in.resize(1);
        in[0] = chunk;
            
        mFftObj->Process(in, dummySc, NULL);
        
        pos += step*bufferSize;
    }
    
    if (overlapForce)
    {
        // Restore
        mFftObj->Reset(bufferSize, prevOverlap, prevFreqRes, prevSampleRate);
    }
    
    mZoomFactor = 1.0;
}

void
SpectrogramView::SetSampleRate(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
}

#endif
