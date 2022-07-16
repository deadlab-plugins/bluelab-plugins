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
#include <BLSpectrogram4.h>

#include <GhostViewerFftObj.h>
#include <SpectrogramDisplayScroll4.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <GraphTimeAxis6.h>
#include <GraphFreqAxis2.h>

#include <GraphAxis2.h>

#include <PlugBypassDetector.h>
#include <BLTransport.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "GhostViewer.h"

#include "IPlug_include_in_plug_src.h"

//
// With 1024, we miss some frequencies
#define BUFFER_SIZE 2048

// 2: Quality is almost the same as with 4
// 2: CPU: 37%
//
// 4: CPU: 41%
//
#define OVERSAMPLING 2 //4
#define FREQ_RES 1
#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

// Spectrogram
#define DISPLAY_SPECTRO 1

#define SPECTRO_MAX_NUM_COLS 512 //128

// GUI Size
#define NUM_GUI_SIZES 4

#define PLUG_WIDTH_MEDIUM 920
#define PLUG_HEIGHT_MEDIUM 636 //620

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 718 //702

// Take care of Protools min width (454)
// And take care of vertical black line on the right of the spectrogram
// with some width values
#define PLUG_WIDTH_PORTRAIT 460
#define PLUG_HEIGHT_PORTRAIT 718 //702

//#define GHOSTVIEWER_FPS 50

//#define Y_LOG_SCALE_FACTOR 3.5

// Optimization
#define OPTIM_SKIP_IFFT 1

#define MAX_NUM_TIME_AXIS_LABELS 10

#define USE_DROP_DOWN_MENU 1

// Reload a project with scroll speed=1, play, open GUI
// => graph axis speed was wrong
#define FIX_ABLETON11_SCROLL_SPEED 1

#if 0
TODO: "MON" option: use a toggle button ("MON" text, with rectangular border, toggle/hilight)

TODO: check the resoltion of the spectrogram
=> if we take 1024 (full res), do we have a better image?

TODO: implement with Metal (colormap..), and on Windows
                                      
TODO: other plugin: remove MAkeDefaultPReset()

---

BUG: Windows 10 + MPC Software: blank GUI
Daniel Eberli <eberlid@gmail.com> (was refunded)

TODO: display phase DERIVATIVES (instead of only phases)
=> this was done line that in the first Spectogram plugin, and
was very good

TODO: add 4th button for gui size retina

IDEA: to have more "accurate" spectrogram: take BUFFER_SIZE=1024 instead of 2048
(test done in Sonic Visualiser => looks better !)

PROBLEM: StudioOne Sierra crackles
=> set buffer size to 1024 to fix

NOTE: FLStudio Mac Sierra: scrolling jitters a bit, and when turning brightness or contrast, it jitters more
=> this is due to default block size that is 4096
=> decreasing block size to 1024 for example solves the problems

The scroll is hacked (when using a second screen ? ) on Protools

BUG: VST3: crashes Reaper when quitting

NOTE: Modification in IPlugAAX for meta parameters (RolloverButtons)
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipRange = "Brightness - Colormap brightness";
static char *tooltipContrast = "Contrast - Colormap contrast";
static char *tooltipColormap = "Colormap";
static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";
static char *tooltipGUISizePortrait = "GUI Size: Portrait";
static char *tooltipScrollSpeed = "Speed - Scroll speed";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";


enum EParams
{
    kRange = 0,
    kContrast,
    kColorMap,
    
    kScrollSpeed,
    kMonitor,

    kSpectroMeterTimeMode,
    kSpectroMeterFreqMode,

    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    kGUISizePortrait,
    
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
    
    kGraphX = 0,
    kGraphY = 0,
    
    kRangeX = 191,
    kRangeY = 436,
    
    kContrastX = 284,
    kContrastY = 436,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsColorMapX = 150,
    kRadioButtonsColorMapY = 429,
    kRadioButtonsColorMapVSize = 96,
    kRadioButtonsColorMapNumButtons = 6,
#else
    kColorMapX = 66,
    kColorMapY = 440,
    kColorMapWidth = 80,
#endif
    
    // GUI size
    kGUISizeSmallX = 12,
    kGUISizeSmallY = 430,
    
    kGUISizeMediumX = 12,
    kGUISizeMediumY = 453,
    
