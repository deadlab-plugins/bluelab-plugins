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
 
#ifndef __STEREOWIDTH__
#define __STEREOWIDTH__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLVectorscope.h>

#include <BLUtilsPlug.h>

#define SET_LATENCY_IN_GUI_THREAD 0

// Hide last vertorscope mode (5), which is legacy and a bit weird
#define SHOW_LEGACY_VECTORSCOPE_MODE 0 // 1
// Keep it to 5 in any case.
// If we have only 4 modes, the data associated with last mode will be NULL
#define NUM_VECTORSCOPE_MODES 5

// It now avoids clicks, except if we turn the knob very quickly
#define USE_BASS_FOCUS_SMOOTHER 1 //0

using namespace iplug;
using namespace igraphics;

class PseudoStereoObj2;
class BLCorrelationComputer2;
class CrossoverSplitterNBands4;
class BLUpmixGraphDrawer;
class GraphControl12;
class GUIHelper12;
class ParamSmoother2;
class BLWidthAdjuster;
class BLStereoWidener;
class StereoWidth final : public Plugin,
    public BLVectorscopePlug
{
 public:
    StereoWidth(const InstanceInfo &info);
    virtual ~StereoWidth();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    // Vectorscope callbacks
    void VectorscopeUpdatePanCB(BL_FLOAT newPan) override;
    void VectorscopeUpdateDPanCB(BL_FLOAT dpan) override;
    void VectorscopeUpdateWidthCB(BL_FLOAT newWidth) override;
    void VectorscopeUpdateDWidthCB(BL_FLOAT dwidth) override;
    void VectorscopeUpdateDepthCB(BL_FLOAT newDepth) override;
    void VectorscopeUpdateDDepthCB(BL_FLOAT ddepth) override;
    void VectorscopeResetAllParamsCB() override;

    // Used for changing the vectorscope mode without blinking
    // "mode number" buttons 
    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init();

    //
    // Simple version
    void StereoWidenSimple(vector<WDL_TypedBuf<BL_FLOAT> > *samples);
    // Version with width adjuster
    void StereoWiden(vector<WDL_TypedBuf<BL_FLOAT> > *samples, bool doPan);
    void ComputeCorrelation(const vector<WDL_TypedBuf<BL_FLOAT> > &samples);
    void SetBassFocusFreq(BL_FLOAT freq);
    void ApplyOutGain(vector<WDL_TypedBuf<BL_FLOAT> > *samples);
    void StereoToMono(vector<WDL_TypedBuf<BL_FLOAT> > *samples);
  
    //
    void UpdateWidthMeter(BL_FLOAT width);
    
    //
    void CreateVectorscope(IGraphics *pGraphics);
    void CreateVectorscopeButtonsParams();
    void CreateVectorscopeButtons(IGraphics *pGraphics);
    void CreateVectorscopeGraphs(IGraphics *pGraphics);
    
    void UpdateVectorscopeMode(BLVectorscope::Mode mode);
    
    void UpdateLatency();

    // Width limit
    void UpdateWidthMeterLimit();

    BL_FLOAT GetLimitedWidth();

    bool IsWidthLimitEnabled();

    void SetWidthLimitEnabled(bool flag);

    BL_FLOAT GetWidthLimitSmooth();

    void SetWidthLimitSmooth(BL_FLOAT smoothFactor);

    // Width boost
    void UpdateWidthBoostFactor();

    void UpdateWidth();

    void VectorscopeSetControlsOverGraphs();
    void VectorscopeClearControlsOverGraphs();
        
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    GUIHelper12 *mGUIHelper;
    
    //
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;

    // Stereo widen
    BL_FLOAT mWidth;
  
    ParamSmoother2 *mStereoWidthSmoother;
  
    // Pan
    ParamSmoother2 *mPanSmoother;
    BL_FLOAT mPan;
    
    bool mMonoOut;
    bool mMonoToStereo;
  
    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;
    
    BL_FLOAT mBassFocusFreq;
    CrossoverSplitterNBands4 *mBassFocusBandSplitters[2];

#if USE_BASS_FOCUS_SMOOTHER
    ParamSmoother2 *mBassFocusSmoother;
#endif
    
    PseudoStereoObj2 *mPseudoStereoObj;
    
    vector<IControl *> mVectorscopeControls;
  
    VumeterControl *mWidthVumeter;
  
    // Correlation
    VumeterControl *mCorrelationVumeter;
    
    BLCorrelationComputer2 *mCorrelationComputer;

    IControl *mPanControl;
    IControl *mWidthControl;
    
    // Vectorscope
    BLVectorscope *mVectorscope;
  
    GraphControl12 *mVectorscopeGraphs[NUM_VECTORSCOPE_MODES];
  
    BLUpmixGraphDrawer *mUpmixDrawer;
  
    BLVectorscope::Mode mVectorscopeMode;
    
#if !SET_LATENCY_IN_GUI_THREAD
    int mLatency;
    int mPrevLatency;
#endif

    // Width limit
    bool mWidthLimitEnabled;
    BL_FLOAT mWidthLimitSmoothFactor;
    IControl *mWidthLimitControl;
    IControl *mWidthLimitSmoothControl;
    BLWidthAdjuster *mWidthAdjuster;
    BLStereoWidener *mStereoWidener;

    // Width boost
    bool mWidthBoost;
    BL_FLOAT mWidthBoostFactor;

    bool mVectorscopeModeChanged;

    BLUtilsPlug mBLUtilsPlug;

    bool mBassToMono;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf3;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
    
    BL_PROFILE_DECLARE;
};

#endif
