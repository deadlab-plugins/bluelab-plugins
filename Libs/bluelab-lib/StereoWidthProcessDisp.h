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
//  StereoWidthProcessDisp.h
//  BL-PitchShift
//
//  Created by Pan on 20/04/18.
//
//

#ifndef __BL_PitchShift__StereoWidthProcessDisp__
#define __BL_PitchShift__StereoWidthProcessDisp__

#include <SmoothAvgHistogram.h>
#include <CMA2Smoother.h>

#include "FftProcessObj16.h"

// NOTE: there was some DebugDrawer code here

// From StereoWidthProcess2 => code clean
//
// StereoWidthProcessDisp: from StereoWidthProcess3
// Removed processing, kept only display

class CMA2Smoother;
class StereoWidthProcessDisp : public MultichannelProcess
{
public:
    enum DisplayMode
    {
        SIMPLE = 0,
        SOURCE,
        SCANNER
    };
    
    StereoWidthProcessDisp(int bufferSize,
                        BL_FLOAT overlapping, BL_FLOAT oversampling,
                        BL_FLOAT sampleRate);
    
    virtual ~StereoWidthProcessDisp();
    
    void Reset() override;
    
    void Reset(int bufferSize, int overlapping,
               int oversampling, BL_FLOAT sampleRate) override;
    
    void SetDisplayMode(enum DisplayMode mode);
    
    void
    ProcessInputFft(vector<WDL_TypedBuf<WDL_FFT_COMPLEX> * > *ioFftSamples,
                    const vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > *scBuffer) override;
    
    // Get the result
    void GetWidthValues(WDL_TypedBuf<BL_FLOAT> *xValues,
                        WDL_TypedBuf<BL_FLOAT> *yValues,
                        WDL_TypedBuf<BL_FLOAT> *outColorWeights);
    
protected:
    void TemporalSmoothing(WDL_TypedBuf<BL_FLOAT> *sourceRs,
                           WDL_TypedBuf<BL_FLOAT> *sourceThetas);

    void SpatialSmoothing(WDL_TypedBuf<BL_FLOAT> *sourceRs,
                          WDL_TypedBuf<BL_FLOAT> *sourceThetas);
    
    void NormalizeRs(WDL_TypedBuf<BL_FLOAT> *sourceRs, bool discardClip);
    
    void ComputeColorWeightsMagns(WDL_TypedBuf<BL_FLOAT> *outWeights,
                                  const WDL_TypedBuf<BL_FLOAT> &magnsL,
                                  const WDL_TypedBuf<BL_FLOAT> &magnsR);
    
    void ComputeColorWeightsFreqs(WDL_TypedBuf<BL_FLOAT> *outWeights,
                                  const WDL_TypedBuf<BL_FLOAT> &freqs);
    
    int GetDisplayRefreshRate();

    //
    
    int mBufferSize;
    BL_FLOAT mOverlapping;
    BL_FLOAT mOversampling;
    BL_FLOAT mSampleRate;
    
    enum DisplayMode mDisplayMode;
    
    WDL_TypedBuf<BL_FLOAT> mWidthValuesX;
    WDL_TypedBuf<BL_FLOAT> mWidthValuesY;
    WDL_TypedBuf<BL_FLOAT> mColorWeights;
    
    // For smoothing (to improve the display)
    SmoothAvgHistogram *mSourceRsHisto;
    SmoothAvgHistogram *mSourceThetasHisto;
    
    // Do not refresh display at each step
    // (too CPU costly, particularly when using oversalpling=32)
    long long int mProcessNum;
    
    // Choosing this value as the same as overlapping is good
    int mDisplayRefreshRate;

    CMA2Smoother *mSmoother;
};

#endif /* defined(__BL_PitchShift__StereoPhasesProcess__) */
