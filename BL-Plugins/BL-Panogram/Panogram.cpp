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

#include <PanogramFftObj.h>
#include <SpectrogramDisplayScroll4.h>

#include <PanogramGraphDrawer.h>

#include <PanogramCustomDrawer.h>
#include <PanogramCustomControl.h>

#include <PanogramPlayFftObj.h>

#include <StereoWidenProcess.h>
#include <DelayObj4.h>

// See: http://www.rs-met.com/documents/tutorials/StereoProcessing.pdf
#include <PseudoStereoObj2.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <GraphTimeAxis6.h>
#include <GraphAxis2.h>

#include <Scale.h>

#include <ParamSmoother2.h>

#include <PlugBypassDetector.h>
#include <BLTransport.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Panogram.h"

#include "IPlug_include_in_plug_src.h"

// Idea of panogram from: https://www.irisa.fr/prive/kadi/Sujets_CTR/Emmanuel/Vincent_sujet1_article_avendano.pdf

#define DEBUG_GRAPH 0

// With 1024, we miss some frequencies
#define BUFFER_SIZE 2048

// 2: Smoother (which is better than 4 => more distinct lines)
// 2: CPU: 41%
// 4: CPU: 47%
#define OVERSAMPLING 2 //4 //1 //32 //1 //4

#define FREQ_RES 1 //4 seems to improve transients with 4

#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

// Spectrogram
#define DISPLAY_SPECTRO 1

#define SPECTRO_MAX_NUM_COLS 512 //128

// 4 pixels
#define SELECTION_BORDER_SIZE 8

// Graph
#define NUM_CURVES 1

// Curves
#define Y_AXIS_CURVE_HZ      0

// When set to BUFFER_SIZE, it makes less flat areas at 0
#define GRAPH_CONTROL_NUM_POINTS BUFFER_SIZE //BUFFER_SIZE/4

// GUI Size
#define NUM_GUI_SIZES 3

#define GUI_WIDTH_MEDIUM 1080
#define GUI_HEIGHT_MEDIUM 652

#define GUI_WIDTH_BIG 1200
#define GUI_HEIGHT_BIG 734

// Since the GUI is very full, reduce the offset after
// the radiobuttons title text
#define RADIOBUTTONS_TITLE_OFFSET -5

// Panogram
// NOTE: Can't skip, because we play previously recorded sound
#define OPTIM_SKIP_IFFT 0 //1

#define FIX_BAR_PLAY_BAR_SPEED 1

// Avoid selecting outside the view
#define FIX_CLIP_SELECTION 1

// FIX: just after having freezed, when clicking on the view,
// the spectrogram shifted lightly on the left
#define FIX_SPECTRO_SHIFT_CLICK 1

// FIX: play, freeze, wait a little, try to play a selection => no sound
// This is because the data continues to arrive to the FftObj, and fills with zeros
// This appended with in Reaper with TestSynchor project, where there are silences
#define FIX_FREEZE_FFT_OBJ 1

// 5 ms: not centered sound
// 20 ms: sound more centered
#define MONO2STEREO_DELAY 20.0 //5.0

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

// Use PseudoStereoObj instead of StereoWidenProcess
#define USE_PSEUDO_STEREO_OBJ 1

// NEW, after Feb2020 update
#define HACK_FOR_MONITORING 0 //1

#define MAX_NUM_TIME_AXIS_LABELS 10

// FIX imprecise selection, due to spectro scroll4 scale
#define FIX_SPECTRO_SCROLL4_SELECTION 1

#define USE_DROP_DOWN_MENU 1

#if 0
TODO: add a switch for vertical display scroll (left channel on the left, right channel on the riht). Los Teignos Audiofanzine, Christopher Kissel MonoToStereo.
    
TODO: add scroll speed?

TODO: consider soft masking complex => to have a better sound when playing region
    
TODO: idea: make a control to extract parts of the audio in the stereo field: 1 position know, 1 width knob, 1 "focus" knob (that will use DUET) => so we could extract a single instrument if it is well isolated/focused in the stereo field.
For "focus" knob, maybe start by trying a simple threshold: keeps everything above the threshold

IDEA: add the DUET algorithm
https://www.researchgate.net/publication/220736985_Degenerate_Unmixing_Estimation_Technique_using_the_Constant_Q_Transform

IDEA: idea from DUET/StereoDeReverb: add a threshold, to separate stereo focused sound, and stereo spread sound. button to invert the threshold. Add soft masks complex, to get a good thresholded sound, that could be used in a mix. Use soft masks complex too, in order to play a selection without artifacts.

TODO: for mono2stere, apply the improvement from UST
=> "downmix compatible and no coloration" instead of Lauridsen
TODO: apply optimization from UST: STEREO_WIDEN_SQRT_CUSTOM_OPTIM
(complex multiplications instead of sin cos and tan)

BUG: test Sierra, Studio One 4 => if the input sound is mono, or even if it a false stereo
(double the mono sound), the output of the plugins seems to be always mono (even on a stereo track)
BUG: Reason 10, Sierra: in freeze mode, to play continuously, the play button must be kept pressed
(if we release the mouse click on the button, it stopls playing)

BUG: with host block size > 1024, this jitters (especially when dragging a selection over panogram)
BUG (little): checkbox not working very well with Cubase 10 Mac

TODO: out of selection: colored black transparent
TODO: add 4th button for gui size retina

TODO/BUG: MultiProcessObjs does not reset when FftProcessObj resets
TODO: MultiProcessObjs: keep mBufferSize and so on in the parent class (like ProcessObj)
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipRange = "Brightness - Colormap brightness";
static char *tooltipContrast = "Contrast - Colormap contrast";
static char *tooltipSharpness = "Sharpness - Sharpness of the data";
static char *tooltipColormap = "Colormap";
static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";
static char *tooltipPlayStop = "Play/Stop - Play selected region";
static char *tooltipFreeze = "Freeze - Freeze the scrolling";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipStereoWidth =
    "Stereo Width - Increase or decrease the stereo effect";
static char *tooltipPan = "Pan - Pan L/R";
static char *tooltipMonoToStereo = "Mono To Stereo - Generate stereo from mono input";
static char *tooltipviewOrientation = "Scrolling Direction: Horizontal or vertical";

enum EParams
{
    kRange = 0,
    kContrast,
    kColorMap,

    kSharpness,
    //kScrollSpeed,

    kFreeze,
    kPlayStop,
    
    kStereoWidth,
    kPan,
    kMonoToStereo,

    kMonitor,
    kOutGain,
    
    kViewOrientation,

    kGUISizeSmall,
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

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,

    kKnobWidth = 72,
    kKnobHeight = 72,
    
    kGraphX = 0,
    kGraphY = 0,
    
    kRangeX = 191,
    kRangeY = 435,
    
    kContrastX = 286,
    kContrastY = 435,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsColorMapX = 154,
    kRadioButtonsColorMapY = 443,
    kRadioButtonColorMapVSize = 100,
    kRadioButtonColorMapNumButtons = 6,
#else
    kColorMapX = 84,
    kColorMapY = 440,
    kColorMapWidth = 80,
#endif
    
    kSharpnessX = 381,
    kSharpnessY = 435,
    
