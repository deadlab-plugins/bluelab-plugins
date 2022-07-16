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
 
#ifndef __GAIN12__
#define __GAIN12__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"
//#include "IMidiQueue.h"
#include "IPlugMidi.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

#define GAIN_VALUE 12.0

using namespace iplug;
using namespace igraphics;

class GUIHelper12;
class ParamSmoother2;
class FftProcessObj16;
//
class SmoothCurveDB;
class NoiseRemover final : public Plugin
{
 public:
    NoiseRemover(const InstanceInfo &info);
    virtual ~NoiseRemover();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);
    
    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void Init();
    void ApplyParams();

    void OnIdle();
        
    void CreateGraphAxes();
    void CreateGraphCurves();
    void UpdateCurves();

    void UpdateLatency();
    
    // Output gain
    ParamSmoother2 *mRatioSmoother;

    //
    BL_FLOAT mRatio;
    BL_FLOAT mNoiseFloorOffset;
    int mResolution;
    BL_FLOAT mNoiseSmoothTimeMs;
    
    FftProcessObj16 *mFftObj;
    NoiseRemoverObj *mNoiseRemoverObjs[2];
    
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    GUIHelper12 *mGUIHelper;

    BLUtilsPlug mBLUtilsPlug;

    bool mUseSoftMasks;
    bool mLatencyChanged;
    
    // display
    GraphControl12 *mGraph;

    GraphAmpAxis *mAmpAxis;
    GraphAxis2 *mHAxis;

    GraphFreqAxis2 *mFreqAxis;
    GraphAxis2 *mVAxis;
    
    // Curves
    GraphCurve5 *mInCurve;
    SmoothCurveDB *mInCurveSmooth;
    
    GraphCurve5 *mSigCurve;
    SmoothCurveDB *mSigCurveSmooth;
    
    GraphCurve5 *mNoiseCurve;
    SmoothCurveDB *mNoiseCurveSmooth;
    
private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf7;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf8;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf9;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf10;
    
    BL_PROFILE_DECLARE;
};

#endif
