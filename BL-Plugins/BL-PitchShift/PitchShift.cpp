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

#include <FftProcessObj16.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include "PitchShift.h"

#include "IPlug_include_in_plug_src.h"


#include <PitchShifterInterface.h>
#include <PitchShifter.h>
#include <PitchShifterSmb.h>
#include <PitchShifterBLSmb.h>
#include <PitchShifterPrusa.h>
#include <PitchShifterJh.h>
#include <PitchShifterPV.h>

#include <ParamSmoother2.h>
#include <DelayObj4.h>

#include <IRadioButtonsControl.h>

//Debug stereo phases
#define DEBUG_GRAPH_PHASES 0
#if DEBUG_GRAPH_PHASES

#define DEBUG_GRAPH 1
#include <DebugGraph.h>

// TODO: set the correct number
#define GRAPH_NUM_CURVES 14
#endif

#define USE_DROP_DOWN_MENU 1

// For information about different algorithms, see
// https://www.katjaas.nl/pitchshift/pitchshift.html

#if 0
/*
NOTE: there are two pitch sift files in WDL!!! :)
IDEA: maybe try to use r8brain to pitch shift / simple algo

TODO: do not remove from the catalog (better)do not remove (on Gearslutz, one user said it was very good for "continuous" sounds) => Then add a second mode (implement a simple/basic pitch shift algorithm which keep transients e.g by resampling) => and add a checkbox to choose which mode (test some other pitch shift plugins in the market!)

TODO(not to do...): remove it from the catalog, and refund users with a coupon

PROBLEM: Ableton, Mac Sierra, AU => change quality while playing
=> this makes a strage stereo effect
=> if we stop and restart playback, the stereo effect vanishes

TODO: maybe compare with version v4.4,
to check if StereoPhasesProcess really worked at this time

NOTE: the sound seems very better when using at 88KHz sample rate
(this may be due to the variable buffer size, and then we have
 more frequency precision)
*/
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipFactor = "Factor - Pitch factor, in half tones";
static char *tooltipQuality =
    "Harmonic Quality - Processing quality for harmonic mode";
static char *tooltipTransBoost = "Transient Boost - Boost output transients";
static char *tooltipMethod =
    "Method - Harmonic, harmonic + transient or transient only";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipDryWet =
    "Dry/Wet - Adjust between original signal and pitched signal";

enum EParams
{
    kMethod = 0,
    
    kFactor,
    kQuality,
    kTransBoost,

    kDryWet,
    kOutGain,
    
    //kDebugGraph,
    
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
    
    kFactorX = 146,
    kFactorY = 90,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsQualityX = 400,
    kRadioButtonsQualityY = 64,
    kRadioButtonQualityVSize = 101,
    kRadioButtonQualityNumButtons = 4,
#else
    kQualityX = 257,
    kQualityY = 89,
    kQualityWidth = 80,
#endif
    
    kTransBoostX = 284,
    kTransBoostY = 124,

    kRadioButtonsMethodX = 15,
    kRadioButtonsMethodY = 50,
    kRadioButtonMethodVSize = 68,
    kRadioButtonMethodNumButtons = 3,

    kOutGainX = 391,
    kOutGainY = 124,

    kDryWetX = 391,
    kDryWetY = 24
};

//
PitchShift::PitchShift(const InstanceInfo &info)
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

PitchShift::~PitchShift()
{ 
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    for (int i = 0; i < 2; i++)
    {
        if (mPitchShifters[i] != NULL)
            delete mPitchShifters[i];
    }

    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
    if (mDryWetSmoother != NULL)
        delete mDryWetSmoother;

    for (int i = 0; i < 2; i++)
    {
        if (mInputDelays[i] != NULL)
            delete mInputDelays[i];
    }
}

IGraphics *
PitchShift::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
PitchShift::MyMakeLayout(IGraphics *pGraphics)
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
PitchShift::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    // Init WDL FFT
    FftProcessObj16::Init();
  
    mUIOpened = false;
    mControlsCreated = false;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;

    mTransBoostControl = NULL;
    mQualityControl = NULL;
    
    //
    mPitchShifters[0] = NULL;
    mPitchShifters[1] = NULL;
    mNumChannels = 1;

    mOutGainSmoother = NULL;
    mDryWetSmoother = NULL;

    for (int i = 0; i < 2; i++)
        mInputDelays[i] = NULL;
}

