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

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLUtilsEnvelope.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphControl12.h>
#include <GraphAxis2.h>
#include <GraphCurve5.h>

#include <BLScanDisplay.h>
#include <PlugBypassDetector.h>

#include <Window.h>
#include <FftProcessObj16.h>
#include <TransientShaperFftObj3.h>
#include <SamplesDelayer.h>
#include <FifoDecimator.h>

#include <ParamSmoother2.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "bl_config.h"

#include "Shaper.h"

#include "IPlug_include_in_plug_src.h"

// We don't need the quality parameter
// It has no effect on this plugin
// (because we process the samples and not the fft)
#define DISABLE_QUALITY_PARAM 1

#define BUFFER_SIZE 2048

#define OVERSAMPLING_0 4
#define OVERSAMPLING_1 8
#define OVERSAMPLING_2 16
#define OVERSAMPLING_3 32

#define FREQ_RES 1

#define VARIABLE_HANNING 1
#define USE_ANALYSIS_WINDOWING 0  // false !

// Graph
#define CURVE_FILL_ALPHA 0.2

#define GRAPH_MAX_TRANSIENT_Y 20.0

// GUI
#define SCROLL 1

#if !SCROLL
#define FIFO_DECIM_COEFF 1.0
#else
// 4 is very sexy
// 32 is very slow
// 8 is very cool: sexy, and we don't see
// too much the gaps between transient frames
//#define FIFO_DECIM_COEFF 4.0 //32.0
#define FIFO_DECIM_COEFF 8.0
#endif

// display envelopes or samples ?
#define DISPLAY_ENVELOPES 0

#define GRAPH_TRANSIENT_SCALE 2.0

#define MAX_PRECISION 0.99

// Not really useful,
// and not easy to set because we don't have
// the curve here to display its effect
#define HIDE_PRECISION_KNOB 1

// Not used anymore
//#define HIDE_TRANSIENTNESS_CURVE 1

#define USE_VARIABLE_BUFFER_SIZE 1

#define CURVE_NUM_POINTS BUFFER_SIZE/4

#define BEVEL_CURVES 1

#define CURVE_INPUT 0
#define CURVE_OUTPUT 2 //1
#define CURVE_TRANSIENTS 1 //2
#define NUM_CURVES 3

//#define MIN_ZOOM 0.5
//#define MAX_ZOOM 2.0

#define MIN_ZOOM 0.25
#define MAX_ZOOM 4.0

#if 0
This transient shaper has a good look:
https://www.youtube.com/watch?v=Le7fOEVMNV8
-> good idea to make waveform scroll (really scrolling, not "scan" mode
-> and make it multiband!!!

TODO: try to eliminate totally the attack, to have this kind of synth sound
https://www.youtube.com/watch?v=yoLbeqaojs0 (BlueMangoo AttackSoftener)
  
BUG(not a bug ?): the knob "s" and "p" looks reversed ! (check "Debloss trio")
  => in fact, it seems to depend on the sound that is processed
  
- sometimes, "rumble" sound when very short and repetive transients are detected
(problem still here ?)

- voix off Seb: "s" and "p" seem swapped

SMALL PROBLEMs:
- When extracting "s", there remains some parts of "p"

- MEME LEAK: maybe a very small memory leak (Reaper, 88200Hz)
=> after a long time, 3MB more)

- Tester avec une gros changement de frequence comme dans l article ! (et tester params)
- Mono detection pour eviter les effects de phasing ?

TEST: test again with ANALYSIS_WINDOWING=1 ?
TEST: test again with VARIABLE_HANNING=0 ??
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";
static char *tooltipSoftHard = "Transients - Increase or decrease transients";
static char *tooltipFreqAmpRatio =
    "Noise/Attack - Adjust for noisy transients/punchy transients";

enum EParams
{
    kSoftHard = 0,

#if !HIDE_PRECISION_KNOB
    kPrecision,
#endif
    
    //kMode = 3,
    kFreqAmpRatio,

