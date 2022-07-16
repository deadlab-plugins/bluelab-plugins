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
#include <GraphControl12.h>
#include <SecureRestarter.h>

#include <GraphControl12.h>
#include <SpectrogramDisplay2.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <BLReverb.h>
#include <BLReverbSndF.h>

#include <MultiViewer.h>
#include <BLReverbViewer.h>

#include <MultiViewer2.h>

#include <MouseCustomControl.h>

#include "config.h"
#include "bl_config.h"

#include "Reverb.h"

#include "IPlug_include_in_plug_src.h"


// 1 second -> 20 seconds
#define VIEWER_DEFAULT_DURATION 1.0
#define VIEWER_MIN_DURATION     0.1
#define VIEWER_MAX_DURATION     20.0

#define MOUSE_ZOOM_COEFF 0.01 //0.1 //1.0

// Useful to show preset to keep
#define DBG_DUMP_PRESET 1 //0 //1

// Must be the same buffer size as in SamplesToSpectrogram
#define BUFFER_SIZE 2048

#define UPDATE_VIEW_ON_PARAM_CHANGE 1

// DEBUG
BL_FLOAT USE_REVERB_PRESET[] = { 1/**/, 0, -12.2222, -5.55556, 1.125, 1, 1, 10, 0.1, 0, 0, 18000,     50, 8784.1, 7624.39, 0.1, 0 };

#if 0
TODO: fade out IR in BLReverbIR

TODO: test BLReverbIR class and optimize using it

BUG: double-click doesn t work anymore to reset knob (sometimes)

NOTE: Bass LPF, is it really working ?

NOTE: set zoom as parameter, so it will be restored ?

NOTE: with very low time intervals (e.g 100ms), the spectorgram is not eactly
aligned to the IR (the IR is ok)

TODO: implement presets like in sndfilter
#endif

//struct {
//  int osf; double p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16;
//} _presets[] = {
BL_FLOAT _presets[][17] = {
  //OSF ERtoLt ERWet Dry ERFac ERWdth Wdth Wet Wander BassB Spin InpLP BasLP DmpLP OutLP RT60  Delay
  {1, 0.40, -9.0,-10, 1.6, 0.7, 1.0, -0, 0.27, 0.15, 0.7,17000, 500, 7000,10000, 3.2,0.020},
  {2, 0.30, -9.0, -8, 1.0, 0.7, 1.0, -8, 0.32, 0.25, 0.7,18000, 600, 9000,17000, 2.1,0.010},
  {1, 0.30, -9.0, -8, 1.0, 0.7, 1.0, -8, 0.27, 0.20, 0.5,18000, 600, 7000, 9000, 2.3,0.010},
  {2, 0.30, -9.0, -8, 1.2, 0.7, 1.0, -8, 0.27, 0.20, 0.7,18000, 500, 8000,16000, 2.8,0.010},
  {1, 0.30, -9.0, -8, 1.2, 0.7, 1.0, -8, 0.25, 0.15, 0.5,18000, 500, 6000, 8000, 2.9,0.010},
  {2, 0.20, -9.0, -8, 1.4, 0.7, 1.0, -8, 0.17, 0.20, 1.0,18000, 400, 9000,14000, 3.8,0.018},
  {2, 0.20, -9.0, -8, 1.5, 0.7, 1.0, -8, 0.20, 0.20, 0.5,18000, 400, 5000, 7000, 4.2,0.018},
  {2, 0.70, -8.0, -8, 0.7,-0.4, 0.8, -8, 0.20, 0.30, 1.6,18000,1000,18000,18000, 0.5,0.005},
  {3, 0.70, -8.0, -8, 0.8, 0.6, 0.9, -8, 0.30, 0.30, 0.4,18000, 300,10000,18000, 0.5,0.005},
  {2, 0.50, -8.0, -8, 1.2,-0.4, 0.8, -8, 0.20, 0.10, 1.6,18000,1000,18000,18000, 0.8,0.008},
  {2, 0.50, -8.0, -8, 1.2, 0.6, 0.9, -8, 0.30, 0.10, 0.4,18000, 300,10000,18000, 1.2,0.016},
  {2, 0.20, -8.0, -8, 2.2,-0.4, 0.9, -8, 0.20, 0.10, 1.6,18000,1000,16000,18000, 1.8,0.010},
  {2, 0.20, -8.0, -8, 2.2, 0.6, 0.9, -8, 0.30, 0.10, 0.4,18000, 500, 9000,18000, 1.9,0.020},
  {2, 0.50, -7.0, -7, 1.2,-0.4, 0.8,-70, 0.20, 0.10, 1.6,18000,1000,18000,18000, 0.8,0.008},
  {2, 0.50, -7.0, -7, 1.2, 0.6, 0.9,-70, 0.30, 0.10, 0.4,18000, 300,10000,18000, 1.2,0.016},
  {2, 0.00,-70.0,-20, 1.0, 1.0, 1.0, -8, 0.20, 0.10, 1.6,18000,1000,16000,18000, 1.8,0.000},
  {2, 0.00,-70.0,-20, 1.0, 1.0, 1.0, -8, 0.30, 0.20, 0.4,18000, 500, 9000,18000, 1.9,0.000},
  {2, 0.10,-16.0,-15, 1.0, 0.1, 1.0, -5, 0.35, 0.05, 1.0,18000, 100,10000,18000,12.0,0.000},
  {2, 0.10,-16.0,-15, 1.0, 0.1, 1.0, -5, 0.40, 0.05, 1.0,18000, 100, 9000,18000,30.0,0.000}  
};

