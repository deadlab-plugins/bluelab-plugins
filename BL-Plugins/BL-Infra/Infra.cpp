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
#include <InfraProcess2.h>
#include <FftProcessBufObj.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <GraphControl12.h>
#include <GraphFreqAxis2.h>
#include <GraphAmpAxis.h>
#include <GraphAxis2.h>

#include <GraphCurve5.h>
#include <SmoothCurveDB.h>

#include <Scale.h>

#include <BlaTimer.h>

#include <ParamSmoother2.h>

#include "Infra.h"

#include "IPlug_include_in_plug_src.h"

//
#define USE_VARIABLE_BUFFER_SIZE 1

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

// CHECK: maybe it could work well with 1024 buffer ?
#define BUFFER_SIZE 2048

#define OVERSAMPLING 4 //2 //4 //32

#define FREQ_RES 1

#define KEEP_SYNTHESIS_ENERGY 0

// When set to 0, we are accurate for frequencies
#define VARIABLE_HANNING 0

// -60: generic
// -70: "oohoo" => gets more partials
// -75: 'oohoo" => get all partials
//
// 100.0: good for most of tested sounds
#define DEFAULT_TRACKER_THRESHOLD -100.0//-75.0 //-70.0 //-70.0 //-60.0

#define THRESHOLD_IS_DB 1

#define RANGE_500_HZ 1

#define USE_PHANT_ADAPTIVE 1

#define GRAPH_MIN_DB -119.0 // Take care of the noise/harmo bottom dB
#define GRAPH_MAX_DB 10.0

#define GRAPH_CURVE_NUM_VALUES 512 //256
//#define GRAPH_CURVE_NUM_VALUES 1024

// See Air for details
//#define CURVE_SMOOTH_COEFF 0.95
#define CURVE_SMOOTH_COEFF_MS 1.4

#define SHOW_SUM_CURVE 1

#if 0
/*TODO: port to PartialTracker5, and check that it is still ok

TODO: add a Sync option, like with InfraSynth
  
NOTE: try to optimize: create a class that generates a series of partials (1 sine and multiples), without recomputing the sine each time. Compute the first sine, then get the other sines from it. (check if it possible, for sine x 2, this is possible, try to find a generic way).

NOTE: Gearslutz forum: "Your sub generator is also pretty cool but won't dethrone Refuse Lowender for me."

IDEA(Emrah): i just think maybe there should be a harmonic distortion on added sub not the dry signal

TODO(next version): use a toggle button for fixed/not fixed phantom freq

Doc on beat frequencies: "http://hyperphysics.phy-astr.gsu.edu/hbase/Sound/beat.html"

NOTE: SINE_LUT adds a little noise on high freqs

TODO: use NRBJFilter if need low pass filter
(instead of the current one)

Similar plugs:
- MaxxBass (Waves)
- Subbass (Logic)

TODO: add sidechain input for BD (and a knob for sc strength)

IDEA: compute harmonics only when mix is > 0%
(should not optimize a lot...)
*/
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipThreshold = "Threshold - Harmonic detection threshold";
static char *tooltipPhantomFreq = "Phantom Frequency";
static char *tooltipPhantomMix =
    "Phantom Fundamental Mix - Amount of phantom fundamental effect";
static char *tooltipSubOrder = "Sub Frequency Order - How many octaves under";
static char *tooltipSubMix = "Sub Frequency Mix - Amount of sub frequency";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipMonoOut = "Mono Out - Force output to mono";
static char *tooltipPhantAdaptive = "Adaptive Phantom Frequency";
static char *tooltipBassFocus = "Bass Focus - Mono lowest frequencies";

enum EParams
{
    kThreshold = 0,

    kPhantomFreq,
    kPhantAdaptive,
    kPhantomMix,

    kSubOrder,
    kSubMix,

    kBassFocus,

    kMonoOut,
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
    
    kThresholdX = 42,
    kThresholdY = 375,
    
    kPhantomFreqX = 274,
    kPhantomFreqY = 222,
  
    kPhantomMixX = 256,
    kPhantomMixY = 339,
    
    kOutGainX = 390,
    kOutGainY = 374,

