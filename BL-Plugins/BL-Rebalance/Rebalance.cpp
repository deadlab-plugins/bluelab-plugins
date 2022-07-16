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
#include <deque>
#include <fstream>
#include <string>
using namespace std;

#include <darknet.h>

#include <GraphControl12.h>
#include <BLSpectrogram4.h>
#include <SpectrogramDisplayScroll4.h>
#include <GraphTimeAxis6.h>
#include <GraphFreqAxis2.h>
#include <GraphAxis2.h>
#include <Scale.h>

#include <FftProcessObj16.h>

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLUtilsFile.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>

#include <RebalanceProcessor2.h>

#include <Rebalance_defs.h>

#include <BlaTimer.h>

#include <ParamSmoother2.h>

#include <BLTransport.h>

// Debug
#define DBG_PERF_APP 1 //0 //1
#if DBG_PERF_APP && (defined APP_API)
#define APP_API 0
#endif

// Do not manage data dumping on WIN32
// (more simple for compilation on WIN32)
#ifndef WIN32
#if APP_API
#include <AudioFile.h>
#endif
#endif

#include "Rebalance.h"

#include "IPlug_include_in_plug_src.h"
#include "IControl.h"

//
// Saving
//

#define SKIP_SOURCE_SILENCE 1 //0
// -60dB is 0.001 in amp scale
#define SKIP_SOURCE_SILENCE_THRS_DB -200.0 //-120.0 //-60.0

// Generation parameters
#if 0 // Mac
#define BASE_DIR "/Volumes/NIKO-1TB/"
#define TARGET_DIR "/Volumes/ND/DeepLearning/data/"
#endif

#if 1 // Linux
#define BASE_DIR "/media/niko/NIKO-1TB/"
//#define TARGET_DIR "/media/niko/ND/DeepLearning/data/"
#define TARGET_DIR "/home/niko/Documents/BlueLabAudio-Debug/DeepLearning/data/"
#endif

#define TRACK_LIST_FILE BASE_DIR"Mixes-DeepLearning/MSD100/Tracks.txt"
#define MIXTURES_PATH BASE_DIR"Mixes-DeepLearning/MSD100/Mixtures"
#define SOURCE_PATH BASE_DIR"Mixes-DeepLearning/MSD100/Sources"

#define VOCALS_FILE_NAME "vocals.wav"
#define BASS_FILE_NAME   "bass.wav"
#define DRUMS_FILE_NAME  "drums.wav"
#define OTHER_FILE_NAME  "other.wav"

#define MIX_SAVE_FILE TARGET_DIR"mix0.dat"
#define MIX_STEREO_SAVE_FILE TARGET_DIR"mix1.dat"
#define SOURCE_SAVE_FILE_VOCAL TARGET_DIR"source0.dat"
#define SOURCE_SAVE_FILE_BASS TARGET_DIR"source1.dat"
#define SOURCE_SAVE_FILE_DRUMS TARGET_DIR"source2.dat"
#define SOURCE_SAVE_FILE_OTHER TARGET_DIR"source3.dat"

#define DEFAULT_SENSITIVITY 1.0

// Fix some possible problem in OpenSourceFile()
#define FIX_OPEN_SOURCE_FILE 1

#define SHOW_QUALITY_BUTTON 0 //1

// Use log on magnitudes (to be like db scale)
//
// NOTE: do not use it for the moment, because without it,
// we hae high contrast, and this could be better for training.
#define USE_DB_MAGNS 0 //1

#define DB_EPS 1e-15
#define MIN_DB -200.0

#define MAX_NUM_TIME_AXIS_LABELS 10

// On Linux, launch Rebalance app for every track
// to avoid swap overflow.
// Use the enveironment variable BL_TRACK_NUM
// to know which track to process
#define DUMP_SAVE_MEM_LINUX 1

// DEBUG
#define DBG_HIDE_GRAPH 0 //1

// Dump each data, including overlap x4
// => will dump 4x more data
#define FULL_OVERLAP_DUMP 1

// Dump real mono signal instead of taking only the left channel
#define DUMP_REAL_MONO 1

#define DEBUG_BRIGHTNESS_CONTRAST 0 //1

#if PROCESS_SIGNAL_DB
// 1%
#define SOFT_SOLO_VALUE 1.0
#define MAX_MIX_VALUE 200.0
#else
#define SOFT_SOLO_VALUE 20.0
// NOTE: if changing this value, think to look at SOFT_MASKING_HACK
#define MAX_MIX_VALUE 200.0
#endif

#define UPDATE_SPECTRO_DELAY 250 //1000

// For new GUI: remove some controls
#define USE_SIMPLIFIED_GUI 1

#define MODEL_NUM_FASTER  0
#define MODEL_NUM_BETTER  1
#define MODEL_NUM_OFFLINE 2

#if 0
/*
Seidy (Gearslutz):
- solo button
- out gain knob


TODO: test the following thing: for the stereo case, before any DNN, try to separate each spectrogram part (partials, strokes...) by using stereo similarity (so in the ideal case, we will end with 4 spectrogams classes). Use technique from Panogram, and possibly technique from DUET. Then finally apply DNN to the classes, to recognize the type of the class (vocal, bass...).
Maybe use a DUET in 3D (magns, phases, and frequencies for the 3rd dimension). Make dilation, then erosion, then find connected components.

SUPPORT: ziouziou.100@gmail.com (support bl), wanted to separate vocal, to keep only instruments. He asked several times. When improved, maybe send him an email.
SUPPORT: when new version, write email to wheatwilliams1@gmail.com
(remove vocal => pumping effect)

Spectrogram: see https://www.youtube.com/watch?v=KUZLXAK8do4
(maybe do not set axes on spectrogram...). Horizontal or vertical?
Maybe no big knob...

TODO: test training with dct/idct, for "decorrelating data"

BUG: when rendering at 88200Hz, the detecftion is bad.

TODO: add a spetrogram view on the top of Rebalance. And a button to hide and disable it (to gain space, and perfs). When hidden, it will hide beightness and contrast too.

TODO: add a out gain knob !!! (as we often keep only one part, we loose gain, and need to increase it again!)

TODO: check presets in IPlug2


IDEA: maybe do not use BLAS in the plugin (this could be risky...)

IDEA: if multiple representation works very well, pass the result to the test pointed by the papers, to compare. (MNIST etc...)

IDEA: make a version of darknet that works on complex numbers, then add re/im to train (instead of magns)

TODO: training takes normalized sounds. apply it in prediction too.

New Spotify paper (Factor GAN): https://research.atspotify.com/making-sense-of-music-by-extracting-and-analyzing-individual-instruments-in-a-song/?fbclid=IwAR0JaFy3clD38KMe-AwkUxdjVgSExGZ9AR7sI7NDEYuJqH5BQ_JPtLmjiVk
https://openreview.net/forum?id=Hye1RJHKwB
https://github.com/f90/FactorGAN	

Real Time Souce Separation: https://www.frontiersin.org/articles/10.3389/fnins.2020.00434/full

Real Time Source Separation (simple)
https://www.latentsound.com/?fbclid=IwAR0JaFy3clD38KMe-AwkUxdjVgSExGZ9AR7sI7NDEYuJqH5BQ_JPtLmjiVk

Griffin Lim: https://github.com/bkvogel/griffin_lim/blob/master/audio_utilities.py

			 
TODO: change the name to REBALANCER (Rebalance plugin/vet is 1st result in google for the moment, before RX8...)

TODO: try to use 4 separared models (with unet), and see if it is different than a single model for 4 instruments.

IDEA: avoid computing sound always in 44100Hz (and upsampling after if necessary): downsample to 44100H, compute masks, upsample masks, then apply them to the native samplerate!

- TEST: Check on some other tracks
- TODO: check on all hosts, to check for latency=10k

- IDEA: when modulo, compute data every time at the beginning?

- SITE: comparison with previous version (sound files)
- SITE: "karaoke" sound file, with voice removed
- COMMERCIAL ARG: "this is not an online service, this is a simple plugin!"
"state of the art technique"
"it is made with 100% BlueLab own technology" => no third party tehnology
- COMMERCIAL: all the features are included in the plugin, no need for a standalone application,
or connecting to an online portal.

- NOTE: at the beginning of the bounce, is it really separated?

NOTE: in general plugins latency can go until about 1 second!
(0.8 or 0.9 seconds looks possible)
https://www.kvraudio.com/forum/viewtopic.php?t=407732
and with 32x256, 32 samples with overlap 4 makes 0.37 seconds
*/
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipVocal = "Vocal - Increase or decrease vocal";
static char *tooltipBass = "Bass - Increase or decrease bass";
static char *tooltipDrums = "Drums - Increase or decrease drums";
static char *tooltipOther = "Other - Increase or decrease other parts";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipSolo0 = "Solo Vocal";
static char *tooltipSolo1 = "Solo Bass";
static char *tooltipSolo2 = "Solo Drums";
static char *tooltipSolo3 = "Solo Other";
static char *tooltipMute0 = "Mute Vocal";
static char *tooltipMute1 = "Mute Bass";
static char *tooltipMute2 = "Mute Drums";
static char *tooltipMute3 = "Mute Other";
static char *tooltipModel = "Model - Choose DNN model quality";

