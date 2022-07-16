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

//#include <SASViewerProcess4.h>
#include <SASViewerProcess5.h>

//#include <SASViewerRender4.h>
#include <SASViewerRender5.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <ParamSmoother2.h>

// For defines
#include <PartialTracker6.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "SASViewer.h"

#include "IPlug_include_in_plug_src.h"

#define BUFFER_SIZE 2048
#define OVERSAMPLING 4
#define FREQ_RES 1 // Origin
//#define FREQ_RES 2 // Test
#define FREQ_RES_IMPROV 1
#define WINDOW_VARIABLE_HANNING 0
#define WINDOW_HANNING 0 //1  // Origin
// Partials are better tracked with gaussian ana window
// (just by changing the window)
#define WINDOW_GAUSSIAN 1 //0 New

#define KEEP_SYNTHESIS_ENERGY 0

// GUI Size
#define NUM_GUI_SIZES 3

#define PLUG_WIDTH_MEDIUM 920
#define PLUG_HEIGHT_MEDIUM 640

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 722

#define DEBUG_PARTIALS 0 //1 // Origin 1

#define USE_LEGACY_LOCK 1

// Do not compute in stereo?
#define FORCE_MONO 1

// Set parameters right for debugging perfs by launching app
#define DEBUG_PERFS 0 //1


#if 0
TODO: use fast sine oscillator available in iPlug2 / Oscillator.h

TODO: test method sent y Mickael, to "cut" the partials from harmo+noise spectrum
http://dafx10.iem.at/papers/DerryFitzGerald_DAFx10_P15.pdf
See also: https://dafx2020.mdw.ac.at/proceedings/papers/DAFx2020_paper_40.pdf
And: https://github.com/eloimoliner/TTN_separator?fbclid=IwAR1ndA5E6FpVpdnYh8zQdlsZ2HFrAwF1oexW9G1T97ABLlG10cOF_XcFDUo

SASSynth: keyboard / add Pythagore and Zarlino temperament

IDEA: to track pitch shift accurately and without consuming too much CPU, use a chromagram, and do the tracking on it !

IDEA: When this will work, add a midi keyboard to change the pitch of any sound !!

- gen noise under envelope
- axes + find good scales for freq, amplitude, warping...

REFACT: re-order the parameters in the code

OPTIM: avoid making ifft during partials synthesis (simply add it to noise)

NOTE: noise only is bad ?

TODO:
- checkbox to ear origin partials
- noise checkbox (+ display noise on a grid)

NOTE: check warping (right direction ?)


INTERFACE: two parts, left/right (source / result)

TODO: Estimate(): return a list of frequency sorted by increasing error
- it will be necessary to make a peak detection !

TODO: test on FLStudio Windows 10 and check there are no sound clicks (due to GL graphics)
+ test the same on Studio One / Windows 10

NOTE: 50Hz may be too high for min freq. (bell is 52Hz)
- test with lower values and deejeridoo
- voice to digeridoo: very strong low pitch ??!!

SITE: make a promo short video + a toturorial video (~10mn)

TODO: FreqsToMelSmooth()

IDEA: the SAS + noise morphing plugin => call i X-MORPH (! like Xenomorph...)
IDEA: X-Morph: manage sidechain
IDEA: check the components we want to morph (amp, freq, color, warp, noise)



BUG: on Protools Mac, automations on Angle0, Angle1, and CamFov have no effect on the view

NOTE: instrument mono-voice only ?

TODO: SASFrame => manage sample rate changes (after reset)

NOTE ALGO: managed to render clear sinusoids by
- rectangular synthesis window
- render samples on the first 1/4 of the buffer
- managed phase in partials
- correctly computed phases in sine synthesis
- interpolating between partial frequencies (prev to current), in sample generation loop

IDEA for the SASSynth: make possible to freeze the pitch => so the frequency will be static,
only the color, warping etc will change
#endif

enum EParams
{
    kDisplayMode = 0,
    kSpeed,
    kDensity,
    kScale,
    
    kThreshold,
    kThreshold2,
    
    kSynthMode,

    kSynthEvenPartials, // pair
    kSynthOddPartials, // impair
    
    kOutGain,

    kAmpFactor,
    kFreqFactor,
    kColorFactor,
    kWarpingFactor,
    
    kAngle0,
    kAngle1,
    kCamFov,
    
    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    
    kHarmoNoiseMix,
    
    kTimeSmoothCoeff,
    kTimeSmoothNoiseCoeff,
    
    kShowTrackingLines,

    kNeriDelta,
    
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
    