    kRadioButtonsSubOrderX = 131,
    kRadioButtonsSubOrderY = 278,
    kRadioButtonsSubOrderFrames = 3,
    kRadioButtonsSubOrderNumButtons = 4,
    kRadioButtonsSubOrderVSize = 92,
    
    kSubMixX = 140,
    kSubMixY = 339,
    
    kMonoOutX = 344,
    kMonoOutY = 329,
    
    kPhantAdaptiveX = 13,
    kPhantAdaptiveY = 329,

    kBassFocusX = 345,
    kBassFocusY = 290
};

//
Infra::Infra(const InstanceInfo &info)
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
    
    //MakeDefaultPreset((char *) "-", kNumPrograms);
    
    BL_PROFILE_RESET;
}

Infra::~Infra()
{
    // Process
    if (mFftObj != NULL)
        delete mFftObj;

    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            delete mInfraProcessObjs[i];
    }

    // In
    for (int i = 0; i < 2; i++)
    {
        if (mFftObjsIn[i] != NULL)
            delete mFftObjsIn[i];
    }

    for (int i = 0; i < 2; i++)
    {
        if (mBufObjsIn[i] != NULL)
            delete mBufObjsIn[i];
    }

    // Out
    if (mFftObjOut != NULL)
        delete mFftObjOut;
    if (mBufObjOut != NULL)
        delete mBufObjOut;
    
    delete mOutGainSmoother;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mAmpAxis != NULL)
        delete mAmpAxis;

    if (mVAxis != NULL)
        delete mVAxis;
    
    //
    if (mInputCurve != NULL)
        delete mInputCurve;
    if (mInputCurveSmooth != NULL)
        delete mInputCurveSmooth;
    
    if (mOscillatorsCurve != NULL)
        delete mOscillatorsCurve;
    if (mOscillatorsCurveSmooth != NULL)
        delete mOscillatorsCurveSmooth;

    if (mSumCurve != NULL)
        delete mSumCurve;
    if (mSumCurveSmooth != NULL)
        delete mSumCurveSmooth;
}

IGraphics *
Infra::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
Infra::MyMakeLayout(IGraphics *pGraphics)
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
    
    //pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
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
Infra::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;

    // Process
    mFftObj = NULL;

    mInfraProcessObjs[0] = NULL;
    mInfraProcessObjs[1] = NULL;

    // In
    mFftObjsIn[0] = NULL;
    mFftObjsIn[1] = NULL;

    mBufObjsIn[0] = NULL;
    mBufObjsIn[1] = NULL;

    // Out
    mFftObjOut = NULL;
    mBufObjOut = NULL;
    
    //
    mPhantFreqControl = NULL;
        
    mOutGainSmoother = NULL;

    mMonoOutChanged = false;  
    mPhantAdaptive = false;

    mBassFocusChanged = false;

    mGraph = NULL;
    
    //
    mAmpAxis = NULL;
    mHAxis = NULL;
    
    mFreqAxis = NULL;
    mVAxis = NULL;
    
    //
    mInputCurve = NULL;
    mInputCurveSmooth = NULL;
    
    mOscillatorsCurve = NULL;
    mOscillatorsCurveSmooth = NULL;

    mSumCurve = NULL;
    mSumCurveSmooth = NULL;
    
    mGUIHelper = NULL;
}

