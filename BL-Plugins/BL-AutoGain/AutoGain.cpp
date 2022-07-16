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

#include <FftProcessObj16.h>

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphControl12.h>
#include <GraphFreqAxis2.h>
#include <GraphAmpAxis.h>
#include <GraphAxis2.h>

#include <GraphCurve5.h>
#include <SmoothCurveDB.h>

#include <Scale.h>

#include <BLVumeter2SidesControl.h>

#include <ParamSmoother2.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "AutoGain.h"

#include "IPlug_include_in_plug_src.h"


#define USE_DOT_PRECISION 0

#define MIN_GAIN -12.0
#define MAX_GAIN 12.0

// Fft
#define BUFFER_SIZE 2048
#define OVERSAMPLING 4
#define FREQ_RES 1
#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

//
#define HIGH_RES_LOG_CURVES 1

#define GRAPH_MIN_DB -119.0 // Take care of the noise/harmo bottom dB
#define GRAPH_MAX_DB 10.0
#define GRAPH_CURVE_NUM_VALUES 512 //256

// See Air for details
//#define CURVE_SMOOTH_COEFF 0.95
#define CURVE_SMOOTH_COEFF_MS 1.4

// Origin: 1 (but it doesn't pass thread sanitizer)
// With 0, there is no thread sanitizer anymore.
#define DISABLE_ENABLE_VUMETER 0 //1

// Origin: 1 (vumeter disabled in write mode)
// Otherwise if we have automations already recorded,
// the vumeter would jitter between automation values and coputed values
#define DISABLE_VUMETER_WRITE_MODE 1 //0

// Origin: 0 Try to fix vumeter in write mode, when automation is "write"
// This fix doesn't work, the vumeter jitters a lots when not playing.
//
// NOTE: this seems not possible to fix, because we don't know if the host
// is in automation "read" or "write", then we don't know if we must read the automation
// to update the vumeter, or if we must use the computed gain to update the vumeter.
#define FIX_VUMETER_WRITE_MODE 0 //1

// Waveform11/linux: insert plugin => everything freezed
#define FIX_WAVEFORM11_FREEZE 1

// During OnReset(), Ableton 11 (Win) thought that user modified automation
// curve, then disabled the automation
#define FIX_ABLETON11_DISABLE_PARAM_AUTOMATION 1

// When starting in "read" mode, vumeter didn't move because control was disabled
#define FIX_ABLETON11_VUMETER_READ_MODE 1


#if 0
TODO: test side chain workaround on Logic
TODO: test well side chain on Protools (2in, 2out, 1sidechain)

TODO: diminish the plugin height a bit (by reducing the vumeter height)
TODO: fill some curves (to have better gfx)

PROBLEM:
For example Studio One 4, Windows 7
- side chain works in mono and stereo with VST3
- sidechain works for VST2 in stereo
- sidechain doesn't work in VST2 format in mono
=> it is because VST only supports static sidechain definition
(so in mono, with config 2-2-2, 1 input + 1 side chain are considered as 2 stereo channels
 
 NOTE: StudioOne, Sierra, VST: side chain does not enter in the plugins
 => must use AU or VST3 for sidechain
 NOTE(2): Sutdio One 4 Windows 7: side chain does not inter in the plugin with VST2 (must use VST3)
 
 NOTE: Cubase Pro 10, Sierra: Cubase supports side chain for third party plugins only in VST3 format
 
 CRASH: Mixcraft, VST3, Windows 7
 Open the autogain project (one AutoGain is already loaded), create a Denoiser,
 remove the AutoGain, create a ne AutoGain (all while playing): it crashes
 (But not in Reaper, Mixcraft stability ?)
 
 WARNING: hack for sidechain VST
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipDryWet = "Dry/Wet - Adjust between original signal and processed signal";
static char *tooltipScGain = "Side-Chain Gain";
static char *tooltipSpeed = "Speed - Gain auto-adjust speed";
static char *tooltipMode = "Automation Mode - Bypass, write or read";
static char *tooltipGain = "Gain - Current gain applied to signal";
 
enum EParams
{
    kMode = 0,
    
    kDryWet,
    kScGain,
    kGainSmooth,
    
    kGain,

#if USE_LEGACY_SILENCE_THRESHOLD
    kThreshold,
#endif
    
#if USE_DOT_PRECISION
    kPrecision,
#endif

#if 0
    kGraph,
#endif
    
    kNumParams
};
 
const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobWidth = 72,
    kKnobHeight = 72,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,
    
    kDryWetX = 90,
    kDryWetY = 338,

#if USE_LEGACY_SILENCE_THRESHOLD
    kThresholdX = 240,
    kThresholdY = 498,
#endif
    
#if USE_DOT_PRECISION
    kPrecisionX = 240,
    kPrecisionY = 496,
#endif
    
    kGainSmoothX = 244,
    kGainSmoothY = 374,
    
    kScGainX = 244,
    kScGainY = 240,
    
    kRadioButtonModeX = 62,
    kRadioButtonModeY = 258,
    kRadioButtonModeVSize = 46,
    kRadioButtonModeNumButtons = 2,

    // Vumeter
    kGainX = 374,
    kGainY = 232,
    
    kGraphX = 0,
    kGraphY = 0
};


