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
 
#ifndef __LOFI__
#define __LOFI__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

using namespace iplug;
using namespace igraphics;

class GraphControl12;
class GraphAxis2;
class GUIHelper12;
class Bufferizer;
class ParamSmoother2;
class LoFi final : public Plugin
{
 public:
    LoFi(const InstanceInfo &info);
    virtual ~LoFi();

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
    void ApplyParams();
    
    void Init();
    
    void CreateGraphAxis();
    void CreateGraphCurve();
    void UpdateCurve(const WDL_TypedBuf<BL_FLOAT> &buf);
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    GraphControl12 *mGraph;
    
    GraphAxis2 *mVAxis;
    
    GraphCurve5 *mLoFiCurve;

    ParamSmoother2 *mDepthSmoother;
    BL_FLOAT mDepth;
    
    ParamSmoother2 *mInGainSmoother;
    BL_FLOAT mInGain;
    
    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;

    Bufferizer *mGraphBufferizer;

    GUIHelper12 *mGUIHelper;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    
    BL_PROFILE_DECLARE;
};

#endif