void
Infra::Init(int oversampling, int freqRes)
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT defaultGain = 1.0; // 1 is 0dB
    mOutGain = defaultGain;

    BL_FLOAT sampleRate = GetSampleRate();
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultGain);
  
    int bufferSize = BUFFER_SIZE;
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif

    // Process
    if (mFftObj == NULL)
    {
        int numChannels = 2;
        int numScInputs = 0;
    
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < numChannels; i++)
        {
            mInfraProcessObjs[i] = new InfraProcess2(bufferSize,
                                                     oversampling, freqRes,
                                                     sampleRate);
            mInfraProcessObjs[i]->SetThreshold(DEFAULT_TRACKER_THRESHOLD);
            
            processObjs.push_back(mInfraProcessObjs[i]);
        }

        // Set twin obj, for bass focus
        mInfraProcessObjs[0]->SetTwinMasterObj(mInfraProcessObjs[1]);
        
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      bufferSize, oversampling, freqRes,
                                      sampleRate);
            
        // OPTIM PROF Infra
        mFftObj->SetSkipIFft(-1, true);
      
        // Useful ?
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);

        
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
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }

    // In
    if (mFftObjsIn[0] == NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            int numChannels = 1;
            int numScInputs = 0;
    
            vector<ProcessObj *> processObjs;
            mBufObjsIn[i] = new FftProcessBufObj(bufferSize, oversampling,
                                                 freqRes, sampleRate);
            
            processObjs.push_back(mBufObjsIn[i]);

            mFftObjsIn[i] = new FftProcessObj16(processObjs,
                                                numChannels, numScInputs,
                                                bufferSize, oversampling, freqRes,
                                                sampleRate);

            if (i == 0)
                mFftObjsIn[i]->SetSkipFft(-1, true);
            
            // OPTIM PROF Infra
            mFftObjsIn[i]->SetSkipIFft(-1, true);
      
            // Useful ?
            mFftObjsIn[i]->Reset(bufferSize, oversampling, freqRes, sampleRate);

        
#if !VARIABLE_HANNING
            mFftObjsIn[i]->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                             FftProcessObj16::WindowHanning);
            
            mFftObjsIn[i]->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                              FftProcessObj16::WindowHanning);
#else
            mFftObjsIn[i]->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                             FftProcessObj16::WindowVariableHanning);
        
            mFftObjsIn[i]->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                              FftProcessObj16::WindowVariableHanning);
#endif
      
            mFftObjsIn[i]->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                                  KEEP_SYNTHESIS_ENERGY);
        }
    }
    else
    {
        for (int i = 0; i < 2; i++)
            mFftObjsIn[i]->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }
    
    // Out
    if (mFftObjOut == NULL)
    {
        int numChannels = 1;
        int numScInputs = 0;
    
        vector<ProcessObj *> processObjs;
        mBufObjOut = new FftProcessBufObj(bufferSize, oversampling,
                                          freqRes, sampleRate);
            
        processObjs.push_back(mBufObjOut);

        //
        mFftObjOut = new FftProcessObj16(processObjs,
                                         numChannels, numScInputs,
                                         bufferSize, oversampling, freqRes,
                                         sampleRate);
            
        // OPTIM PROF Infra
        mFftObjOut->SetSkipIFft(-1, true);
      
        // Useful ?
        mFftObjOut->Reset(bufferSize, oversampling, freqRes, sampleRate);

        
#if !VARIABLE_HANNING
        mFftObjOut->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                      FftProcessObj16::WindowHanning);
        
        mFftObjOut->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowHanning);
#else
        mFftObjOut->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                      FftProcessObj16::WindowVariableHanning);
        
        mFftObjOut->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowVariableHanning);
#endif
      
        mFftObjOut->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                           KEEP_SYNTHESIS_ENERGY);
    }
    else
    {
        mFftObjOut->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }

    int blockSize = GetBlockSize();
    int latency = mFftObj->ComputeLatency(blockSize);
    SetLatency(latency);
    
    mIsInitialized = true;
}

void
Infra::InitParams()
{
    // Threshold
#if !THRESHOLD_IS_DB
    BL_FLOAT defaultThreshold = 50.0;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold,
                                     0.0, 100.0, 0.1, "%");
#else
    BL_FLOAT defaultThreshold = DEFAULT_TRACKER_THRESHOLD;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold,
                                     -120.0, 0.0, 0.1, "dB");
#endif
    
    // Phantom Freq
    BL_FLOAT defaultPhantomFreq = 20.0;
    mPhantomFreq = defaultPhantomFreq;
#if !RANGE_500_HZ
    GetParam(kPhantomFreq)->InitDouble("PhantomFreq", defaultPhantomFreq,
                                       1.0, 250.0, 0.1, "Hz");
#else
    GetParam(kPhantomFreq)->InitDouble("PhantomFreq", defaultPhantomFreq,
                                       1.0, 500.0, 0.1, "Hz");
