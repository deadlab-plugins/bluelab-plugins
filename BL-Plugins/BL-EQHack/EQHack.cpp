#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <FftProcessObj16.h>

#include <EQHackFftObj2.h>
#include <FftProcessBufObj.h>

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <AirProcess2.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphControl12.h>
#include <GraphFreqAxis2.h>
#include <GraphAxis2.h>
#include <GraphAmpAxis.h>

#include <GraphCurve5.h>
#include <SmoothCurveDB.h>

#include <Scale.h>

#include <CMA2Smoother.h>

#include <IBLSwitchControl.h>

#include <ParamSmoother2.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "EQHack.h"

#include "IPlug_include_in_plug_src.h"
#include "IGraphics_include_in_plug_src.h"

//
#define BUFFER_SIZE 2048
#define OVERSAMPLING 4 // With 32, result is better (less low freq vibrations in noise)
#define FREQ_RES 1
#define KEEP_SYNTHESIS_ENERGY 0
#define VARIABLE_HANNING 1

//#define GRAPH_MIN_DB -119.0 // Take care of the noise/harmo bottom dB
//#define GRAPH_MAX_DB 10.0

// TEST
#define GRAPH_MIN_DB -40.0 // Take care of the noise/harmo bottom dB
#define GRAPH_MAX_DB 40.0

// NOTE: maybe decrease a bit this value, to 512 or 256
//#define GRAPH_CURVE_NUM_VALUES 1024
#define GRAPH_CURVE_NUM_VALUES 512

//#define CURVE_SMOOTH_COEFF 0.95
//#define CURVE_SMOOTH_COEFF_SLOW 0.975

//#define CURVE_SMOOTH_COEFF_SLOW 0.985
//#define CURVE_SMOOTH_COEFF_FAST 0.9

#define UPDATE_CURVES_ON_IDLE 1 //0

#if !UPDATE_CURVES_ON_IDLE
// See Air for details
#define CURVE_SMOOTH_COEFF_SLOW_MS 9.42
#define CURVE_SMOOTH_COEFF_FAST_MS 1.35
#else
#define CURVE_SMOOTH_COEFF_SLOW_MS 2.5
#define CURVE_SMOOTH_COEFF_FAST_MS 0.3
#endif

#define STRENGTH_COEFF 4.0

// Logic equalizer max dB is +/- 24dB
// For Protools, this is +/- 18dB
#define MIN_DB -40.0
#define MAX_DB 40.0

#define APPLY_MIN_DB -100.0
#define APPLY_MAX_DB 0.0

#define DEFAULT_VALUE_RATIO 1.0

// Origin learn and guess too a very long time to stabilize
#define SMOOTH_FASTER 1

// Display instant curve in guess mode
#define DISPLAY_INSTANT_GUESS 1

#define SMOOTHER_WINDOW_SIZE 10

// Optimization
#define OPTIM_SKIP_IFFT 1

#define USE_LEGACY_LOCK 1

#if 0
// BUG: preset factory reset doesn't clear the curve 

// NOTE: on protools, need "allZero" flag, othersie the learn curve will return slightly to zero

NOTE: for debugging au, launch:
/usr/bin/auvaltool
with arguments:
-v aufx 'tqz9' 'BlLa'

TODO: draw the main horiz axis in another color or biggest depth compared to the other grid lines

USER REQUEST: "real time EQHack"/Analog EQ plugin + curve
fredguest@mac.com (mail support)
"What I mean by "real time" is using EQHACK without audio files, but rather an incoming audio stream. The use case would be if you are mixing virtual instrument tracks that have not been printed to audio files yet, and you are tuning them analog EQ emulations while arranging/mixing. "
AND if possible: use a single track and side chain pre/post fx
(no need to use a second track) Niko: may be possible in some hosts.
SUMMARY: use a "monitor" mode, to process eqhack without audio region
"I’m on Logic Pro X. It’s pretty easy to sidechain tracks in Logic, and I believe you can side chain a track to itself, so I think it might work!"
=> send him a mail when ready?
Fred => See the Bertom Analyzer which makes exactly what we want: https://www.bertomaudio.com/eqca.html
Sandwitch a plugin to analyze between two Bertom EQ Curve Analyzer
Niko - 20200921: tests sidechain => conclusion: for the moment, can t get the pre-fx side chain from the same track
BUG: on Reaper + AU, input is null and side chain is filled (or reverse, depending on the place in the code)
BUG: Logic(my version): if no sidechain is connected, then Logic copy the side input data to the sidechain data
(there is currently a workaround in IPlugBase, using memcmp)

