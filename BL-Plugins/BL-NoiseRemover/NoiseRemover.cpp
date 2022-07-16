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
#include <NoiseRemoverObj.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include <ParamSmoother2.h>

// Display
#include <GraphControl12.h>
#include <GraphFreqAxis2.h>
#include <GraphAmpAxis.h>
#include <GraphAxis2.h>

#include <GraphCurve5.h>
#include <SmoothCurveDB.h>

//#include <Scale.h> ??

#include "NoiseRemover.h"

#include "IPlug_include_in_plug_src.h"

/////////// config
#define ROBOTO_FN "Roboto-Regular.ttf"

#define FONT_REGULAR_FN "font-regular.ttf"
#define FONT_LIGHT_FN "font-light.ttf"
#define FONT_BOLD_FN "font-bold.ttf"

#define FONT_OPENSANS_EXTRA_BOLD_FN "OpenSans-ExtraBold.ttf"
#define FONT_ROBOTO_BOLD_FN "Roboto-Bold.ttf"

#define MANUAL_FN "BL-Denoiser_manual-EN.pdf"
// For WIN32
#define MANUAL_FULLPATH_FN "manual\NoiseRemover_manual-EN.pdf"

#define DUMMY_RESOURCE_FN "dummy.txt"

#define BACKGROUND_FN "background.png"
#define PLUGNAME_FN "plugname.png"
#define HELP_BUTTON_FN "help-button.png"
#define TEXTFIELD_FN "textfield.png"
#define CHECKBOX_FN "checkbox.png"

#define KNOB_FN "knob.svg"
#define KNOB_SMALL_FN "knob-small.svg"

#define GRAPH_FN "graph.png"
///////////

#define BUFFER_SIZE 2048
#define OVERLAPPING 4

// Display
#define GRAPH_MIN_DB -119.0 // Take care of the noise/harmo bottom dB
#define GRAPH_MAX_DB 10.0

#define GRAPH_CURVE_NUM_VALUES 512
#define CURVE_SMOOTH_COEFF_MS 1.4
#define NOISE_CURVE_SMOOTH_COEFF_MS 0.0 //1.4

#define RATIO_SMOOTH_TIME_MS 100.0

// In theory, we don't need the controls
#define SHOW_CONTROLS 1 //0 // 1

// for Spectral irregularity
#define MIN_RESO 4
#define MAX_RESO 512 //64

static char *tooltipHelp = "Help - Display Help";
static char *tooltipRatio = "Ratio (noise -> signal)";
static char *tooltipSoftMasks = "Smart Resynthesis - Higher quality resynthesis";
static char *tooltipNoiseFloorOffset = "Noise floor offset";
static char *tooltipResolution = "Resolution";
static char *tooltipNoiseSmoothTimeMs = "Noise curve smooth time";

enum EParams
{
    kRatio = 0,
    kUseSoftMasks,
    kNoiseFloorOffset,
    kResolution,
    kNoiseSmoothTimeMs,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kGraphX = 0,
    kGraphY = 0,
    
    kKnobWidth = 72,
    kKnobHeight = 72,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,
    
    kRatioX = 194,
    kRatioY = 245,

    kUseSoftMasksX = 40,
    kUseSoftMasksY = 282,
    
    kNoiseFloorOffsetX = 100,
    kNoiseFloorOffsetY = 225,

    kResolutionX = 100,
    kResolutionY = 325,

    kNoiseSmoothTimeMsX = 388,
    kNoiseSmoothTimeMsY = 225
};

//
NoiseRemover::NoiseRemover(const InstanceInfo &info)
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

NoiseRemover::~NoiseRemover()
{
    if (mRatioSmoother != NULL)
        delete mRatioSmoother;

    if (mGUIHelper != NULL)
        delete mGUIHelper;

    // Display
    if (mInCurve != NULL)
        delete mInCurve;
    if (mSigCurve != NULL)
        delete mSigCurve;
    if (mNoiseCurve != NULL)
        delete mNoiseCurve;

    if (mInCurveSmooth != NULL)
        delete mInCurveSmooth;
    if (mSigCurveSmooth != NULL)
        delete mSigCurveSmooth;
    if (mNoiseCurveSmooth != NULL)
        delete mNoiseCurveSmooth;

    
}

