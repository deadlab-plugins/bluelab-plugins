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
 
#ifndef __PANOGRAM__
#define __PANOGRAM__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <ResizeGUIPluginInterface.h>
#include <PlaySelectPluginInterface.h>

// For view orientation
#include <SpectrogramDisplayScroll4.h>

// NOTE Panogram: no need it ! (we now have FIX_BAD_DISPLAY_HIGH_SAMPLERATES)
//
// Use resampler for sample rates > 48KHz
// (which make bad graphics, very inaccurate, and not smooth)
//
// NOTE: maybe could have use bigger buffer size instead (FftProcessObj16) ?
#define USE_RESAMPLER 0

#if USE_RESAMPLER
#include "../../WDL/resample.h"
#endif

#define STEREO_WIDEN_PARAM_SMOOTHERS 1

// For VST3, do not set latency in GUI thread
// (Cubase 10 mc Sierra VST3 => freeze if set latency in GUI thread)
//#if !VST3_API
//#define SET_LATENCY_IN_GUI_THREAD 1
//#else
#define SET_LATENCY_IN_GUI_THREAD 0
//#endif


using namespace iplug;
using namespace igraphics;

class FftProcessObj16;
class SpectrogramDisplayScroll4;

class PanogramFftObj;
class PanogramGraphDrawer;
class PanogramCustomDrawer;
class PanogramCustomControl;

class PanogramPlayFftObj;
class DelayObj4;
class PseudoStereoObj2;
class GraphControl12;
class IGUIResizeButtonControl;
class GUIHelper12;
class GraphTimeAxis6;
class ParamSmoother2;
class PlugBypassDetector;
class BLTransport;
class IBLSwitchControl;

class Panogram final : public Plugin,
    public ResizeGUIPluginInterface,
    public PlaySelectPluginInterface
{
 public:
    Panogram(const InstanceInfo &info);
    virtual ~Panogram();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;
    
    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;
  
    void PreResizeGUI(int guiSizeIdx,
                      int *outNewGUIWidth, int *outNewGUIHeight) override;
    
    // Used for disabling monitor button in the right thread.
    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics, int offset);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init(int oversampling, int freqRes);
    void SetColorMap(int colorMapNum);
    //void SetScrollSpeed(int scrollSpeedNum);
    void UpdateSpectrogramData();
    
    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    
    // Time axis
    void UpdateTimeAxis();
    void UpdateTimeAxisFromTransport();
    
    void CreateSpectrogramDisplay(bool createFromInit);
    void CreateGraphAxes();

    // PlaySelectPluginInterface
    //

    // Play bar
    void SetBarActive(bool flag) override;
    bool IsBarActive() override;
    void SetBarPos(BL_FLOAT x) override;
    BL_FLOAT GetBarPos() override;
    void ResetPlayBar() override;
    void ClearBar() override;
    
    void StartPlay() override;
    void StopPlay() override;
    bool PlayStarted() override;
    
    // Selection
    bool IsSelectionActive() override;
    void UpdateSelection(BL_FLOAT x0, BL_FLOAT y0,
                         BL_FLOAT x1, BL_FLOAT y1,
                         bool updateCenterPos,
                         bool activateDrawSelection = false,
                         bool updateCustomControl = false) override;
    void SelectionChanged() override;
    bool PlayBarOutsideSelection() override;

    // Both
    void BarSetSelection(int x) override;
    void SetSelectionActive(bool flag) override;

    void UpdatePlayBar() override;

    void GetGraphSize(int *width, int *height) override;
    
    // Specific to Panogram
    void ClipSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                       BL_FLOAT *x1, BL_FLOAT *y1,
                       int width, int height);

    void AlignPosToBuffers(BL_FLOAT *pos);    
    void AlignPosToBuffersCeil(BL_FLOAT *pos);
    void AlignChannelsSize();
    
    void UpdateSpectroPlay();
    
    // Stereo widen
    void StereoWiden(vector<WDL_TypedBuf<BL_FLOAT> > *samples);
    
    void UpdateLatency();
  
    void SetFreezeFromParamChange(bool freeze);
    void SetFreezeFromApplyParams(bool freeze);
    
    void SetPlayStopFromParamChange(bool playFlag);
    void SetPlayStopFromApplyParams(bool playFlag);

    void UpdateAxesViewOrientation();
        
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    FftProcessObj16 *mFftObj;
    PanogramFftObj *mPanogramObj;
    
    BLSpectrogram4 *mSpectrogram;
    SpectrogramDisplayScroll4 *mSpectrogramDisplay;
    SpectrogramDisplayScroll4::SpectrogramDisplayScrollState *
        mSpectroDisplayScrollState;
 
    long mProcessCount;

