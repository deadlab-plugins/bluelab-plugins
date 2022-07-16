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

#include <KnobSmoother.h>

#include <IBLSwitchControl.h>

#include "Sine.h"

#include "IPlug_include_in_plug_src.h"


#define USE_MAX_PRECISION 1

// Fix clicks when turning the freq knob
#define FIX_CLICKS 1

// New method, makes less discontinuities when turning knob
#define USE_KNOB_SMOOTHER 1

#if 0 
TODO: try to use WDL optimized oscillator
          
DONE: make a "mon" button, so it plays the sine sound even when play is not pressed
(remark from gearslutz discussion)

TODO: make the frequency change without sound crackles (if possible to implement it quickly) => it would be useful for demos !

Bug in Logic, old version (unsolvable):
When the plugin is bypassed, when start playback, there is one buffer
and ProcessDoubleReplacing is called once before really bypassing
See: https://www.logicprohelp.com/forum/viewtopic.php?t=31237
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipFreq = "Frequency - Frequency of sine wave";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipPassThru = "Pass Thru - Generate sine wave over input";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";

enum EParams
{
    kFreq = 0,
    
    kPassThru,
    kMonitor,
    kOutGain,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kFreqX = 194,
    kFreqY = 37,
    
    kOutGainX = 360,
    kOutGainY = 72,
  
    kPassThruX = 60,
    kPassThruY = 28,

    kCheckboxMonitorX = 349,
    kCheckboxMonitorY = 28
};

//
Sine::Sine(const InstanceInfo &info)
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

Sine::~Sine()
{
#if !USE_KNOB_SMOOTHER
    if (mFreqSmoother != NULL)
        delete mFreqSmoother;
#else
    if (mFreqKnobSmoother != NULL)
        delete mFreqKnobSmoother;
#endif
    
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Sine::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
Sine::MyMakeLayout(IGraphics *pGraphics)
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

#if 0 //1
    pGraphics->ShowFPSDisplay(true);
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
Sine::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mOutGainSmoother = NULL;

    mFreqSmoother = NULL;
    mFreqKnobSmoother = NULL;

    mIsPlaying = false;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
Sine::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = defaultOutGain;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);

    BL_FLOAT defaultFreq = 440.0;
    
    // Use custom damp, to be reactive
#if !FIX_CLICKS
    mFreqSmoother = new ParamSmoother2(sampleRate, defaultFreq, 0.0001);
#else
#if !USE_KNOB_SMOOTHER
    // Origin: makes some steps when turning the knob
    // (no step when automation)
    mFreqSmoother = new ParamSmoother2(sampleRate, defaultFreq, 14.0); // 140.0 ?
#endif

#if USE_KNOB_SMOOTHER
    mFreqKnobSmoother = new KnobSmoother(this, kFreq, sampleRate);
#endif

#endif
    
    mTime = 0;
    mPhase = 0.0;
  
    mIsInitialized = true;
}

void
Sine::InitParams()
{
    BL_FLOAT defaultFreq = 440.0;
    mFreq = defaultFreq;
    
#if !USE_MAX_PRECISION
    GetParam(kFreq)->InitDouble("Freq", defaultFreq, 0.1, 22000.0, 0.1, "Hz",
                                0, "", IParam::ShapePowCurve(3.0));
#else
    GetParam(kFreq)->InitDouble("Freq", defaultFreq, 0.01, 22000.0, 0.01, "Hz",
                                0, "", IParam::ShapePowCurve(3.0));
#endif

    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -24.0, 24.0, 0.1, "dB");

    // Pass Thru
    mPassThru = false;
    //GetParam(kPassThru)->InitInt("PassThru", 0, 0, 1);
    GetParam(kPassThru)->InitEnum("PassThru", 0, 2,
                                  "", IParam::kFlagsNone, "",
                                  "Off", "On");

    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
}

void
Sine::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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
#if 1 // Original code (not to be used with 0 inputs and "generator" AU type
    if (in.empty() || out.empty())
        return;
#endif
#if 0 // in can be empty, because BL-Sine can be a "generator" AU plugin
    if (out.empty())
        return;