IDEA: try with kick tracks => does the curve dimishes between kicks ? => in this case freeze the curve when the sound level is very low
"yeah there are a lot of tools that can be used for similar purpose like isotope matchEQ, IK multimedia, TDR plugins, new fangled audio "

NOTE: other similar plugins: "MatchEQ", "eq matching"

Emrah: "TDR NOVA GE can match/learn the EQ dynamically and match"

NOTE: StudioOne, Sierra, VST: side chain does not enter in the plugins
=> must use AU or VST3 for sidechain

NOTE: Cubase Pro 10, Sierra: Cubase supports side chain for third party plugins only in VST3 format

TODO: tweak well allZero threshold EPS (test with Confusius-Stereo-EQ1000)
(Problem on Orion, Windows only ?
 maybe the two clip were a bit shifted, or small sidechain delay...)
(checked with Reaper, Logic => no problem)
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipMode = "Mode - Learn existing EQ, guess learned EQ or reset";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";

enum EParams
{
    kMode = 0,
    kMonitor,

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

    kRadioButtonsModeX = 597,
    kRadioButtonsModeY = 95,
    kRadioButtonsModeVSize = 68,
    kRadioButtonsModeNumButtons = 3,

    kCheckboxMonitorX = 596,
    kCheckboxMonitorY = 220
};


//
EQHack::EQHack(const InstanceInfo &info)
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

EQHack::~EQHack()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mFftObj != NULL)
        delete mFftObj;

    if (mEQHackObj != NULL)
        delete mEQHackObj;
    
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
    if (mEQLearnCurve != NULL)
        delete mEQLearnCurve;
    
    if (mEQLearnCurveSmooth != NULL)
        delete mEQLearnCurveSmooth;
    
    if (mEQInstantCurve != NULL)
        delete mEQInstantCurve;
    
    if (mEQInstantCurveSmooth != NULL)
        delete mEQInstantCurveSmooth;
    
    if (mEQGuessCurve != NULL)
        delete mEQGuessCurve;
    
    if (mEQGuessCurveSmooth != NULL)
        delete mEQGuessCurveSmooth;

    for (int i = 0; i < 2; i++)
    {
        if (mSmoothers[i] != NULL)
            delete mSmoothers[i];
    }
}

IGraphics *
EQHack::MyMakeGraphics()
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
EQHack::MyMakeLayout(IGraphics *pGraphics)
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
EQHack::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();

    mPrevSampleRate = GetSampleRate();
    
    mFftObj = NULL;
    mEQHackObj = NULL;
    
    mGUIHelper = NULL;
    
    mGraph = NULL;
    
    mFreqAxis = NULL;
    mHAxis = NULL;

    mAmpAxis = NULL;
    mVAxis = NULL;
    
    mEQLearnCurve = NULL;
    mEQLearnCurveSmooth = NULL;
    
    mEQInstantCurve = NULL;
    mEQInstantCurveSmooth = NULL;
    
    mEQGuessCurve = NULL;
    mEQGuessCurveSmooth = NULL;

    for (int i = 0; i < 2; i++)
        mSmoothers[i] = NULL;

    mMonitorEnabled = false;
    mMonitorControl = NULL;
    mIsPlaying = false;

    mMustUpdateCurvesLF = false;
    mUpdateLearnCurveLF = false;
}

