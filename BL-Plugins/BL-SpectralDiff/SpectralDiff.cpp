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

#ifdef APP_API
#ifndef WIN32 // WIN32 does not have basename
// For basename();
#include <libgen.h>
#endif
#endif

#include <FftProcessObj16.h>
#include <SpectralDiffObj.h>

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

#include <ParamSmoother2.h>

#if APP_API
#include <AudioFile.h>
#include <Scale.h>
#endif

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "SpectralDiff.h"

#include "IPlug_include_in_plug_src.h"


// When sidechain is mono, take only the left stereo channel
#define PREFER_LEFT_CHANNEL 1

// More damp looks better
// But we could do the same thing as with EQHack
#define MORE_DAMP_DIFF_CURVE 0

// With 1024, the frequency axis is not well displayed
// (GRAPH_NUM_POINTS has had to be changed too)
//#define BUFFER_SIZE 1024
#define BUFFER_SIZE 2048

// OVERSAMPLING
// 1: the curves are not well displayed
// 4: we don't need to consume too many resources
#if !NO_SOUND_OUTPUT
#define OVERSAMPLING 4
#else
// When no sound, if OVERSAMPLING > 1, the side chain
// and the signal are not matching exactly anymore
#define OVERSAMPLING 1
#endif

#define FREQ_RES 1

#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

#define GRAPH_MIN_DB -119.0 //-120.0
#define GRAPH_MAX_DB 10.0 //0.0

#define GRAPH_EPS_DB 1e-15

// Maybe too fast
//#define SMOOTH_HISTO_COEFF 0.5

// Too dampy, the first display is too long to appear
//#define SMOOTH_HISTO_COEFF 0.8

//#define SMOOTH_HISTO_COEFF 0.5
//#define SMOOTH_HISTO_COEFF 0.8
//#define HISTO_DEFAULT_VALUE -60.0

// Graph...
#define MIDDLE_DB_GRAPH -60.0

// Graph
#define GRAPH_NUM_CURVES 4

#define GRAPH_DIFF_ZERO_CURVE 0
#define GRAPH_SIGNAL0_CURVE 1
#define GRAPH_SIGNAL1_CURVE 2
#define GRAPH_DIFF_CURVE 3

// Changed when changed buffer size
// (otherwise, the curves were not displayed on the first 1/4 of the graph)
//#define GRAPH_NUM_POINTS 1024 //512

//#define CURVE_FILL_ALPHA 0.2

// Axis
//#define MIN_AXIS_VALUE 20.0
//#define MAX_AXIS_VALUE 20000.0


// Signal curves are more round like this
#define HIGH_RES_LOG_CURVES 1

#if !HIGH_RES_LOG_CURVES
#define GRAPH_NUM_POINTS 1024
#else
#define GRAPH_NUM_POINTS 256
#define GRAPH_DEC_FACTOR 0.25
#endif

// See Air for details
//#define CURVE_SMOOTH_COEFF 0.95
#define CURVE_SMOOTH_COEFF_MS 1.4

// GUI Size
#define PLUG_WIDTH_MEDIUM 752
#define PLUG_HEIGHT_MEDIUM 428

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 592

// GUI Size
#define NUM_GUI_SIZES 3

  // Display diff computed inside the fft ?
#define DISPLAY_FFT_DIFF 0

// Optimization
#define OPTIM_SKIP_IFFT 1

//
#if 0
TODO: recompile, to benefit from FIX_TOO_MANY_INPUT_CHANNELS

NOTE: since the resize buttons, on Logic (and Protools, mac), it will be necessary to
remove the prev version (without resize gui buttons)from the track
and to insert the new version.
Otherwise the first GUI button won t be selected at the beginning, 
and the display will clear when trying to changr the GUI size

BUG: Ableton 10, Sierra, VST3: the sound does not make the line move

NOTE: StudioOne, Sierra, VST: side chain does not enter in the plugins
=> must use AU or VST3 for sidechain

NOTE: Cubase Pro 10, Sierra: Cubase supports side chain for third party plugins only in VST3 format
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";

enum EParams
{  
    kGUISizeSmall = 0,
    kGUISizeMedium,
    kGUISizeBig,

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