    // GUI size
    kGUISizeSmallX = 12,
    kGUISizeSmallY = 429,
    
    kGUISizeMediumX = 12,
    kGUISizeMediumY = 452,
    
    kGUISizeBigX = 12,
    kGUISizeBigY = 475, //476,

    /*kRadioButtonsScrollSpeedX = 360,
      kRadioButtonsScrollSpeedY = 429,
      kRadioButtonScrollSpeedVSize = 50,
      kRadioButtonScrollSpeedNumButtons = 3,
    */
    
    kMonitorX = 880,
    kMonitorY = 403,

    kFreezeX = 466,
    kFreezeY = 436,
    
    kPlayStopX = 465,
    kPlayStopY = 495,
    
    kOutGainX = 893,
    kOutGainY = 435,
    
    // Stereo widen
    kStereoWidthX = 608,
    kStereoWidthY = 414,
    
    kPanX = 725,
    kPanY = 435,
    
    kMonoToStereoX = 715,
    kMonoToStereoY = 403,

    kCheckboxViewOrientationX = 46,
    kCheckboxViewOrientationY = 429
};

//
Panogram::Panogram(const InstanceInfo &info)
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

Panogram::~Panogram()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mPanogramObj != NULL)
        delete mPanogramObj;
    
    if (mPanogramCustomDrawerState != NULL)
        delete mPanogramCustomDrawerState;
    
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;

    if (mCustomControl != NULL)
        delete mCustomControl;
    
#if !USE_PSEUDO_STEREO_OBJ
    if (mDelayObj != NULL)
        delete mDelayObj;
#endif
    
#if USE_PSEUDO_STEREO_OBJ
    if (mPseudoStereoObj != NULL)
        delete mPseudoStereoObj;
#endif
    
#if STEREO_WIDEN_PARAM_SMOOTHERS
    if (mStereoWidthSmoother != NULL)
        delete mStereoWidthSmoother;

    if (mPanSmoother != NULL)
        delete mPanSmoother;
#endif
  
    if (mTimeAxis != NULL)
        delete mTimeAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;

    if (mVAxis != NULL)
        delete mVAxis;
    
    if (mBypassDetector != NULL)
        delete mBypassDetector;

    if (mTransport != NULL)
        delete mTransport;

    if (mSpectroDisplayScrollState != NULL)
        delete mSpectroDisplayScrollState;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Panogram::MyMakeGraphics()
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
Panogram::MyMakeLayout(IGraphics *pGraphics)
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

#if 0 //1
    pGraphics->ShowFPSDisplay(true);
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
Panogram::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();

    mFftObj = NULL;
   
    mPanogramObj = NULL;

    mGraphDrawer = NULL;

    mOutGainSmoother = NULL;

    mCustomDrawer = NULL;
    mPanogramCustomDrawerState = NULL;
    
    mCustomControl = NULL;
    
#if !USE_PSEUDO_STEREO_OBJ
    mDelayObj = NULL;
#endif
    
#if USE_PSEUDO_STEREO_OBJ
    mPseudoStereoObj = NULL;
#endif
    
#if STEREO_WIDEN_PARAM_SMOOTHERS
    mStereoWidthSmoother = NULL;

    mPanSmoother = NULL;
#endif
  
    mTimeAxis = NULL;
    mHAxis = NULL;

    mVAxis = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectroDisplayScrollState = NULL;
    
    mPrevSampleRate = GetSampleRate();
    //mPrevSampleRate = -1;
    
    // From Waves
    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    mPlayButton = NULL;
    mStereoWidthKnob = NULL;
    mPanKnob = NULL;
    mMonoToStereoButton = NULL;
    
    // Dummy values, to avoid undefine (just in case)
    mGraphWidthSmall = 256;
    mGraphHeightSmall = 256;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;

    mIsPlaying = false;
    mWasPlaying = false;
    
    mIsInitialized = false;

    mGraph = NULL;    
    mGUIHelper = NULL;

    mBypassDetector = NULL;
    mTransport = NULL;

    mMustUpdateTimeAxis = true;

    mLatency = 0;
    mPrevLatency = 0;
    
    mViewOrientation = SpectrogramDisplayScroll4::HORIZONTAL;

    mPlay = false;
    mPlayBarPos = 0.0;
}

void
Panogram::InitParams()
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
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 5);
#else
    GetParam(kColorMap)->InitEnum("ColorMap", 0, 6, "", IParam::kFlagsNone,
                                  "", "Blue", "Wasp", "Green",
                                  "Rainbow", "Dawn", "Sweet");
#endif
    
    // Sharpness
    BL_FLOAT defaultSharpness = 0.0;
    mSharpness = defaultSharpness;
    GetParam(kSharpness)->InitDouble("Sharpness", defaultSharpness,
                                     0.0, 100, 1.0, "%",
                                     0, "", IParam::ShapePowCurve(2.0));
    
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
    
    
    //int  defaultScrollSpeed = 2;
    //mScrollSpeedNum = defaultScrollSpeed;
    //GetParam(kScrollSpeed)->InitInt("ScrollSpeed", defaultScrollSpeed, 0, 2);
    
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");

    //
    mFreeze = false;
    //GetParam(kFreeze)->InitInt("Freeze", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kFreeze)->InitEnum("Freeze", 0, 2,
                                "", IParam::kFlagMeta, "",
                                "Off", "On");

    mPlay = false;
    //GetParam(kPlayStop)->InitInt("PlayStop", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kPlayStop)->InitEnum("PlayStop", 0, 2,
                                  "", IParam::kFlagMeta, "",
                                  "Stop", "Play");
 
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain,
                                   -16.0, 16.0, 0.01, "dB");
    
    // Stereo widen
    mStereoWidth = 0.0;
    GetParam(kStereoWidth)->InitDouble("StereoWidth", 0.0, -100.0, 100.0, 0.01, "%");

    mPan = 0.0;
    GetParam(kPan)->InitDouble("Pan", 0.0, -100.0, 100.0, 0.01, "%");

    mMonoToStereo = false;
    //GetParam(kMonoToStereo)->InitInt("Mono2Stereo", 0, 0, 1);
    GetParam(kMonoToStereo)->InitEnum("Mono2Stereo", 0, 2,
                                      "", IParam::kFlagsNone, "",
                                      "Off", "On");

    SpectrogramDisplayScroll4::ViewOrientation defaultViewOrientation =
        SpectrogramDisplayScroll4::HORIZONTAL;
    mViewOrientation = defaultViewOrientation;
    //GetParam(kViewOrientation)->
    //    InitInt("ViewOrientation", defaultViewOrientation, 0, 1);
    GetParam(kViewOrientation)->
        InitEnum("ViewOrientation", defaultViewOrientation, 2,
                 "", IParam::kFlagsNone, "",
                 "Horizontal", "Vertical");
}

