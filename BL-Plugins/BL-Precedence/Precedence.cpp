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

#include <DelayObj4.h>

#include <ParamSmoother2.h>

#include "Precedence.h"

#include "IPlug_include_in_plug_src.h"

//
#define MAX_DELAY 50.0

// 50ms at 44100x8
#define MAX_DELAY_SAMPLES 17540

// HACK
#define VALUE_TEXT_OFFSET2 11
#define VALUE_TEXT_SIZE 12
#define VALUE2_TEXT_OFFSET 6
#define VALUE_TEXT_OFFSET3 16

#define LR_TEXT_BACKGROUND 1

static char *tooltipHelp = "Help - Display help";
static char *tooltipDelay = "Delay - Delay on L or R channel";

//
#if 0
TODO: make the first text field editable, like in SampleDelay
 
PROBLEM (not repro): Protools Win
- when bypassed, the volume of the right channel increased

PROBLEM: on Reason, when turning the knob, it rumbles more than with the other hosts (in the opposite ear than where the sound is "panned") Same with Ableton Sierra and FLStudio Sierra.

IDEA: Emrah: "add a knob for max MS, so when the knob is at the maximum, it doesn t go over the max" (see facebook discussion) => (but "one knob" is cool !)

NOTE: we will have no sound until we have buffered enough samples on left and right
(this could be improved by starting playing the channel which has 0 delay)

TODO: take care of editable text fields => There are two text fields !
#endif
    
enum EParams
{
    kNormDelay = 0,
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kNormDelayX = 194,
    kNormDelayY = 36,

#if !LR_TEXT_BACKGROUND
    // -30 Y ?
    kTextLX = 170,
    kTextLY = 182,
    
    kTextRX = 282,
    kTextRY = 182
#endif
};

//
Precedence::Precedence(const InstanceInfo &info)
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