    kGraphX = 0,
    kGraphY = 0,
    
    // GUI size
    kGUISizeSmallX = 15,
    kGUISizeSmallY = 201,
  
    kGUISizeMediumX = 40,
    kGUISizeMediumY = 201,
    
    kGUISizeBigX = 64,
    kGUISizeBigY = 201
};


//
SpectralDiff::SpectralDiff(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , ResizeGUIPluginInterface(this)
{
    TRACE;

    // By default, we can't resize a plugin GUI more than 2 x the original size
    // In SpectralDiff particularly, we need to resize more,
    // so whange the constraints
    SetSizeConstraints(PLUG_WIDTH, PLUG_WIDTH_BIG, PLUG_HEIGHT, PLUG_WIDTH_BIG);
    
    InitNull();
    InitParams();

    Init(OVERSAMPLING, FREQ_RES);
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

SpectralDiff::~SpectralDiff()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mFftObj != NULL)
        delete mFftObj;

    for (int i = 0; i < 2; i++)
    {
        if (mSpectralDiffObjs[i] != NULL)
            delete mSpectralDiffObjs[i];
    }
    
    //
    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mAmpAxis != NULL)
        delete mAmpAxis;

    if (mVAxis != NULL)
        delete mVAxis;
    
    if (mSignal0Curve != NULL)
        delete mSignal0Curve;
    if (mSignal0CurveSmooth != NULL)
        delete mSignal0CurveSmooth;
    
    if (mSignal1Curve != NULL)
        delete mSignal1Curve;
    if (mSignal1CurveSmooth != NULL)
        delete mSignal1CurveSmooth;
    
    if (mDiffCurve != NULL)
        delete mDiffCurve;
    if (mDiffCurveSmooth != NULL)
        delete mDiffCurveSmooth;

#if APP_API
    delete mScale;
#endif
}

IGraphics *
SpectralDiff::MyMakeGraphics()
{
    int newGUIWidth;
    int newGUIHeight;
    GetNewGUISize(mGUISizeIdx, &newGUIWidth, &newGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            newGUIWidth, newGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);

    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       newGUIWidth, newGUIHeight,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
SpectralDiff::MyMakeLayout(IGraphics *pGraphics)
{
    ENTER_PARAMS_MUTEX;

    // Remove all the controls, useful for example just after a GUI resize
    pGraphics->RemoveAllControls();

    // IGraphics: DEFAULT_FONT
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    
    pGraphics->LoadFont("font-regular", FONT_REGULAR_FN);
    pGraphics->LoadFont("font-light", FONT_LIGHT_FN);
    pGraphics->LoadFont("font-bold", FONT_BOLD_FN);

    // Style: BLULAB_V3
    pGraphics->LoadFont("OpenSans-ExtraBold", FONT_OPENSANS_EXTRA_BOLD_FN);
    pGraphics->LoadFont("Roboto-Bold", FONT_ROBOTO_BOLD_FN);
    
    if (mGUISizeIdx == 0)
        pGraphics->AttachBackground(BACKGROUND_FN);
    else if (mGUISizeIdx == 1)
        pGraphics->AttachBackground(BACKGROUND_MED_FN);
    else if (mGUISizeIdx == 2)
        pGraphics->AttachBackground(BACKGROUND_BIG_FN);

    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);

    pGraphics->EnableTooltips(true);
    pGraphics->SetTooltipsDelay(TOOLTIP_DELAY);
    
    CreateControls(pGraphics, mGUIOffsetY);

    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectralDiff::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mFftObj = NULL;
    
    mSpectralDiffObjs[0] = NULL;
    mSpectralDiffObjs[1] = NULL;
    
    mGUIHelper = NULL;
    
    mGraph = NULL;
    
    mAmpAxis = NULL;
    mHAxis = NULL;
    
    mFreqAxis = NULL;
    mVAxis = NULL;
    
    mSignal0Curve = NULL;
    mSignal0CurveSmooth = NULL;
    
    mSignal1Curve = NULL;
    mSignal1CurveSmooth = NULL;
    
    mDiffCurve = NULL;
    mDiffCurveSmooth = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;

    mGraphWidthSmall = 256;
    mGraphHeightSmall = 56;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;

#if APP_API
    mBriefDislay = false;
    mScale = NULL;
#endif
}