void
Panogram::ApplyParams()
{
    if (mPanogramObj != NULL)
    {
        BLSpectrogram4 *spectro = mPanogramObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }
    
    if (mPanogramObj != NULL)
    {
        BLSpectrogram4 *spectro = mPanogramObj->GetSpectrogram();
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
    
    //SetScrollSpeed(mScrollSpeedNum);

    if (mMonitorEnabled && !mIsPlaying)
        mTransport->SetDAWTransportValueSec(0.0);
    
    if (mPanogramObj != NULL)
    {
        mPanogramObj->SetSharpness(mSharpness);
    }
    
    SetFreezeFromApplyParams(mFreeze);
    SetPlayStopFromApplyParams(mPlay);

    // Out gain
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
    
#if STEREO_WIDEN_PARAM_SMOOTHERS
    if (mStereoWidthSmoother != NULL)
        mStereoWidthSmoother->ResetToTargetValue(mStereoWidth);
#endif
            
#if STEREO_WIDEN_PARAM_SMOOTHERS
    if (mPanSmoother != NULL)
        mPanSmoother->ResetToTargetValue(mPan);
#endif
        
    // Mono to stereo
    UpdateLatency();
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Panogram::Init(int oversampling, int freqRes)
{    
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> dispProcessObjs;
        for (int i = 0; i < 2; i++)
        {
            mPanogramPlayFftObjs[i] = new PanogramPlayFftObj(BUFFER_SIZE,
                                                             oversampling,
                                                             freqRes, sampleRate);
            dispProcessObjs.push_back(mPanogramPlayFftObjs[i]);
        }
        
        int numChannels = 2;
        int numScInputs = 0;
        
        mFftObj = new FftProcessObj16(dispProcessObjs,
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

        //
        mPanogramObj = new PanogramFftObj(BUFFER_SIZE, oversampling, freqRes,
                                          sampleRate);
        
        mPanogramObj->SetPlayFftObjs(mPanogramPlayFftObjs);
        
        // Must artificially call reset because MultiProcessObjs does not reset
        // when FftProcessObj resets
        mPanogramObj->Reset(BUFFER_SIZE, oversampling, freqRes,
                            sampleRate);
        
        mFftObj->AddMultichannelProcess(mPanogramObj);
        
        mFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                        KEEP_SYNTHESIS_ENERGY);
    }
    else
    {
        mFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mPanogramObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);

        for (int i = 0; i < 2; i++)
            mPanogramPlayFftObjs[i]->Reset(BUFFER_SIZE, oversampling,
                                           freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);

    //
    mDelayObj = NULL;    
#if !USE_PSEUDO_STEREO_OBJ
    BL_FLOAT delaySamples = (MONO2STEREO_DELAY/1000.0)*sampleRate;
    mDelayObj = new DelayObj4(delaySamples);
#endif
    
    mPseudoStereoObj = NULL;
#if USE_PSEUDO_STEREO_OBJ
    mPseudoStereoObj = new PseudoStereoObj2(sampleRate);
#endif
    
    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);

#if STEREO_WIDEN_PARAM_SMOOTHERS
    BL_FLOAT defaultStereoWidth = 0.0;
    mStereoWidthSmoother = new ParamSmoother2(sampleRate, defaultStereoWidth);
  
    BL_FLOAT defaultPan = 0.0;
    mPanSmoother = new ParamSmoother2(sampleRate, defaultPan);
#endif

#if USE_RESAMPLER
    BL_FLOAT sampleRate = GetSampleRate();
    CheckSampleRate(&sampleRate);
    mPrevSampleRate = sampleRate; // Dangerous
    
    // WDL_Resampler::SetMode arguments are bool interp, int filtercnt, bool sinc, int sinc_size, int sinc_interpsize
    // sinc mode will get better results, but will use more cpu
    // todo: explain arguments
    mResampler.SetMode(true, 1, false, 0, 0);
    mResampler.SetFilterParms();
    // set it output driven
    mResampler.SetFeedMode(false);
    // set input and output samplerates
    mResampler.SetRates(44100., GetSampleRate());
#endif
    
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
Panogram::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
#if 0 // DEBUG
    static double prevTime = -1.0;
    static double prevSR = -1.0;
    double rate = BLDebug::ComputeRealSampleRate(&prevTime, &prevSR, nFrames);
    fprintf(stderr, "samples rate: %g\n", rate);
#endif
    
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

#if !SET_LATENCY_IN_GUI_THREAD
    if (mLatency != mPrevLatency)
    {
        SetLatency(mLatency);
        mPrevLatency = mLatency;
    }
#endif
    
    if (mGraph != NULL)
        mGraph->Lock();
    
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
            mGraph->Unlock();

        if (mGraph != NULL)
            mGraph->PushAllData();
        
        return;
    }

    // Duplicate mono channel if necessary
    if (in.size() == 1)
        in.push_back(in[0]);
  
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    mIsPlaying = isPlaying;

#if HACK_FOR_MONITORING
    isPlaying = true;
#endif

    if (mUIOpened)
    { 
        if (mTransport != NULL)
        {
            mTransport->SetTransportPlaying(isPlaying && !mFreeze,
                                            mMonitorEnabled && !mFreeze,
                                            transportTime, nFrames);
        }
        
        if (!(isPlaying && !mFreeze) && mWasPlaying && (mMonitorEnabled && !mFreeze))
            // Playing just stops and monitor is enabled
        {
            // => Reset time axis time
            if (mTransport != NULL)
                mTransport->SetDAWTransportValueSec(0.0);
        }
    }

    mWasPlaying = isPlaying;
    
#if FIX_FREEZE_FFT_OBJ
    // Stop getting values and mask values when we freeze
    bool enablePanoFftObj = !mFreeze && (isPlaying || mMonitorEnabled);
    mPanogramObj->SetEnabled(enablePanoFftObj);
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

#if USE_RESAMPLER
    if (mUseResampler)
    {
        if (in.size() == 2)
        {
            // FIX: with sample rate 88200Hz, the performances will drop
            // progressively while playing
            //
            // This was due to accumulation of samples in the second channel 
            //
            // Keep only the first channel
            // So next when call to mDispFftObj->Process(in, dummy, &out);
            // This will void haveing two input channels of different size
            
            for (int k = 0; k < 2; k++)
            {
                WDL_ResampleSample *resampledAudio=NULL;
                int numSamples = nFrames*44100.0/GetSampleRate();
                int numSamples0 =
                    mResamplers[k].ResamplePrepare(numSamples, 1, &resampledAudio);
                for (int i = 0; i < numSamples0; i++)
                {
                    if (i >= nFrames)
                        break;
                    resampledAudio[i] = in[k].Get()[i];
                }
      
                WDL_TypedBuf<BL_FLOAT> &outSamples = mTmpBuf3;
                outSamples.Resize(numSamples);
                int numResampled = mResamplers[k].ResampleOut(outSamples.Get(),
                                                              nFrames, numSamples, 1);
                if (numResampled != numSamples)
                {
                    //failed somehow
                    memset(outSamples.Get(), 0, numSamples*sizeof(BL_FLOAT));
                }
        
                in[k] = outSamples;
            }
        }
    }