    kSpeedX = 195,
    kSpeedY = 440,
    kSpeedFrames = 180,
    
    kDensityX = 265,
    kDensityY = 440,
    kDensityFrames = 180,
    
    kScaleX = 335,
    kScaleY = 440,
    kScaleFrames = 180,
    
    kThresholdX = 405,
    kThresholdY = 440,
    kThresholdFrames = 180,

    kThreshold2X = 460, //475,
    kThreshold2Y = 440,
    kThreshold2Frames = 180,
    
    kRadioButtonsDisplayModeX = 150,
    kRadioButtonsDisplayModeY = 451,
    kRadioButtonsDisplayModeVSize = 127,
    kRadioButtonsDisplayModeNumButtons = 8,
    
    kRadioButtonsSynthModeX = 736,
    kRadioButtonsSynthModeY = 440,
    kRadioButtonsSynthModeVSize = 54,
    kRadioButtonsSynthModeNumButtons = 3,
    
    // GUI size
    kGUISizeSmallX = 20,
    kGUISizeSmallY = 423,
    
    kGUISizeMediumX = 20,
    kGUISizeMediumY = 451,
    
    kGUISizeBigX = 20,
    kGUISizeBigY = 479,
    
    //
    kSynthEvenPartialsX = 746,
    kSynthEvenPartialsY = 530,

    kSynthOddPartialsX = 746,
    kSynthOddPartialsY = 580,
    
    //

    // Amp factor
    kAmpFactorX = 526,
    kAmpFactorY = 440,
    kAmpFactorFrames = 180,
    
    // Frequency factor (pitch)
    kFreqFactorX = 586,
    kFreqFactorY = 440,
    kFreqFactorFrames = 180,

    // Color factor
    kColorFactorX = 526,
    kColorFactorY = 540,
    kColorFactorFrames = 180,

    // Warping factor
    kWarpingFactorX = 586,
    kWarpingFactorY = 540,
    kWarpingFactorFrames = 180,
    
    kOutGainX = 666,
    kOutGainY = 440,
    kOutGainFrames = 180,

    kHarmoNoiseMixX = 666,
    kHarmoNoiseMixY = 540,
    kHarmoNoiseMixFrames = 180,
    
    kLogoAnimFrames = 31,
    
    kShowTrackingLinesX = 205,
    kShowTrackingLinesY = 540,
    
    kTimeSmoothCoeffX = 265,
    kTimeSmoothCoeffY = 540,
    kTimeSmoothCoeffFrames = 180,
    
    kTimeSmoothNoiseCoeffX = 475,
    kTimeSmoothNoiseCoeffY = 540,
    kTimeSmoothNoiseCoeffFrames = 180,
    
    kNeriDeltaX = 405,
    kNeriDeltaY = 540,
    kNeriDeltaFrames = 180
};