enum EParams
{    
    kVocal = 0,
    kBass,
    kDrums,
    kOther,

#if !USE_SIMPLIFIED_GUI
    kVocalSensitivity,
    kBassSensitivity,
    kDrumsSensitivity,
    kOtherSensitivity,
#endif

#if !USE_SIMPLIFIED_GUI
    // Global precision
    kPrecision,
#endif

#if !USE_SIMPLIFIED_GUI
#if SHOW_QUALITY_BUTTON
    kQuality,
#endif
#endif

#if !USE_SIMPLIFIED_GUI
#if USE_DBG_PREDICT_MASK_THRESHOLD
    kDbgThreshold,
#endif
#endif

    kSolo0,
    kSolo1,
    kSolo2,
    kSolo3,

    kMute0,
    kMute1,
    kMute2,
    kMute3,

#if !USE_SIMPLIFIED_GUI
    // For debugging
    kRange,
    kContrast,
    kHardSolo,
#endif

    kModel,

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
    
    kVocalX = 39,
    kVocalY = 344,
    
    kBassX = 153, //152,
    kBassY = 344,
    
    kDrumsX = 261,
    kDrumsY = 344,
    
    kOtherX = 370, //369,
    kOtherY = 344,

#if !USE_SIMPLIFIED_GUI
    // Global precision
    // (soft/hard)
    kPrecisionX = 470,
    kPrecisionY = 80,
    kKnobPrecisionFrames = 180,

    // Precisions
    kVocalSensitivityX = 52,
    kVocalSensitivityY = 200,
    kKnobVocalSensitivityFrames = 180,
    
    kBassSensitivityX = 152,
    kBassSensitivityY = 200,
    kKnobBassSensitivityFrames = 180,
    
    kDrumsSensitivityX = 252,
    kDrumsSensitivityY = 200,
    kKnobDrumsSensitivityFrames = 180,
    
    kOtherSensitivityX = 352,
    kOtherSensitivityY = 200,
    kKnobOtherSensitivityFrames = 180,
#endif
    
    kOutGainX = 486,
    kOutGainY = 415,
    kKnobOutGainFrames = 180,

    //
    kSolo0X = 52,
    kSolo0Y = 476,

    kSolo1X = 165,
    kSolo1Y = 476,

    kSolo2X = 274,
    kSolo2Y = 476,

    kSolo3X = 383,
    kSolo3Y = 476,

    //
    kMute0X = 80, 
    kMute0Y = 476,

    kMute1X = 193,
    kMute1Y = 476,

    kMute2X = 302,
    kMute2Y = 476,

    kMute3X = 411,
    kMute3Y = 476,    

#if !USE_SIMPLIFIED_GUI
    
#if SHOW_QUALITY_BUTTON
    kRadioButtonsQualityX = 455,
    kRadioButtonsQualityY = 46,
    kRadioButtonQualityVSize = 96,
    kRadioButtonQualityNumButtons = 6,
    kRadioButtonsQualityFrames = 2,
#endif

#endif

#if !USE_SIMPLIFIED_GUI
    // For debugging
    kRangeX = 415,
    kRangeY = 80,
    kRangeFrames = 180,
    
    kContrastX = 415,
    kContrastY = 160,
    kContrastFrames = 180,

    kHardSoloX = 20,
    kHardSoloY = 590
#endif

    kModelX = 462,
    kModelY = 362,
    kModelWidth = 80
};

//
Rebalance::Rebalance(const InstanceInfo &info)
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


Rebalance::~Rebalance()
{
    if (mPredictProcessor != NULL)
        delete mPredictProcessor;
    
    for (int i = 0; i < NUM_STEM_SOURCES + 1; i++)
    {
        if (mDumpProcessors[i] != NULL)
            delete mDumpProcessors[i];
    }

    if (mTimeAxis != NULL)
        delete mTimeAxis;
  
    if (mFreqAxis != NULL)
        delete mFreqAxis;
  
    if (mHAxis != NULL)
        delete mHAxis;
  
    if (mVAxis != NULL)
        delete mVAxis;

    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mSpectroDisplayScrollState != NULL)
        delete mSpectroDisplayScrollState;
    
    if (mTransport != NULL)
        delete mTransport;
}

IGraphics *
Rebalance::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    mGraphics = graphics;

    return graphics;
}

void
Rebalance::MyMakeLayout(IGraphics *pGraphics)
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

    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
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
Rebalance::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mGUIHelper = NULL;

    mIsPlaying = false;
    
    mGraph = NULL;

    mHAxis = NULL;
    mVAxis = NULL;

    mTimeAxis = NULL;
    mFreqAxis = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectroDisplayScrollState = NULL;
    
    mMustUpdateSpectrogram = true;
    mMustUpdateTimeAxis = true;
        
    mPrevSampleRate = GetSampleRate();
    mWasPlaying = false;
    
    // In release, do not display the model
    _darknet_quiet_flag = 1;

    // Very useful for debugging!
    //_darknet_quiet_flag = 0;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mVocalSensitivity = DEFAULT_SENSITIVITY;
    mBassSensitivity = DEFAULT_SENSITIVITY;
    mDrumsSensitivity = DEFAULT_SENSITIVITY;
    mOtherSensitivity = DEFAULT_SENSITIVITY;
    
    mDumpCount = 0;
    
    mQualityChanged = false;
    
    // Processors
    mPredictProcessor = NULL;
    
    for (int i = 0; i < NUM_STEM_SOURCES + 1; i++)
    {
        mDumpProcessors[i] = NULL;
    }

    mOutGain = 1.0;
    mOutGainSmoother = NULL;

    for (int i = 0; i < 4; i++)
        mSolos[i] = false;
    for (int i = 0; i < 4; i++)
        mMutes[i] = false;
    
    mIsInitialized = false;
    mGraphics = NULL;

    mTransport = NULL;

    mModelNum = -1; //0;

#if USE_RESAMPLER
    BL_FLOAT sampleRate = GetSampleRate();
    CheckSampleRate(&sampleRate);
    mPrevSampleRate = sampleRate;

    for (int i = 0; i < 2; i++)
    {
        // WDL_Resampler::SetMode arguments are bool interp,
        // int filtercnt, bool sinc, int sinc_size, int sinc_interpsize
        // sinc mode will get better results, but will use more cpu
        // todo: explain arguments
        mResamplersIn[i].SetMode(true, 1, false, 0, 0);
        mResamplersIn[i].SetFilterParms();
        // set it output driven
        mResamplersIn[i].SetFeedMode(false);
        // set input and output samplerates
        mResamplersIn[i].SetRates(GetSampleRate(), sampleRate);
    }

    for (int i = 0; i < 2; i++)
    {
        // WDL_Resampler::SetMode arguments are bool interp,
        // int filtercnt, bool sinc, int sinc_size, int sinc_interpsize
        // sinc mode will get better results, but will use more cpu
        // todo: explain arguments
        mResamplersOut[i].SetMode(true, 1, false, 0, 0);
        mResamplersOut[i].SetFilterParms();
        // set it output driven
        mResamplersOut[i].SetFeedMode(false);
        // set input and output samplerates
        mResamplersOut[i].SetRates(sampleRate, GetSampleRate());
    }
#endif
}

void
Rebalance::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
        
#if APP_API
    // App
    InitApp(sampleRate);
#else 
    // Plugin
    InitPlug(sampleRate);
#endif

    mIsInitialized = true;
}

