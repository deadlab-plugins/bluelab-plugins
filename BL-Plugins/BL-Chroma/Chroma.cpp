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

#include <ChromaFftObj2.h>
#include <SpectrogramDisplayScroll4.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <GraphTimeAxis6.h>
#include <GraphAxis2.h>

#include <Scale.h>

#include <PlugBypassDetector.h>
#include <BLTransport.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Chroma.h"

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
#define PLUG_HEIGHT_MEDIUM 620

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 702

// Take care of Protools min width (454)
// And take care of vertical black line on the right of the spectrogram
// with some width values
#define PLUG_WIDTH_PORTRAIT 702 //618 //460
#define PLUG_HEIGHT_PORTRAIT 702

// Since the GUI is very full, reduce the offset after
// the radiobuttons title text
#define RADIOBUTTONS_TITLE_OFFSET -5

//#define Y_LOG_SCALE_FACTOR 3.5

// Display axis lines
#define USE_DISPLAY_LINES 1

// More accurate y when displaying lines
#define FIX_ACCURACY 1

// Optimization
#define OPTIM_SKIP_IFFT 1

// Was for Petr / Linux / Bitwig Studio
//#define WATERMARK_MESSAGE "Alpha version - Not For Resale"

#define MAX_NUM_TIME_AXIS_LABELS 10

#define USE_DROP_DOWN_MENU 1

#if 0
Petr Certik <petr@certik.cz> (contact@bluelab-plugs.com)
Niko: sent alpha linux version => everything is ok!
Petr: proposed to give UI feedback when new desing will be ready
Niko: ok! will send new version with new design!
    
"thanks for the quick reply. Great news! I would love an alpha version to test out. I'm using bitwig studio on x86_64 (arch linux distribution, but that shouldn't matter). Bitwig sadly doesn't directly support LV2 yet, but VSTs run just fine."


TODO: re-teste with high sample rates (we use resampler!)

TODO: add 4th button for gui size retina

PROBLEM: StudioOne Sierra crackles
=> set buffer size to 1024 to fix

NOTE: FLStudio Mac Sierra: scrolling jitters a bit, and when turning brightness or contrast, it jitters more
=> this is due to default block size that is 4096
=> decreasing block size to 1024 for example solves the problems


NOTE: Ableton, Sierra: OpenGL takes more resources in Ableton => and sometimes it makes audio crackles

NOTE: Sierra, Ableton, AU => resize GUI, this makes a resize animation

IDEA: take care of Nyquist (why 88200Hz is less blue and more yellow ?)

IDEA: detect chords ?

BUG: VST3: crashes Reaper when quitting

WARNING: for GUI resize, the behaviour of IControl::IControl() was modified (IParam)

NOTE: a major modification of the fonts has been done on Windows
For Mac, the Tahoma font was probably not found, then we used the default WDL Mac font (Monaco)
Change on Windows: use the default font too "Verdana", so it looks like the font on Mac
Had to change some font size on Windows.
=> this will affect all the plugins on Windows

NOTE: Modification in IPlugAAX for meta parameters (RolloverButtons)
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipRange = "Brightness - Colormap brightness";
static char *tooltipContrast = "Contrast - Colormap contrast";
static char *tooltipColormap = "Colormap";
static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";
static char *tooltipGUISizePortrait = "GUI Size: Portrait";
static char *tooltipScrollSpeed = "Speed - Scroll Speed";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";
static char *tooltipATune = "A Tuning - Frequency of the reference A note";
static char *tooltipSharpness = "Sharpness - Sharpness of the data";
static char *tooltipDisplayLines = "Lines - Display lines for each note";

enum EParams
{
    kRange = 0,
    kContrast,
    kColorMap,

    kSharpness,
    kDisplayLines,
    
    kScrollSpeed,

    kATune,

    kMonitor,
    
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
    
    kRangeX = 192,
    kRangeY = 434,
    
    kContrastX = 284,
    kContrastY = 434,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsColorMapX = 150,
    kRadioButtonsColorMapY = 429,
    kRadioButtonColorMapVSize = 82,
    kRadioButtonColorMapNumButtons = 5,
#else
    kColorMapX = 79,
    kColorMapY = 439,
    kColorMapWidth = 80,
#endif
    
    // GUI size
    kGUISizeSmallX = 12,
    kGUISizeSmallY = 428,
    
