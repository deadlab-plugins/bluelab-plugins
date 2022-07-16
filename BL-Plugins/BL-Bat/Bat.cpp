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
#include <SpectrogramDisplay2.h>

#include <BLImage.h>
#include <ImageDisplay2.h>

#include <BatFftObj5.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <GraphTimeAxis5.h>
#include <GraphFreqAxis2.h>

#include <GraphAxis2.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Bat.h"

#include "IPlug_include_in_plug_src.h"

//
// With 1024, we miss some frequencies
#define BUFFER_SIZE 2048

// 2: Quality is almost the same as with 4
// 2: CPU: 37%
//
// 4: CPU: 41%
//
#define OVERSAMPLING 4
#define FREQ_RES 1
#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

// Spectrogram
#define DISPLAY_SPECTRO 1

// GUI Size
#define NUM_GUI_SIZES 3

#define PLUG_WIDTH_MEDIUM 1080
#define PLUG_HEIGHT_MEDIUM 652

#define PLUG_WIDTH_BIG 1200
#define PLUG_HEIGHT_BIG 734

// DEBUG
#define DEBUG_IMAGE_DISPLAY 1

// Bat
// NOTE: Can't skip, because we play previously recorded sound
#define OPTIM_SKIP_IFFT 0 //1


#if 0
TODO: try the DUET algorithm (more sources than microphones), "blind"
https://www.researchgate.net/publication/220736985_Degenerate_Unmixing_Estimation_Technique_using_the_Constant_Q_Transform

NOTE: This doesn t work well maybe the tests here with reverb, and low azimuth changes are not suitable
NOTE2: fake test with white noise and delay(precedence) works very well
NOTE3: in the paper, they are in anechoic chamber
NOTE4: fake test with IR works very well
IDEA: make a scanner instead of a source detector => generate impulses, and try to get a map with reflected signal


IDEA (...): integrate stencil, but invert the result, and ponderate with the band line rel amplitude for a given freq set
- invert conicidence values
- integrate over mask line, but ponderate with the relative amplitudes (dependant on the bin line)
=> so we take more the frequencies that are actually played
=> and we may get rid of the static background noise from the Zoom H1

TODO(optim): use simdpp to optimize (download the version of simdpp that is designed for C++98)

IDEA: to optimize MODE_2D3: compute 2x2 conicidences, then interpolate coincidences (instead of computing many conincidences on interpolated sounds)

TODO: check that we can really detect several sources at the same time
if not, check SourceLocalisationSystem2::FindMinima3()

TODO: algo: pre-compute stencil masks to optimize


BUG (little): checkbox not working very well with Cubase 10 Mac
TODO: add 4th button for gui size retina
NOTE: check Chroma TODO list

TODO/BUG: MultiProcessObjs does not reset when FftProcessObj resets
TODO: MultiProcessObjs: keep mBufferSize and so on in the parent class (like ProcessObj)
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

    kSharpness,
    kSmooth,
    kIntermicCoeff,
    kSmoothData,
      
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
    kRangeY = 450,
    kKnobRangeFrames = 180,
    
    kContrastX = 285,
    kContrastY = 450,
    kKnobContrastFrames = 180,
    
    // GUI size
    kGUISizeSmallX = 20,
    kGUISizeSmallY = 435,
    
    kGUISizeMediumX = 20,
    kGUISizeMediumY = 463,
    
    kGUISizeBigX = 20,
    kGUISizeBigY = 491,
    
    kRadioButtonsColorMapX = 154,
    kRadioButtonsColorMapY = 443,
    kRadioButtonColorMapVSize = 100,
    kRadioButtonColorMapNumButtons = 6,
  
    kSharpnessX = 370,
    kSharpnessY = 450,
    kKnobSharpnessFrames = 180,
    
    kSmoothX = 470,
    kSmoothY = 450,
    kKnobSmoothFrames = 180,
  
    kIntermicCoeffX = 570,
    kIntermicCoeffY = 450,
    kKnobIntermicCoeffFrames = 180,
  
    kSmoothDataX = 670,
    kSmoothDataY = 450,
    kKnobSmoothDataFrames = 180,
    
    kLogoAnimFrames = 31
};

//
Bat::Bat(const InstanceInfo &info)
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

Bat::~Bat()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mBatObj != NULL)
        delete mBatObj;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
    
    if (mSpectrogramDisplayState != NULL)
        delete mSpectrogramDisplayState;
}