void
EQHack::InitParams()
{
    //arguments are: name, defaultVal, minVal, maxVal, step, label
    EQHackPluginInterface::Mode defaultMode = EQHackPluginInterface::LEARN;
    mMode = defaultMode;
  
    //GetParam(kMode)->InitInt("Mode", (int)defaultMode, 0, 2);
    GetParam(kMode)->InitEnum("Mode", (int)defaultMode, 2,
                              "", IParam::kFlagsNone, "",
                              "Learn", "Guess", "Reset");

    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
}

void
EQHack::ApplyParams()
{
    ModeChanged();
    
    if (mEQLearnCurveSmooth != NULL)
        mEQLearnCurveSmooth->SetValues(mLearnCurve);
}

void
EQHack::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        int numChannels = 1;
        int numScInputs = 1;
        
        vector<ProcessObj *> processObjs;
        mEQHackObj = new EQHackFftObj2(BUFFER_SIZE, OVERSAMPLING,
                                       FREQ_RES, sampleRate);
        processObjs.push_back(mEQHackObj);
        
        
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
        mFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }

    for (int i = 0; i < 2; i++)
        mSmoothers[i] = new CMA2Smoother(GRAPH_CURVE_NUM_VALUES, //BUFFER_SIZE,
                                         SMOOTHER_WINDOW_SIZE);
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
EQHack::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    //if (BLDebug::ExitAfter(this, 10))
    //    return;
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    if (mGraph != NULL)
        mGraph->Lock();
    
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
        {
            mGraph->Unlock();
            mGraph->PushAllData();
        }
        
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

    mIsPlaying = IsTransportPlaying();
    if (mIsPlaying || mMonitorEnabled)
    {
        if (mFftObj == NULL)
        {
            mGraph->Unlock();
            mGraph->PushAllData();
            
            return;
        }
        
        // Convert signal to mono
        WDL_TypedBuf<BL_FLOAT> &monoIn = mTmpBuf8;
        if (in.size() > 0)
        {
            monoIn = in[0];
      
            if (in.size() > 1)
                BLUtils::StereoToMono(&monoIn, in[0], in[1]);
        }

        // Convert sc to mono
        WDL_TypedBuf<BL_FLOAT> &monoScIn = mTmpBuf9;
        if (scIn.size() > 0)
            // At least one side chain !
        {
            monoScIn = scIn[0];
            if (scIn.size() > 1)
            {
                BLUtils::StereoToMono(&monoScIn, scIn[0], scIn[1]);
            }
        }
  
        // Analyse or apply
        if (monoScIn.GetSize() > 0)
        {
            vector< WDL_TypedBuf<BL_FLOAT> > &monoInVec = mTmpBuf10;
            monoInVec.resize(1);
            monoInVec[0] = monoIn;
            
            vector< WDL_TypedBuf<BL_FLOAT> > &monoScInVec = mTmpBuf11;
            monoScInVec.resize(1);
            monoScInVec[0] = monoScIn;
      
            mFftObj->Process(monoInVec, monoScInVec, NULL);
        }
  
        WDL_TypedBuf<BL_FLOAT> &diffBuf = mTmpBuf12;
        mEQHackObj->GetDiffBuffer(&diffBuf);
    
        // Do not update the learn curve is side chain is not connected.
        // This will avoid reseting the learn curve when we use the plugin
        // only for applying a recorded learn curve
        bool updateLearnCurve = (scIn.size() > 0);

#if !UPDATE_CURVES_ON_IDLE
        UpdateCurves(&diffBuf, updateLearnCurve);
#else
        // Will need to update later, to avoid data race
        mMustUpdateCurvesLF = true;
        mUpdateLearnCurveLF = updateLearnCurve;
        mUpdateCurveBufLF = diffBuf;
#endif
    }

    // FIX: (in Orion for example)
    // Avoid playing the last part of samples indefinitely
    // when stopping the transport (with new AllZero)
    // (in fact, prev "allZero + bypass" was not safe)
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }

    if (mGraph != NULL)
    {
        mGraph->Unlock();
        mGraph->PushAllData();
    }
    
    BL_PROFILE_END;
}