//
class ReverbCustomMouseControl //: public MouseCustomControl
    : public GraphCustomControl
{
public:
    ReverbCustomMouseControl(Reverb *plug);
  
    virtual ~ReverbCustomMouseControl() {}
  
    virtual void OnMouseUp(float x, float y, const IMouseMod &pMod) override;
  
protected:
    Reverb *mPlug;
};

ReverbCustomMouseControl::ReverbCustomMouseControl(Reverb *plug)
{
    mPlug = plug;
}

void
ReverbCustomMouseControl::OnMouseUp(float x, float y, const IMouseMod &pMod)
{
    mPlug->OnMouseUp();
}

class ReverbCustomControl : public GraphCustomControl
{
public:
    ReverbCustomControl(Reverb *plug) { mPlug = plug; };
  
    virtual ~ReverbCustomControl() {}
  
    virtual void OnMouseDrag(float x, float y, float dX, float dY,
                             const IMouseMod &pMod) override;
  
protected:
    Reverb *mPlug;
};

void
ReverbCustomControl::OnMouseDrag(float x, float y, float dX, float dY,
                                 const IMouseMod &pMod)
{
    //int maxDelta = (abs(dX) > abs(dY)) ? dX : dY;
    int maxDelta = dY;
  
    mPlug->UpdateTimeZoom(maxDelta);
}

//

enum EParams
{
    kGraph = 0,

    // NOTE: for presets, parameters must not be switched
    kOversampFactor,
  
    // Early
    kEarlyAmount,
    kEarlyWet,
    kEarlyDry,
    kEarlyFactor,
    kEarlyWidth,
    
    // Reverb
    kRevWidth,
    kWet,
    kWander,
    kBassBoost,
    kSpin,
    kInputLPF,
    kBassLPF,
    kDampLPF,
    
    // Common
    kOutputLPF,
    kRT60,
    kDelay,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,
    
    kGraphX = 0,
    kGraphY = 0,
    
    //
    kOversampFactorX = 642,
    kOversampFactorY = 380,
    kOversampFactorFrames = 180,
  
    // Early
    kEarlyAmountX = 42,
    kEarlyAmountY = 380,
    kEarlyAmountFrames = 180,
  
    kEarlyWetX = 42,
    kEarlyWetY = 480,
    kEarlyWetFrames = 180,
    
    kEarlyDryX = 562,
    kEarlyDryY = 480,
    kEarlyDryFrames = 180,
    
    kEarlyFactorX = 122,
    kEarlyFactorY = 380,
    kEarlyFactorFrames = 180,
  
    kEarlyWidthX = 122,
    kEarlyWidthY = 480,
    kEarlyWidthFrames = 180,
  
    // Reverb
    kRevWidthX = 222,
    kRevWidthY = 380,
    kRevWidthFrames = 180,
  
    kWetX = 222,
    kWetY = 480,
    kWetFrames = 180,
  
    kWanderX = 302,
    kWanderY = 380,
    kWanderFrames = 180,
  
    kBassBoostX = 382,
    kBassBoostY = 380,
    kBassBoostFrames = 180,
  
    kSpinX = 462,
    kSpinY = 380,
    kSpinFrames = 180,
  
    kInputLPFX = 302,
    kInputLPFY = 480,
    kInputLPFFrames = 180,
  
    kBassLPFX = 382,
    kBassLPFY = 480,
    kBassLPFFrames = 180,
  