IGraphics *
Bat::MyMakeGraphics()
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
Bat::MyMakeLayout(IGraphics *pGraphics)
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
Bat::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mFftObj = NULL;
    mBatObj = NULL;
    
    mGraph = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectrogramDisplayState = NULL;
    
    mImage = NULL;
    mImageDisplay = NULL;
    
    mPrevSampleRate = GetSampleRate();
    
    // From Waves
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
Bat::InitParams()
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
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 5);

    // Sharpness
    BL_FLOAT defaultSharpness = 0.0;
    mSharpness = defaultSharpness;
    GetParam(kSharpness)->InitDouble("Sharpness", defaultSharpness, 0.0, 100, 1.0, "%",
                                     0, "", IParam::ShapePowCurve(2.0));

    // Smooth
    BL_FLOAT defaultSmooth = 0.9;
    mSmooth = defaultSmooth;
    GetParam(kSmooth)->InitDouble("TimeSmooth", defaultSmooth, 0.0, 100, 1.0, "%");
  
    // Smooth data
    BL_FLOAT defaultSmoothData = 0.0;
    mSmoothData = defaultSmoothData;
    GetParam(kSmoothData)->InitDouble("TimeSmoothData", defaultSmoothData, 0.0, 100, 1.0, "%");
    
    // Intermic coeff
    BL_FLOAT defaultIntermicCoeff = 1.0;
    mIntermicCoeff = defaultIntermicCoeff;
    GetParam(kIntermicCoeff)->InitDouble("IntermicCoeff", 
                                         defaultIntermicCoeff, 0.01, 10.0, 0.01, "");
  
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->InitInt("SmallGUI", 0, 0, 1, "", IParam::kFlagMeta);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    
    GetParam(kGUISizeMedium)->InitInt("MediumGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kGUISizeBig)->InitInt("BigGUI", 0, 0, 1, "", IParam::kFlagMeta);
}

void
Bat::ApplyParams()
{
    if (mBatObj != NULL)
    {
        BLSpectrogram4 *spectro = mBatObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }

#if DEBUG_IMAGE_DISPLAY
        if (mImage != NULL)
            mImage->SetRange(mRange);
	
        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateImage(false);
            mImageDisplay->UpdateColormap(true);
        }
#endif
    }
    
    if (mBatObj != NULL)
    {
        BLSpectrogram4 *spectro = mBatObj->GetSpectrogram();
        spectro->SetContrast(mContrast);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }

#if DEBUG_IMAGE_DISPLAY
        if (mImage != NULL)
            mImage->SetContrast(mContrast);

        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateImage(false);
            mImageDisplay->UpdateColormap(true);
        }
#endif
    }
    
    if (mGraph != NULL)
    {
        SetColorMap(mColorMapNum);
        
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->UpdateColormap(true);

#if DEBUG_IMAGE_DISPLAY
        if (mImageDisplay != NULL)
            mImageDisplay->UpdateColormap(true);
#endif
    }
    
    if (mBatObj != NULL)
        mBatObj->SetSharpness(mSharpness);

    if (mBatObj != NULL)
	    mBatObj->SetTimeSmooth(mSmooth);
    
    if (mBatObj != NULL)
        mBatObj->SetIntermicCoeff(mIntermicCoeff);

    if (mBatObj != NULL)
        mBatObj->SetTimeSmoothData(mSmoothData);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Bat::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        // Must use a second array
        // because the heritage doesn't convert autmatically from
        // WDL_TypedBuf<PostTransientFftObj2 *> to WDL_TypedBuf<ProcessObj *>
        // (tested with std vector too)
        vector<ProcessObj *> processObjs;
        int numChannels = 2;
        int numScInputs = 2;
        
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

        mBatObj = new BatFftObj5(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        // Must artificially call reset because MultiProcessObjs does not reset
        // when FftProcessObj resets
        mBatObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mFftObj->AddMultichannelProcess(mBatObj);
    }
    else
    {
        FftProcessObj16 *fftObj = mFftObj;
        fftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mBatObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);
    CreateImageDisplay(true);
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
Bat::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

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
            mGraph->Unlock();
        
        return;
    }

    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
    mSecureRestarter.Process(in);

    if (mUIOpened)
    {
        // FIX: avoid a scroll jump when stop playing,
        // at the moment we start to draw a selection
        if (mPrevPlaying && !IsTransportPlaying())
        {
            if (mSpectrogramDisplay != NULL)
                mSpectrogramDisplay->UpdateSpectrogram(true);
#if DEBUG_IMAGE_DISPLAY
            if (mImageDisplay != NULL)
                mImageDisplay->UpdateImage(true);
#endif
        }
    }
    mPrevPlaying = IsTransportPlaying();
      
    //
    //vector<WDL_TypedBuf<BL_FLOAT> > playOut = out;
    mFftObj->Process(in, scIn, &out);
    
    // ??
    mMustUpdateSpectrogram = true;

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
	
