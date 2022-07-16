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

#include "StereoPan.h"

#include "IPlug_include_in_plug_src.h"

// No reduction at the center
#define LINEAR_TAPER 0

// -3dB at the center
#define SQRT_TAPER 1

// -3dB at the center
#define SIN_COS_TAPER 0

#if 0
TODO: add a widget for the selection of the pan law
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipLeftPan = "Left Pan - Pan left channel";
static char *tooltiRightPan = "Right Pan - Pan right channel";

enum EParams
{
    kLeftPan = 0,
    kRightPan,
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kLeftPanX = 118,
    kLeftPanY = 38,
    
    kRightPanX = 270,
    kRightPanY = 38
};

//
StereoPan::StereoPan(const InstanceInfo &info)
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

StereoPan::~StereoPan()
{
    if (mLeftPanSmoother != NULL)
        delete mLeftPanSmoother;
    
    if (mRightPanSmoother != NULL)
        delete mRightPanSmoother;

    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
StereoPan::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
StereoPan::MyMakeLayout(IGraphics *pGraphics)
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
StereoPan::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    mLeftPanSmoother = NULL;
    mRightPanSmoother = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;

    mLeftPan = 0.0;
    mRightPan = 1.0;
}

void
StereoPan::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    BL_FLOAT defaultLeftPan = 0.0; //-100.0;
    mLeftPanSmoother = new ParamSmoother2(sampleRate, defaultLeftPan);
    
    BL_FLOAT defaultRightPan = 1.0; //100.0;
    mRightPanSmoother = new ParamSmoother2(sampleRate, defaultRightPan);
    
    mIsInitialized = true;
}

void
StereoPan::InitParams()
{
    BL_FLOAT defaultLeftPan = -100.0;
    mLeftPan = defaultLeftPan/100.0;
    GetParam(kLeftPan)->InitDouble("LeftPan", defaultLeftPan, -100., 100.0, 1.0, "%");

    BL_FLOAT defaultRightPan = 100.0;
    mRightPan = defaultRightPan/100.0;
    GetParam(kRightPan)->InitDouble("RightPan", defaultRightPan,
                                    -100., 100.0, 1.0, "%");
}

void
StereoPan::ProcessBlock(iplug::sample **inputs,
                        iplug::sample **outputs, int nFrames)
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

    // Signal processing
    for (int i = 0; i < nFrames; i++)
    {
        if (mLeftPanSmoother == NULL)
            break;

        if (mRightPanSmoother == NULL)
            break;
      
        // Parameters update
        mLeftPan = mLeftPanSmoother->Process();
      
        mRightPan = mRightPanSmoother->Process();
        
        BL_FLOAT leftSample = in[0].Get()[i];
        BL_FLOAT rightSample = (in.size() < 2) ? leftSample : in[1].Get()[i];

#if LINEAR_TAPER
        out[0].Get()[i] = (1.0 - mLeftPan)*leftSample + (1.0 - mRightPan)*rightSample;
      
        if (out.size() > 1)
            out[1].Get()[i] = mLeftPan*leftSample + mRightPan*rightSample;
#endif
    
#if SQRT_TAPER
        out[0].Get()[i] =
            sqrt(1.0 - mLeftPan)*leftSample +
            sqrt(1.0 - mRightPan)*rightSample;
      
        if (out.size() > 1)
            out[1].Get()[i] =
                sqrt(mLeftPan)*leftSample +
                sqrt(mRightPan)*rightSample;
#endif
      
#if SIN_COS_TAPER
        out[0].Get()[i] = cos(mLeftPan*M_PI/2.0)*leftSample +
        cos(mRightPan*M_PI/2.0)*rightSample;
      
        if (out.size() > 1)
            out[1].Get()[i] = sin(mLeftPan*M_PI/2.0)*leftSample +
            sin(mRightPan*M_PI/2.0)*rightSample;
#endif
    }

    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
StereoPan::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kLeftPanX, kLeftPanY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kLeftPan,
                              TEXTFIELD_FN,
                              "LEFT",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipLeftPan);

    mGUIHelper->CreateKnobSVG(pGraphics,
                              kRightPanX, kRightPanY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kRightPan,
                              TEXTFIELD_FN,
                              "RIGHT",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltiRightPan);
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //kLogoAnimFrames, GUIHelper12::BOTTOM);
    
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
StereoPan::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
StereoPan::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
StereoPan::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kLeftPan:
        {
            BL_FLOAT leftPan = GetParam(kLeftPan)->Value();
            leftPan = (leftPan/100.0 + 1.0)/2.0;
            mLeftPan = leftPan;
            
            if (mLeftPanSmoother != NULL)
                mLeftPanSmoother->SetTargetValue(mLeftPan);
        }
        break;
        
        case kRightPan:
        {
            BL_FLOAT rightPan = GetParam(kRightPan)->Value();
            rightPan = (rightPan/100.0 + 1.0)/2.0;
            mRightPan = rightPan;
            
            if (mRightPanSmoother != NULL)
                mRightPanSmoother->SetTargetValue(mRightPan);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoPan::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoPan::OnUIClose()
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
StereoPan::ApplyParams()
{
    if (mLeftPanSmoother != NULL)
        mLeftPanSmoother->ResetToTargetValue(mLeftPan);

    if (mRightPanSmoother != NULL)
        mRightPanSmoother->ResetToTargetValue(mRightPan);
}