//
SASViewer::SASViewer(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , ResizeGUIPluginInterface(this)
{
    TRACE;

    //SetLatency(2048);
        
    InitNull();
    InitParams();

    Init(OVERSAMPLING, FREQ_RES);
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

SASViewer::~SASViewer()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            delete mSASViewerProcess[i];
    }
    
    if (mSASViewerRender != NULL)
        delete mSASViewerRender;
    
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
SASViewer::MyMakeGraphics()
{
    int newGUIWidth;
    int newGUIHeight;
    GetNewGUISize(mGUISizeIdx, &newGUIWidth, &newGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            newGUIWidth, newGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);

    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       //PLUG_WIDTH, PLUG_HEIGHT,
                                       newGUIWidth, newGUIHeight,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
SASViewer::MyMakeLayout(IGraphics *pGraphics)
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
    
    CreateControls(pGraphics, mGUIOffsetY);
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
SASViewer::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mFftObj = NULL;
    for (int i = 0; i < 2; i++)
        mSASViewerProcess[i] = NULL;
    
    mGraph = NULL;
    
    mSASViewerRender = NULL;
    
    mOutGainSmoother = NULL;
    
    mDisplayMode = SASViewerProcess5::TRACKING;

    mSynthMode = SASFrameSynth::RAW_PARTIALS;
    mSynthEvenPartials = true;
    mSynthOddPartials = true;
    
    mHarmoNoiseMix = 0.0;
    
    mThreshold = 0.0;
    mThreshold2 = 0.0; //1.0;
    
    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    // Dummy values, to avoid undefine (just in case)
    mGraphWidthSmall = 256;
    mGraphHeightSmall = 256;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
SASViewer::InitParams()
{
    // Speed
    BL_FLOAT defaultSpeed = 50.0;
    mSpeed = defaultSpeed;
    GetParam(kSpeed)->InitDouble("Speed", defaultSpeed, 0.0, 100.0, 0.1, "%");
    
    // Density
    BL_FLOAT defaultDensity = 50.0;
    mDensity = defaultDensity;
    GetParam(kDensity)->InitDouble("Density", defaultDensity, 0.0, 100.0, 0.1, "%");
    
    // Mode
    SASViewerProcess5::DisplayMode defaultDisplayMode =
#if !DEBUG_PERFS
        SASViewerProcess5::AMPLITUDE;
#else
        SASViewerProcess5::TRACKING;
#endif
        
    mDisplayMode = defaultDisplayMode;
    GetParam(kDisplayMode)->InitInt("DisplayMode", defaultDisplayMode, 0, 7);
    
    // Scale
    BL_FLOAT defaultScale = 50.0;
    mScale = defaultScale;
    GetParam(kScale)->InitDouble("Scale", defaultScale, 0.0, 100.0, 0.1, "%");
    
    // Threshold
#if USE_BL_PEAK_DETECTOR
    BL_FLOAT defaultThreshold = -60.0;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->
        InitDouble("Threshold", defaultThreshold, -120.0, 0.0, 0.1, "dB");
#endif

#if USE_BILLAUER_PEAK_DETECTOR
#if !DEBUG_PERFS
    BL_FLOAT defaultThreshold = 0.01; //0.25;
#else
    // Put it to the max
    BL_FLOAT defaultThreshold = 0.0;
#endif
    
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->
        InitDouble("Threshold", defaultThreshold*100., 0.0, 100.0, 0.1, "%");

#if !DEBUG_PERFS
    BL_FLOAT defaultThreshold2 = 0.0; //1.0;
#else
    // Put it to the max
    BL_FLOAT defaultThreshold2 = 1.0;
#endif
    
    mThreshold2 = defaultThreshold2;
    GetParam(kThreshold2)->
        InitDouble("Threshold2", defaultThreshold2*100., 0.0, 100.0, 0.1, "%");
#endif
    
    //
    
    // Amp
    BL_FLOAT defaultAmpFactor = 1.0;
    mAmpFactor = defaultAmpFactor;
    GetParam(kAmpFactor)->
        InitDouble("AmpFactor", defaultAmpFactor, 0.0, 2.0, 0.01, "");

    // Freq
    BL_FLOAT defaultFreqFactor = 1.0;
    mFreqFactor = defaultFreqFactor;
    GetParam(kFreqFactor)->
        InitDouble("FreqFactor", defaultFreqFactor, 0.5, 2.0, 0.01, "");

    // Color
    BL_FLOAT defaultColorFactor = 1.0;
    mColorFactor = defaultColorFactor;
    GetParam(kColorFactor)->
        InitDouble("ColorFactor", defaultColorFactor, 0.0, 2.0, 0.01, "");

    // Warping
    BL_FLOAT defaultWarpingFactor = 1.0;
    mWarpingFactor = defaultWarpingFactor;
    GetParam(kWarpingFactor)->
        InitDouble("WarpingFactor", defaultWarpingFactor, 0.0, 2.0, 0.01, "");
    
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");
    
    // Synth mode
    SASFrameSynth::SynthMode defaultSynthMode = SASFrameSynth::RAW_PARTIALS;
    mSynthMode = defaultSynthMode;
    GetParam(kSynthMode)->InitInt("SynthMode", defaultSynthMode, 0, 2);
    
    // Synth even and odd partials
    bool defaultSynthEvenPartials = true;
    mSynthEvenPartials = defaultSynthEvenPartials;
    GetParam(kSynthEvenPartials)->
        InitInt("SynthEvenPartials", defaultSynthEvenPartials, 0, 1);

    bool defaultSynthOddPartials = true;
    mSynthOddPartials = defaultSynthOddPartials;
    GetParam(kSynthOddPartials)->
        InitInt("SynthOddPartials", defaultSynthOddPartials, 0, 1);
    
    // Mix
    BL_FLOAT defaultHarmoNoiseMix = 0.0;
    mHarmoNoiseMix = defaultHarmoNoiseMix;
    GetParam(kHarmoNoiseMix)->
        InitDouble("HarmoNoiseMix", defaultHarmoNoiseMix, -100.0, 100.0, 0.1, "%");
    
#if 1
    // Do not take the risk of making a non-tested host crash...
    const char *degree = " ";
#endif
    
    // Angle0
    BL_FLOAT defaultAngle0 = 0.0;
    mAngle0 = defaultAngle0;
    GetParam(kAngle0)->InitDouble("Angle0", defaultAngle0,
                                  -MAX_CAM_ANGLE_0, MAX_CAM_ANGLE_0, 0.1, degree);
    
    // Angle1
    BL_FLOAT defaultAngle1 = 15.0;
    mAngle1 = defaultAngle1;
    GetParam(kAngle1)->InitDouble("Angle1", defaultAngle1,
                                  MIN_CAM_ANGLE_1, MAX_CAM_ANGLE_1, 0.1, degree);
    
    // CamFov
    BL_FLOAT defaultCamFov = 45.0;
    mCamFov = defaultCamFov;
    GetParam(kCamFov)->InitDouble("CamFov", defaultCamFov,
                                  MIN_FOV, MAX_FOV, 0.1, degree);
    
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->InitInt("SmallGUI", 0, 0, 1, "", IParam::kFlagMeta);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    GetParam(kGUISizeMedium)->InitInt("MediumGUI", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kGUISizeBig)->InitInt("BigGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    // Display tracking lines
    bool defaultShowTrackingLines = true;
    mShowTrackingLines = defaultShowTrackingLines;
    GetParam(kShowTrackingLines)->InitInt("ShowTrackingLines",
                                          defaultShowTrackingLines, 0, 1);
    
    // Debug
    // NOTE: Makes PartialTacker6/QIFFT fail if > 0
    // If need to smooth, we will have to smooth in complex domain!
    //BL_FLOAT defaultTimeSmoothCoeff = 0.0; //0.5;
    BL_FLOAT defaultTimeSmoothCoeff = 0.5;
    mTimeSmoothCoeff = defaultTimeSmoothCoeff;
    GetParam(kTimeSmoothCoeff)->InitDouble("TimeSmoothCoeff",
                                           defaultTimeSmoothCoeff,
                                           0.0, 1.0, 0.001, "");
    
    // Neri delta (ratio valie/spurious)
    BL_FLOAT defaultNeriDelta = 20.0;
    mNeriDelta = defaultNeriDelta*0.01;
    GetParam(kNeriDelta)->InitDouble("NeriDelta", defaultNeriDelta,
                                     0.0, 100.0, 0.1, "%");

    // Debug
    BL_FLOAT defaultTimeSmoothNoiseCoeff = 0.5;
    mTimeSmoothNoiseCoeff = defaultTimeSmoothNoiseCoeff;
    GetParam(kTimeSmoothNoiseCoeff)->InitDouble("TimeSmoothNoiseCoeff",
                                                defaultTimeSmoothNoiseCoeff,
                                                0.0, 1.0, 0.001, "");
}

void
SASViewer::ApplyParams()
{
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetSpeed(mSpeed);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetDensity(mDensity);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetScale(mScale);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetCamAngle0(mAngle0);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetCamAngle1(mAngle1);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetCamFov(mCamFov);
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
        {
            mSASViewerProcess[i]->SetThreshold(mThreshold);
            mSASViewerProcess[i]->SetThreshold2(mThreshold2);
        }
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
        {
            mSASViewerProcess[i]->SetAmpFactor(mAmpFactor);
            mSASViewerProcess[i]->SetFreqFactor(mFreqFactor);
            mSASViewerProcess[i]->SetColorFactor(mColorFactor);
            mSASViewerProcess[i]->SetWarpingFactor(mWarpingFactor);
        }
    }
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetDisplayMode(mDisplayMode);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetSynthMode(mSynthMode);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
        {
            mSASViewerProcess[i]->SetSynthEvenPartials(mSynthEvenPartials);
            mSASViewerProcess[i]->SetSynthOddPartials(mSynthOddPartials);
        }
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetHarmoNoiseMix(mHarmoNoiseMix);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetShowTrackingLines(mShowTrackingLines);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetTimeSmoothCoeff(mTimeSmoothCoeff);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->SetTimeSmoothNoiseCoeff(mTimeSmoothNoiseCoeff);
    }
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
SASViewer::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
        
    mOutGainSmoother = new ParamSmoother2(sampleRate, 0.0);
    
    if (mFftObj == NULL)
    {
#if !FORCE_MONO
        int numChannels = 2;
#else
        int numChannels = 1;
#endif
        
        int numScInputs = 0;
        
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < numChannels; i++)
        {
            mSASViewerProcess[i] = new SASViewerProcess5(BUFFER_SIZE*FREQ_RES,
                                                         oversampling, freqRes,
                                                         sampleRate);
            
            if (i == 0)
                mSASViewerProcess[i]->SetSASViewerRender(mSASViewerRender);
            processObjs.push_back(mSASViewerProcess[i]);
        }
        
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      BUFFER_SIZE, oversampling, freqRes,
                                      sampleRate);
#if WINDOW_HANNING
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowHanning);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowHanning);
        
