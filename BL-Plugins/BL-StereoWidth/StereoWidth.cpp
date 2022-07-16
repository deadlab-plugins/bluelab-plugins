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

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphControl12.h>

#include <StereoWidthGraphDrawer2.h>
#include <BLUpmixGraphDrawer.h>

#include <BLVectorscope.h>

// See: http://www.rs-met.com/documents/tutorials/StereoProcessing.pdf
#include <PseudoStereoObj2.h>
#include <StereoWidenProcess.h>
#include <BLCorrelationComputer2.h>
#include <CrossoverSplitterNBands4.h>

#include <ParamSmoother2.h>

// Width limit
#include <BLWidthAdjuster.h>
#include <BLStereoWidener.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "StereoWidth.h"

#include "IPlug_include_in_plug_src.h"

//
#define MIN_BASS_FOCUS_FREQ 20.0
#define DEFAULT_BASS_FOCUS_FREQ 100.0
// Henrik/Seidy for Gearlutz suggests 5KHz
// He says Brainworks goes to 22KHz
//
// Set to 6Kz, which is the limit between Presence and Brillance 
// See: https://www.teachmeaudio.com/mixing/techniques/audio-spectrum
#define MAX_BASS_FOCUS_FREQ 6000.0

#define DEFAULT_BASS_FOCUS_SMOOTH_TIME_MS 280.0 //140.0

// Do we force mono for frequencies under bass focus?
//
// We shoudn't, so the user could increase the width of high frequencies only,
// without touching the med and low frequecies (as succested by a user by mail)
//
// If we would turn force mono, this will ensure very low freqs are really mono,
// but with the risk to turn to mono medium frequeciens, which is not preferable.
// (The plugin is better to be a "stereo widener", than a "stereo narrower")
#define BASS_FOCUS_FORCE_MONO 0 // 1

//#define CORRELATION_SMOOTH_TIME_MS 100.0
#define CORRELATION_SMOOTH_TIME_MS 500.0

// FIX: StereoWidth2, Reaper, Mac. Choose a mode using
// point rendering with quads (e.g POLAR_SAMPLE)
// Save and quit. When we restart, this given mode doesn't
// display points (only the circle graph drawer)
// This is because the mWhitePixImg image seems not correct
// If we re-create it at the beginning of each loop, we don't have the problem.
#define FIX_GRAPH_RECREATE_WHITE_IMAGE_HACK 1

// Width boost
#define WIDTH_BOOST_FACTOR 5.0

#define MANAGE_CONTROLS_OVER_GRAPH 1

// When bass to mono option is not checked, pan evrything instead of only
// high frequencies
#define POST_PAN_NO_BASS_TO_MONO 1

// Don't use IParam for meters ?
#define METERS_NO_PARAM 1

#if 0
TODO: check if nvRenderTriangles (from the chinese update), is more powerful than glQuads.

TODO: remove the last old view, and add a panogram view (interactive?, with a circle)
TODO: add width limit and depth knob (disable depth when stereoize is activated?)

TODO: "mono -> st": call it "stereoize" (or pseudostereo)
TODO: optimize with GraphControl10::SetCurveOptimSameColor(true) (like in UST) (OPTIM_QUADS_SAME_COLOR, nvgQuadS)
=> could gain 15% CPU

TODO: recompile to benefit from FIX_FILTER_UNDERFLOW
(for the moment, toes the perfs go to 100% when no sound ?)

TODO: check if bass focus delays the low frequencies only ("sheared spectrogram") like in UST, and add a delay in this case, to have straight spectrograms.

Henrik mail: crash cubase at startup
"So first the problem. I'm having an issue with stereowidth crashing Cubase 10 pro while trying to open projects that use the plugin. When I remove the file from my plugins folder the projects load fine. I'm not sure if it could be that the trial period has ran out (how long are the trials?) or I just dismissed the issue before. Running on Macos Mojave."
    
DONE:
- Tested Cubase 10.0.50 + Mac OSX Sierra, (plugin scan, use) => no problem
- Passed Vangrind on Mavericks (plug scan, use) => no problem
- Launched Cubase under debuger, + Memory guard on Sierra, (plugin scan, use) => no problem
IDEA Niko: maybe this is a plugin before or after, that quits badly just after scanning, and makes StereoWidth crash
(this was often the case on Protools, the solution was to remove the plugin scanned just before, or just after, scan all, then re-add the removed plugin)

Gearslutz private message Henrik (pseudo: Seidy)
Very interesting remarks
- bass focus not limited to 500Hz, but at the minimum to 5KHz
=> so we can make very subtle stereo changes in "air" of the sound only
(brainworks bx_control2 goes until 22KHz)
- stereo width beyond 100% ("super width"). some other plugins do that.
- GUI improvements! he made a mockup to have a more organized GUI.
- "Maybe it would be good idea to make correlation bar a bit more obvious(bigger?) "
"or even implement red-yellow-green lights to show very clearly to be cautious."

Rasmus Wirje wants presets for all the plugins !
(do it after having implemented presets for UST)

TODO: bass focus: make mono before cut freq (for the moment, this is "untouched") (as done in UST)

TODO: - and make a good video
TODO: in the product page, make screenshots of every verctorscope modes

NOTE: mono to stereo (fast convolver): the waveform vary if we change the buffer size

PROBLEM: StudioOne Sierra crackles
=> set buffer size to 1024 to fix

NOTE: Cubase 10, Sierra: in case of resources overrun, Cubase sends zeros to the plugin (no sound)

PROBLEM: Ableton, Sierra, AU => GFX perfs are limit
=> this makes crackles when the GUI is visible

NOTE on performances: when using stereo->mono, the oversampling is 16
We go from 37% cpu to 48% cpu
This is mainly due to fft and fft to magn phases
(no need to try to decimate more points !)

NOTE: threshold weights at 2 (dB) => optim x10 for volument render (TODO: try it for simple rendering too)
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipWidthMeter = "Width Meter";
static char *tooltipStereoWidth =
    "Stereo Width - Increase or decrease the stereo effect";
static char *tooltipMonoToStereo =
    "Mono To Stereo - Generate stereo from mono input";
static char *tooltipPan = "Pan - Pan L/R";
static char *tooltipCorrelationMeter = "Correlation Meter";
static char *tooltipBassFocus = "Bass Focus - Bass focus limit frequency";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipMonoOut = "Mono Out - Force output to mono";
static char *tooltipVectorscopeMode0 = "Display Mode: Polar samples";
static char *tooltipVectorscopeMode1 = "Display Mode: Lissajous";
static char *tooltipVectorscopeMode2 = "Display Mode: Flame";
static char *tooltipVectorscopeMode3 = "Display Mode: Grid";
static char *tooltipWidthLimit = "Width Limit - Enable width limiter";
static char *tooltipWidthLimitSmoothFactor = "Limit Speed - Width limiter speed";
static char *tooltipWidthBoost = "Stereo Boost - Enable stereo width boost";
static char *tooltipBassToMono = "Bass To Mono - Force low frequencies to mono";