#endif

    // Stereo widen
    StereoWiden(&in);
    
    // Play
    //
    vector<WDL_TypedBuf<BL_FLOAT> > dummy;
          
    // If setting the host input as input, we would have some
    // host sound additionnaly to the inner sound played
    // in edit mode
    vector<WDL_TypedBuf<BL_FLOAT> > &playIn = mTmpBuf4;

    // Anyway, we must set playIn to the right size
    // Otherwise later when calling mFftObj->Process(playIn, ...),
    // the size of playIn, and then number of requested samples may be wrong
    // (can happpen when changing buffer size while in freeze mode)
    playIn = in;
    
    if ((isPlaying || mMonitorEnabled) && !mFreeze)
    {
        //playIn = in;
        
        for (int i = 0; i < 2; i++)
            mPanogramPlayFftObjs[i]->SetMode(PanogramPlayFftObj::RECORD);
    }
    
    if (mFreeze)
    {
        for (int i = 0; i < 2; i++)
            mPanogramPlayFftObjs[i]->SetMode(PanogramPlayFftObj::PLAY);
    }
    
    if (!isPlaying && !mFreeze && !mMonitorEnabled)
    {
        for (int i = 0; i < 2; i++)
            mPanogramPlayFftObjs[i]->SetMode(PanogramPlayFftObj::BYPASS);
    }
    
    for (int i = 0; i < 2; i++)
        mPanogramPlayFftObjs[i]->SetHostIsPlaying(isPlaying || mMonitorEnabled);
    
    vector<WDL_TypedBuf<BL_FLOAT> > &playOut = mTmpBuf5;
    playOut = out;
    
    mFftObj->Process(playIn, dummy, &playOut);
    
    // Test, to avoid playing a single line in loop when play button is off
    if ((isPlaying && !mFreeze) ||
        (mMonitorEnabled && !mFreeze) ||
        (mPlay && mFreeze))
        out = playOut; // Normal behavior

#if 1 // NEW
    if (mPlay)
    {
        if (mPanogramPlayFftObjs[0] != NULL)
            mPlayBarPos = mPanogramPlayFftObjs[0]->GetSelPlayPosition();
    }
#endif
    
#if 0 // Moved to OnIdle
    if (mPlay)
    {
        UpdatePlayBar();
    }
    else
    {
        if (mCustomDrawer != NULL)
            mCustomDrawer->SetPlayBarActive(false);
    }
#endif

    if (mPanogramPlayFftObjs[0] != NULL)
    {
        if (mPanogramPlayFftObjs[0]->SelectionPlayFinished())
        {
            ResetPlayBar();
        }
    }
    
    // Update only if it is playing,
    // otherwise the view would continue to scroll when transport is stopped
    if ((isPlaying || mMonitorEnabled) && !mFreeze)
    {
        mMustUpdateSpectrogram = true;
    }
    
    if (mCustomDrawer != NULL)
    {
        if (!mFreeze && !mCustomDrawer->IsSelectionActive())
            // Simply play, without selection rectangle
        {
            // Simply play the sound, without getting the result from FFtProcessObj
            // This way, we avoid making latency for nothing
            out = in;
        }
    }

    // Apply out gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
        
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

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
        mGraph->Unlock();

    if (mGraph != NULL)
        mGraph->PushAllData();
    
    BL_PROFILE_END;
}

void
Panogram::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);
    
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
    
    // Graph drawer
    mGraphDrawer = new PanogramGraphDrawer();
    mGraphDrawer->SetViewOrientation((PanogramGraphDrawer::ViewOrientation)
                                     mViewOrientation);
    mGraph->AddCustomDrawer(mGraphDrawer);
    
    // Custom drawer and control
    mCustomDrawer = new PanogramCustomDrawer(this, 0.0, 0.0, 1.0, 1.0,
                                             mPanogramCustomDrawerState);
    mCustomDrawer->SetViewOrientation((PanogramCustomDrawer::ViewOrientation)
                                      mViewOrientation);
    mPanogramCustomDrawerState = mCustomDrawer->GetState();
    mGraph->AddCustomDrawer(mCustomDrawer);

    if (mCustomControl == NULL)
        mCustomControl = new PanogramCustomControl(this);
    mCustomControl->SetViewOrientation((PanogramCustomControl::ViewOrientation)
                                       mViewOrientation);
    mGraph->AddCustomControl(mCustomControl);

    if (mSpectrogramDisplay != NULL)
        mCustomControl->SetSpectrogramDisplay(mSpectrogramDisplay);
    
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

    // Sharpness
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSharpnessX, kSharpnessY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSharpness,
                              TEXTFIELD_FN,
                              "SHARPNESS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSharpness);
    
    // ColorMap num
#if !USE_DROP_DOWN_MENU// Radio buttons
    const char *colormapRadioLabels[kRadioButtonColorMapNumButtons] =
        { "BLUE", "WASP", "GREEN", "RAINBOW", "DAWN", "SWEET" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsColorMapX,
                                   kRadioButtonsColorMapY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonColorMapNumButtons,
                                   kRadioButtonColorMapVSize,
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
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kFreezeX,
                                   kFreezeY + offset,
                                   CHECKBOX_FN, kFreeze, "FREEZE",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipFreeze);

    // Play button
    mPlayButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                   kPlayStopX, kPlayStopY + offset,
                                                   BUTTON_PLAY_FN,
                                                   kPlayStop,
                                                   "", true,
                                                   true, false,
                                                   tooltipPlayStop);
    mPlayButton->SetDisabled(!mFreeze);

    // Out gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX, kOutGainY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipOutGain);

    // Stereo widen
    //
    
    // Width
    mStereoWidthKnob = mGUIHelper->CreateKnobSVG(pGraphics,
                                                 kStereoWidthX,
                                                 kStereoWidthY + offset,
                                                 kKnobWidth, kKnobHeight,
                                                 KNOB_FN,
                                                 kStereoWidth,
                                                 TEXTFIELD_FN,
                                                 "WIDTH",
                                                 GUIHelper12::SIZE_DEFAULT,
                                                 NULL, true,
                                                 tooltipStereoWidth);
    
    //mStereoWidthKnob->SetDisabled(mFreeze);

    // Also disable knob value automatically
    pGraphics->DisableControl(kStereoWidth, mFreeze);
        
    // Pan
    mPanKnob = mGUIHelper->CreateKnobSVG(pGraphics,
                                         kPanX, kPanY + offset,
                                         kKnobSmallWidth, kKnobSmallHeight,
                                         KNOB_SMALL_FN,
                                         kPan,
                                         TEXTFIELD_FN,
                                         "PAN",
                                         GUIHelper12::SIZE_DEFAULT,
                                         NULL, true,
                                         tooltipPan);
    
    //mPanKnob->SetDisabled(mFreeze);
    pGraphics->DisableControl(kPan, mFreeze);
 
    mMonoToStereoButton = mGUIHelper->CreateToggleButton(pGraphics,
                                                         kMonoToStereoX,
                                                         kMonoToStereoY + offset,
                                                         CHECKBOX_FN, kMonoToStereo,
                                                         "MONO->ST",
                                                         GUIHelper12::SIZE_SMALL,
                                                         true,
                                                         tooltipMonoToStereo);
    mMonoToStereoButton->SetDisabled(mFreeze);
    
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
    
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kMonitorX,
                                                     kMonitorY + offset,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT,
                                                     true,
                                                     tooltipMonitor);

    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kCheckboxViewOrientationX,
                                     kCheckboxViewOrientationY + offset,
                                     BUTTON_DOWN_ARROW_FN,
                                     kViewOrientation, "ORIENTATION",
                                     true,
                                     true, false,
                                     tooltipviewOrientation);
    
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
Panogram::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
Panogram::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    if (mTransport != NULL)
        mTransport->Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    if (mStereoWidthSmoother != NULL)
        mStereoWidthSmoother->Reset(sampleRate);
    if (mPanSmoother != NULL)
        mPanSmoother->Reset(sampleRate);
    
