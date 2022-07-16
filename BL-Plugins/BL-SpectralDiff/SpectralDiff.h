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
 
#ifndef __SPECTRAL_DIFF__
#define __SPECTRAL_DIFF__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsFile.h>
#include <BLUtilsPlug.h>

#include <ResizeGUIPluginInterface.h>

using namespace iplug;
using namespace igraphics;


class FftProcessObj16;
class SpectralDiffObj;
class GraphControl12;
class GraphFreqAxis2;
class GraphAmpAxis;
class GraphAxis2;
class GUIHelper12;
class SmoothCurveDB;
class IGUIResizeButtonControl;
class SpectralDiff final : public Plugin,
    public ResizeGUIPluginInterface
{
 public:
    SpectralDiff(const InstanceInfo &info);
    virtual ~SpectralDiff();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    void PreResizeGUI(int guiSizeIdx,
                      int *outNewGUIWidth, int *outNewGUIHeight) override;

    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics, int offset);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init(int oversampling, int freqRes);
    
    void CreateGraphAxes();
    void CreateGraphCurves();
    
    void UpdateCurves(const WDL_TypedBuf<BL_FLOAT> &signal0,
                      const WDL_TypedBuf<BL_FLOAT> &signal1,
                      const WDL_TypedBuf<BL_FLOAT> &diff);

    void ComputeDiffDB(WDL_TypedBuf<BL_FLOAT> *result,
                       const WDL_TypedBuf<BL_FLOAT> &signal0,
                       const WDL_TypedBuf<BL_FLOAT> &signal1);

    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    FftProcessObj16 *mFftObj;
    SpectralDiffObj *mSpectralDiffObjs[2];
    
    GraphControl12 *mGraph;
    
    GraphAmpAxis *mAmpAxis;
    GraphAxis2 *mHAxis;
    
    GraphFreqAxis2 *mFreqAxis;
    GraphAxis2 *mVAxis;
    
    GraphCurve5 *mSignal0Curve;
    SmoothCurveDB *mSignal0CurveSmooth;
    
    GraphCurve5 *mSignal1Curve;
    SmoothCurveDB *mSignal1CurveSmooth;
    
    GraphCurve5 *mDiffCurve;
    SmoothCurveDB *mDiffCurveSmooth;

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

    // For GUI resize
    int mGUIOffsetX;
    int mGUIOffsetY;
    
    //
    GUIHelper12 *mGUIHelper;
    
    //
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;

#if APP_API
    void RunAppDiff();
    void CheckAppStartupArgs();
    void PrintUsage(const char *cmdName);
    BL_FLOAT ComputeDiff();
    void PrintDiff(BL_FLOAT diff);
    bool OpenFile(const char *fileName, WDL_TypedBuf<BL_FLOAT> *samples);
        
    //
    bool mBriefDislay;
    char mFileName0[FILENAME_SIZE];
    char mFileName1[FILENAME_SIZE];

    Scale *mScale;
#endif

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
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11;
    
    BL_PROFILE_DECLARE;
};

#endif
