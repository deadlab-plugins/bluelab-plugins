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
//  PanogramPlayFftObj.cpp
//  BL-Ghost
//
//  Created by Pan on 02/06/18.
//
//

#include <HistoMaskLine2.h>

#include <BLUtils.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include "PanogramPlayFftObj.h"

// BAD: doesn't work well
// Align better the sound played with the graphics
// PROBLEM: no sound with thin selections
#define FIX_SHIFT_LINE_COUNT 0 //1

// At the right of the panogram, just before the play bar
// rewinds, the last line is played several times, making a garbage sound
#define FIX_LAST_LINES_BAD_SOUND 1

// New method => works better
// Align better the sound with selection and play bar
//
// NOTE: works best with (0, 1)
// => selection play is accurate when playing a rectangle selection
// (SHIFT_X_SELECTION_PART1=0)
// And without selection, the sound starts accurately when play bar passes
// over the start of the spectro data
// (SHIFT_X_SELECTION_PART2=1)
#define SHIFT_X_SELECTION_PART1 0 //1
// Set to 0 because when the selection has small width,
// the moving play bar is not displayed anymore
#define SHIFT_X_SELECTION_PART2 0 //1

// With Reaper and SyncroTest (sine):
// Use a project freezed by default
// de-freeze, play host, stop host, freeze, then try to play the panogram => no sound
#define FIX_NO_SOUND_FIRST_DE_FREEZE 1


PanogramPlayFftObj::PanogramPlayFftObj(int bufferSize, int oversampling, int freqRes,
                                       BL_FLOAT sampleRate)
: ProcessObj(bufferSize)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);
    
    mLineCount = 0;
    
    mMode = RECORD;
    
    mSelectionEnabled = false;
    mSelectionPlayFinished = false;
    
    mDataSelection[0] = 0.0;
    mDataSelection[1] = 0.0;
    mDataSelection[2] = 0.0;
    mDataSelection[3] = 0.0;
    
    mNumCols = 0;
    
    mIsPlaying = false;
    
    mHostIsPlaying = false;
}

PanogramPlayFftObj::~PanogramPlayFftObj() {}