IGraphics *
NoiseRemover::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
NoiseRemover::MyMakeLayout(IGraphics *pGraphics)
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
NoiseRemover::InitNull()
{
    BLUtilsPlug::PlugInits();

    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;

    mRatio = 0.0;
    mRatioSmoother = NULL;
    mNoiseFloorOffset = 0.0;
    mResolution = MIN_RESO;
    mNoiseSmoothTimeMs = 0.0;
        
    mLatencyChanged = true;
    
    mFftObj = NULL;
    
    mIsInitialized = false;

    mGUIHelper = NULL;

    // Display
    //
    mGraph = NULL;
    
    mAmpAxis = NULL;
    mHAxis = NULL;
    
    mFreqAxis = NULL;
    mVAxis = NULL;
    
    //
    mInCurve = NULL;
    mSigCurve = NULL;
    mNoiseCurve = NULL;
    
    mInCurveSmooth = NULL;
    mSigCurveSmooth = NULL;
    mNoiseCurveSmooth = NULL;

    
}

void
NoiseRemover::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT defaultRatio = 1.0; // 1 is 0dB
    mRatio = defaultRatio;

    BL_FLOAT sampleRate = GetSampleRate();
    mRatioSmoother = new ParamSmoother2(sampleRate, defaultRatio);
    
    BL_FLOAT defaultNoiseFloorOffset = 0.0;
    mNoiseFloorOffset = defaultNoiseFloorOffset;

    int defaultResolution = MIN_RESO;
    mResolution = defaultResolution;

    mNoiseSmoothTimeMs = 0.0;
    
    // Init fft
    if (mFftObj == NULL)
    {
        vector<ProcessObj *> processObjs;

        BL_FLOAT sampleRate = GetSampleRate();
        
        // Signal
        for (int i = 0; i < 2; i++)
        {
            mNoiseRemoverObjs[i] =
                new NoiseRemoverObj(BUFFER_SIZE,
                                    OVERLAPPING, sampleRate);

            mNoiseRemoverObjs[i]->SetRatio(mRatio);
                
            processObjs.push_back(mNoiseRemoverObjs[i]);
        }
    
        
        int numChannels = 2;
        int numScInputs = 0;
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      BUFFER_SIZE,
                                      OVERLAPPING,
                                      1,
                                      sampleRate);
    
        mFftObj->
            SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                              FftProcessObj16::WindowHanning);
      
        mFftObj->
            SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                               FftProcessObj16::WindowHanning);
    }
    else
    {
        mFftObj->Reset(BUFFER_SIZE, OVERLAPPING,
                       1, sampleRate);
    }

    mLatencyChanged = true;
    UpdateLatency();

    ApplyParams();
    
    mIsInitialized = true;
}

void
NoiseRemover::InitParams()
{
    // Ratio
    BL_FLOAT defaultRatio = 100.0;
    mRatio = 100.0;
    GetParam(kRatio)->InitDouble("Ratio", defaultRatio, 0.0, 100.0, 0.01, "%");

    // Use soft masks
    bool defaultUseSoftMasks = true; //false;
    mUseSoftMasks = defaultUseSoftMasks;
    GetParam(kUseSoftMasks)->InitEnum("SmartResynth", 0, 2,
                                      "", IParam::kFlagsNone, "",
                                      "Off", "On");

    // Noise floor offset
#define OFFSET_MAX 2.0
    BL_FLOAT defaultNoiseFloorOffset = 0.02; //0.24; //0.2; //0.0;
    mNoiseFloorOffset = 0.0;
    GetParam(kNoiseFloorOffset)->
        InitDouble("NoiseFloorOffset",
                   defaultNoiseFloorOffset, -OFFSET_MAX, OFFSET_MAX, 0.01, "%");

    // Resolution
    int defaultResolution = MIN_RESO;
    mResolution = defaultResolution;
    GetParam(kResolution)->InitInt("Resolution",
                                   defaultResolution,
                                   1, MAX_RESO);

    BL_FLOAT defaultNoiseSmoothTimeMs = 50.0; //0.0;
    mNoiseSmoothTimeMs = defaultNoiseSmoothTimeMs;
    GetParam(kNoiseSmoothTimeMs)->
        InitDouble("NoiseSmoothTimeMs",
                   defaultNoiseSmoothTimeMs, 0.0,
                   1000.0,
                   //5000.0,
                   1.0, "ms");
}