#endif

    // Phantom Mix
    BL_FLOAT defaultPhantomMix = 50.0;
    mPhantomMix = defaultPhantomMix;
    GetParam(kPhantomMix)->InitDouble("PhantomMix", defaultPhantomMix,
                                      0.0, 100.0, 0.1, "%");
  
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 1 is 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain,
                                   -12.0, 12.0, 0.1, "dB");
    
    // Sub Freq Order
    BL_FLOAT defaultSubOrder = 1.0;
    mSubOrder = defaultSubOrder;
  
    // 4 possible octaves
    // Must use double type, otherwise OnMouseDrag() has problems
    // Must use an offset so the knob bitmap changed at
    // the same time as the string tone name
#define SUB_ORDER_PARAM_OFFSET 0.0 //0.5
    GetParam(kSubOrder)->InitDouble("SubOrder", defaultSubOrder,
                                    1.0 + SUB_ORDER_PARAM_OFFSET,
                                    4.0 + SUB_ORDER_PARAM_OFFSET,
                                    1.0, "");
    
    
    // Sub Freq Mix
    BL_FLOAT defaultSubMix = 0.0;
    mSubMix = defaultSubMix;
    GetParam(kSubMix)->InitDouble("SubMix", defaultSubMix, 0.0, 100.0, 0.1, "%");
    
    // Mono out
    mMonoOut = false;
    //GetParam(kMonoOut)->InitInt("MonoOut", 0, 0, 1);
    GetParam(kMonoOut)->InitEnum("MonoOut", 0, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
    
    // Phant adaptive
#if USE_PHANT_ADAPTIVE
    mPhantAdaptive = false;
    //GetParam(kPhantAdaptive)->InitInt("AdaptivePhant", 0, 0, 1);
    GetParam(kPhantAdaptive)->InitEnum("AdaptivePhant", 0, 2,
                                       "", IParam::kFlagsNone, "",
                                       "Off", "On");
#endif

    // Bass focus
    mBassFocus = false;
    //GetParam(kBassFocus)->InitInt("BassFocus", 0, 0, 1);
    GetParam(kBassFocus)->InitEnum("BassFocus", 0, 2,
                                   "", IParam::kFlagsNone, "",
                                   "Off", "On");
}

void
Infra::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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
    {
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
    
    // Must reset if mono out changed
    // Otherwise there is a phasing effect that appears
    // (and it stays)
    // This is because the two channels are not synchronized in FftObj 
    if (mMonoOutChanged || mBassFocusChanged)
        OnReset();
    mMonoOutChanged = false;
    mBassFocusChanged = false;
    
    int outSize = (int)out.size();
    if (mMonoOut)
    {
        WDL_TypedBuf<BL_FLOAT> monoIn;
        if (in.size() == 1)
            monoIn = in[0];
        if (in.size() == 2)
        {
            BLUtils::StereoToMono(&monoIn, in[0], in[1]);
            
            in.resize(1);
            in[0] = monoIn;
        }
        
        if (out.size() == 2)
            out.resize(1);
    }

    mFftObj->Process(in, scIn, &out);

    //
    if (!in.empty() && !out.empty())
    {
        vector<WDL_TypedBuf<BL_FLOAT> > &dummyOut = mTmpBuf7;
        dummyOut = out;

        // Process prev input
        vector<WDL_TypedBuf<BL_FLOAT> > &delayedInput = mTmpBuf10;
        delayedInput.resize(1);
        delayedInput[0] = in[0];

        mFftObjsIn[0]->Process(delayedInput, scIn, &delayedInput);
        mFftObjsIn[1]->Process(delayedInput, scIn, &dummyOut);

        // Process output, to get the corresonding fft
        mFftObjOut->Process(out, scIn, &dummyOut);
    }
    
    // Apply output gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
    if (mMonoOut) // && (outSize == 2))
    {
        out.resize(2);
        out[1] = out[0];
    }
    
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
Infra::CreateControls(IGraphics *pGraphics)
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
    
    CreateGraphAxes();
    CreateGraphCurves();
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    // Threshold
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kThresholdX, kThresholdY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kThreshold,
                              TEXTFIELD_FN,
                              "THRS",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipThreshold);

    // Phantom freq
    mPhantFreqControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                                  kPhantomFreqX,
                                                  kPhantomFreqY,
                                                  kKnobSmallWidth, kKnobSmallHeight,
                                                  KNOB_SMALL_FN,
                                                  kPhantomFreq,
                                                  TEXTFIELD_FN,
                                                  "PHANT FREQ",
                                                  GUIHelper12::SIZE_SMALL,
                                                  NULL, true,
                                                  tooltipPhantomFreq);

    // Phantom mix
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kPhantomMixX, kPhantomMixY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kPhantomMix,
                              TEXTFIELD_FN,
                              "PHANT MIX",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipPhantomMix);

    // Out gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX, kOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipOutGain);

    // Sub order
    const char *subOrders[] = { BUTTON_NUMBER_1_FN,
                                BUTTON_NUMBER_2_FN,
                                BUTTON_NUMBER_3_FN,
                                BUTTON_NUMBER_4_FN };
    
    mGUIHelper->CreateRadioButtonsCustom(pGraphics,
                                         kRadioButtonsSubOrderX,
                                         kRadioButtonsSubOrderY,
                                         subOrders,
                                         // 4 sub order values
                                         kRadioButtonsSubOrderNumButtons,
                                         // 3 frames
                                         kRadioButtonsSubOrderFrames,
                                         kRadioButtonsSubOrderVSize,
                                         kSubOrder,
                                         true,
                                         tooltipSubOrder);
    
    // Sub mix
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSubMixX, kSubMixY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kSubMix,
                              TEXTFIELD_FN,
                              "SUB MIX",
                              GUIHelper12::SIZE_SMALL,
                              NULL, true,
                              tooltipSubMix);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kMonoOutX,
                                   kMonoOutY,
                                   CHECKBOX_FN,
                                   kMonoOut,
                                   "MONO OUT",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipMonoOut);