void
Rebalance::InitParams()
{
    // Vocal
#if !APP_API
    BL_FLOAT defaultVocal = 100.0;
    
    mVocal = defaultVocal*0.01;
    GetParam(kVocal)->InitDouble("VocalMix", defaultVocal,
                                 0.0, MAX_MIX_VALUE, 0.1, "%");
#endif
    
    // Bass
#if !APP_API
    BL_FLOAT defaultBass = 100.0;
    mBass = defaultBass*0.01;
    
    GetParam(kBass)->InitDouble("BassMix", defaultBass,
                                0.0, MAX_MIX_VALUE, 0.1, "%");
#endif
    
    // Drums
#if !APP_API
    BL_FLOAT defaultDrums = 100.0;
    mDrums = defaultDrums*0.01;
    
    GetParam(kDrums)->InitDouble("DrumsMix", defaultDrums,
                                 0.0, MAX_MIX_VALUE, 0.1, "%");
#endif
    
    // Other
#if !APP_API
    BL_FLOAT defaultOther = 100.0;
    mOther = defaultOther*0.01;
    
    GetParam(kOther)->InitDouble("OtherMix", defaultOther,
                                 0.0, MAX_MIX_VALUE, 0.1, "%");
#endif

#if !USE_SIMPLIFIED_GUI
    // New code
    // Precision
    BL_FLOAT defaultMasksContrast = 0.0; // Soft
    mMasksContrast = defaultMasksContrast;
    
    GetParam(kPrecision)->InitDouble("SoftHard", defaultMasksContrast,
                                     0.0, 100.0, 0.1, "%");
#endif

#if !USE_SIMPLIFIED_GUI
    
    // Vocal precision
#if !APP_API
    
    BL_FLOAT defaultVocalSensitivity = DEFAULT_SENSITIVITY*100.0;
    mVocalSensitivity = defaultVocalSensitivity/100.0;
    
    GetParam(kVocalSensitivity)->InitDouble("VocalSensitivity",
                                            defaultVocalSensitivity,
                                            0.0, 100.0, 0.1, "%");
#endif
    
    // Bass precision
#if !APP_API
    
    BL_FLOAT defaultBassSensitivity = DEFAULT_SENSITIVITY*100.0;
    mBassSensitivity = defaultBassSensitivity/100.0;
    
    GetParam(kBassSensitivity)->InitDouble("BassSensitivity",
                                           defaultBassSensitivity,
                                           0.0, 100.0, 0.1, "%");
#endif
    
    // Drums precision
#if !APP_API
    
    BL_FLOAT defaultDrumsSensitivity = DEFAULT_SENSITIVITY*100.0;
    mDrumsSensitivity = defaultDrumsSensitivity/100.0;
    
    GetParam(kDrumsSensitivity)->InitDouble("DrumsSensitivity",
                                            defaultDrumsSensitivity,
                                            0.0, 100.0, 0.1, "%");
    
#endif
    
    // Other precision
#if !APP_API
    
    BL_FLOAT defaultOtherSensitivity = DEFAULT_SENSITIVITY*100.0;
    mOtherSensitivity = defaultOtherSensitivity/100.0;
    
    GetParam(kOtherSensitivity)->InitDouble("OtherSensitivity",
                                            defaultOtherSensitivity,
                                            0.0, 100.0, 0.1, "%");
    
#endif

#endif

#if !USE_SIMPLIFIED_GUI
    
#if SHOW_QUALITY_BUTTON
    // Quality
    int defaultQuality = 0;
    GetParam(kQuality)->InitInt("Quality", (int)defaultQuality, 0, 5);
#endif

#endif
    
#if USE_DBG_PREDICT_MASK_THRESHOLD
    GetParam(kDbgThreshold)->InitDouble("DBG_Threshold",
                                        0.0, 0.0, 1.0, 0.001, "");
#endif

#if !APP_API
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 1 is 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");
#endif

#if !USE_SIMPLIFIED_GUI
    
#if !APP_API

#if DEBUG_BRIGHTNESS_CONTRAST
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    GetParam(kRange)->InitDouble("Brightness", defaultRange, -1.0, 1.0, 0.01, "");
    
    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
#endif
    
#endif

#endif

#if !USE_SIMPLIFIED_GUI
    mHardSolo = false;
    GetParam(kHardSolo)->InitInt("HardSolo", (int)mHardSolo, 0, 1, "");
#endif

    GetParam(kModel)->InitEnum("Model", 0, 3, "", IParam::kFlagsNone,
                               "", "Faster", "Better", "Offline");
    
    InitSoloMuteButtonsParams();
}

void
Rebalance::InitSoloMuteButtonsParams()
{
    //GetParam(kSolo0)->InitInt("VocalSolo", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kSolo0)->InitEnum("VocalSolo", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
    
    //GetParam(kSolo1)->InitInt("BassSolo", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kSolo1)->InitEnum("BassSolo", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
 
    //GetParam(kSolo2)->InitInt("DrumsSolo", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kSolo2)->InitEnum("DrumsSolo", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
    
    //GetParam(kSolo3)->InitInt("OtherSolo", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kSolo3)->InitEnum("OtherSolo", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");

    //GetParam(kMute0)->InitInt("VocalMute", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kMute0)->InitEnum("VocalMute", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
 
    //GetParam(kMute1)->InitInt("BassMute", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kMute1)->InitEnum("BassMute", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
    
    //GetParam(kMute2)->InitInt("DrumsMute", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kMute2)->InitEnum("DrumsMute", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
 
    //GetParam(kMute3)->InitInt("OtherMute", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kMute3)->InitEnum("OtherMute", 0, 2,
                               "", IParam::kFlagMeta, "",
                               "Off", "On");
}

void
Rebalance::InitApp(BL_FLOAT sampleRate)
{
    for (int i = 0; i < NUM_STEM_SOURCES + 1; i++)
    {
        mDumpProcessors[i] = new RebalanceProcessor2(sampleRate,
                                                     REBALANCE_TARGET_SAMPLE_RATE,
                                                     REBALANCE_BUFFER_SIZE,
                                                     REBALANCE_TARGET_BUFFER_SIZE,
                                                     REBALANCE_OVERLAPPING,
                                                     REBALANCE_NUM_SPECTRO_COLS);

        int dumpOverlap = 1;
#if FULL_OVERLAP_DUMP
        dumpOverlap = 4;
#endif
        mDumpProcessors[i]->InitDump(dumpOverlap);
    }
    
    GenerateMSD100();
}
                                                  
void
Rebalance::InitPlug(BL_FLOAT sampleRate)
{
    mPredictProcessor = new RebalanceProcessor2(sampleRate,
                                                REBALANCE_TARGET_SAMPLE_RATE,
                                                REBALANCE_BUFFER_SIZE,
                                                REBALANCE_TARGET_BUFFER_SIZE,
                                                REBALANCE_OVERLAPPING,
                                                REBALANCE_NUM_SPECTRO_COLS);
 
    mPredictProcessor->InitDetect(*this);

    BL_FLOAT defaultOutGain = 1.0; // 1 is 0dB
    mOutGain = defaultOutGain;    
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
    
    SetColorMap();

#if USE_SIMPLIFIED_GUI
    mMasksContrast = 0.0;

    mVocalSensitivity = DEFAULT_SENSITIVITY;
    mBassSensitivity = DEFAULT_SENSITIVITY;
    mDrumsSensitivity = DEFAULT_SENSITIVITY;
    mOtherSensitivity = DEFAULT_SENSITIVITY;

    mHardSolo = true;
#endif
    
    ApplyParams();
    
    UpdateLatency();

    UpdateTimeAxis();
    
    mTransport = new BLTransport(sampleRate);
    if (mTransport != NULL)
        mTransport->SetSoftResynchEnabled(true);
    
    if (mGraph != NULL)
        mGraph->SetTransport(mTransport);
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetTransport(mTransport);
    if (mTimeAxis != NULL)
        mTimeAxis->SetTransport(mTransport);

    mNeedRecomputeSpectrogram = true;
    mPrevSpectroUpdateTime = BLUtils::GetTimeMillis();
    mNeedRecomputeMasks = false;
}

void
Rebalance::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    //if (BLDebug::ExitAfter(this, 2)) //10))
    //    return;
        
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    bool isPlaying = IsTransportPlaying();
    BL_FLOAT transportTime = BLUtilsPlug::GetTransportTime(this);

    mIsPlaying = isPlaying;

    // If APP_API and DBG_PERF_APP, we want the time axis to scroll
    // (like if we monitored)
    bool fakeMonitorEnabled = false;
#if APP_API && DBG_PERF_APP
    fakeMonitorEnabled = true;
#endif

    if (mGraph != NULL)
        mGraph->Lock();

    if (mModelNum == MODEL_NUM_OFFLINE)
    {
        // With this, time axis labels are really better aligned with spectrogram
        if (mTransport != NULL)
            mTransport->HardResynch();
    }
    
    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();
    
#if APP_API && !DBG_PERF_APP
    // If we are in standalone application mode, we will want to load
    // sound files using buttons and to generate the out audio file and binary masks
    // in one step (not in ProcessDoubleReplacing)
    return;