    kGUISizeMediumX = 12,
    kGUISizeMediumY = 451,
    
    kGUISizeBigX = 12,
    kGUISizeBigY = 474,
    
    kGUISizePortraitX = 35,
    kGUISizePortraitY = 428,

    kRadioButtonsScrollSpeedX = 362,
    kRadioButtonsScrollSpeedY = 448,
    kRadioButtonScrollSpeedVSize = 68,
    kRadioButtonScrollSpeedNumButtons = 3,
    
    kCheckboxMonitorX = 618,
    kCheckboxMonitorY = 470,

    kATuneX = 440,
    kATuneY = 434,
    kKnobATuneFrames = 180,
    
    kSharpnessX = 532,
    kSharpnessY = 434,

    kDisplayLinesX = 618,
    kDisplayLinesY = 434
};

//
Chroma::Chroma(const InstanceInfo &info)
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

Chroma::~Chroma()
{
    if (mDispFftObj != NULL)
        delete mDispFftObj;
    
    if (mChromaObj != NULL)
        delete mChromaObj;
    
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
Chroma::MyMakeGraphics()
{
    int newGUIWidth;
    int newGUIHeight;
    GetNewGUISize(mGUISizeIdx, &newGUIWidth, &newGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            newGUIWidth, newGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);

    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this,
                     newGUIWidth, newGUIHeight,
                     fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
Chroma::MyMakeLayout(IGraphics *pGraphics)
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

#if 0 //1
    pGraphics->ShowFPSDisplay(true);
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
Chroma::InitNull()
{
    BLUtilsPlug::PlugInits();
        
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mDispFftObj = NULL;
    mChromaObj = NULL;
    
    mGraph = NULL;
    
    mHAxis = NULL;
    mVAxis = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectroDisplayScrollState = NULL;
    
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
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;

    mMustUpdateSpectrogram = true;
    mMustUpdateTimeAxis = true;

    mIsPlaying = false;
    mWasPlaying = false;
    
    mBypassDetector = NULL;
    mTransport = NULL;

    //mFirstTimeCreate = true;
    mMustSetScrollSpeed = true;

    mScrollSpeedNum = 0;
    mPrevScrollSpeedNum = -1;
    
#if USE_RESAMPLER
    BL_FLOAT sampleRate = GetSampleRate();
    CheckSampleRate(&sampleRate);
    mPrevSampleRate = sampleRate;
    
    // WDL_Resampler::SetMode arguments are bool interp,
    // int filtercnt, bool sinc, int sinc_size, int sinc_interpsize
    // sinc mode will get better results, but will use more cpu
    // todo: explain arguments
    mResampler.SetMode(true, 1, false, 0, 0);
    mResampler.SetFilterParms();
    // set it output driven
    mResampler.SetFeedMode(false);
    // set input and output samplerates
    mResampler.SetRates(REF_SAMLERATE, GetSampleRate());
#endif
}

void
Chroma::InitParams()
{
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    //GetParam(kRange)->InitDouble("Range", defaultRange, -1.0, 1.0, 0.01, "");
    GetParam(kRange)->InitDouble("Brightness", defaultRange, -1.0, 1.0, 0.01, "");
    
    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
    
    // ColorMap num
    int  defaultColorMap = 0;
    mColorMapNum = defaultColorMap;
#if !USE_DROP_DOWN_MENU
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 4);
#else
    GetParam(kColorMap)->InitEnum("ColorMap", 0, 5, "", IParam::kFlagsNone,
                                  "", "Blue", "Green", "Sweet", "Rainbow", "Dawn");
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
        InitInt("PortraitGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
    
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

    // Atune
    BL_FLOAT defaultATune = 440.0;
    mATune = defaultATune;
    GetParam(kATune)->InitDouble("A Tune", defaultATune, 420.0, 460, 0.1, "Hz");
  
    // Sharpness
    BL_FLOAT defaultSharpness = 0.0;
    mSharpness = defaultSharpness;
    GetParam(kSharpness)->InitDouble("Sharpness", defaultSharpness,
                                     0.0, 100, 1.0, "%",
                                     0, "", IParam::ShapePowCurve(2.0));

    mDisplayLines = false;
    
#if USE_DISPLAY_LINES
    //GetParam(kDisplayLines)->InitInt("DisplayLines", 0, 0, 1);
    GetParam(kDisplayLines)->InitEnum("DisplayLines", 0, 2,
                                      "", IParam::kFlagsNone, "",
                                      "Off", "On");
#endif
}

void
Chroma::ApplyParams()
{
    if (mChromaObj != NULL)
    {
        BLSpectrogram4 *spectro = mChromaObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }
    
    if (mChromaObj != NULL)
    {
        BLSpectrogram4 *spectro = mChromaObj->GetSpectrogram();
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

    if (mMonitorEnabled && !mIsPlaying)
        mTransport->SetDAWTransportValueSec(0.0);

    if (mChromaObj != NULL)
    {
        mChromaObj->SetATune(mATune);
    }

    if (mChromaObj != NULL)
    {
        mChromaObj->SetSharpness(mSharpness);
    }

#if USE_DISPLAY_LINES
    UpdateVAxis();
#endif
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Chroma::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
            
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
        mChromaObj = new ChromaFftObj2(BUFFER_SIZE, oversampling,
                                       freqRes, sampleRate);
        dispProcessObjs.push_back(mChromaObj);

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
        
        mChromaObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);
    
    ApplyParams();
    
    UpdateTimeAxis();

    // NOTE: this is not 100% reliable
    // (200ms fails sometimes, increase ?)
    mBypassDetector = new PlugBypassDetector();

    BL_FLOAT realSampleRate = GetSampleRate();
    mTransport = new BLTransport(realSampleRate);
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
Chroma::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;
    
    bool isPlaying = IsTransportPlaying();
    // Here, must the the real sample rate
    BL_FLOAT transportTime = BLUtilsPlug::GetTransportTime(this);

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetBypassed(false);
    
    if (mBypassDetector != NULL)
    {
        mBypassDetector->TouchFromAudioThread();
        mBypassDetector->SetTransportPlaying(isPlaying || mMonitorEnabled);
    }
    
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
            mTransport->SetDAWTransportValueSec(0.0);
        }
    }
    
    mWasPlaying = isPlaying;
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows
    // all without stop
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

    vector<WDL_TypedBuf<BL_FLOAT> > dummy;
    vector<WDL_TypedBuf<BL_FLOAT> > &inMono = mTmpBuf3;
    inMono.resize(1);
    BLUtils::StereoToMono(&inMono[0], in);
            
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
            
        WDL_ResampleSample *resampledAudio=NULL;
        int numSamples = nFrames*REF_SAMLERATE/GetSampleRate();
        int numSamples0 = mResampler.ResamplePrepare(numSamples, 1,
                                                     &resampledAudio);
        for (int i = 0; i < numSamples0; i++)
        {
            if (i >= nFrames)
                break;
     
            resampledAudio[i] = inMono[0].Get()[i];
        }
      
        WDL_TypedBuf<BL_FLOAT> &outSamples = mTmpBuf4;
        outSamples.Resize(numSamples);
        int numResampled = mResampler.ResampleOut(outSamples.Get(),
                                                  nFrames, numSamples, 1);
        
        if (numResampled != numSamples)
        {
            //failed somehow
            memset(outSamples.Get(), 0, numSamples*sizeof(BL_FLOAT));
        }
        
        inMono[0] = outSamples; // TODO: improve memory management here
    }