void
NoiseRemover::ProcessBlock(iplug::sample **inputs,
                           iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    if (mLatencyChanged)
    {
        UpdateLatency();

        mLatencyChanged = false;
    }
    
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

    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
    {
        if (mGraph != NULL)
            mGraph->PushAllData();
        
        return;
    }
    
    mSecureRestarter.Process(in);

    // Ratio param
    BL_FLOAT ratio = mRatioSmoother->Process();
    {
        for (int i = 0; i < 2; i++)
            mNoiseRemoverObjs[i]->SetRatio(ratio);
    }

    // Process
    mFftObj->Process(in, scIn, &out);
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
    
    // Display
    UpdateCurves();

    if (mGraph != NULL)
        mGraph->PushAllData();
  
    BL_PROFILE_END;
}

void
NoiseRemover::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);

    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);
    
    // Graph
    // Separator
    IColor sepIColor;
    mGUIHelper->GetGraphSeparatorColor(&sepIColor);
    int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurves();
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);

#if SHOW_CONTROLS
    // Knob(s)

    // Ratio
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kRatioX, kRatioY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              //kRatioFrames,
                              kRatio,
                              TEXTFIELD_FN,
                              "RATIO",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipRatio);

    // Use soft masks
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseSoftMasksX,
                                   kUseSoftMasksY,
                                   CHECKBOX_FN, kUseSoftMasks,
                                   "SMART RESYNTH",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipSoftMasks);

    // Noise floor offset
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kNoiseFloorOffsetX, kNoiseFloorOffsetY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_FN,
                              kNoiseFloorOffset,
                              TEXTFIELD_FN,
                              "NOISE FLOOR OFFSET",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipNoiseFloorOffset);

    // Resolution
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kResolutionX, kResolutionY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_FN,
                              kResolution,
                              TEXTFIELD_FN,
                              "RESOLUTION",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipResolution);

    // Smooth factor
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kNoiseSmoothTimeMsX, kNoiseSmoothTimeMsY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_FN,
                              kNoiseSmoothTimeMs,
                              TEXTFIELD_FN,
                              "SMOOTH FACTOR",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipNoiseSmoothTimeMs);
#endif
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //                           kLogoAnimFrames, GUIHelper12::BOTTOM);

#if 0
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN, GUIHelper12::BOTTOM);
#endif
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM,
                                 tooltipHelp);
  
    mGUIHelper->CreateDemoMessage(pGraphics);
  
    mControlsCreated = true;
}

void
NoiseRemover::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
NoiseRemover::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    if (mRatioSmoother != NULL)
    {
        BL_FLOAT smoothTime = RATIO_SMOOTH_TIME_MS;
        int blockSize = GetBlockSize();
        smoothTime /= blockSize;
        
        mRatioSmoother->Reset(sampleRate, smoothTime);
    }
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
NoiseRemover::OnIdle()
{
    if (!mUIOpened)
        return;
    
    ENTER_PARAMS_MUTEX;

    mInCurveSmooth->PullData();
    mSigCurveSmooth->PullData();
    mNoiseCurveSmooth->PullData();
    
    LEAVE_PARAMS_MUTEX;

    // We don't need mutex anymore now
    mInCurveSmooth->ApplyData();
    mSigCurveSmooth->ApplyData();
    mNoiseCurveSmooth->ApplyData();
    
}