#endif
    
    if (mQualityChanged)
    {
        UpdateLatency();
      
        mQualityChanged = false;
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
    
#if 0 //1 // Debug
    if (BLUtilsPlug::PlugIOAllZero(in, out))
        return;
#endif    
    
    if (mUIOpened)
    {
        if (mTransport != NULL)
        {
            mTransport->SetTransportPlaying(isPlaying, fakeMonitorEnabled,
                                            transportTime, nFrames);
        }

        if (!isPlaying && mWasPlaying)
            // Playing just stops and monitor is enabled
        {
            // => Reset time axis time
            if (mTransport != NULL)
                mTransport->SetDAWTransportValueSec(0.0);
        }

        mWasPlaying = isPlaying;

        if (isPlaying && !fakeMonitorEnabled)
        {
            if (mTransport != NULL)
                mTransport->SetDAWTransportValueSec(transportTime);
        }
        
        if (mTimeAxis != NULL)
        {
            if (isPlaying || fakeMonitorEnabled)
            {
                if (mMustUpdateTimeAxis)
                {
                    UpdateTimeAxis();
                    mMustUpdateTimeAxis = false;
                }
            }
        }
    }
    
    //mWasPlaying = isPlaying;

    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes,
    // this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
  
    mSecureRestarter.Process(in);
    
    // If we have only one channel, duplicate it
    // (simpler for the rest...) 
    if (in.size() == 1)
    {
        in.resize(2);
        in[1] = in[0];
    }
    
    out = in;
    
#if USE_RESAMPLER
    if (mUseResampler)
    {
        // FIX: with sample rate 88200Hz, the performances will drop
        // progressively while playing
        //
        // This was due to accumulation of samples in the second channel 
        //
        // Keep only the first channel
        // So next when call to mDispFftObj->Process(in, dummy, &out);
        // This will void haveing two input channels of different size

        BL_FLOAT sampleRate = GetSampleRate();
        CheckSampleRate(&sampleRate);
        
        WDL_ResampleSample *resampledAudio=NULL;
        int numSamples = nFrames*sampleRate/GetSampleRate();

        for (int k = 0; k < out.size(); k++)
        {
            int numSamples0 = mResamplersIn[k].ResamplePrepare(numSamples, 1,
                                                               &resampledAudio);
            for (int i = 0; i < numSamples0; i++)
            {
                if (i >= nFrames)
                    break;
     
                resampledAudio[i] = out[k].Get()[i];
            }
      
            WDL_TypedBuf<BL_FLOAT> &outSamples = mTmpBuf3;
            outSamples.Resize(numSamples);
            int numResampled = mResamplersIn[k].ResampleOut(outSamples.Get(),
                                                            nFrames, numSamples, 1);

            if (numResampled != numSamples)
            {
                // Failed somehow
                memset(outSamples.Get(), 0, numSamples*sizeof(BL_FLOAT));
            }
            
            out[k] = outSamples; // TODO: improve memory management here
        }
    }
#endif
    
    // Avoid taking a lot of resources when not playing
    if (isPlaying || fakeMonitorEnabled)
    {
        if (mPredictProcessor != NULL)
            mPredictProcessor->Process(&out);
    }

#if USE_RESAMPLER
    if (mUseResampler)
    {
        // FIX: with sample rate 88200Hz, the performances will drop
        // progressively while playing
        //
        // This was due to accumulation of samples in the second channel 
        //
        // Keep only the first channel
        // So next when call to mDispFftObj->Process(in, dummy, &out);
        // This will void haveing two input channels of different size

        BL_FLOAT sampleRate = GetSampleRate();
        CheckSampleRate(&sampleRate);
        
        WDL_ResampleSample *resampledAudio=NULL;
        int numSamples = nFrames*GetSampleRate()/sampleRate;

        for (int k = 0; k < out.size(); k++)
        {
            int numSamples0 = mResamplersOut[k].ResamplePrepare(nFrames, 1,
                                                                &resampledAudio);
            for (int i = 0; i < numSamples0; i++)
            {
                if (i >= nFrames)
                    break;
     
                resampledAudio[i] = out[k].Get()[i];
            }
      
            WDL_TypedBuf<BL_FLOAT> &outSamples = mTmpBuf4;
            outSamples.Resize(nFrames);
            int numResampled = mResamplersOut[k].ResampleOut(outSamples.Get(),
                                                             numSamples, nFrames, 1);

            if (numResampled != nFrames)
            {
                // Failed somehow
                memset(outSamples.Get(), 0, numSamples*sizeof(BL_FLOAT));
            }
            
            out[k] = outSamples; // TODO: improve memory management here
        }
    }
#endif
    
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
                           
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);

    /*if (mUIOpened)
      {
      if (isPlaying)
      mMustUpdateSpectrogram = true;
      }*/

    if (mUIOpened)
    {
        if (isPlaying || fakeMonitorEnabled)
        {
            // Update the spectrogram GUI
            // Must do it here to be in the correct thread
            // (and not in the idle() thread
            //if (mMustUpdateSpectrogram)
            //{
            if (mSpectrogramDisplay != NULL)
            {
                mSpectrogramDisplay->UpdateSpectrogram(true);
                mMustUpdateSpectrogram = false;
            }
            //}
            //else
            //{
            //    // Do not update the spectrogram texture
            //    if (mSpectrogramDisplay != NULL)
            //    {
            //        mSpectrogramDisplay->UpdateSpectrogram(false);
            //    }
            //}
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
Rebalance::CreateControls(IGraphics *pGraphics)
{    
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
#if !DBG_HIDE_GRAPH
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);

    mGraph->SetUseLegacyLock(true);
    
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
#endif

#if !APP_API
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kVocalX, kVocalY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kVocal,
                              TEXTFIELD_FN,
                              "VOCAL",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipVocal);

    mGUIHelper->CreateKnobSVG(pGraphics,
                              kBassX, kBassY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kBass,
                              TEXTFIELD_FN,
                              "BASS",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipBass);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kDrumsX, kDrumsY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kDrums,
                              TEXTFIELD_FN,
                              "DRUMS",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipDrums);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOtherX, kOtherY,
                              kKnobWidth, kKnobHeight,
                              KNOB_FN,
                              kOther,
                              TEXTFIELD_FN,
                              "OTHER",
                              GUIHelper12::SIZE_BIG,
                              NULL, true,
                              tooltipOther);

#if !USE_SIMPLIFIED_GUI
    mGUIHelper->CreateKnob(pGraphics,
                           kPrecisionX, kPrecisionY,
                           KNOB_SMALL_FN,
                           kKnobPrecisionFrames,
                           kPrecision,
                           TEXTFIELD_FN,
                           "HARDNESS");
#endif

#if !USE_SIMPLIFIED_GUI
    mGUIHelper->CreateKnob(pGraphics,
                           kVocalSensitivityX, kVocalSensitivityY,
                           KNOB_SMALL_FN,
                           kKnobVocalSensitivityFrames,
                           kVocalSensitivity,
                           TEXTFIELD_FN,
                           "SENSITIVITY");
    
    mGUIHelper->CreateKnob(pGraphics,
                           kBassSensitivityX, kBassSensitivityY,
                           KNOB_SMALL_FN,
                           kKnobBassSensitivityFrames,
                           kBassSensitivity,
                           TEXTFIELD_FN,
                           "SENSITIVITY");
  
    mGUIHelper->CreateKnob(pGraphics,
                           kDrumsSensitivityX, kDrumsSensitivityY,
                           KNOB_SMALL_FN,
                           kKnobDrumsSensitivityFrames,
                           kDrumsSensitivity,
                           TEXTFIELD_FN,
                           "SENSITIVITY");
    
    mGUIHelper->CreateKnob(pGraphics,
                           kOtherSensitivityX, kOtherSensitivityY,
                           KNOB_SMALL_FN,
                           kKnobOtherSensitivityFrames,
                           kOtherSensitivity,
                           TEXTFIELD_FN,
                           "SENSITIVITY");
#endif
    
#endif

#if !USE_SIMPLIFIED_GUI
    
#if SHOW_QUALITY_BUTTON
    
#define NUM_RADIO_LABELS_QUALITY 6
    const char *radioLabelsQuality[NUM_RADIO_LABELS_QUALITY] =
    { "RT - FAST", "RT - MED", "RT - BEST", "BC - FAST", "BC - MED", "BC - BEST" };
    
    mGUIHelper->CreateRadioButtonsEx(this, pGraphics,
                                     RADIOBUTTON_QUALITY_ID,
                                     RADIOBUTTON_QUALITY_FN,
                                     kRadioButtonsQualityFrames,
                                     kRadioButtonsQualityX,
                                     kRadioButtonsQualityY,
                                     kRadioButtonQualityNumButtons,
                                     kRadioButtonQualityVSize,
                                     kQuality, false, "QUALITY",
                                     RADIOBUTTON_DIFFUSE_ID, RADIOBUTTON_DIFFUSE_FN,
                                     IText::kAlignNear, IText::kAlignNear,
                                     radioLabelsQuality,
                                     NUM_RADIO_LABELS_QUALITY,
                                     TITLE_TEXT_OFFSET_Y,
                                     GUIHelper11::SIZE_SMALL);
#endif

#endif
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX, kOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT, NULL, true,
                              tooltipOutGain);

#if !USE_SIMPLIFIED_GUI
    
#if !APP_API

#if DEBUG_BRIGHTNESS_CONTRAST
    // Range
    mGUIHelper->CreateKnob(pGraphics,
                           kRangeX, kRangeY,
                           KNOB_SMALL_FN,
                           kRangeFrames,
                           kRange,
                           TEXTFIELD_FN,
                           "BRIGHTNESS",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Contrast
    mGUIHelper->CreateKnob(pGraphics,
                           kContrastX, kContrastY,
                           KNOB_SMALL_FN,
                           kContrastFrames,
                           kContrast,
                           TEXTFIELD_FN,
                           "CONTRAST",
                           GUIHelper12::SIZE_DEFAULT);
#endif
    
#endif

#endif

#if !USE_SIMPLIFIED_GUI
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kHardSoloX,
                                   kHardSoloY,
                                   CHECKBOX_FN,
                                   kHardSolo, "HARD SOLO",
                                   GUIHelper12::SIZE_SMALL);