void
PitchShift::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();

    for (int i = 0; i < 2; i++)
    {
        if (mPitchShifters[i] == NULL)
        {
            if (i == 0)
                mPitchShifters[i] = new PitchShifterBLSmb();
            else if (i == 1)
                mPitchShifters[i] = new PitchShifterPrusa();
            
            mPitchShifters[i]->SetNumChannels(mNumChannels);        
            mPitchShifters[i]->SetFactor(mFactor);
            mPitchShifters[i]->SetQuality(0);
            mPitchShifters[i]->Reset(sampleRate, blockSize);
        }
    }

    UpdateLatency();

    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);

    BL_FLOAT defaultDryWet = 1.0;
    mDryWetSmoother = new ParamSmoother2(sampleRate, defaultDryWet);

    int shifterNum = (mMethod == 0) ? 0 : 1;
    int latency = mPitchShifters[shifterNum]->ComputeLatency(blockSize);
    for (int i = 0; i < 2; i++)
        mInputDelays[i] = new DelayObj4(latency);

    mMethodChanged = true;
    mFactorChanged = true;
    mQualityChanged = true;
    
    mIsInitialized = true;
}

void
PitchShift::InitParams()
{
    BL_FLOAT defaultFactor = 0.0;
    BL_FLOAT defaultFactorNorm = 1.0;
    mFactor = defaultFactorNorm;
    mFactorChanged = true;
  
    GetParam(kFactor)->InitDouble("Factor", defaultFactor, -12.0, 12.0, 0.01, "ht");

    // Transient boost factor
    double defaultTransBoost = 0.0;
    mTransBoost = defaultTransBoost;
    GetParam(kTransBoost)->InitDouble("TransBoost", defaultTransBoost,
                                      0.0, 100.0, 0.1, "%");

    // Quality
    Quality defaultQuality = STANDARD;
    mHarmoQuality = defaultQuality;
#if !USE_DROP_DOWN_MENU
    GetParam(kQuality)->InitInt("HarmoQuality", (int)defaultQuality, 0, 3);
#else
    GetParam(kQuality)->InitEnum("HarmoQuality", 0, 4, "", IParam::kFlagsNone,
                                 "", "1 - Fast", "2", "3", "4 - Best");
#endif
    
    // PitchShift method
    Method defaultMethod = PITCH_SHIFT_SMB;
    mMethod = defaultMethod;
    //GetParam(kMethod)->InitInt("Method", (int)defaultMethod, 0, 2);
    GetParam(kMethod)->InitEnum("Method", (int)defaultMethod, 3,
                                "", IParam::kFlagsNone, "",
                                "Harmonic", "Mix", "Transient");

    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 1 in amp is 0dB //defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");

    // Out gain
    BL_FLOAT defaultDryWet = 100.0;
    mDryWet = defaultDryWet*0.01; 
    GetParam(kDryWet)->InitDouble("DryWet", defaultDryWet, 0.0, 100.0, 0.1, "%");
}

