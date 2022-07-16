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
 
#ifndef __SHAPER__
#define __SHAPER__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <ZoomCustomControl.h>

#include <BLUtilsPlug.h>

// Use standard waveform display, or scan display?
#define USE_SCAN_DISPLAY 1

using namespace iplug;
using namespace igraphics;

class GraphControl12;
class GraphAxis2;
class GUIHelper12;

class ParamSmoother2;
class TransientShaperFftObj3;
class FftProcessObj16;

class BLScanDisplay;
class PlugBypassDetector;

class IBLSwitchControl;

class Shaper final : public Plugin,
    public ZoomListener
{
 public:
    Shaper(const InstanceInfo &info);
    virtual ~Shaper();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

#if USE_SCAN_DISPLAY
    // Used for disabling monitor button in the right thread.
    void OnIdle() override;
#endif

    void UpdateZoom(BL_FLOAT zoomChange) override;
    void ResetZoom() override;
        
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init(int oversampling, int freqRes);

    void CreateGraphAxis();
    
#if !USE_SCAN_DISPLAY
    void CreateGraphCurves();
#else
    void SetCurvesStyle();
#endif
    
    void UpdateCurves(const WDL_TypedBuf<BL_FLOAT> &inputBuf,
                      const WDL_TypedBuf<BL_FLOAT> &outputBuf,
                      const WDL_TypedBuf<BL_FLOAT> &transientness);
    
    void QualityChanged();

    //
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    GraphControl12 *mGraph;

    GraphAxis2 *mVAxis;
    
#if !USE_SCAN_DISPLAY
    GraphCurve5 *mInputCurve;
    GraphCurve5 *mOutputCurve;
#else
    BLScanDisplay *mScanDisplay;
#endif
    
    BL_FLOAT mOutGain;
    ParamSmoother2 *mOutGainSmoother;

    BL_FLOAT mSoftHard; 
    BL_FLOAT mPrecision;
    BL_FLOAT mGain;

    FftProcessObj16 *mFftObj;
    vector<TransientShaperFftObj3 *> mTransObjs;

    enum Quality
    {
        STANDARD = 0,
        HIGH,
        VERY_HIGH,
        OFFLINE
    };
    
    enum Quality mQuality;

    // At 0, detect only using freqs
    // At 1, detect only using amps
    // At 0.5, detect with both
    BL_FLOAT mFreqAmpRatio;
    
    int mOversampling;
    int mFreqRes;
    
    // Lazy evaluatoon variables to avoid crash with
    // threads concurrency (when mouse down)
    bool mQualityChanged;
    bool mMustReset;
    
    GUIHelper12 *mGUIHelper;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;

#if USE_SCAN_DISPLAY
    IBLSwitchControl *mMonitorControl;

    PlugBypassDetector *mBypassDetector;

    bool mIsPlaying;
    bool mMonitorEnabled;
#endif

    ZoomCustomControl *mZoomControl;
    BL_FLOAT mZoom;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11;
    
    BL_PROFILE_DECLARE;
};

#endif