void
SpectralDiff::InitParams()
{
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->
        InitInt("SmallGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    
    GetParam(kGUISizeMedium)->
        InitInt("MediumGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
    
    GetParam(kGUISizeBig)->
        InitInt("BigGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
}

void
SpectralDiff::ApplyParams()
{
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
SpectralDiff::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < 2; i++)
        {
            mSpectralDiffObjs[i] =
                new SpectralDiffObj(BUFFER_SIZE, OVERSAMPLING, FREQ_RES, sampleRate);
      
            processObjs.push_back(mSpectralDiffObjs[i]);
        }

#if !APP_API
        int numChannels = 2;
        int numScInputs = 2;
#else
        int numChannels = 1;
        int numScInputs = 1;
#endif
        
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      BUFFER_SIZE, oversampling, freqRes,
                                      sampleRate);

#if OPTIM_SKIP_IFFT
        mFftObj->SetSkipIFft(-1, true);
#endif
        
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
    }
    else
    {
        //BL_FLOAT sampleRate = GetSampleRate();
        
        FftProcessObj16 *fftObj = mFftObj;
        fftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }

#if APP_API
    mScale = new Scale();
#endif
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
SpectralDiff::ProcessBlock(iplug::sample **inputs,
                           iplug::sample **outputs, int nFrames)
{
#if APP_API
    return;
#endif
    
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

    mSecureRestarter.Process(in);
    
    WDL_TypedBuf<BL_FLOAT> &signal0Spect = mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> &signal1Spect = mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> &diffSpect = mTmpBuf5;

    bool curvesChanged = false;
        
#if !PREFER_LEFT_CHANNEL
    // If we have only one side chain, process all in mono
    if ((scIn.size() == 1) && (in.size() == 2))
    {
        WDL_TypedBuf<BL_FLOAT> &inMono = mTmpBuf6;
        BLUtils::StereoToMono(&inMono, in[0], in[1]);
        
        vector<WDL_TypedBuf<BL_FLOAT> > &inMonoVec = mTmpBuf7;
        inMonoVec.resize(1);
        inMonoVec[0] = inMono;
      
#if !NO_SOUND_OUTPUT
        mFftObj->Process(inMonoVec, out, NULL, nFrames);
        
        // Duplicate to the second output if necessary
        if (out[1] != NULL)
            memcpy(out[1], out[0], nFrames*sizeof(BL_FLOAT));
#else
        mFftObj->Process(inMonoVec, scIn, NULL);
#endif
        
        // Get the results
        mSpectralDiffObjs[0]->GetSignal0BufferSpect(&signal0Spect);
        mSpectralDiffObjs[0]->GetSignal1BufferSpect(&signal1Spect);
        mSpectralDiffObjs[0]->GetDiffSpect(&diffSpect);
    }
    else
#endif // !PREFER_LEFT_CHANNEL   
    {
        WDL_TypedBuf<BL_FLOAT> *signal0SpectSt = mTmpBuf8;
        WDL_TypedBuf<BL_FLOAT> *signal1SpectSt = mTmpBuf9;
        WDL_TypedBuf<BL_FLOAT> *diffSpectSt = mTmpBuf10;

#if !NO_SOUND_OUTPUT
        vector<WDL_TypedBuf<BL_FLOAT> > dummy;
        
        mFftObj->Process(in, &dummy, &out);
#else
        mFftObj->Process(in, scIn, NULL);
#endif
        
        // Init default values in case of sidechain is not connected
        signal0Spect.Resize(BUFFER_SIZE/2);
        BLUtils::FillAllZero(&signal0Spect);
        
        signal1Spect.Resize(BUFFER_SIZE/2);
        BLUtils::FillAllZero(&signal1Spect);
        
        diffSpect.Resize(BUFFER_SIZE/2);
        BLUtils::FillAllZero(&diffSpect);

        // Get data
        for (int i = 0; i < 2; i++)
        {
#if !PREFER_LEFT_CHANNEL
            if ((i < in.size()) && (i < out.size()))
#else
                if ((i < in.size()) && (i < scIn.size()))
#endif
                {
                    // Get the results
                    bool res0 = mSpectralDiffObjs[i]->
                        GetSignal0BufferSpect(&signal0SpectSt[i]);
                    bool res1 = mSpectralDiffObjs[i]->
                        GetSignal1BufferSpect(&signal1SpectSt[i]);
                    bool res2 = mSpectralDiffObjs[i]->GetDiffSpect(&diffSpectSt[i]);

                    if (res0 || res1 || res2)
                        curvesChanged = true;
                }
        }

        if ((in.size() > 0) && (scIn.size() > 0))
        {
            BLUtils::StereoToMono(&signal0Spect,
                                  signal0SpectSt[0], signal0SpectSt[1]);
            BLUtils::StereoToMono(&signal1Spect,
                                  signal1SpectSt[0], signal1SpectSt[1]);
            BLUtils::StereoToMono(&diffSpect,
                                  diffSpectSt[0], diffSpectSt[1]);
        }
    }

    // Update curves
    if (curvesChanged)
        UpdateCurves(signal0Spect, signal1Spect, diffSpect);
    
#if !NO_SOUND_OUTPUT
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
#endif
  
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
  
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
SpectralDiff::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);

    // GUIResize
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);
     
    int newGraphWidth = mGraphWidthSmall + mGUIOffsetX;
    int newGraphHeight = mGraphHeightSmall + mGUIOffsetY;
    mGraph->Resize(newGraphWidth, newGraphHeight);

    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);

    // Separator
    IColor sepIColor;
    mGUIHelper->GetGraphSeparatorColor(&sepIColor);
    int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurves();
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeSmallX, kGUISizeSmallY + offset,
                                      BUTTON_RESIZE_SMALL_FN,
                                      kGUISizeSmall,
                                      "", 0,
                                      tooltipGUISizeSmall);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeMediumX, kGUISizeMediumY + offset,
                                      BUTTON_RESIZE_MEDIUM_FN,
                                      kGUISizeMedium,
                                      "", 1,
                                      tooltipGUISizeMedium);
    
    mGUISizeBigButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeBigX, kGUISizeBigY + offset,
                                      BUTTON_RESIZE_BIG_FN,
                                      kGUISizeBig,
                                      "", 2,
                                      tooltipGUISizeBig);
    
    
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
SpectralDiff::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
SpectralDiff::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    for (int i = 0; i < 2; i++)
    {
        if (mSpectralDiffObjs[i] != NULL)
            mSpectralDiffObjs[i]->Reset(BUFFER_SIZE, OVERSAMPLING,
                                        FREQ_RES, sampleRate);
    }
    
    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    if (mSignal0Curve != NULL)
        mSignal0Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mSignal1Curve != NULL)
        mSignal1Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mDiffCurve != NULL)
        mDiffCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);

    if (mSignal0CurveSmooth != NULL)
        mSignal0CurveSmooth->Reset(sampleRate);
    if (mSignal1CurveSmooth != NULL)
        mSignal1CurveSmooth->Reset(sampleRate);
    if (mDiffCurveSmooth != NULL)
        mDiffCurveSmooth->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SpectralDiff::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;

    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kGUISizeSmall:
        {
            int activated = GetParam(paramIdx)->Int();
            if (activated)
            {
                if (mGUISizeIdx != 0)
                {
                    mGUISizeIdx = 0;

                    GUIResizeParamChange(mGUISizeIdx);
                    ApplyGUIResize(mGUISizeIdx);
                }
            }
        }
        break;
            
        case kGUISizeMedium:
        {
            int activated = GetParam(paramIdx)->Int();
            if (activated)
            {
                if (mGUISizeIdx != 1)
                {
                    mGUISizeIdx = 1;

                    GUIResizeParamChange(mGUISizeIdx);
                    ApplyGUIResize(mGUISizeIdx);
                }
            }
        }
        break;
            
        case kGUISizeBig:
        {
            int activated = GetParam(paramIdx)->Int();
            if (activated)
            {
                if (mGUISizeIdx != 2)
                {
                    mGUISizeIdx = 2;

                    GUIResizeParamChange(mGUISizeIdx);
                    ApplyGUIResize(mGUISizeIdx);
                }
            }
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectralDiff::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

#if APP_API
    RunAppDiff();
#endif
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectralDiff::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    mGraph = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;

    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectralDiff::CreateGraphAxes()
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
SpectralDiff::CreateGraphCurves()
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
        
        mSignal0Curve = new GraphCurve5(GRAPH_NUM_POINTS);
        mSignal0Curve->SetDescription("signal", descrColor);
        mSignal0Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mSignal0Curve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mSignal0Curve->SetFill(true);
        mSignal0Curve->SetFillAlpha(fillAlpha);
        mSignal0Curve->SetColor(signal0Color[0], signal0Color[1], signal0Color[2]);
        mSignal0Curve->SetLineWidth(2.0);
        
        mSignal0CurveSmooth = new SmoothCurveDB(mSignal0Curve,
                                                //CURVE_SMOOTH_COEFF,
                                                curveSmoothCoeff,
                                                GRAPH_NUM_POINTS,
                                                GRAPH_MIN_DB,
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
    
        // Signal1 curve
        int signal1Color[4];
        //mGUIHelper->GetGraphCurveColorLightBlue(signal1Color);
        mGUIHelper->GetGraphCurveColorFakeCyan(signal1Color);
        
        mSignal1Curve = new GraphCurve5(GRAPH_NUM_POINTS);
        mSignal1Curve->SetDescription("sc signal", descrColor);
        mSignal1Curve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mSignal1Curve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        //mSignal1Curve->SetFill(true);
        mSignal1Curve->SetFillAlpha(fillAlpha);
        mSignal1Curve->SetColor(signal1Color[0], signal1Color[1], signal1Color[2]);
        
        mSignal1CurveSmooth = new SmoothCurveDB(mSignal1Curve,
                                                //CURVE_SMOOTH_COEFF,
                                                curveSmoothCoeff,
                                                GRAPH_NUM_POINTS,
                                                GRAPH_MIN_DB,
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
    
        // Diff curve
        int diffColor[4];
        mGUIHelper->GetGraphCurveColorGreen(diffColor);
        
        mDiffCurve = new GraphCurve5(GRAPH_NUM_POINTS);
        mDiffCurve->SetDescription("diff", descrColor);
        mDiffCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mDiffCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        
        mDiffCurve->SetFillAlpha(fillAlpha);
        mDiffCurve->SetColor(diffColor[0], diffColor[1], diffColor[2]);
        
        mDiffCurveSmooth = new SmoothCurveDB(mDiffCurve,
                                             //CURVE_SMOOTH_COEFF,
                                             curveSmoothCoeff,
                                             GRAPH_NUM_POINTS,
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

    // Set diff curve at the end
    mDiffCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mDiffCurve);
}

void
SpectralDiff::ComputeDiffDB(WDL_TypedBuf<BL_FLOAT> *result,
                            const WDL_TypedBuf<BL_FLOAT> &signal0,
                            const WDL_TypedBuf<BL_FLOAT> &signal1)
{
    result->Resize(signal0.GetSize());
  
    for (int i = 0; i < result->GetSize(); i++)
    {
        BL_FLOAT sig0 = signal0.Get()[i];
        BL_FLOAT sig1 = signal1.Get()[i];
    
        BL_FLOAT sig0DB = BLUtils::AmpToDBClip(sig0,
                                               (BL_FLOAT)GRAPH_EPS_DB,
                                               (BL_FLOAT)GRAPH_MIN_DB);
        BL_FLOAT sig1DB = BLUtils::AmpToDBClip(sig1,
                                               (BL_FLOAT)GRAPH_EPS_DB,
                                               (BL_FLOAT)GRAPH_MIN_DB);
    
        // Compute the diff (can be negative)
        //BL_FLOAT diff = sig1DB - sig0DB;
        BL_FLOAT diff = sig0DB - sig1DB;
    
        //diff = fabs(diff);
    
        // Take the half (to stay in bounds in case of differencing -120 and 0)
        // And offset to the middle of the graph
        BL_FLOAT diffDisplay = diff/2.0 + MIDDLE_DB_GRAPH;
    
        diffDisplay = BLUtils::DBToAmp(diffDisplay);
        
        result->Get()[i] = diffDisplay;
    }
}

void
SpectralDiff::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT },
        { PLUG_WIDTH_MEDIUM, PLUG_HEIGHT_MEDIUM },
        { PLUG_WIDTH_BIG, PLUG_HEIGHT_BIG }
    };
    
    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