void
PitchShift::ProcessBlock(iplug::sample **inputs,
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

    if (mMethodChanged)
    {
        MethodChanged();

        mMethodChanged = false;
    }
    
    if (mFactorChanged)
    {
        for (int i = 0; i < 2; i++)
        {
            if (mPitchShifters[i] != NULL)
                mPitchShifters[i]->SetFactor(mFactor);
        }
        
        mFactorChanged = false;
    }

    if (mQualityChanged)
    {
        // Apply quality only on PitchShifterBLSmb
        if (mPitchShifters[0] != NULL)
            mPitchShifters[0]->SetQuality(mHarmoQuality);

        // For Prusa
        for (int i = 0; i < 2; i++)
            mInputDelays[i]->Reset();
        // Latency may cna change with Prusa
        UpdateLatency();
            
        mQualityChanged = false;
    }

    //
    int numChannels = in.size();
    if (numChannels != mNumChannels)
    {
        mNumChannels = numChannels;
        for (int i = 0; i < 2; i++)
        {
            if (mPitchShifters[i] != NULL)
                mPitchShifters[i]->SetNumChannels(numChannels);
        }
    }
    
#if !DEBUG_PITCH_OBJ
    for (int i = 0; i < 2; i++)
    {
        if (mPitchShifters[i] != NULL)
            mPitchShifters[i]->SetTransBoost(mTransBoost);
    }
#endif
    
    // Process
    int shifterNum = (mMethod == 0) ? 0 : 1;
    if (mPitchShifters[shifterNum] != NULL)
        mPitchShifters[shifterNum]->Process(in, &out);

    // Process input delay fir dry wet
    for (int i = 0; i < 2; i++)
    {
        if (i >= in.size())
            break;
        
        mInputDelays[i]->ProcessSamples(&in[i]);
    }

    //for (int i = 0; i < out.size(); i++)
    //    BLUtils::MultValues(&out[i], -1.0);
                        
    // Dry/Wet
    BLUtilsPlug::ApplyDryWet(in, &out, mDryWetSmoother);

    // Gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
    //
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
PitchShift::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kFactorX, kFactorY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,                             
                              kFactor,
                              TEXTFIELD_FN,
                              "FACTOR",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipFactor);

    mTransBoostControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                                   kTransBoostX, kTransBoostY,
                                                   kKnobSmallWidth, kKnobSmallHeight,
                                                   KNOB_SMALL_FN,
                                                   kTransBoost,
                                                   TEXTFIELD_FN,
                                                   "TRANS BOOST",
                                                   GUIHelper12::SIZE_DEFAULT,
                                                   NULL, true,
                                                   tooltipTransBoost);

#if !USE_DROP_DOWN_MENU// Radio buttons
    const char *radioLabelsQuality[] = { "FAST 1", "2", "3", "BEST 4" };
    mQualityControl = mGUIHelper->CreateRadioButtons(pGraphics,
                                                     kRadioButtonsQualityX,
                                                     kRadioButtonsQualityY,
                                                     RADIOBUTTON_FN,
                                                     kRadioButtonQualityNumButtons,
                                                     kRadioButtonQualityVSize,
                                                     kQuality,
                                                     false,
                                                     "HARMONIC QUALITY",
                                                     EAlign::Far,
                                                     EAlign::Far,
                                                     radioLabelsQuality);
#else
    mQualityControl = mGUIHelper->CreateDropDownMenu(pGraphics,
                                                     kQualityX, kQualityY,
                                                     kQualityWidth,
                                                     kQuality,
                                                     "HARMO QUALITY",
                                                     GUIHelper12::SIZE_DEFAULT,
                                                     tooltipQuality);
#endif
    
    // Method
    const char *radioLabelsMethod[] = { "HARMONIC", "MIX", "TRANSIENT" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsMethodX,
                                   kRadioButtonsMethodY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonMethodNumButtons,
                                   kRadioButtonMethodVSize,
                                   kMethod,
                                   false,
                                   "METHOD",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsMethod,
                                   tooltipMethod);

    // Out gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX, kOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipOutGain);

    // Dry wet
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kDryWetX, kDryWetY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kDryWet,
                              TEXTFIELD_FN,
                              "DRY/WET",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipDryWet);
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    //, GUIHelper12::BOTTOM);
    
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
PitchShift::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
PitchShift::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();

    for (int i = 0; i < 2; i++)
    {
        if (mPitchShifters[i] != NULL)
            mPitchShifters[i]->Reset(sampleRate, blockSize);
    }
    
    //
    UpdateLatency();
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);

    if (mDryWetSmoother != NULL)
        mDryWetSmoother->Reset(sampleRate);

    for (int i = 0; i < 2; i++)
        mInputDelays[i]->Reset();
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
PitchShift::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kFactor:
        {
            BL_FLOAT factor = GetParam(kFactor)->Value();
            BL_FLOAT newFactor = ComputeFactor(factor);

            if (std::fabs(newFactor - mFactor) > BL_EPS)
            {
                mFactor = newFactor;
                
                mFactorChanged = true;
            }
        }
        break;
    
        case kTransBoost:
        {
            BL_FLOAT transBoost = GetParam(kTransBoost)->Value();
            mTransBoost = transBoost/100.0;
        }
        
        case kQuality:
        {
            Quality qual = (Quality)GetParam(kQuality)->Int();
            
            if (qual != mHarmoQuality)
            {
                mHarmoQuality = qual;   
                mQualityChanged = true;
            }
        }
        break;

        case kMethod:
        {
            Method method = (Method)GetParam(kMethod)->Int();

            if (method != mMethod)
            {
                mMethod = method;

                SetMethod(mMethod);

                mMethodChanged = true;
            }
        }
        break;
        
        case kOutGain:
        {
            BL_FLOAT outGain = GetParam(kOutGain)->DBToAmp();
            mOutGain = outGain;
            
            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(outGain);
        }
        break;

        case kDryWet:
        {
            BL_FLOAT value = GetParam(kDryWet)->Value();
            
            mDryWet = value*0.01;
            
            if (mDryWetSmoother != NULL)
                mDryWetSmoother->SetTargetValue(mDryWet);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
PitchShift::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;   
    LEAVE_PARAMS_MUTEX;
}