#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif

    if (sampleRate != mPrevSampleRate)
    {
        mPrevSampleRate = sampleRate;

        if (mPanogramObj != NULL)
            mPanogramObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);

        for (int i = 0; i < 2; i++)
        {
            mPanogramPlayFftObjs[i]->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);
        }

        if (mCustomDrawer != NULL)
            mCustomDrawer->Reset();
        if (mCustomControl != NULL)
            mCustomControl->Reset();

        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->Reset();
    }//
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);
    
    if (mTransport != NULL)
        mTransport->Reset(sampleRate);
    
    // Panogram play obj
    if (mPanogramObj != NULL)
    {
        int numSpectroCols = mPanogramObj->GetNumColsAdd();

        for (int i = 0; i < 2; i++)
        {
            if (mPanogramPlayFftObjs[i] != NULL)
                mPanogramPlayFftObjs[i]->SetNumCols(numSpectroCols);
        }
    }
    
#if !USE_PSEUDO_STEREO_OBJ
    if (mDelayObj != NULL)
    {
        BL_FLOAT delaySamples = (MONO2STEREO_DELAY/1000.0)*sampleRate;
        mDelayObj->SetDelay(delaySamples);
    }
#endif
    
#if USE_PSEUDO_STEREO_OBJ
    if (mPseudoStereoObj != NULL)
    {
        int blockSize = GetBlockSize();
        mPseudoStereoObj->Reset(sampleRate, blockSize);
    }
#endif
    
    // Latency
    UpdateLatency();
  
    UpdateTimeAxis();

#if USE_RESAMPLER
    for (int i = 0; i < 2; i++)
    {
        if (mResamplers[i] != NULL)
        {
            mResamplers[i].Reset();
            // Set input and output samplerates
            mResamplers[i].SetRates(GetSampleRate(), 44100.0);
        }
    }
#endif
  
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Panogram::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mPanogramObj != NULL)
            {
                BLSpectrogram4 *spectro = mPanogramObj->GetSpectrogram();
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
            
            if (mPanogramObj != NULL)
            {
                BLSpectrogram4 *spectro = mPanogramObj->GetSpectrogram();
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
    
        case kSharpness:
        {
            BL_FLOAT sharpness = GetParam(paramIdx)->Value();
            sharpness /= 100.0;
            mSharpness = sharpness;
            
            if (mPanogramObj != NULL)
                mPanogramObj->SetSharpness(mSharpness);
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
        
        /*case kScrollSpeed:
          {
          mScrollSpeedNum = GetParam(kScrollSpeed)->Int();
          
          SetScrollSpeed(mScrollSpeedNum);
          }
          break;
        */
        
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
                    
                    if ((mIsPlaying && !mFreeze) || mMonitorEnabled)
                        mMustUpdateTimeAxis = true;
                }
            }
        }
        break;

        case kFreeze:
        {
            int value = GetParam(kFreeze)->Value();
            bool freeze = (value == 1);
        
            mFreeze = freeze;
            SetFreezeFromParamChange(mFreeze);
        }
        break;
       
        case kPlayStop:
        {
            int val = GetParam(kPlayStop)->Int();
        
            mPlay = (val == 1);
        
            SetPlayStopFromParamChange(mPlay);
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
        
        // Stereo widen
        case kStereoWidth:
        {
            BL_FLOAT value = GetParam(kStereoWidth)->Value();

            mStereoWidth = value/100.0;
            
#if STEREO_WIDEN_PARAM_SMOOTHERS
            if (mStereoWidthSmoother != NULL)
                mStereoWidthSmoother->SetTargetValue(mStereoWidth);
#endif
        }
        break;
          
        case kPan:
        {
            BL_FLOAT value = GetParam(kPan)->Value();

            mPan = value/100.0;
            
#if STEREO_WIDEN_PARAM_SMOOTHERS
            if (mPanSmoother != NULL)
                mPanSmoother->SetTargetValue(mPan);
#endif
        }
        break;
       
        case kMonoToStereo:
        {
            int value = GetParam(kMonoToStereo)->Int();
        
            mMonoToStereo = (value == 1);
        
            UpdateLatency();
        }
        break;

        case kViewOrientation:
        {
            int value = GetParam(paramIdx)->Int();
          
            mViewOrientation = (SpectrogramDisplayScroll4::ViewOrientation)value;

            if (mSpectrogramDisplay != NULL)
                mSpectrogramDisplay->SetViewOrientation(mViewOrientation);

            if (mGraphDrawer != NULL)
                mGraphDrawer->
                    SetViewOrientation((PanogramGraphDrawer::ViewOrientation)
                                       mViewOrientation);

            UpdateAxesViewOrientation();

            if (mCustomDrawer != NULL)
                mCustomDrawer->
                    SetViewOrientation((PanogramCustomDrawer::ViewOrientation)
                                       mViewOrientation);

            if (mCustomControl != NULL)
                mCustomControl->
                    SetViewOrientation((PanogramCustomControl::ViewOrientation)
                                       mViewOrientation);

            if (mCustomControl != NULL)
                mCustomControl->
                    SetViewOrientation((PanogramCustomControl::ViewOrientation)
                                       mViewOrientation);
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Panogram::SetFreezeFromParamChange(bool freeze)
{
    mPlay = false;
    
    if (!freeze)
    {
        // If we toggle off freeze, set the play button to "stop"
        SetParameterValue(kPlayStop, 0.0);
        if (mPlayButton != NULL)
            mPlayButton->SetValue(0.0);
        
        SetBarActive(false);
        
#if 1 // FIX: with no selection, freeze on, freeze off, play
        // => the sound is played in the middle instead of on the right of the graph
        if (mCustomDrawer != NULL)
        {
            if (!mCustomDrawer->IsSelectionActive())
            {
                for (int i = 0; i < 2; i++)
                    mPanogramPlayFftObjs[i]->SetSelectionEnabled(false);
            }
        }
#endif
    }
    
    if (freeze)
    {
        // Set the play button to "stop" even when toggleing freeze on
        // This avoids having the playbar moving jut at startup
        // For this, kPlayStop must be declared before kFreeze,
        //SetParameterFromGUI(kPlayStop, 0.0);
        SetParameterValue(kPlayStop, 0.0);

        if (mPlayButton != NULL)
            mPlayButton->SetValue(0.0);
    }
    
    mFreeze = freeze;
    
    if (mPlayButton != NULL)
        mPlayButton->SetDisabled(!mFreeze);
    
    if (mFreeze)
    {
        // If we don't have a current selection,
        // reset the previous bar state
        if (mCustomDrawer != NULL)
        {
            if (!mCustomDrawer->IsSelectionActive())
            {
                for (int i = 0; i < 2; i++)
                {
                    if (mPanogramPlayFftObjs[i] != NULL)
                        mPanogramPlayFftObjs[i]->RewindToNormValue(0.0);
                }
                
                BarSetSelection(0.0);
                SetBarPos(0.0);
            }
        }
        
        // FIX: avoid a scroll jump after having freezed,
        // at the moment we start to draw a selection
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->UpdateSpectrogram(true);
    }
    
    // Stereo widen gray
    if (mStereoWidthKnob != NULL)
    {
        //mStereoWidthKnob->SetDisabled(mFreeze);

        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kStereoWidth, mFreeze);
    }
    
    if (mPanKnob != NULL)
    {
        //mPanKnob->SetDisabled(mFreeze);

        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kPan, mFreeze);
    }
    
    if (mMonoToStereoButton != NULL)
        mMonoToStereoButton->SetDisabled(mFreeze);
    
    if (mMonitorControl != NULL)
        mMonitorControl->SetDisabled(mFreeze || mIsPlaying);
}