    kGUISizeBigX = 12,
    kGUISizeBigY = 476,
    
    kGUISizePortraitX = 35,
    kGUISizePortraitY = 430,

    kRadioButtonsScrollSpeedX = 362,
    kRadioButtonsScrollSpeedY = 448,
    kRadioButtonsScrollSpeedVSize = 70,
    kRadioButtonsScrollSpeedNumButtons = 3,
    
    kCheckboxMonitorX = 77,
    kCheckboxMonitorY = 478,

    kSpectroMeterX = 159,
    kSpectroMeterY = 407,
    kSpectroMeterTextWidth = 91
};

//
GhostViewer::GhostViewer(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , ResizeGUIPluginInterface(this)
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

GhostViewer::~GhostViewer()
{
    if (mDispFftObj != NULL)
        delete mDispFftObj;
    
    if (mGhostViewerObj != NULL)
        delete mGhostViewerObj;
    
    if (mTimeAxis != NULL)
        delete mTimeAxis;
    
    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mVAxis != NULL)
        delete mVAxis;

    if (mBypassDetector != NULL)
        delete mBypassDetector;

    if (mTransport != NULL)
        delete mTransport;

    if (mSpectroMeter != NULL)
        delete mSpectroMeter;

    if (mSpectroDisplayScrollState != NULL)
        delete mSpectroDisplayScrollState;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
GhostViewer::MyMakeGraphics()
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
GhostViewer::MyMakeLayout(IGraphics *pGraphics)
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
    else if (mGUISizeIdx == 3)
        pGraphics->AttachBackground(BACKGROUND_PORTRAIT_FN);

    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);

    pGraphics->EnableTooltips(true);
    pGraphics->SetTooltipsDelay(TOOLTIP_DELAY);
    
    CreateControls(pGraphics, mGUIOffsetY);

    //if (mFirstTimeCreate)
    //{
    mMustSetScrollSpeed = true;
    //mFirstTimeCreate = false;
    //}
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mDispFftObj = NULL;
    mGhostViewerObj = NULL;
    
    mGraph = NULL;
    
    mHAxis = NULL;
    mVAxis = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectroDisplayScrollState = NULL;
    
    mMustUpdateSpectrogram = true;
    
    mPrevSampleRate = GetSampleRate();
    
    // From Waves
    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizePortraitButton = NULL;
    
    // Dummy values, to avoid undefine (just in case)
    mGraphWidthSmall = 256;
    mGraphHeightSmall = 256;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;
    
    mTimeAxis = NULL;
    mFreqAxis = NULL;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
    
    mWasPlaying = false;

    mIsPlaying = false;

    mBypassDetector = NULL;
    mTransport = NULL;

    mMustUpdateTimeAxis = true;

    mSpectroMeter = NULL;
    
    mPrevMouseX = -1.0;
    mPrevMouseY = -1.0;

    //mFirstTimeCreate = true;
    mMustSetScrollSpeed = true;

    mScrollSpeedNum = 0;
    mPrevScrollSpeedNum = -1;
}

void
GhostViewer::InitParams()
{
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    GetParam(kRange)->InitDouble("Brightness", defaultRange, -1.0, 1.0, 0.01, "");
    
    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
    
    // ColorMap num
    int  defaultColorMap = 0;
    mColorMapNum = defaultColorMap;
#if !USE_DROP_DOWN_MENU
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 5/*4*/);
#else
    GetParam(kColorMap)->
        InitEnum("ColorMap", 0, 6, "", IParam::kFlagsNone,
                 "", "Blue", "Gray", "Rainbow", "Wasp", "Dawn", "Purple");