    kDampLPFX = 462,
    kDampLPFY = 480,
    kDampLPFFrames = 180,
  
    // Common
    kOutputLPFX = 722,
    kOutputLPFY = 380,
    kOutputLPFFrames = 180,
  
    kRT60X = 642,
    kRT60Y = 480,
    kRT60Frames = 180,
  
    kDelayX = 722,
    kDelayY = 480,
    kDelayFrames = 180,
    
    kLogoAnimFrames = 31
};

//
Reverb::Reverb(const InstanceInfo &info)
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

Reverb::~Reverb()
{
    if (mReverb != NULL)
        delete mReverb;

    if (mMultiViewer != NULL)
        delete mMultiViewer;

    if (mReverbViewer != NULL)
        delete mReverbViewer;

    if (mCustomControl != NULL)
        delete mCustomControl;

    if (mCustomMouseControl != NULL)
        delete mCustomMouseControl;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Reverb::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
Reverb::MyMakeLayout(IGraphics *pGraphics)
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
    
    CreateControls(pGraphics);
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
Reverb::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mGUIHelper = NULL;
    mGraph = NULL;
    mSpectrogramDisplay = NULL;
    mSpectrogramDisplayState = NULL;
    
    mReverb = NULL;
    mMultiViewer = NULL;
    mReverbViewer = NULL;
    mCustomControl = NULL;
    mCustomMouseControl = NULL;
    
    mReverb = NULL;

    mIsInitialized = false;
}

void
Reverb::InitParams()
{
    // Oversampling
    GetParam(kOversampFactor)->InitInt("Oversampling", 1, 1, 4, "");
  
    // Early
    mEarlyAmount = 0.0;
    GetParam(kEarlyAmount)->InitDouble("EarlyAmount", 0.0, 0.0, 100.0, 0.1, "%");
    mEarlyWet = -70.0;
    GetParam(kEarlyWet)->InitDouble("EarlyWet", -70.0, -70.0, 10.0, 0.1, "dB");
    mEarlyDry = -70.0;
    GetParam(kEarlyDry)->InitDouble("EarlyDry", -70.0, -70.0, 10.0, 0.1, "dB");
    mEarlyFactor = 0.5;
    GetParam(kEarlyFactor)->InitDouble("EarlyFactor", 0.5, 0.5, 2.5, 0.01, "");
    mEarlyWidth = -100.0;
    GetParam(kEarlyWidth)->InitDouble("EarlyWidth", -100.0, 0.0, 100.0, 0.1, "%");
  
    // Reverb
    mRevWidth = 0.0;
    GetParam(kRevWidth)->InitDouble("Width", 0.0, 0.0, 100.0, 0.1, "%");
    mWet = -70.0;
    GetParam(kWet)->InitDouble("Wet", -70.0, -70.0, 10.0, 0.1, "dB");
    mWander = 0.1;
    GetParam(kWander)->InitDouble("Wander", 0.1, 0.1, 0.6, 0.01, "");
    mBassBoost = 0.0;
    GetParam(kBassBoost)->InitDouble("BassBoost", 0.0, 0.0, 50.0, 0.1, "%");
    mSpin = 0.0;
    GetParam(kSpin)->InitDouble("Spin", 0.0, 0.0, 10.0, 0.01, "");
    mInputLPF = 200.0;
    GetParam(kInputLPF)->InitDouble("InputLPF", 200.0, 200.0, 18000.0, 0.1, "Hz",
                                    0, "", IParam::ShapePowCurve(2.0));
    mBassLPF = 50.0;
    GetParam(kBassLPF)->InitDouble("kBassLPF", 50.0, 50.0, 1050.0, 0.1, "Hz",
                                   0, "", IParam::ShapePowCurve(2.0));
    mDampLPF = 200.0;
    GetParam(kDampLPF)->InitDouble("DampLPF", 200.0, 200.0, 18000.0, 0.1, "Hz",
                                   0, "", IParam::ShapePowCurve(2.0));
  
    // Common
    mOutputLPF = 200.0;
    GetParam(kOutputLPF)->InitDouble("OutputLPF", 200.0, 200.0, 18000.0, 0.1, "Hz",
                                     0, "", IParam::ShapePowCurve(2.0));
    mRT60 = 0.1;
    GetParam(kRT60)->InitDouble("RT60", 0.1, 0.1, 20.0, 0.001, "s");
    mDelay = 0.0;
    GetParam(kDelay)->InitDouble("Delay", 0.0, -0.5, 0.5, 0.001, "s");
}

