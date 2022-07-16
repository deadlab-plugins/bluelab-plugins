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

#include <AirProcess3.h>

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

#include <FastMath.h>

#include <CrossoverSplitterNBands4.h>
#include <DelayObj4.h>
#include <FftProcessBufObj.h>

#include <ParamSmoother2.h>

//#include <TestParamSmoother.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Air.h"

#include "IPlug_include_in_plug_src.h"

//
#define USE_VARIABLE_BUFFER_SIZE 1

#define BUFFER_SIZE 2048
#define OVERSAMPLING 4 // With 32, result is better (less low freq vibrations in noise)
#define FREQ_RES 1
#define KEEP_SYNTHESIS_ENERGY 0
#define VARIABLE_HANNING 0 // When set to 0, we are accurate for frequencies

//#define GRAPH_MIN_DB -120.0
//#define GRAPH_MAX_DB 0.0
//#define GRAPH_MIN_DB -139.0
#define GRAPH_MIN_DB -119.0 // Take care of the noise/harmo bottom dB
#define GRAPH_MAX_DB 10.0

#define GRAPH_CURVE_NUM_VALUES 512 //256

//#define CURVE_SMOOTH_COEFF 0.95
//#define CURVE_SMOOTH_COEFF_MS 2.777

// With SmoothCurveDB::OPTIM_LOCK_FREE, we process less often,
// so we need to be less smooth
//
// NOTE: with 1, this is a bit less smooth than before
//#define CURVE_SMOOTH_COEFF_MS 1.0 //0.7 //1.4
// NOTE: with 1.4, this is smoother
#define CURVE_SMOOTH_COEFF_MS 1.4

#define SHOW_SUM_CURVE 1 //0

// -60: generic
// -70: "oohoo" => gets more partials
// -75: 'oohoo" => get all partials
//
// 100.0: good for most of tested sounds
#define DEFAULT_TRACKER_THRESHOLD -100.0//-75.0 //-70.0 //-70.0 //-60.0

#define THRESHOLD_IS_DB 1

// Improvements
#define USE_FREQ_SPLITTER 1 // 0

#define DEFAULT_SPLIT_FREQ 20.0 //100.0
//#define DEFAULT_SPLIT_FREQ_SMOOTH_TIME_MS 0.1 // 0.1ms (default: 140ms)
#define DEFAULT_SPLIT_FREQ_SMOOTH_TIME_MS 280.0 //140.0

#define MIN_SPLIT_FREQ 20.0


#if 0
SALES: talk of "spectral processing", not of "partial tracking"

- IDEA: do not filter partials, just use them not tracked, to get the noise envelope (and avoid missing partials that have lost tracking)

TODO: try to improve the sound when the parameters are near the limit, with Wiener soft masking (complex)

TODO: graph with 2 curved (air and partials), mix/balance the 2 curves

TODO: add an option to generate white noise under a noise envelope (so we can get a perfect whisper, without any remaining partials). Maybe add a parameter to setup the envelope smoothness.

TODO: add an EQ visualization, with partials in orange, and noise in blue. when turning the knob, partials or noise become more transparent in the visualization (but keep a bit of alpha at the end to even see each part).

Logic: click when bypass
During playing, if we bypass briefly, and re-enable,
there is garbage in the sound, then a big click later
(often, not every time)
This is the case for all plugins with a latency
With plugin with no latency, there is no problem
=> this is a Logic bug !
(tested with Izotope Denoise, this is the same problem !)

TEST: test performances: 34% CPU (ok)

TODO: make a code clean
- remaining GUI SIZE code, remaining camera fov accessors in .h

IDEA: quality button ? (test before)
- better noise quality with OVERSAMPLING 32

NOTE(?): there remains some musical noise in the noise envelope ("oohoo")
MANUAL: tell that with mix at 0, the signal is not exactly the same as when bypassesd

NOTE: "tss" is still there