#elif WINDOW_VARIABLE_HANNING 
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowVariableHanning);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowVariableHanning);
#elif WINDOW_GAUSSIAN
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowGaussian);
        // Synth window could be set to Hanning later if necessary 
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowGaussian);
#endif
        
        mFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                        KEEP_SYNTHESIS_ENERGY);

#if FREQ_RES_IMPROV
        mFftObj->SetFreqResImprov(true);
#endif
    }
    else
    {
        mFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    CreateSASViewerRender(true);
    ApplyParams();
    
    mIsInitialized = true;
}

void
SASViewer::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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
        
        return;
    }

#if FORCE_MONO
    int numOutChannels = out.size();
    
    // Take the left channel, do not mix L and R to mono
    // (more consistent for SAS)
    in.resize(1);
    out.resize(1);
#endif
    
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    bool isPlaying = IsTransportPlaying();

#if DEBUG_PERFS
    isPlaying = true;
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
    
    if (isPlaying)
    {
        mFftObj->Process(in, scIn, &out);
    }
    
    // Apply output gain
    if (mOutGainSmoother != NULL)
    {
        for (int i = 0; i < nFrames; i++)
        {
            BL_FLOAT gain = mOutGainSmoother->Process();
        
            out[0].Get()[i] *= gain;
        
            if (out.size() > 1)
                out[1].Get()[i] *= gain;
        }
    }