#endif

    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kModelX, kModelY,
                                   kModelWidth,
                                   kModel,
                                   "MODEL",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipModel);
    
    CreateSoloMuteButtons(pGraphics);
    
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

//
#define CREATE_SOLO_BUTTON(__BUTTON_NUM__)                              \
{                                                                       \
    mGUIHelper->CreateRolloverButton(pGraphics,                         \
                                     kSolo##__BUTTON_NUM__##X,          \
                                     kSolo##__BUTTON_NUM__##Y,          \
                                     SOLO_TOGGLE_BUTTON_FN,             \
                                     kSolo##__BUTTON_NUM__, "",         \
                                     true,                              \
                                     true, false,                       \
                                     tooltipSolo##__BUTTON_NUM__);      \
}

#define CREATE_MUTE_BUTTON(__BUTTON_NUM__)                              \
{                                                                       \
    mGUIHelper->CreateRolloverButton(pGraphics,                         \
                                     kMute##__BUTTON_NUM__##X,          \
                                     kMute##__BUTTON_NUM__##Y,          \
                                     MUTE_TOGGLE_BUTTON_FN,             \
                                     kMute##__BUTTON_NUM__, "",         \
                                     true,                              \
                                     true, false,                       \
                                     tooltipMute##__BUTTON_NUM__);      \
}

void
Rebalance::CreateSoloMuteButtons(IGraphics *pGraphics)
{
    CREATE_SOLO_BUTTON(0);
    CREATE_SOLO_BUTTON(1);
    CREATE_SOLO_BUTTON(2);
    CREATE_SOLO_BUTTON(3);

    CREATE_MUTE_BUTTON(0);
    CREATE_MUTE_BUTTON(1);
    CREATE_MUTE_BUTTON(2);
    CREATE_MUTE_BUTTON(3);
}

void
Rebalance::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Rebalance::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    if (mTransport != NULL)
        mTransport->Reset();
    
    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset
  
    DoReset();

    mDumpCount = 0;
    
    UpdateLatency();

    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    if (mSpectrogramDisplay != NULL)
    {
        mSpectrogramDisplay->SetFftParams(REBALANCE_BUFFER_SIZE,
                                          REBALANCE_OVERLAPPING,
                                          sampleRate);
    }
    
    if (mTransport != NULL)
    {
        //mTransport->Reset(sampleRate);
        BL_FLOAT realSampleRate = GetSampleRate();
        mTransport->Reset(realSampleRate);
    }
    
    UpdateTimeAxis();

#if USE_RESAMPLER
    for (int i = 0; i < 2; i++)
    {
        mResamplersIn[i].Reset();
        // set input and output samplerates
        mResamplersIn[i].SetRates(GetSampleRate(), sampleRate);
    }

    for (int i = 0; i < 2; i++)
    {
        mResamplersOut[i].Reset();
        // set input and output samplerates
        mResamplersOut[i].SetRates(sampleRate, GetSampleRate());
    }
#endif
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Rebalance::DoReset()
{
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    int blockSize = GetBlockSize();
    
    if (mPredictProcessor != NULL)
        mPredictProcessor->Reset(sampleRate, blockSize);
    
    for (int i = 0; i < NUM_STEM_SOURCES + 1; i++)
    {
        if (mDumpProcessors[i] != NULL)
            mDumpProcessors[i]->Reset(sampleRate, blockSize);
    }

    if (sampleRate != mPrevSampleRate)
    {
        // Frame rate have changed
        if (mFreqAxis != NULL)
            mFreqAxis->Reset(REBALANCE_BUFFER_SIZE, sampleRate);
        
        mPrevSampleRate = sampleRate;
    }

    if (mSpectrogramDisplay != NULL)
    {
        mSpectrogramDisplay->SetFftParams(REBALANCE_BUFFER_SIZE,
                                          REBALANCE_OVERLAPPING,
                                          sampleRate);
    }

    UpdateTimeAxis();
}

#define CHECK_SOLO_PARAM(__PARAM_NUM__)                                 \
    case kSolo##__PARAM_NUM__:                                          \
    {                                                                   \
        int value = GetParam(paramIdx)->Int();                          \
        mSolos[__PARAM_NUM__] = value;                                  \
        UpdateSoloMute();                                               \
        CheckRecomputeSpectrogram();                                    \
    }                                                                   \
    break;

#define CHECK_MUTE_PARAM(__PARAM_NUM__)                                 \
    case kMute##__PARAM_NUM__:                                          \
    {                                                                   \
        int value = GetParam(paramIdx)->Int();                          \
        mMutes[__PARAM_NUM__] = value;                                  \
        UpdateSoloMute();                                               \
        CheckRecomputeSpectrogram();                                    \
    }                                                                   \
    break;

void
Rebalance::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
#if !APP_API
        case kVocal:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            mVocal = ComputeMixParam(value);
                
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetVocal(mVocal);

            UpdateSoloMute();

            CheckRecomputeSpectrogram();
        }
        break;
    
        case kBass:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            mBass = ComputeMixParam(value);
                
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetBass(mBass);

            UpdateSoloMute();

            CheckRecomputeSpectrogram();
        }
        break;
         
        case kDrums:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            mDrums = ComputeMixParam(value);
                
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetDrums(mDrums);

            UpdateSoloMute();

            CheckRecomputeSpectrogram();
        }
        break;
          
        case kOther:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            mOther = ComputeMixParam(value);
            
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetOther(mOther);

            UpdateSoloMute();

            CheckRecomputeSpectrogram();
        }
        break;

#if !USE_SIMPLIFIED_GUI
        // Global precision
        case kPrecision:
        {
            BL_FLOAT contrast = GetParam(paramIdx)->Value();
          
            mMasksContrast = contrast/100.0;
        
            // Set progressive soft/hard setting
            // Good! => it is progressive
            mMasksContrast = BLUtils::ApplyParamShape(mMasksContrast, 1.0/2.5);
        
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetMasksContrast(mMasksContrast);

            CheckRecomputeSpectrogram();
        }
        break;
#endif

#if !USE_SIMPLIFIED_GUI
        case kVocalSensitivity:
        {
            BL_FLOAT vocalSensitivity = GetParam(paramIdx)->Value();
        
            mVocalSensitivity = vocalSensitivity/100.0;
        
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetVocalSensitivity(mVocalSensitivity);

            CheckRecomputeSpectrogram();
        }
        break;
    
        case kBassSensitivity:
        {
            BL_FLOAT bassSensitivity = GetParam(paramIdx)->Value();
        
            mBassSensitivity = bassSensitivity/100.0;
        
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetBassSensitivity(mBassSensitivity);

            CheckRecomputeSpectrogram();
        }
        break;
    
        case kDrumsSensitivity:
        {
            BL_FLOAT drumsSensitivity = GetParam(paramIdx)->Value();
        
            mDrumsSensitivity = drumsSensitivity/100.0;
        
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetDrumsSensitivity(mDrumsSensitivity);

            CheckRecomputeSpectrogram();
        }
        break;
    
        case kOtherSensitivity:
        {
            BL_FLOAT otherSensitivity = GetParam(paramIdx)->Value();
        
            mOtherSensitivity = otherSensitivity/100.0;
        
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetOtherSensitivity(mOtherSensitivity);

            CheckRecomputeSpectrogram();
        }
        break;
#endif
        
#endif

#if USE_DBG_PREDICT_MASK_THRESHOLD
        case kDbgThreshold:
        {
            BL_FLOAT thrs = GetParam(paramIdx)->Value();
            
            if (mPredictProcessor != NULL)
                mPredictProcessor->SetDbgThreshold(thrs);

            CheckRecomputeSpectrogram();
        }
        break;
#endif

        CHECK_SOLO_PARAM(0);
        CHECK_SOLO_PARAM(1);
        CHECK_SOLO_PARAM(2);
        CHECK_SOLO_PARAM(3);

        CHECK_MUTE_PARAM(0);
        CHECK_MUTE_PARAM(1);
        CHECK_MUTE_PARAM(2);
        CHECK_MUTE_PARAM(3);
        
        case kOutGain:
        {
            BL_FLOAT gain = GetParam(kOutGain)->DBToAmp();
            mOutGain = gain;
      
            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(mOutGain);
        }
        break;

#if !USE_SIMPLIFIED_GUI
        
#if !APP_API

#if DEBUG_BRIGHTNESS_CONTRAST
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();

            if (mPredictProcessor != NULL)
            {
                BLSpectrogram4 *spectro = mPredictProcessor->GetSpectrogram();
                spectro->SetRange(mRange);
            }

            if (mSpectrogramDisplay != NULL)
            {
                mSpectrogramDisplay->UpdateSpectrogram(false);
                mSpectrogramDisplay->UpdateColormap(true);
            }
        }
        break;
            
        case kContrast:
        {
            mContrast = GetParam(paramIdx)->Value();

            if (mPredictProcessor != NULL)
            {
                BLSpectrogram4 *spectro = mPredictProcessor->GetSpectrogram();
                spectro->SetContrast(mContrast);
            }

            if (mSpectrogramDisplay != NULL)
            {
                mSpectrogramDisplay->UpdateSpectrogram(false);
                mSpectrogramDisplay->UpdateColormap(true);
            }
        }
        break;