//
AutoGain::AutoGain(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    TRACE;
    
    InitNull();
    InitParams();
    
    Init(OVERSAMPLING, FREQ_RES);

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

AutoGain::~AutoGain()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mAutoGainObj != NULL)
        delete mAutoGainObj;
    
    //
    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mAmpAxis != NULL)
        delete mAmpAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mVAxis != NULL)
        delete mVAxis;
    
    // Curves
    if (mSignal0Curve != NULL)
        delete mSignal0Curve;
    if (mSignal0CurveSmooth != NULL)
        delete mSignal0CurveSmooth;
    
    if (mSignal1Curve != NULL)
        delete mSignal1Curve;
    if (mSignal1CurveSmooth != NULL)
        delete mSignal1CurveSmooth;
    
    if (mResultCurve != NULL)
        delete mResultCurve;
    if (mResultCurveSmooth != NULL)
        delete mResultCurveSmooth;
    
}

IGraphics *
AutoGain::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
AutoGain::MyMakeLayout(IGraphics *pGraphics)
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
AutoGain::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    mCreatingControls = false;
    
    mGUIHelper = NULL;
    
    mFftObj = NULL;
    mAutoGainObj = NULL;
    
    mGainVumeter = NULL;
    
    mScGainControl = NULL;
    mSpeedControl = NULL;
 
    mGraph = NULL;
    
    mAmpAxis = NULL;
    mFreqAxis = NULL;
    mHAxis = NULL;
    mVAxis = NULL;
    
    // Curves
    mSignal0Curve = NULL;
    mSignal0CurveSmooth = NULL;
    
    mSignal1Curve = NULL;
    mSignal1CurveSmooth = NULL;
    
    mResultCurve = NULL;
    mResultCurveSmooth = NULL;
    
    mNumSamplesSinceLastDot = 0;
}

void
AutoGain::InitParams()
{
    BL_FLOAT defaultGain = 0.0;
    mGain = defaultGain;
    GetParam(kGain)->InitDouble("Gain", defaultGain, MIN_GAIN, MAX_GAIN, 0.1, "dB");

#if USE_LEGACY_SILENCE_THRESHOLD
    BL_FLOAT defaultThreshold = -80.0; //-120.0;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, -120.0,
                                     0.0, 0.1, "dB");
#endif
    
#if USE_DOT_PRECISION
    BL_FLOAT defaultPrecision = 50.0;
    mPrecision = defaultPrecision;
    GetParam(kPrecision)->InitDouble("Precision", defaultPrecision, 1.0, 50.0, 1.0, "d/s");
#endif
    
    BL_FLOAT defaultGainSmooth = 50.0;
    mGainSmooth = defaultGainSmooth;
    GetParam(kGainSmooth)->InitDouble("Speed"/*"Smooth"*/,
                                      defaultGainSmooth, 0.0, 100.0, 0.1, "%");
    
    BL_FLOAT defaultScGain = 0.0;
    mScGain = defaultScGain;
    GetParam(kScGain)->InitDouble("ScGain", defaultScGain, -12.0, 12.0, 0.1, "dB");
    
    BL_FLOAT defaultDryWet = 100.0;
    mDryWet = defaultDryWet;
    GetParam(kDryWet)->InitDouble("DryWet", defaultDryWet, 0.0, 100.0, 0.1, "%");
    
    AutoGainObj2::Mode defaultMode = AutoGainObj2::BYPASS_WRITE;
    mMode = defaultMode;
    //GetParam(kMode)->InitInt("Mode", (int)defaultMode, 0, 1);
    GetParam(kMode)->InitEnum("Mode", (int)defaultMode, 2,
                              "", IParam::kFlagsNone, "",
                              "BypassWrite", "Read");
}