IDEA: method resynth sin + substract origin signal => noise envelope
(described here: https://www.dsprelated.com/freebooks/sasp/Spectral_Modeling_Synthesis.html)
 and here: http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.134.7632&rep=rep1&type=pdf
 => +
 - synthetize many sinusoids using fft + freq adjust obj
 
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipThreshold = "Threshold - Harmonic detection threshold";
static char *tooltipMix = "Harmonic/Air - Mix between harmonic and air";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipSoftMasks = "Smart Resynthesis - Higher quality resynthesis";
static char *tooltipSplitFreq = "Wet Limit Frequency - Signal is untouched before";
static char *tooltipWetGain = "Wet Gain - Gain applied to wet signal";
 
enum EParams
{
    kThreshold = 0,
    kMix,
    kUseSoftMasks,

    kSplitFreq,
    kWetGain,

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
    
    kThresholdX = 60,
    kThresholdY = 374,
    
    kMixX = 150,
    kMixY = 338,
    
    kOutGainX = 360,
    kOutGainY = 274,

    kUseSoftMasksX = 40,
    kUseSoftMasksY = 282,
    
    kGraphX = 0,
    kGraphY = 0,

    // Improvements
    kSplitFreqX = 270,
    kSplitFreqY = 374,

    kWetGainX = 360,
    kWetGainY = 374
};


 //
 Air::Air(const InstanceInfo &info)
 : Plugin(info, MakeConfig(kNumParams, kNumPresets))
 {
     TRACE;

     // TEST
     //TestParamSmoother::RunTest();
  
     InitNull();
     InitParams();

     Init(OVERSAMPLING, FREQ_RES);
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
     mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
     mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
     BL_PROFILE_RESET;
 }

Air::~Air()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mFftObj != NULL)
        delete mFftObj;
    
    for (int i = 0; i < 2; i++)
    {
        if (mAirProcessObjs[i] != NULL)
            delete mAirProcessObjs[i];
    }
    
    delete mOutGainSmoother;
    
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
    if (mAirCurve != NULL)
        delete mAirCurve;
    if (mAirCurveSmooth != NULL)
        delete mAirCurveSmooth;
    
    if (mHarmoCurve != NULL)
        delete mHarmoCurve;
    if (mHarmoCurveSmooth != NULL)
        delete mHarmoCurveSmooth;
    
    if (mSumCurve != NULL)
        delete mSumCurve;
    if (mSumCurveSmooth != NULL)
        delete mSumCurveSmooth;

#if USE_FREQ_SPLITTER
    // Improvements
    for (int i = 0; i < 2; i++)
    {   
        if (mBandSplittersIn[i] != NULL)
            delete mBandSplittersIn[i];
    }

    for (int i = 0; i < 2; i++)
    {   
        if (mBandSplittersOut[i] != NULL)
            delete mBandSplittersOut[i];
    }
    
    if (mWetGainSmoother != NULL)
        delete mWetGainSmoother;

    for (int i = 0; i < 2; i++)
        delete mInputDelays[i];

    if (mFftObjOut != NULL)
        delete mFftObjOut;
    if (mBufObjOut != NULL)
        delete mBufObjOut;

#if USE_SPLIT_FREQ_SMOOTHER
    if (mSplitFreqSmoother != NULL)
        delete mSplitFreqSmoother;
#endif
    
#endif
}