    kZoom,
    
#if !DISABLE_QUALITY_PARAM
    kQuality,
#endif
    
#if USE_SCAN_DISPLAY
    kMonitor,
#endif

    kOutGain,

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

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,

    kKnobWidth = 72,
    kKnobHeight = 72,

    kGraphX = 0,
    kGraphY = 0,
    
    kSoftHardX = 196,
    kSoftHardY = 272,
  
    kRadioButtonsQualityX = 428,
    kRadioButtonsQualityY = 302,
    kRadioButtonQualityVSize = 83,
    kRadioButtonQualityNumButtons = 4,
    
    kFreqAmpRatioX = 77,
    kFreqAmpRatioY = 306,

#if !HIDE_PRECISION_KNOB
    kPrecisionX = 310,
    kPrecisionY = 320,
#endif
    
    kOutGainX = 350,
    kOutGainY = 310,

#if USE_SCAN_DISPLAY
    kCheckboxMonitorX = 339,
    kCheckboxMonitorY = 265
#endif
};


//
Shaper::Shaper(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    TRACE;
    
    InitNull();
    InitParams();

    Init(mOversampling, mFreqRes);
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

Shaper::~Shaper()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mFftObj != NULL)
        delete mFftObj;
    
    for (int i = 0; i < 2; i++)
    {
        if (mTransObjs[i] != NULL)
            delete mTransObjs[i];
    }

    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;

#if !USE_SCAN_DISPLAY
    if (mVAxis != NULL)
        delete mVAxis;

    if (mInputCurve != NULL)
        delete mInputCurve;
    if (mOutputCurve != NULL)
        delete mOutputCurve;
#else
    if (mScanDisplay != NULL)
        delete mScanDisplay;

    if (mBypassDetector != NULL)
        delete mBypassDetector;
#endif

    if (mZoomControl != NULL)
        delete mZoomControl;
}

IGraphics *
Shaper::MyMakeGraphics()
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
Shaper::MyMakeLayout(IGraphics *pGraphics)
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
Shaper::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    mGraph = NULL;
    mGUIHelper = NULL;

    mVAxis = NULL;
    
#if !USE_SCAN_DISPLAY
    mInputCurve = NULL;
    mOutputCurve = NULL;
#else
    mScanDisplay = NULL;
    mBypassDetector = NULL;

    mIsPlaying = false;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
#endif

    mZoomControl = NULL;
    
    mOutGainSmoother = NULL;

    mFftObj = NULL;

    mZoom = 1.0;

    // NOTE: mPrecision seems not initialized!
}

void
Shaper::InitParams()
{
    BL_FLOAT defaultSoftHard = 0.0;
    mSoftHard = 0.0;
    GetParam(kSoftHard)->InitDouble("Transients", defaultSoftHard,
                                    -100.0, 100.0, 0.1, "%");

#if !HIDE_PRECISION_KNOB
    BL_FLOAT defaultPrecision = 15.0;;
    mPrecision = defaultPrecision / 100.0;
    GetParam(kPrecision)->InitDouble("Precision",
                                     defaultPrecision, 0.0, 100.0, 0.01, "%");
#endif

    // Quality
    Quality defaultQuality = STANDARD;
    
    mQuality = defaultQuality;
    mOversampling = OVERSAMPLING_0;
    mFreqRes = FREQ_RES;
    
#if !DISABLE_QUALITY_PARAM
    GetParam(kQuality)->InitInt("Quality", (int)defaultQuality, 0, 3);
#endif
    
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");

    double defaultFreqAmpRatio = 50.0;
    mFreqAmpRatio = defaultFreqAmpRatio/100.0;
    GetParam(kFreqAmpRatio)->InitDouble("NoiseAttackRatio",
                                        defaultFreqAmpRatio, 0.0, 100.0, 0.01, "%");

#if USE_SCAN_DISPLAY
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
#endif

    BL_FLOAT defaultZoom = 1.0;
    mZoom = defaultZoom;
    GetParam(kZoom)->InitDouble("Zoom", defaultZoom, MIN_ZOOM, MAX_ZOOM, 0.001, "");
}