#if DEBUG_IMAGE_DISPLAY
            if (mImageDisplay != NULL)
            {
                mImageDisplay->UpdateImage(true);
                mMustUpdateSpectrogram = false;
            }
#endif
        }
        else
        {
            // Do not update the spectrogram texture
            if (mSpectrogramDisplay != NULL)
                mSpectrogramDisplay->UpdateSpectrogram(false);
	
#if DEBUG_IMAGE_DISPLAY
            if (mImageDisplay != NULL)
                mImageDisplay->UpdateImage(false);
#endif
        }
    }
    
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
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
Bat::CreateControls(IGraphics *pGraphics, int offset)
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

    CreateSpectrogramDisplay(false);
    CreateImageDisplay(false);
    
    // Range
    mGUIHelper->CreateKnob(pGraphics,
                           kRangeX, kRangeY + offset,
                           KNOB_SMALL_FN,
                           kKnobRangeFrames,
                           kRange,
                           TEXTFIELD_FN,
                           "BRIGHTNESS",
                           GUIHelper12::SIZE_SMALL);
    
    // Contrast
    mGUIHelper->CreateKnob(pGraphics,
                           kContrastX, kContrastY + offset,
                           KNOB_SMALL_FN,
                           kKnobContrastFrames,
                           kContrast,
                           TEXTFIELD_FN,
                           "CONTRAST",
                           GUIHelper12::SIZE_SMALL);

    // Shaprness
    mGUIHelper->CreateKnob(pGraphics,
                           kSharpnessX, kSharpnessY + offset,
                           KNOB_SMALL_FN,
                           kKnobSharpnessFrames,
                           kSharpness,
                           TEXTFIELD_FN,
                           "SHARPNESS",
                           GUIHelper12::SIZE_SMALL);

    // Time smooth
    mGUIHelper->CreateKnob(pGraphics,
                           kSmoothX, kSmoothY + offset,
                           KNOB_SMALL_FN,
                           kKnobSmoothFrames,
                           kSmooth,
                           TEXTFIELD_FN,
                           "TIME SMOOTH",
                           GUIHelper12::SIZE_SMALL);

    // Intermic coeff
    mGUIHelper->CreateKnob(pGraphics,
                           kIntermicCoeffX, kIntermicCoeffY + offset,
                           KNOB_SMALL_FN,
                           kKnobIntermicCoeffFrames,
                           kIntermicCoeff,
                           TEXTFIELD_FN,
                           "INTERMIC COEFF",
                           GUIHelper12::SIZE_SMALL);

    // Time smooth data
    mGUIHelper->CreateKnob(pGraphics,
                           kSmoothDataX, kSmoothDataY + offset,
                           KNOB_SMALL_FN,
                           kKnobSmoothDataFrames,
                           kSmoothData,
                           TEXTFIELD_FN,
                           "TIME SMOOTH D.",
                           GUIHelper12::SIZE_SMALL);
    
    // ColorMap num
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
Bat::OnReset()
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
      
        mBatObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);
    }

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Bat::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mBatObj != NULL)
            {
                BLSpectrogram4 *spectro = mBatObj->GetSpectrogram();
                spectro->SetRange(mRange);
                
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }

#if DEBUG_IMAGE_DISPLAY
                if (mImage != NULL)
                    mImage->SetRange(mRange);
	    
                if (mImageDisplay != NULL)
                {
                    mImageDisplay->UpdateImage(false);
                    mImageDisplay->UpdateColormap(true);
                }