IGraphics *
Air::MyMakeGraphics()
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
Air::MyMakeLayout(IGraphics *pGraphics)
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
Air::InitNull()
{                  
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();

    // TEST
    //FastMath::SetFast(true);
    
    //
    mFftObj = NULL;
    
    mAirProcessObjs[0] = NULL;
    mAirProcessObjs[1] = NULL;
    
    mOutGainSmoother = NULL;
    
    mMixTextControl = NULL;
    
    mGUIHelper = NULL;
    
    //
    mGraph = NULL;
    
    mAmpAxis = NULL;
    mHAxis = NULL;
    
    mFreqAxis = NULL;
    mVAxis = NULL;
    
    //
    mAirCurve = NULL;
    mAirCurveSmooth = NULL;
    
    mHarmoCurve = NULL;
    mHarmoCurveSmooth = NULL;
    
    mSumCurve = NULL;
    mSumCurveSmooth = NULL;

    mLatencyChanged = true;

#if USE_FREQ_SPLITTER
    // Improvements
    for (int i = 0; i < 2; i++)
        mBandSplittersIn[i] = NULL;

    for (int i = 0; i < 2; i++)
        mBandSplittersOut[i] = NULL;
    
    mWetGainSmoother = NULL;

    for (int i = 0; i < 2; i++)
        mInputDelays[i] = NULL;

    mFftObjOut = NULL;
    mBufObjOut = NULL;

#if USE_SPLIT_FREQ_SMOOTHER
    mSplitFreqSmoother = NULL;
#endif
    
#endif
}

void
Air::InitParams()
{
#if !THRESHOLD_IS_DB
    // Threshold
    BL_FLOAT defaultThreshold = 50.0;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, 0.0, 100.0, 0.1, "%");
#else
    // Threshold
    BL_FLOAT defaultThreshold = DEFAULT_TRACKER_THRESHOLD;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, -120.0, 0.0, 0.1, "dB");
#endif

    // Mix
    BL_FLOAT defaultMix = 0.0;
    mMix = defaultMix;
    GetParam(kMix)->InitDouble("HarmoAirMix", defaultMix, -100.0, 100.0, 0.1, "%");
    
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 1 in amp is 0dB //defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");

    bool defaultUseSoftMasks = false;
    mUseSoftMasks = defaultUseSoftMasks;
    //GetParam(kUseSoftMasks)->InitInt("SmartResynth", 0, 0, 1);
    GetParam(kUseSoftMasks)->InitEnum("SmartResynth", 0, 2,
                                      "", IParam::kFlagsNone, "",
                                      "Off", "On");

#if USE_FREQ_SPLITTER
    // Improvements
    BL_FLOAT defaultSplitFreq = DEFAULT_SPLIT_FREQ;
    mSplitFreq = defaultSplitFreq;
    GetParam(kSplitFreq)->InitDouble("WetFreq",
                                     defaultSplitFreq, MIN_SPLIT_FREQ,
                                     20000.0,
                                     1.0,
                                     "Hz", IParam::kFlagMeta,
                                     "", IParam::ShapePowCurve(4.0));
    // Wet gain
    BL_FLOAT defaultWetGain = 0.0;
    mWetGain = 1.0; // 1 in amp is 0dB //defaultOutGain;
    GetParam(kWetGain)->InitDouble("WetGain", defaultWetGain,
                                   -12.0,
                                   12.0, 0.1, "dB");
#endif
}

void
Air::ApplyParams()
{
    for (int i = 0; i < 2; i++)
    {
        if (mAirProcessObjs[i] != NULL)
            mAirProcessObjs[i]->SetThreshold(mThreshold);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mAirProcessObjs[i] != NULL)
            mAirProcessObjs[i]->SetMix(mMix);
    }
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    for (int i = 0; i < 2; i++)
    {
        if (mAirProcessObjs[i] != NULL)
            mAirProcessObjs[i]->SetUseSoftMasks(mUseSoftMasks);
    }
    mLatencyChanged = true;
            
    if (mMixTextControl != NULL)
        UpdateMixTextColor(mMixTextControl, mMix);
    
    UpdateCurvesMixAlpha();

#if USE_FREQ_SPLITTER
    // Improvements
    SetSplitFreq(mSplitFreq);
    
    if (mWetGainSmoother != NULL)
        mWetGainSmoother->ResetToTargetValue(mWetGain);

#if USE_SPLIT_FREQ_SMOOTHER
    if (mSplitFreqSmoother != NULL)
        mSplitFreqSmoother->ResetToTargetValue(mSplitFreq);
#endif
    
#endif
}