enum EParams
{
    kMonoToStereo = 0,

    // Width boost
    kWidthBoost,

    // Bass to mono
    kBassToMono,
    
    //kDebugGraph,

    // Bass focus
    kBassFocus,

    // Set width meter before width for good update at startup
    // So when loading parameters, the real kStereoWidth will overwrite kWidthMeter
    //kWidthMeter = 0,
    kStereoWidth,
    
    kPan,

    // Width limit
    kWidthLimit,
    kWidthLimitSmoothFactor,
    
#if !METERS_NO_PARAM
    kWidthMeter,
    
    // Correlation
    kCorrelationMeter,
#endif

    // Mono out
    kMonoOut,

    // Out gain
    kOutGain,

    // Vectorscope modes
    kVectorscopeMode0,
    kVectorscopeMode1,
    kVectorscopeMode2,
    kVectorscopeMode3,

#if SHOW_LEGACY_VECTORSCOPE_MODE
    kVectorscopeMode4,
#endif
    
#if 0
    kVectorscopeGraph0,
    kVectorscopeGraph1,
    kVectorscopeGraph2,
    kVectorscopeGraph3,
    kVectorscopeGraph4,
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
    
    kVectorscopeGraphX = 0,
    kVectorscopeGraphY = 0,
  
    kStereoWidthX = 150,
    kStereoWidthY = 387,
  
    kMonoToStereoX = 36,
    kMonoToStereoY = 288,
  
    kPanX = 273,
    kPanY = 418,
  
    // Vectorscope modes
#if SHOW_LEGACY_VECTORSCOPE_MODE
    kVectorscopeMode0X = 350,
    kVectorscopeMode0Y = 16,
  
    kVectorscopeMode1X = 372,
    kVectorscopeMode1Y = 16,
  
    kVectorscopeMode2X = 394,
    kVectorscopeMode2Y = 16,
  
    kVectorscopeMode3X = 416,
    kVectorscopeMode3Y = 16,
#else
    kVectorscopeMode0X = 350,
    kVectorscopeMode0Y = 16,
  
    kVectorscopeMode1X = 372,
    kVectorscopeMode1Y = 16,
  
    kVectorscopeMode2X = 394,
    kVectorscopeMode2Y = 16,
  
    kVectorscopeMode3X = 416,
    kVectorscopeMode3Y = 16,
#endif
    
    // Bass focus
    kBassFocusX = 60,
    kBassFocusY = 418,
  
    // Mono out
    kMonoOutX = 346,
    kMonoOutY = 387,
  
    // Out gain
    kOutGainX = 359,
    kOutGainY = 418,

    // Correlation meter
    kCorrelationMeterX = 19,
    kCorrelationMeterY = 248,
    kCorrelationMeterFrames = 65,
    
    // Width meter
    //
    kWidthMeterX = 17,
    kWidthMeterY = 262,
    kWidthMeterFrames = 65,
    
    // Width limit
    kWidthLimitCheckboxX = 237,
    kWidthLimitCheckboxY = 288,

    kWidthLimitSmoothFactorX = 273,
    kWidthLimitSmoothFactorY = 319,

    // Width boost
    kWidthBoostCheckboxX = 36,
    kWidthBoostCheckboxY = 319,

    // Bass to mono
    kBassToMonoCheckboxX = 36,
    kBassToMonoCheckboxY = 350
};


//
StereoWidth::StereoWidth(const InstanceInfo &info)
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

StereoWidth::~StereoWidth()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mPseudoStereoObj != NULL)
        delete mPseudoStereoObj;

    if (mStereoWidthSmoother != NULL)
        delete mStereoWidthSmoother;

    if (mPanSmoother != NULL)
        delete mPanSmoother;

    if (mCorrelationComputer != NULL)
        delete mCorrelationComputer;

    for (int i = 0; i < 2; i++)
    {   
        if (mBassFocusBandSplitters[i] != NULL)
            delete mBassFocusBandSplitters[i];
    }

#if USE_BASS_FOCUS_SMOOTHER
    if (mBassFocusSmoother != NULL)
        delete mBassFocusSmoother;
#endif
    
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;

    // Width limit
    if (mWidthAdjuster != NULL)
        delete mWidthAdjuster;

    if (mStereoWidener != NULL)
        delete mStereoWidener;
}

IGraphics *
StereoWidth::MyMakeGraphics()
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
StereoWidth::MyMakeLayout(IGraphics *pGraphics)
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
StereoWidth::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    mIsInitialized = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mGUIHelper = NULL;

    mStereoWidthSmoother = NULL;
    mPanSmoother = NULL;
    mOutGainSmoother = NULL;

    for (int i = 0; i < 2; i++)
        mBassFocusBandSplitters[i] = NULL;

    mPseudoStereoObj = NULL;
    mWidthVumeter = NULL;
    mCorrelationVumeter = NULL;
    mCorrelationComputer = NULL;
    mVectorscope = NULL;

    mVectorscopeMode = BLVectorscope::POLAR_SAMPLE;
    
    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
        mVectorscopeGraphs[i] = NULL;

    mUpmixDrawer = NULL;
    
    mPanControl = NULL;
    mWidthControl = NULL;

    // Width limit
    mWidthLimitEnabled = false;
    mWidthLimitSmoothFactor = 0.0;
    mWidthLimitControl = NULL;
    mWidthLimitSmoothControl = NULL;
    mWidthAdjuster = NULL;
    mStereoWidener = NULL;

    // Width boost
    mWidthBoost = false;
    mWidthBoostFactor = 1.0;

    mVectorscopeModeChanged = true;

#if USE_BASS_FOCUS_SMOOTHER
    mBassFocusSmoother = NULL;
#endif

    mBassToMono = false;
}

