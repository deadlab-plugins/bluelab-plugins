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
 
#ifndef __WAV3S__
#define __WAV3S__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <LinesRender2.h>

#include <View3DPluginInterface.h>
#include <ResizeGUIPluginInterface.h>

using namespace iplug;
using namespace igraphics;

class FftProcessObj16;
class GraphControl12;
class IGUIResizeButtonControl;
class WavesProcess;
class WavesRender;
class IBLSwitchControl;

// New version: re-create the graph each time we close/open the windows, or resize gui
// and keep the data in other objects, not in the graph.
class Wav3s final : public Plugin,
    public ResizeGUIPluginInterface,
    public View3DPluginInterface
{
 public:
    enum Color
    {
        COLOR_WHITE = 0,
        COLOR_BLUE,
        COLOR_GREEN,
        COLOR_RED,
        COLOR_ORANGE,
        COLOR_PURPLE
    };
    
    Wav3s(const InstanceInfo &info);
    virtual ~Wav3s();

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
    
    void SetColor(enum Color col);
    
    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    
    void CreateWavesRender(bool createFromInit);

    void AdjustSpeedSR(BL_FLOAT *speed);
        
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    FftProcessObj16 *mFftObj;
    WavesProcess *mWavesProcess;
    
    WavesRender *mWavesRender;
    
    //
    BL_FLOAT mSpeed;
    BL_FLOAT mDensity;
    BL_FLOAT mScale;
    
    enum LinesRender2::Mode mMode;
    enum LinesRender2::ScrollDirection mScrollDir;
    bool mShowAxes;
    bool mDBScale;
    BL_FLOAT mAngle0;
    BL_FLOAT mAngle1;
    BL_FLOAT mCamFov;
    enum Color mColor;
    
    // Graph size (when GUI is small)
    int mGraphWidthSmall;
    int mGraphHeightSmall;
    
    // "Hack" for avoiding infinite loop + restoring correct size at initialization
    //
    // NOTE: imported from Waves
    //
    IGUIResizeButtonControl *mGUISizeSmallButton;
    IGUIResizeButtonControl *mGUISizeMediumButton;
    IGUIResizeButtonControl *mGUISizeBigButton;
    int mGUISizeIdx;
    
    GUIHelper12 *mGUIHelper;
    
    IBLSwitchControl *mMonitorControl;
    bool mIsPlaying;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    // For GUI resize
    int mGUIOffsetX;
    int mGUIOffsetY;
    
    bool mMonitorEnabled;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf5;
    
    BL_PROFILE_DECLARE;
};

#endif