Precedence::~Precedence()
{
    for (int i = 0; i < 2; i++)
    {
        if (mDelayObjs[i] != NULL)
            delete mDelayObjs[i];
    }
    
    delete mNormDelaySmoother;

    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Precedence::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
Precedence::MyMakeLayout(IGraphics *pGraphics)
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
Precedence::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mNormDelaySmoother = NULL;
    
    mDelayObjs[0] = NULL;
    mDelayObjs[1] = NULL;
  
    mTextControlMs = NULL;
    mTextControlSamples = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
Precedence::Init()
{ 
    if (mIsInitialized)
        return;

    // Current
    mDelayL = 0.0;
    mDelayR = 0.0;
  
    // Delay objs
    mDelayObjs[0] = new DelayObj4(mDelayL);
    mDelayObjs[1] = new DelayObj4(mDelayR);
    
    BL_FLOAT defaultNormDelay = 0.0;
    mNormDelay = defaultNormDelay;
  
    BL_FLOAT sampleRate = GetSampleRate();
    mNormDelaySmoother = new ParamSmoother2(sampleRate, defaultNormDelay);

    mIsInitialized = true;
}

void
Precedence::InitParams()
{
    BL_FLOAT defaultNormDelay = 0.0;
    GetParam(kNormDelay)->InitDouble("NormalizedDelay", defaultNormDelay,
                                     -1.0, 1.0, 0.01, "");
}

void
Precedence::ProcessBlock(iplug::sample **inputs,
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

    // Fill with zeros just in case
    for (int i = 0; i < nFrames; i++)
    {
        out[0].Get()[i] = 0.0;
    
        if (out.size() > 1)
            out[1].Get()[i] = 0.0;
    }
  
    // Process samples
    for (int i = 0; i < nFrames; i++)
    {
        mNormDelay = mNormDelaySmoother->Process();
        
        ComputeDelays(false);
        
        mDelayObjs[0]->SetDelay(mDelayL);
        mDelayObjs[1]->SetDelay(mDelayR);
      
        // Channel 0
        if ((in.size() > 0) && (out.size() > 0))
        {
            BL_FLOAT in0 = in[0].Get()[i];
            BL_FLOAT out0 = mDelayObjs[0]->ProcessSample(in0);
            out[0].Get()[i] = out0;
        }
        
        // Channel mono in
        if ((in.size() < 2) && (out.size() > 1))
        {
            BL_FLOAT in0 = in[0].Get()[i];
            BL_FLOAT out1 = mDelayObjs[1]->ProcessSample(in0);
            out[1].Get()[i] = out1;
        }
        
        // Channel 1
        if ((in.size() > 1) && (out.size() > 1))
        {
            BL_FLOAT in1 = in[1].Get()[i];
            BL_FLOAT out1 = mDelayObjs[1]->ProcessSample(in1);
            out[1].Get()[i] = out1;
        }
    }
      
    // Copy output      
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
Precedence::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    IControl *control = mGUIHelper->CreateKnobSVG(pGraphics,
                                                  kNormDelayX, kNormDelayY,
                                                  kKnobWidth, kKnobHeight,
                                                  KNOB_FN,
                                                  //kNormDelayFrames,
                                                  kNormDelay,
                                                  TEXTFIELD_FN,
                                                  "DELAY",
                                                  GUIHelper12::SIZE_BIG,
                                                  NULL, false,
                                                  tooltipDelay);
  
    float knobWidth;
    float knobHeight;
    control->GetSize(&knobWidth, &knobHeight);
  
    int textX = kNormDelayX + knobWidth/2;
    int textYMs = kNormDelayY + knobHeight + VALUE_TEXT_OFFSET2 + VALUE_TEXT_OFFSET3;

    mTextControlMs = mGUIHelper->CreateValueText(pGraphics, textX, textYMs,
                                                 // Add spaces to avoid text clipping
                                                 "       0.00 ms       ");

#if 1 // Not editable value text!
    int textYSamples = kNormDelayY + knobHeight +
        VALUE_TEXT_SIZE + VALUE_TEXT_OFFSET2 + VALUE2_TEXT_OFFSET +
        VALUE_TEXT_OFFSET3;
    mTextControlSamples =
        mGUIHelper->CreateValueText(pGraphics, textX, textYSamples,
                                    // Add spaces to avoid text clipping
                                    "       0 smp       ");
#endif
    
#if !LR_TEXT_BACKGROUND
    // "L"
    mGUIHelper->CreateTitle(pGraphics,
                            kTextLX, kTextLY,
                            "L", GUIHelper12::SIZE_BIG, EAlign::Center);
  
    // "R"
    mGUIHelper->CreateTitle(pGraphics,
                            kTextRX, kTextRY,
                            "R", GUIHelper12::SIZE_BIG, EAlign::Center);
#endif
    
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
Precedence::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Precedence::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mNormDelaySmoother != NULL)
    {
        mNormDelaySmoother->Reset(sampleRate);
        mNormDelaySmoother->ResetToTargetValue(mNormDelay);
    }
  
    // Sample rate may have changed
    // Recompute the dealys (for ms)
    ComputeDelays(true);

    for (int i = 0; i < 2; i++)
    {
        if (mDelayObjs[i] != NULL)
            mDelayObjs[i]->Reset();
    }
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Precedence::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kNormDelay:
        {
            BL_FLOAT normDelay = GetParam(kNormDelay)->Value();
            mNormDelay = normDelay;
            
            if (mNormDelaySmoother != NULL)
                mNormDelaySmoother->SetTargetValue(normDelay);
      
            ComputeDelays(true);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Precedence::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
Precedence::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mTextControlMs = NULL;
    mTextControlSamples = NULL;
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Precedence::ApplyParams()
{
    if (mNormDelaySmoother != NULL)
        mNormDelaySmoother->ResetToTargetValue(mNormDelay);
    
    ComputeDelays(true);
}

void
Precedence::ComputeDelays(bool updateTextFields)
{
#define PARAM_SHAPE 0.375
  
    BL_FLOAT normDelayL = 0.0;
    BL_FLOAT normDelayR = 0.0;
  
    BL_FLOAT delayL = 0.0;
    BL_FLOAT delayR = 0.0;
  
    char textMs[256];
    char textSamples[256];
  
    if (mNormDelay <= 0.0)
    {
        normDelayL = -mNormDelay;
        normDelayL = BLUtils::ApplyParamShape(normDelayL, PARAM_SHAPE);
        delayL = normDelayL*MAX_DELAY;
    
        sprintf(textMs, "%.2f ms", delayL);
    }
  
    if (mNormDelay > 0.0)
    {
        normDelayR = mNormDelay;
        normDelayR = BLUtils::ApplyParamShape(normDelayR, PARAM_SHAPE);
        delayR = normDelayR*MAX_DELAY;
    
        sprintf(textMs, "%.2f ms", delayR);
    }
  
    // Compute the delay in samples
    BL_FLOAT sampleRate = GetSampleRate();
    if (sampleRate > 0.0)
    {
        // Invert delays, to use the DELAY knob "as a pan"
        mDelayR = (delayL/1000.0)*sampleRate;
        mDelayL = (delayR/1000.0)*sampleRate;
    }
  
    if (updateTextFields)
    {
        // Update thext control Ms
        //mTextControlMs->SetTextFromPlug(textMs);
        if (mTextControlMs != NULL)
        {
            mTextControlMs->SetStr(textMs);
            mTextControlMs->SetDirty(false);
        }
    }
  
    if (mNormDelay <= 0.0)
    {
        sprintf(textSamples, "%d smp", (int)mDelayR);
    }
  
    if (mNormDelay > 0.0)
    {
        sprintf(textSamples, "%d smp", (int)mDelayL);
    }
  
    if (updateTextFields)
    {
        // Update thext control Ms
        if (mTextControlSamples != NULL)
        {
            mTextControlSamples->SetStr(textSamples);
            mTextControlSamples->SetDirty(false);
        }
    }
}