#endif
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes,
    // this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
  
    mSecureRestarter.Process(in);

    // Set the outputs to 0
    //
    // FIX: on Protools, in render mode, after play is finished,
    // there is a buzz sound
    for (int i = 0; i < out.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &out0 = out[i];
        BLUtils::FillAllZero(&out0);
    }
    
    mIsPlaying = IsTransportPlaying();
    if (mIsPlaying || mMonitorEnabled)
    {
        BL_FLOAT sampleRate = GetSampleRate();
        BL_FLOAT sampleRateInv = 1.0/sampleRate;
        
        // Signal generation
#if !FIX_CLICKS
        // Version without phase correction
        // Make clicks when turning the freq knob
        for (int i = 0; i < nFrames; i++)
        {
            // Parameters update
            BL_FLOAT freq = mFreqSmoother->Process();
      
            BL_FLOAT phase = 0.0;
      
            BL_FLOAT amp = 0.25;
            
            BL_FLOAT t = ((BL_FLOAT)mTime)*sampleRateInv;
            BL_FLOAT val = amp*sin(TWO_PI*freq*t + phase);
      
            mTime++;
      
            if (out.size() > 0)
                out[0].Get()[i] = val;
          
            if (out.size() > 1)
                out[1].Get()[i] = val;
        }
#else
        // Version with phase correction
        // The sound is very continupus when turning the freq knob
        // The phase is adjusted when frequency changes, in order
        // to keep waveform continuity.
        for (int i = 0; i < nFrames; i++)
        {
#if !USE_KNOB_SMOOTHER
            BL_FLOAT prevFreq = mFreqSmoother->PickCurrentValue();
#else
            BL_FLOAT prevFreq = mFreqKnobSmoother->PickCurrentValue();
#endif

            // Parameters update
#if !USE_KNOB_SMOOTHER
            BL_FLOAT freq = mFreqSmoother->Process();
#else
            BL_FLOAT freq = mFreqKnobSmoother->Process();
#endif

            BL_FLOAT amp = 0.25;
      
            BL_FLOAT t = ((BL_FLOAT)mTime)*sampleRateInv;
      
            // Update phase in case frequency changed,
            // to guarantee waveform continuity
            mPhase += TWO_PI*t*(prevFreq - freq);
      
            BL_FLOAT val = amp*sin(TWO_PI*freq*t + mPhase);
      
            mTime++;
      
            if (out.size() > 0)
                out[0].Get()[i] = val;
      
            if (out.size() > 1)
                out[1].Get()[i] = val;
        }
#endif

        // Apply out gain
        BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
        
        if (mPassThru)
        {
            for (int i = 0; i < nFrames; i++)
            {
                out[0].Get()[i] += in[0].Get()[i];
        
                if ((out.size() > 1) && (in.size() > 1))
                    out[1].Get()[i] += in[1].Get()[i];
            }
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
Sine::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kFreqX, kFreqY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,                              
                              kFreq,
                              TEXTFIELD_FN,
                              "FREQ",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipFreq);

    // Out Gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX, kOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT, NULL, true,
                              tooltipOutGain);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kPassThruX,
                                   kPassThruY,
                                   CHECKBOX_FN, kPassThru, "PASS THRU",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipPassThru);

    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX,
                                                     kCheckboxMonitorY,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT,
                                                     true,
                                                     tooltipMonitor);
    
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
Sine::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Sine::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
#if !USE_KNOB_SMOOTHER
    if (mFreqSmoother != NULL)
        mFreqSmoother->Reset(sampleRate);
#else
    if (mFreqKnobSmoother != NULL)
        mFreqKnobSmoother->Reset(sampleRate);
#endif
    
    mTime = 0;
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Sine::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
#if !USE_KNOB_SMOOTHER
        case kFreq:
        {
            BL_FLOAT freq = GetParam(kFreq)->Value();

            mFreq = freq;

            if (mFreqSmoother != NULL)
                mFreqSmoother->SetTargetValue(freq);
        }
        break;
#endif

        case kOutGain:
        {
            BL_FLOAT outGain = GetParam(kOutGain)->DBToAmp();
            mOutGain = outGain;

            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(outGain);
        }
        break;

        case kPassThru:
        {
            int value = GetParam(kPassThru)->Value();
            mPassThru = (value == 1);
        }
        break;

        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            
            mMonitorEnabled = (value == 1);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Sine::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
Sine::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mMonitorControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Sine::ApplyParams()
{
#if !USE_KNOB_SMOOTHER
    if (mFreqSmoother != NULL)
        mFreqSmoother->ResetToTargetValue(mFreq);
#else
    if (mFreqKnobSmoother != NULL)
        mFreqKnobSmoother->ResetToTargetValue();
#endif

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
}

void
Sine::OnIdle()
{
    ENTER_PARAMS_MUTEX;

    if (mUIOpened)
    {
        if (mMonitorControl != NULL)
        {
            bool disabled = mMonitorControl->IsDisabled();
            if (disabled != mIsPlaying)
                mMonitorControl->SetDisabled(mIsPlaying);
        }
    }
    
    LEAVE_PARAMS_MUTEX;
}
