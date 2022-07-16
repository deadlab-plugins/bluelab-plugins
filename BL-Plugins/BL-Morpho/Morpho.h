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
 
#ifndef __MORPHO__
#define __MORPHO__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"
//#include "IMidiQueue.h"
#include "IPlugMidi.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

#include <SoSourcesView.h>
#include <SySourcesView.h>

#include <View3DPluginInterface.h>

#include <Morpho_defs.h>

using namespace iplug;
using namespace igraphics;

class GUIHelper12;
class ParamSmoother2;
class Panel;
class MorphoObj;
class Morpho final : public Plugin,
                     public SoSourcesViewListener,
                     public SySourcesViewListener,
                     public View3DPluginInterface
{
 public:
    Morpho(const InstanceInfo &info);
    virtual ~Morpho();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;

    void OnIdle() override;

    bool OnKeyDown(const IKeyPress& key) override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    // View3DPluginInterface
    void SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1) override;
    void SetCameraFov(BL_FLOAT angle) override;
    
    // SoSourcesViewListener
    void SoSourceChanged(const SoSource *source) override;
    void OnRemoveSource(int sourceNum) override;
        
    // SySourcesViewListener
    void SySourceChanged(const SySource *source) override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);
    
    void CreateControls(IGraphics *pGraphics);
    void CreateSourcesControls(IGraphics *pGraphics);
    void CreateSynthesisControls(IGraphics *pGraphics);
        
    void InitNull();
    void InitParams();
    void Init();
    void ApplyParams();

    void PlugModeChanged();

    void SyCurrentSourceMutedChanged();
    void SyCurrentSourceTypeMixChanged();
    void CheckSoCurrentSourceControlsDisabled();
    void SyCurrentSourceSynthTypeChanged();
    void SoCurrentSourceApplyChanged();
    void SyCurrentSourceTypeChanged();
    
    void RefreshWaterfallAngles();
    
    void SetTabsBarStyle(ITabsBarControl *ẗabsBar);

    //
    BL_FLOAT PitchFactorToHt(BL_FLOAT factor);
    BL_FLOAT PitchHtToFactor(BL_FLOAT ht);
            
    BL_FLOAT FactorToParam(BL_FLOAT factor);
    BL_FLOAT ParamToFactor(BL_FLOAT param);
    
    //
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    GUIHelper12 *mGUIHelper;

    BLUtilsPlug mBLUtilsPlug;

    //
    Panel *mSourcesPanel;
    Panel *mSynthPanel;

    MorphoPlugMode mPlugMode;

    // Keep controls to gray them out when necessary
    vector<IControl *> mSyCurrentSourceControls;
    vector<IControl *> mSyCurrentSourceSynthTypeControls;
    vector<IControl *> mSyCurrentSourceTypeFileControls;
    vector<IControl *> mSyCurrentSourceMixHide;
    IControl *mSoApplyButtonControl;

    
    MorphoObj *mMorphoObj;    

    vector<IControl *> mSoCurrentSourceControls;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
  
    BL_PROFILE_DECLARE;
};

#endif