void
AutoGain::ApplyParams()
{
    if (mAutoGainObj != NULL)
    {
        mAutoGainObj->SetGainSmooth(mGainSmooth);

#if USE_LEGACY_SILENCE_THRESHOLD
        mAutoGainObj->SetThreshold(mThreshold);
#endif
        
        mAutoGainObj->SetPrecision(mPrecision);
        mAutoGainObj->SetDryWet(mDryWet);
        mAutoGainObj->SetScGain(mScGain);
        mAutoGainObj->SetMode(mMode);
        mAutoGainObj->SetGainSmooth(mGainSmooth);
        
        if (mMode == AutoGainObj2::READ)
            mAutoGainObj->SetGain(mGain);
    }
    
    SetMode(mMode);

    // Update the vumeter
    if (mGainVumeter != NULL)
    {
        BL_FLOAT gain = (mGain - MIN_GAIN)/(MAX_GAIN - MIN_GAIN);
        gain = 1.0 - gain;
        mGainVumeter->SetValueForce(gain);
        mGainVumeter->SetDirty(false);
    }
}

void
AutoGain::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    int bufferSize = BUFFER_SIZE;
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        int numChannels = 2;
        int numScInputs = 2;
        
        vector<ProcessObj *> processObjs;
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      bufferSize, oversampling, freqRes,
                                      sampleRate);
#if !VARIABLE_HANNING
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowHanning);
        
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowHanning);
#else
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowVariableHanning);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowVariableHanning);
#endif
        
        mFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                        KEEP_SYNTHESIS_ENERGY);
        
        mAutoGainObj = new AutoGainObj2(bufferSize,
                                        oversampling, freqRes,
                                        sampleRate,
                                        MIN_GAIN, MAX_GAIN);
            
        mFftObj->AddMultichannelProcess(mAutoGainObj);
    }
    else
    {
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }
    
    ApplyParams();
    
    mIsInitialized = true;
}
 
void
AutoGain::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    //if (BLDebug::ExitAfter(this, 10))
    //    return;
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;
    
    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT()
        
    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > &scIn = mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf2;
    BLUtilsPlug::GetPlugIOBuffers(this, inputs, outputs, nFrames,
                              &in, &scIn, &out);
  
    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
    {
        if (mGraph != NULL)
            mGraph->PushAllData();
        
        return;
    }
  
#if 0 //1 // Debug
    if (BLUtilsPlug::PlugIOAllZero(in, out))
    {
        if (mGraph != NULL)
            mGraph->PushAllData();
      
        return;
    }
#endif

    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows all without stop
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
    
    //
    mFftObj->Process(in, scIn, &out);

#if !FIX_WAVEFORM11_FREEZE
    RefreshGainForAutomation();
#endif
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
 
    UpdateCurves();

    if (mGraph != NULL)
        mGraph->PushAllData();

    BL_PROFILE_END;
}

void
AutoGain::CreateControls(IGraphics *pGraphics)
{
    mCreatingControls = true;
    
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);
    
    // Separator
    IColor sepIColor;
    mGUIHelper->GetGraphSeparatorColor(&sepIColor);
    int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurves();

    mGainVumeter = mGUIHelper->CreateVumeter2SidesV(pGraphics,
                                                    kGainX, kGainY,
                                                    VUMETER_FN,
                                                    kGain, "GAIN",
                                                    5.0, 4.0,
                                                    4.0, 5.0,
                                                    tooltipGain);
    
#if USE_LEGACY_SILENCE_THRESHOLD
    // Threshold
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kThresholdX, kThresholdY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kThreshold,
                              TEXTFIELD_FN,
                              "SILENCE THRS",
                              GUIHelper12::SIZE_SMALL);
#endif
    
#if USE_DOT_PRECISION
    // Precision
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kPrecisionX, kPrecisionY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kPrecisionFrames,
                              kPrecision,
                              TEXTFIELD_FN,
                              "PRECISION",
                              GUIHelper12::SIZE_SMALL);