void
Panogram::SetFreezeFromApplyParams(bool freeze)
{
    mFreeze = freeze;
    
    // Stereo widen gray
    if (mStereoWidthKnob != NULL)
    {
        //mStereoWidthKnob->SetDisabled(mFreeze);

        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kStereoWidth, mFreeze);
    }
    
    if (mPanKnob != NULL)
    {
        //mPanKnob->SetDisabled(mFreeze);

        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kPan, mFreeze);
    }
    
    if (mMonoToStereoButton != NULL)
        mMonoToStereoButton->SetDisabled(mFreeze);
    
    if (mMonitorControl != NULL)
        mMonitorControl->SetDisabled(mFreeze || mIsPlaying);

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetViewOrientation(mViewOrientation);

    if (mGraphDrawer != NULL)
        mGraphDrawer->SetViewOrientation((PanogramGraphDrawer::ViewOrientation)
                                         mViewOrientation);

    UpdateAxesViewOrientation();

    if (mCustomDrawer != NULL)
        mCustomDrawer->SetViewOrientation((PanogramCustomDrawer::ViewOrientation)
                                          mViewOrientation);

    if (mCustomControl != NULL)
        mCustomControl->SetViewOrientation((PanogramCustomControl::ViewOrientation)
                                           mViewOrientation);
}

void
Panogram::SetPlayStopFromParamChange(bool playFlag)
{
    for (int i = 0; i < 2; i++)
        mPanogramPlayFftObjs[i]->SetIsPlaying(playFlag);
    
#if 1   // IMPROV: play, stop, play => the playbar now restarts at the beginning
        // of the selection of the bar
    if (mPlay)
    {
        ResetPlayBar();
    }
#endif
}

void
Panogram::SetPlayStopFromApplyParams(bool playFlag)
{
    for (int i = 0; i < 2; i++)
        mPanogramPlayFftObjs[i]->SetIsPlaying(playFlag);
    
    BL_FLOAT value = 0.0;
    if (mPlay)
        value = 1.0;
    
    SetParameterValue(kPlayStop, value);
    if (mPlayButton != NULL)
        mPlayButton->SetValue(value);
}

void
Panogram::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // NOTE: Panogram specific
    //
    // Test, in order to update the time axis only at startup,
    // not when closed and re-opened
    if (mMustUpdateTimeAxis)
    {
        UpdateTimeAxis();
        mMustUpdateTimeAxis = false;
    }
    
    //SetScrollSpeed(mScrollSpeedNum);
    
    LEAVE_PARAMS_MUTEX;
}

void
Panogram::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mMonitorControl = NULL;

    mGraph = NULL;

    mPlayButton = NULL;
    mStereoWidthKnob = NULL;
    mPanKnob = NULL;
    mMonoToStereoButton = NULL;
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    mSpectrogramDisplay = NULL;
    if (mPanogramObj != NULL)
        mPanogramObj->SetSpectrogramDisplay(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;

    mCustomDrawer = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Panogram::SetColorMap(int colorMapNum)
{
    if (mPanogramObj != NULL)
    {
        BLSpectrogram4 *spec = mPanogramObj->GetSpectrogram();
    
        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[6] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_WASP,
            ColorMapFactory::COLORMAP_GREEN,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_DAWN_FIXED,
            ColorMapFactory::COLORMAP_SWEET
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

/*void
  Panogram::SetScrollSpeed(int scrollSpeedNum)
  {
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
    
  if (mPanogramObj != NULL)
  {
  mPanogramObj->SetSpeedMod(speedMod);
        
  if (mSpectrogramDisplay != NULL)
  {
  mSpectrogramDisplay->SetSpeedMod(speedMod);
  }
        
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
*/

void
Panogram::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT }
        ,{ GUI_WIDTH_MEDIUM, GUI_HEIGHT_MEDIUM }
        ,{ GUI_WIDTH_BIG, GUI_HEIGHT_BIG }
    };
    
    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
Panogram::PreResizeGUI(int guiSizeIdx,
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
    
    mSpectrogramDisplay = NULL;
    mPanogramObj->SetSpectrogramDisplay(NULL);

    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
Panogram::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] =
        { kGUISizeSmall
          ,kGUISizeMedium
          ,kGUISizeBig
        };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
    { mGUISizeSmallButton
      ,mGUISizeMediumButton
      ,mGUISizeBigButton
    };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

void
Panogram::OnIdle()
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
            if (disabled != (mIsPlaying || mFreeze))
                mMonitorControl->SetDisabled(mIsPlaying || mFreeze);
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

        //#if !RESET_TIME_AXIS_WHEN_NO_PLAY
        if ((mIsPlaying && !mFreeze) || mMonitorEnabled)
            //#endif
        {
            if (mMustUpdateTimeAxis)
            {
                //ENTER_PARAMS_MUTEX;
                
                UpdateTimeAxis();
                
                //LEAVE_PARAMS_MUTEX;
                
                mMustUpdateTimeAxis = false;
            }
        }

#if 1
        if (mPlay)
        {
            UpdatePlayBar();
        }
        else
        {
            if (mCustomDrawer != NULL)
                mCustomDrawer->SetPlayBarActive(false);
        }
#endif
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Panogram::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    // Axis
    BLSpectrogram4 *spectro = mPanogramObj->GetSpectrogram();
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
    // (there is a small width scale in SpectrogramDsiplayScroll,
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
Panogram::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mPanogramObj == NULL))
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    mSpectrogram = mPanogramObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    mSpectrogram->SetYScale(Scale::LINEAR);
    mSpectrogram->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        bool stateWasNull = (mSpectroDisplayScrollState == NULL);
        mSpectrogramDisplay =
            new SpectrogramDisplayScroll4(mSpectroDisplayScrollState);
        mSpectroDisplayScrollState = mSpectrogramDisplay->GetState();

        if (mSpectrogramDisplay != NULL)
        {
            if (stateWasNull)
            {
                mSpectrogramDisplay->SetSpectrogram(mSpectrogram, 0.0, 0.0, 1.0, 1.0);
                mSpectrogramDisplay->SetTransport(mTransport);
                
                mSpectrogramDisplay->SetViewOrientation(mViewOrientation);
                
                // Use the sample rate possibly modified by CheckSampleRate()
                mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING,
                                                  sampleRate);
            }
        
            mPanogramObj->SetSpectrogramDisplay(mSpectrogramDisplay);
            mGraph->AddCustomDrawer(mSpectrogramDisplay);
        }
    }
}