void
EQHack::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
 
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);

#if !UPDATE_CURVES_ON_IDLE
    mGraph->SetUseLegacyLock(true);
#endif
    
    // Separator
    IColor sepIColor;
    mGUIHelper->GetGraphSeparatorColor(&sepIColor);
    int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
    mGraph->SetSeparatorY0(2.0, sepColor);

    // Separator on the right
    mGraph->SetSeparatorX1(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurves();
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    //int offsetY = graphSize[1];

    const char *modeRadioLabels[] = { "LEARN", "GUESS", "RESET"};
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeX,
                                   kRadioButtonsModeY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsModeNumButtons,
                                   kRadioButtonsModeVSize,
                                   kMode,
                                   false,
                                   "MODE",
                                   EAlign::Near,
                                   EAlign::Near,
                                   modeRadioLabels,
                                   tooltipMode);

    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX,
                                                     kCheckboxMonitorY,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT, true,
                                                     tooltipMonitor);
    
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
EQHack::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
EQHack::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;
      
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    if (sampleRate != mPrevSampleRate)
    {
        mPrevSampleRate = sampleRate;

        // Do not clear values, otherwise in Guess mode,
        // the learn curve would totally disappear here
        //if (mEQLearnCurveSmooth != NULL)
        //    mEQLearnCurveSmooth->ClearValues();

        // Reset the lear curve if the sample rate changes
        // Ideally, we would have to resample the curve, but here it is more
        // practical to just reset it
        ResetLearnCurve();
        
        if (mEQInstantCurveSmooth != NULL)
            mEQInstantCurveSmooth->ClearValues();
        if (mEQGuessCurveSmooth != NULL)
            mEQGuessCurveSmooth->ClearValues();

        // Force reset mode before UpdateGraph()
        EQHackPluginInterface::Mode prevMode = mMode;
        mMode = EQHackPluginInterface::RESET;
        UpdateCurves(NULL, false);
        mMode = prevMode;
    }

    if (mFreqAxis != NULL)
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    
    if (mEQLearnCurve != NULL)
        mEQLearnCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mEQInstantCurve != NULL)
        mEQInstantCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
    if (mEQGuessCurve != NULL)
        mEQGuessCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);

    if (mEQLearnCurveSmooth != NULL)
        mEQLearnCurveSmooth->Reset(sampleRate);
    if (mEQInstantCurveSmooth != NULL)
        mEQInstantCurveSmooth->Reset(sampleRate);
    if (mEQGuessCurveSmooth != NULL)
        mEQGuessCurveSmooth->Reset(sampleRate);
    
    UpdateCurvesSmoothFactor();
        
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
EQHack::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kMode:
        {
            EQHackPluginInterface::Mode mode =
            (EQHackPluginInterface::Mode)GetParam(paramIdx)->Int();
            if (mode != mMode)
            {
                mMode = mode;
	    
                ModeChanged();
            }
        }
        break;
        
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            
            mMonitorEnabled = (value == 1);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
EQHack::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
EQHack::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;

    mMonitorControl = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
EQHack::OnIdle()
{
    ENTER_PARAMS_MUTEX;
    
    if (mUIOpened)
    {
        if (mMonitorControl != NULL)
        {
            bool prevDisabled = mMonitorControl->IsDisabled();
            if (mIsPlaying != prevDisabled) 
                mMonitorControl->SetDisabled(mIsPlaying);
        }
    }

#if UPDATE_CURVES_ON_IDLE
    // Update curves in the GUI thread, to avoid data race
    // (and it saves performances!)
    if (mMustUpdateCurvesLF)
    {
        UpdateCurves(&mUpdateCurveBufLF, mUpdateLearnCurveLF);
        
        mMustUpdateCurvesLF = false;
    }
#endif
    
    LEAVE_PARAMS_MUTEX;
}