#endif

#endif

#if !USE_SIMPLIFIED_GUI
        case kHardSolo:
        {
            int value = GetParam(paramIdx)->Int();

            mHardSolo = value;

            UpdateSoloMute();

            CheckRecomputeSpectrogram();
        }
        break;
#endif

#endif
        case kModel:
        {
            int modelNum = GetParam(paramIdx)->Int();
            
            SetModelNum(modelNum);
        }
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Rebalance::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();

    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // Test, in order to update the time axis only at startup,
    // not when closed and re-opened
    if (mMustUpdateTimeAxis)
    {
        UpdateTimeAxis();
        mMustUpdateTimeAxis = false;
    }

#if 1 // Specific to Rebalance
    if (mIsPlaying)
    {
        if (mPredictProcessor != NULL)
            mPredictProcessor->RecomputeSpectrogram(mNeedRecomputeMasks);
    }
#endif
    
    // Reset recomputation time and flag
    // See OnIdle();
    mPrevSpectroUpdateTime = BLUtils::GetTimeMillis();
    mNeedRecomputeSpectrogram = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
Rebalance::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;

    if (mTimeAxis != NULL)
        mTimeAxis->SetGraph(NULL);
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    mSpectrogramDisplay = NULL;
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetSpectrogramDisplay(NULL);
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

bool
Rebalance::OpenMixFile(const char *fileName)
{
#ifndef WIN32
#if APP_API
    
    // NOTE: Would crash when opening very large files
    // under the debugger, and using Scheme -> Guard Edge/ Guard Malloc
   
    ENTER_PARAMS_MUTEX;
    
#if FIX_OPEN_SOURCE_FILE
    mCurrentMixChannels[0].Resize(0);
    mCurrentMixChannels[1].Resize(0);
#endif
    
    vector<WDL_TypedBuf<BL_FLOAT> > mixChannels;
    
    // Open the audio file
    BL_FLOAT sampleRate = GetSampleRate();
    AudioFile *audioFile = AudioFile::Load(fileName, &mixChannels);
    if (audioFile == NULL)
    {
        LEAVE_PARAMS_MUTEX;
        
        return false;
    }
                            
    audioFile->Resample(sampleRate);
    
    if (!mixChannels.empty())
    {
#if DUMP_REAL_MONO
        BLUtils::StereoToMono(&mixChannels);     
#endif
        
#if !FIX_OPEN_SOURCE_FILE
        mCurrentMixChannels[0].Add(mixChannels[0].Get(), mixChannels[0].GetSize());
        if (mixChannels.size() > 1)
            mCurrentMixChannels[1].Add(mixChannels[1].Get(),
                                       mixChannels[1].GetSize());
        else
            mCurrentMixChannels[1] = mCurrentMixChannels[0];
#else
        mCurrentMixChannels[0] = mixChannels[0];
        if (mixChannels.size() > 1)
            mCurrentMixChannels[1] = mixChannels[1];
        else
            mCurrentMixChannels[1] = mixChannels[0];
#endif
    }
    
    delete audioFile;

#endif //APP_API
#endif // WIN32

    LEAVE_PARAMS_MUTEX;
    
    return true;
}

bool
Rebalance::OpenSourceFile(const char *fileName, WDL_TypedBuf<BL_FLOAT> *result)
{
#ifndef WIN32
#if APP_API
    
    // NOTE: Would crash when opening very large files
    // under the debugger, and using Scheme -> Guard Edge/ Guard Malloc
    
    ENTER_PARAMS_MUTEX;
  
#if FIX_OPEN_SOURCE_FILE
    result->Resize(0);
#endif
    
    vector<WDL_TypedBuf<BL_FLOAT> > sourceChannels;
    
    // Open the audio file
    BL_FLOAT sampleRate = GetSampleRate();
    AudioFile *audioFile = AudioFile::Load(fileName, &sourceChannels);
    if (audioFile == NULL)
    {
        LEAVE_PARAMS_MUTEX;
        
        return false;
    }
    
    audioFile->Resample(sampleRate);
    
    if (!sourceChannels.empty())
    {
#if DUMP_REAL_MONO
        BLUtils::StereoToMono(&sourceChannels);     
#endif
        
#if !FIX_OPEN_SOURCE_FILE
        result->Add(sourceChannels[0].Get(), sourceChannels[0].GetSize());
#else
        *result = sourceChannels[0];
#endif
    }
    
    delete audioFile;
    
#endif // APP_API
#endif //WIN32

    LEAVE_PARAMS_MUTEX;
    
    return true;
}

long
Rebalance::Generate()
{
    // NOTE: Take only the left channel
    //
    if (mCurrentMixChannels[0].GetSize() != mCurrentSourceChannels0[0].GetSize())
        return 0;
    
    long pos = 0;
    while(pos < mCurrentMixChannels[0].GetSize() - REBALANCE_BUFFER_SIZE)
    {
        // Stems
        //
        for (int i = 0; i < NUM_STEM_SOURCES; i++)
        {
            vector<WDL_TypedBuf<BL_FLOAT> > in;
            in.resize(1);
            in[0].Add(&mCurrentSourceChannels0[i].Get()[pos], REBALANCE_BUFFER_SIZE);
            
            // Process
            if (mDumpProcessors[i] != NULL)
                mDumpProcessors[i]->Process(&in);
        }
        
        // Mix
        //
        vector<WDL_TypedBuf<BL_FLOAT> > in;
        in.resize(2);
        in[0].Add(&mCurrentMixChannels[0].Get()[pos], REBALANCE_BUFFER_SIZE);
        in[1].Add(&mCurrentMixChannels[1].Get()[pos], REBALANCE_BUFFER_SIZE);
        
        // Process
        if (mDumpProcessors[NUM_STEM_SOURCES] != NULL)
            mDumpProcessors[NUM_STEM_SOURCES]->Process(&in);
        
        ComputeAndDump();

        pos += REBALANCE_BUFFER_SIZE;
    }
    
    return pos;
}

void
Rebalance::ConsumeBuffers(long numToConsume)
{
    for (int k = 0; k < 2; k++)
    {
        BLUtils::ConsumeLeft(&mCurrentMixChannels[k], (int)numToConsume);
    }
    
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
    {
        BLUtils::ConsumeLeft(&mCurrentSourceChannels0[i], (int)numToConsume);
    }
}

void
Rebalance::GenerateMSD100()
{
    // Get the tracks list
    vector<string> tracks;
    
    std::ifstream input(TRACK_LIST_FILE);
    std::string line;
    
    while(std::getline(input, line))
    {
        tracks.push_back(line);
    }

#if !DUMP_SAVE_MEM_LINUX
    for (int i = 0; i < tracks.size(); i++)
#else
    char *trackNum = getenv("BL_TRACK_NUM");
    if (trackNum == NULL)
        return;
    int i = atoi(trackNum);
    if ((i < 0) || (i >= tracks.size()))
        return;
#endif
    {
        const char *track = tracks[i].c_str();
        
        char message0[2048];
        sprintf(message0, "dump: %d/%d", i + 1, (int)tracks.size());
        BLDebug::AppendMessage("dump.txt", message0);
        
        // Open the mix file
        char mixPath[1024];
        sprintf(mixPath, "%s/%s/mixture.wav", MIXTURES_PATH, track);
        
        OpenMixFile(mixPath);
        
        // Open the vocal source path
        char sourcePath0[1024];
        sprintf(sourcePath0, "%s/%s/%s", SOURCE_PATH, track, VOCALS_FILE_NAME);
        OpenSourceFile(sourcePath0, &mCurrentSourceChannels0[0]);
        
        // Open the bass source path
        char sourcePath1[1024];
        sprintf(sourcePath1, "%s/%s/%s", SOURCE_PATH, track, BASS_FILE_NAME);
        OpenSourceFile(sourcePath1, &mCurrentSourceChannels0[1]);
        
        // Open the vocal source path
        char sourcePath2[1024];
        sprintf(sourcePath2, "%s/%s/%s", SOURCE_PATH, track, DRUMS_FILE_NAME);
        OpenSourceFile(sourcePath2, &mCurrentSourceChannels0[2]);
        
        // Open the vocal source path
        char sourcePath3[1024];
        sprintf(sourcePath3, "%s/%s/%s", SOURCE_PATH, track, OTHER_FILE_NAME);
        OpenSourceFile(sourcePath3, &mCurrentSourceChannels0[3]);
        
        // Generate
        Generate();
    
        BLDebug::AppendMessage("dump.txt", "ok\n");
    }

#if DUMP_SAVE_MEM_LINUX
    // Relaunch for each track
    exit(0);
#endif
}

