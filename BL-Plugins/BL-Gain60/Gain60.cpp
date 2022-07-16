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
 
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <vector>
using namespace std;

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include <ParamSmoother2.h>

#include "Gain60.h"

#include "IPlug_include_in_plug_src.h"

static char *tooltipHelp = "Help - Display Help";
static char *tooltipGain = "Gain";

enum EParams
{
    kGain = 0,    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kGainX = 194,
    kGainY = 37
};

//
Gain60::Gain60(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    TRACE;
    
    InitNull();
    InitParams();

    Init();

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    //MakeDefaultPreset((char *) "-", kNumPrograms);
    
    BL_PROFILE_RESET;
}


Gain60::~Gain60()
{
    if (mGainSmoother != NULL)
        delete mGainSmoother;

    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Gain60::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
Gain60::MyMakeLayout(IGraphics *pGraphics)
{
    ENTER_PARAMS_MUTEX;

    // IGraphics: DEFAULT_FONT
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    
    pGraphics->LoadFont("font-regular", FONT_REGULAR_FN);
    pGraphics->LoadFont("font-light", FONT_LIGHT_FN);
    pGraphics->LoadFont("font-bold", FONT_BOLD_FN);

    // Style: BLULAB_V3
    pGraphics->LoadFont("OpenSans-ExtraBold", FONT_OPENSANS_EXTRA_BOLD_FN);
    pGraphics->LoadFont("Roboto-Bold", FONT_ROBOTO_BOLD_FN);
    
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachBackground(BACKGROUND_FN);
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif

    // For rollover buttons
    pGraphics->EnableMouseOver(true);

    pGraphics->EnableTooltips(true);
    pGraphics->SetTooltipsDelay(TOOLTIP_DELAY);
    
    CreateControls(pGraphics);
  
    ApplyParams();
  
    // Demo mode
    mDemoManager.Init(this, pGraphics);
  
    mUIOpened = true;
  
    LEAVE_PARAMS_MUTEX;
}

void
Gain60::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mGain = 0.0;
    
    mGainSmoother = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
Gain60::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT defaultGain = 1.0; // 1 is 0dB
    mGain = defaultGain;

    BL_FLOAT sampleRate = GetSampleRate();
    mGainSmoother = new ParamSmoother2(sampleRate, defaultGain);
    
    mIsInitialized = true;
}

void
Gain60::InitParams()
{
    BL_FLOAT defaultGain = 0.0;
    mGain = 1.0; // 1 is 0dB
    GetParam(kGain)->InitDouble("Gain", defaultGain,
                                -GAIN_VALUE, GAIN_VALUE, 0.1, "dB");
}

void
Gain60::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();
  
    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > &scIn = mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf2;
    BLUtilsPlug::GetPlugIOBuffers(this, inputs, outputs, nFrames,
                              &in, &scIn, &out);
  
    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
        return;
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes,
    // this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
  
    mSecureRestarter.Process(in);

    // Apply output gain
    BLUtilsPlug::ApplyGain(in, &out, mGainSmoother);
      
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
Gain60::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
 
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kGainX, kGainY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kGain,
                              TEXTFIELD_FN,
                              "GAIN",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipGain);

    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //                           kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN, GUIHelper12::BOTTOM);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM,
                                 tooltipHelp);
  
    mGUIHelper->CreateDemoMessage(pGraphics);
  
    mControlsCreated = true;
}

void
Gain60::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Gain60::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    if (mGainSmoother != NULL)
        mGainSmoother->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Gain60::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
      
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kGain:
        {
            BL_FLOAT gain = GetParam(kGain)->DBToAmp();
            mGain = gain;
      
            if (mGainSmoother != NULL)
                mGainSmoother->SetTargetValue(mGain);
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Gain60::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Gain60::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Gain60::ApplyParams()
{
    if (mGainSmoother != NULL)
        mGainSmoother->ResetToTargetValue(mGain);
}