void
NoiseRemover::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kRatio:
        {
            BL_FLOAT ratio = GetParam(kRatio)->Value();
            mRatio = ratio*0.01;

            mRatioSmoother->SetTargetValue(mRatio);
            
            //for (int i = 0; i < 2; i++)
            //    mNoiseRemoverObjs[i]->SetRatio(mRatio);
        }
        break;

        case kUseSoftMasks:
        {
            int value = GetParam(kUseSoftMasks)->Value();
            bool useSoftMasksFlag = (value == 1);
            mUseSoftMasks = useSoftMasksFlag;
      
            for (int i = 0; i < 2; i++)
            {
                if (mNoiseRemoverObjs[i] != NULL)
                    mNoiseRemoverObjs[i]->SetUseSoftMasks(mUseSoftMasks);
            }

            mLatencyChanged = true;
        }
        break;

        case kNoiseFloorOffset:
        {
            BL_FLOAT noiseFloorOffset = GetParam(kNoiseFloorOffset)->Value();
             mNoiseFloorOffset = noiseFloorOffset*0.01;

            for (int i = 0; i < 2; i++)
            {
                if (mNoiseRemoverObjs[i] != NULL)
                    mNoiseRemoverObjs[i]->SetNoiseFloorOffset(mNoiseFloorOffset);
            }
        }
        break;

        case kResolution:
        {
            int reso = GetParam(kResolution)->Int();
            mResolution = reso;

            for (int i = 0; i < 2; i++)
            {
                if (mNoiseRemoverObjs[i] != NULL)
                    mNoiseRemoverObjs[i]->SetResolution(mResolution);
            }
        }
        break;

        case kNoiseSmoothTimeMs:
        {
            BL_FLOAT value = GetParam(kNoiseSmoothTimeMs)->Value();
            mNoiseSmoothTimeMs = value;

            for (int i = 0; i < 2; i++)
            {
                if (mNoiseRemoverObjs[i] != NULL)
                    mNoiseRemoverObjs[i]->
                        SetNoiseSmoothTimeMs(mNoiseSmoothTimeMs);
            }
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
NoiseRemover::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
NoiseRemover::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;

    mGraph = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
NoiseRemover::ApplyParams()
{
    if (mRatioSmoother != NULL)
        mRatioSmoother->ResetToTargetValue(mRatio);

    for (int i = 0; i < 2; i++)
    {
        if (mNoiseRemoverObjs[i] != NULL)
            mNoiseRemoverObjs[i]->SetUseSoftMasks(mUseSoftMasks);
    }
    mLatencyChanged = true;

    for (int i = 0; i < 2; i++)
    {
        if (mNoiseRemoverObjs[i] != NULL)
            mNoiseRemoverObjs[i]->SetNoiseFloorOffset(mNoiseFloorOffset);
    }

    for (int i = 0; i < 2; i++)
    {
        if (mNoiseRemoverObjs[i] != NULL)
            mNoiseRemoverObjs[i]->SetResolution(mResolution);
    }
}

void
NoiseRemover::CreateGraphAxes()
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
    mFreqAxis->Init(mHAxis, mGUIHelper, horizontal,
                    BUFFER_SIZE, sampleRate, graphWidth);
    mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    mAmpAxis->Init(mVAxis, mGUIHelper, GRAPH_MIN_DB, GRAPH_MAX_DB, graphWidth);
}