void
Air::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
        
    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
    
    int bufferSize = BUFFER_SIZE;
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        int numChannels = 2;
        int numScInputs = 0;
        
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < numChannels; i++)
        {
            mAirProcessObjs[i] = new AirProcess3(bufferSize,
                                                 oversampling, freqRes,
                                                 sampleRate);
            mAirProcessObjs[i]->SetThreshold(DEFAULT_TRACKER_THRESHOLD);

#if USE_FREQ_SPLITTER
            mAirProcessObjs[i]->SetEnableSum(false);
#endif
            
            processObjs.push_back(mAirProcessObjs[i]);
        }
        
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
    }
    else
    {
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }

#if USE_FREQ_SPLITTER
    // For displaying output when using freq splitter and wet gain 
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

    //
    BL_FLOAT splitFreqs[1] = { DEFAULT_SPLIT_FREQ };
    for (int i = 0; i < 2; i++)
        mBandSplittersIn[i] =
            new CrossoverSplitterNBands4(2, splitFreqs, sampleRate);
    for (int i = 0; i < 2; i++)
        mBandSplittersOut[i] =
            new CrossoverSplitterNBands4(2, splitFreqs, sampleRate);
    
    BL_FLOAT defaultWetGain = 1.0;
    mWetGainSmoother = new ParamSmoother2(sampleRate, defaultWetGain);

    for (int i = 0; i < 2; i++)
        mInputDelays[i] = new DelayObj4(BUFFER_SIZE);

#if USE_SPLIT_FREQ_SMOOTHER
    BL_FLOAT defaultSplitFreq = DEFAULT_SPLIT_FREQ;
    BL_FLOAT splitFreqSmoothTime = DEFAULT_SPLIT_FREQ_SMOOTH_TIME_MS;

    // Adjust, because smoothing is done only once in each ProcessBlock()
    int blockSize = GetBlockSize();
    splitFreqSmoothTime /= blockSize;
    
    mSplitFreqSmoother =
        new ParamSmoother2(sampleRate, defaultSplitFreq,
                           splitFreqSmoothTime);
#endif
    
#endif
    
    //
    mLatencyChanged = true;
    UpdateLatency();

    ApplyParams();
    
    mIsInitialized = true;
}

void
Air::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    //if (BLDebug::ExitAfter(this, 10))
    //    return;

    if (mLatencyChanged)
    {
        UpdateLatency();

        mLatencyChanged = false;
    }

    mBLUtilsPlug.CheckReset(this);
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
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
    
    //
    mFftObj->Process(in, scIn, &out);

#if USE_FREQ_SPLITTER
    // Improvements
    
#if USE_SPLIT_FREQ_SMOOTHER
    if (mSplitFreqSmoother != NULL)
    {
        if (!mSplitFreqSmoother->IsStable())
        {
            BL_FLOAT splitFreq = mSplitFreqSmoother->Process();

            mSplitFreq = splitFreq;
            
            SetSplitFreq(splitFreq);
        }
    }