#endif

    if (isPlaying || mMonitorEnabled)
    {
        vector<WDL_TypedBuf<BL_FLOAT> > dummy2;
        mDispFftObj->Process(inMono, dummy2, &out);
        
        mMustUpdateSpectrogram = true;
        
        if (mUIOpened)
        {
            if (isPlaying && !mMonitorEnabled)
                mTransport->SetDAWTransportValueSec(transportTime);
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
Chroma::CreateControls(IGraphics *pGraphics, int offset)
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
    UpdateVAxis();
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
    const char *colormapRadioLabels[kRadioButtonColorMapNumButtons] =
        { "BLUE", "GREEN", "SWEET", "RAINBOW", "DAWN" };
    
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
        
    const char *scrollSpeedRadioLabels[kRadioButtonScrollSpeedNumButtons] =
        { "x1", "x2", "x4" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsScrollSpeedX,
                                   kRadioButtonsScrollSpeedY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonScrollSpeedNumButtons,
                                   kRadioButtonScrollSpeedVSize,
                                   kScrollSpeed,
                                   false, "SPEED",
                                   EAlign::Near,
                                   EAlign::Near,
                                   scrollSpeedRadioLabels,
                                   tooltipScrollSpeed);

    // A Tune
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kATuneX, kATuneY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kATune,
                              TEXTFIELD_FN,
                              "A TUNE",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipATune);
    
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

#if USE_DISPLAY_LINES
    // Display lines checkbox
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kDisplayLinesX,
                                   kDisplayLinesY + offset,
                                   CHECKBOX_FN, kDisplayLines,
                                   "LINES",
                                   GUIHelper12::SIZE_DEFAULT, true,
                                   tooltipDisplayLines);