void
EQHack::CreateGraphAxes()
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
        mAmpAxis = new GraphAmpAxis(true, GraphAmpAxis::DENSITY_10DB);
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

    // Added dB offsets to avoid displaying the extremity axis labels
    mAmpAxis->Init(mVAxis, mGUIHelper,
                   GRAPH_MIN_DB + 0.1,
                   GRAPH_MAX_DB - 0.1, graphWidth);
}

void
EQHack::CreateGraphCurves()
{
#define REF_SAMPLERATE 44100.0
    BL_FLOAT curveSmoothCoeffSlow =
        ParamSmoother2::ComputeSmoothFactor(CURVE_SMOOTH_COEFF_SLOW_MS,
                                            REF_SAMPLERATE);
    BL_FLOAT curveSmoothCoeffFast =
        ParamSmoother2::ComputeSmoothFactor(CURVE_SMOOTH_COEFF_FAST_MS,
                                            REF_SAMPLERATE);
    
    if (mEQLearnCurve == NULL)
        // Not yet created
    {
        BL_FLOAT sampleRate = GetSampleRate();
        
        int descrColor[4];
        mGUIHelper->GetGraphCurveDescriptionColor(descrColor);
        
        // EQHack curve
        int eqLearnColor[4];
        mGUIHelper->GetGraphCurveColorBlue(eqLearnColor);

        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        
        mEQLearnCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mEQLearnCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mEQLearnCurve->SetDescription("EQ (learn)", descrColor);
        mEQLearnCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mEQLearnCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mEQLearnCurve->SetFill(true); // NEW
        mEQLearnCurve->SetFillAlpha(fillAlpha); // NEW
        mEQLearnCurve->SetColor(eqLearnColor[0], eqLearnColor[1], eqLearnColor[2]);
        mEQLearnCurve->SetLineWidth(2.0);
        
        mEQLearnCurveSmooth = new SmoothCurveDB(mEQLearnCurve,
                                                //CURVE_SMOOTH_COEFF_SLOW,
                                                curveSmoothCoeffSlow,
                                                GRAPH_CURVE_NUM_VALUES,
                                                1.0, // 1 in amp is 0 in dB (middle)
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
        mEQLearnCurveSmooth->SetUseLegacyLock(USE_LEGACY_LOCK);
    
        // Harmo curve
        int instantColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(instantColor);
        
        mEQInstantCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mEQInstantCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mEQInstantCurve->SetDescription("EQ (instant)", descrColor);
        mEQInstantCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mEQInstantCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mEQInstantCurve->SetColor(instantColor[0], instantColor[1], instantColor[2]);
        
        mEQInstantCurveSmooth = new SmoothCurveDB(mEQInstantCurve,
                                                  //CURVE_SMOOTH_COEFF_FAST,
                                                  curveSmoothCoeffFast,
                                                  GRAPH_CURVE_NUM_VALUES,
                                                  1.0, // 1 in amp is 0 in dB (middle)
                                                  GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                  sampleRate);
        mEQInstantCurveSmooth->SetUseLegacyLock(USE_LEGACY_LOCK);
    
        int guessColor[4];
        mGUIHelper->GetGraphCurveColorGreen(guessColor);
        
        mEQGuessCurve = new GraphCurve5(GRAPH_CURVE_NUM_VALUES);
        mEQGuessCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mEQGuessCurve->SetDescription("EQ (guess)", descrColor);
        mEQGuessCurve->SetXScale(Scale::LOG, 0.0, sampleRate*0.5);
        mEQGuessCurve->SetYScale(Scale::DB, GRAPH_MIN_DB, GRAPH_MAX_DB);
        mEQGuessCurve->SetColor(guessColor[0], guessColor[1], guessColor[2]);
        
        mEQGuessCurveSmooth = new SmoothCurveDB(mEQGuessCurve,
                                                //CURVE_SMOOTH_COEFF_SLOW,
                                                curveSmoothCoeffSlow,
                                                GRAPH_CURVE_NUM_VALUES,
                                                1.0, // 1 in amp is 0 in dB (middle)
                                                GRAPH_MIN_DB, GRAPH_MAX_DB,
                                                sampleRate);
        mEQGuessCurveSmooth->SetUseLegacyLock(USE_LEGACY_LOCK);
    }

    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mEQLearnCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mEQLearnCurve);

    mEQInstantCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mEQInstantCurve);
    
    mEQGuessCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mEQGuessCurve);

    UpdateCurvesSmoothFactor();
}