#endif
    
    if ((mSplitFreq >= MIN_SPLIT_FREQ) &&
        (mBandSplittersIn[0] != NULL))
    {
        // Split in
        vector<WDL_TypedBuf<BL_FLOAT> > &inLo = mTmpBuf6;
        vector<WDL_TypedBuf<BL_FLOAT> > &inHi = mTmpBuf7;
        inLo.resize(in.size());
        inHi.resize(in.size());
        for (int i = 0; i < in.size(); i++)
        {
            WDL_TypedBuf<BL_FLOAT> *resultBuf = mTmpBuf8;
            mBandSplittersIn[i]->Split(in[i], resultBuf);

            inLo[i] = resultBuf[0];
            inHi[i] = resultBuf[1];
        }

        // Split out
        vector<WDL_TypedBuf<BL_FLOAT> > &outLo = mTmpBuf9;
        vector<WDL_TypedBuf<BL_FLOAT> > &outHi = mTmpBuf10;
        outLo.resize(in.size());
        outHi.resize(in.size());
        for (int i = 0; i < out.size(); i++)
        {
            WDL_TypedBuf<BL_FLOAT> *resultBuf = mTmpBuf11;
            mBandSplittersOut[i]->Split(out[i], resultBuf);

            outLo[i] = resultBuf[0];
            outHi[i] = resultBuf[1];
        }

        // Delay input
        for (int i = 0; i < 2; i++)
        {
            if (i >= inLo.size())
                break;

            mInputDelays[i]->ProcessSamples(&inLo[i]);
        }
        
        // Apply wet gain
        BLUtilsPlug::ApplyGain(outHi, &outHi, mWetGainSmoother);

        // Sum
        BLUtilsPlug::SumSignals(inLo, outHi, &out);
    }
    
    // Genrate out curve (in any case, if we filter or not)
    vector<WDL_TypedBuf<BL_FLOAT> > &monoOut = mTmpBuf12;
    monoOut.resize(1);
    BLUtils::StereoToMono(&monoOut[0], out);
    
    vector<WDL_TypedBuf<BL_FLOAT> > &dummyOut = mTmpBuf13;
    dummyOut = monoOut;
    
    vector<WDL_TypedBuf<BL_FLOAT> > dummyScIn;
    mFftObjOut->Process(monoOut, dummyScIn, &dummyOut);
#endif

    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
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
Air::CreateControls(IGraphics *pGraphics)
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
                              "THRESHOLD",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipThreshold);
    
    // Mix
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kMixX, kMixY,
                              72, 72,
                              KNOB_FN,
                              kMix,
                              TEXTFIELD_FN,
                              "HARMO/AIR",
                              GUIHelper12::SIZE_DEFAULT,
                              &mMixTextControl,
                              true,
                              tooltipMix);
    
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
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseSoftMasksX,
                                   kUseSoftMasksY,
                                   CHECKBOX_FN, kUseSoftMasks,
                                   "SMART RESYNTH",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipSoftMasks);

#if USE_FREQ_SPLITTER
    // Improvements
    //
    
    // Split freq
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSplitFreqX, kSplitFreqY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSplitFreq,
                              TEXTFIELD_FN,
                              "WET FREQ",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSplitFreq);
    
    // Wet gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kWetGainX, kWetGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kWetGain,
                              TEXTFIELD_FN,
                              "WET GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipWetGain);
#endif
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    //, GUIHelper12::BOTTOM);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //                           kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM,
                                 tooltipHelp);
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    //
    mControlsCreated = true;
}

void
Air::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Air::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
        
    int bufferSize = BUFFER_SIZE;
        
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mFftObj != NULL)
    {
      // Called when we restart the playback
      // The cursor position may have changed
      // Then we must reset
        mFftObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
    }
        
    for (int i = 0; i < 2; i++)
    {
      if (mAirProcessObjs[i] != NULL)
          mAirProcessObjs[i]->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
    }
    
    UpdateLatency();

    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    if (mHarmoCurve != NULL)
        mHarmoCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mAirCurve != NULL)
        mAirCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mSumCurve != NULL)
        mSumCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);

    // Curves
    if (mAirCurveSmooth != NULL)
        mAirCurveSmooth->Reset(sampleRate);
    if (mHarmoCurveSmooth != NULL)
        mHarmoCurveSmooth->Reset(sampleRate);
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->Reset(sampleRate);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);

#if USE_FREQ_SPLITTER
    // Improvements
    for (int i = 0; i < 2; i++)
    {
        if (mBandSplittersIn[i] != NULL)
            mBandSplittersIn[i]->Reset(sampleRate);
    }

    for (int i = 0; i < 2; i++)
    {
        if (mBandSplittersOut[i] != NULL)
            mBandSplittersOut[i]->Reset(sampleRate);
    }
    
    if (mWetGainSmoother != NULL)
        mWetGainSmoother->Reset(sampleRate);

    for (int i = 0; i < 2; i++)
        mInputDelays[i]->Reset();

    if (mFftObjOut != NULL)
        mFftObjOut->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);