#endif
    
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
    
    GetParam(kGUISizePortrait)->
        InitInt("PortraitGUI", 0, 0, 1,
                "", IParam::kFlagMeta | IParam::kFlagCannotAutomate);
    
    int  defaultScrollSpeed = 2;
    mScrollSpeedNum = defaultScrollSpeed;
    mPrevScrollSpeedNum = -1;
    GetParam(kScrollSpeed)->InitInt("ScrollSpeed", defaultScrollSpeed, 0, 2);
    
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");

    // Spectro meter modes
    mMeterTimeMode = SpectroMeter::SPECTRO_METER_TIME_HMS;
    //GetParam(kSpectroMeterTimeMode)->InitInt("MeterTimeMode", mMeterTimeMode, 0, 1);
    GetParam(kSpectroMeterTimeMode)->InitEnum("MeterTimeMode", mMeterTimeMode, 2,
                                              "", IParam::kFlagsNone, "",
                                              "Samples", "HMS");

    mMeterFreqMode = SpectroMeter::SPECTRO_METER_FREQ_HZ;
    //GetParam(kSpectroMeterFreqMode)->InitInt("MeterFreqMode", mMeterFreqMode, 0, 1);
    GetParam(kSpectroMeterFreqMode)->InitEnum("MeterFreqMode", mMeterFreqMode, 2,
                                              "", IParam::kFlagsNone, "",
                                              "Hz", "Bins");
}

void
GhostViewer::ApplyParams()
{
    if (mGhostViewerObj != NULL)
    {
        BLSpectrogram4 *spectro = mGhostViewerObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }
    
    if (mGhostViewerObj != NULL)
    {
        BLSpectrogram4 *spectro = mGhostViewerObj->GetSpectrogram();
        spectro->SetContrast(mContrast);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }
    
    if (mGraph != NULL)
    {
        SetColorMap(mColorMapNum);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }

    // Reload a project with scroll speed=1, play, open GUI
    // => graph axis speed was wrong
#if FIX_ABLETON11_SCROLL_SPEED
    mPrevScrollSpeedNum = -1;
    SetScrollSpeed(mScrollSpeedNum);
#endif
    
    if (mMonitorEnabled && !mIsPlaying)
        mTransport->SetDAWTransportValueSec(0.0);

    if (mSpectroMeter != NULL)
        mSpectroMeter->SetTimeMode(mMeterTimeMode);    
    if (mSpectroMeter != NULL)
        mSpectroMeter->SetFreqMode(mMeterFreqMode);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
GhostViewer::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mDispFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> dispProcessObjs;
        mGhostViewerObj =
            new GhostViewerFftObj(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        dispProcessObjs.push_back(mGhostViewerObj);

        // Use 1 channel and convert input to mono
        int numChannels = 1;
        int numScInputs = 0;
        
        mDispFftObj = new FftProcessObj16(dispProcessObjs,
                                          numChannels, numScInputs,
                                          BUFFER_SIZE, oversampling, freqRes,
                                          sampleRate);
        
#if OPTIM_SKIP_IFFT
        mDispFftObj->SetSkipIFft(-1, true);
#endif
        
#if !VARIABLE_HANNING
        mDispFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowHanning);
        mDispFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowHanning);
#else
        mDispFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowVariableHanning);
        mDispFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowVariableHanning);
#endif
        
        mDispFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                            KEEP_SYNTHESIS_ENERGY);
    }
    else
    {
        FftProcessObj16 *dispFftObj = mDispFftObj;
        dispFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mGhostViewerObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);
    
    ApplyParams();
    
    UpdateTimeAxis();

    // NOTE: this is not 100% reliable
    // (200ms fails sometimes, increase ?)
    mBypassDetector = new PlugBypassDetector();

    mTransport = new BLTransport(sampleRate);
    if (mTransport != NULL)
        mTransport->SetSoftResynchEnabled(true);
    
    if (mGraph != NULL)
        mGraph->SetTransport(mTransport);
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetTransport(mTransport);
    if (mTimeAxis != NULL)
        mTimeAxis->SetTransport(mTransport);
    
    mIsInitialized = true;
}

