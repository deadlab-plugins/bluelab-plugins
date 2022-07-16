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
 
#ifndef __PITCHSHIFT__
#define __PITCHSHIFT__

#include <vector>
using namespace std;

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

// DEBUG: Pitch obj instead of transient booster + pitch
#define DEBUG_PITCH_OBJ 0

using namespace iplug;
using namespace igraphics;

class GUIHelper12;
class DelayObj4;
class PitchShifterInterface;
class PitchShift final : public Plugin
{
public:
    PitchShift(const InstanceInfo &info);
    virtual ~PitchShift();

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
    void Init();
    void ApplyParams();

    //
    void QualityChanged();

    enum Method
    {
        PITCH_SHIFT_SMB = 0,
        PITCH_SHIFT_PRUSA_MIX,
        PITCH_SHIFT_PRUSA_TRANSIENTS
    };
    
    void SetMethod(Method method);
    void MethodChanged();
        
    void UpdateLatency();
        
    static BL_FLOAT ComputeFactor(BL_FLOAT val);

    //
    BL_FLOAT mFactor;
    bool mFactorChanged;
    
    BL_FLOAT mTransBoost;
    
    IControl *mTransBoostControl;
    IControl *mQualityControl;
    
    enum Quality
    {
        STANDARD = 0,
        HIGH,
        VERY_HIGH,
        OFFLINE
    };
    
    enum Quality mHarmoQuality;
    
    // For FIX_FREEZE_CUBASE10_VST3
    bool mQualityChanged;

    enum Method mMethod;
    bool mMethodChanged;
    
    //
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    GUIHelper12 *mGUIHelper;
    
    // 2, for 2 methods
    PitchShifterInterface *mPitchShifters[2];

    // May be used with some types of PITCH_SHIFTER_CLASS
    int mNumChannels;

    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;

    ParamSmoother2 *mDryWetSmoother;
    BL_FLOAT mDryWet;

    // ipnut delays, for dry wet synch
    DelayObj4 *mInputDelays[2];

    BLUtilsPlug mBLUtilsPlug;
    
private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    
    BL_PROFILE_DECLARE;
};

#endif