#if USE_SPLIT_FREQ_SMOOTHER
    if (mSplitFreqSmoother != NULL)
    {
        BL_FLOAT splitFreqSmoothTime = DEFAULT_SPLIT_FREQ_SMOOTH_TIME_MS;
        
        // Adjust, because smoothing is done only once in each ProcessBlock()
        int blockSize = GetBlockSize();
        splitFreqSmoothTime /= blockSize;
    
        mSplitFreqSmoother->Reset(sampleRate, splitFreqSmoothTime);
    }
#endif
    
#endif

    UpdateCurvesSmoothFactor();
        
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

// NOTE: OnIdle() is called from the GUI thread
void
Air::OnIdle()
{
    if (!mUIOpened)
        return;
    
    ENTER_PARAMS_MUTEX;

    mAirCurveSmooth->PullData();
    mHarmoCurveSmooth->PullData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->PullData();
    
    LEAVE_PARAMS_MUTEX;

    // We don't need mutex anymore now
    mAirCurveSmooth->ApplyData();
    mHarmoCurveSmooth->ApplyData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->ApplyData();
    
}

void
Air::OnParamChange(int paramIdx)
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
                if (mAirProcessObjs[i] != NULL)
                    mAirProcessObjs[i]->SetThreshold(threshold);
            }
        }
        break;
            
        case kMix:
        {
            BL_FLOAT mix = GetParam(kMix)->Value();
            mix /= 100.0;
            
            // Reverse [-1, 1] (to have the noise on the right)
            mix = -mix;
            
            mMix = mix;
            for (int i = 0; i < 2; i++)
            {
                if (mAirProcessObjs[i] != NULL)
                    mAirProcessObjs[i]->SetMix(mix);
            }
            
            if (mMixTextControl != NULL)
                UpdateMixTextColor(mMixTextControl, mMix);
            
            UpdateCurvesMixAlpha();
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

        case kUseSoftMasks:
        {
            int value = GetParam(kUseSoftMasks)->Value();
            bool useSoftMasksFlag = (value == 1);
            mUseSoftMasks = useSoftMasksFlag;
      
            for (int i = 0; i < 2; i++)
            {
                if (mAirProcessObjs[i] != NULL)
                    mAirProcessObjs[i]->SetUseSoftMasks(mUseSoftMasks);
            }

            mLatencyChanged = true;
        }
        break;

#if USE_FREQ_SPLITTER
        // Improvements
        case kSplitFreq:
        {
            BL_FLOAT splitFreq = GetParam(paramIdx)->Value();
            mSplitFreq = splitFreq;
            
#if USE_SPLIT_FREQ_SMOOTHER
            if (mSplitFreqSmoother != NULL)
                mSplitFreqSmoother->SetTargetValue(mSplitFreq);
#else
            SetSplitFreq(mSplitFreq);
#endif
        }
        break;
      
        case kWetGain:
        {
            BL_FLOAT gain = GetParam(paramIdx)->DBToAmp();
            mWetGain = gain;
            
            if (mWetGainSmoother != NULL)
                mWetGainSmoother->SetTargetValue(gain);
        }
        break;
#endif
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Air::UpdateMixTextColor(ITextControl *textControl, BL_FLOAT param)
{
    if (mGUIHelper == NULL)
        return;
    
    IColor valueTextColor;
    mGUIHelper->GetValueTextColor(&valueTextColor);
    
#define RANGE 0.25
    
    param = (param + 1.0)/2.0;
    
    BL_FLOAT t = 1.0 - 2.0*std::fabs(param - 0.5);
    
    if (t >= RANGE)
    {
        textControl->SetTextColor(valueTextColor);
        
        return;
    }
    
    // Normalize t between 0 and 1
    t = t / RANGE;
    
    // Red
    IColor failColor(255, 255, 0, 0);
    
    IColor resultColor;
    resultColor.A = valueTextColor.A;
    resultColor.R = (1.0 - t)*failColor.R + t*valueTextColor.R;
    resultColor.G = (1.0 - t)*failColor.G + t*valueTextColor.G;
    resultColor.B = (1.0 - t)*failColor.B + t*valueTextColor.B;
    
    textControl->SetTextColor(resultColor);
}

void
Air::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Air::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    mGraph = NULL;
    mMixTextControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
}