void
GhostViewer::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();
    
    bool isPlaying = IsTransportPlaying();
    BL_FLOAT transportTime = BLUtilsPlug::GetTransportTime(this);
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetBypassed(false);
    
    if (mBypassDetector != NULL)
    {
        mBypassDetector->TouchFromAudioThread();
        mBypassDetector->SetTransportPlaying(isPlaying || mMonitorEnabled);
    }
    
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
    
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    mIsPlaying = isPlaying;

    if (isPlaying || mMonitorEnabled)
    {
        // Set scroll speed only when playing
        // Will avoid shift bugs when resizing GUI
        if (mMustSetScrollSpeed)
        {
            mPrevScrollSpeedNum = -1;
            SetScrollSpeed(mScrollSpeedNum);
            
            mMustSetScrollSpeed = false;
        }
    }
    
    if (mUIOpened)
    { 
        if (mTransport != NULL)
        {
            mTransport->SetTransportPlaying(isPlaying, mMonitorEnabled,
                                            transportTime, nFrames);
        }
        
        if (!isPlaying && mWasPlaying && mMonitorEnabled)
            // Playing just stops and monitor is enabled
        {
            // => Reset time axis time
            if (mTransport != NULL)
                mTransport->SetDAWTransportValueSec(0.0);
        }
    }

    mWasPlaying = isPlaying;
    
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
     
    if (isPlaying || mMonitorEnabled)
    {
        vector<WDL_TypedBuf<BL_FLOAT> > dummy;
        vector<WDL_TypedBuf<BL_FLOAT> > &inMono = mTmpBuf3;
        inMono.resize(1);
        BLUtils::StereoToMono(&inMono[0], in);
        
        mDispFftObj->Process(inMono, dummy, &out);
        
        mMustUpdateSpectrogram = true;

        if (mUIOpened)
        {
            if (isPlaying && !mMonitorEnabled)
            {
                if (mTransport != NULL)
                    mTransport->SetDAWTransportValueSec(transportTime);
            }
        }
    }
    
    // The plugin does not modify the sound, so bypass
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    if (mUIOpened)
    {
        if (isPlaying || mMonitorEnabled)
        {
            // Update the spectrogram GUI
            // Must do it here to be in the correct thread
            // (and not in the idle() thread
            if (mMustUpdateSpectrogram)
            {
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(true);
                    mMustUpdateSpectrogram = false;
                }
            }
            else
            {
                // Do not update the spectrogram texture
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                }
            }
        }
    }
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
    
    if (mGraph != NULL)
        mGraph->PushAllData();
    
    BL_PROFILE_END;
}

void
GhostViewer::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN/*kGraph*/);
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);

    mGraph->AddCustomControl(this);
    
    // GUIResize
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
    CreateSpectrogramDisplay(false);
    
    mGraph->SetTransport(mTransport);
    
    // Range
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kRangeX, kRangeY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kRange,
                              TEXTFIELD_FN,
                              "BRIGHTNESS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipRange);
    
    // Contrast
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kContrastX, kContrastY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kContrast,
                              TEXTFIELD_FN,
                              "CONTRAST",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipContrast);
    

#if !USE_DROP_DOWN_MENU// Radio buttons
    // ColorMap num
    const char *colormapRadioLabels[kRadioButtonsColorMapNumButtons] =
        { "BLUE", "GRAY", "RAINBOW", "WASP", "DAWN", "PURPLE" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsColorMapX,
                                   kRadioButtonsColorMapY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsColorMapNumButtons,
                                   kRadioButtonsColorMapVSize,
                                   kColorMap,
                                   false,
                                   "COLORMAP",
                                   EAlign::Far,
                                   EAlign::Far,
                                   colormapRadioLabels);
#else
    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kColorMapX, kColorMapY + offset,
                                   kColorMapWidth,
                                   kColorMap,
                                   "COLORMAP",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipColormap);