#endif
    
    // Smooth
    mSpeedControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                              kGainSmoothX, kGainSmoothY,
                                              kKnobSmallWidth, kKnobSmallHeight,
                                              KNOB_SMALL_FN,
                                              kGainSmooth,
                                              TEXTFIELD_FN,
                                              "SPEED",
                                              GUIHelper12::SIZE_SMALL,
                                              NULL, true,
                                              tooltipSpeed);

    if (mMode == AutoGainObj2::READ)
    {
        //mSpeedControl->SetDisabled(true);

        // Also disable knob value automatically
        pGraphics->DisableControl(kGainSmooth, true);
    }
    
    // Side chain gain
    mScGainControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                               kScGainX, kScGainY,
                                               kKnobSmallWidth, kKnobSmallHeight,
                                               KNOB_SMALL_FN,
                                               kScGain,
                                               TEXTFIELD_FN,
                                               "SC GAIN",
                                               GUIHelper12::SIZE_SMALL,
                                               NULL, true,
                                               tooltipScGain);
    if (mMode == AutoGainObj2::READ)
    {
        //mScGainControl->SetDisabled(true);

        // Also disable knob value automatically
        pGraphics->DisableControl(kScGain, true);
    }
    
    // Dry/wet
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kDryWetX, kDryWetY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kDryWet,
                              TEXTFIELD_FN,
                              "DRY/WET",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipDryWet);
    
    // Mode
    const char *radioLabelsMode[kRadioButtonModeNumButtons] =
        { "Bypass/Write", "Read" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonModeX,
                                   kRadioButtonModeY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeNumButtons,
                                   kRadioButtonModeVSize,
                                   kMode,
                                   false,
                                   "AUTOMATION",
                                   EAlign::Far, EAlign::Center,
                                   radioLabelsMode,
                                   tooltipMode);
    
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
    
    //
    mControlsCreated = true;
    
    mCreatingControls = false;
}

void
AutoGain::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
AutoGain::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    int bufferSize = BUFFER_SIZE;
    BL_FLOAT sampleRate = GetSampleRate();
    
#if TEST_BUFFERS_SIZE
    bufferSize = Utils::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mAutoGainObj != NULL)
        mAutoGainObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
    if (mFftObj != NULL)
        mFftObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);

    // If we set kGain parameter, Ableton 11 thinks the user modified the automation
    // curve manually, and disables the automation
    // (automation curve goes from orange to white, and to reenable it, need
    // to right click on the parameter and click "re-enable automation"
    // 
#if !FIX_ABLETON11_DISABLE_PARAM_AUTOMATION
    SetParameterValue(kGain, GetParam(kGain)->GetNormalized());
#endif
    
    mNumSamplesSinceLastDot = 0;

    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    // Curves
    if (mSignal0Curve != NULL)
        mSignal0Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mSignal1Curve != NULL)
        mSignal1Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mResultCurve != NULL)
        mResultCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);

    if (mSignal0CurveSmooth != NULL)
        mSignal0CurveSmooth->Reset(sampleRate);
    if (mSignal1CurveSmooth != NULL)
        mSignal1CurveSmooth->Reset(sampleRate);
    if (mResultCurveSmooth != NULL)
        mResultCurveSmooth->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

// NOTE: OnIdle() is called from the GUI thread
void
AutoGain::OnIdle()
{
    if (!mUIOpened)
        return;
    
    ENTER_PARAMS_MUTEX;

    if (mSignal0CurveSmooth != NULL)
        mSignal0CurveSmooth->PullData();
    if (mSignal1CurveSmooth != NULL)
        mSignal1CurveSmooth->PullData();
    if (mResultCurveSmooth != NULL)
        mResultCurveSmooth->PullData();
        
    LEAVE_PARAMS_MUTEX;

    // We don't need mutex anymore now
    if (mSignal0CurveSmooth != NULL)
        mSignal0CurveSmooth->ApplyData();
    if (mSignal1CurveSmooth != NULL)
        mSignal1CurveSmooth->ApplyData();
    if (mResultCurveSmooth != NULL)
        mResultCurveSmooth->ApplyData();

#if FIX_WAVEFORM11_FREEZE
    RefreshGainForAutomation();
#endif
}

void
AutoGain::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;

    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
#if USE_LEGACY_SILENCE_THRESHOLD
        case kThreshold:
        {
            BL_FLOAT threshold = GetParam(kThreshold)->Value();
            mThreshold = threshold;
            
            if (mAutoGainObj != NULL)
                mAutoGainObj->SetThreshold(mThreshold);
        }
        break;
#endif
        