void
Panogram::CreateGraphAxes()
{
    //bool firstTimeCreate = (mHAxis == NULL);
    
    // Create

    // Time axis
    //
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mTimeAxis = new GraphTimeAxis6(false, false);
        mTimeAxis->SetTransport(mTransport);
        
        mTimeAxis->Init(mGraph, mHAxis, mGUIHelper,
                        BUFFER_SIZE, 1.0, MAX_NUM_TIME_AXIS_LABELS);
    }
    else
    {
        mGraph->SetGraphTimeAxis(mTimeAxis);
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    
    // L/R axis
    //
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();

        int axisColor[4];
        mGUIHelper->GetGraphAxisColor(axisColor);
    
        int axisOverlayColor[4];
        mGUIHelper->GetGraphAxisOverlayColor(axisOverlayColor);
    
        int axisLabelColor[4];
        mGUIHelper->GetGraphAxisLabelColor(axisLabelColor);
    
        int axisLabelOverlayColor[4];
        mGUIHelper->GetGraphAxisLabelOverlayColor(axisLabelOverlayColor);
    
        BL_GUI_FLOAT lineWidth = mGUIHelper->GetGraphAxisLineWidthBold();


        bool displayLines = false;
        if (!displayLines)
        {
            axisColor[3] = 0;
            axisOverlayColor[3] = 0;
        }
        
        BL_FLOAT fontSizeCoeff = 2.0;
        
        int graphWidth;
        int graphHeight;
        mGraph->GetSize(&graphWidth, &graphHeight);
        
        mVAxis->InitVAxis(Scale::LINEAR,
                          0.0, 1.0,
                          axisColor, axisLabelColor,
                          lineWidth,
                          (graphWidth - 40.0)/graphWidth, 0.0,
                          axisLabelOverlayColor,
                          fontSizeCoeff,
                          false,
                          axisOverlayColor);
    
        // Vertical axis: L/R
        //
#define NUM_VAXIS_DATA 4
        static char *VAXIS_DATA[NUM_VAXIS_DATA][2] =
        {
            { "0.0", "" },
            { "0.0416", "R" },
            { "0.9583", "L" },
            { "1.0", "" }
        };
        
        mVAxis->SetData(VAXIS_DATA, NUM_VAXIS_DATA);
    }
    
    // Update
    mGraph->SetVAxis(mVAxis);

    UpdateAxesViewOrientation();
}

#if USE_RESAMPLER
// If sample rate is too high, force to 44100 and enable resamling
void
Panogram::CheckSampleRate(BL_FLOAT *ioSampleRate)
{
    mUseResampler = false;
    
    if (*ioSampleRate > 48000.0)
    {
        mUseResampler = true;
        
        *ioSampleRate = 44100.0;
    }
}
#endif

