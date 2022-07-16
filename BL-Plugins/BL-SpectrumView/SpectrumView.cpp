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
#include <SpectrumViewFftObj.h>

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

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "SpectrumView.h"

#include "IPlug_include_in_plug_src.h"

// Better precision with high buffer size
// (exspecially in low frequencies)
#define BUFFER_SIZE 2048 //16384 //8192 //4096 //2048

//
#define OVERSAMPLING 4 // 2

#define FREQ_RES 1

#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

// Graph
#define NUM_CURVES 2

// Curves
#define GRAPH_VAXIS_CURVE 0
#define GRAPH_SIGNAL_CURVE 1

#define CURVE_FILL_ALPHA 0.2

#define GRAPH_NUM_POINTS 256
#define GRAPH_DEC_FACTOR 0.25 //0.03125

// Use log before decimate => more accurate for low frequencies
#define HIGH_RES_LOG_CURVES 1

#define Y_LOG_SCALE_FACTOR 3.5

#define MIN_LEVEL_DB -60.0

#define GRAPH_MIN_DB -119.0 //-120.0
#define GRAPH_MAX_DB 10.0 //0.0
#define GRAPH_EPS_DB 1e-15
#define MIN_DB2 -200.0

#define CURVE_SMOOTH_COEFF 0.5

// GUI Size
#define PLUG_WIDTH_MEDIUM 800
#define PLUG_HEIGHT_MEDIUM 560

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 708

// GUI Size
#define NUM_GUI_SIZES 3

// Since the GUI is very full, reduce the offset after
// the radiobuttons title text
//#define RADIOBUTTONS_TITLE_OFFSET -5


// Ableton, Windows
// - play
// - change to medium GUI
// - change to small GUI
// => after a little while, the medium GUI button is selected again automatically,
// and the GUI starts to resize, then it freezes
//
// FIX: use SetValueFromUserInput() to avoid a call by the host to VSTSetParameter()
// NOTE: may still freeze some rare times
#define FIX_ABLETON_RESIZE_GUI_FREEZE 1

// Optimization
#define OPTIM_SKIP_IFFT 1

#if 0
TODO: denomal number problem => must fix!
Steps: play a sound, stop it => when processing silence it comsumes more CPU.

TODO/OPTIM: make a full float version, to see if it optimizes

TODO: check installer standalone (win and mac)
TODO: check well for all hosts
(because we use SetParameterFromPlug, when toggle off freeze when restart play)
SITE: say that stadalone version can be used to calibrate a
"facade" during a concert
#endif

enum EParams
{
    kGraph = 0,
    kFreeze,
    kPrecision,
    
    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    
    kNumParams
};
 
const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kGraphX = 0,
    kGraphY = 0,

    kFreezeX = 103,
    kFreezeY = 288,
    
    kRadioButtonPrecisionX = 250,
    kRadioButtonPrecisionY = 288,
    kRadioButtonPrecisionVSize = 68,
    kRadioButtonPrecisionNumButtons = 4,
    
    // GUI size
    kGUISizeSmallX = 20,
    kGUISizeSmallY = 271,
  
    kGUISizeMediumX = 20,
    kGUISizeMediumY = 298,
    
    kGUISizeBigX = 20,
    kGUISizeBigY = 327,
    
    kLogoAnimFrames = 31
};


//
SpectrumView::SpectrumView(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , ResizeGUIPluginInterface(this)
{
    TRACE;
    
    // By default, we can't resize a plugin GUI more than 2 x the original size
    // In SpectrumView particularly, we need to resize more,
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

SpectrumView::~SpectrumView()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mFftObj != NULL)
        delete mFftObj;

    if (mSpectrumViewObj != NULL)
        delete mSpectrumViewObj;
    
    //
    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mAmpAxis != NULL)
        delete mAmpAxis;

    if (mVAxis != NULL)
        delete mVAxis;
    
    //
    if (mSpectrumCurve != NULL)
        delete mSpectrumCurve;
    
    if (mSpectrumCurveSmooth != NULL)
        delete mSpectrumCurveSmooth;
}

IGraphics *
SpectrumView::MyMakeGraphics()
{
    int newGUIWidth;
    int newGUIHeight;
    GetNewGUISize(mGUISizeIdx, &newGUIWidth, &newGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            newGUIWidth, newGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);

    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       //PLUG_WIDTH, PLUG_HEIGHT,
                                       newGUIWidth, newGUIHeight,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
SpectrumView::MyMakeLayout(IGraphics *pGraphics)
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
    
    pGraphics->AttachBackground(BACKGROUND_FN);

#ifdef __linux__
    pGraphics->AttachTextEntryControl();
#endif
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);
    
    CreateControls(pGraphics, mGUIOffsetY);

    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;

    EndResizeGUI(this);
}