void
Air::UpdateLatency()
{
    // Latency
    int blockSize = GetBlockSize();
    int latency = 0;
    
    if (mFftObj != NULL)
        latency += mFftObj->ComputeLatency(blockSize);
    
    if (mAirProcessObjs[0] != NULL)
    {
        int lat0 = mAirProcessObjs[0]->GetLatency();
        latency += lat0;
    }
    
    SetLatency(latency);

    mLatencyChanged = false;

    // Update delay ovjes with latency
#if USE_FREQ_SPLITTER
    for (int i = 0; i < 2; i++)
    {
        if (mInputDelays[i] != NULL)
            mInputDelays[i]->SetDelay(latency);
    }
#endif
}

void
Air::CreateGraphAxes()
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
Air::CreateGraphCurves()
{
    //BL_FLOAT sampleRate = GetSampleRate();
#define REF_SAMPLERATE 44100.0
    BL_FLOAT curveSmoothCoeff =
        ParamSmoother2::ComputeSmoothFactor(CURVE_SMOOTH_COEFF_MS, REF_SAMPLERATE);
    
    if (mAirCurve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
    
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        // Air curve
        int airColor[4];
        //mGUIHelper->GetGraphCurveColorBlue(airColor);
        mGUIHelper->GetGraphCurveColorFakeCyan(airColor);
        
        mAirCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mAirCurve->SetDescription("air", descrColor);
        mAirCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5); //
        mAirCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mAirCurve->SetFill(true);
        mAirCurve->SetFillAlpha(fillAlpha);
        mAirCurve->SetColor(airColor[0], airColor[1], airColor[2]);
        mAirCurve->SetLineWidth(2.0);
        
        mAirCurveSmooth = new SmoothCurveDB(mAirCurve,
                                            curveSmoothCoeff,
                                            GRAPH_CURVE_NUM_VALUES,
                                            GRAPH_MIN_DB,
                                            GRAPH_MIN_DB, GRAPH_MAX_DB,
                                            sampleRate);
    
        // Harmo curve
        int harmoColor[4];
        //mGUIHelper->GetGraphCurveColorGreen(harmoColor);
        mGUIHelper->GetGraphCurveColorPurple(harmoColor);
        
        mHarmoCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mHarmoCurve->SetDescription("harmo", descrColor);
        mHarmoCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mHarmoCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mHarmoCurve->SetFill(true);
        mHarmoCurve->SetFillAlpha(fillAlpha);
        mHarmoCurve->SetColor(harmoColor[0], harmoColor[1], harmoColor[2]);
        
        mHarmoCurveSmooth = new SmoothCurveDB(mHarmoCurve,
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
                                            curveSmoothCoeff,
                                            GRAPH_CURVE_NUM_VALUES,
                                            GRAPH_MIN_DB,
                                            GRAPH_MIN_DB, GRAPH_MAX_DB,
                                            sampleRate);
#endif
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
        
    mAirCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mAirCurve);
    
    mHarmoCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mHarmoCurve);

    // Add sum curve over all other curves
#if SHOW_SUM_CURVE
    mSumCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mSumCurve);
#endif

    UpdateCurvesSmoothFactor();
}