#if USE_PHANT_ADAPTIVE
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kPhantAdaptiveX,
                                   kPhantAdaptiveY,
                                   CHECKBOX_FN,
                                   kPhantAdaptive,
                                   "ADAPTIVE",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipPhantAdaptive);
#endif

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kBassFocusX,
                                   kBassFocusY,
                                   CHECKBOX_FN,
                                   kBassFocus,
                                   "BASS FOCUS",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipBassFocus);
    
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
Infra::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Infra::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    int bufferSize = BUFFER_SIZE;
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset

    // Process
    if (mFftObj != NULL)
        mFftObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);

    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
        {
            mInfraProcessObjs[i]->Reset(bufferSize, OVERSAMPLING,
                                        FREQ_RES, sampleRate);

            mInfraProcessObjs[i]->SetBassFocus(mBassFocus);

            // Do not try to bass focus/twin objs if mono out
            // because if mono out, we will process and use only the first channel
            if (mMonoOut)
                mInfraProcessObjs[i]->SetBassFocus(false);
        }
    }
    
    // In
    for (int i = 0; i < 2; i++)
    {
        if (mFftObjsIn[i] != NULL)
            mFftObjsIn[i]->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
    }

    for (int i = 0; i < 2; i++)
    {
        if (mBufObjsIn[i] != NULL)
            mBufObjsIn[i]->Reset(bufferSize, OVERSAMPLING,
                                 FREQ_RES, sampleRate);
    }

    // Out
    if (mFftObjOut != NULL)
        mFftObjOut->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
    if (mBufObjOut != NULL)
        mBufObjOut->Reset(bufferSize, OVERSAMPLING,
                          FREQ_RES, sampleRate);
    
    //
    if (mFftObj != NULL)
    {
        int blockSize = GetBlockSize();
        int latency = mFftObj->ComputeLatency(blockSize);
        SetLatency(latency);
    }
    
    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);

    // Curves
    if (mInputCurve != NULL)
        mInputCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mOscillatorsCurve != NULL)
        mOscillatorsCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mSumCurve != NULL)
        mSumCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);

    // Curves smooth
    if (mInputCurveSmooth != NULL)
        mInputCurveSmooth->Reset(sampleRate);
    if (mOscillatorsCurveSmooth != NULL)
        mOscillatorsCurveSmooth->Reset(sampleRate);
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