void
Panogram::UpdateSpectroPlay()
{
    BL_FLOAT x0 = 0.0;
    BL_FLOAT y0 = 0.0;
    BL_FLOAT x1 = 1.0;
    BL_FLOAT y1 = 1.0;
    
#if !FIX_BAR_PLAY_BAR_SPEED
    if ((mCustomDrawer->IsSelectionActive())
#else
    if ((mCustomDrawer->IsSelectionActive()) ||
        mCustomDrawer->IsBarActive())
#endif
        mCustomDrawer->GetSelection(&x0, &y0, &x1, &y1);

#if FIX_SPECTRO_SCROLL4_SELECTION
     if (mSpectrogramDisplay != NULL)
     {
         // Manage the fact tha the spectro scroll 4 scales
         // the spectrogram, to keep invisible margins on the left and on the right
         // (to hide black borders)
         BL_FLOAT tn0;
         BL_FLOAT tn1;
         mSpectrogramDisplay->GetTimeBoundsNorm(&tn0, &tn1);
             
         x0 = (x0 + tn0)/(1.0 + tn0 + (1.0 - tn1));
         x1 = (x1 + tn0)/(1.0 + tn0 + (1.0 - tn1));
     }
#endif
    
     for (int i = 0; i < 2; i++)
        mPanogramPlayFftObjs[i]->SetNormSelection(x0, y0, x1, y1);
}

void
Panogram::StereoWiden(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    if (samples->empty())
        return;
    
    if (mMonoToStereo)
    {
#if !USE_PSEUDO_STEREO_OBJ
        StereoWidenProcess::MonoToStereo(samples, mDelayObj);
#else
        mPseudoStereoObj->ProcessSamples(samples);
#endif
    }
    
    vector<WDL_TypedBuf<BL_FLOAT> *> samples0;
    samples0.resize(2);
    samples0[0] = &(*samples)[0];
    samples0[1] = &(*samples)[1];
    
#if !STEREO_WIDEN_PARAM_SMOOTHERS
    StereoWidenProcess::StereoWiden(&samples0, mStereoWidth);
    StereoWidenProcess::Balance(&samples0, mPan);
#else
    if (mStereoWidthSmoother != NULL)
        StereoWidenProcess::StereoWiden(&samples0, mStereoWidthSmoother);
    if (mPanSmoother != NULL)
        StereoWidenProcess::Balance(&samples0, mPanSmoother);
#endif
}

void
Panogram::UpdateLatency()
{
    if (mPseudoStereoObj == NULL)
        return;
    
    if (mMonoToStereo)
    {
#if !USE_PSEUDO_STEREO_OBJ
        BL_FLOAT sampleRate = GetSampleRate();
        
        // Mono to stereo adds a latency of 20ms
        int latency = (MONO2STEREO_DELAY/1000.0)*sampleRate;
#else
        int latency = mPseudoStereoObj->GetLatency();
#endif
        
#if SET_LATENCY_IN_GUI_THREAD
        SetLatency(latency);
#else
        mLatency = latency;
#endif
    }
    else
    {
#if SET_LATENCY_IN_GUI_THREAD
        SetLatency(0);
#else
        mLatency = 0;
#endif
    }
}
 
void
Panogram::SetBarActive(bool flag)
{
    if (!mFreeze && flag)
        return;
    
    if (mCustomDrawer != NULL)
        mCustomDrawer->SetBarActive(flag);
}

bool
Panogram::IsBarActive()
{
    if (mCustomDrawer == NULL)
        return false;
    
    bool barActive = mCustomDrawer->IsBarActive();
    
    return barActive;
}

BL_FLOAT
Panogram::GetBarPos()
{
    BL_FLOAT pos = mCustomDrawer->GetBarPos();
    return pos;
}
        
void
Panogram::SetBarPos(BL_FLOAT x)
{
    if (mGraph == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    if (!mFreeze)
        return;
        
    // Custom drawer
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    bool barActive = mCustomDrawer->IsBarActive();
    
    BL_FLOAT xf = ((BL_FLOAT)x)/width;
    mCustomDrawer->SetBarPos(xf);
    
    if (barActive)
        BarSetSelection(x);
    
    // But don't display this selection in the drawer
    // (we still have the bar displayed
    mCustomDrawer->ClearSelection();
    
    // Update for playing
    UpdateSpectroPlay();
}

void
Panogram::ResetPlayBar()
{
    if (!mFreeze)
        return;
        
    // Set the play bar to origin
    for (int i = 0; i < 2; i++)
    {
        if (mPanogramPlayFftObjs[i] != NULL)
            mPanogramPlayFftObjs[i]->RewindToStartSelection();
    }
}

void
Panogram::UpdatePlayBar()
{
    if (mPanogramPlayFftObjs[0] == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    if (!mPlay)
        return;
    
    // Set play bar pos every time
    // because when we stop playing, we want
    // to retrive it at the origin
    //BL_FLOAT playPos = mPanogramPlayFftObjs[0]->GetSelPlayPosition();
    mCustomDrawer->SetSelPlayBarPos(/*playPos*/mPlayBarPos);
}

void
Panogram::GetGraphSize(int *width, int *height)
{
    if (mGraph != NULL)
        mGraph->GetSize(width, height);
}

void
Panogram::UpdateSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                          bool updateCenterPos, bool activateDrawSelection,
                          bool updateCustomControl)
{
    if (mGraph == NULL)
        return;
    
    if (mCustomDrawer == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
#if FIX_CLIP_SELECTION
    ClipSelection(&x0, &y0, &x1, &y1, width, height);
#endif
    
    BL_FLOAT x0f;
    BL_FLOAT y0f;
    
    BL_FLOAT x1f;
    BL_FLOAT y1f;
    
    // Swap if necessary
    if (x1 < x0)
    {
        int tmp = x1;
        x1 = x0;
        x0 = tmp;
    }
    
    if (y1 < y0)
    {
        int tmp = y1;
        y1 = y0;
        y0 = tmp;
    }
    
    // NOTE: reorder here may be useless
    
    // Reorder if necessary
    if (x0 <= x1)
    {
        x0f = ((BL_FLOAT)x0)/width;
        x1f = ((BL_FLOAT)x1)/width;
    }
    else
    {
        x0f = ((BL_FLOAT)x1)/width;
        x1f = ((BL_FLOAT)x0)/width;
    }
    
    if (y0 <= y1)
    {
        y0f = ((BL_FLOAT)y0)/height;
        y1f = ((BL_FLOAT)y1)/height;
    }
    else
    {
        y0f = ((BL_FLOAT)y1)/height;
        y1f = ((BL_FLOAT)y0)/height;
    }
    
    mCustomDrawer->SetSelection(x0f, y0f, x1f, y1f);
    if (activateDrawSelection)
        mCustomDrawer->SetSelectionActive(true);
    
    if (updateCustomControl)
        mCustomControl->SetSelection(x0, y0, x1, y1);
    
    UpdateSpectroPlay();
}

void
Panogram::ClipSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                        BL_FLOAT *x1, BL_FLOAT *y1,
                        int width, int height)
{
    if (*x0 < 0.0)
        *x0 = 0.0;
    if (*x0 > width - 1)
        *x0 = width - 1;
    
    if (*x1 < 0.0)
        *x1 = 0.0;
    if (*x1 > width - 1)
        *x1 = width - 1;
    
    if (*y0 < 0.0)
        *y0 = 0.0;
    if (*y0 > height - 2)
        *y0 = height - 2;
    
    if (*y1 < 0.0)
        *y1 = 0.0;
    if (*y1 > height - 2)
        *y1 = height - 2;
}

void
Panogram::ClearBar()
{
    if (mCustomDrawer != NULL)
        mCustomDrawer->ClearBar();
}

bool
Panogram::PlayBarOutsideSelection()
{
    if (mPanogramPlayFftObjs[0] == NULL)
        return false;
    
    BL_FLOAT playPos = mPanogramPlayFftObjs[0]->GetPlayPosition();
    BL_FLOAT normSelection[4];
    mPanogramPlayFftObjs[0]->GetNormSelection(normSelection);
    if ((playPos < normSelection[0]) ||
        (playPos > normSelection[2]))
        return true;
    
    return false;
}

void
Panogram::BarSetSelection(int x)
{
    if (mGraph == NULL)
        return;

    if (mCustomControl == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    // Update the selection, to be able to play from the bar
    // (choose everything after the bar)
    
    BL_FLOAT minNormX = 0.0;
    BL_FLOAT maxNormX = 1.0;
    
    BL_FLOAT normViewWidth = maxNormX - minNormX;
    
    BL_FLOAT endSelection = ((BL_FLOAT)width)/normViewWidth;
    
    int selection[4] = { x, 0, (int)endSelection, height };
    UpdateSelection(selection[0], selection[1],
                    selection[2], selection[3],
                    false);
    
    mCustomControl->SetSelection(selection[0], selection[1],
                                 selection[2], selection[3]);
    
    BL_FLOAT selectionDrawer[4] = { ((BL_FLOAT)x)/width, 0, 1.0, 1.0 };
    mCustomDrawer->SetSelection(selectionDrawer[0], selectionDrawer[1],
                                selectionDrawer[2], selectionDrawer[3]);
}

void
Panogram::SetSelectionActive(bool flag)
{
    if (mCustomDrawer != NULL)
        mCustomDrawer->SetSelectionActive(flag);
    
    if (mCustomControl != NULL)
        mCustomControl->SetSelectionActive(flag);
    
#if !FIX_BAR_PLAY_BAR_SPEED
    if (!flag)
    {
        for (int i = 0; i < 2; i++)
        {
            if (mPanogramPlayFftObjs[i] != NULL)
                mPanogramPlayFftObjs[i]->SetNormSelection(0.0, 0.0, 1.0, 1.0);
        }
    }
#endif
    
#if 1 // FIX: play from the right side if no selection
      // (and not from the middle of the graph 
    for (int i = 0; i < 2; i++)
    {
        if (mPanogramPlayFftObjs[i] != NULL)
            mPanogramPlayFftObjs[i]->SetSelectionEnabled(flag);
    }
#endif
}

void
Panogram::StartPlay()
{
    mPlay = true;
}

void
Panogram::StopPlay()
{
    mPlay = false;
}

bool
Panogram::PlayStarted()
{
    return mPlay;
}
 
void
Panogram::SelectionChanged() {}

bool
Panogram::IsSelectionActive()
{
    if (mCustomDrawer == NULL)
        return false;
    
    bool res = mCustomDrawer->IsSelectionActive();
    
    return res;
}

void
Panogram::UpdateAxesViewOrientation()
{
    if (mHAxis != NULL) // Time axis
    {
        mHAxis->SetViewOrientation((GraphAxis2::ViewOrientation)mViewOrientation);

        mHAxis->SetForceLabelHAlign(-1);
        
        if (mViewOrientation == SpectrogramDisplayScroll4::VERTICAL)
            mHAxis->SetForceLabelHAlign(NVG_ALIGN_RIGHT);
    }
    
    if (mVAxis != NULL) // "L"/"R"
        mVAxis->SetViewOrientation((GraphAxis2::ViewOrientation)mViewOrientation);
}
