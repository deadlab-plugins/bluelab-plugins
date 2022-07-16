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
 
#ifndef __CHROMA__
#define __CHROMA__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <ResizeGUIPluginInterface.h>

// Use resampler for sample rates > 48KHz
// (which make bad graphics, very inaccurate, and not smooth)
//
// NOTE: maybe could have use bigger buffer size instead (FftProcessObj16) ?
#define USE_RESAMPLER 1

#if USE_RESAMPLER
#include "../../WDL/resample.h"
#endif

#define REF_SAMLERATE 44100.0
#define MAX_SAMLERATE 48000.0

using namespace iplug;
using namespace igraphics;

class FftProcessObj16;
class SpectrogramDisplayScroll4;
class ChromaFftObj2;
class GraphControl12;
class IGUIResizeButtonControl;
class GraphTimeAxis6;
class GraphAxis2;
class GUIHelper12;
class PlugBypassDetector;
class BLTransport;
class IBLSwitchControl;

// New version: re-create the graph each time we close/open the windows, or resize gui
// and keep the data in other objects, not in the graph.
class Chroma final : public Plugin,
    public ResizeGUIPluginInterface
{
 public:
    Chroma(const InstanceInfo &info);
    virtual ~Chroma();

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
    void SetScrollSpeed(int scrollSpeedNum);
    void UpdateSpectrogramData();
    
    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    
    // Time axis
    void UpdateTimeAxis();
    
    void CreateSpectrogramDisplay(bool createFromInit);
    void CreateGraphAxes();

    void UpdateVAxis();
    
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    FftProcessObj16 *mDispFftObj;
    ChromaFftObj2 *mChromaObj;
    
    BLSpectrogram4 *mSpectrogram;
    SpectrogramDisplayScroll4 *mSpectrogramDisplay;
    SpectrogramDisplayScroll4::SpectrogramDisplayScrollState *
        mSpectroDisplayScrollState;
    
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
    IGUIResizeButtonControl *mGUISizePortraitButton;
    
    BL_FLOAT mRange;
    BL_FLOAT mContrast;
    int mColorMapNum;
    int mScrollSpeedNum;

    int mGUISizeIdx;
    
    GUIHelper12 *mGUIHelper;
    
    GraphAxis2 *mHAxis;
    GraphTimeAxis6 *mTimeAxis;
    bool mMustUpdateTimeAxis;
    
    GraphAxis2 *mVAxis;
    
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
    
#if USE_RESAMPLER
    void CheckSampleRate(BL_FLOAT *ioSampleRate);
    
    // Set to true if sample rate is > 48KHz
    bool mUseResampler;
    
    WDL_Resampler mResampler;
#endif
  
    BL_FLOAT mATune;
    BL_FLOAT mSharpness;
    bool mDisplayLines;

    PlugBypassDetector *mBypassDetector;
    BLTransport *mTransport;

    //bool mFirstTimeCreate;
    bool mMustSetScrollSpeed;
    int mPrevScrollSpeedNum;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    
    BL_PROFILE_DECLARE;
};

#endif