void
StereoWidth::InitParams()
{
    BL_FLOAT defaultWidth = 0.0;
    mWidth = defaultWidth;
    GetParam(kStereoWidth)->InitDouble("Width", defaultWidth,
                                       -100.0, 100.0, 0.01, "%");

    if (mWidthAdjuster != NULL)
        mWidthAdjuster->SetWidth(mWidth*mWidthBoostFactor);
    
    int defaultMonoToStereo = 0;
    mMonoToStereo = defaultMonoToStereo;
    //GetParam(kMonoToStereo)->InitInt("MonoToStereo", defaultMonoToStereo, 0, 1);
    GetParam(kMonoToStereo)->InitEnum("MonoToStereo", defaultMonoToStereo, 2,
                                      "", IParam::kFlagsNone, "",
                                      "Off", "On");

    BL_FLOAT defaultPan = 0.0;
    mPan = defaultPan;
    GetParam(kPan)->InitDouble("Pan", 0.0, -100.0, 100.0, 0.01, "%");

#if !METERS_NO_PARAM
    GetParam(kCorrelationMeter)->
        InitDouble("CorrelationMeter", 0.0, -1.0, 1.0, 0.01,
                   "", IParam::kFlagMeta | IParam::kFlagCannotAutomate);

#endif

#if !METERS_NO_PARAM
    BL_FLOAT defaultWidthMeterValue = 0.5;
    GetParam(kWidthMeter)->
        InitDouble("WidthMeter",
                   defaultWidthMeterValue, 0.0, 1.0, 0.01,
                   "",
                   IParam::kFlagMeta | IParam::kFlagCannotAutomate);
#endif
    
    BL_FLOAT defaultBassFocusFreq = DEFAULT_BASS_FOCUS_FREQ;
    mBassFocusFreq = defaultBassFocusFreq;
    GetParam(kBassFocus)->InitDouble("BassFocus",
                                     defaultBassFocusFreq, 0.0, MAX_BASS_FOCUS_FREQ,
                                     1.0, "Hz", IParam::kFlagMeta,
                                     "", IParam::ShapePowCurve(4.0)); // for 6KHz

    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1,
                                   "dB", IParam::kFlagMeta);
    
    bool defaultMonoOut = false;
    mMonoOut = defaultMonoOut;
    //GetParam(kMonoOut)->InitInt("MonoOut", defaultMonoOut, 0, 1, "", IParam::kFlagMeta);
    GetParam(kMonoOut)->InitEnum("MonoOut", defaultMonoOut, 2,
                                 "", IParam::kFlagMeta, "",
                                 "Off", "On");

    CreateVectorscopeButtonsParams();

    // Width limit
    //GetParam(kWidthLimit)->InitInt("WidthLimit", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kWidthLimit)->InitEnum("WidthLimit", 0, 2,
                                    "", IParam::kFlagMeta, "",
                                    "Off", "On");
 
    GetParam(kWidthLimitSmoothFactor)->
        InitDouble("WidthLimitSpeed", 50.0, 0.0, 100.0,
                   0.01, "%", IParam::kFlagMeta);
    
    int defaultWidthBoost = 0;
    mWidthBoost = defaultWidthBoost;
    mWidthBoostFactor = 1.0;
    //GetParam(kWidthBoost)->InitInt("WidthBoost", defaultWidthBoost, 0, 1);
    GetParam(kWidthBoost)->InitEnum("WidthBoost", defaultWidthBoost, 2,
                                    "", IParam::kFlagsNone, "",
                                    "Off", "On");

    bool defaultBassToMono = false;
    mBassToMono = defaultBassToMono;
    //GetParam(kBassToMono)->InitInt("BassToMono", defaultBassToMono, 0, 1);
    GetParam(kBassToMono)->InitEnum("BassToMono", defaultBassToMono, 2,
                                    "", IParam::kFlagsNone, "",
                                    "Off", "On");
}

void
StereoWidth::ApplyParams()
{
    // Width
    if (mStereoWidthSmoother != NULL)
        mStereoWidthSmoother->ResetToTargetValue(mWidth*mWidthBoostFactor);
        
    UpdateWidthMeter(mWidth*mWidthBoostFactor);
        
    if (mUpmixDrawer != NULL)
    {
        // Gain was removed, replaced by width for homogeneity
            
        // Artificially normalize
        BL_FLOAT widthNorm = (mWidth*mWidthBoostFactor + 1.0)*0.5;
            
        mUpmixDrawer->SetGain(widthNorm);
    }
    
    // Pan
    if (mPanSmoother != NULL)
        mPanSmoother->ResetToTargetValue(mPan);
        
    if (mUpmixDrawer != NULL)
        mUpmixDrawer->SetPan(mPan);
    
    // Mode
    if (mVectorscope != NULL)
        mVectorscope->SetMode(mVectorscopeMode);
    
    UpdateVectorscopeMode(mVectorscopeMode);
    
    // Bass focus
    //SetBassFocusFreq(mBassFocusFreq);

#if USE_BASS_FOCUS_SMOOTHER
    if (mBassFocusSmoother != NULL)
        mBassFocusSmoother->ResetToTargetValue(mBassFocusFreq);
#endif
    
    // Out gain
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    // Width limit
    UpdateWidthMeterLimit();

    UpdateWidthBoostFactor();
}

void
StereoWidth::Init()
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    mPseudoStereoObj = new PseudoStereoObj2(sampleRate);
  
#if !SET_LATENCY_IN_GUI_THREAD
    mLatency = 0;
    mPrevLatency = 0;
#endif
    
    BL_FLOAT bassFreqs[1] = { DEFAULT_BASS_FOCUS_FREQ };
    for (int i = 0; i < 2; i++)
        mBassFocusBandSplitters[i] =
            new CrossoverSplitterNBands4(2, bassFreqs, sampleRate);

    int blockSize = GetBlockSize();
    BL_FLOAT smoothTime = DEFAULT_SMOOTHING_TIME_MS;
        
    BL_FLOAT defaultStereoWidth = 0.0;
    mStereoWidthSmoother = new ParamSmoother2(sampleRate,
                                              defaultStereoWidth,
                                              smoothTime);
  
    BL_FLOAT defaultPan = 0.0;
    mPanSmoother = new ParamSmoother2(sampleRate, defaultPan);
    
    BL_FLOAT defaultGain = 1.0; // 0 dB
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultGain);
  
    mCorrelationComputer =
        new BLCorrelationComputer2(sampleRate, CORRELATION_SMOOTH_TIME_MS);

    mWidthAdjuster = new BLWidthAdjuster(sampleRate);

    mStereoWidener = new BLStereoWidener(sampleRate);
    
#if USE_BASS_FOCUS_SMOOTHER
    BL_FLOAT defaultBassFocusFreq = DEFAULT_BASS_FOCUS_FREQ;
    BL_FLOAT bassFocusSmoothTime = DEFAULT_BASS_FOCUS_SMOOTH_TIME_MS;

    // Adjust, because smoothing is done only once in each ProcessBlock()
    bassFocusSmoothTime /= blockSize;
    
    mBassFocusSmoother =
        new ParamSmoother2(sampleRate, defaultBassFocusFreq,
                           bassFocusSmoothTime);