void
Shaper::ApplyParams()
{
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    if (mScanDisplay != NULL)
        mScanDisplay->SetEnabled(mIsPlaying || mMonitorEnabled);

    if (mScanDisplay != NULL)
        mScanDisplay->SetZoom(mZoom);
}

void
Shaper::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    BL_FLOAT defaultOutGain = 1.0; // 0dB
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
    
    int bufferSize = BUFFER_SIZE;
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        mTransObjs.resize(2);

        int maxNumPoints = GetBlockSize();
        if (maxNumPoints < CURVE_NUM_POINTS)
            maxNumPoints = CURVE_NUM_POINTS;
        
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < 2; i++)
        {
            TransientShaperFftObj3 *transObj =
                new TransientShaperFftObj3(bufferSize,
                                           mOversampling, mFreqRes,
                                           sampleRate,
                                           maxNumPoints,
                                           1.0,
                                           true);

            transObj->SetPrecision(mPrecision);
            transObj->SetSoftHard(mSoftHard);
            transObj->SetFreqAmpRatio(mFreqAmpRatio);
            
            mTransObjs[i] = transObj;
            processObjs.push_back(transObj);
        }
        
        int numChannels = 2;
        int numScInputs = 0;
        
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      bufferSize, oversampling, freqRes,
                                      sampleRate);
        
#if USE_ANALYSIS_WINDOWING
        
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
        
#else // !USE_ANALYSIS_WINDOWING
        
#if !VARIABLE_HANNING
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowRectangular);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowHanning);
#else
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowRectangular);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowVariableHanning);
#endif
        
#endif
        
        mFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS, false);
    }
    else
    {
        FftProcessObj16 *fftObj = mFftObj;
        //BL_FLOAT sampleRate = GetSampleRate();
        fftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }
    
    int blockSize = GetBlockSize();
    int latency = mFftObj->ComputeLatency(blockSize);
    SetLatency(latency);

#if USE_SCAN_DISPLAY
    mScanDisplay = new BLScanDisplay(NUM_CURVES, sampleRate);

    SetCurvesStyle();
    
    mScanDisplay->SetGraph(mGraph);

    mBypassDetector = new PlugBypassDetector();
#endif

    mZoom = 1.0;
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
Shaper::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    if (mGraph != NULL)
        mGraph->Lock();
  
    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();

#if USE_SCAN_DISPLAY
    bool isPlaying = IsTransportPlaying();

    if (mBypassDetector != NULL)
    {
        mBypassDetector->TouchFromAudioThread();
        mBypassDetector->SetTransportPlaying(isPlaying || mMonitorEnabled);
    }
#endif
    
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
            mGraph->Unlock();

        if (mGraph != NULL)
            mGraph->PushAllData();
        
        return;
    }

#if USE_SCAN_DISPLAY
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    mIsPlaying = isPlaying;

    if (mUIOpened)
        mScanDisplay->SetEnabled(mIsPlaying || mMonitorEnabled);
#endif
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes,
    // this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for
    // insufficient resources
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

    // Set the parameters
    for (int i = 0; i < 2; i++)
    {
        if (mTransObjs[i] != NULL)
        {
            mTransObjs[i]->SetSoftHard(mSoftHard);
            mTransObjs[i]->SetPrecision(mPrecision);
            
            mTransObjs[i]->SetFreqAmpRatio(mFreqAmpRatio);
        }
    }

    // Process
    WDL_TypedBuf<BL_FLOAT> *transientness = mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> *resInput = mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> *resOutput = mTmpBuf5;
      
    const WDL_TypedBuf<BL_FLOAT> &scInMono = mTmpBuf6;
#if USE_SIDE_CHAIN
    BLUtils::StereoToMono(&scInMono, scIn[0], scIn[1], nFrames);