void
PanogramPlayFftObj::ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                                     const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)
{
    if (mMode == BYPASS)
        return;
    
    if (mMode == PLAY)
    {
        if (mSelectionEnabled)
        {
            BL_FLOAT x1 = mDataSelection[2];
            if (mLineCount > x1)
                mSelectionPlayFinished = true;
        }
        else if (mLineCount >= mNumCols)
        {
            mSelectionPlayFinished = true;
        }
        
        WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf0;
        magns.Resize(ioBuffer->GetSize()/2);
        
        WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf1;
        phases.Resize(ioBuffer->GetSize()/2);
        
        if (!mSelectionPlayFinished)
        {
            //GetDataLine(mCurrentMagns, &magns);
            //GetDataLine(mCurrentPhases, &phases);
            
            int lineCount = mLineCount;
            
            bool inside = true;
            
#if FIX_SHIFT_LINE_COUNT
            inside = ShiftXPlayBar(&lineCount);
            
#if !FIX_LAST_LINES_BAD_SOUND
            inside = true;
#endif

#endif
            
            if (inside)
            {
                GetDataLineMask(mCurrentMagns, &magns, lineCount);
                GetDataLineMask(mCurrentPhases, &phases, lineCount);
            }
            else
            {
                // Outside selection => silence !
                BLUtils::FillAllZero(&magns);
                BLUtils::FillAllZero(&phases);
            }
        }
        
        // Avoid a residual play on all the frequencies
        // when looping, at the end of the loop
        if (mSelectionPlayFinished)
        {
            BLUtils::FillAllZero(&magns);
            BLUtils::FillAllZero(&phases);
        }
        
        BLUtilsComp::MagnPhaseToComplex(ioBuffer, magns, phases);
        
        BLUtils::ResizeFillZeros(ioBuffer, ioBuffer->GetSize()*2);
        BLUtilsFft::FillSecondFftHalf(ioBuffer);
        
        if (mIsPlaying)
        {
            // Increment
            mLineCount++;
        }
    }
    
    if (mMode == RECORD)
    {
        //WDL_TypedBuf<WDL_FFT_COMPLEX> ioBuffer0 = *ioBuffer;
        //BLUtils::TakeHalf(&ioBuffer0);

        WDL_TypedBuf<WDL_FFT_COMPLEX> &ioBuffer0 = mTmpBuf2;
        BLUtils::TakeHalf(*ioBuffer, &ioBuffer0);
        
        WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf3;
        WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf4;
        BLUtilsComp::ComplexToMagnPhase(&magns, &phases, ioBuffer0);

        if (mCurrentMagns.size() != mNumCols)
        {
            mCurrentMagns.push_back(magns);
            if (mCurrentMagns.size() > mNumCols)
                mCurrentMagns.pop_front();
        }
        else
        {
            mCurrentMagns.freeze();
            mCurrentMagns.push_pop(magns);
        }

        if (mCurrentPhases.size() != mNumCols)
        {
            mCurrentPhases.push_back(phases);
            if (mCurrentPhases.size() > mNumCols)
                mCurrentPhases.pop_front();
        }
        else
        {
            mCurrentPhases.freeze();
            mCurrentPhases.push_pop(phases);
        }
        
        // Play only inside selection
        //
        if (mHostIsPlaying && mSelectionEnabled)
        {
            WDL_TypedBuf<BL_FLOAT> &magns0 = mTmpBuf5;
            magns0.Resize(ioBuffer->GetSize()/2);
            
            WDL_TypedBuf<BL_FLOAT> &phases0 = mTmpBuf6;
            phases0.Resize(ioBuffer->GetSize()/2);
            
            //
            int lineCount = (mDataSelection[0] + mDataSelection[2])/2.0;
            
#if FIX_SHIFT_LINE_COUNT
            ShiftXPlayBar(&lineCount);
#endif
            
            GetDataLineMask(mCurrentMagns, &magns0, lineCount);
            GetDataLineMask(mCurrentPhases, &phases0, lineCount);

            //BLUtils::MagnPhaseToComplex(ioBuffer, magns0, phases0); 
            //BLUtils::ResizeFillZeros(ioBuffer, ioBuffer->GetSize()*2);
            //BLUtils::FillSecondFftHalf(ioBuffer);

            BLUtilsComp::MagnPhaseToComplex(&ioBuffer0, magns0, phases0);
            BLUtils::SetBuf(ioBuffer, ioBuffer0);
            BLUtilsFft::FillSecondFftHalf(ioBuffer);
        }
    }
}

void
PanogramPlayFftObj::Reset(int bufferSize, int oversampling,
                          int freqRes, BL_FLOAT sampleRate)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);
    
    mLineCount = 0;
    
    mSelectionEnabled = false;
    mSelectionPlayFinished = false;

    // Be sure to clear the previous data
    ClearData();
}

void
PanogramPlayFftObj::SetMode(Mode mode)
{
    mMode = mode;
}

void
PanogramPlayFftObj::SetNormSelection(BL_FLOAT x0, BL_FLOAT y0,
                                     BL_FLOAT x1, BL_FLOAT y1)
{
    // Swap
    BL_FLOAT newY0 = 1.0 - y1;
    BL_FLOAT newY1 = 1.0 - y0;
    y0 = newY0;
    y1 = newY1;
    
    mDataSelection[0] = x0*mNumCols;
    mDataSelection[1] = y0*mBufferSize/2;
    mDataSelection[2] = x1*mNumCols;
    mDataSelection[3] = y1*mBufferSize/2;
    
#if SHIFT_X_SELECTION_PART1
    ShiftXSelection(&mDataSelection[0]);
    ShiftXSelection(&mDataSelection[2]);
#endif
    
    mSelectionEnabled = true;
}