#endif
    
    const char *scrollSpeedRadioLabels[kRadioButtonsScrollSpeedNumButtons] =
        { "x1", "x2", "x4" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsScrollSpeedX,
                                   kRadioButtonsScrollSpeedY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsScrollSpeedNumButtons,
                                   kRadioButtonsScrollSpeedVSize,
                                   kScrollSpeed,
                                   false, "SPEED",
                                   EAlign::Near,
                                   EAlign::Near,
                                   scrollSpeedRadioLabels,
                                   tooltipScrollSpeed);
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeSmallX, kGUISizeSmallY + offset,
                                      BUTTON_RESIZE_SMALL_FN,
                                      kGUISizeSmall, "", 0,
                                      tooltipGUISizeSmall);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeMediumX, kGUISizeMediumY + offset,
                                      BUTTON_RESIZE_MEDIUM_FN,
                                      kGUISizeMedium, "", 1,
                                      tooltipGUISizeMedium);
    
    mGUISizeBigButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeBigX, kGUISizeBigY + offset,
                                      BUTTON_RESIZE_BIG_FN,
                                      kGUISizeBig, "", 2,
                                      tooltipGUISizeBig);
    
    mGUISizePortraitButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizePortraitX, kGUISizePortraitY + offset,
                                      BUTTON_RESIZE_PORTRAIT_FN,
                                      kGUISizePortrait, "", 3,
                                      tooltipGUISizePortrait);
    
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX,
                                                     kCheckboxMonitorY + offset,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT, true,
                                                     tooltipMonitor);

    if (mSpectroMeter == NULL)
    {
        BL_FLOAT sampleRate = GetSampleRate();
        mSpectroMeter = new SpectroMeter(kSpectroMeterX, kSpectroMeterY,
                                         kSpectroMeterTextWidth,
                                         kSpectroMeterTimeMode,
                                         kSpectroMeterFreqMode,
                                         BUFFER_SIZE, sampleRate,
                                         SpectroMeter::SPECTRO_METER_DISPLAY_POS);
        
        mSpectroMeter->SetTextFieldHSpacing(6);
        mSpectroMeter->SetTextFieldVSpacing(7);
        
        int bgColor[4];
        mGUIHelper->GetGraphCurveDarkBlue(bgColor);
        IColor bgIColor(bgColor[3], bgColor[0], bgColor[1], bgColor[2]);
        
        mSpectroMeter->SetBackgroundColor(bgIColor);
        
        IColor borderColor;
        mGUIHelper->GetValueTextColor(&borderColor);
        
        float borderWidth = 1.5; //2.0;
        mSpectroMeter->SetBorderColor(borderColor);
        mSpectroMeter->SetBorderWidth(borderWidth);
    }
    
    if (mSpectroMeter != NULL)
        mSpectroMeter->GenerateUI(mGUIHelper, pGraphics, 0, offset);
    
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
    
    //
    mControlsCreated = true;
}

void
GhostViewer::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
GhostViewer::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    if (mTransport != NULL)
        mTransport->Reset();
        
    BL_FLOAT sampleRate = GetSampleRate();
    if (sampleRate != mPrevSampleRate)
    {
        // Frame rate have changed
        if (mFreqAxis != NULL)
            mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
        
        mPrevSampleRate = sampleRate;

        if (mGhostViewerObj != NULL)
            mGhostViewerObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);
    }
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);

    if (mTransport != NULL)
        mTransport->Reset(sampleRate);
    
    UpdateTimeAxis();

    if (mSpectroMeter != NULL)
        mSpectroMeter->Reset(BUFFER_SIZE, sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
GhostViewer::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mGhostViewerObj != NULL)
            {
                BLSpectrogram4 *spectro = mGhostViewerObj->GetSpectrogram();
                spectro->SetRange(mRange);
                
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }
            }
        }
        break;
            
        case kContrast:
        {
            mContrast = GetParam(paramIdx)->Value();
            
            if (mGhostViewerObj != NULL)
            {
                BLSpectrogram4 *spectro = mGhostViewerObj->GetSpectrogram();
                spectro->SetContrast(mContrast);
            
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }
            }
        }
        break;
            
        case kColorMap:
        {
            mColorMapNum = GetParam(paramIdx)->Int();
            
            if (mGraph != NULL)
            {
                SetColorMap(mColorMapNum);
            
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateColormap(true);
                }
            }
        }
        break;
            
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
            
        case kGUISizePortrait:
        {
            int activated = GetParam(paramIdx)->Int();
            if (activated)
            {
                if (mGUISizeIdx != 3)
                {
                    mGUISizeIdx = 3;

                    GUIResizeParamChange(mGUISizeIdx);
                    ApplyGUIResize(mGUISizeIdx);
                }
            }
        }
        break;
            
        case kScrollSpeed:
        {
            int scrollSpeedNum = GetParam(kScrollSpeed)->Int();

            mScrollSpeedNum = scrollSpeedNum;
            mMustSetScrollSpeed = true;

            // Will do set scroll speed later
        }
        break;
            
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            bool valueBool = (value == 1);

            if (valueBool != mMonitorEnabled)
            {
                mMonitorEnabled = valueBool;
            
                if (mMonitorEnabled)
                { 
                    if (mTransport != NULL)
                        mTransport->SetDAWTransportValueSec(0.0);
                    
                    // The following two block fixes time axis speed
                    // FIX: play, stop, monitor, stop monitor, resize gui, monitor
                    // => the time axis speed went wrong
                    if (mTransport != NULL)
                    {
                        if (mTransport->IsTransportPlaying())
                            mTransport->Reset();
                    }
                    
                    if (mIsPlaying || mMonitorEnabled)
                        mMustUpdateTimeAxis = true;
                }
            }
        }
        break;

        // Spectro meter
        case kSpectroMeterTimeMode:
        {
            int value = GetParam(paramIdx)->Int();
            mMeterTimeMode = (SpectroMeter::TimeMode)value;
            
            if (mSpectroMeter != NULL)
                mSpectroMeter->SetTimeMode(mMeterTimeMode);
        }
        break;
            
        case kSpectroMeterFreqMode:
        {
            int value = GetParam(paramIdx)->Int();
            mMeterFreqMode = (SpectroMeter::FreqMode)value;
            
            if (mSpectroMeter != NULL)
                mSpectroMeter->SetFreqMode(mMeterFreqMode);
        } 
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