// NOTE: OnIdle() is called from the GUI thread
void
Infra::OnIdle()
{
    if (!mUIOpened)
        return;
    
    ENTER_PARAMS_MUTEX;

    mInputCurveSmooth->PullData();
    mOscillatorsCurveSmooth->PullData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->PullData();
    
    LEAVE_PARAMS_MUTEX;

    // We don't need mutex anymore now
    mInputCurveSmooth->ApplyData();
    mOscillatorsCurveSmooth->ApplyData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->ApplyData();
}

void
Infra::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kThreshold:
        {
            BL_FLOAT threshold = GetParam(kThreshold)->Value();
            mThreshold = threshold;

            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetThreshold(threshold);
            }
        }
        break;
    
        case kPhantomFreq:
        {
            BL_FLOAT phantomFreq = GetParam(kPhantomFreq)->Value();
            mPhantomFreq = phantomFreq;
            
            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetPhantomFreq(phantomFreq);
            }
        }
        break;
          
        case kPhantomMix:
        {
            BL_FLOAT phantomMix = GetParam(kPhantomMix)->Value();
            phantomMix = phantomMix/100.0;
            mPhantomMix = phantomMix;
            
            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetPhantomMix(phantomMix);
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
    
        case kSubOrder:
        {
            BL_FLOAT subOrder = GetParam(kSubOrder)->Value();
            int subOrderI = (int)subOrder;
            mSubOrder = subOrderI;
            
            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetSubOrder(subOrderI);
            }
        }
        break;
         
        case kSubMix:
        {
            BL_FLOAT subMix = GetParam(kSubMix)->Value();
            subMix = subMix/100.0;
            mSubMix = subMix;
            
            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetSubMix(subMix);
            }
        }
        break;
     
        case kMonoOut:
        {
            int value = GetParam(kMonoOut)->Value();

            bool valueBool = (value == 1);
            
            mMonoOutChanged = (valueBool != mMonoOut);
            
            mMonoOut = valueBool;
            
            //mMonoOutChanged = true;
        }
        break;
          
#if USE_PHANT_ADAPTIVE
        case kPhantAdaptive:
        {
            int value = GetParam(kPhantAdaptive)->Value();
            mPhantAdaptive = (value == 1);
            
            for (int i = 0; i < 2; i++)
            {
                if (mInfraProcessObjs[i] != NULL)
                    mInfraProcessObjs[i]->SetAdaptivePhantomFreq(mPhantAdaptive);
            }
            
            if (mPhantFreqControl != NULL)
            {
                //mPhantFreqControl->SetDisabled(mPhantAdaptive);

                // Also disable knob value automatically
                IGraphics *graphics = GetUI();
                if (graphics != NULL)
                    graphics->DisableControl(kPhantomFreq, mPhantAdaptive);
            }
        }
        break;
#endif

        case kBassFocus:
        {
            int value = GetParam(kBassFocus)->Value();

            bool valueBool = (value == 1);
            
            mBassFocusChanged = (valueBool != mBassFocus);
            
            mBassFocus = valueBool;
            
            //mBassFocusChanged = true;
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Infra::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Infra::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;

    mGraph = NULL;
    mPhantFreqControl = NULL;

    LEAVE_PARAMS_MUTEX;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Infra::ApplyParams()
{       
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetThreshold(mThreshold);
    }
              
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetPhantomFreq(mPhantomFreq);
    }
        
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetPhantomMix(mPhantomMix);
    }
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
        
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetSubOrder(mSubOrder);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetSubMix(mSubMix);
    }
          
#if USE_PHANT_ADAPTIVE        
    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
            mInfraProcessObjs[i]->SetAdaptivePhantomFreq(mPhantAdaptive);
    }
    
    if (mPhantFreqControl != NULL)
    {
        //mPhantFreqControl->SetDisabled(mPhantAdaptive);

        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kPhantomFreq, mPhantAdaptive);
    }
#endif

    for (int i = 0; i < 2; i++)
    {
        if (mInfraProcessObjs[i] != NULL)
        {
            mInfraProcessObjs[i]->SetBassFocus(mBassFocus);

            // Do not try to bass focus/twin objs if mono out
            // because if mono out, we will process and use only the first channel
            if (mMonoOut)
                mInfraProcessObjs[i]->SetBassFocus(false);
        }
    }
}