#if FORCE_MONO
    if (numOutChannels == 2)
    {
        out.resize(2);
        out[1] = out[0];
    }
#endif    
        
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    
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
SASViewer::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN /*kGraph*/);
    mGraph->SetUseLegacyLock(USE_LEGACY_LOCK);
        
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetGraph(mGraph);
    
    // GUIResize
    int newGraphWidth = mGraphWidthSmall + mGUIOffsetX;
    int newGraphHeight = mGraphHeightSmall + mGUIOffsetY;
    mGraph->Resize(newGraphWidth, newGraphHeight);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    
#if !DEBUG_PARTIALS
    mGraph->SetClearColor(0, 0, 0, 255);
#else
    mGraph->SetClearColor(255, 255, 255, 255);
    // Cancel the black background image
    IBitmap dummyBitmap;
    mGraph->SetBackgroundImage(pGraphics, dummyBitmap);
#endif
    
    int sepColor[4] = { 24, 24, 24, 255 };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    // Speed
    mGUIHelper->CreateKnob(pGraphics,
                           kSpeedX, kSpeedY + offset,
                           KNOB_SMALL_FN,
                           kSpeedFrames,
                           kSpeed,
                           TEXTFIELD_FN,
                           "SPEED",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Density
    mGUIHelper->CreateKnob(pGraphics,
                           kDensityX, kDensityY + offset,
                           KNOB_SMALL_FN,
                           kDensityFrames,
                           kDensity,
                           TEXTFIELD_FN,
                           "DENSITY",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Mode
    const char *radioLabelsDisplayMode[kRadioButtonsDisplayModeNumButtons] =
        { "DETECTION", "TRACKING", "HARMO", "NOISE",
          "AMPLITUDE", "FREQUENCY", "COLOR", "WARPING" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsDisplayModeX,
                                   kRadioButtonsDisplayModeY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsDisplayModeNumButtons,
                                   kRadioButtonsDisplayModeVSize,
                                   kDisplayMode,
                                   false,
                                   "MODE",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsDisplayMode);
    
    // Scale
    mGUIHelper->CreateKnob(pGraphics,
                           kScaleX, kScaleY + offset,
                           KNOB_SMALL_FN,
                           kScaleFrames,
                           kScale,
                           TEXTFIELD_FN,
                           "SCALE",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Threshold (Billauer delta)
    mGUIHelper->CreateKnob(pGraphics,
                           kThresholdX, kThresholdY + offset,
                           KNOB_SMALL_FN,
                           kThresholdFrames,
                           kThreshold,
                           TEXTFIELD_FN,
                           "THRES.",
                           GUIHelper12::SIZE_DEFAULT);

    // Threshold2 (Billauer peak discrimination)
    mGUIHelper->CreateKnob(pGraphics,
                           kThreshold2X, kThreshold2Y + offset,
                           KNOB_SMALL_FN,
                           kThreshold2Frames,
                           kThreshold2,
                           TEXTFIELD_FN,
                           "THRES2.",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Synth even/odd partials
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kSynthEvenPartialsX,
                                   kSynthEvenPartialsY + offset,
                                   CHECKBOX_FN,
                                   kSynthEvenPartials,
                                   "ST EVEN");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kSynthOddPartialsX,
                                   kSynthOddPartialsY + offset,
                                   CHECKBOX_FN,
                                   kSynthOddPartials,
                                   "ST ODD");
    
    //
    
    // Amp
    mGUIHelper->CreateKnob(pGraphics,
                           kAmpFactorX, kAmpFactorY + offset,
                           KNOB_SMALL_FN,
                           kAmpFactorFrames,
                           kAmpFactor,
                           TEXTFIELD_FN,
                           "AMP X",
                           GUIHelper12::SIZE_DEFAULT);

    // Freq
    mGUIHelper->CreateKnob(pGraphics,
                           kFreqFactorX, kFreqFactorY + offset,
                           KNOB_SMALL_FN,
                           kFreqFactorFrames,
                           kFreqFactor,
                           TEXTFIELD_FN,
                           "FREQ X",
                           GUIHelper12::SIZE_DEFAULT);

    // Color
    mGUIHelper->CreateKnob(pGraphics,
                           kColorFactorX, kColorFactorY + offset,
                           KNOB_SMALL_FN,
                           kColorFactorFrames,
                           kColorFactor,
                           TEXTFIELD_FN,
                           "COL X",
                           GUIHelper12::SIZE_DEFAULT);

    // Warping
    mGUIHelper->CreateKnob(pGraphics,
                           kWarpingFactorX, kWarpingFactorY + offset,
                           KNOB_SMALL_FN,
                           kWarpingFactorFrames,
                           kWarpingFactor,
                           TEXTFIELD_FN,
                           "WARP X",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Out gain
    mGUIHelper->CreateKnob(pGraphics,
                           kOutGainX, kOutGainY + offset,
                           KNOB_SMALL_FN,
                           kOutGainFrames,
                           kOutGain,
                           TEXTFIELD_FN,
                           "OUT GAIN",
                           GUIHelper12::SIZE_DEFAULT);
    
    const char *radioLabelsSynthMode[kRadioButtonsSynthModeNumButtons] =
        { "RAW", "SOURCE", "RESYNTH" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsSynthModeX,
                                   kRadioButtonsSynthModeY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsSynthModeNumButtons,
                                   kRadioButtonsSynthModeVSize,
                                   kSynthMode,
                                   false,
                                   "SYNTH",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsSynthMode);

    // HarmoNoiseMix
    mGUIHelper->CreateKnob(pGraphics,
                           kHarmoNoiseMixX, kHarmoNoiseMixY + offset,
                           KNOB_SMALL_FN,
                           kHarmoNoiseMixFrames,
                           kHarmoNoiseMix,
                           TEXTFIELD_FN,
                           "HARMO/NOISE",
                           GUIHelper12::SIZE_DEFAULT);
    
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeSmallX, kGUISizeSmallY + offset,
                                      BUTTON_RESIZE_SMALL_FN,
                                      kGUISizeSmall,
                                      "", 0);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeMediumX, kGUISizeMediumY + offset,
                                      BUTTON_RESIZE_MEDIUM_FN,
                                      kGUISizeMedium,
                                      "", 1);
    
    mGUISizeBigButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeBigX, kGUISizeBigY + offset,
                                      BUTTON_RESIZE_BIG_FN,
                                      kGUISizeBig,
                                      "", 2);
    
    // ProProcess 1: time smooth magns
    mGUIHelper->CreateKnob(pGraphics,
                           kTimeSmoothCoeffX,
                           kTimeSmoothCoeffY + offset,
                           KNOB_SMALL_FN,
                           kTimeSmoothCoeffFrames,
                           kTimeSmoothCoeff,
                           TEXTFIELD_FN,
                           "TIME SM",
                           GUIHelper12::SIZE_DEFAULT);

    
    // Debug
    mGUIHelper->CreateKnob(pGraphics,
                           kNeriDeltaX, kNeriDeltaY + offset,
                           KNOB_SMALL_FN,
                           kNeriDeltaFrames,
                           kNeriDelta,
                           TEXTFIELD_FN,
                           "DELTA",
                           GUIHelper12::SIZE_DEFAULT);
    
    mGUIHelper->CreateKnob(pGraphics,
                           kTimeSmoothNoiseCoeffX,
                           kTimeSmoothNoiseCoeffY + offset,
                           KNOB_SMALL_FN,
                           kTimeSmoothNoiseCoeffFrames,
                           kTimeSmoothNoiseCoeff,
                           TEXTFIELD_FN,
                           "T SM NOISE",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Show trasking lines flag
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kShowTrackingLinesX,
                                   kShowTrackingLinesY + offset,
                                   CHECKBOX_FN, kShowTrackingLines, "SHOW TRACK");
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    //, GUIHelper12::BOTTOM);
    
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
    
    CreateSASViewerRender(false);
    
    //
    mControlsCreated = true;
}

