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

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphControl12.h>
#include <GraphAxis2.h>

#include <GraphCurve5.h>

#include <ParamSmoother2.h>

#include <Bufferizer.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "LoFi.h"

#include "IPlug_include_in_plug_src.h"

#define MAX_BIT_DEPTH 16.0

#define NUM_CURVES 1
#define CURVE_NUM_POINTS 512

#define GRAPH_WAVEFORM_CURVE 0

// Not used (but this class could be useful)
#define USE_BUFFERIZER 1 //0
#define BUFFERIZER_SIZE 512

#define CURVE_FILL_ALPHA 0.0

#define BEVEL_CURVES 1


#if 0
NOTE: FLStudio 20, Sierra, AU or VST => the curve in the GUI is animated too slowly (this is not due to block size...)

DEMO: "8-bit effect": in gain in the middle, slide between 2 and 4 bits => that "sparkles"
#endif 

static char *tooltipHelp = "Help - Display Help";
static char *tooltipDepth = "Bit Depth - How many bits to keep";
static char *tooltipInGain = "In Gain - Input gain";
static char *tooltipOutGain = "Out Gain - Output gain";

enum EParams
{
    kInGain = 0,
    kDepth,
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

    kKnobWidth = 72,
    kKnobHeight = 72,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,
    
    kGraphX = 0,
    kGraphY = 0,
    
    kDepthX = 194,
    kDepthY = 189,
  
    kInGainX = 91,
    kInGainY = 224,
    
    kOutGainX = 333,
    kOutGainY = 224
};


//
LoFi::LoFi(const InstanceInfo &info)
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
    
    BL_PROFILE_RESET;
}

LoFi::~LoFi()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mDepthSmoother != NULL)
        delete mDepthSmoother;

    if (mInGainSmoother != NULL)
        delete mInGainSmoother;

    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
  
#if USE_BUFFERIZER
    if (mGraphBufferizer != NULL)
        delete mGraphBufferizer;
#endif

    if (mVAxis != NULL)
        delete mVAxis;
    
    if (mLoFiCurve != NULL)
        delete mLoFiCurve;
}

IGraphics *
LoFi::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
LoFi::MyMakeLayout(IGraphics *pGraphics)
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
LoFi::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    mGraph = NULL;
    mGUIHelper = NULL;
    
    mVAxis = NULL;
    mLoFiCurve = NULL;

    mDepthSmoother = NULL;
    mInGainSmoother = NULL;
    mOutGainSmoother = NULL;

    mGraphBufferizer = NULL;
}

void
LoFi::InitParams()
{
    BL_FLOAT defaultDepth = MAX_BIT_DEPTH;
    mDepth = defaultDepth;
    GetParam(kDepth)->InitDouble("Depth", defaultDepth, 1.0,
                                 MAX_BIT_DEPTH, 0.1, "bit",
                                 0, "", IParam::ShapePowCurve(3.0));

    BL_FLOAT defaultInGain = 0.0;
    mInGain = 1.0; // 0dB
    GetParam(kInGain)->InitDouble("InGain", defaultInGain, -12.0, 12.0, 0.1, "dB");

    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");
}

void
LoFi::ApplyParams()
{
    if (mDepthSmoother != NULL)
        mDepthSmoother->SetTargetValue(mDepth);

    if (mInGainSmoother != NULL)
        mInGainSmoother->SetTargetValue(mInGain);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->SetTargetValue(mOutGain);
}

void
LoFi::Init()
{
    if (mIsInitialized)
        return;

    BL_FLOAT defaultDepth = MAX_BIT_DEPTH;

    BL_FLOAT sampleRate = GetSampleRate();
    mDepthSmoother = new ParamSmoother2(sampleRate, defaultDepth);

    // Gain smoother
    BL_FLOAT defaultInGain = 1.0; // 0dB
    mInGainSmoother = new ParamSmoother2(sampleRate, defaultInGain);
    
    BL_FLOAT defaultOutGain = 1.0; // 0dB
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
  
#if USE_BUFFERIZER
    mGraphBufferizer = new Bufferizer(BUFFERIZER_SIZE);
#endif

    ApplyParams();
    
    mIsInitialized = true;
}

