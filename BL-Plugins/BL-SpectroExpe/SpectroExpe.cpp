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

#include <SpectroExpeFftObj.h>
#include <SpectrogramDisplayScroll3.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <GraphTimeAxis6.h>
#include <GraphFreqAxis2.h>

#include <GraphAxis2.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "SpectroExpe.h"

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
#define PLUG_WIDTH_PORTRAIT 460
#define PLUG_HEIGHT_PORTRAIT 702

//#define SPECTROEXPE_FPS 50

//#define Y_LOG_SCALE_FACTOR 3.5

// Optimization
#define OPTIM_SKIP_IFFT 1

#define MAX_NUM_TIME_AXIS_LABELS 10

#if 0
NOTE: improve masks for Rebalance
=> panogram style: separates well between melodic (vocal, gtr, bass) and drums
=> panogram style + mono->stereo: less good, but interesting also! (separates better than nothing)
=> chroma: does not add anything interesting

=> simple width => gives more clean results than panogram style
(was necessary to adjust brightness/contrast)

TODO: check well brightness/contrast/normalization of each mask

IDEAS: .....
TODO: add a simple pan mode (e.g like stereowide, take the middle of the 2 imag parts)
(or else make magnitudes division)

IDEA: for width, avoid having pixels at 0, when width is narrow => the DNN won t see them
find a solution to "label" the widths (maybe use several channels and rotating hue...)

TODO: test "width" converted in DB in the main class
TODO: maybe find something with DUET Histograms
......

Conclusion: Use simple spectro + stereo width (linear scale factor 1000)
=> gives a good separation of drums / vocal + clean gtr

#endif

enum EParams
{
    kGraph = 0,
    kRange,
    kContrast,
    kColorMap,
    
    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    kGUISizePortrait,
    
    kScrollSpeed,
    kMonitor,
    
    kMode,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,
    
    kGraphX = 0,
    kGraphY = 0,
    
    kRangeX = 205,
    kRangeY = 429,
    kRangeFrames = 180,
    
    kContrastX = 285,
    kContrastY = 429,
    kContrastFrames = 180,
    
    kRadioButtonsColorMapX = 150,
    kRadioButtonsColorMapY = 429,
    kRadioButtonsColorMapVSize = 82,
    kRadioButtonsColorMapNumButtons = 5,
    
    // GUI size
    kGUISizeSmallX = 20,
    kGUISizeSmallY = 421,
    
    kGUISizeMediumX = 20,
    kGUISizeMediumY = 449,
    
    kGUISizeBigX = 20,
    kGUISizeBigY = 477,
    
    kGUISizePortraitX = 48,
    kGUISizePortraitY = 421,

    kRadioButtonsScrollSpeedX = 360,
    kRadioButtonsScrollSpeedY = 429,
    kRadioButtonsScrollSpeedVSize = 50,
    kRadioButtonsScrollSpeedNumButtons = 3,
    
    kCheckboxMonitorX = 418,
    kCheckboxMonitorY = 429,
    
    kRadioButtonsModeX = 460,
    kRadioButtonsModeY = 429,
    kRadioButtonsModeVSize = 130, //100,
    kRadioButtonsModeNumButtons = 8, //6,
    
    kLogoAnimFrames = 31
};

//
SpectroExpe::SpectroExpe(const InstanceInfo &info)
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

SpectroExpe::~SpectroExpe()
{
    if (mDispFftObj != NULL)
        delete mDispFftObj;
    
    if (mSpectroExpeObj != NULL)
        delete mSpectroExpeObj;
    
    if (mTimeAxis != NULL)
        delete mTimeAxis;
    
    if (mFreqAxis != NULL)
        delete mFreqAxis;
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mVAxis != NULL)
        delete mVAxis;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
SpectroExpe::MyMakeGraphics()
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
SpectroExpe::MyMakeLayout(IGraphics *pGraphics)
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
SpectroExpe::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mDispFftObj = NULL;
    mSpectroExpeObj = NULL;
    
    mGraph = NULL;
    
    mHAxis = NULL;
    mVAxis = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    
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
    
    mMode = SpectroExpeFftObj::SPECTROGRAM;
}