void
PanogramPlayFftObj::GetNormSelection(BL_FLOAT selection[4])
{
    int lineSize = mBufferSize/2;
    
    selection[0] = mDataSelection[0]/mNumCols;
    selection[1] = mDataSelection[1]/lineSize;
    
    selection[2] = mDataSelection[2]/mNumCols;
    selection[3] = mDataSelection[3]/lineSize;
}

void
PanogramPlayFftObj::SetSelectionEnabled(bool flag)
{
    mSelectionEnabled = flag;
}

void
PanogramPlayFftObj::RewindToStartSelection()
{
    // Rewind to the beginning of the selection
    mLineCount = mDataSelection[0];
    
    mSelectionPlayFinished = false;
}

void
PanogramPlayFftObj::RewindToNormValue(BL_FLOAT value)
{
    mLineCount = value*mNumCols;
    mLineCount = 0;
    
    mSelectionPlayFinished = false;
}

bool
PanogramPlayFftObj::SelectionPlayFinished()
{
    return mSelectionPlayFinished;
}

BL_FLOAT
PanogramPlayFftObj::GetPlayPosition()
{
    int lineCount = mLineCount;
    
    BL_FLOAT res = ((BL_FLOAT)lineCount)/mNumCols;
    
    return res;
}

BL_FLOAT
PanogramPlayFftObj::GetSelPlayPosition()
{
    int lineCount = mLineCount;

#if SHIFT_X_SELECTION_PART2
    //lineCount -= 4;
    lineCount -= 8; // Very accurate when no selection!
    if (lineCount < mDataSelection[0] + 1 /*+ 2*/)
        lineCount = mDataSelection[0] + 1 /*+ 2*/;
#endif
    
    BL_FLOAT res = ((BL_FLOAT)(lineCount - mDataSelection[0]))/
        (mDataSelection[2] - mDataSelection[0]);
    
    return res;
}

void
PanogramPlayFftObj::SetNumCols(int numCols)
{   
    if (numCols != mNumCols)
    {
        mNumCols = numCols;
    
        mCurrentMagns.resize(mNumCols);
        mCurrentMagns.freeze();
        
        mCurrentPhases.resize(mNumCols);
        mCurrentPhases.freeze();
        
#if FIX_NO_SOUND_FIRST_DE_FREEZE
        /*mMaskLines.clear();
        
          HistoMaskLine2 maskLine(mBufferSize);
          for (int i = 0; i < mNumCols; i++)
          {
          mMaskLines.push_back(maskLine);
          }*/

        mMaskLines.resize(mNumCols);
#endif

        ClearData();
    }
}

void
PanogramPlayFftObj::SetIsPlaying(bool flag)
{
    mIsPlaying = flag;
}

void
PanogramPlayFftObj::SetHostIsPlaying(bool flag)
{
    mHostIsPlaying = flag;
}

void
PanogramPlayFftObj::AddMaskLine(const HistoMaskLine2 &maskLine)
{
    if (mMode == RECORD)
    {
        mMaskLines.push_back(maskLine);
        if (mMaskLines.size() > mNumCols)
            mMaskLines.pop_front();
    }
}

void
PanogramPlayFftObj::GetDataLine(const bl_queue<WDL_TypedBuf<BL_FLOAT> > &inData,
                                WDL_TypedBuf<BL_FLOAT> *data,
                                int lineCount)
{
    BL_FLOAT x0 = mDataSelection[0];
    BL_FLOAT x1 = mDataSelection[2];
    
    // Here, we manage data on x
    if ((lineCount < x0) || (lineCount > x1) ||
        (lineCount >= mNumCols))
    {
        // Set to 0 outside x selection
        BLUtils::FillAllZero(data);
    }
    else
    {
        // Here, we manage data on y
        if (lineCount < mNumCols)
        {
            // Original line
            *data = inData[lineCount];
            
            // Set to 0 outside y selection
            BL_FLOAT y0 = mDataSelection[1];
            if (y0 < 0.0)
                y0 = 0.0;
            
            // After Valgrind tests
            // Could happen if we dragged the whole selection outside
            if (y0 >= data->GetSize())
                y0 = data->GetSize() - 1.0;
            
            BL_FLOAT y1 = mDataSelection[3];
            if (y1 >= data->GetSize())
                y1 = data->GetSize() - 1.0;
            
            // Can happen if we dragged the whole selection outside
            if (y1 < 0.0)
                y1 = 0.0;
            
            for (int i = 0; i <= y0; i++)
                data->Get()[i] = 0.0;
            
            for (int i = y1; i < data->GetSize(); i++)
                data->Get()[i] = 0.0;
        }
        else
        {
            BLUtils::FillAllZero(data);
        }
    }
}