void
EQHack::UpdateCurves(const WDL_TypedBuf<BL_GUI_FLOAT> *currentEQ0,
                     bool updateLearnCurve)
{
    if (!mUIOpened)
        return;
  
    if (currentEQ0 == NULL)
        return;
    
    if (mMode == EQHackPluginInterface::LEARN)
    {
        if (mEQInstantCurveSmooth != NULL)
            mEQInstantCurveSmooth->SetValues(*currentEQ0);
        
        if (mEQLearnCurveSmooth != NULL)
        {
            if (updateLearnCurve)
            {
                WDL_TypedBuf<BL_GUI_FLOAT> &currentEQ = mTmpBuf4;
                currentEQ = *currentEQ0;
                
                mEQLearnCurveSmooth->SetValues(currentEQ);
                mEQLearnCurveSmooth->GetHistogramValuesDB(&mLearnCurve);
            }
        }
    }
    else if (mMode == EQHackPluginInterface::GUESS)
    {
        if (mEQInstantCurveSmooth != NULL)
        {
            mEQInstantCurveSmooth->SetValues(*currentEQ0);

            // Substract
            WDL_TypedBuf<BL_FLOAT> &currentEQ = mTmpBuf7;
            mEQInstantCurveSmooth->GetHistogramValuesDB(&currentEQ);

            if (mLearnCurve.GetSize() == currentEQ.GetSize())
            {
                BLUtils::SubstractValues(&currentEQ, mLearnCurve);
                BLUtils::MultValues(&currentEQ, (BL_FLOAT)-1.0);
                BLUtils::AddValues(&currentEQ, (BL_FLOAT)0.5); // Hack
            
                //mEQInstantCurve->SetValues5(currentEQ, false, false);
                mEQInstantCurve->SetValues5(currentEQ, false, false);
            }
        }
        
        if (mEQGuessCurveSmooth != NULL)
        {
            WDL_TypedBuf<BL_GUI_FLOAT> &currentEQ = mTmpBuf5;
            currentEQ = *currentEQ0;
            
            mEQGuessCurveSmooth->SetValues(currentEQ);

            WDL_TypedBuf<BL_FLOAT> &learnCurve = mTmpBuf6;
            mEQGuessCurveSmooth->GetHistogramValuesDB(&learnCurve);

            if (mLearnCurve.GetSize() == learnCurve.GetSize())
            {
                // Substract
                BLUtils::SubstractValues(&learnCurve, mLearnCurve);
                BLUtils::MultValues(&learnCurve, (BL_FLOAT)-1.0);
                BLUtils::AddValues(&learnCurve, (BL_FLOAT)0.5); // Hack
                //mEQGuessCurve->SetValues5(learnCurve, false, false);
                mEQGuessCurve->SetValues5(learnCurve, false, false);
            }
            
            // Compute the guess curve color, depending on the confidence of the match
            BL_FLOAT matchCoeff = 0.0;
            if (mLearnCurve.GetSize() == learnCurve.GetSize())
            {
                matchCoeff = 
                    BLUtils::ComputeCurveMatchCoeff2(learnCurve.Get(),
                                                     mLearnCurve.Get(),
                                                     learnCurve.GetSize());
            }
            
            unsigned int matchColor0[3] = { 255, 64, 64 };
            unsigned int matchColor1[3] = { 64, 255, 64 };
            
            unsigned int resultColor[3];
            for (int i = 0; i < 3; i++)
                resultColor[i] = (1.0 - matchCoeff)*matchColor0[i] +
                    matchCoeff*matchColor1[i];
            
            if (mEQGuessCurve != NULL)
                mEQGuessCurve->SetColor(resultColor[0],
                                        resultColor[1],
                                        resultColor[2]);
        }
    }
}

