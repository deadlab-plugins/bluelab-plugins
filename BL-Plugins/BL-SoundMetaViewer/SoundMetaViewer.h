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
 
#ifndef __SOUND_META_VIEWER__
#define __SOUND_META_VIEWER__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>

#include <SMVProcess4.h>

#include <View3DPluginInterface.h>

using namespace iplug;
using namespace igraphics;

class FftProcessObj16;
class PostTransientFftObj2;
class SoundMetaViewerFftObj3;
class SMVProcess4;
class SMVVolRender3;
class GraphControl12;
class ParamSmoother2;
class SoundMetaViewer final : public Plugin,
    public View3DPluginInterface
{
public:
    SoundMetaViewer(const InstanceInfo &info);
    virtual ~SoundMetaViewer();
  
    void OnReset() override;
    void OnParamChange(int paramIdx) override;
    
    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    //
    void SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1) override;
    void SetCameraFov(BL_FLOAT angle) override;
    
protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init(int oversampling, int freqRes);
    
    void CartesianToPolarFlat(WDL_TypedBuf<BL_FLOAT> *xVector,
                              WDL_TypedBuf<BL_FLOAT> *yVector);
    
    //
    void UpdateTimeAxis(bool isPlaying);
    
    
    void ApplyGain(vector<WDL_TypedBuf<BL_FLOAT> > *samples,
                   ParamSmoother2 *gainSmoother);

    //
    // Pan
    ParamSmoother2 *mPanSmoother;
    
    // Gains
    ParamSmoother2 *mInGainSmoother;
    ParamSmoother2 *mOutGainSmoother;
    
    ParamSmoother2 *mStereoWidthSmoother;

    //
    FftProcessObj16 *mFftObj;
    SMVProcess4 *mSMVProcess;

    friend class SMVCustomKeyControl;
    SMVVolRender3 *mVolRender;

    //
    BL_FLOAT mQualityXY;
    BL_FLOAT mQualityT;
    BL_FLOAT mSpeed;
  
    BL_FLOAT mSpeedT;
    BL_FLOAT mStereoWidth;
    BL_FLOAT mPan;
    BL_FLOAT mAngle0;
    BL_FLOAT mAngle1;
    BL_FLOAT mCamFov;
    SMVProcess4::ModeX mModeX;
    SMVProcess4::ModeY mModeY;
    SMVProcess4::ModeColor mModeColor;
    BL_FLOAT mRange;
    BL_FLOAT mContrast;
    BL_FLOAT mPointSize;
    int mAlgo;
    BL_FLOAT mRenderAlgoParam;
    BL_FLOAT mAlphaCoeff;
    bool mAutoQuality;
    BL_FLOAT mInGain;
    BL_FLOAT mOutGain;
    bool mIsPlaying;
    bool mFreeze;
    bool mPlayStop;
    
    //
    int mColormap;
    bool mInvertColormap;
    bool mAngularMode;
    
    BL_FLOAT mQuality;
    
    BL_FLOAT mThreshold;
    BL_FLOAT mThresholdCenter;
    
    //BL_FLOAT mStereoWidth;
    
    // Clip the voxels outside selection ?
    bool mClipFlag;

    IControl *mQualityKnob;
    IControl *mPlayButton;
    
    // For reaper automation which updates continuously
    //
    int mPrevColormap;
    
    BL_FLOAT mPrevRange;
    BL_FLOAT mPrevContrast;
    
    BL_FLOAT mPrevSpeed;
    
    BL_FLOAT mPrevThreshold;
    BL_FLOAT mPrevThresholdCenter;
    
    int mPrevClipFlag;
    
    int mPrevModeX;
    int mPrevModeY;
    int mPrevModeCol;
    
    BL_FLOAT mPrevAlphaCoeff;
    
    BL_FLOAT mPrevQuality;
    
    int mPrevFreeze;
    
    int mPrevAutoQuality;
    
    BL_FLOAT mPrevStereoWidth;
  
    int mPrevPlayStop;
  
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    
    GUIHelper12 *mGUIHelper;
        
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;

 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    
    BL_PROFILE_DECLARE;
};

#endif