#endif
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
StereoWidth::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    if (mVectorscopeGraphs[0] != NULL)
        mVectorscopeGraphs[0]->Lock();

    UpdateWidthMeterLimit();
    
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
        if (mVectorscopeGraphs[0] != NULL)
            mVectorscopeGraphs[0]->Unlock();

        for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
        {
            if (mVectorscopeGraphs[i] != NULL)
                mVectorscopeGraphs[i]->PushAllData();
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

    // NOTE: the original StereoWidth plugin in IPlug1 used IsAllZero()

    // If we have only one channel, duplicate it
    // This is simpler like that, for later stereoize
    if (in.size() == 1)
    {
        in.resize(2);
        in[1] = in[0];
    }
    
    // Bass focus
#if USE_BASS_FOCUS_SMOOTHER
    if (mBassFocusSmoother != NULL)
    {
        if (!mBassFocusSmoother->IsStable())
        {
            BL_FLOAT bassFocusFreq = mBassFocusSmoother->Process();

            mBassFocusFreq = bassFocusFreq;
            
            SetBassFocusFreq(bassFocusFreq);
        }
    }
#endif

    if ((mBassFocusFreq > MIN_BASS_FOCUS_FREQ) &&
        (mBassFocusBandSplitters[0] != NULL))
    {
        vector<WDL_TypedBuf<BL_FLOAT> > &resultLo = mTmpBuf3;
        vector<WDL_TypedBuf<BL_FLOAT> > &resultHi = mTmpBuf4;
        resultLo.resize(in.size());
        resultHi.resize(in.size());
        for (int i = 0; i < in.size(); i++)
        {
            WDL_TypedBuf<BL_FLOAT> *resultBuf = mTmpBuf5;
            mBassFocusBandSplitters[i]->Split(in[i], resultBuf);
      
            resultLo[i] = resultBuf[0];
            resultHi[i] = resultBuf[1];
        }

        //#if BASS_FOCUS_FORCE_MONO
        if (mBassToMono)
        {
            if (resultLo.size() > 1)
            {
                BLUtils::StereoToMono(&resultLo);
            }
        }
        //#endif

#if POST_PAN_NO_BASS_TO_MONO
        bool doPanHighFreqOnly = true;
        if (!mBassToMono)
            // Pan only high freqs
            doPanHighFreqOnly = false;
#endif
        
        //StereoWidenSimple(&resultHi);
        StereoWiden(&resultHi, doPanHighFreqOnly);

        // Compute correlation on stereo widened signal only
        // (to get consistency between correlation meter and limited width meter)
        if (IsTransportPlaying())
            ComputeCorrelation(resultHi);
        
        vector<WDL_TypedBuf<BL_FLOAT> > &result = mTmpBuf6;
        result.resize(in.size());
        for (int i = 0; i < in.size(); i++)
        {
            WDL_TypedBuf<BL_FLOAT> &resultSum = mTmpBuf7;
            BLUtils::AddValues(&resultSum, resultLo[i], resultHi[i]);
      
            result[i] = resultSum;
        }

        if (!doPanHighFreqOnly)
            // Pan everything
        {
            if (result.size() == 2)
            {
                vector<WDL_TypedBuf<BL_FLOAT> *> samplesToPan;
                samplesToPan.resize(2);
                samplesToPan[0] = &result[0];
                samplesToPan[1] = &result[1];
                
                if (mPanSmoother != NULL)
                    StereoWidenProcess::Balance(&samplesToPan, mPanSmoother);
            }
        }
        
        out = result;
    }
    else
        // No bass focus
    {
        //StereoWidenSimple(&in);
        StereoWiden(&in, true);
      
        out = in;
    }
    
    ApplyOutGain(&out);
    
    if (mMonoOut)
    {
        StereoToMono(&out);
    }
    
    if (mVectorscope != NULL)
        mVectorscope->AddSamples(out);

    // If bass focus is disabled, must compute correlation on the whole signal
    if (mBassFocusFreq <= MIN_BASS_FOCUS_FREQ)
    {
        if (IsTransportPlaying())
            ComputeCorrelation(out);
    }
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
    
    if (mVectorscopeGraphs[0] != NULL)
        mVectorscopeGraphs[0]->Unlock();

    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
    {
        if (mVectorscopeGraphs[i] != NULL)
            mVectorscopeGraphs[i]->PushAllData();
    }
    
    BL_PROFILE_END;
}