#endif

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

#if 0 // Was for Petr / Linux / Bitwig Studio
    int watermarkColor0[4];
    mGUIHelper->GetGraphCurveColorRed(watermarkColor0);
    IColor watermarkColor(watermarkColor0[3], watermarkColor0[0],
                          watermarkColor0[1], watermarkColor0[2]);
    mGUIHelper->CreateWatermarkMessage(pGraphics, WATERMARK_MESSAGE,
                                       &watermarkColor);
#endif
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    //
    mControlsCreated = true;
}

void
Chroma::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
Chroma::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    if (mTransport != NULL)
        mTransport->Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
  
    if (sampleRate != mPrevSampleRate)
    {
        mPrevSampleRate = sampleRate;

        if (mChromaObj != NULL)
            mChromaObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);
    }
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);

    if (mTransport != NULL)
    {
        BL_FLOAT realSampleRate = GetSampleRate();
        mTransport->Reset(realSampleRate);
    }
    
    UpdateTimeAxis();
    
#if USE_RESAMPLER
    mResampler.Reset();
    // set input and output samplerates
    mResampler.SetRates(GetSampleRate(), REF_SAMLERATE);
#endif
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Chroma::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mChromaObj != NULL)
            {
                BLSpectrogram4 *spectro = mChromaObj->GetSpectrogram();
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
            
            if (mChromaObj != NULL)
            {
                BLSpectrogram4 *spectro = mChromaObj->GetSpectrogram();
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

        case kATune:
        {
            BL_FLOAT aTune = GetParam(paramIdx)->Value();
            mATune = aTune;
            
            if (mChromaObj != NULL)
                mChromaObj->SetATune(mATune);
        }
        break;
    
        case kSharpness:
        {
            BL_FLOAT sharpness = GetParam(paramIdx)->Value();
            sharpness /= 100.0;
            mSharpness = sharpness;
            
            if (mChromaObj != NULL)
                mChromaObj->SetSharpness(mSharpness);
        }
        break;

#if USE_DISPLAY_LINES
        case kDisplayLines:
        {
            int value = GetParam(kDisplayLines)->Value();
            mDisplayLines = (value == 1);
	  
            UpdateVAxis();
        }
        break;
#endif

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
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Chroma::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Chroma::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;

    if (mTimeAxis != NULL)
        mTimeAxis->SetGraph(NULL);
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    mSpectrogramDisplay = NULL;
    if (mChromaObj != NULL)
        mChromaObj->SetSpectrogramDisplay(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizePortraitButton = NULL;

    mMonitorControl = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Chroma::SetColorMap(int colorMapNum)
{
    if (mChromaObj != NULL)
    {
        BLSpectrogram4 *spec = mChromaObj->GetSpectrogram();
    
        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[5] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_GREEN,
            ColorMapFactory::COLORMAP_SWEET,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_DAWN_FIXED
        };
        ColorMapFactory::ColorMap colorMapNum0 = colorMapNums[colorMapNum];
    
        spec->SetColorMap(colorMapNum0);

        // FIX: set to false,false to fix
        // BUG: play data, stop, change colormap
        // => there is sometimes a small jumps of the data to the left
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->UpdateSpectrogram(false);
    }
}

void
Chroma::SetScrollSpeed(int scrollSpeedNum)
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
    
    if (mChromaObj != NULL)
    {
        mChromaObj->SetSpeedMod(speedMod);
        
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSpeedMod(speedMod);

        if (mTransport != NULL)
        {
            // Reset only if the spectrogram is moving
            // FIX: GhostViewer: play, stop, then change the spectro speed
            // => there was a jump in the spectrogram
            if (mTransport->IsTransportPlaying())
                mTransport->Reset();
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

    mScrollSpeedNum = scrollSpeedNum;
    mPrevScrollSpeedNum = mScrollSpeedNum;
}

void
Chroma::GetNewGUISize(int guiSizeIdx, int *width, int *height)
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
Chroma::PreResizeGUI(int guiSizeIdx,
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
    
    mMonitorControl = NULL;
    mControlsCreated = false;
    
    mSpectrogramDisplay = NULL;
    mChromaObj->SetSpectrogramDisplay(NULL);

    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
Chroma::GUIResizeParamChange(int guiSizeIdx)
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

void
Chroma::OnIdle()
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
        }
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Chroma::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    // Axis
    BLSpectrogram4 *spectro = mChromaObj->GetSpectrogram();
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
    
    //BL_FLOAT spacingSeconds = 1.0;
    
    // Manage to have always around 10 labels 
    if (mTimeAxis != NULL)
        mTimeAxis->Reset(BUFFER_SIZE, timeDuration,
                         MAX_NUM_TIME_AXIS_LABELS, timeOffsetSec);
}

void
Chroma::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mChromaObj == NULL))
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();