void
SpectroExpe::InitParams()
{
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    GetParam(kRange)->InitDouble("Range", defaultRange, -1.0, 1.0, 0.01, "");
    
    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
    
    // ColorMap num
    int  defaultColorMap = 0;
    mColorMapNum = defaultColorMap;
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 4);
    
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->InitInt("SmallGUI", 0, 0, 1, "", IParam::kFlagMeta);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    
    GetParam(kGUISizeMedium)->InitInt("MediumGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kGUISizeBig)->InitInt("BigGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kGUISizePortrait)->InitInt("PortraitGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    int  defaultScrollSpeed = 2;
    mScrollSpeedNum = defaultScrollSpeed;
    GetParam(kScrollSpeed)->InitInt("ScrollSpeed", defaultScrollSpeed, 0, 2);
    
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    
    SpectroExpeFftObj::Mode defaultMode = SpectroExpeFftObj::SPECTROGRAM;
    mMode = defaultMode;
    GetParam(kMode)->InitInt("Mode", defaultMode, 0, /*5*/7);
}

void
SpectroExpe::ApplyParams()
{
    if (mSpectroExpeObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroExpeObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
    }
    
    if (mSpectroExpeObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroExpeObj->GetSpectrogram();
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
    
    SetScrollSpeed(mScrollSpeedNum);
    
    if (mMonitorEnabled)
    {
        if (mTimeAxis != NULL)
            mTimeAxis->UpdateFromTransport(0.0);
    }
    
    SetMode(mMode);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
SpectroExpe::Init(int oversampling, int freqRes)
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
        vector<ProcessObj *> processObjs;
        
        int numChannels = 2; //1;
        int numScInputs = 0;
        
        mDispFftObj = new FftProcessObj16(processObjs,
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

        mSpectroExpeObj = new SpectroExpeFftObj(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        // Must artificially call reset because MultiProcessObjs does not reset
        // when FftProcessObj resets
        mSpectroExpeObj->Reset(BUFFER_SIZE, oversampling, freqRes,
                               sampleRate);
        
        mDispFftObj->AddMultichannelProcess(mSpectroExpeObj);
    }
    else
    {
        FftProcessObj16 *dispFftObj = mDispFftObj;
        dispFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mSpectroExpeObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);
    
    ApplyParams();
    
    UpdateTimeAxis();
    
    mIsInitialized = true;
}

void
SpectroExpe::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
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
    
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    bool isPlaying = IsTransportPlaying();
    mIsPlaying = isPlaying;

    if (mUIOpened)
    { 
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->SetIsPlaying(isPlaying || mMonitorEnabled);
        }
    
        if (mTimeAxis != NULL)
        {
            mTimeAxis->SetTransportPlaying(isPlaying || mMonitorEnabled);
        
            if (isPlaying || mMonitorEnabled)
            {
                if (mMustUpdateTimeAxis)
                {
                    UpdateTimeAxis();
                    mMustUpdateTimeAxis = false;
                }
            }
        
            if (!isPlaying && mWasPlaying && mMonitorEnabled)
                // Playing just stops and monitor is enabled
            {
                // => Reset time axis time
                mTimeAxis->UpdateFromTransport(0.0);
            }
        
            // FIX: play, then while playing, bypass plug using the host
            // => the time axis continues to scroll
            mTimeAxis->SetMustUpdate();
        }
    }
    
    mWasPlaying = isPlaying;
    
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

    if (mUIOpened)
    { 
        if (isPlaying || mMonitorEnabled)
        {
            vector<WDL_TypedBuf<BL_FLOAT> > dummy;
            mDispFftObj->Process(in, dummy, &out);
        
            mMustUpdateSpectrogram = true;
        
            // Time axis
            if (isPlaying)
            {
                BL_FLOAT sampleRate = GetSampleRate();
                BL_FLOAT samplePos = GetTransportSamplePos();
                if (samplePos >= 0.0)
                {
                    BL_FLOAT currentTime = samplePos/sampleRate;
                    if (mTimeAxis != NULL)
                        mTimeAxis->UpdateFromTransport(currentTime);
                }
            }
        }
    }
    
    // The plugin does not modify the sound, so bypass
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    if (mUIOpened)
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
SpectroExpe::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN,
                                     kGraph);
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);
     
    // GUIResize
    int newGraphWidth = mGraphWidthSmall + mGUIOffsetX;
    int newGraphHeight = mGraphHeightSmall + mGUIOffsetY;
    mGraph->Resize(newGraphWidth, newGraphHeight);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);
    int sepColor[4] = { 24, 24, 24, 255 };
    mGraph->SetSeparatorY0(2.0, sepColor);
        
    CreateGraphAxes();
    CreateSpectrogramDisplay(false);
    
    // Range
    mGUIHelper->CreateKnob(pGraphics,
                           kRangeX, kRangeY + offset,
                           KNOB_SMALL_FN,
                           kRangeFrames,
                           kRange,
                           TEXTFIELD_FN,
                           "BRIGHTNESS",
                           GUIHelper12::SIZE_DEFAULT);
    
    // Contrast
    mGUIHelper->CreateKnob(pGraphics,
                           kContrastX, kContrastY + offset,
                           KNOB_SMALL_FN,
                           kContrastFrames,
                           kContrast,
                           TEXTFIELD_FN,
                           "CONTRAST",
                           GUIHelper12::SIZE_DEFAULT);
    
    
    // ColorMap num
    const char *colormapRadioLabels[kRadioButtonsColorMapNumButtons] =
    { "BLUE", "GRAY", "RAINBOW", "WASP", "DAWN" };
    
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
                                   scrollSpeedRadioLabels);
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeSmallX, kGUISizeSmallY + offset,
                                      BUTTON_RESIZE_SMALL_FN,
                                      kGUISizeSmall, //
                                      "", 0);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeMediumX, kGUISizeMediumY + offset,
                                      BUTTON_RESIZE_MEDIUM_FN,
                                      kGUISizeMedium, //
                                      "", 1);
    
    mGUISizeBigButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizeBigX, kGUISizeBigY + offset,
                                      BUTTON_RESIZE_BIG_FN,
                                      kGUISizeBig, //
                                      "", 2);
    
    mGUISizePortraitButton = (IGUIResizeButtonControl *)
    mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                      kGUISizePortraitX, kGUISizePortraitY + offset,
                                      BUTTON_RESIZE_PORTRAIT_FN,
                                      kGUISizePortrait, //
                                      "", 3);
    
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX,
                                                     kCheckboxMonitorY + offset,
                                                     CHECKBOX_FN, kMonitor, "MON");
    
    const char *modeRadioLabels[kRadioButtonsModeNumButtons] =
    { "SPECTROGRAM", "PANOGRAM", "PANOGRAM FREQ",
      "CHROMAGRAM", "CHROMAGRAM FREQ", "STEREO WIDTH",
      "DUET MAGNS", "DUET PHASES"};
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeX,
                                   kRadioButtonsModeY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsModeNumButtons,
                                   kRadioButtonsModeVSize,
                                   kMode,
                                   false, "MODE",
                                   EAlign::Near,
                                   EAlign::Near,
                                   modeRadioLabels);
    
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
SpectroExpe::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    if (sampleRate != mPrevSampleRate)
    {
        // Frame rate have changed
        mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
        
        mPrevSampleRate = sampleRate;
        
        mSpectroExpeObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);
    }
    
    if (mSpectrogramDisplay != NULL)
    {
        //BL_FLOAT sampleRate = GetSampleRate();
        mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);
    }
    
    UpdateTimeAxis();

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SpectroExpe::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mSpectroExpeObj != NULL)
            {
                BLSpectrogram4 *spectro = mSpectroExpeObj->GetSpectrogram();
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
            
            if (mSpectroExpeObj != NULL)
            {
                BLSpectrogram4 *spectro = mSpectroExpeObj->GetSpectrogram();
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
            mScrollSpeedNum = GetParam(kScrollSpeed)->Int();
            
            SetScrollSpeed(mScrollSpeedNum);
        }
        break;
            
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            
            mMonitorEnabled = (value == 1);
            
            if (mMonitorEnabled)
            {
                if (mTimeAxis != NULL)
                    mTimeAxis->UpdateFromTransport(0.0);
            }
        }
        break;
            
        case kMode:
        {
            mMode = (SpectroExpeFftObj::Mode)GetParam(kMode)->Int();
            
            SetMode(mMode);
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectroExpe::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    UpdateTimeAxis();
    SetScrollSpeed(mScrollSpeedNum);
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectroExpe::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mMonitorControl = NULL;

    mGraph = NULL;
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    
    mSpectrogramDisplay = NULL;
    if (mSpectroExpeObj != NULL)
        mSpectroExpeObj->SetSpectrogramDisplay(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizePortraitButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectroExpe::SetColorMap(int colorMapNum)
{
    if (mSpectroExpeObj != NULL)
    {
        BLSpectrogram4 *spec = mSpectroExpeObj->GetSpectrogram();
    
        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[5] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_GREY,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_WASP,
            ColorMapFactory::COLORMAP_DAWN_FIXED
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
SpectroExpe::SetScrollSpeed(int scrollSpeedNum)
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
    
    if (mSpectroExpeObj != NULL)
    {
        mSpectroExpeObj->SetSpeedMod(speedMod);
        
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
SpectroExpe::GetNewGUISize(int guiSizeIdx, int *width, int *height)
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
SpectroExpe::PreResizeGUI(int guiSizeIdx,
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
    mSpectroExpeObj->SetSpectrogramDisplay(NULL);

    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectroExpe::GUIResizeParamChange(int guiSizeIdx)
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
SpectroExpe::OnIdle()
{
    ENTER_PARAMS_MUTEX;
    
    if (mUIOpened)
    {
        if (mMonitorControl != NULL)
        {
            mMonitorControl->SetDisabled(mIsPlaying);
        }
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SpectroExpe::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    // Axis
    BLSpectrogram4 *spectro = mSpectroExpeObj->GetSpectrogram();
    int numBuffers = spectro->GetMaxNumCols();
    
    BL_FLOAT ratio = mSpectrogramDisplay->GetScaleRatio();
    numBuffers *= ratio;
    
    // Adjust to exactly the number of visible columns in the
    // spectrogram
    // (there is a small width scale in SpectrogramDsiplayScroll, to hide border columns)
    int speedMod = mSpectrogramDisplay->GetSpeedMod();
    numBuffers *= speedMod;
    
    BL_FLOAT timeDuration =
    GraphTimeAxis6::ComputeTimeDuration(numBuffers,
                                        BUFFER_SIZE,
                                        OVERSAMPLING,
                                        sampleRate);
    
    //BL_FLOAT spacingSeconds = 1.0;
    // Manage to have always around 10 labels
    //spacingSeconds = timeDuration/10.0;
    
    if (mTimeAxis != NULL)
        mTimeAxis->Reset(BUFFER_SIZE, timeDuration,
                         //spacingSeconds);
                         MAX_NUM_TIME_AXIS_LABELS);
}

void
SpectroExpe::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mSpectroExpeObj == NULL))
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    mSpectrogram = mSpectroExpeObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    //dispSpec->SetYLogScale(false, 1.0);
    //mSpectrogram->SetYLogScale(true); //, Y_LOG_SCALE_FACTOR);
    mSpectrogram->SetYScale(Scale::MEL);
    mSpectrogram->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        mSpectrogramDisplay = new SpectrogramDisplayScroll3(this);
        mSpectrogramDisplay->SetSpectrogram(mSpectrogram, 0.0, 0.0, 1.0, 1.0);
    
        mSpectrogramDisplay->SetFftParams(BUFFER_SIZE, OVERSAMPLING, sampleRate);
        mSpectroExpeObj->SetSpectrogramDisplay(mSpectrogramDisplay);
    
        mGraph->AddCustomDrawer(mSpectrogramDisplay);
    }
}

void
SpectroExpe::CreateGraphAxes()
{
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mTimeAxis = new GraphTimeAxis6(false);
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2(false, Scale::MEL);
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);
    
    BL_FLOAT sampleRate = GetSampleRate();
    int graphWidth = mGraph->GetRECT().W();
    
    mTimeAxis->Init(mGraph, mHAxis, mGUIHelper,
                    BUFFER_SIZE, 1.0, MAX_NUM_TIME_AXIS_LABELS/*1.0*/);
    
    mFreqAxis->Init(mVAxis, mGUIHelper, false, BUFFER_SIZE, sampleRate, graphWidth);
    mFreqAxis->Reset(BUFFER_SIZE, sampleRate);
}

void
SpectroExpe::SetMode(SpectroExpeFftObj::Mode mode)
{
    mMode = mode;
    
    if (mSpectroExpeObj != NULL)
        mSpectroExpeObj->SetMode(mode);
    
    if (mMode == SpectroExpeFftObj::SPECTROGRAM)
    {
        mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::MEL);
    }
    else if (mMode == SpectroExpeFftObj::PANOGRAM)
    {
        mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::LINEAR);
    }
    else if (mMode == SpectroExpeFftObj::PANOGRAM_FREQ)
    {
        mSpectrogram->SetValueScale(Scale::LINEAR);
        //mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::MEL);
    }
    else if (mMode == SpectroExpeFftObj::CHROMAGRAM)
    {
        mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::LINEAR);
    }
    else if (mMode == SpectroExpeFftObj::CHROMAGRAM_FREQ)
    {
        //mSpectrogram->SetValueScale(Scale::LINEAR);
        mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::MEL);
        //mSpectrogram->SetYScale(Scale::LINEAR);
    }
    else if (mMode == SpectroExpeFftObj::STEREO_WIDTH)
    {
        mSpectrogram->SetValueScale(Scale::LINEAR);
        //mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::MEL);
    }
    else if (mMode == SpectroExpeFftObj::DUET_MAGNS)
    {
        mSpectrogram->SetValueScale(Scale::LINEAR);
        mSpectrogram->SetYScale(Scale::MEL);
    }
    else if (mMode == SpectroExpeFftObj::DUET_PHASES)
    {
        mSpectrogram->SetValueScale(Scale::LINEAR);
        //mSpectrogram->SetValueScale(Scale::DB);
        mSpectrogram->SetYScale(Scale::MEL);
    }
}