void
EQHack::ModeChanged(bool fromUnserial)
{
    // Could have been serialized and restored...
    
    if (mEQInstantCurveSmooth != NULL)
        mEQInstantCurveSmooth->ClearValues();
    if (mEQGuessCurveSmooth != NULL)
        mEQGuessCurveSmooth->ClearValues();

    if (mMode == EQHackPluginInterface::LEARN)
    {
        SetCurveSpecsModeLearn();

        if (!fromUnserial)
            ResetLearnCurve();
    }
    else if (mMode == EQHackPluginInterface::GUESS)
    {
        SetCurveSpecsModeGuess();
        
        if (mEQLearnCurveSmooth != NULL)
            mEQLearnCurveSmooth->SetValues(mLearnCurve, true);
    }
    else if (mMode == EQHackPluginInterface::RESET)
    {
        if (mEQLearnCurveSmooth != NULL)
            mEQLearnCurveSmooth->ClearValues();
        if (mEQInstantCurveSmooth != NULL)
            mEQInstantCurveSmooth->ClearValues();
        if (mEQGuessCurveSmooth != NULL)
            mEQGuessCurveSmooth->ClearValues();

        ResetLearnCurve();
    }
  
    UpdateCurves(NULL, false);
}

void
EQHack::SetCurveSpecsModeLearn()
{
    // Avg EQ Hack
    if (mEQGuessCurve != NULL)
        mEQGuessCurve->SetAlpha(0.0);
  
    if (mEQLearnCurve != NULL)
    {
        mEQLearnCurve->SetAlpha(1.0);
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        mEQLearnCurve->SetFillAlpha(fillAlpha);
    }
}

// Unused
void
EQHack::SetCurveSpecsModeApply()
{
    // Avg EQ Hack
    if (mEQGuessCurve != NULL)
        mEQGuessCurve->SetAlpha(0.0);
  
    if (mEQLearnCurve != NULL)
    {
        mEQLearnCurve->SetAlpha(1.0);
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        mEQLearnCurve->SetFillAlpha(fillAlpha);
    }
}

// Unused
void
EQHack::SetCurveSpecsModeApplyInv()
{
    // Avg EQ Hack
    if (mEQGuessCurve != NULL)
        mEQGuessCurve->SetAlpha(0.0);
  
    if (mEQLearnCurve != NULL)
    {
        mEQLearnCurve->SetAlpha(0.0);
        mEQLearnCurve->SetFillAlpha(0.0);
    }
}

void
EQHack::SetCurveSpecsModeGuess()
{
    if (mEQGuessCurve != NULL)
        mEQGuessCurve->SetAlpha(1.0);
  
    if (mEQLearnCurve != NULL)
    {
        mEQLearnCurve->SetAlpha(1.0);
        float fillAlpha = mGUIHelper->GetGraphCurveFillAlpha();
        mEQLearnCurve->SetFillAlpha(fillAlpha);
    }
}

void
EQHack::CMASmooth(int smootherIndex, WDL_TypedBuf<BL_FLOAT> *buf)
{  
    WDL_TypedBuf<BL_FLOAT> bufSmooth = *buf;
    if (mSmoothers[smootherIndex] != NULL)
        mSmoothers[smootherIndex]->ProcessOne(buf->Get(),
                                              bufSmooth.Get(),
                                              buf->GetSize(),
                                              SMOOTHER_WINDOW_SIZE);
    *buf = bufSmooth;
}