void
Rebalance::ComputeAndDump()
{
    if (mDumpProcessors[0] == NULL)
        return;
    
    // Avoid opening and closing files each time
    void *cookies[6];
    
    cookies[0] = BLUtilsFile::AppendValuesFileBinFloatInit(MIX_SAVE_FILE);
    cookies[1] = BLUtilsFile::AppendValuesFileBinFloatInit(MIX_STEREO_SAVE_FILE);
    cookies[2] = BLUtilsFile::AppendValuesFileBinFloatInit(SOURCE_SAVE_FILE_VOCAL);
    cookies[3] = BLUtilsFile::AppendValuesFileBinFloatInit(SOURCE_SAVE_FILE_BASS);
    cookies[4] = BLUtilsFile::AppendValuesFileBinFloatInit(SOURCE_SAVE_FILE_DRUMS);
    cookies[5] = BLUtilsFile::AppendValuesFileBinFloatInit(SOURCE_SAVE_FILE_OTHER);
    
    //
    while(mDumpProcessors[0]->HasEnoughDumpData())
    {
        // Stems
        WDL_TypedBuf<BL_FLOAT>
        sourceData[NUM_STEM_SOURCES][REBALANCE_NUM_SPECTRO_COLS];
        for (int k = 0; k < NUM_STEM_SOURCES; k++)
        {
            mDumpProcessors[k]->GetSpectroData(sourceData[k]);
        }
       
        // Mix spectro
        WDL_TypedBuf<BL_FLOAT> mixSpectroData[REBALANCE_NUM_SPECTRO_COLS];
        mDumpProcessors[NUM_STEM_SOURCES]->GetSpectroData(mixSpectroData);
        
        // Mix stereo width
        WDL_TypedBuf<BL_FLOAT> mixStereoData[REBALANCE_NUM_SPECTRO_COLS];
        mDumpProcessors[NUM_STEM_SOURCES]->GetStereoData(mixStereoData);
        
        bool mustDump = true;
#if SKIP_SOURCE_SILENCE
        if (DataIsSilence(mixSpectroData))
            mustDump = false;
#endif
        
#if USE_DB_MAGNS
        MagnsToDbNorm(mixData);

        for (int k = 0; k < NUM_STEM_SOURCES; k++)
            MagnsToDbNorm(sourceData[k]);
#endif
        
        NormalizeData(mixSpectroData, sourceData);
        
        // TODO: normalize stereo data here?
        
        if (mustDump)
        {
            // Dump mix (several columns)
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[0], mixSpectroData[j]);

            // Mix stereo width
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[1], mixStereoData[j]);
            
            // Dump sources (all the column)
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[2], sourceData[0][j]);
        
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[3], sourceData[1][j]);
        
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[4], sourceData[2][j]);
        
            for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
                BLUtilsFile::AppendValuesFileBinFloat(cookies[5], sourceData[3][j]);
        }
    }
    
    for (int i = 0; i < 6; i++)
    {
        BLUtilsFile::AppendValuesFileBinFloatShutdown(cookies[i]);
    }
}

void
Rebalance::NormalizeData(WDL_TypedBuf<BL_FLOAT> mixData[REBALANCE_NUM_SPECTRO_COLS],
                         WDL_TypedBuf<BL_FLOAT>
                         sourceData[NUM_STEM_SOURCES][REBALANCE_NUM_SPECTRO_COLS])
{
#define INF 1e15
    
    // First, normalize the mix data
    BL_FLOAT mixMin = INF;
    BL_FLOAT mixMax = -INF;
    for (int i = 0; i < REBALANCE_NUM_SPECTRO_COLS; i++)
    {
        for (int j = 0; j < mixData[i].GetSize(); j++)
        {
            BL_FLOAT val = mixData[i].Get()[j];
            
            if (val > mixMax)
                mixMax = val;
            
            if (val < mixMin)
                mixMin = val;
                
        }
    }
    
    if (fabs(mixMax - mixMin) > BL_EPS)
    {
        BL_FLOAT mixInv = 1.0/(mixMax - mixMin);
        
        for (int i = 0; i < REBALANCE_NUM_SPECTRO_COLS; i++)
        {
            for (int j = 0; j < mixData[i].GetSize(); j++)
            {
                BL_FLOAT val = mixData[i].Get()[j];
            
                val = (val - mixMin)*mixInv;
            }
        }
    }
    
    //TODO: will have to revert this process after prediction
    
    // Apply mix normalization to the sources
    if (fabs(mixMax - mixMin) > BL_EPS)
    {
        BL_FLOAT mixInv = 1.0/(mixMax - mixMin);
        
        for (int i = 0; i < REBALANCE_NUM_SPECTRO_COLS; i++)
        {
            for (int j = 0; j < mixData[i].GetSize(); j++)
            {
                for (int k = 0; k < NUM_STEM_SOURCES; k++)
                {
                    BL_FLOAT val = sourceData[k][i].Get()[j];
                    
                    val = (val - mixMin)*mixInv;
                    
                    sourceData[k][i].Get()[j] = val;
                }
            }
        }
    }
}

void
Rebalance::UpdateLatency()
{
    if (mPredictProcessor == NULL)
        return;
    
    int latency = mPredictProcessor->GetLatency();

#if USE_RESAMPLER
    if (mUseResampler)
    {
        BL_FLOAT sampleRate = GetSampleRate();
        CheckSampleRate(&sampleRate);
            
        BL_FLOAT coeff = GetSampleRate()/sampleRate;
        latency *= coeff;
    }   
#endif
    
    SetLatency(latency);
}
 
bool
Rebalance::
DataIsSilence(const WDL_TypedBuf<BL_FLOAT> data[REBALANCE_NUM_SPECTRO_COLS])
{
    BL_FLOAT maxValue = 0.0;
    for (int i = 0; i < REBALANCE_NUM_SPECTRO_COLS; i++)
    {
        BL_FLOAT m = BLUtils::ComputeMax(data[i]);
        if (m > maxValue)
            maxValue = m;
    }
    
    BL_FLOAT thrsAmp = DBToAmp(SKIP_SOURCE_SILENCE_THRS_DB);
    if (maxValue < thrsAmp)
        return true;
    
    return false;
}

void
Rebalance::MagnsToDbNorm(WDL_TypedBuf<BL_FLOAT> magns[REBALANCE_NUM_SPECTRO_COLS])
{
    for (int j = 0; j < REBALANCE_NUM_SPECTRO_COLS; j++)
    {
        for (int i = 0; i < magns[j].GetSize(); i++)
        {
            BL_FLOAT m = magns[j].Get()[i];
            
            m = log2(1.0 + m);
            
            magns[j].Get()[i] = m;
        }
    }
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Rebalance::ApplyParams()
{
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetVocal(mVocal);
        
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetBass(mBass);
    
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetDrums(mDrums);
        
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetOther(mOther);

    UpdateSoloMute();
        
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetMasksContrast(mMasksContrast);
        
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetVocalSensitivity(mVocalSensitivity);
        
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetBassSensitivity(mBassSensitivity);
    
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetDrumsSensitivity(mDrumsSensitivity);
    
    if (mPredictProcessor != NULL)
        mPredictProcessor->SetOtherSensitivity(mOtherSensitivity);

#if DEBUG_BRIGHTNESS_CONTRAST
    if (mPredictProcessor != NULL)
    {
        BLSpectrogram4 *spectro = mPredictProcessor->GetSpectrogram();
        spectro->SetRange(mRange);
        spectro->SetContrast(mContrast);
    }

    if (mSpectrogramDisplay != NULL)
    {
        mSpectrogramDisplay->UpdateSpectrogram(false);
        mSpectrogramDisplay->UpdateColormap(true);
    }
#endif

    SetModelNum(mModelNum);
}

// NOTE: OnIdle() is called from the GUI thread
void
Rebalance::OnIdle()
{
    // If APP_API and DBG_PERF_APP, we want the time axis to scroll
    // (like if we monitored)
    bool fakeMonitorEnabled = false;
#if APP_API && DBG_PERF_APP
    fakeMonitorEnabled = true;
#endif

#if APP_API
    if (!fakeMonitorEnabled)
        return;
#endif

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
        if (mIsPlaying || fakeMonitorEnabled)
        {
            if (mMustUpdateTimeAxis)
            {
                //ENTER_PARAMS_MUTEX;
                
                UpdateTimeAxis();
                
                //LEAVE_PARAMS_MUTEX;
                
                mMustUpdateTimeAxis = false;
            }
        }

        CheckRecomputeSpectrogramIdle();
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Rebalance::CreateGraphAxes()
{
    bool firstTimeCreate = (mHAxis == NULL);
    
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mTimeAxis = new GraphTimeAxis6(false);
        mTimeAxis->SetTransport(mTransport);
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2(false);
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);

    if (firstTimeCreate)
    {
        BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
        CheckSampleRate(&sampleRate);
#endif
    
        int graphWidth;
        int graphHeight;
        mGraph->GetSize(&graphWidth, &graphHeight);
    
        BL_FLOAT offsetY = 0.0;
        mTimeAxis->Init(mGraph, mHAxis, mGUIHelper,
                        REBALANCE_BUFFER_SIZE,
                        1.0,
                        //1.0,
                        MAX_NUM_TIME_AXIS_LABELS,
                        offsetY);
    
        mFreqAxis->Init(mVAxis, mGUIHelper, false,
                        REBALANCE_BUFFER_SIZE, sampleRate, graphWidth);
        mFreqAxis->Reset(REBALANCE_BUFFER_SIZE, sampleRate);
    
        BL_FLOAT freqAxisBounds[2] = { 0.0, 1.0 };
        mFreqAxis->SetBounds(freqAxisBounds);
    }
    else
    {
        mTimeAxis->SetGraph(mGraph);
    }
}

// TODO (maybe...)
void
Rebalance::SetScrollSpeed()
{
    // Scroll speed
    if (mPredictProcessor != NULL)
    {
        int speedMod = 1;
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

void
Rebalance::SetColorMap()
{
    if (mPredictProcessor == NULL)
        return;

    BLSpectrogram4 *spec = mPredictProcessor->GetSpectrogram();
    if (spec != NULL)
    {
        spec->SetColorMap(ColorMapFactory::COLORMAP_PURPLE);
        //spec->SetColorMap(ColorMapFactory::COLORMAP_BLUE);

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
Rebalance::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    // Axis
    BLSpectrogram4 *spectro = mPredictProcessor->GetSpectrogram();
    if (spectro != NULL)
    {
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
        
        BL_FLOAT timeDuration =
            GraphTimeAxis6::ComputeTimeDuration(numBuffers,
                                                REBALANCE_BUFFER_SIZE,
                                                REBALANCE_OVERLAPPING,
                                                sampleRate);
 
        if (mTimeAxis != NULL)
            mTimeAxis->Reset(REBALANCE_BUFFER_SIZE,
                             timeDuration, MAX_NUM_TIME_AXIS_LABELS);
    }
}

void
Rebalance::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mPredictProcessor == NULL))
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    mSpectrogram = mPredictProcessor->GetSpectrogram();

    if (mSpectrogram != NULL)
    {
        mSpectrogram->SetDisplayPhasesX(false);
        mSpectrogram->SetDisplayPhasesY(false);
        mSpectrogram->SetDisplayMagns(true);
        mSpectrogram->SetYScale(Scale::MEL);
        mSpectrogram->SetDisplayDPhases(false);

        // As it should be!
        mSpectrogram->SetValueScale(Scale::DB, PROCESS_SIGNAL_MIN_DB, 0.0);
            
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
            
                mSpectrogramDisplay->SetFftParams(REBALANCE_BUFFER_SIZE,
                                                  REBALANCE_OVERLAPPING,
                                                  sampleRate);
            }
            
            mPredictProcessor->SetSpectrogramDisplay(mSpectrogramDisplay);
            
            mGraph->AddCustomDrawer(mSpectrogramDisplay);
        }
    }
}

