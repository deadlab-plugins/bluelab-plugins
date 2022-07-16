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

#include "SampleDelay.h"

#include "IPlug_include_in_plug_src.h"

#define MAX_DELAY 2048

// HACK
#define VALUE_TEXT_OFFSET2 11
#define VALUE_TEXT_SIZE 12
#define VALUE2_TEXT_OFFSET 6
#define SAMPLE_DELAY_TEXT_OFFSET 4
#define VALUE_TEXT_OFFSET3 12

//
#if 0
PROBLEM: on Reason, when turning the knob, it rumbles more than with the other hosts

TODO: take care of editable text fields => There are two text fields !
#endif


static char *tooltipHelp = "Help - Display help";
static char *tooltipDelay = "Delay - Delay in samples and ms";

enum EParams
{
    kDelay = 0,
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kDelayX = 194,
    kDelayY = 36
};

//
SampleDelay::SampleDelay(const InstanceInfo &info)
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

SampleDelay::~SampleDelay()
{
    for (int i = 0; i < 2; i++)
    {
        if (mDelayObjs[i] != NULL)
            delete mDelayObjs[i];
    }

    if (mDelaySmoother != NULL)
        delete mDelaySmoother;

    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
SampleDelay::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
SampleDelay::MyMakeLayout(IGraphics *pGraphics)
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
SampleDelay::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mDelaySmoother = NULL;
    
    mDelayObjs[0] = NULL;
    mDelayObjs[1] = NULL;
  
    mTextControl = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
SampleDelay::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT defaultDelay = 0.0;
    
    mDelay = defaultDelay;;

    BL_FLOAT sampleRate = GetSampleRate();
    mDelaySmoother = new ParamSmoother2(sampleRate, defaultDelay);
  
    // Delay objs
    mDelayObjs[0] = new DelayObj4(mDelay);
    mDelayObjs[1] = new DelayObj4(mDelay);

    mIsInitialized = true;
}

void
SampleDelay::InitParams()
{
    BL_FLOAT defaultDelay = 0.0;
    GetParam(kDelay)->InitDouble("Delay", defaultDelay, 0, MAX_DELAY, 1, "smp");
}

void
SampleDelay::ProcessBlock(iplug::sample **inputs,
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
        BL_FLOAT delay = mDelaySmoother->Process();
        
        mDelayObjs[0]->SetDelay(delay);
        mDelayObjs[1]->SetDelay(delay);
        
        // Channel 0
        if ((in.size() > 0) && (out.size() > 0))
        {
            BL_FLOAT in0 = in[0].Get()[i];
            BL_FLOAT out0 = mDelayObjs[0]->ProcessSample(in0);
            out[0].Get()[i] = out0;
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
SampleDelay::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    IControl *control = mGUIHelper->CreateKnobSVG(pGraphics,
                                                  kDelayX, kDelayY,
                                                  kKnobWidth, kKnobHeight,
                                                  KNOB_FN,
                                                  //kDelayFrames,
                                                  kDelay,
                                                  TEXTFIELD_FN,
                                                  "DELAY",
                                                  GUIHelper12::SIZE_BIG,
                                                  NULL, true,
                                                  tooltipDelay);
  
    float knobWidth;
    float knobHeight;
    control->GetSize(&knobWidth, &knobHeight);
  
    int textX = kDelayX + knobWidth/2;
  
    int textYSamples = kDelayY + knobHeight +
        VALUE_TEXT_SIZE + VALUE_TEXT_OFFSET2 +
        VALUE2_TEXT_OFFSET + SAMPLE_DELAY_TEXT_OFFSET + VALUE_TEXT_OFFSET3;
    mTextControl =
        mGUIHelper->CreateValueText(pGraphics, textX, textYSamples,
                                    // Add spaces to avoid text clipping
                                    "       0 ms        ");
  
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
SampleDelay::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
SampleDelay::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    if (mDelaySmoother != NULL)
    {
        mDelaySmoother->Reset(sampleRate);
        mDelaySmoother->ResetToTargetValue(mDelay);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mDelayObjs[i] != NULL)
            mDelayObjs[i]->Reset();
    }

    UpdateDelayText();
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SampleDelay::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kDelay:
        {
            int delay = GetParam(kDelay)->Value();
            mDelay = delay;
            
            if (mDelaySmoother != NULL)
                mDelaySmoother->SetTargetValue(delay);

            UpdateDelayText();
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SampleDelay::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
SampleDelay::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mTextControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
SampleDelay::ApplyParams()
{        
    if (mDelaySmoother != NULL)
        mDelaySmoother->ResetToTargetValue(mDelay);

    UpdateDelayText();
}

void
SampleDelay::UpdateDelayText()
{
    // Compute the delay in ms
    BL_FLOAT sampleRate = GetSampleRate();
    if (sampleRate > 0.0)
    { 
        BL_FLOAT delayS = ((BL_FLOAT)mDelay)/sampleRate;
        BL_FLOAT delayMS = delayS*1000.0;
    
        char text[256];
        sprintf(text, "%.2f ms", delayMS);
    
        if (mTextControl != NULL)
        {
            mTextControl->SetStr(text);
            mTextControl->SetDirty(false);
        }
    }
}