#if USE_RESAMPLER
    CheckSampleRate(&sampleRate);
#endif
    
    mSpectrogram = mChromaObj->GetSpectrogram();
    
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

        if (stateWasNull)
        {
            mSpectrogramDisplay->SetSpectrogram(mSpectrogram, 0.0, 0.0, 1.0, 1.0);

            mSpectrogramDisplay->SetTransport(mTransport);

            mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);
        }
        
        mChromaObj->SetSpectrogramDisplay(mSpectrogramDisplay);
    
        mGraph->AddCustomDrawer(mSpectrogramDisplay);
    }
}

void
Chroma::CreateGraphAxes()
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
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);

    if (firstTimeCreate)
    {
        UpdateVAxis();
        
        mTimeAxis->Init(mGraph, mHAxis, mGUIHelper,
                        BUFFER_SIZE, 1.0, MAX_NUM_TIME_AXIS_LABELS);
    }
    else
    {
        mTimeAxis->SetGraph(mGraph);
    }
}

#if USE_RESAMPLER
// If sample rate is too high, force to 44100 and enable resamling
void
Chroma::CheckSampleRate(BL_FLOAT *ioSampleRate)
{
    mUseResampler = false;
    
    if (*ioSampleRate > MAX_SAMLERATE)
    {
        mUseResampler = true;
        
        *ioSampleRate = REF_SAMLERATE;
    }
}
#endif

void
Chroma::UpdateVAxis()
{
    if (mVAxis == NULL)
        return;
  
    // Swap axis lines color and axis overlay color
    
    int axisColor[4];
    mGUIHelper->GetGraphAxisColor(axisColor);
    
    int axisOverlayColor[4];
    mGUIHelper->GetGraphAxisOverlayColor(axisOverlayColor);
    
    int axisLabelColor[4];
    mGUIHelper->GetGraphAxisLabelColor(axisLabelColor);
    
    int axisLabelOverlayColor[4];
    mGUIHelper->GetGraphAxisLabelOverlayColor(axisLabelOverlayColor);
    
    BL_GUI_FLOAT lineWidth = mGUIHelper->GetGraphAxisLineWidthBold();

    if (!mDisplayLines)
    {
        axisColor[3] = 0;
        axisOverlayColor[3] = 0;
    }

    BL_FLOAT fontSizeCoeff = 2.0;

    if (mGraph == NULL)
        return;
    
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

    // Does not aligne lines depth to screen pixels
    // (because we must be very accurate with lines drawing,
    // when using the plugin for tuning).
    mVAxis->SetAlignToScreenPixels(false);
    
    // Vertical axis: tones
#define NUM_VAXIS_DATA 14
    static char *VAXIS_DATA [NUM_VAXIS_DATA][2] =
    {
        { "0.0", "" },
        { "0.0416", "C" },
        
        { "0.1249", "C#" },
        { "0.2082", "D" },
        { "0.2916", "D#" },
        { "0.3749", "E" },
        { "0.4582", "F" },
        { "0.5416", "F#" },
        { "0.6249", "G" },
        { "0.7082", "G#" },
        { "0.7916", "A" },
        { "0.8749", "A#" },
        
        { "0.9583", "B" },
        { "1.0", "" }
    };

    mVAxis->SetData(VAXIS_DATA, NUM_VAXIS_DATA);
}