void
SASViewer::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
SASViewer::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    if (mFftObj != NULL)
        mFftObj->Reset();
    
    for (int i = 0; i < 2; i++)
    {
        if (mSASViewerProcess[i] != NULL)
            mSASViewerProcess[i]->Reset();
    }

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SASViewer::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kSpeed:
        {
            BL_FLOAT speed = GetParam(kSpeed)->Value();
            speed = speed/100.0;
            mSpeed = speed;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetSpeed(mSpeed);
        }
        break;
            
        case kDensity:
        {
            BL_FLOAT density = GetParam(kDensity)->Value();
            density = density/100.0;
            mDensity = density;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetDensity(density);
        }
        break;
            
        case kScale:
        {
            BL_FLOAT scale = GetParam(kScale)->Value();
            scale = scale/100.0;
            mScale = scale;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetScale(scale);
        }
        break;
            
        case kAngle0:
        {
            BL_FLOAT angle = GetParam(kAngle0)->Value();
            mAngle0 = angle;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetCamAngle0(angle);
        }
        break;
            
        case kAngle1:
        {
            BL_FLOAT angle = GetParam(kAngle1)->Value();
            mAngle1 = angle;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetCamAngle1(angle);
        }
        break;
            
        case kCamFov:
        {
            BL_FLOAT angle = GetParam(kCamFov)->Value();
            mCamFov = angle;
            
            if (mSASViewerRender != NULL)
                mSASViewerRender->SetCamFov(angle);
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

        // Billauer delta
        case kThreshold:
        {
            BL_FLOAT threshold = GetParam(kThreshold)->Value();

#if USE_BILLAUER_PEAK_DETECTOR
            threshold *= 0.01;
#endif
            
            mThreshold = threshold;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetThreshold(threshold);
            }
        }
        break;

        // Billauer peak discrimination
        case kThreshold2:
        {
            BL_FLOAT threshold2 = GetParam(kThreshold2)->Value();

            threshold2 *= 0.01;
            
            mThreshold2 = threshold2;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetThreshold2(threshold2);
            }
        }
        break;
        
        //

        // Amp
        case kAmpFactor:
        {
            BL_FLOAT factor = GetParam(kAmpFactor)->Value();
            mAmpFactor = factor;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetAmpFactor(factor);
            }
        }
        break;

        // Amp
        case kFreqFactor:
        {
            BL_FLOAT factor = GetParam(kFreqFactor)->Value();
            mFreqFactor = factor;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetFreqFactor(factor);
            }
        }
        break;

        // Color
        case kColorFactor:
        {
            BL_FLOAT factor = GetParam(kColorFactor)->Value();
            mColorFactor = factor;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetColorFactor(factor);
            }
        }
        break;

        // Warping
        case kWarpingFactor:
        {
            BL_FLOAT factor = GetParam(kWarpingFactor)->Value();
            mWarpingFactor = factor;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetWarpingFactor(factor);
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
            
        case kDisplayMode:
        {
            enum SASViewerProcess5::DisplayMode mode =
                (enum SASViewerProcess5::DisplayMode)GetParam(kDisplayMode)->Int();
            mDisplayMode = mode;
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetDisplayMode(mode);
            }
        }
        break;
            
        case kSynthMode:
        {
            //enum SASFrame3::SynthMode mode =
            //    (enum SASFrame3::SynthMode)GetParam(kSynthMode)->Int();
            enum SASFrameSynth::SynthMode mode =
                (enum SASFrameSynth::SynthMode)GetParam(kSynthMode)->Int();
            mSynthMode = mode;
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetSynthMode(mode);
            }
        }
        break;
            
        case kSynthEvenPartials:
        {
            int value = GetParam(kSynthEvenPartials)->Value();
            bool flag = (value == 1);
            mSynthEvenPartials = flag;
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetSynthEvenPartials(flag);
            }
        }
        break;

        case kSynthOddPartials:
        {
            int value = GetParam(kSynthOddPartials)->Value();
            bool flag = (value == 1);
            mSynthOddPartials = flag;
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetSynthOddPartials(flag);
            }
        }
        break;
        
        case kHarmoNoiseMix:
        {
            BL_FLOAT mix = GetParam(kHarmoNoiseMix)->Value();
            mix /= 100.0;
            
            // Reverse [-1, 1] (to have the noise on the right)
            mix = -mix;
            
            mHarmoNoiseMix = mix;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetHarmoNoiseMix(mix);
            }
        }
        break;
          
        case kShowTrackingLines:
        {
            int value = GetParam(kShowTrackingLines)->Value();
            bool flag = (value == 1);
            mShowTrackingLines = flag;
  
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetShowTrackingLines(flag);
            }
        }
        break;
        
        // 0.5 is a good value
        case kTimeSmoothCoeff:
        {
            BL_FLOAT param = GetParam(kTimeSmoothCoeff)->Value();
            mTimeSmoothCoeff = param;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetTimeSmoothCoeff(mTimeSmoothCoeff);
            }
        }
        break;
            
        // Debug
        // 1.0 is a good value
        case kNeriDelta:
        {
            BL_FLOAT value = GetParam(kNeriDelta)->Value();

            BL_FLOAT delta = value*0.01;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetNeriDelta(delta);
            }
        }
        break;
            
        case kTimeSmoothNoiseCoeff:
        {
            BL_FLOAT param = GetParam(kTimeSmoothNoiseCoeff)->Value();
            mTimeSmoothNoiseCoeff = param;
            
            for (int i = 0; i < 2; i++)
            {
                if (mSASViewerProcess[i] != NULL)
                    mSASViewerProcess[i]->SetTimeSmoothNoiseCoeff(mTimeSmoothNoiseCoeff);
            }
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SASViewer::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    ApplyParams();
    
    LEAVE_PARAMS_MUTEX;
}

