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
//  InfrasonicViewerFftObj2.h
//  BL-GhostViewer
//
//  Created by Pan on 02/06/18.
//
//

#ifndef __BL_GhostViewer__InfrasonicViewerFftObj2__
#define __BL_GhostViewer__InfrasonicViewerFftObj2__

#include "FftProcessObj16.h"

// From InfrasonicViewerFftObj
//
// Fixes for jittering

// Disable for debugging
#define USE_SPECTRO_SCROLL 1

class BLSpectrogram4;
class SpectrogramDisplayScroll4;

class InfrasonicViewerFftObj2 : public ProcessObj
{
public:
    InfrasonicViewerFftObj2(int bufferSize, int oversampling, int freqRes,
                            BL_FLOAT sampleRate);
    
    virtual ~InfrasonicViewerFftObj2();
    
    void ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                          const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer = NULL);
    
    void Reset(int bufferSize, int oversampling, int freqRes, BL_FLOAT sampleRate,
               BL_FLOAT timeWindowSec);
    
    BLSpectrogram4 *GetSpectrogram();
    
#if USE_SPECTRO_SCROLL
    void SetSpectrogramDisplay(SpectrogramDisplayScroll4 *spectroDisplay);
#else
    void SetSpectrogramDisplay(SpectrogramDisplay *spectroDisplay);
#endif
    
    BL_FLOAT GetMaxFreq();
    void SetMaxFreq(BL_FLOAT freq);
    
    void SetTimeWindowSec(BL_FLOAT timeWindowSec);
    
protected:
    void AddSpectrogramLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                            const WDL_TypedBuf<BL_FLOAT> &phases);
    
    void SelectSubSonic(const WDL_TypedBuf<BL_FLOAT> &inMagns,
                        const WDL_TypedBuf<BL_FLOAT> &inPhases,
                        WDL_TypedBuf<BL_FLOAT> *outMagns,
                        WDL_TypedBuf<BL_FLOAT> *outPhases);
    void SelectSubSonic(const WDL_TypedBuf<WDL_FFT_COMPLEX> &inData,
                        WDL_TypedBuf<WDL_FFT_COMPLEX> *outData);
    
    int ComputeLastBin(BL_FLOAT freq);

    //
    BLSpectrogram4 *mSpectrogram;
    
#if USE_SPECTRO_SCROLL
    SpectrogramDisplayScroll4 *mSpectroDisplay;
#else
    SpectrogramDisplay *mSpectroDisplay;
#endif
    
    long mLineCount;
    
    deque<WDL_TypedBuf<BL_FLOAT> > mOverlapLines;
    
    // Infrasonic
    BL_FLOAT mMaxFreq;
    
    BL_FLOAT mTimeWindowSec;

private:
    // Tmp buffers
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
};

#endif /* defined(__BL_GhostViewer__InfrasonicViewerFftObj2__) */