void
Rebalance::UpdateSoloMute()
{
    bool needMutes[4] { false, false, false, false };
    
    bool anySolo = false;
    for (int i = 0; i < 4; i++)
    {
        if (mSolos[i])
        {
            anySolo = true;
            break;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (anySolo && !mSolos[i])
            needMutes[i] = true;

        if (mMutes[i] && !mSolos[i])
            needMutes[i] = true;
    }

    if (mPredictProcessor != NULL)
    {
        BL_FLOAT soloValue = 0.0;
        if (!mHardSolo)
            // Soft solo
            soloValue = ComputeMixParam(SOFT_SOLO_VALUE);
        
        for (int i = 0; i < 4; i++)
        {    
            switch (i)
            {
                case 0:
                {
                    if (needMutes[i])
                        mPredictProcessor->SetVocal(soloValue);
                    else
                        mPredictProcessor->SetVocal(mVocal);
                }
                break;
                        
                case 1:
                {
                    if (needMutes[i])
                        mPredictProcessor->SetBass(soloValue);
                    else
                        mPredictProcessor->SetBass(mBass);
                }
                break;
                        
                case 2:
                {
                    if (needMutes[i])
                        mPredictProcessor->SetDrums(soloValue);
                    else
                        mPredictProcessor->SetDrums(mDrums);
                }
                break;
                        
                case 3:
                {
                    if (needMutes[i])
                        mPredictProcessor->SetOther(soloValue);
                    else
                        mPredictProcessor->SetOther(mOther);
                }
                break;
                
                default:
                    break;
            }
        }
    }
}

#if PROCESS_SIGNAL_DB
BL_FLOAT
Rebalance::ComputeMixParam(BL_FLOAT valuePercent)
{
    // Later, it is processed in dB
    // Must not use a too high value
#define MAX_VAL 1.1
    
    BL_FLOAT value = valuePercent*0.01;

    if (value > 1.0)
    {
        // Use a max value, far before 200%,
        // to avoid increasing enourmously the gain
        // NOTE: before, turning just a little made the host saturate immediatly
        BL_FLOAT t = (value - 1.0);
        value = 1.0 + t*(MAX_VAL - 1.0);
    }
    else
    {
        // Use param shape, to have good continuity when reducing the mix %
        value = BLUtils::ApplyParamShape(value, 10.0);
    }
    
    return value;
}
#else
BL_FLOAT
Rebalance::ComputeMixParam(BL_FLOAT valuePercent)
{
    BL_FLOAT value = valuePercent*0.01;
    
    return value;
}
#endif

void
Rebalance::CheckRecomputeSpectrogram(bool recomputeMasks)
{
    bool isPlaying = IsTransportPlaying();
    if (!isPlaying)
    {
        mNeedRecomputeSpectrogram = true;
        mNeedRecomputeMasks = recomputeMasks;
    }
}

// Recompute the spectrogram only after a delay
// Otherwise, when turning a knob, the whole spectrogram
// whould have been recomputed hundreds of times in sort time delay
void
Rebalance::CheckRecomputeSpectrogramIdle()
{
    if (!mNeedRecomputeSpectrogram)
        return;
    
    long int now = BLUtils::GetTimeMillis();
    long int delay = now - mPrevSpectroUpdateTime;
    
    if (delay >= UPDATE_SPECTRO_DELAY)
    {
        if (mPredictProcessor != NULL)
            mPredictProcessor->RecomputeSpectrogram(mNeedRecomputeMasks);
        
        mPrevSpectroUpdateTime = now;
        mNeedRecomputeSpectrogram = false;
    }
}

void
Rebalance::SetModelNum(int modelNum)
{
    // Default value ?
    if (modelNum == -1)
        modelNum = MODEL_NUM_FASTER;
    
    if (mModelNum != modelNum)
    {
        // Model changed
        // We will need to set everything
        //
        // NOTE: here, at startup, mSpectrogramDisplay can be NULL
        if (mPredictProcessor != NULL)
        {
            if (modelNum == 0)
            {
                mPredictProcessor->SetModelNum(0);
                mPredictProcessor->SetPredictModuloNum(3);
            }
            else if (modelNum == 1)
            {
                mPredictProcessor->SetModelNum(1);
                mPredictProcessor->SetPredictModuloNum(3);
            }
            else if (modelNum == 2)
            {
                mPredictProcessor->SetModelNum(1);
                mPredictProcessor->SetPredictModuloNum(0);
            }
        }
    
        CheckRecomputeSpectrogram(true);
    }
    else
    {
        CheckRecomputeSpectrogram(false);
    }
    
    
    // In any case, update mSpectrogramDisplay, which could have been NULL
    // last time model changed
    if (modelNum == MODEL_NUM_FASTER)
    {
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSmoothScrollDisabled(false);
        
        if (mTransport != NULL)
            mTransport->SetUseLegacyLock(false);
    }
    else if (modelNum == MODEL_NUM_BETTER)
    {
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSmoothScrollDisabled(false);

        if (mTransport != NULL)
            mTransport->SetUseLegacyLock(false);
    }
    else if (modelNum == MODEL_NUM_OFFLINE)
    {    
        // Disable smooth scroll. The computation is so slow that
        // smooth scrolling would make strange results
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSmoothScrollDisabled(true);

        // Better to disable legacy lock when playing with offline model
        // => Time axis labels are a bit better aigned with spectrogram
        if (mTransport != NULL)
            mTransport->SetUseLegacyLock(true);
    }
    
    mModelNum = modelNum;
}

#if USE_RESAMPLER
// If sample rate is too high, force to 44100Hz or 48000Hz and enable resampling
void
Rebalance::CheckSampleRate(BL_FLOAT *ioSampleRate)
{
    mUseResampler = false;

    // For 48000Hz, continue to not resample
    // => the result is still ok, and if we tried to resample,
    // that madwould make clicks
    if (*ioSampleRate > REF_SAMLERATE1)
    {
        mUseResampler = true;

        // Choose the best mutliple 44100Hz, or 48000Hz
        // Otherwise we would have fft obj feeding problems, and clicks 
        if ((int)(*ioSampleRate) % (int)REF_SAMLERATE0 == 0)
            *ioSampleRate = REF_SAMLERATE0;
        else
            *ioSampleRate = REF_SAMLERATE1;
    }
}
#endif