void
PitchShift::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    mTransBoostControl = NULL;
    mQualityControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
PitchShift::ApplyParams()
{

    for (int i = 0; i < 2; i++)
        mPitchShifters[i]->SetFactor(mFactor);
    
#if !DEBUG_PITCH_OBJ
    for (int i = 0; i < 2; i++)
        mPitchShifters[i]->SetTransBoost(mTransBoost);
#endif

    mQualityChanged = true;

    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();
            
    for (int i = 0; i < 2; i++)
    {
        if (mPitchShifters[i] != NULL)
        {
            
            mPitchShifters[i]->SetNumChannels(mNumChannels);
        
            mPitchShifters[i]->SetFactor(mFactor);
            
            mPitchShifters[i]->Reset(sampleRate, blockSize);
        }
    }

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    if (mDryWetSmoother != NULL)
        mDryWetSmoother->ResetToTargetValue(mDryWet);

    SetMethod(mMethod);

    mMethodChanged = true;
}

#if 0 // % of octave
BL_FLOAT
PitchShift::ComputeFactor(BL_FLOAT val)
{
    if (val >= 0.0)
    {
        val = (1.0 + val/2.0);
    }
    else
    {
        val = -val;
        val = 1.0/(1.0 + val/2.0);
    }
  
    val = BLUtilsMath::Round(val, 2);
  
    return val;
}
#endif

// Semi-tones and "cents"
// See: http://www.sengpielaudio.com/calculator-centsratio.htm
BL_FLOAT
PitchShift::ComputeFactor(BL_FLOAT val)
{
    // 1200 cents for 1 octave
    BL_FLOAT cents = val*100.0;
  
    BL_FLOAT ratio = pow(2.0, cents/1200);
  
    return ratio;
}

// Pitch shift method
void
PitchShift::SetMethod(Method method)
{
    mMethod = method;

    if (mTransBoostControl != NULL)
    {
        if (mMethod == PITCH_SHIFT_SMB)
        {
            //mTransBoostControl->SetDisabled(false);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kTransBoost, false);
        }
        else
        {
            //mTransBoostControl->SetDisabled(true);
            
            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kTransBoost, true);
        }
    }

    if (mQualityControl != NULL)
    {
        if (mMethod == PITCH_SHIFT_SMB)
            mQualityControl->SetDisabled(false);
        else
            mQualityControl->SetDisabled(true);
    }
}

void
PitchShift::MethodChanged()
{
    // Setup only PitchShifterPrusa
    
    // Mix: harmonic + transient
    // e.g Thumb Piano example
    // Keep get good transients while keeping good frequencies
    if (mMethod == PITCH_SHIFT_PRUSA_MIX)
    {
        if (mPitchShifters[1] != NULL)
            mPitchShifters[1]->SetQuality(1);
    }
    // Transient: priority to good transients
    // e.g: Drums
    // Gives better result on drums
    else if (mMethod == PITCH_SHIFT_PRUSA_TRANSIENTS)
    {
        if (mPitchShifters[1] != NULL)
            mPitchShifters[1]->SetQuality(0);
    }
    
    // For Prusa
    for (int i = 0; i < 2; i++)
        mInputDelays[i]->Reset();
    
    // Latency may change with Prusa
    UpdateLatency();
}

void
PitchShift::UpdateLatency()
{
    int blockSize = GetBlockSize();

    int shifterNum = (mMethod == 0) ? 0 : 1;
    
    int latency = mPitchShifters[shifterNum]->ComputeLatency(blockSize);
    SetLatency(latency);
    
    for (int i = 0; i < 2; i++)
    {
        if (mInputDelays[i] != NULL)
            mInputDelays[i]->SetDelay(latency);
    }
}