void
PanogramPlayFftObj::GetDataLineMask(const bl_queue<WDL_TypedBuf<BL_FLOAT> > &inData,
                                    WDL_TypedBuf<BL_FLOAT> *data,
                                    int lineCount)
{
    BL_FLOAT x0 = mDataSelection[0];
    BL_FLOAT x1 = mDataSelection[2];
    
    // Here, we manage data on x
    if ((lineCount < x0) || (lineCount > x1) ||
        (lineCount >= mNumCols))
    {
        // Set to 0 outside x selection
        BLUtils::FillAllZero(data);
    }
    else
    {
        // Here, we manage data on y
        if ((lineCount > 0) &&(lineCount < mNumCols))
        {
            // Original line
            *data = inData[lineCount];
            
            // Set to 0 outside y selection
            BL_FLOAT y0 = mDataSelection[1];
            if (y0 < 0.0)
                y0 = 0.0;
            
            // After Valgrind tests
            // Could happen if we dragged the whole selection outside
            if (y0 >= data->GetSize())
                y0 = data->GetSize() - 1.0;
            
            BL_FLOAT y1 = mDataSelection[3];
            if (y1 >= data->GetSize())
                y1 = data->GetSize() - 1.0;
            
            // Can happen if we dragged the whole selection outside
            if (y1 < 0.0)
                y1 = 0.0;
            
            // Mask
            if (lineCount < mMaskLines.size())
            {
                HistoMaskLine2 &maskLine = mMaskLines[lineCount];
                maskLine.Apply(data, y0, y1);
            }
        }
        else
        {
            BLUtils::FillAllZero(data);
        }
    }
}

// For play bar
bool
PanogramPlayFftObj::ShiftXPlayBar(int *xValue)
{
    // Shift a little, to be exactly at the middle of the selection
    *xValue += mOverlapping*2;
    if (*xValue >= mNumCols)
    {
        *xValue = mNumCols - 1;
        
        return false;
    }
    
    return true;
}

#if 0
bool
PanogramPlayFftObj::ShiftX(BL_FLOAT *xValue)
{
    int xValueInt = *xValue;
    bool result = ShiftX(&xValueInt);
    *xValue = xValueInt;
    
    return result;
}
#endif

// For selection
bool
PanogramPlayFftObj::ShiftXSelection(BL_FLOAT *xValue)
{
    // Shift a little, to be exactly at the middle of the selection
    *xValue += 2;
    if (*xValue >= mNumCols)
    {
        *xValue = mNumCols - 1;
        
        return false;
    }
    
    return true;
}

void
PanogramPlayFftObj::ClearData()
{
    WDL_TypedBuf<BL_FLOAT> &zeros = mTmpBuf7;
    zeros.Resize(mBufferSize/2);
    BLUtils::FillAllZero(&zeros);
    
    for (int i = 0; i < mNumCols; i++)
    {
        mCurrentMagns[i] = zeros;
        mCurrentPhases[i] = zeros;
    }

#if FIX_NO_SOUND_FIRST_DE_FREEZE
    HistoMaskLine2 maskLine(mBufferSize);
    mMaskLines.clear(maskLine);
#endif
}