void
Reverb::ApplyParams()
{
    if (mReverb != NULL)
    {             
        mReverb->SetOversampFactor(mOversampFactor);
        mReverb->SetEarlyAmount(mEarlyAmount);
        mReverb->SetEarlyWet(mEarlyWet);
        mReverb->SetEarlyDry(mEarlyDry);
        mReverb->SetEarlyFactor(mEarlyFactor);
        mReverb->SetEarlyWidth(mEarlyWidth);
        mReverb->SetWidth(mRevWidth);
        mReverb->SetWet(mWet);
        mReverb->SetWander(mWander);
        mReverb->SetBassBoost(mBassBoost);
        mReverb->SetSpin(mSpin);
        mReverb->SetInputLPF(mInputLPF);
        mReverb->SetBassLPF(mBassLPF);
        mReverb->SetDampLPF(mDampLPF);
        mReverb->SetOutputLPF(mOutputLPF);
        mReverb->SetRT60(mRT60);
        mReverb->SetDelay(mDelay);
        
        //mReverbViewer->Update();
    }    

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->UpdateSpectrogram(false);
            
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Reverb::Init()
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
  
    bool optim = false;
    mReverb = new BLReverbSndF(sampleRate, optim);

    mMultiViewer = new MultiViewer2(sampleRate, BUFFER_SIZE);
  
    mCustomControl = new ReverbCustomControl(this);
    mCustomMouseControl = new ReverbCustomMouseControl(this);
    
    mReverbViewer = new BLReverbViewer(mReverb, mMultiViewer,
                                       VIEWER_DEFAULT_DURATION, sampleRate);
  
    mViewerTimeDuration = VIEWER_DEFAULT_DURATION;
  
    //
    ApplyParams();
    
    mIsInitialized = true;
}