void
StereoWidth::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    // Separator
    /*IColor sepIColor;
      mGUIHelper->GetGraphSeparatorColor(&sepIColor);
      int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
      mGraph->SetSeparatorY0(2.0, sepColor); */
    
    // Width
    mWidthControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                              kStereoWidthX, kStereoWidthY,
                                              kKnobWidth, kKnobHeight,
                                              KNOB_FN,
                                              kStereoWidth,
                                              TEXTFIELD_FN,
                                              "WIDTH",
                                              GUIHelper12::SIZE_DEFAULT,
                                              NULL, true,
                                              tooltipStereoWidth);
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kMonoToStereoX,
                                   kMonoToStereoY,
                                   CHECKBOX_FN, kMonoToStereo, "MONO->ST",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipMonoToStereo);

    // Pan
    mPanControl = mGUIHelper->CreateKnobSVG(pGraphics,
                                            kPanX, kPanY,
                                            kKnobSmallWidth, kKnobSmallHeight,
                                            KNOB_SMALL_FN,
                                            kPan,
                                            TEXTFIELD_FN,
                                            "PAN",
                                            GUIHelper12::SIZE_DEFAULT,
                                            NULL, true,
                                            tooltipPan);

    CreateVectorscope(pGraphics);
        
    mCorrelationVumeter = mGUIHelper->CreateVumeter(pGraphics,
                                                    kCorrelationMeterX,
                                                    kCorrelationMeterY,
                                                    CORRELATION_METER_FN,
                                                    kCorrelationMeterFrames,
#if !METERS_NO_PARAM
                                                    kCorrelationMeter,
#else
                                                    kNoParameter,
#endif
                                                    NULL,
                                                    tooltipCorrelationMeter);

    mWidthVumeter = mGUIHelper->CreateVumeter(pGraphics,
                                              kWidthMeterX,
                                              kWidthMeterY,
                                              WIDTH_METER_FN,
                                              kWidthMeterFrames,
#if !METERS_NO_PARAM
                                              kWidthMeter,
#else
                                              kNoParameter,
#endif
                                              NULL,
                                              tooltipWidthMeter);

    // Bass focus
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kBassFocusX, kBassFocusY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kBassFocus,
                              TEXTFIELD_FN,
                              "BASS FOCUS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipBassFocus);

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

    // Mono out
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kMonoOutX,
                                   kMonoOutY,
                                   CHECKBOX_FN, kMonoOut, "MONO OUT",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipMonoOut);

    // Width limit
    mWidthLimitControl =
        mGUIHelper->CreateToggleButton(pGraphics,
                                       kWidthLimitCheckboxX, kWidthLimitCheckboxY,
                                       CHECKBOX_FN, kWidthLimit,
                                       "WIDTH LIMIT",
                                       GUIHelper12::SIZE_DEFAULT, true,
                                       tooltipWidthLimit);
    
    mWidthLimitSmoothControl =
        mGUIHelper->CreateKnobSVG(pGraphics,
                                  kWidthLimitSmoothFactorX,
                                  kWidthLimitSmoothFactorY,
                                  kKnobSmallWidth, kKnobSmallHeight,
                                  KNOB_SMALL_FN,
                                  kWidthLimitSmoothFactor,
                                  TEXTFIELD_FN,
                                  "LIMIT SPEED",
                                  GUIHelper12::SIZE_DEFAULT,
                                  NULL, true,
                                  tooltipWidthLimitSmoothFactor);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kWidthBoostCheckboxX,
                                   kWidthBoostCheckboxY,
                                   CHECKBOX_FN, kWidthBoost, "WIDTH BOOST",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipWidthBoost);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kBassToMonoCheckboxX,
                                   kBassToMonoCheckboxY,
                                   CHECKBOX_FN,
                                   kBassToMono, "BASS TO MONO",
                                   GUIHelper12::SIZE_SMALL,
                                   true,
                                   tooltipBassToMono);
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //                               kLogoAnimFrames, GUIHelper12::BOTTOM);
    
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
StereoWidth::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
StereoWidth::OnReset()
{
    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset
    
    TRACE;
    ENTER_PARAMS_MUTEX;
    
    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();
    
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    if (mStereoWidthSmoother != NULL)
    {    
        BL_FLOAT smoothTime = DEFAULT_SMOOTHING_TIME_MS;
        
        mStereoWidthSmoother->Reset(sampleRate, smoothTime);
    }
    
    if (mPanSmoother != NULL)
        mPanSmoother->Reset(sampleRate);
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    
    //
    if (mPseudoStereoObj != NULL)
        mPseudoStereoObj->Reset(sampleRate, blockSize);

    if (mCorrelationComputer != NULL)
        mCorrelationComputer->Reset(sampleRate);
  
    for (int i = 0; i < 2; i++)
    {
        if (mBassFocusBandSplitters[i] != NULL)
            mBassFocusBandSplitters[i]->Reset(sampleRate);
    }

    if (mVectorscope != NULL)
        mVectorscope->Reset(sampleRate);

    if (mWidthAdjuster != NULL)
        mWidthAdjuster->Reset(sampleRate);

    if (mStereoWidener != NULL)
        mStereoWidener->Reset(sampleRate);
    
    UpdateLatency();

#if USE_BASS_FOCUS_SMOOTHER
    if (mBassFocusSmoother != NULL)
    {
        BL_FLOAT bassFocusSmoothTime = DEFAULT_BASS_FOCUS_SMOOTH_TIME_MS;
        
        // Adjust, because smoothing is done only once in each ProcessBlock()
        int blockSize = GetBlockSize();
        bassFocusSmoothTime /= blockSize;
    
        mBassFocusSmoother->Reset(sampleRate, bassFocusSmoothTime);
    }
#endif
    
    //
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

#define CHECK_VECTORSCOPE_PARAM(__MODE_NUM__)                             \
    case kVectorscopeMode##__MODE_NUM__:                                  \
    {                                                                     \
        int value = GetParam(paramIdx)->Int();                            \
        if (value == 1)                                                   \
        {                                                                 \
            BLVectorscope::Mode mode = (BLVectorscope::Mode)__MODE_NUM__; \
            mVectorscopeMode = mode;                                      \
            mVectorscopeModeChanged = true;                               \
        }                                                                 \
    }                                                                     \
    break;

void
StereoWidth::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kStereoWidth:
        {
            BL_FLOAT value = GetParam(kStereoWidth)->Value();
            BL_FLOAT width = value/100.0;
            mWidth = width;

            UpdateWidthBoostFactor();
            UpdateWidth();
        }
        break;
    
        case kPan:
        {
            BL_FLOAT value = GetParam(kPan/*paramIdx*/)->Value();
      
            BL_FLOAT pan = value/100.0;
            mPan = pan;

            if (mPanSmoother != NULL)
                mPanSmoother->SetTargetValue(pan);
      
            if (mUpmixDrawer != NULL)
                mUpmixDrawer->SetPan(pan);
        }
        break;
      
        case kMonoToStereo:
        {
            int value = GetParam(kMonoToStereo)->Value();
            mMonoToStereo = (value == 1);
        }
        break;
           
        CHECK_VECTORSCOPE_PARAM(0);
        CHECK_VECTORSCOPE_PARAM(1);
        CHECK_VECTORSCOPE_PARAM(2);
        CHECK_VECTORSCOPE_PARAM(3);

#if SHOW_LEGACY_VECTORSCOPE_MODE
        CHECK_VECTORSCOPE_PARAM(4);
#endif
        
        case kBassFocus:
        {
            BL_FLOAT bassFocusFreq = GetParam(paramIdx)->Value();
            mBassFocusFreq = bassFocusFreq;
            
#if USE_BASS_FOCUS_SMOOTHER
            if (mBassFocusSmoother != NULL)
                mBassFocusSmoother->SetTargetValue(mBassFocusFreq);
#else
            SetBassFocusFreq(mBassFocusFreq);
#endif
        }
        break;
      
        case kOutGain:
        {
            BL_FLOAT gain = GetParam(paramIdx)->DBToAmp();
            mOutGain = gain;
            
            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(gain);
        }
        break;
      
        case kMonoOut:
        {
            int value = GetParam(paramIdx)->Int();
            mMonoOut = (value == 1);
            
        }
        break;

        // Width limit
        case kWidthLimit:
        {
            int value = GetParam(paramIdx)->Int();
            mWidthLimitEnabled = (value == 1);

            SetWidthLimitEnabled(mWidthLimitEnabled);
            
            if (mWidthLimitSmoothControl != NULL)
            {
                //mWidthLimitSmoothControl->SetDisabled(!mWidthLimitEnabled);

                // Also disable knob value automatically
                IGraphics *graphics = GetUI();
                if (graphics != NULL)
                    graphics->DisableControl(kWidthLimitSmoothFactor,
                                             mWidthBoost || // NEW
                                             !mWidthLimitEnabled);
            }
        }
        break;                                                       

        case kWidthLimitSmoothFactor:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mWidthLimitSmoothFactor = 1.0 - value/100.0;
            
            SetWidthLimitSmooth(mWidthLimitSmoothFactor);
        }
        break;

        case kWidthBoost:
        {
            int value = GetParam(kWidthBoost)->Value();
            mWidthBoost = (value == 1);
            
            UpdateWidthBoostFactor();

            if (mWidthAdjuster != NULL)
            {
                BL_FLOAT sampleRate = GetSampleRate();
                mWidthAdjuster->Reset(sampleRate);
            }

            // Disable width limit if width boost is enabled
            //
            SetWidthLimitEnabled(!mWidthBoost && mWidthLimitEnabled);

            if (mWidthLimitControl != NULL)
                mWidthLimitControl->SetDisabled(mWidthBoost);
            
            if (mWidthLimitSmoothControl != NULL)
            {
                //mWidthLimitSmoothControl->
                //    SetDisabled(mWidthBoost || !mWidthLimitEnabled);

                // Also disable knob value automatically
                IGraphics *graphics = GetUI();
                if (graphics != NULL)
                    graphics->DisableControl(kWidthLimitSmoothFactor,
                                             mWidthBoost || !mWidthLimitEnabled);
            }
        }
        break;

        case kBassToMono:
        {
            int value = GetParam(kBassToMono)->Value();
            mBassToMono = (value == 1);
        }
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoWidth::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoWidth::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;

    // Make sure to not try to display controls that have already been deleted
    VectorscopeClearControlsOverGraphs();
        
    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
        mVectorscopeGraphs[i] = NULL;

    if (mVectorscope != NULL)
    {
        mVectorscope->SetGraphs(mVectorscopeGraphs[0],
                                mVectorscopeGraphs[1],
                                mVectorscopeGraphs[2],
                                mVectorscopeGraphs[3],
                                mVectorscopeGraphs[4]);
    }
    
    mUpmixDrawer = NULL;

    mWidthVumeter = NULL;
    mCorrelationVumeter = NULL;
    
    mVectorscopeControls.clear();
    
    mPanControl = NULL;
    mWidthControl = NULL;

    mWidthLimitControl = NULL;
    mWidthLimitSmoothControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoWidth::UpdateLatency()
{
    if (mMonoToStereo)
    {
        int latency = 0;
        if (mPseudoStereoObj != NULL)
            latency = mPseudoStereoObj->GetLatency();
    
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
StereoWidth::StereoWidenSimple(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    if (samples->empty())
        return;
  
    if (mMonoToStereo)
    {
        if (mPseudoStereoObj != NULL)
            mPseudoStereoObj->ProcessSamples(samples);
    }
  
    vector<WDL_TypedBuf<BL_FLOAT> *> samples0;
    samples0.resize(2);
    samples0[0] = &(*samples)[0];
    samples0[1] = &(*samples)[1];
  
    if (mStereoWidthSmoother != NULL)
        StereoWidenProcess::StereoWiden(&samples0, mStereoWidthSmoother);

    if (mPanSmoother != NULL)
        StereoWidenProcess::Balance(&samples0, mPanSmoother);
}

void
StereoWidth::StereoWiden(vector<WDL_TypedBuf<BL_FLOAT> > *samples, bool doPan)
{
    if (samples->empty())
        return;
  
    if (mMonoToStereo)
    {
        if (mPseudoStereoObj != NULL)
            mPseudoStereoObj->ProcessSamples(samples);
    }
  
    vector<WDL_TypedBuf<BL_FLOAT> *> samples0;
    samples0.resize(2);
    samples0[0] = &(*samples)[0];
    samples0[1] = &(*samples)[1];

    if (mStereoWidener != NULL)
        mStereoWidener->StereoWiden(&samples0, mWidthAdjuster);

    if (doPan)
    {
        if (mPanSmoother != NULL)
            StereoWidenProcess::Balance(&samples0, mPanSmoother);
    }
}

void
StereoWidth::UpdateVectorscopeMode(BLVectorscope::Mode mode)
{
    if (mVectorscopeGraphs[mode] != NULL)
        mVectorscopeGraphs[mode]->SetDataChanged();

    // Activate one button
    SetParameterValue(kVectorscopeMode0 + mode, 1.0);
    
    if (mode != 0)
    {       
        SetParameterValue(kVectorscopeMode0, 0.0);

        if (mVectorscopeControls.size() > 0)
            mVectorscopeControls[0]->SetValue(0.0);
    }

    if (mode != 1)
    {       
        SetParameterValue(kVectorscopeMode1, 0.0);
        
        if (mVectorscopeControls.size() > 1)
            mVectorscopeControls[1]->SetValue(0.0);
    }

    if (mode != 2)
    {       
        SetParameterValue(kVectorscopeMode2, 0.0);
        
        if (mVectorscopeControls.size() > 2)
            mVectorscopeControls[2]->SetValue(0.0);
    }

    if (mode != 3)
    {       
        SetParameterValue(kVectorscopeMode3, 0.0);
        
        if (mVectorscopeControls.size() > 3)
            mVectorscopeControls[3]->SetValue(0.0);
    }

#if SHOW_LEGACY_VECTORSCOPE_MODE
    if (mode != 4)
    {       
        SetParameterValue(kVectorscopeMode4, 0.0);
        
        if (mVectorscopeControls.size() > 4)
            mVectorscopeControls[4]->SetValue(0.0);
    }
#endif

    // Set ALL buttons dirty (need to redraw them all over the changed graph)
    for (int i = 0; i < mVectorscopeControls.size(); i++)
    {
        IControl *c = mVectorscopeControls[i];
        c->SetDirty(false);
    }
}

void
StereoWidth::CreateVectorscope(IGraphics *pGraphics)
{
    //CreateVectorscopeButtons(pGraphics);
  
    BL_FLOAT sampleRate = GetSampleRate();
    mVectorscope = new BLVectorscope(this, sampleRate);

    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
        mVectorscopeGraphs[i] = NULL;
  
    mUpmixDrawer = NULL;
  
    CreateVectorscopeGraphs(pGraphics);

    // Buttons over graphs!
    CreateVectorscopeButtons(pGraphics);

#if MANAGE_CONTROLS_OVER_GRAPH
    VectorscopeSetControlsOverGraphs();
#endif
}

void
StereoWidth::CreateVectorscopeButtonsParams()
{
    // Vectorscope mode
    GetParam(kVectorscopeMode0)->InitInt("VectorscopeMode0", 1, 0, 1,
                                         "", IParam::kFlagMeta);
    GetParam(kVectorscopeMode1)->InitInt("VectorscopeMode1", 0, 0, 1,
                                         "", IParam::kFlagMeta);
    GetParam(kVectorscopeMode2)->InitInt("VectorscopeMode2", 0, 0, 1,
                                         "", IParam::kFlagMeta);
    GetParam(kVectorscopeMode3)->InitInt("VectorscopeMode3", 0, 0, 1,
                                         "", IParam::kFlagMeta);

#if SHOW_LEGACY_VECTORSCOPE_MODE
    GetParam(kVectorscopeMode4)->InitInt("VectorscopeMode4", 0, 0, 1,
                                         "", IParam::kFlagMeta);
#endif
}

#define CREATE_VECTORSCOPE_BUTTON(__BUTTON_NUM__)                               \
    {                                                                           \
        IControl *c =                                                           \
            mGUIHelper->CreateRolloverButton(pGraphics,                         \
                                       kVectorscopeMode##__BUTTON_NUM__##X,     \
                                       kVectorscopeMode##__BUTTON_NUM__##Y,     \
                                       VECTORSCOPE_MODE_##__BUTTON_NUM__##_TOGGLE_BUTTON_FN, \
                                       kVectorscopeMode##__BUTTON_NUM__,        \
                                       NULL, true, false, true,                 \
                                       tooltipVectorscopeMode##__BUTTON_NUM__); \
        mVectorscopeControls.push_back(c);                                      \
    }

void
StereoWidth::CreateVectorscopeButtons(IGraphics *pGraphics)
{
    CREATE_VECTORSCOPE_BUTTON(0);
    CREATE_VECTORSCOPE_BUTTON(1);
    CREATE_VECTORSCOPE_BUTTON(2);
    CREATE_VECTORSCOPE_BUTTON(3);

#if SHOW_LEGACY_VECTORSCOPE_MODE
    CREATE_VECTORSCOPE_BUTTON(4);
#endif
}

void
StereoWidth::CreateVectorscopeGraphs(IGraphics *pGraphics)
{
    // Graph0
    mVectorscopeGraphs[0] = mGUIHelper->CreateGraph(this, pGraphics,
                                                    kVectorscopeGraphX,
                                                    kVectorscopeGraphY,
                                                    GRAPH_FN);
    //kVectorscopeGraph0);

    // Graph1
    mVectorscopeGraphs[1] = mGUIHelper->CreateGraph(this, pGraphics,
                                                    kVectorscopeGraphX,
                                                    kVectorscopeGraphY,
                                                    GRAPH_FN);
    //kVectorscopeGraph1);

    // Graph2
    mVectorscopeGraphs[2] = mGUIHelper->CreateGraph(this, pGraphics,
                                                    kVectorscopeGraphX,
                                                    kVectorscopeGraphY,
                                                    GRAPH_FN);
    //kVectorscopeGraph2);

    // Graph3
    mVectorscopeGraphs[3] = mGUIHelper->CreateGraph(this, pGraphics,
                                                    kVectorscopeGraphX,
                                                    kVectorscopeGraphY,
                                                    GRAPH_FN);
    //kVectorscopeGraph3);

#if SHOW_LEGACY_VECTORSCOPE_MODE
    // Graph4
    mVectorscopeGraphs[4] = mGUIHelper->CreateGraph(this, pGraphics,
                                                    kVectorscopeGraphX,
                                                    kVectorscopeGraphY,
                                                    GRAPH_FN);
    //kVectorscopeGraph4);
#endif
    
#if FIX_GRAPH_RECREATE_WHITE_IMAGE_HACK
    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
    {
        if (mVectorscopeGraphs[i] != NULL)
            mVectorscopeGraphs[i]->SetRecreateWhiteImageHack(true);
    } 
#endif
  
    mVectorscope->SetGraphs(mVectorscopeGraphs[0],
                            mVectorscopeGraphs[1],
                            mVectorscopeGraphs[2],
                            mVectorscopeGraphs[3],
                            mVectorscopeGraphs[4],
                            mGUIHelper);
  
    mUpmixDrawer = mVectorscope->GetUpmixGraphDrawer();
  
    // Set the circle in the middle in height by default
    mUpmixDrawer->SetDepth(0.5);
}

void
StereoWidth::ComputeCorrelation(const vector<WDL_TypedBuf<BL_FLOAT> > &samples)
{
    if (samples.size() != 2)
        return;

    if (mCorrelationComputer == NULL)
        return;

    if (mCorrelationVumeter == NULL)
        return;
    
    WDL_TypedBuf<BL_FLOAT> *samples2 = mTmpBuf8;
    samples2[0] = samples[0];
    samples2[1] = samples[1];
  
    mCorrelationComputer->Process(samples2);
  
    BL_FLOAT corr = mCorrelationComputer->GetCorrelation();
  
    BL_FLOAT normCorr = (corr + 1.0)*0.5;
    
    mCorrelationVumeter->SetValue(normCorr);
    mCorrelationVumeter->SetDirty(false);
}

void
StereoWidth::SetBassFocusFreq(BL_FLOAT freq)
{
    //mBassFocusFreq = freq;
  
    if (freq > MIN_BASS_FOCUS_FREQ)
    {
        for (int i = 0; i < 2; i++)
        {
            if (mBassFocusBandSplitters[i] != NULL)
                mBassFocusBandSplitters[i]->SetCutoffFreq(0, freq);
        }
    }
}

void
StereoWidth::ApplyOutGain(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    if (samples->empty())
        return;

    if (mOutGainSmoother == NULL)
        return;
    
    // Signal processing
    for (int i = 0; i < (*samples)[0].GetSize(); i++)
    {
        // Parameters update
        BL_FLOAT gain = mOutGainSmoother->Process();
    
        BL_FLOAT leftSample = (*samples)[0].Get()[i];
        (*samples)[0].Get()[i] = gain*leftSample;
    
        if ((samples->size() > 1))
        {
            BL_FLOAT rightSample = (*samples)[1].Get()[i];
            (*samples)[1].Get()[i] = gain*rightSample;
        }
    }
}

void
StereoWidth::StereoToMono(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    if (samples->size() <= 1)
        return;
  
    WDL_TypedBuf<BL_FLOAT> &mono = mTmpBuf9;
    BLUtils::StereoToMono(&mono, (*samples)[0], (*samples)[1]);
  
    (*samples)[0] = mono;
    (*samples)[1] = mono;
}

void
StereoWidth::UpdateWidthMeter(BL_FLOAT width)
{    
    BL_FLOAT normWidth = (width + 1.0)*0.5;
    if (mWidthVumeter != NULL)
    {
        mWidthVumeter->SetValue(normWidth);
        mWidthVumeter->SetDirty(false);
    }
}

void
StereoWidth::VectorscopeUpdatePanCB(BL_FLOAT newPan)
{
    // Pan
    newPan = newPan/2.0 + 0.5;
    
    SetParameterValue(kPan, newPan);
    
    if (mPanControl != NULL)
    {
        mPanControl->SetValue(newPan);
        mPanControl->SetDirty(false);
    }
}

void
StereoWidth::VectorscopeUpdateDPanCB(BL_FLOAT dpan)
{
    // Pan
    BL_FLOAT normPan = GetParam(kPan)->GetNormalized();
    normPan += dpan;
    if (normPan < 0.0)
        normPan = 0.0;
    if (normPan > 1.0)
        normPan = 1.0;
    
    SetParameterValue(kPan, normPan);
    
    if (mPanControl != NULL)
    {
        mPanControl->SetValue(normPan);
        mPanControl->SetDirty(false);
    }
}

void
StereoWidth::VectorscopeUpdateWidthCB(BL_FLOAT width)
{
    // Width
    BL_FLOAT normWidth = GetParam(kStereoWidth)->ToNormalized(width);
        
    SetParameterValue(kStereoWidth, normWidth);
    
    if (mWidthControl != NULL)
    {
        mWidthControl->SetValue(normWidth);
        mWidthControl->SetDirty(false);
    }
}

void
StereoWidth::VectorscopeUpdateDWidthCB(BL_FLOAT dwidth)
{
    // Width
    BL_FLOAT normWidth = GetParam(kStereoWidth)->GetNormalized();
    normWidth += dwidth;
    if (normWidth < 0.0)
        normWidth = 0.0;
    if (normWidth > 1.0)
        normWidth = 1.0;
    
    SetParameterValue(kStereoWidth, normWidth);
    //GetParam(kStereoWidth)->Set(normWidth);
    
    if (mWidthControl != NULL)
    {
        mWidthControl->SetValue(normWidth);
        mWidthControl->SetDirty(false);
    }
}

void
StereoWidth::VectorscopeUpdateDepthCB(BL_FLOAT newDepth)
{
    // Not used
}

void
StereoWidth::VectorscopeUpdateDDepthCB(BL_FLOAT ddepth)
{
    // Not used
}

void
StereoWidth::VectorscopeResetAllParamsCB()
{
    BL_FLOAT defaultPan = GetParam(kPan)->GetDefault();
    VectorscopeUpdatePanCB(defaultPan);

    BL_FLOAT defaultStereoWidth = GetParam(kStereoWidth)->GetDefault();
    VectorscopeUpdateWidthCB(defaultStereoWidth);
}

BL_FLOAT
StereoWidth::GetLimitedWidth()
{
    if (!mWidthAdjuster->IsEnabled())
        return mWidth*mWidthBoostFactor;
    
    return mWidthAdjuster->GetLimitedWidth();
}

bool
StereoWidth::IsWidthLimitEnabled()
{
    return mWidthLimitEnabled;
}

void
StereoWidth::SetWidthLimitEnabled(bool flag)
{
    mWidthAdjuster->SetEnabled(flag);
}

BL_FLOAT
StereoWidth::GetWidthLimitSmooth()
{
    return mWidthLimitSmoothFactor;
}

void
StereoWidth::SetWidthLimitSmooth(BL_FLOAT smoothFactor)
{
    mWidthLimitSmoothFactor = smoothFactor;
    
    mWidthAdjuster->SetSmoothFactor(smoothFactor);
}

void
StereoWidth::UpdateWidthMeterLimit()
{
    BL_FLOAT width = GetLimitedWidth();

    UpdateWidthMeter(width);
}

void
StereoWidth::UpdateWidthBoostFactor()
{
    if (!mWidthBoost)
        mWidthBoostFactor = 1.0;
    else
    {
        if (mWidth < 0.0)
            mWidthBoostFactor = 1.0;
        else
            mWidthBoostFactor = WIDTH_BOOST_FACTOR;
    }

    UpdateWidth();
}

void
StereoWidth::UpdateWidth()
{
    BL_FLOAT width = mWidth*mWidthBoostFactor;
    
    if (mStereoWidthSmoother != NULL)
        mStereoWidthSmoother->SetTargetValue(width);
    
    UpdateWidthMeter(width);
    
    if (mUpmixDrawer != NULL)
    {
        // Gain was removed, replaced by width for homogeneity
        
        // Artificially normalize
        BL_FLOAT widthNorm = (width + 1.0)*0.5;
        
        mUpmixDrawer->SetGain(widthNorm);
    }
    
    if (mWidthAdjuster != NULL)
        mWidthAdjuster->SetWidth(mWidth*mWidthBoostFactor);
    
    UpdateWidthMeterLimit();
}

void
StereoWidth::OnIdle()
{   
    ENTER_PARAMS_MUTEX;

    if (mVectorscopeModeChanged)
    {
        if (mVectorscope != NULL)
            mVectorscope->SetMode(mVectorscopeMode);
        
        UpdateVectorscopeMode(mVectorscopeMode);

        mVectorscopeModeChanged = false;
    }

    // Quick and dirty method, that doesn't work at 100%
#if !MANAGE_CONTROLS_OVER_GRAPH
    // FIX: vectorscope "number" buttons refresh was bad
    //
    // dirty all the buttons every time, to be sure that they are correctly
    // redrawn over the graph
    for (int i = 0; i < mVectorscopeControls.size(); i++)
    {
        IControl *c = mVectorscopeControls[i];
        c->SetDirty(false);
    }
#endif
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoWidth::VectorscopeSetControlsOverGraphs()
{
    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
    {
        GraphControl12 *graph = mVectorscopeGraphs[i];
        if (graph != NULL)
        {
            for (int j = 0; j < mVectorscopeControls.size(); j++)
            {
                IControl *control = mVectorscopeControls[j];
                graph->AddControlOverGraph(control);
            }
        }
    }
}

void
StereoWidth::VectorscopeClearControlsOverGraphs()
{
    for (int i = 0; i < NUM_VECTORSCOPE_MODES; i++)
    {
        GraphControl12 *graph = mVectorscopeGraphs[i];
        if (graph != NULL)
            graph->ClearControlsOverGraph();
    }
}
    
