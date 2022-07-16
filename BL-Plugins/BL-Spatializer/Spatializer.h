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
 
#ifndef __SPATIALIZER__
#define __SPATIALIZER__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"
#include "IPlugMidi.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

#define USE_SMOOTH_CONVOLVER 1

using namespace iplug;
using namespace igraphics;

// TEST: test was done to update the response every 1024 samples
// => does not work

// TEST: a test was done with FftProcessObj
// => the sound was bad

// NOTE: before, we used directly a simple convolver
// That was removed because that didn't work anymore

#define USE_FIXED_BUFFER_OBJ 1

#if USE_FIXED_BUFFER_OBJ
class FixedBufferObj;
#endif

class GUIHelper12;
class KemarHRTF;
class HRTF;
class FftConvolver6;
class BufProcessObjSmooth2;
class ParamSmoother2;
class Spatializer final : public Plugin
{
 public:
    Spatializer(const InstanceInfo &info);
    virtual ~Spatializer();

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

    void CreateHrtf();
    
    static BL_FLOAT ComputeEarDistance(BL_FLOAT elevation, BL_FLOAT azimuth,
                                       BL_FLOAT interauralDistance, BL_FLOAT sourceDistance,
                                       int earNum);
    
    static BL_FLOAT ComputeDelayWidth(BL_FLOAT elevation, BL_FLOAT azimuth,
                                      int chan, BL_FLOAT apparentSourceWidth);
  
    static BL_FLOAT ComputeDelayDistance(BL_FLOAT elevation, BL_FLOAT azimuth, int chan,
                                         BL_FLOAT sourceDistance);
  
    static void NormalizeDelays(BL_FLOAT delays[2]);
    
    static void IncreaseDelays(BL_FLOAT delays[2], BL_FLOAT factor);
    
    void ResampleImpulse(WDL_TypedBuf<BL_FLOAT> *impulse);

    
#if USE_FIXED_BUFFER_OBJ
    void UpdateLatency();
#endif

    //
    /* Spatialization algorithm members. */
  
    // 0 to 360 degrees
    BL_FLOAT mAzimuth;
  
    // -40 to 90 degrees
    BL_FLOAT mElevation;
    
    // 1 to 90
    BL_FLOAT mSourceWidth;
    
    // 0 to 1
    BL_FLOAT mOutGain;
    
    // This one is necessary to void pops when changing the gain
    ParamSmoother2 *mOutGainSmoother;
    
    // This is necessary, to avoid crackles (maximum width)
    // (Test done: automation + display azim curve => small steps)
    ParamSmoother2 *mAzimSmoother;
    ParamSmoother2 *mElevSmoother;
    
    ParamSmoother2 *mSourceWidthSmoother;
    
#if USE_SMOOTH_CONVOLVER
    BufProcessObjSmooth2 *mConvolvers[2];
#else
    // Fft convolution of impulse response
    // One converver for each channels
    FftConvolver6 *mConvolvers[2];
#endif
    
    // The Hrtf to load and to convolve
    HRTF *mHrtf;
    
    // Does impulse need update ?
    bool mImpulseUpdate;
    
#if USE_FIXED_BUFFER_OBJ
    FixedBufferObj *mFixedBufObj;
#endif
    
    //
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    GUIHelper12 *mGUIHelper;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    
    BL_PROFILE_DECLARE;
};

#endif