#endif
      
    for (int i = 0; i < 2; i++)
    {
        transientness[i].Resize(CURVE_NUM_POINTS);
        BLUtils::FillAllZero(&transientness[i]);

        resInput[i].Resize(CURVE_NUM_POINTS);
        BLUtils::FillAllZero(&resInput[i]);

        resOutput[i].Resize(CURVE_NUM_POINTS);
        BLUtils::FillAllZero(&resOutput[i]);
    }
      
    /*bool processed =*/ mFftObj->Process(in, scIn, &out);

    // Really watch if the object just did a process step
    // So the BLScanDisplay won't depend on sample rate.
    // ('processed' is true al long as the object can provide new processed samples,
    // not just when the fft process objs Process() was called
    bool hasNewData = false;
    if ((mTransObjs.size() > 0) && mTransObjs[0]->HasNewData())
    {
        hasNewData = true;

        for (int i = 0; i < mTransObjs.size(); i++)
            mTransObjs[i]->TouchNewData();
    }
    
    if (hasNewData)
    {
        for (int i = 0; i < 2; i++)
        {
            mTransObjs[i]->GetTransientness(&transientness[i]);
            mTransObjs[i]->GetCurrentInput(&resInput[i]);
            mTransObjs[i]->GetCurrentOutput(&resOutput[i]);
        }
    }
  
    // Apply output gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
 
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

    // Update graph
    if (hasNewData)
    {
        WDL_TypedBuf<BL_FLOAT> &transientness0 = mTmpBuf7;
        BLUtils::StereoToMono(&transientness0, transientness[0], transientness[1]);
        
        WDL_TypedBuf<BL_FLOAT> &resInput0 = mTmpBuf8;
        BLUtils::StereoToMono(&resInput0, resInput[0], resInput[1]);
        
        WDL_TypedBuf<BL_FLOAT> &resOutput0 = mTmpBuf9;
        BLUtils::StereoToMono(&resOutput0, resOutput[0], resOutput[1]);
        
#if DISPLAY_ENVELOPES
        WDL_TypedBuf<BL_FLOAT> &envInput = mTmpBuf10;
        BLUtilsEnvelope::ComputeEnvelopeSmooth2(resInput0, &envInput, 0.01);
        resInput0 = envInput;
        
        WDL_TypedBuf<BL_FLOAT> &envOutput = mTmpBuf11;
        BLUtilsEnvelope::ComputeEnvelopeSmooth2(resOutput0, &envOutput, 0.01);
        resOutput0 = envOutput;
#endif
        
        // More beautiful
        BLUtils::MultValues(&transientness0, GRAPH_TRANSIENT_SCALE);
        
        UpdateCurves(resInput0, resOutput0, transientness0);
    }

    if (mScanDisplay != NULL)
        mScanDisplay->UpdateZoom();
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
    
    if (mGraph != NULL)
        mGraph->Unlock();

    if (mGraph != NULL)
        mGraph->PushAllData();
    
    BL_PROFILE_END;
}

void
Shaper::CreateControls(IGraphics *pGraphics)
{
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
    
    CreateGraphAxis();
        
#if !USE_SCAN_DISPLAY
    CreateGraphCurves();
#else
    SetCurvesStyle();
    
    if (mScanDisplay != NULL)
        mScanDisplay->SetGraph(mGraph);
#endif

    if (mZoomControl == NULL)
        mZoomControl = new ZoomCustomControl(this);
    mGraph->AddCustomControl(mZoomControl);
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);

    // Soft / Hard
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSoftHardX, kSoftHardY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kSoftHard,
                              TEXTFIELD_FN,
                              "TRANSIENTS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSoftHard);

    mGUIHelper->CreateKnobSVG(pGraphics,
                              kFreqAmpRatioX, kFreqAmpRatioY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kFreqAmpRatio,
                              TEXTFIELD_FN,
                              "NOISE / ATTACK",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipFreqAmpRatio);
    
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