#endif
            }
        }
        break;
            
        case kContrast:
        {
            mContrast = GetParam(paramIdx)->Value();
            
            if (mBatObj != NULL)
            {
                BLSpectrogram4 *spectro = mBatObj->GetSpectrogram();
                spectro->SetContrast(mContrast);
            
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }

#if DEBUG_IMAGE_DISPLAY
                if (mImage != NULL)
                    mImage->SetContrast(mContrast);

                if (mImageDisplay != NULL)
                {
                    mImageDisplay->UpdateImage(false);
                    mImageDisplay->UpdateColormap(true);
                }
#endif
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
                    mSpectrogramDisplay->UpdateColormap(true);

                if (mImageDisplay != NULL)
                    mImageDisplay->UpdateColormap(true);
            }
        }
        break;

        case kSharpness:
        {
            BL_FLOAT sharpness = GetParam(paramIdx)->Value();
            sharpness /= 100.0;
            mSharpness = sharpness;

            if (mBatObj != NULL)
                mBatObj->SetSharpness(mSharpness);
        }
        break;
    
        case kSmooth:
        {
            BL_FLOAT smooth = GetParam(kSmooth)->Value();
            smooth /= 100.0;
            mSmooth = smooth;
	  
            if (mBatObj != NULL)
                mBatObj->SetTimeSmooth(mSmooth);
        }
        break;
    
        case kIntermicCoeff:
        {
            BL_FLOAT coeff = GetParam(kIntermicCoeff)->Value();
            mIntermicCoeff = coeff;
	  
            if (mBatObj != NULL)
                mBatObj->SetIntermicCoeff(mIntermicCoeff);
        }
        break;
    
        case kSmoothData:
        {
            BL_FLOAT smooth = GetParam(kSmoothData)->Value();
            smooth /= 100.0;
            mSmoothData = smooth;
	  
            if (mBatObj != NULL)
                mBatObj->SetTimeSmoothData(smooth);
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
                    
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Bat::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
Bat::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph

    mGraph = NULL;
    
    mSpectrogramDisplay = NULL;
    if (mBatObj != NULL)
        mBatObj->SetSpectrogramDisplay(NULL);
    
    mImageDisplay = NULL;
    if (mBatObj != NULL)
        mBatObj->SetImageDisplay(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Bat::SetColorMap(int colorMapNum)
{
    if (mBatObj != NULL)
    {
        BLSpectrogram4 *spec = mBatObj->GetSpectrogram();
    
        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[kRadioButtonColorMapNumButtons] =
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

#if DEBUG_IMAGE_DISPLAY
        if (mImage != NULL)
            mImage->SetColorMap(colorMapNum0);
#endif
    }
}

void
Bat::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT },
        { PLUG_WIDTH_MEDIUM, PLUG_HEIGHT_MEDIUM },
        { PLUG_WIDTH_BIG, PLUG_HEIGHT_BIG },
    };
    
    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
Bat::PreResizeGUI(int guiSizeIdx,
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
    if (mBatObj != NULL)
        mBatObj->SetSpectrogramDisplay(NULL);
    
    mImageDisplay = NULL;
    if (mBatObj != NULL)
        mBatObj->SetImageDisplay(NULL);
    
    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
Bat::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium, kGUISizeBig };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
    { mGUISizeSmallButton, mGUISizeMediumButton,
      mGUISizeBigButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

void
Bat::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mBatObj == NULL))
        return;
    
    mSpectrogram = mBatObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    mSpectrogram->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        mSpectrogramDisplay = new SpectrogramDisplay2(mSpectrogramDisplayState);
        mSpectrogramDisplayState = mSpectrogramDisplay->GetState();
        
        mSpectrogramDisplay->SetBounds(0.0, 0.0, 1.0, 1.0);
        mSpectrogramDisplay->SetSpectrogram(mSpectrogram);
    
        mBatObj->SetSpectrogramDisplay(mSpectrogramDisplay);
        
        mGraph->AddCustomDrawer(mSpectrogramDisplay);
    }
}

void
Bat::CreateImageDisplay(bool createFromInit)
{
#if DEBUG_IMAGE_DISPLAY
    if (!createFromInit && (mBatObj == NULL))
        return;
    
    mImage = mBatObj->GetImage();
    
    if (mGraph != NULL)
    {
        mImageDisplay = new ImageDisplay2();
        mImageDisplay->SetBounds(0.0, 0.0, 0.25, 1.0);
        mImageDisplay->SetImage(mImage);
    
        mBatObj->SetImageDisplay(mImageDisplay);
        
        mGraph->AddCustomDrawer(mImageDisplay);
    }
#endif
}
