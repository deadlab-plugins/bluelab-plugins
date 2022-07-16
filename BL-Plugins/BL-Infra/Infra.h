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
 
#ifndef __INFRA__
#define __INFRA__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include "IPlugMidi.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

using namespace iplug;
using namespace igraphics;

class GUIHelper12;
class FftProcessObj16;
class InfraProcess2;
class FftProcessBufObj;
class ParamSmoother2;
class GraphControl12;
class GraphFreqAxis2;
class GraphAmpAxis;
class GraphAxis2;
class SmoothCurveDB;
class Infra final : public Plugin
{
 public:
    Infra(const InstanceInfo &info);
    virtual ~Infra();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);
    
    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void Init(int oversampling, int freqRes);
    void ApplyParams();

    void CreateGraphAxes();
    void CreateGraphCurves();
    void UpdateCurves();
    
    // Used for processing
    FftProcessObj16 *mFftObj;
    InfraProcess2 *mInfraProcessObjs[2];

    // Used just to have the fft corresponding to the input (delayed)
    // Need 2 steps, to be in synch with mFftObjOut
    // input => mInfraProcessObjs => mBufObjOut => output
    // input => mBufObjsIn[0] => mBufObjsIn[1] => delayed input
    FftProcessObj16 *mFftObjsIn[2];
    FftProcessBufObj *mBufObjsIn[2];
    
    // Used just to have the fft corresponding to the output
    FftProcessObj16 *mFftObjOut;
    FftProcessBufObj *mBufObjOut;
    
    //
    GraphControl12 *mGraph;
    
    GraphAmpAxis *mAmpAxis;
    GraphAxis2 *mHAxis;
    
    GraphFreqAxis2 *mFreqAxis;
    GraphAxis2 *mVAxis;
    
    //
    GraphCurve5 *mInputCurve;
    SmoothCurveDB *mInputCurveSmooth;
    
    GraphCurve5 *mOscillatorsCurve;
    SmoothCurveDB *mOscillatorsCurveSmooth;

    GraphCurve5 *mSumCurve;
    SmoothCurveDB *mSumCurveSmooth;

    // Generate sub octave and phant partials in mono?
    bool mBassFocus;
    bool mBassFocusChanged;
    
    // 
    bool mMonoOut;
    
    // For reset, to avoid phasing effect when switching mono out
    bool mMonoOutChanged;

    // Avoid rumble, by adjusting phantom freq to the current freq
    // (instead of constant phant freq)
    bool mPhantAdaptive;
    
    IControl *mPhantFreqControl;
  
    // Output gain
    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;

    BL_FLOAT mThreshold;
    BL_FLOAT mPhantomFreq;
    BL_FLOAT mPhantomMix;
    int mSubOrder;
    BL_FLOAT mSubMix;
    
    //
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    GUIHelper12 *mGUIHelper;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf7;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf10;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf12;
    
    BL_PROFILE_DECLARE;
};

#endif