#if USE_DOT_PRECISION
        case kPrecision:
        {
            BL_FLOAT precision = GetParam(kPrecision)->Value();
            mPrecision = precision;
            
            if (mAutoGainObj != NULL)
                mAutoGainObj->SetPrecision(mPrecision);
        }
        break;
#endif
            
        case kDryWet:
        {
            BL_FLOAT dryWet = GetParam(kDryWet)->Value();
            mDryWet = dryWet/100.0;
            
            if (mAutoGainObj != NULL)
                mAutoGainObj->SetDryWet(mDryWet);
        }
        break;
            
        case kMode:
        {
            AutoGainObj2::Mode mode = (AutoGainObj2::Mode)GetParam(paramIdx)->Int();

            if (mAutoGainObj != NULL)
                mAutoGainObj->SetMode(mode);

            SetMode(mode);
        }
        break;

#if 0 // ORIGIN
        // We must have this case, when automation sets the gain parameter,
        // we want to retrieve mGain !
        case kGain:
        {
            if (mMode == AutoGainObj2::READ)
            {
                BL_FLOAT gain = GetParam(kGain)->Value();
                mGain = gain;
                
                if (mAutoGainObj != NULL)
                    mAutoGainObj->SetGain(mGain);

                // Update the vumeter from automation
                if (mGainVumeter != NULL)
                {
                    BL_FLOAT gain0 = (mGain - MIN_GAIN)/(MAX_GAIN - MIN_GAIN);
                    gain0 = 1.0 - gain0;
                    mGainVumeter->SetValueForce(gain0);
                    mGainVumeter->SetDirty(false);
                }
            }
        }
        break;
#endif
        
        case kScGain:
        {
            BL_FLOAT scGain = GetParam(kScGain)->Value();
            mScGain = scGain;
            
            if (mAutoGainObj != NULL)
                mAutoGainObj->SetScGain(mScGain);
        }
        break;
            
        case kGainSmooth:
        {
            BL_FLOAT value = GetParam(kGainSmooth)->Value();
            BL_FLOAT gainSmooth = value*0.01;

            // The knob is named "gain speed", reverse it
            gainSmooth = 1.0 - gainSmooth;
            
            mGainSmooth = gainSmooth;
            
            if (mAutoGainObj != NULL)
                mAutoGainObj->SetGainSmooth(mGainSmooth);
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
AutoGain::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
AutoGain::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;

    mGraph = NULL;
    mGainVumeter = NULL;
    
    mScGainControl = NULL;
    mSpeedControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
}

void
AutoGain::CreateGraphAxes()
{
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2(true, Scale::LOG);
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
        mAmpAxis = new GraphAmpAxis();
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);
    
    BL_FLOAT sampleRate = GetSampleRate();
    int graphWidth = mGraph->GetRECT().W();
    
    bool horizontal = true;
    mFreqAxis->Init(mHAxis, mGUIHelper, horizontal, BUFFER_SIZE,
                    sampleRate, graphWidth);
    mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    mAmpAxis->Init(mVAxis, mGUIHelper, GRAPH_MIN_DB, GRAPH_MAX_DB, graphWidth);
}