void
Reverb::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

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
        
        return;
    }
    
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

    if (out.size() == 2)
    {
        if (in.size() == 1)
        {
            if (mReverb != NULL)
                mReverb->Process(in[0], &out[0], &out[1]);
        }
        else if (in.size() == 2)
        {
            WDL_TypedBuf<BL_FLOAT> stereoInput[2] = { in[0], in[1] };
            
            if (mReverb != NULL)
                mReverb->Process(stereoInput, &out[0], &out[1]);
        }
          
        BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    }
    else
        // Bad number of outputs
    {
        BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    }
  
    
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
Reverb::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);

    // TODO: mouse catcher
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN,
                                     kGraph);

    mSpectrogramDisplay = new SpectrogramDisplay2(mSpectrogramDisplayState);
    mSpectrogramDisplayState = mSpectrogramDisplay->GetState();
    
    if (mMultiViewer != NULL)
        mMultiViewer->SetGraph(mGraph, mSpectrogramDisplay, mGUIHelper);    

    mGraph->AddCustomControl(mCustomControl);

    // HACK, to avoid creating a IPanelMouseControl like in iPlug1
    // Now, mCustomControl and mCustomMouseControl must be the same object
    // And now, we need to set UPDATE_VIEW_ON_PARAM_CHANGE to 1
    mGraph->AddCustomControl(mCustomMouseControl);
        
    
    // Oversampling
    mGUIHelper->CreateKnob(pGraphics,
                           kOversampFactorX, kOversampFactorY,
                           KNOB_SMALL_FN,
                           kOversampFactorFrames,
                           kOversampFactor,
                           TEXTFIELD_FN,
                           "OVERSAMP",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Early
    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyAmountX, kEarlyAmountY,
                           KNOB_SMALL_FN,
                           kEarlyAmountFrames,
                           kEarlyAmount,
                           TEXTFIELD_FN,
                           "AMOUNT",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyWetX, kEarlyWetY,
                           KNOB_SMALL_FN,
                           kEarlyWetFrames,
                           kEarlyWet,
                           TEXTFIELD_FN,
                           "WET",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyDryX, kEarlyDryY,
                           KNOB_SMALL_FN,
                           kEarlyDryFrames,
                           kEarlyDry,
                           TEXTFIELD_FN,
                           "DRY",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyFactorX, kEarlyFactorY,
                           KNOB_SMALL_FN,
                           kEarlyFactorFrames,
                           kEarlyFactor,
                           TEXTFIELD_FN,
                           "FACTOR",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyWidthX, kEarlyWidthY,
                           KNOB_SMALL_FN,
                           kEarlyWidthFrames,
                           kEarlyWidth,
                           TEXTFIELD_FN,
                           "WIDTH",
                           GUIHelper12::SIZE_DEFAULT);

    // Reverb
    mGUIHelper->CreateKnob(pGraphics,
                           kRevWidthX, kRevWidthY,
                           KNOB_SMALL_FN,
                           kRevWidthFrames,
                           kRevWidth,
                           TEXTFIELD_FN,
                           "WIDTH",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kWetX, kWetY,
                           KNOB_SMALL_FN,
                           kWetFrames,
                           kWet,
                           TEXTFIELD_FN,
                           "WET",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kWanderX, kWanderY,
                           KNOB_SMALL_FN,
                           kWanderFrames,
                           kWander,
                           TEXTFIELD_FN,
                           "WANDER",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kBassBoostX, kBassBoostY,
                           KNOB_SMALL_FN,
                           kBassBoostFrames,
                           kBassBoost,
                           TEXTFIELD_FN,
                           "BASS BOOST",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kSpinX, kSpinY,
                           KNOB_SMALL_FN,
                           kSpinFrames,
                           kSpin,
                           TEXTFIELD_FN,
                           "SPIN",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kInputLPFX, kInputLPFY,
                           KNOB_SMALL_FN,
                           kInputLPFFrames,
                           kInputLPF,
                           TEXTFIELD_FN,
                           "INPUT LPF",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kBassLPFX, kBassLPFY,
                           KNOB_SMALL_FN,
                           kBassLPFFrames,
                           kBassLPF,
                           TEXTFIELD_FN,
                           "BASS LPF",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kDampLPFX, kDampLPFY,
                           KNOB_SMALL_FN,
                           kDampLPFFrames,
                           kDampLPF,
                           TEXTFIELD_FN,
                           "DAMP LPF",
                           GUIHelper12::SIZE_DEFAULT);

    // Common
    mGUIHelper->CreateKnob(pGraphics,
                           kOutputLPFX, kOutputLPFY,
                           KNOB_SMALL_FN,
                           kOutputLPFFrames,
                           kOutputLPF,
                           TEXTFIELD_FN,
                           "OUTPUT LPF",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kRT60X, kRT60Y,
                           KNOB_SMALL_FN,
                           kRT60Frames,
                           kRT60,
                           TEXTFIELD_FN,
                           "RT60",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kDelayX, kDelayY,
                           KNOB_SMALL_FN,
                           kDelayFrames,
                           kDelay,
                           TEXTFIELD_FN,
                           "DELAY",
                           GUIHelper12::SIZE_DEFAULT);

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
Reverb::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();
    if (mReverb != NULL)
        mReverb->Reset(sampleRate, blockSize);

    if (mMultiViewer != NULL)
        mMultiViewer->Reset(sampleRate, BUFFER_SIZE);

    if (mReverbViewer != NULL)
        mReverbViewer->Reset(sampleRate);

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Reverb::OnParamChange(int paramIdx)
{  
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kOversampFactor:
        {
            int factor = GetParam(kOversampFactor)->Int();
            mOversampFactor = factor;
            
            if (mReverb != NULL)
                mReverb->SetOversampFactor(factor);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        // Early
        case kEarlyAmount:
        {
            BL_FLOAT value = GetParam(kEarlyAmount)->Value();
            BL_FLOAT amount = value/100.0;
            mEarlyAmount = amount;
            
            if (mReverb != NULL)
                mReverb->SetEarlyAmount(amount);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kEarlyWet:
        {
            BL_FLOAT wet = GetParam(kEarlyWet)->Value();
            mEarlyWet = wet;
            
            if (mReverb != NULL)
                mReverb->SetEarlyWet(wet);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kEarlyDry:
        {
            BL_FLOAT dry = GetParam(kEarlyDry)->Value();
            mEarlyDry = dry;
            
            if (mReverb != NULL)
                mReverb->SetEarlyDry(dry);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
      
        case kEarlyFactor:
        {
            BL_FLOAT factor = GetParam(kEarlyFactor)->Value();
            mEarlyFactor = factor;
            
            if (mReverb != NULL)
                mReverb->SetEarlyFactor(factor);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kEarlyWidth:
        {
            BL_FLOAT value = GetParam(kEarlyWidth)->Value();
            BL_FLOAT width = value/100.0;
            mEarlyWidth = width;
            
            if (mReverb != NULL)
                mReverb->SetEarlyWidth(width);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
      
        // Reverb
        case kRevWidth:
        {
            BL_FLOAT value = GetParam(kRevWidth)->Value();
            BL_FLOAT width = value/100.0;
            mRevWidth = width;
            
            if (mReverb != NULL)
                mReverb->SetWidth(width);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
     
        case kWet:
        {
            BL_FLOAT wet = GetParam(kWet)->Value();
            mWet = wet;
            
            if (mReverb != NULL)
                mReverb->SetWet(wet);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kWander:
        {
            BL_FLOAT wander = GetParam(kWander)->Value();
            mWander = wander;
            
            if (mReverb != NULL)
                mReverb->SetWander(wander);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kBassBoost:
        {
            BL_FLOAT value = GetParam(kBassBoost)->Value();
            BL_FLOAT boost = value/100.0;
            mBassBoost = boost;
            
            if (mReverb != NULL)
                mReverb->SetBassBoost(boost);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kSpin:
        {
            BL_FLOAT spin = GetParam(kSpin)->Value();
            mSpin = spin;
            
            if (mReverb != NULL)
                mReverb->SetSpin(spin);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kInputLPF:
        {
            BL_FLOAT lpf = GetParam(kInputLPF)->Value();
            mInputLPF = lpf;
            
            if (mReverb != NULL)
                mReverb->SetInputLPF(lpf);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kBassLPF:
        {
            BL_FLOAT lpf = GetParam(kBassLPF)->Value();
            mBassLPF = lpf;
            
            if (mReverb != NULL)
                mReverb->SetBassLPF(lpf);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
      
        case kDampLPF:
        {
            BL_FLOAT lpf = GetParam(kDampLPF)->Value();
            mDampLPF = lpf;
            
            if (mReverb != NULL)
                mReverb->SetDampLPF(lpf);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
      
        // Common
        case kOutputLPF:
        {
            BL_FLOAT lpf = GetParam(kOutputLPF)->Value();
            mOutputLPF = lpf;
            
            if (mReverb != NULL)
                mReverb->SetOutputLPF(lpf);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kRT60:
        {
            BL_FLOAT rt60 = GetParam(kRT60)->Value();
            mRT60 = rt60;
            
            if (mReverb != NULL)
                mReverb->SetRT60(rt60);
      
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        case kDelay:
        {
            BL_FLOAT delay = GetParam(kDelay)->Value();
            mDelay = delay;
            
            if (mReverb != NULL)
                mReverb->SetDelay(delay);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
        }
        break;
    
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Reverb::OnUIOpen()
{    
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
Reverb::OnUIClose()
{   
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;

    if (mMultiViewer != NULL)
        mMultiViewer->SetGraph(NULL, NULL);
    
    mSpectrogramDisplay = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

int
Reverb::UnserializeState(const IByteChunk &pChunk, int startPos)
{
    TRACE;
  
    //IMutexLock lock(this);
  
    int res = IPluginBase::UnserializeParams(pChunk, startPos);

    if (mReverbViewer != NULL)
        mReverbViewer->Update();
  
    return res;
}

void
Reverb::ApplyPreset(int presetNum)
{
    for (int i = 0; i < 17; i++)
    {
        GetParam(i - kOversampFactor)->Set(_presets[presetNum][i]);
        BLUtilsPlug::TouchPlugParam(this, i); // ?
    }
}

void
Reverb::ApplyPreset(BL_FLOAT preset[])
{
    for (int i = 0; i < 17; i++)
    {
        GetParam(i - kOversampFactor)->Set(preset[i]);
        BLUtilsPlug::TouchPlugParam(this, i); // ?
    }
}

void
Reverb::OnMouseUp()
{
    if (mReverbViewer != NULL)
        mReverbViewer->Update();
  
#if DBG_DUMP_PRESET
    if (mReverb != NULL)
        mReverb->DumpPreset();
#endif
}

void
Reverb::UpdateTimeZoom(int mouseDelta)
{ 
    BL_FLOAT dZoom = mouseDelta*MOUSE_ZOOM_COEFF;
    
    mViewerTimeDuration *= (1.0 + dZoom);
    if (mViewerTimeDuration < VIEWER_MIN_DURATION)
        mViewerTimeDuration = VIEWER_MIN_DURATION;
    
    if (mViewerTimeDuration > VIEWER_MAX_DURATION)
        mViewerTimeDuration = VIEWER_MAX_DURATION;

    if (mReverbViewer != NULL)
        mReverbViewer->SetDuration(mViewerTimeDuration);
}