#if FIX_ABLETON11_SCROLL_SPEED
    // Flush legacy lock buffering, so that mSpectrogramDisplay->GetSpeedMod()
    // will return the current value
    OnIdle();
#endif
    
    // Test, in order to update the time axis only at startup,
    // not when closed and re-opened
    if (mMustUpdateTimeAxis)
    {
        UpdateTimeAxis();
        mMustUpdateTimeAxis = false;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    mMonitorControl = NULL;

    mGraph = NULL;

    if (mTimeAxis != NULL)
        mTimeAxis->SetGraph(NULL);
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    
    mSpectrogramDisplay = NULL;
    if (mGhostViewerObj != NULL)
        mGhostViewerObj->SetSpectrogramDisplay(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizePortraitButton = NULL;

    if (mSpectroMeter != NULL)
        mSpectroMeter->ClearUI();
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::SetColorMap(int colorMapNum)
{
    if (mGhostViewerObj != NULL)
    {
        BLSpectrogram4 *spec = mGhostViewerObj->GetSpectrogram();

        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_GREY,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_WASP,
            ColorMapFactory::COLORMAP_DAWN_FIXED,
            ColorMapFactory::COLORMAP_PURPLE
        };
        ColorMapFactory::ColorMap colorMapNum0 = colorMapNums[colorMapNum];
    
        spec->SetColorMap(colorMapNum0);
    
        // FIX: set to false,false to fix
        // BUG: play data, stop, change colormap
        // => there is sometimes a small jumps of the data to the left
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
        }
    }
}

void
GhostViewer::SetScrollSpeed(int scrollSpeedNum)
{
    if (scrollSpeedNum == mPrevScrollSpeedNum)
        // Nothing to do
        return;
        
    int speedMod = 1;
    switch(scrollSpeedNum)
    {
        case 0:
            speedMod = 4;
            break;
            
        case 1:
            speedMod = 2;
            break;
            
        case 2:
            speedMod = 1;
            break;
            
        default:
            break;
    }

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetSpeedMod(speedMod);
    
    if (mGhostViewerObj != NULL)
    {
        mGhostViewerObj->SetSpeedMod(speedMod);

        // And also update the transport only if the speed changed
        // FIX: play, change GUI size => the time axis scrolls too fast
        if (mTransport != NULL)
        {
            // Reset only if the spectrogram is moving
            // FIX: GhostViewer: play, stop, then change the spectro speed
            // => there was a jump in the spectrogram
            if (mTransport->IsTransportPlaying())
                mTransport->Reset();
        }
            
        // Update time axis only if scroll speed changed
        if (mIsPlaying || mMonitorEnabled)
        {
            // Do not directly update the time axis here,
            // otherwise there is a problem:
            // - play
            // - stop
            // - change the speed
            // => the time axis changes, whereas the spectrogram data
            // is not chaging at all
            mMustUpdateTimeAxis = true;
        }
    }

    mScrollSpeedNum = scrollSpeedNum;
    mPrevScrollSpeedNum = mScrollSpeedNum;
}

void
GhostViewer::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT },
        { PLUG_WIDTH_MEDIUM, PLUG_HEIGHT_MEDIUM },
        { PLUG_WIDTH_BIG, PLUG_HEIGHT_BIG },
        { PLUG_WIDTH_PORTRAIT, PLUG_HEIGHT_PORTRAIT }
    };
    
    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