void
Air::UpdateCurves()
{
    if (!mUIOpened)
        return;
    
    WDL_TypedBuf<BL_FLOAT> &air = mTmpBuf3;
    mAirProcessObjs[0]->GetNoise(&air);
    mAirCurveSmooth->SetValues(air);
    
    WDL_TypedBuf<BL_FLOAT> &harmo = mTmpBuf4;
    mAirProcessObjs[0]->GetHarmo(&harmo);
    mHarmoCurveSmooth->SetValues(harmo);
    
    if (mSumCurveSmooth != NULL)
    {
#if !USE_FREQ_SPLITTER
        WDL_TypedBuf<BL_FLOAT> &sum = mTmpBuf5;
        mAirProcessObjs[0]->GetSum(&sum);
        mSumCurveSmooth->SetValues(sum);
#else
        WDL_TypedBuf<BL_FLOAT> &output = mTmpBuf4;
        mBufObjOut->GetBuffer(&output);
        mSumCurveSmooth->SetValues(output);
#endif
    }

    // Lock free
    mAirCurveSmooth->PushData();
    mHarmoCurveSmooth->PushData();
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->PushData();
}

void
Air::UpdateCurvesMixAlpha()
{
    // knob left: 1.0
    // knob middle: 0
    // knob right: -1
    
    if (mGUIHelper == NULL)
        // Not yet initialized
        return;
    
    float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
    
    if (mMix <= 0.0)
    {
        if (mHarmoCurve != NULL)
        {
            BL_FLOAT t = 1.0 + mMix;
            
            t = BLUtils::ApplyParamShape(t, (BL_FLOAT)2.0);
            
            mHarmoCurve->SetAlpha(t);
            mHarmoCurve->SetFillAlpha(t*fillAlpha);
        }
        
        if (mAirCurve != NULL)
        {
            mAirCurve->SetAlpha(1.0);
            mAirCurve->SetFillAlpha(fillAlpha);
        }
    }
    
    if (mMix >= 0.0)
    {
        if (mAirCurve != NULL)
        {
            BL_FLOAT t = (1.0 - mMix);
            
            t = BLUtils::ApplyParamShape(t, (BL_FLOAT)2.0);
            
            mAirCurve->SetAlpha(t);
            mAirCurve->SetFillAlpha(t*fillAlpha);
        }
        
        if (mHarmoCurve != NULL)
        {
            mHarmoCurve->SetAlpha(1.0);
            mHarmoCurve->SetFillAlpha(fillAlpha);
        }
    }
}

void
Air::SetSplitFreq(BL_FLOAT freq)
{  
    if (freq >= MIN_SPLIT_FREQ)
    {
        for (int i = 0; i < 2; i++)
        {
            if (mBandSplittersIn[i] != NULL)
                mBandSplittersIn[i]->SetCutoffFreq(0, freq);
        }

        for (int i = 0; i < 2; i++)
        {
            if (mBandSplittersOut[i] != NULL)
                mBandSplittersOut[i]->SetCutoffFreq(0, freq);
        }
    }
}

void
Air::UpdateCurvesSmoothFactor()
{
#if 0
    // Disabled!
    //
    // No need to use smooth factor adjust depending on block size,
    // since we use SmoothCurveDB::OPTIM_LOCK_FREE

#define REF_BLOCK_SIZE 512.0
    
    BL_FLOAT smoothingTimeMs = CURVE_SMOOTH_COEFF_MS;

    int blockSize = GetBlockSize();

    BL_FLOAT blockSizeCoeff = REF_BLOCK_SIZE/(BL_FLOAT)blockSize;

    // Do not need to smooth more for small block size,
    // since SmoothCurveDB::OPTIM_LOCK_FREE
    //if (blockSizeCoeff > 1.0)
    //    blockSizeCoeff = 1.0;
    
    smoothingTimeMs *= blockSizeCoeff;

    BL_FLOAT sampleRate = GetSampleRate();
    BL_FLOAT smoothFactor =
        ParamSmoother2::ComputeSmoothFactor(smoothingTimeMs, sampleRate);
    
    if (mAirCurveSmooth != NULL)
        mAirCurveSmooth->Reset(sampleRate, smoothFactor);
    if (mHarmoCurveSmooth != NULL)
        mHarmoCurveSmooth->Reset(sampleRate, smoothFactor);
    if (mSumCurveSmooth != NULL)
        mSumCurveSmooth->Reset(sampleRate, smoothFactor);
#endif
}
