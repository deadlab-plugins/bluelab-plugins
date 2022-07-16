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

#include <OversampProcessObj.h>
#include <Oversampler3.h>

#include <SaturateOverObj.h>

#include <ParamSmoother2.h>

#include <GraphControl12.h>
#include <GraphAxis2.h>

#include <GraphCurve5.h>

#include <Bufferizer.h>

#include "Saturate.h"

#include "IPlug_include_in_plug_src.h"


#define OVERSAMPLING 4

#define FILTER_NYQUIST 1

#define USE_OUT_GAIN 1

#define NUM_CURVES 1
#define CURVE_NUM_POINTS 512

#define GRAPH_WAVEFORM_CURVE 0

// Not used (but this class could be useful)
#define USE_BUFFERIZER 1 //0
#define BUFFERIZER_SIZE 512

#define CURVE_FILL_ALPHA 0.0

#define BEVEL_CURVES 1


#if 0
If possible, improve oversampling + filtering with Oversampler3 (Nyquist filter sinc convol)
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipRatio = "Ratio - Saturation ratio";
static char *tooltipOutGain = "Out Gain - Output gain";

enum EParams
{
    kRatio = 0,
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
    
    kRatioX = 194,
    kRatioY = 187,
  
    kOutGainX = 331,
    kOutGainY = 225
};

//
Saturate::Saturate(const InstanceInfo &info)
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

Saturate::~Saturate()
{
    for (int i = 0; i < 2; i++)
    {
        if (mOversampObjs[i] != NULL)
            delete mOversampObjs[i];
    }

    if (mRatioSmoother != NULL)
        delete mRatioSmoother;
  
#if USE_OUT_GAIN
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
#endif
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    //
#if USE_BUFFERIZER
    if (mGraphBufferizer != NULL)
        delete mGraphBufferizer;
#endif

    if (mVAxis != NULL)
        delete mVAxis;
    
    if (mSaturateCurve != NULL)
        delete mSaturateCurve;    
}

IGraphics *
Saturate::MyMakeGraphics()
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
Saturate::MyMakeLayout(IGraphics *pGraphics)
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
Saturate::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    mRatioSmoother = NULL;
    mOutGainSmoother = NULL;

    for (int i = 0; i < 2; i++)
        mOversampObjs[i] = NULL;
    
    mGraph = NULL;
    mGUIHelper = NULL;

    mVAxis = NULL;
    mSaturateCurve = NULL;

    mGraphBufferizer = NULL;
}

void
Saturate::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT defaultRatio = 0.0;
    
    BL_FLOAT sampleRate = GetSampleRate();
    mRatioSmoother = new ParamSmoother2(sampleRate, defaultRatio);

#if USE_OUT_GAIN
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
#endif
    
    mOversampObjs[0] = new SaturateOverObj(OVERSAMPLING, sampleRate);
    mOversampObjs[1] = new SaturateOverObj(OVERSAMPLING, sampleRate);

#if USE_BUFFERIZER
    mGraphBufferizer = new Bufferizer(BUFFERIZER_SIZE);
#endif

    ApplyParams();
    
    //
    mIsInitialized = true;
}

void
Saturate::InitParams()
{
    BL_FLOAT defaultRatio = 0.0;
    mRatio = defaultRatio;
    GetParam(kRatio)->InitDouble("SatRatio", defaultRatio, 0., 99.9, 0.1, "%",
                                 0, "", IParam::ShapePowCurve(0.5));

#if USE_OUT_GAIN
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -24.0, 24.0, 0.1, "dB");
#endif
}

void
Saturate::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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

    // Signal processing
    for (int i = 0; i < nFrames; i++)
    {
        // Parameters update
        BL_FLOAT ratio = mRatioSmoother->Process();
    
        for (int j = 0; j < 2; j++)
            mOversampObjs[j]->SetRatio(ratio);
    }

    mOversampObjs[0]->Process(in[0].Get(), out[0].Get(), nFrames);
  
    if ((in.size() > 1) && (out.size() > 1))
        mOversampObjs[1]->Process(in[1].Get(), out[1].Get(), nFrames);

    // Apply output gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
    // Copy output      
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

    // Graph
    WDL_TypedBuf<BL_FLOAT> &resOutput = mTmpBuf3;
    BLUtils::StereoToMono(&resOutput, out);
  
#if USE_BUFFERIZER
    // Graph bufferizer

    // Bufferize, so we are sure to have the same waveform whatever the block size
    // (otherwise, with block size e.g 32, curve would contain very few data)
    if (mGraphBufferizer != NULL)
    {
        mGraphBufferizer->AddValues(resOutput);
        
        // Manage block size > 512 (otherwise there would be lag bug if bs e.g 2048)
        while(true)
        {
            WDL_TypedBuf<BL_FLOAT> &resOutBuf = mTmpBuf4;
            bool bufSuccess = mGraphBufferizer->GetBuffer(&resOutBuf);
            if (bufSuccess)
                UpdateCurve(resOutBuf);

            if (!bufSuccess)
                break;
        }
    }   
#else
    UpdateCurve(resOutput);
#endif

    
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
Saturate::CreateControls(IGraphics *pGraphics)
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
    CreateGraphCurve();
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kRatioX, kRatioY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kRatio,
                              TEXTFIELD_FN,
                              "RATIO",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipRatio);

    // Out Gain
#if USE_OUT_GAIN
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
Saturate::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Saturate::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();

    if (mRatioSmoother != NULL)
        mRatioSmoother->Reset(sampleRate);
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    
    for (int i = 0; i < 2; i++)
        mOversampObjs[i]->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Saturate::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRatio:
        {
            BL_FLOAT ratio = GetParam(kRatio)->Value();
            ratio = ratio / 100.0;
            mRatio = ratio;

            if (mRatioSmoother != NULL)
                mRatioSmoother->SetTargetValue(ratio);
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
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Saturate::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    LEAVE_PARAMS_MUTEX;
}

void
Saturate::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    mGraph = NULL;
    
    LEAVE_PARAMS_MUTEX;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Saturate::ApplyParams()
{
    if (mRatioSmoother != NULL)
        mRatioSmoother->ResetToTargetValue(mRatio);
        
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
}

// Exact code copy from LoFi 
void
Saturate::CreateGraphAxis()
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
        { "-1.0", "-100%" },
        { "-0.5", "-50%" },
        { "0.0",  " 0%" },
        { "0.5",  " 50%" },
        { "1.0",  " 100%" }
    };

    mVAxis->SetMinMaxValues(-1.0, 1.0);
    mVAxis->SetData(SIG_AXIS_DATA, NUM_SIG_AXIS_DATA);
}

// Exact code copy from LoFi 
void
Saturate::CreateGraphCurve()
{
    if (mSaturateCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int loFiCurveColor[4];
        mGUIHelper->GetGraphCurveColorLightRed(loFiCurveColor);
        
        mSaturateCurve = new GraphCurve5(CURVE_NUM_POINTS);
        mSaturateCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mSaturateCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mSaturateCurve->SetColor(loFiCurveColor[0],
                             loFiCurveColor[1],
                             loFiCurveColor[2]);
        mSaturateCurve->SetLineWidth(2.0);
      
#if BEVEL_CURVES
        mSaturateCurve->SetBevel(true);
#endif
    }
    
    if (mGraph == NULL)
        return;
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mSaturateCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSaturateCurve);
}

// Exact code copy from LoFi 
void
Saturate::UpdateCurve(const WDL_TypedBuf<BL_FLOAT> &buf)
{
    if (!mUIOpened)
        return;
    
    mSaturateCurve->SetValues5(buf);
}