void
AutoGain::CreateGraphCurves()
{
#define REF_SAMPLERATE 44100.0
    BL_FLOAT curveSmoothCoeff =
        ParamSmoother2::ComputeSmoothFactor(CURVE_SMOOTH_COEFF_MS, REF_SAMPLERATE);
    
    if (mSignal0Curve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
    
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        // Signal0 curve
        int signal0Color[4];
        mGUIHelper->GetGraphCurveColorBlue(signal0Color);
        
        mSignal0Curve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mSignal0Curve->SetDescription("signal", descrColor);
        mSignal0Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5); //
        mSignal0Curve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mSignal0Curve->SetFill(true); // NEW
        mSignal0Curve->SetFillAlpha(fillAlpha);
        mSignal0Curve->SetColor(signal0Color[0], signal0Color[1], signal0Color[2]);
        mSignal0Curve->SetLineWidth(2.0);
        
        mSignal0CurveSmooth = new SmoothCurveDB(mSignal0Curve,
                                                //CURVE_SMOOTH_COEFF,
                                                curveSmoothCoeff,
                                                GRAPH_CURVE_NUM_VALUES,
                                                GRAPH_MIN_DB,
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
    
        // Signal1 curve
        int signal1Color[4];
        //mGUIHelper->GetGraphCurveColorGreen(signal1Color);
        mGUIHelper->GetGraphCurveColorFakeCyan(signal1Color);
        
        mSignal1Curve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mSignal1Curve->SetDescription("sc signal", descrColor);
        mSignal1Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mSignal1Curve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mSignal1Curve->SetFillAlpha(fillAlpha);
        mSignal1Curve->SetColor(signal1Color[0], signal1Color[1], signal1Color[2]);
        
        mSignal1CurveSmooth = new SmoothCurveDB(mSignal1Curve,
                                                //CURVE_SMOOTH_COEFF,
                                                curveSmoothCoeff,
                                                GRAPH_CURVE_NUM_VALUES,
                                                GRAPH_MIN_DB,
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
    
        
        // Result curve
        int resultColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(resultColor);
        
        mResultCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mResultCurve->SetDescription("out", descrColor);
        mResultCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mResultCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mResultCurve->SetFillAlpha(fillAlpha);
        mResultCurve->SetColor(resultColor[0], resultColor[1], resultColor[2]);
        
        mResultCurveSmooth = new SmoothCurveDB(mResultCurve,
                                               //CURVE_SMOOTH_COEFF,
                                               curveSmoothCoeff,
                                               GRAPH_CURVE_NUM_VALUES,
                                               GRAPH_MIN_DB,
                                               GRAPH_MIN_DB, GRAPH_MAX_DB,
                                               sampleRate);
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mSignal0Curve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSignal0Curve);
    
    mSignal1Curve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSignal1Curve);
    
    mResultCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mResultCurve);
}

void
AutoGain::UpdateCurves()
{
    if (!mUIOpened)
        return;
  
    if (mSignal0CurveSmooth != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &signal0 = mTmpBuf3;
        mAutoGainObj->GetCurveSignal0(&signal0);
        
        BLUtils::DBToAmp(&signal0);
        
        mSignal0CurveSmooth->SetValues(signal0);
    }
    
    if (mSignal1CurveSmooth != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &signal1 = mTmpBuf4;
        mAutoGainObj->GetCurveSignal1(&signal1);
        
        BLUtils::DBToAmp(&signal1);
        
        mSignal1CurveSmooth->SetValues(signal1);
    }
    
    if (mResultCurveSmooth != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &result = mTmpBuf5;
        mAutoGainObj->GetCurveResult(&result);
        
        BLUtils::DBToAmp(&result);
        
        mResultCurveSmooth->SetValues(result);
    }

    // Lock free
    if (mSignal0CurveSmooth != NULL)
        mSignal0CurveSmooth->PushData();
    if (mSignal1CurveSmooth != NULL)
        mSignal1CurveSmooth->PushData();
    if (mResultCurveSmooth != NULL)
        mResultCurveSmooth->PushData();
}
 
void
AutoGain::SetMode(AutoGainObj2::Mode mode)
{
    if (mode == mMode)
        // Mode didn't change
        return;

    mMode = mode;
    
    if (mMode == AutoGainObj2::BYPASS_WRITE)
    {
        if (mGainVumeter != NULL)
        {
#if DISABLE_VUMETER_WRITE_MODE
            //mGainVumeter->SetInformHostParamChange(true);
            //mGainVumeter->SetLocked(true);
            if (!mGainVumeter->IsDisabled())
                mGainVumeter->SetDisabled(true);
#endif
        }

        if (mScGainControl != NULL)
        {
            //mScGainControl->SetDisabled(false);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kScGain, false);
        }

        if (mSpeedControl != NULL)
        {
            //mSpeedControl->SetDisabled(false);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kGainSmooth, false);
        }
    }
    
    if (mMode == AutoGainObj2::READ)
    {
        // We don't need to display the previous curves anymore
        if (mSignal0Curve != NULL)
        {
            mSignal0Curve->Reset(CURVE_VALUE_UNDEFINED);
            mSignal1Curve->Reset(CURVE_VALUE_UNDEFINED);
            mResultCurve->Reset(CURVE_VALUE_UNDEFINED);
        }
        
        if (mGainVumeter != NULL)
        {
            // FIX: Reason not reading automation correctly
            //mGainVumeter->SetInformHostParamChange(false);
        
#if DISABLE_VUMETER_WRITE_MODE
            // Avoid the vumeter to be modified by both ProcessDoubleReplacing()
            // and a possible automation
            //mGainVumeter->SetLocked(false);
            if (mGainVumeter->IsDisabled())
                mGainVumeter->SetDisabled(false);
#endif
        }

        if (mScGainControl != NULL)
        {
            //mScGainControl->SetDisabled(true);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kScGain, true);
        }

        if (mSpeedControl != NULL)
        {
            //mSpeedControl->SetDisabled(true);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kGainSmooth, true);
        }

        // First of all, set default gain
        // (automations if any, will modify it later)
        // Otherwise when there is no automation and we switch to read mode,
        // the previous gain value stays forever
        double normDefaultGain = 0.5;
        SetParameterValue(kGain, normDefaultGain);

        mGain = 0.0;
        if (mGainVumeter != NULL)
            mGainVumeter->SetValue(normDefaultGain);
    }

    // Reset the smoothers
    if (mAutoGainObj != NULL)
        mAutoGainObj->Reset();
}