#if !HIDE_PRECISION_KNOB
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kPrecisionX, kPrecisionY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kPrecision,
                              TEXTFIELD_FN,
                              "PRECISION",
                              GUIHelper12::SIZE_DEFAULT);
#endif

    // NOTE: not ported to iPlug2 yet (because unused)
#if !DISABLE_QUALITY_PARAM
    GetParam(kQuality)->InitInt("Quality", (int)defaultQuality, 0, 3);
  
  
#define NUM_RADIO_LABELS 4
    const char *radioLabels[NUM_RADIO_LABELS] = { "FAST 1", "2", "3", "BEST 4" };
    
    guiHelper.CreateRadioButtons(this, pGraphics,
                                 RADIOBUTTONS_QUALITY_ID,
                                 RADIOBUTTONS_QUALITY_FN,
                                 kRadioButtonsQualityFrames,
                                 kRadioButtonsQualityX, kRadioButtonsQualityY,
                                 kRadioButtonQualityNumButtons,
                                 kRadioButtonQualityVSize, kQuality, false, "QUALITY",
                                 RADIOBUTTON_DIFFUSE_ID, RADIOBUTTON_DIFFUSE_FN,
                                 IText::kAlignFar, IText::kAlignFar, radioLabels,
                                 NUM_RADIO_LABELS);
#endif

#if USE_SCAN_DISPLAY
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX,
                                                     kCheckboxMonitorY,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT, true,
                                                     tooltipMonitor);
#endif
    
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
Shaper::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Shaper::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    
    int bufferSize = BUFFER_SIZE;
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif

    if (mFftObj != NULL)
        mFftObj->Reset(bufferSize, mOversampling, mFreqRes, sampleRate);

    int maxNumPoints = GetBlockSize();
    if (maxNumPoints < CURVE_NUM_POINTS)
        maxNumPoints = CURVE_NUM_POINTS;
    
    for (int i = 0; i < 2; i++)
    {
        if (mTransObjs[i] != NULL)
        {
            // Necessary in order to have constant scan scroll speed 
            mTransObjs[i]->SetTrackIO(maxNumPoints, 1.0,
                                      true, true, true);
        }
    }
    
    if (mFftObj != NULL)
    {
        int blockSize = GetBlockSize();
        int latency = mFftObj->ComputeLatency(blockSize);
        SetLatency(latency);
    }

    if (mScanDisplay != NULL)
        mScanDisplay->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Shaper::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kSoftHard:
        {
            BL_FLOAT softHard = GetParam(paramIdx)->Value();
            mSoftHard = softHard/100.0;
        }
        break;

#if !HIDE_PRECISION_KNOB
        case kPrecision:
        {
            BL_FLOAT precision = GetParam(kPrecision)->Value();
            precision = precision / 100.0;
            precision = BLUtils::ApplyParamShape(precision, 4.0);
            mPrecision = MAX_PRECISION*precision;
        }
        break;
#endif

#if !DISABLE_QUALITY_PARAM
        case kQuality:
        {
            Quality qual = (Quality)GetParam(paramIdx)->Int();
          
            if (qual != mQuality)
            {
                mQuality = qual;
              
                QualityChanged();
            }
        }
        break;
#endif
        
        case kFreqAmpRatio:
        {
            BL_FLOAT freqAmpRatio = GetParam(kFreqAmpRatio)->Value();
            freqAmpRatio /= 100.0;
            mFreqAmpRatio = freqAmpRatio;
        }
        break;
      
        case kOutGain:
        {
            BL_FLOAT gain = GetParam(kOutGain)->DBToAmp();
            mOutGain = gain;

            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(gain);
        }
        break;
        
#if USE_SCAN_DISPLAY
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            
            mMonitorEnabled = (value == 1);
            
            if (mScanDisplay != NULL)
                mScanDisplay->SetEnabled(mIsPlaying || mMonitorEnabled);
        }
        break;