bool
EQHack::SerializeState(IByteChunk &pChunk) const
{
    TRACE;
    ((EQHack *)this)->ENTER_PARAMS_MUTEX;
  
    // Previosu version was to save "avgValues"
    // It as wrong
  
    // Serialize the avg values curve
    int size = mLearnCurve.GetSize();
    pChunk.Put(&size);
  
    for (int i = 0; i < size; i++)
    {
        BL_FLOAT v = mLearnCurve.Get()[i];
        pChunk.Put(&v);
    }
  
    bool res = SerializeParams(pChunk);

    ((EQHack *)this)->LEAVE_PARAMS_MUTEX;
    
    return res;
}

int
EQHack::UnserializeState(const IByteChunk &pChunk, int startPos)
{
    TRACE;
    ENTER_PARAMS_MUTEX;
  
    // Unserialize avg values curve
    WDL_TypedBuf<BL_GUI_FLOAT> avgValues;
      
    int size;
    startPos = pChunk.Get(&size, startPos);
  
    avgValues.Resize(size);
  
    for (int i = 0; i < size; i++)
    {
        BL_FLOAT v;
        startPos = pChunk.Get(&v, startPos);
    
        avgValues.Get()[i] = v;
    }
  
    // Security: set values only if it is coherent
    // This avoid problems if one day we change the buffer size, and we have old
    // saved states of the plugin
    mLearnCurve = avgValues;

    mLearnCurve.Resize(GRAPH_CURVE_NUM_VALUES);
        
    if (mEQLearnCurveSmooth != NULL)
        mEQLearnCurveSmooth->SetValues(mLearnCurve);
    
    // Update the interface
    ModeChanged(true);
  
    int res = UnserializeParams(pChunk, startPos);

    LEAVE_PARAMS_MUTEX;
    
    return res;
}

void
EQHack::ResetLearnCurve()
{
    mLearnCurve.Resize(GRAPH_CURVE_NUM_VALUES);
    
    // 1.0 = 0dB
    // Hack 0.5
    BLUtils::FillAllValue(&mLearnCurve, (BL_FLOAT)0.5);
    if (mEQLearnCurveSmooth != NULL)
    {
        mEQLearnCurveSmooth->SetValues(mLearnCurve, true); //
        mEQLearnCurveSmooth->GetHistogramValuesDB(&mLearnCurve);
    }
}

void
EQHack::UpdateCurvesSmoothFactor()
{
#if !UPDATE_CURVES_ON_IDLE
    // Use legacy lock mechanism, so we must adapt smoothness
    
#define REF_BLOCK_SIZE 512.0
    
    BL_FLOAT smoothingTimeSlowMs = CURVE_SMOOTH_COEFF_SLOW_MS;
    BL_FLOAT smoothingTimeFastMs = CURVE_SMOOTH_COEFF_FAST_MS;

    int blockSize = GetBlockSize();

    BL_FLOAT blockSizeCoeff = REF_BLOCK_SIZE/(BL_FLOAT)blockSize;

    // Do not need to smooth more for small block size,
    // since SmoothCurveDB::OPTIM_LOCK_FREE
    //if (blockSizeCoeff > 1.0)
    //    blockSizeCoeff = 1.0;
    
    smoothingTimeSlowMs *= blockSizeCoeff;
    smoothingTimeFastMs *= blockSizeCoeff;

    BL_FLOAT sampleRate = GetSampleRate();
    BL_FLOAT smoothFactorSlow =
        ParamSmoother2::ComputeSmoothFactor(smoothingTimeSlowMs, sampleRate);
    BL_FLOAT smoothFactorFast =
        ParamSmoother2::ComputeSmoothFactor(smoothingTimeFastMs, sampleRate);
    
    if (mEQLearnCurveSmooth != NULL)
        mEQLearnCurveSmooth->Reset(sampleRate, smoothFactorSlow);
    if (mEQInstantCurveSmooth != NULL)
        mEQInstantCurveSmooth->Reset(sampleRate, smoothFactorFast);
    if (mEQGuessCurveSmooth != NULL)
        mEQGuessCurveSmooth->Reset(sampleRate, smoothFactorSlow);
#endif
}