void
AutoGain::UpdateVumeterGain()
{
    if (mMode == AutoGainObj2::BYPASS_WRITE)
    {
        mGain = mAutoGainObj->GetGain();
        
        if (!mCreatingControls)
        {
            // Update plug param and vumeter
            GetParam(kGain)->Set(mGain);
            
#if FIX_VUMETER_WRITE_MODE 
            if (mGainVumeter != NULL)
                mGainVumeter->SetValueForce(mGain);
#endif
            
            // This fixes the vumeter that didn't update
            if (mGainVumeter != NULL)
            {
                BL_FLOAT gain = (mGain - MIN_GAIN)/(MAX_GAIN - MIN_GAIN);
                
#if 0 // 
                // Reverse, so when we increase the sc gain (with no sc),
                // the vumeter increases)
                gain = 1.0 - gain;
#endif
                
                mGainVumeter->SetValueForce(gain);
                mGainVumeter->SetDirty(false);
            }
        }
    }
}

void
AutoGain::RefreshGainForAutomation()
{
    if (mMode == AutoGainObj2::BYPASS_WRITE)
    {
        mGain = mAutoGainObj->GetGain();

#if DISABLE_ENABLE_VUMETER
        bool vumeterDisabled = false;
        if (mUIOpened)
        {
            vumeterDisabled = mGainVumeter->IsDisabled();

            if (mGainVumeter->IsDisabled())
                mGainVumeter->SetDisabled(false);
        }
#endif
        // Is it in the right thread ?
        UpdateVumeterGain();
                
        bool putDot = true;
#if USE_DOT_PRECISION
        putDot = false;
        
        // Test if we must put an automation dot
        mNumSamplesSinceLastDot += nFrames;
        
        BL_FLOAT sampleRate = GetSampleRate();
        long numSamplesPutDot = sampleRate/mPrecision;
        if (mNumSamplesSinceLastDot >= numSamplesPutDot)
        {
            mNumSamplesSinceLastDot -= numSamplesPutDot;
            
            putDot = true;
        }
#endif
        
        if (putDot)
        {
            // Avoid updating the parameter, with transfert to the vumeter,
            // while currently creating it.
            if (!mCreatingControls)
            {
                // Refresh (for automations, as if the user had clicked).

#if 0 // Origin
      // Works for vst2
      // vst3: do not write automation
                // NOTE: possible data race here
                SetParameterValue(kGain, GetParam(kGain)->GetNormalized());
#endif
#if 1 // Works for Reaper vst3/write automation
                SendParameterValueFromUI(kGain, GetParam(kGain)->GetNormalized());
#endif

                // For Bitwig linux (with this, vst2 does write automation)
                BLUtilsPlug::TouchPlugParam(this, kGain);
            }
            
#if DISABLE_ENABLE_VUMETER
            if (mUIOpened)
            {
                if (mGainVumeter->IsDisabled() != vumeterDisabled)
                    mGainVumeter->SetDisabled(vumeterDisabled);
            }
#endif
        }
    }

    // NEW (for VST3)
    if (mMode == AutoGainObj2::READ)
    {
        if (mGainVumeter != NULL)
        {
#if FIX_ABLETON11_VUMETER_READ_MODE
            // FIX: Ableton11, open project with automation curve,
            // and starting with "read" mode
            // => vumeter didn't move when playing
            // => so ensure it is not disable in read mode!
            if (mGainVumeter->IsDisabled())
                mGainVumeter->SetDisabled(false);
#endif
            
            BL_FLOAT normGain = mGainVumeter->GetValue();
            mGain = GetParam(kGain)->FromNormalized(normGain);
            
            mAutoGainObj->SetGain(mGain);
        }
    }
}