#endif

        case kZoom:
        {
            BL_FLOAT zoom = GetParam(kZoom)->Value();
            mZoom = zoom;

            if (mScanDisplay != NULL)
                mScanDisplay->SetZoom(mZoom);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Shaper::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Shaper::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    mGraph = NULL;

#if USE_SCAN_DISPLAY
    mMonitorControl = NULL;
    
    if (mScanDisplay != NULL)
        mScanDisplay->SetGraph(NULL);
#endif
    
    LEAVE_PARAMS_MUTEX;
}

void
Shaper::CreateGraphAxis()
{
    // Create
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
    }
    
    // Update
    mGraph->SetVAxis(mVAxis);
    
    //
    // Freq axis
    //
    int axisColor[4];
    mGUIHelper->GetGraphAxisColor(axisColor);
    
    int axisLabelColor[4];
    mGUIHelper->GetGraphAxisLabelColor(axisLabelColor);
    
    int axisLabelOverlayColor[4];
    mGUIHelper->GetGraphAxisLabelOverlayColor(axisLabelOverlayColor);
    
    int width = 1;
    int height = 1;
    if (mGraph != NULL)
        mGraph->GetSize(&width, &height);
    
    float lineWidth = 1.0;
    mVAxis->InitVAxis(Scale::LINEAR,
                      0.0, 1.0,
                      axisColor, axisLabelColor,
                      lineWidth,
                      3.0/((BL_FLOAT)width),
                      0.0,
                      axisLabelOverlayColor);

#define NUM_SIG_AXIS_DATA 5
    static char *SIG_AXIS_DATA [NUM_SIG_AXIS_DATA][2] =
    {
#if !USE_SCAN_DISPLAY
        // Labels
        { "-1.0", "-100%" },
        { "-0.5", "-50%" },
        { "0.0",  " 0%" },
        { "0.5",  " 50%" },
        { "1.0",  " 100%" }
#else
        // No labels, only lines
        { "-1.0", "" },
        { "-0.5", "" },
        { "0.0",  "" },
        { "0.5",  "" },
        { "1.0",  "" }
#endif
    };

    mVAxis->SetMinMaxValues(-1.0, 1.0);
    mVAxis->SetData(SIG_AXIS_DATA, NUM_SIG_AXIS_DATA);
}

#if !USE_SCAN_DISPLAY
void
Shaper::CreateGraphCurves()
{
    int curveDescriptionColor[4];
    mGUIHelper->GetGraphCurveDescriptionColor(curveDescriptionColor);
    
    if (mInputCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int curveColor[4];
        mGUIHelper->GetGraphCurveColorBlue(curveColor);
        
        mInputCurve = new GraphCurve5(CURVE_NUM_POINTS);
        mInputCurve->SetDescription("input", curveDescriptionColor);
        mInputCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mInputCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mInputCurve->SetColor(curveColor[0], curveColor[1], curveColor[2]);
        mInputCurve->SetLineWidth(2.0);
      
#if BEVEL_CURVES
        mInputCurve->SetBevel(true);
#endif
    }

    if (mOutputCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int curveColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(curveColor);
        
        mOutputCurve = new GraphCurve5(CURVE_NUM_POINTS);
        mOutputCurve->SetDescription("output", curveDescriptionColor);
        mOutputCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mOutputCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mOutputCurve->SetColor(curveColor[0], curveColor[1], curveColor[2]);
        mOutputCurve->SetLineWidth(2.0);
      
#if BEVEL_CURVES
        mOutputCurve->SetBevel(true);
#endif
    }
    
    if (mGraph == NULL)
        return;
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mInputCurve->SetViewSize(graphSize[0], graphSize[1]);
    mOutputCurve->SetViewSize(graphSize[0], graphSize[1]);
    
    mGraph->AddCurve(mInputCurve);
    mGraph->AddCurve(mOutputCurve);
}
#endif