SpectralDiff::PreResizeGUI(int guiSizeIdx,
                           int *outNewGUIWidth, int *outNewGUIHeight)
{
    IGraphics *pGraphics = GetUI();
    if (pGraphics == NULL)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    GetNewGUISize(guiSizeIdx, outNewGUIWidth, outNewGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            *outNewGUIWidth, *outNewGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);
    
    mControlsCreated = false;

    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectralDiff::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium, kGUISizeBig };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
        { mGUISizeSmallButton,
          mGUISizeMediumButton,
          mGUISizeBigButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

void
SpectralDiff::UpdateCurves(const WDL_TypedBuf<BL_FLOAT> &signal0,
                           const WDL_TypedBuf<BL_FLOAT> &signal1,
                           const WDL_TypedBuf<BL_FLOAT> &diff)
{
    if (!mUIOpened)
        return;

    mSignal0CurveSmooth->SetValues(signal0);
    mSignal1CurveSmooth->SetValues(signal1);

#if DISPLAY_FFT_DIFF
    mDiffCurveSmooth->SetValues(diff);
#else
    WDL_TypedBuf<BL_FLOAT> &diff2 = mTmpBuf11;
    ComputeDiffDB(&diff2, signal0, signal1);
    mDiffCurveSmooth->SetValues(diff2);
#endif

    // Lock free
    mSignal0CurveSmooth->PushData();
    mSignal1CurveSmooth->PushData();
    mDiffCurveSmooth->PushData();
}

// NOTE: OnIdle() is called from the GUI thread
void
SpectralDiff::OnIdle()
{
    if (!mUIOpened)
        return;
    
    ENTER_PARAMS_MUTEX;

    mSignal0CurveSmooth->PullData();
    mSignal1CurveSmooth->PullData();
    mDiffCurveSmooth->PullData();
    
    LEAVE_PARAMS_MUTEX;

    // We don't need mutex anymore now
    mSignal0CurveSmooth->ApplyData();
    mSignal1CurveSmooth->ApplyData();
    mDiffCurveSmooth->ApplyData();
}

#if APP_API
void
SpectralDiff::RunAppDiff()
{
    CheckAppStartupArgs();

    BL_FLOAT diff = ComputeDiff();
    PrintDiff(diff);
    
    CloseWindow();
    exit(0);
}
#endif

#if APP_API
void
SpectralDiff::CheckAppStartupArgs()
{
    int argc = 0;
    char **argv = NULL;
    GetStartupArgs(&argc, &argv);

    if (argc < 2)
        PrintUsage(argv[0]);

    int argNum = 1;
    if ((strcmp(argv[argNum], "-h") == 0) ||
        (strcmp(argv[argNum], "--help") == 0))
        PrintUsage(argv[0]);

    if ((strcmp(argv[argNum], "-b") == 0) ||
        (strcmp(argv[argNum], "--brief-display") == 0))
    {
        mBriefDislay = true;
        argNum++;
    }

    if (argNum >= argc)
        PrintUsage(argv[0]);

    strcpy(mFileName0, argv[argNum]);

    argNum++;
    if (argNum >= argc)
        PrintUsage(argv[0]);

    strcpy(mFileName1, argv[argNum]);
}

void
SpectralDiff::PrintUsage(const char *cmdName)
{
#ifndef WIN32 // WIN32 does not have basename
    fprintf(stderr,
            "Usage: %s [options] file0.wav file1.wav\n", basename((char *)cmdName));
#else
    fprintf(stderr,
        "Usage: %s [options] file0.wav file1.wav\n", cmdName);
#endif

    fprintf(stderr, "File extensions can be .wav, .aiff|.aif, .flac\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-h, --help : display this help\n");
    fprintf(stderr, "-b, --brief-display : display only the diff value\n");

    CloseWindow();
    exit(0);
}

BL_FLOAT
SpectralDiff::ComputeDiff()
{
    WDL_TypedBuf<BL_FLOAT> samples0;
    bool res0 = OpenFile(mFileName0, &samples0);
    if (!res0)
    {
        fprintf(stderr, "Cannot open file: %s\n", mFileName0);
        CloseWindow();
        exit(0);
    }

    WDL_TypedBuf<BL_FLOAT> samples1;
    bool res1 = OpenFile(mFileName1, &samples1);
    if (!res1)
    {
        fprintf(stderr, "Cannot open file: %s\n", mFileName1);
        CloseWindow();
        exit(0);
    }

    if (samples0.GetSize() != samples1.GetSize())
    {
        fprintf(stderr, "Files have different lengths or different sample rates\n");
        
        CloseWindow();
        exit(0);
    }

    //
    WDL_TypedBuf<BL_FLOAT> signal0Spect;
    WDL_TypedBuf<BL_FLOAT> signal1Spect;
    WDL_TypedBuf<BL_FLOAT> diffSpect;

    signal0Spect.Resize(BUFFER_SIZE/2);
    BLUtils::FillAllZero(&signal0Spect);
    
    signal1Spect.Resize(BUFFER_SIZE/2);
    BLUtils::FillAllZero(&signal1Spect);
    
    diffSpect.Resize(BUFFER_SIZE/2);
    BLUtils::FillAllZero(&diffSpect);
        
    //
    BL_FLOAT sumDiffs = 0.0;
    int numDiffs = 0;
    
    long pos = 0;
    while(pos < samples0.GetSize() - BUFFER_SIZE)
    {
        vector<WDL_TypedBuf<BL_FLOAT> > in;
        in.resize(1);
        in[0].Add(&samples0.Get()[pos], BUFFER_SIZE);

        vector<WDL_TypedBuf<BL_FLOAT> > scIn;
        scIn.resize(1);
        scIn[0].Add(&samples1.Get()[pos], BUFFER_SIZE);
        
        mFftObj->Process(in, scIn, NULL);

        bool res0 = mSpectralDiffObjs[0]->GetSignal0BufferSpect(&signal0Spect);
        bool res1 = mSpectralDiffObjs[0]->GetSignal1BufferSpect(&signal1Spect);
        bool res2 = mSpectralDiffObjs[0]->GetDiffSpect(&diffSpect);

        // Seems better for Rebalance
#if 1 //0 // Diff in amp
        if (res2)
        {
            //sumDiffs += BLUtils::ComputeAvg(diffSpect);
            sumDiffs += BLUtils::ComputeSum(diffSpect);
            numDiffs++;
        }
#endif

#if 0 //1 // Diff in dB
        if (res0)
        {
            mScale->ApplyScaleForEach(Scale::DB, &signal0Spect, -120.0, 0.0);
            mScale->ApplyScaleForEach(Scale::DB, &signal1Spect, -120.0, 0.0);

            WDL_TypedBuf<BL_FLOAT> diff;
            BLUtils::ComputeDiff(&diff, signal1Spect, signal0Spect);
            BLUtils::ComputeAbs(&diff);
            
            sumDiffs += BLUtils::ComputeSum(diff)/(BUFFER_SIZE*0.5);
            numDiffs++;
        }
#endif
        
        pos += BUFFER_SIZE;
    }

    BL_FLOAT diff = 0.0;
    if (numDiffs > 0)
        diff = sumDiffs/numDiffs;
    
    return diff;
}

void
SpectralDiff::PrintDiff(BL_FLOAT diff)
{
    if (!mBriefDislay)
        fprintf(stdout, "Spectral difference: %.2f%%\n", diff*100.0);
    else
        fprintf(stdout, "%.3f\n", diff*100.0);
}

bool
SpectralDiff::OpenFile(const char *fileName, WDL_TypedBuf<BL_FLOAT> *samples)
{
    vector<WDL_TypedBuf<BL_FLOAT> > samples0;
    
    AudioFile *audioFile = AudioFile::Load(fileName, &samples0);
    if (audioFile == NULL)
        return false;

    BLUtils::StereoToMono(samples, samples0);
    
    delete audioFile;

    return true;
}

#endif