#if USE_RESAMPLER
    void CheckSampleRate(BL_FLOAT *ioSampleRate);
    
    // Set to true if sample rate is > 48KHz
    bool mUseResampler;
    
    WDL_Resampler mResamplers[2];
#endif

    // L/R and line
    PanogramGraphDrawer *mGraphDrawer;
    
    bool mFreeze;
    
    bool mPlay;
    BL_FLOAT mPlayBarPos;
            
    BL_FLOAT mOutGain;
    ParamSmoother2 *mOutGainSmoother;
    
    // Selection and play bar
    PanogramCustomDrawer  *mCustomDrawer;
    PanogramCustomDrawer::State *mPanogramCustomDrawerState;
    
    PanogramCustomControl  *mCustomControl;
    
    PanogramPlayFftObj *mPanogramPlayFftObjs[2];

    // For gray out
    IControl *mPlayButton;

    //
    BL_FLOAT mPan;
    
    // Stereo widen
#if !STEREO_WIDEN_PARAM_SMOOTHERS
    BL_FLOAT mStereoWidth;
#else
    ParamSmoother2 *mStereoWidthSmoother;
    ParamSmoother2 *mPanSmoother;
#endif
    
    bool mMonoToStereo;
    
    DelayObj4 *mDelayObj;
    
    IControl *mStereoWidthKnob;
    IControl *mPanKnob;
    IControl *mMonoToStereoButton;
    
#if !SET_LATENCY_IN_GUI_THREAD
    int mLatency;
    int mPrevLatency;
#endif
  
    PseudoStereoObj2 *mPseudoStereoObj;

    //
    bool mNeedRecomputeData;
    
    bool mMustUpdateSpectrogram;
    
    // Graph size (when GUI is small)
    int mGraphWidthSmall;
    int mGraphHeightSmall;
    
    //
    BL_FLOAT mPrevSampleRate;
    
    // "Hack" for avoiding infinite loop + restoring correct size at initialization
    //
    // NOTE: imported from Waves
    //
    IGUIResizeButtonControl *mGUISizeSmallButton;
    IGUIResizeButtonControl *mGUISizeMediumButton;
    IGUIResizeButtonControl *mGUISizeBigButton;
    
    BL_FLOAT mRange;
    BL_FLOAT mContrast;
    int mColorMapNum;
    //int mScrollSpeedNum;

    int mGUISizeIdx;
    
    GUIHelper12 *mGUIHelper;
    
    GraphAxis2 *mHAxis;
    GraphTimeAxis6 *mTimeAxis;
    bool mMustUpdateTimeAxis;

    // For displaying L/R
    GraphAxis2 *mVAxis;

    //
    BL_FLOAT mSharpness;
    BL_FLOAT mStereoWidth;
    
    IBLSwitchControl *mMonitorControl;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    // For GUI resize
    int mGUIOffsetX;
    int mGUIOffsetY;
    
    bool mMonitorEnabled;
    bool mWasPlaying;
    bool mIsPlaying;  

    PlugBypassDetector *mBypassDetector;
    BLTransport *mTransport;

    SpectrogramDisplayScroll4::ViewOrientation mViewOrientation;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf4;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf5;
    
    BL_PROFILE_DECLARE;
};

#endif