void
NoiseRemover::CreateGraphCurves()
{
    //BL_FLOAT sampleRate = GetSampleRate();
#define REF_SAMPLERATE 44100.0
    BL_FLOAT curveSmoothCoeff = ParamSmoother2::
        ComputeSmoothFactor(CURVE_SMOOTH_COEFF_MS, REF_SAMPLERATE);

    BL_FLOAT noiseCurveSmoothCoeff = ParamSmoother2::
        ComputeSmoothFactor(NOISE_CURVE_SMOOTH_COEFF_MS, REF_SAMPLERATE);
    
    if (mInCurve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
    
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        // Input signal
        int inColor[4];
        mGUIHelper->GetGraphCurveColorBlue(inColor);
        
        mInCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mInCurve->SetDescription("input", descrColor);
        mInCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5); //
        mInCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mInCurve->SetFill(true);
        mInCurve->SetFillAlpha(fillAlpha);
        mInCurve->SetColor(inColor[0], inColor[1], inColor[2]);
        mInCurve->SetLineWidth(2.0);
        
        mInCurveSmooth = new SmoothCurveDB(mInCurve,
                                           curveSmoothCoeff,
                                           GRAPH_CURVE_NUM_VALUES,
                                           GRAPH_MIN_DB,
                                           GRAPH_MIN_DB, GRAPH_MAX_DB,
                                           sampleRate);
    
        // output signal  
        int sigColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(sigColor);
        
        mSigCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mSigCurve->SetDescription("signal", descrColor);
        mSigCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mSigCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        //mSigCurve->SetFill(true);
        //mSigCurve->SetFillAlpha(fillAlpha);
        mSigCurve->SetColor(sigColor[0], sigColor[1], sigColor[2]);
        
        mSigCurveSmooth = new SmoothCurveDB(mSigCurve,
                                            curveSmoothCoeff,
                                            GRAPH_CURVE_NUM_VALUES,
                                            GRAPH_MIN_DB,
                                            GRAPH_MIN_DB, GRAPH_MAX_DB,
                                            sampleRate);
        
        // noise
        int noiseColor[4];
        mGUIHelper->GetGraphCurveColorRed(noiseColor);
        
        mNoiseCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mNoiseCurve->SetDescription("noise", descrColor);
        mNoiseCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mNoiseCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        //mNoiseCurve->SetFillAlpha(fillAlpha);
        mNoiseCurve->SetColor(noiseColor[0], noiseColor[1], noiseColor[2]);
        
        mNoiseCurveSmooth = new SmoothCurveDB(mNoiseCurve,
                                              noiseCurveSmoothCoeff,
                                              GRAPH_CURVE_NUM_VALUES,
                                              GRAPH_MIN_DB,
                                              GRAPH_MIN_DB, GRAPH_MAX_DB,
                                              sampleRate);
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
        
    mInCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mInCurve);
    
    mSigCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSigCurve);

    // Add sum curve over all other curves
    mNoiseCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mNoiseCurve);
}

void
NoiseRemover::UpdateCurves()
{
    if (!mUIOpened)
        return;
    
    WDL_TypedBuf<BL_FLOAT> &inSig = mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> &sig = mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> &noise = mTmpBuf5;
    
    mNoiseRemoverObjs[0]->GetInputSignalFft(&inSig);
    mNoiseRemoverObjs[0]->GetSignalFft(&sig);
    mNoiseRemoverObjs[0]->GetNoiseFft(&noise);

    mInCurveSmooth->SetValues(inSig);
    mSigCurveSmooth->SetValues(sig);
    mNoiseCurveSmooth->SetValues(noise);

    // Lock free
    mInCurveSmooth->PushData();
    mSigCurveSmooth->PushData();
    mNoiseCurveSmooth->PushData();
}

void
NoiseRemover::UpdateLatency()
{
    // Latency
    int blockSize = GetBlockSize();
    int latency = 0;
    
    if (mFftObj != NULL)
        latency += mFftObj->ComputeLatency(blockSize);
    
    if (mNoiseRemoverObjs[0] != NULL)
    {
        int lat0 = mNoiseRemoverObjs[0]->GetLatency();
        latency += lat0;
    }
    
    SetLatency(latency);

    mLatencyChanged = false;
}