void
Shaper::UpdateCurves(const WDL_TypedBuf<BL_FLOAT> &inputBuf,
                     const WDL_TypedBuf<BL_FLOAT> &outputBuf,
                     const WDL_TypedBuf<BL_FLOAT> &transientness)
{
    if (!mUIOpened)
        return;

#if !USE_SCAN_DISPLAY
    if (mInputCurve != NULL)
        mInputCurve->SetValues5(inputBuf);

    if (mOutputCurve != NULL)
        mOutputCurve->SetValues5(outputBuf);
#else
    if (mScanDisplay != NULL)
    {
        mScanDisplay->AddSamples(CURVE_INPUT, inputBuf, false);
        mScanDisplay->AddSamples(CURVE_OUTPUT, outputBuf, false);
        mScanDisplay->AddSamples(CURVE_TRANSIENTS, transientness, true);
    }
#endif
}

void
Shaper::QualityChanged()
{
    mOversampling = OVERSAMPLING_0;
  
    switch(mQuality)
    {
        case STANDARD:
            mOversampling = OVERSAMPLING_0;
            break;
      
        case HIGH:
            mOversampling = OVERSAMPLING_1;
            break;
      
        case VERY_HIGH:
            mOversampling = OVERSAMPLING_2;
            break;
      
        case OFFLINE:
            mOversampling = OVERSAMPLING_3;
            break;
      
        default:
            break;
    }
  
    mQualityChanged = true;
}

#if USE_SCAN_DISPLAY
// NOTE: OnIdle() is called from the GUI thread
void
Shaper::OnIdle()
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

        // Stop automatic smooth scroll if the plugin is bypassed
        if (mBypassDetector != NULL)
        {
            mBypassDetector->TouchFromIdleThread();

            bool bypassed = mBypassDetector->PlugIsBypassed();
            
            if (bypassed)
            {
                if (mScanDisplay != NULL)
                    mScanDisplay->SetEnabled(false);
            }
            else
            {
                if (mScanDisplay != NULL)
                    mScanDisplay->SetEnabled(mIsPlaying || mMonitorEnabled);
            }
        }
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Shaper::UpdateZoom(BL_FLOAT zoomChange)
{
    mZoom *= zoomChange;

    if (mZoom < MIN_ZOOM)
        mZoom = MIN_ZOOM;
    if (mZoom > MAX_ZOOM)
        mZoom = MAX_ZOOM;

    if (mScanDisplay != NULL)
        mScanDisplay->SetZoom(mZoom);

    // Parameter
    BLUtilsPlug::SetParameterValue(this, kZoom, mZoom, false);
}
                   
void
Shaper::ResetZoom()
{
    mZoom = 1.0;

    if (mScanDisplay != NULL)
        mScanDisplay->SetZoom(mZoom);

    // Parameter
    BLUtilsPlug::SetParameterValue(this, kZoom, mZoom, false);
}

#if USE_SCAN_DISPLAY
void
Shaper::SetCurvesStyle()
{
    if (mScanDisplay == NULL)
        return;

    int descrColor[4];
    if (mGUIHelper != NULL)
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
        
    int inputColor[4];
    if (mGUIHelper != NULL)
    {
        mGUIHelper->GetGraphCurveColorBlue(inputColor);
        mScanDisplay->SetCurveStyle(CURVE_INPUT, "in", descrColor,
                                    true, 2.0, true, inputColor);
    }
    
    int outputColor[4];
    if (mGUIHelper != NULL)
    {
        mGUIHelper->GetGraphCurveColorLightBlue(outputColor);
        mScanDisplay->SetCurveStyle(CURVE_OUTPUT, "out", descrColor,
                                    true, 1.5, false, outputColor);
    }
    
    int transientColor[4];
    if (mGUIHelper != NULL)
    {
        mGUIHelper->GetGraphCurveColorGreen(transientColor);
        mScanDisplay->SetCurveStyle(CURVE_TRANSIENTS, "transient", descrColor,
                                    false, 2.0,
                                    false, transientColor);
    }
}
#endif

#endif