GhostViewer::PreResizeGUI(int guiSizeIdx,
                          int *outNewGUIWidth, int *outNewGUIHeight)
{
    IGraphics *pGraphics = GetUI();
    if (pGraphics == NULL)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    GetNewGUISize(guiSizeIdx, outNewGUIWidth, outNewGUIHeight);

    UpdatePrevMouse(*outNewGUIWidth, *outNewGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            *outNewGUIWidth, *outNewGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);
    
    mMonitorControl = NULL;
    mControlsCreated = false;
    
    mSpectrogramDisplay = NULL;
    if (mGhostViewerObj != NULL)
        mGhostViewerObj->SetSpectrogramDisplay(NULL);

    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
        
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium,
                              kGUISizeBig, kGUISizePortrait };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
    { mGUISizeSmallButton, mGUISizeMediumButton,
      mGUISizeBigButton, mGUISizePortraitButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

// NOTE: OnIdle() is called from the GUI thread
void
GhostViewer::OnIdle()
{
    // Make sure that mSpectrogramDisplay is up to date here
    // (because after, we use its params to set the time axis)
    ENTER_PARAMS_MUTEX;

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->PullData();

    LEAVE_PARAMS_MUTEX;

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->ApplyData();

    //
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
            if (mTransport != NULL)
                mTransport->SetBypassed(bypassed);

#if 1 // FIX: monitor, bypass => black margin the the right
            if (bypassed && mMonitorEnabled)
            {
                if (mSpectrogramDisplay != NULL)
                    mSpectrogramDisplay->SetBypassed(true);
            }
#endif
        }

        if (mIsPlaying || mMonitorEnabled)
        {
            if (mMustUpdateTimeAxis)
            {
                //ENTER_PARAMS_MUTEX;
                
                UpdateTimeAxis();
                
                //LEAVE_PARAMS_MUTEX;
                
                mMustUpdateTimeAxis = false;
            }
            //}

            //if (mIsPlaying || mMonitorEnabled)
            //{
            
            // When scrolling SpectroMeter time values must change,
            // if we keep the cursor on the graph
            CursorMoved(mPrevMouseX, mPrevMouseY);
        }
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
GhostViewer::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    // Axis
    BLSpectrogram4 *spectro = mGhostViewerObj->GetSpectrogram();
    int numBuffers = spectro->GetMaxNumCols();
    
    // SpectrogramDisplayScroll4 upscale a bit the image, for hiding the borders.
    // If we don't scale and offset the time axis values, we would have an
    // effect of "drift" of the time axis labels, conpared to the spectrogram
    // Applying scale and offset fixes!
    // => the time axis labels follow exactly the spectrogram
    BL_FLOAT timeOffsetSec;
    BL_FLOAT timeScale;
    mSpectrogramDisplay->GetTimeTransform(&timeOffsetSec, &timeScale);
    
    numBuffers *= timeScale;
    
    // Adjust to exactly the number of visible columns in the
    // spectrogram
    // (there is a small width scale in SpectrogramDisplayScroll,
    // to hide border columns)
    int speedMod = mSpectrogramDisplay->GetSpeedMod();
    numBuffers *= speedMod;
    
    BL_FLOAT timeDuration =
        GraphTimeAxis6::ComputeTimeDuration(numBuffers,
                                            BUFFER_SIZE,
                                            OVERSAMPLING,
                                            sampleRate);
    
    if (mTimeAxis != NULL)
        mTimeAxis->Reset(BUFFER_SIZE, timeDuration,
                         MAX_NUM_TIME_AXIS_LABELS, timeOffsetSec);
}

void
GhostViewer::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mGhostViewerObj == NULL))
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    mSpectrogram = mGhostViewerObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    mSpectrogram->SetYScale(Scale::MEL);
    mSpectrogram->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        bool stateWasNull = (mSpectroDisplayScrollState == NULL);
        mSpectrogramDisplay =
            new SpectrogramDisplayScroll4(mSpectroDisplayScrollState);
        mSpectroDisplayScrollState = mSpectrogramDisplay->GetState();

        if (stateWasNull)
        {
            mSpectrogramDisplay->SetSpectrogram(mSpectrogram, 0.0, 0.0, 1.0, 1.0);
            mSpectrogramDisplay->SetTransport(mTransport);
            
            mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);
        }
        
        mGhostViewerObj->SetSpectrogramDisplay(mSpectrogramDisplay);
        mGraph->AddCustomDrawer(mSpectrogramDisplay);
    }
}