void
SpectrumView::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    //
    mFftObj = NULL;
    
    mSpectrumViewObj = NULL;
    
    mGUIHelper = NULL;
    
    //
    mGraph = NULL;
    
    mAmpAxis = NULL;
    mHAxis = NULL;
    
    mFreqAxis = NULL;
    mVAxis = NULL;
    
    //
    mSpectrumCurve = NULL;
    mSpectrumCurveSmooth = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;

    mGraphWidthSmall = 256;
    mGraphHeightSmall = 256;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;

    //
    mFreeze = false;
    mPrecision = 0;
    mBufferSize = BUFFER_SIZE;
    mPrecisionChanged = false;

    mFreezeControl = NULL;
}

void
SpectrumView::InitParams()
{
    int defaultFreeze = 0;
    mFreeze = defaultFreeze;
    GetParam(kFreeze)->InitInt("Freeze", defaultFreeze, 0, 1);

    // Precision
    int defaultPrecision = 0;
    mPrecision = defaultPrecision;
    GetParam(kPrecision)->InitInt("Precision", defaultPrecision, 0, 3);
  
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->InitInt("SmallGUI", 0, 0, 1, "", IParam::kFlagMeta);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    
    GetParam(kGUISizeMedium)->InitInt("MediumGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kGUISizeBig)->InitInt("BigGUI", 0, 0, 1, "", IParam::kFlagMeta);
}

void
SpectrumView::ApplyParams()
{
    PrecisionChanged();
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
SpectrumView::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> spectrumProcessObjs;
        mSpectrumViewObj = new SpectrumViewFftObj(mBufferSize, oversampling, freqRes);
        spectrumProcessObjs.push_back(mSpectrumViewObj);
        
        int numChannels = 1;
        int numScInputs = 0;
        
        mFftObj = new FftProcessObj16(spectrumProcessObjs,
                                      numChannels, numScInputs,
                                      mBufferSize, oversampling, freqRes,
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
        mFftObj->Reset(mBufferSize, oversampling, freqRes, sampleRate);
        
        mSpectrumViewObj->Reset(mBufferSize, oversampling, freqRes, sampleRate);
    }
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
SpectrumView::ProcessBlock(iplug::sample **inputs,
                           iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
    if (!mIsInitialized)
        return;

    if (mGraph != NULL)
        mGraph->Lock();

    if (mPrecisionChanged)
        OnReset();
    mPrecisionChanged = false;
  
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
        
        return;
    }
    
#if 0 //1 // Debug
    if (BLUtilsPlug::PlugIOAllZero(in, out))
    {
        if (mGraph != NULL)
            mGraph->Unlock();
        
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
    
    WDL_TypedBuf<BL_FLOAT> &inSignal = mTmpBuf3;
    inSignal = in[0];
    
    if (in.size() == 2)
    {
        BLUtils::StereoToMono(&inSignal, in[0], in[1]);
    }
      
    //if (IsPlaying())
    {
        vector<WDL_TypedBuf<BL_FLOAT> > &monoIn = mTmpBuf4;

        //monoIn.push_back(inSignal);
        
        monoIn.resize(1);
        monoIn[0] = inSignal;
     
        vector<WDL_TypedBuf<BL_FLOAT> > dummySc;
        vector<WDL_TypedBuf<BL_FLOAT> > &dummyOut = mTmpBuf5;
        dummyOut = monoIn;
        
        mFftObj->Process(monoIn, dummySc, &dummyOut);
        
        WDL_TypedBuf<BL_FLOAT> &fftSignal = mTmpBuf6;
        mSpectrumViewObj->GetSignalBuffer(&fftSignal);
        if (fftSignal.GetSize() > 0)
        {
            if (!mFreeze)
            {
                UpdateCurve(fftSignal);
            }
        }
    }

    // The plugin does not modify the sound, so bypass
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
    
    if (mGraph != NULL)
        mGraph->Unlock();
    
    BL_PROFILE_END;
}

void
SpectrumView::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);
    
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN, kGraph);

    // GUIResize
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);
     
    int newGraphWidth = mGraphWidthSmall + mGUIOffsetX;
    int newGraphHeight = mGraphHeightSmall + mGUIOffsetY;
    mGraph->Resize(newGraphWidth, newGraphHeight);

    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);
    int sepColor[4] = { 24, 24, 24, 255 };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurve();
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeSmallX, kGUISizeSmallY + offset,
                                      BUTTON_RESIZE_SMALL_FN,
                                      kGUISizeSmall, //
                                      "", 0);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeMediumX, kGUISizeMediumY + offset,
                                      BUTTON_RESIZE_MEDIUM_FN,
                                      kGUISizeMedium, //
                                      "", 1);
    
    mGUISizeBigButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeBigX, kGUISizeBigY + offset,
                                      BUTTON_RESIZE_BIG_FN,
                                      kGUISizeBig, //
                                      "", 2);

    const char *radioLabels[] = { "FAST 1", "2", "3", "BEST 4" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonPrecisionX,
                                   kRadioButtonPrecisionY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonPrecisionNumButtons,
                                   kRadioButtonPrecisionVSize,
                                   kPrecision,
                                   false,
                                   "PRECISION",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabels);

    mFreezeControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                    kFreezeX,
                                                    kFreezeY + offset,
                                                    CHECKBOX_FN, kFreeze, "FREEZE",
                                                    GUIHelper12::SIZE_SMALL);
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR); //, GUIHelper12::BOTTOM);
    
    // Logo
    mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
                               kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN, GUIHelper12::BOTTOM);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM);
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    //
    mControlsCreated = true;
}