void
Infra::CreateGraphAxes()
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
Infra::CreateGraphCurves()
{
#define REF_SAMPLERATE 44100.0
    BL_FLOAT curveSmoothCoeff =
        ParamSmoother2::ComputeSmoothFactor(CURVE_SMOOTH_COEFF_MS, REF_SAMPLERATE);
    
    if (mInputCurve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
    
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        // Input curve
        int inputColor[4];
        mGUIHelper->GetGraphCurveColorBlue(inputColor);
        
        mInputCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mInputCurve->SetDescription("in", descrColor);
        mInputCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mInputCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mInputCurve->SetFill(true);
        mInputCurve->SetFillAlpha(fillAlpha);
        mInputCurve->SetColor(inputColor[0], inputColor[1], inputColor[2]);
        mInputCurve->SetLineWidth(2.0);
        
        mInputCurveSmooth = new SmoothCurveDB(mInputCurve,
                                              //CURVE_SMOOTH_COEFF,
                                              curveSmoothCoeff,
                                              GRAPH_CURVE_NUM_VALUES,
                                              GRAPH_MIN_DB,
                                              GRAPH_MIN_DB, GRAPH_MAX_DB,
                                              sampleRate);
    
        // Oscillators curve
        int oscColor[4];
        mGUIHelper->GetGraphCurveColorPurple(oscColor);
        
        mOscillatorsCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mOscillatorsCurve->SetDescription("freqs", descrColor);
        mOscillatorsCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mOscillatorsCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mOscillatorsCurve->SetFill(true);
        mOscillatorsCurve->SetFillAlpha(fillAlpha);
        mOscillatorsCurve->SetColor(oscColor[0], oscColor[1], oscColor[2]);
        
        mOscillatorsCurveSmooth = new SmoothCurveDB(mOscillatorsCurve,
                                                    //CURVE_SMOOTH_COEFF,
                                                    curveSmoothCoeff,
                                                    GRAPH_CURVE_NUM_VALUES,
                                                    GRAPH_MIN_DB,
                                                    GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                    sampleRate);

#if SHOW_SUM_CURVE
        // Sum curve
        int sumColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(sumColor);
        
        mSumCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mSumCurve->SetDescription("out", descrColor);
        mSumCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mSumCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mSumCurve->SetFillAlpha(fillAlpha);
        mSumCurve->SetColor(sumColor[0], sumColor[1], sumColor[2]);
        
        mSumCurveSmooth = new SmoothCurveDB(mSumCurve,
                                            //CURVE_SMOOTH_COEFF,
                                            curveSmoothCoeff,
                                            GRAPH_CURVE_NUM_VALUES,
                                            GRAPH_MIN_DB,
                                            GRAPH_MIN_DB, GRAPH_MAX_DB,
                                            sampleRate);
#endif
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
        
    mInputCurve->SetViewSize(graphSize[0], graphSize[1]);
    
    mOscillatorsCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mOscillatorsCurve);

    // Input after, since its level is often lower
    mGraph->AddCurve(mInputCurve);
    
    // Add sum curve over all other curves
#if SHOW_SUM_CURVE
    mSumCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSumCurve);
#endif
}

void
Infra::UpdateCurves()
{    
    if (!mUIOpened)
        return;
    
    WDL_TypedBuf<BL_FLOAT> &input = mTmpBuf3;
    // Get the input of the second buf obj,
    // so the delay is in synch with output buf obj
    mBufObjsIn[1]->GetBuffer(&input); // 
    mInputCurveSmooth->SetValues(input);

    WDL_TypedBuf<BL_FLOAT> &output = mTmpBuf4;
    mBufObjOut->GetBuffer(&output);
    
    WDL_TypedBuf<BL_FLOAT> &oscillators = mTmpBuf5;
 
    // Substract: out - in = oscillator freqs
    oscillators = output;
    BLUtils::SubstractValues(&oscillators, input);
    
    mOscillatorsCurveSmooth->SetValues(oscillators);
    
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->SetValues(output);
        
    // Lock free
    mInputCurveSmooth->PushData();
    mOscillatorsCurveSmooth->PushData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->PushData();
}