void
GhostViewer::CreateGraphAxes()
{
    bool firstTimeCreate = (mHAxis == NULL);
    
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mTimeAxis = new GraphTimeAxis6(false, false);
        mTimeAxis->SetTransport(mTransport);
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2(false, Scale::MEL);
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);

    if (firstTimeCreate)
    {
        BL_FLOAT sampleRate = GetSampleRate();
        int graphWidth = mGraph->GetRECT().W();

        mTimeAxis->Init(mGraph, mHAxis, mGUIHelper,
                        BUFFER_SIZE, 1.0, MAX_NUM_TIME_AXIS_LABELS);
    
        mFreqAxis->Init(mVAxis, mGUIHelper, false,
                        BUFFER_SIZE, sampleRate, graphWidth);
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
    }
    else
    {
        mTimeAxis->SetGraph(mGraph);
    }
}

void
GhostViewer::OnMouseOver(float x, float y, const IMouseMod &mod)
{
    mPrevMouseX = x;
    mPrevMouseY = y;
    
    CursorMoved(x, y);
}
                             

void
GhostViewer::OnMouseDrag(float x, float y, float dX, float dY,
                         const IMouseMod &mod)
{
    mPrevMouseX = x;
    mPrevMouseY = y;
    
    CursorMoved(x, y);
}

void
GhostViewer::OnMouseOut()
{
    mPrevMouseX = -1.0;
    mPrevMouseY = -1.0;
}

void
GhostViewer::CursorMoved(BL_FLOAT x, BL_FLOAT y)
{
    if (mGraph == NULL)
        return;
    
    if (mSpectroMeter == NULL)
        return;

    IRECT graphBounds = mGraph->GetRECT();
    if ((x >= graphBounds.L) && (x <= graphBounds.R) &&
        (y >= graphBounds.T) & (y <= graphBounds.B))
        // In bounds
    {
        x = x / graphBounds.W();
        y = y / graphBounds.H();
        
        if (mSpectroMeter != NULL)
        {
            BL_FLOAT timeX = GraphNormXToTime(x);
            BL_FLOAT freqY = GraphNormYToFreq(y);
            
            mSpectroMeter->SetCursorPosition(timeX, freqY);
        }
    }
}
    
BL_FLOAT
GhostViewer::GraphNormXToTime(BL_FLOAT normX)
{
    BL_FLOAT timeX = 0.0;
    
    if (mTimeAxis != NULL)
    {
        BL_FLOAT minTimeSec;
        BL_FLOAT maxTimeSec;
        mTimeAxis->GetMinMaxTime(&minTimeSec, &maxTimeSec);
        
        timeX = minTimeSec + normX*(maxTimeSec - minTimeSec);

        if (mSpectrogramDisplay != NULL)
        {
            BL_FLOAT timeOffsetSec;
            BL_FLOAT timeScale;
            mSpectrogramDisplay->GetTimeTransform(&timeOffsetSec, &timeScale);

            // NOTE: for the moment we ignore timeScale
            // => this looks good like this...
            timeX = timeX - timeOffsetSec;
        }
    }

    return timeX;
}

BL_FLOAT
GhostViewer::GraphNormYToFreq(BL_FLOAT normY)
{
    if (mSpectrogram == NULL)
        return 0.0;

    normY = 1.0 - normY;

    if (normY < 0.0)
        normY = 0.0;
    
    BL_FLOAT freq = mSpectrogram->NormYToFreq(normY);

    return freq;
}

void
GhostViewer::UpdatePrevMouse(float newGUIWidth, float newGUIHeight)
{
    if (/*mPlug->*/GetUI() != NULL)
    {
        if ((mPrevMouseX >= 0) && (mPrevMouseY >= 0))
        {
            float currentGUIWidth = /*mPlug->*/GetUI()->Width();
            float currentGUIHeight = /*mPlug->*/GetUI()->Height();
            
            mPrevMouseX /= currentGUIWidth;
            mPrevMouseX *= newGUIWidth;
            
            mPrevMouseY /= currentGUIHeight;
            mPrevMouseY *= newGUIHeight;
        }
    }
}