void
SpectrumView::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    if (mSpectrumViewObj != NULL)
        mSpectrumViewObj->Reset(mBufferSize, OVERSAMPLING, 1, sampleRate);

    if (mFftObj != NULL)
        mFftObj->Reset(mBufferSize, OVERSAMPLING, 1, sampleRate);
    
    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    if (mSpectrumCurve != NULL)
        //mSpectrumCurve->SetXScale(Scale::LOG_FACTOR, 0.0, sampleRate*0.5); //
        mSpectrumCurve->SetXScale(Scale::MEL, 0.0, sampleRate*0.5); //

    mFreeze = false;
    SetParameterValue(kFreeze, 0.0);
    if (mFreezeControl != NULL)
    {
        mFreezeControl->SetValue(0.0);
        mFreezeControl->SetDirty(false);
    }
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SpectrumView::OnParamChange(int paramIdx)
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

                    StartResizeGUI(this);
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

                    StartResizeGUI(this);
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

                    StartResizeGUI(this);
                    GUIResizeParamChange(mGUISizeIdx);
                    ApplyGUIResize(mGUISizeIdx);
                }
            }
        }
        break;

        case kFreeze:
        {
            int value = GetParam(kFreeze)->Value();
            mFreeze = (value == 1);
        }
        break;
    
        case kPrecision:
        {
            int precision = GetParam(paramIdx)->Int();
            if (precision != mPrecision)
            {
                mPrecision = precision;
                
                PrecisionChanged();
            }
        }
        break;

        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectrumView::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectrumView::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    mGraph = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;

    mFreezeControl = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectrumView::CreateGraphAxes()
{
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2();
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
    mFreqAxis->Init(mHAxis, mGUIHelper, horizontal, BUFFER_SIZE, sampleRate, graphWidth);
    mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    mAmpAxis->Init(mVAxis, mGUIHelper, GRAPH_MIN_DB, GRAPH_MAX_DB, graphWidth);
}

void
SpectrumView::CreateGraphCurve()
{
    // TODO: styles !
    
    if (mSpectrumCurve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
    
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        // Spectrum curve
        int spectrumColor[4];
        mGUIHelper->GetGraphCurveColorBlue(spectrumColor);
        
        mSpectrumCurve = new GraphCurve5(GRAPH_NUM_POINTS);
        //mSpectrumCurve->SetDescription("signal", descrColor);
        //mSpectrumCurve->SetXScale(Scale::LOG_FACTOR, 0.0, sampleRate*0.5); //
        mSpectrumCurve->SetXScale(Scale::MEL, 0.0, sampleRate*0.5);
        mSpectrumCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mSpectrumCurve->SetFill(true);
        mSpectrumCurve->SetFillAlpha(fillAlpha);
        mSpectrumCurve->SetColor(spectrumColor[0], spectrumColor[1], spectrumColor[2]);
        mSpectrumCurve->SetLineWidth(2.0);
        
        mSpectrumCurveSmooth = new SmoothCurveDB(mSpectrumCurve,
                                                 CURVE_SMOOTH_COEFF,
                                                 GRAPH_NUM_POINTS,
                                                 GRAPH_MIN_DB,
                                                 GRAPH_MIN_DB, GRAPH_MAX_DB);
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mSpectrumCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSpectrumCurve);
}

void
SpectrumView::GetNewGUISize(int guiSizeIdx, int *width, int *height)
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
SpectrumView::PreResizeGUI(int guiSizeIdx,
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
SpectrumView::GUIResizeParamChange(int guiSizeIdx)
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
SpectrumView::UpdateCurve(const WDL_TypedBuf<BL_FLOAT> &fftSignal)
{
    if (!mUIOpened)
        return;
    
    mSpectrumCurveSmooth->SetValues(fftSignal);
}

void
SpectrumView::PrecisionChanged()
{
    mBufferSize = 2048;
    
    switch(mPrecision)
    {
        case 0:
            mBufferSize = 2048;
            break;
            
        case 1:
            mBufferSize = 4096;
            break;
            
        case 2:
            mBufferSize = 8192;
            break;
            
        case 3:
            mBufferSize =  16384;
            break;
            
        default:
            break;
    }
    
    mPrecisionChanged = true;
}