void
LoFi::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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

    // Apply input gain
    BLUtilsPlug::ApplyGain(in, &in, mInGainSmoother);
      
    // Signal processing
    for (int i = 0; i < nFrames; i++)
    {
        if (mDepthSmoother == NULL)
            continue;
      
        // Parameters update
        BL_FLOAT depth = mDepthSmoother->Process();
          
        // If depth is at the maximum, do nothing, to not alter the signal
        if (depth < MAX_BIT_DEPTH)
        {
            long long int depthI = std::pow((BL_FLOAT)2.0, depth);
              
            // Must divide by 2 to have the right number of steps in the waveform
#define COEFF 0.5
              
            BL_FLOAT leftSample = in[0].Get()[i];
            long long int leftSampleI = ((leftSample + 1.0)*0.5*COEFF)*depthI;
              
            leftSample = ((BL_FLOAT)leftSampleI)/(COEFF*depthI);
            leftSample = leftSample*2.0 - 1.0;
              
            out[0].Get()[i] = leftSample;
              
            if ((in.size() > 1) && (out.size() > 1))
            {
                BL_FLOAT rightSample = in[1].Get()[i];
                long long int rightSampleI = ((rightSample + 1.0)*0.5*COEFF)*depthI;
                
                rightSample = ((BL_FLOAT)rightSampleI)/(COEFF*depthI);
                rightSample = rightSample*2.0 - 1.0;
                
                out[1].Get()[i] = rightSample;
            }
        }
        else
        {
            out[0].Get()[i] = in[0].Get()[i];
            
            if ((in.size() > 1) && (out.size() > 1))
                out[1].Get()[i] = in[1].Get()[i];
        }
    }
      
    // Apply output gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);

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
LoFi::CreateControls(IGraphics *pGraphics)
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

    // In gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kInGainX, kInGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kInGain,
                              TEXTFIELD_FN,
                              "IN GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipInGain);
    
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

    // Depth
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kDepthX, kDepthY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kDepth,
                              TEXTFIELD_FN,
                              "DEPTH",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipDepth);
    
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
LoFi::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
LoFi::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();

    if (mDepthSmoother != NULL)
        mDepthSmoother->Reset(sampleRate);
    if (mInGainSmoother != NULL)
        mInGainSmoother->Reset(sampleRate);
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
LoFi::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kDepth:
        {
            BL_FLOAT depth = GetParam(kDepth)->Value();
            mDepth = depth;

            if (mDepthSmoother != NULL)
                mDepthSmoother->SetTargetValue(depth);
        }
        break;
      
        case kInGain:
        {
            BL_FLOAT gain = GetParam(kInGain)->DBToAmp();
            mInGain = gain;

            if (mInGainSmoother != NULL)
                mInGainSmoother->SetTargetValue(gain);
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
      
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
LoFi::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
LoFi::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    mGraph = NULL;
    
    LEAVE_PARAMS_MUTEX;
}

void
LoFi::CreateGraphAxis()
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

void
LoFi::CreateGraphCurve()
{
    if (mLoFiCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int loFiCurveColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(loFiCurveColor);
        
        mLoFiCurve = new GraphCurve5(CURVE_NUM_POINTS);
        mLoFiCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mLoFiCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mLoFiCurve->SetColor(loFiCurveColor[0],
                             loFiCurveColor[1],
                             loFiCurveColor[2]);
        mLoFiCurve->SetLineWidth(2.0);
      
#if BEVEL_CURVES
        mLoFiCurve->SetBevel(true);
#endif
    }
    
    if (mGraph == NULL)
        return;
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mLoFiCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mLoFiCurve);
}

void
LoFi::UpdateCurve(const WDL_TypedBuf<BL_FLOAT> &buf)
{
    if (!mUIOpened)
        return;
    
    mLoFiCurve->SetValues5(buf);
}
