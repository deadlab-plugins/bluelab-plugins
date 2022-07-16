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
 
#ifndef __SASViewer__
#define __SASViewer__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <SASViewerPluginInterface.h>
#include <ResizeGUIPluginInterface.h>

using namespace iplug;
using namespace igraphics;

class FftProcessObj16;
class GraphControl12;
class IGUIResizeButtonControl;
//class SASViewerProcess4;
class SASViewerProcess5;
//class SASViewerRender4;
class SASViewerRender5;
class ParamSmoother2;

// New version: re-create the graph each time we close/open the windows, or resize gui
// and keep the data in other objects, not in the graph.
class SASViewer final : public Plugin,
    public ResizeGUIPluginInterface,
    public SASViewerPluginInterface
{
 public:
    SASViewer(const InstanceInfo &info);
    virtual ~SASViewer();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;
    
    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;
    
    void PreResizeGUI(int guiSizeIdx,
                      int *outNewGUIWidth, int *outNewGUIHeight) override;
    
    void SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1) override;
    void SetCameraFov(BL_FLOAT angle) override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics, int offset);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init(int oversampling, int freqRes);
    
    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    
    void CreateSASViewerRender(bool createFromInit);

    
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    FftProcessObj16 *mFftObj;
    
    SASViewerProcess5 *mSASViewerProcess[2];
    SASViewerRender5 *mSASViewerRender;
    
    //SASViewerProcess4::Mode mMode;
    SASViewerProcess5::DisplayMode mDisplayMode;

    //SASFrame5::SynthMode mSynthMode;
    SASFrameSynth::SynthMode mSynthMode;

    bool mSynthEvenPartials;
    bool mSynthOddPartials;
    
    //
    BL_FLOAT mHarmoNoiseMix;

    // Peak detection delta
    BL_FLOAT mThreshold;

    // Peak discrimination threshold
    BL_FLOAT mThreshold2;
    
    BL_FLOAT mAmpFactor;
    BL_FLOAT mFreqFactor;
    BL_FLOAT mColorFactor;
    BL_FLOAT mWarpingFactor;
    
    BL_FLOAT mSpeed;
    BL_FLOAT mDensity;
    BL_FLOAT mScale;
    
    BL_FLOAT mAngle0;
    BL_FLOAT mAngle1;
    BL_FLOAT mCamFov;
    
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
    int mGUISizeIdx;
    
    GUIHelper12 *mGUIHelper;
    
    
    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    // For GUI resize
    int mGUIOffsetX;
    int mGUIOffsetY;
    
    bool mShowTrackingLines;
    
    BL_FLOAT mTimeSmoothCoeff;
    
    BL_FLOAT mTimeSmoothNoiseCoeff;

    BLUtilsPlug mBLUtilsPlug;

    // For Neri partial filtering
    BL_FLOAT mNeriDelta;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    
    BL_PROFILE_DECLARE;
};

#endif