void
SASViewer::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetGraph(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
SASViewer::PreResizeGUI(int guiSizeIdx,
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
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetGraph(NULL);
    
    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
SASViewer::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall,
                              kGUISizeMedium,
                              kGUISizeBig };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
    { mGUISizeSmallButton,
      mGUISizeMediumButton,
      mGUISizeBigButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

void
SASViewer::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT },
        { PLUG_WIDTH_MEDIUM, PLUG_HEIGHT_MEDIUM },
        { PLUG_WIDTH_BIG, PLUG_HEIGHT_BIG }
    };

    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
SASViewer::SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1)
{
    BL_FLOAT normAngle0 = (angle0 + MAX_CAM_ANGLE_0)/(MAX_CAM_ANGLE_0*2.0);
    GetParam(kAngle0)->SetNormalized(normAngle0);
    //BLUtilsPlug::TouchPlugParam(this, kAngle0);
    
    BL_FLOAT normAngle1 =
        (angle1 - MIN_CAM_ANGLE_1)/(MAX_CAM_ANGLE_1 - MIN_CAM_ANGLE_1);
    GetParam(kAngle1)->SetNormalized(normAngle1);
    //BLUtilsPlug::TouchPlugParam(this, kAngle1);
}

void
SASViewer::SetCameraFov(BL_FLOAT angle)
{
    BL_FLOAT normAngle = (angle - MIN_FOV)/(MAX_FOV - MIN_FOV);
    GetParam(kCamFov)->SetNormalized(normAngle);
    //BLUtilsPlug::TouchPlugParam(this, kCamFov);
}

void
SASViewer::CreateSASViewerRender(bool createFromInit)
{
    if (!createFromInit && (mSASViewerProcess[0] == NULL))
        return;
    
    if (mSASViewerRender != NULL)
    {
        if (mGraph != NULL)
            mSASViewerRender->SetGraph(mGraph);
        
        return;
    }
    
    BL_FLOAT sampleRate = GetSampleRate();
    mSASViewerRender =
        new SASViewerRender5(this, mGraph, BUFFER_SIZE*FREQ_RES, sampleRate);
    mSASViewerProcess[0]->SetSASViewerRender(mSASViewerRender);
    
#if DEBUG_PARTIALS
    // Show more data
    mSASViewerRender->DBG_SetNumSlices(/*128*/256);
    mSASViewerProcess[0]->DBG_SetDebugPartials(true);
#endif
}
