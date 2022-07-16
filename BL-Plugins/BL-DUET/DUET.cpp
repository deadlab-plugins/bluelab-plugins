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

#include <DUETFftObj2.h>

#include <BLSpectrogram4.h>
#include <SpectrogramDisplay2.h>
#include <BLImage.h>
#include <ImageDisplay2.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <DUETCustomControl.h>
#include <DUETCustomDrawer.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "DUET.h"

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

// When set to BUFFER_SIZE, it makes less flat areas at 0
#define SPECTRO_MAX_NUM_COLS BUFFER_SIZE //BUFFER_SIZE/4

// GUI Size
#define NUM_GUI_SIZES 3 //4

#define PLUG_WIDTH_MEDIUM 1080
#define PLUG_HEIGHT_MEDIUM 652

#define PLUG_WIDTH_BIG 1200
#define PLUG_HEIGHT_BIG 734

// Since the GUI is very full, reduce the offset after
// the radiobuttons title text
#define RADIOBUTTONS_TITLE_OFFSET -5

// Optimization
// NOTE: not yet tested with DUET
#define OPTIM_SKIP_IFFT 0 // 1

#if 0
See: http://research.spa.aalto.fi/projects/sparta_vsts/plugins.html
Sparta DIRAss (two publications that seem interesting)

And Sparta Power Map (keyword: acoustic camera): http://research.spa.aalto.fi/projects/sparta_vsts/plugins.html

NOTE: DUET algorithm (more sources than microphones), "blind"
https://www.researchgate.net/publication/227143748_The_DUET_blind_source_separation_algorithm

Extension:
https://www.researchgate.net/publication/220736985_Degenerate_Unmixing_Estimation_Technique_using_the_Constant_Q_Transform

NOTE: Real Time DUET algorithm: https://www.researchgate.net/publication/2551938_Real-Time_Time-Frequency_Based_Blind_Source_Separation

NOTE: PAC works dawmned well on the example: BlueLab-DUET-TestReaper-VST2-TwoSine. (must increase peak width)
NOTE: PAC works on BlueLab-DUET-TestReaper-VST2-TwoVoices.RPP
(but not perfectly)
NOTE: BlueLab-DUET-TestReaper-VST2-BeesAndBirds: we can extract only the birds, very well!

NOTE: BlueLab-StereoDeReverb-TestReaper-VST2-DeReverb.RPP => beginning of the sound removed when selecting dry signal
=> but it is because it is reverb signal ! (no bug!)

IDEA: track peaks !

IDEA:
- test with different alpha and delta (frequencies, phases, other parameters...)
- maybe check with more than 2 dimensions

IDEA: Panogram: add a "source" mode

TODO: revert "allZero" (currently set for debugging)

TODO: Rebalance with Wiener complex masks

New StereoWidth: many screenshots in the plugin page.
Update with Rebalance update. No videos.

SlowDown plug ? 
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
    
    kThrsFloor,
    kThrsPeaks,
    
    kSmooth,
    
    kUseKernelSmooth,
  
    kDispThrs,
    kDispMax,
    kDispMasks,
    
    kUseSoftMasks,
    kUseSoftMasksComp,
    kSoftMaskSize,
  
    kThrsPeaksWidth,
  
    kUseGradientMasks,
    kThresholdAll,
  
    kHistoSize,
    kAlphaZoom,
    kDeltaZoom,
  
    kPAC,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kGraphX = 0,
    kGraphY = 0,
    
    kRangeX = 210,
    kRangeY = 426,
    kRangeFrames = 180,
    
    kContrastX = 290,
    kContrastY = 426,
    kContrastFrames = 180,
    
    kRadioButtonsColorMapX = 154,
    kRadioButtonsColorMapY = 443,
    kRadioButtonsColorMapVSize = 116,
    kRadioButtonsColorMapNumButtons = 7,
    
    // GUI size
    kGUISizeSmallX = 20,
    kGUISizeSmallY = 435,
    
    kGUISizeMediumX = 20,
    kGUISizeMediumY = 463,
    
    kGUISizeBigX = 20,
    kGUISizeBigY = 491,
    
    kSmoothX = 370,
    kSmoothY = 426,
    kSmoothFrames = 180,
    
    kThrsFloorX = 470,
    kThrsFloorY = 426,
    kThrsFloorFrames = 180,
  
    kThrsPeaksX = 570,
    kThrsPeaksY = 426,
    kThrsPeaksFrames = 180,
    
    kUseKernelSmoothX = 670,
    kUseKernelSmoothY = 426,
    
    kDispThrsX = 670,
    kDispThrsY = 472,
    
    kDispMaxX = 770,
    kDispMaxY = 426,
    
    kDispMasksX = 770,
    kDispMasksY = 472,
    
    kUseSoftMasksX = 870,
    kUseSoftMasksY = 426,
    
    kUseSoftMasksCompX = 870,
    kUseSoftMasksCompY = 472,
    
    kThrsPeaksWidthX = 570,
    kThrsPeaksWidthY = 516,
    kThrsPeaksWidthFrames = 180,
    
    kSoftMaskSizeX = 660,
    kSoftMaskSizeY = 516,
    kSoftMaskSizeFrames = 180,
    
    kUseGradientMasksX = 770,
    kUseGradientMasksY = 516,
    
    kThresholdAllX = 870,
    kThresholdAllY = 516,
    
    //
    kHistoSizeX = 210,
    kHistoSizeY = 516,
    kHistoSizeFrames = 180,
    
    kAlphaZoomX = 290,
    kAlphaZoomY = 516,
    kAlphaZoomFrames = 180,
    
    kDeltaZoomX = 370,
    kDeltaZoomY = 516,
    kDeltaZoomFrames = 180,
    
    //
    kPACX = 480,
    kPACY = 516,
    
    kLogoAnimFrames = 31
};

//
DUET::DUET(const InstanceInfo &info)
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

DUET::~DUET()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mDUETObj != NULL)
        delete mDUETObj;

    if (mGraphDrawer != NULL)
        delete mGraphDrawer;

    if (mGraphControl != NULL)
        delete mGraphControl;
    
    if (mSpectrogramState != NULL)
        delete mSpectrogramState;
}

IGraphics *
DUET::MyMakeGraphics()
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
DUET::MyMakeLayout(IGraphics *pGraphics)
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
DUET::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mFftObj = NULL;
    mDUETObj = NULL;
    
    mGraph = NULL;
    
    mSpectrogram = NULL;
    mSpectrogramDisplay = NULL;
    mSpectrogramState = NULL;
    
    mImage = NULL;
    mImageDisplay = NULL;
    
    mGraphControl = NULL;
    mGraphDrawer = NULL;
    
    mPrevSampleRate = GetSampleRate();

    mParamChanged = false;
    
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
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
    
    mPrevPlaying = false;
}

void
DUET::InitParams()
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
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 6);
    
    // GUI resize
    mGUISizeIdx = 0;
    GetParam(kGUISizeSmall)->InitInt("SmallGUI", 0, 0, 1, "", IParam::kFlagMeta);
    // Set to checkd at the beginning
    GetParam(kGUISizeSmall)->Set(1.0);
    
    GetParam(kGUISizeMedium)->InitInt("MediumGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kGUISizeBig)->InitInt("BigGUI", 0, 0, 1, "", IParam::kFlagMeta);
    
    
    // Threshold floor
    BL_FLOAT defaultThrsFloor = 0.0;
    mThresholdFloor = defaultThrsFloor;
    GetParam(kThrsFloor)->InitDouble("ThresholdFloor", defaultThrsFloor,
                                     0.0, 100, 0.01/*1.0*/, "%",
                                     0, "", IParam::ShapePowCurve(4.0));
    
    // Threshold peak
    BL_FLOAT defaultThrsPeaks = 0.0;
    mThresholdPeaks = defaultThrsPeaks;
    GetParam(kThrsPeaks)->InitDouble("ThresholdPeaks", defaultThrsPeaks,
                                     0.0, 100, 0.01/*1.0*/, "%",
                                     0, "", IParam::ShapePowCurve(4.0));
    
    // Threshold peak width
    BL_FLOAT defaultThrsPeaksWidth = 0.0;
    mThresholdPeaksWidth = defaultThrsPeaksWidth;
    GetParam(kThrsPeaksWidth)->InitDouble("ThresholdPeaksWidth", defaultThrsPeaksWidth,
                                          0.0, 100, 0.001/*1.0*/, "%",
                                          0, "", IParam::ShapePowCurve(4.0));
    
    // Smooth
    BL_FLOAT defaultSmooth = 0.9;
    mSmooth = defaultSmooth;
    GetParam(kSmooth)->InitDouble("TimeSmooth", defaultSmooth, 0.0, 100, 1.0, "%");

    mUseKernelSmooth = false;
    GetParam(kUseKernelSmooth)->InitInt("UseKernelSmooth", 0, 0, 1);

    mDispThreshold = false;
    GetParam(kDispThrs)->InitInt("DisplayThreshold", 0, 0, 1);

    mDispMax = false;
    GetParam(kDispMax)->InitInt("DisplayMaxima", 0, 0, 1);

    mDispMasks = false;
    GetParam(kDispMasks)->InitInt("DisplayMasks", 0, 0, 1);

    mUseSoftMasks = false;
    GetParam(kUseSoftMasks)->InitInt("UseSoftMasks", 0, 0, 1);

    mUseSoftMasksComp = false;
    GetParam(kUseSoftMasksComp)->InitInt("UseSoftMasksComp", 0, 0, 1);
    
    // Soft mask size
    int defaultSoftMaskSize = 8/*4*/;
    mSoftMasksSize = defaultSoftMaskSize;
    GetParam(kSoftMaskSize)->InitInt("SoftMaskSize", defaultSoftMaskSize, 1, 32, "");

    mUseGradientMasks = false;
    GetParam(kUseGradientMasks)->InitInt("UseGradientMasks", 0, 0, 1);

    mThresholdAll = false;
    GetParam(kThresholdAll)->InitInt("ThresholdAll", 0, 0, 1);
  
    int defaultHistoSize = 64;
    mHistoSize = defaultHistoSize;
    GetParam(kHistoSize)->InitInt("HistoSize", defaultHistoSize, 32, 128, "");
    
    double defaultAlphaZoom = 1.0;
    mAlphaZoom = defaultAlphaZoom;
    GetParam(kAlphaZoom)->InitDouble("AlphaZoom", defaultAlphaZoom, 1.0, 10.0, 0.1, "");
  
    double defaultDeltaZoom = 1.0;
    mDeltaZoom = defaultDeltaZoom;
    GetParam(kDeltaZoom)->InitDouble("DeltaZoom", defaultDeltaZoom, 1.0, 10.0, 0.1, "");

    mUsePhaseAliasingCorrection = 0;
    GetParam(kPAC)->InitInt("UsePhaseAliasingCorrection", 0, 0, 1);
}

void
DUET::ApplyParams()
{
    if (mDUETObj != NULL)
    {
        BLSpectrogram4 *spectro = mDUETObj->GetSpectrogram();
        spectro->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
        
        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateImage(false);
            mImageDisplay->UpdateColormap(true);
        }
    }
    
    if (mDUETObj != NULL)
    {
        BLSpectrogram4 *spectro = mDUETObj->GetSpectrogram();
        spectro->SetContrast(mContrast);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColormap(true);
        }
        
        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateImage(false);
            mImageDisplay->UpdateColormap(true);
        }
    }
    
    if (mGraph != NULL)
    {
        SetColorMap(mColorMapNum);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateColormap(true);
        }
        
        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateColormap(true);
        }
    }

    if (mDUETObj != NULL)
    {
        mDUETObj->SetTimeSmooth(mSmooth);
        mDUETObj->SetThresholdFloor(mThresholdFloor);
        mDUETObj->SetThresholdPeaks(mThresholdPeaks);
        mDUETObj->SetThresholdPeaksWidth(mThresholdPeaksWidth);
        mDUETObj->SetUseKernelSmooth(mUseKernelSmooth);
        mDUETObj->SetDisplayThresholded(mDispThreshold);
        mDUETObj->SetDisplayMaxima(mDispMax);
        mDUETObj->SetDisplayMasks(mDispMasks);
        mDUETObj->SetUseSoftMasks(mUseSoftMasks);
        mDUETObj->SetUseSoftMasksComp(mUseSoftMasksComp);
        mDUETObj->SetSoftMaskSize(mSoftMasksSize);
        mDUETObj->SetUseGradientMasks(mUseGradientMasks);
        mDUETObj->SetThresholdAll(mThresholdAll);
        mDUETObj->SetHistogramSize(mHistoSize);
        mDUETObj->SetAlphaZoom(mAlphaZoom);
        mDUETObj->SetDeltaZoom(mDeltaZoom);
        mDUETObj->SetUsePhaseAliasingCorrection(mUsePhaseAliasingCorrection);
    }
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
DUET::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        // Play objs
        vector<ProcessObj *> playProcessObjs;  
        int numChannels = 2; //1;
        int numScInputs = 2;
        
        mFftObj = new FftProcessObj16(playProcessObjs,
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

        mDUETObj = new DUETFftObj2(mGraph,
                                   BUFFER_SIZE, oversampling, freqRes,
                                   sampleRate);
        
        // Must artificially call reset because MultiProcessObjs does not reset
        // when FftProcessObj resets
        mDUETObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mFftObj->AddMultichannelProcess(mDUETObj);
    }
    else
    {
        FftProcessObj16 *dispFftObj = mFftObj;
        dispFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
        
        mDUETObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);
    CreateImageDisplay(true);
	
    ApplyParams();
    
    mIsInitialized = true;
}

void
DUET::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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
    
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    //bool isPlaying = IsTransportPlaying();
    
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
	
            if (mImageDisplay != NULL)
                mImageDisplay->UpdateImage(true);
        }
    }
    mPrevPlaying = IsTransportPlaying();
    
    // Set the outputs to 0
    //
    // FIX: on Protools, in render mode, after play is finished,
    // there is a buzz sound
    for (int i = 0; i < out.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &out0 = out[i];
        BLUtils::FillAllZero(&out0);
    }
    
    //vector<WDL_TypedBuf<BL_FLOAT> > playOut = out;
    mFftObj->Process(in, scIn, &out);
    
    mMustUpdateSpectrogram = true;
    
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

            if (mImageDisplay != NULL)
            {
                mImageDisplay->UpdateImage(true);
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
	
            if (mImageDisplay != NULL)
            {
                mImageDisplay->UpdateImage(false);
                mMustUpdateSpectrogram = false;
            }
        }
    }
    
    if (mParamChanged)
    {
        mDUETObj->Update();
      
        mParamChanged = false;
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
DUET::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN,
                                     kGraph);
    mGraph->GetSize(&mGraphWidthSmall, &mGraphHeightSmall);

    if (mDUETObj != NULL)
        mDUETObj->SetGraph(mGraph);
    
    if (mGraphDrawer == NULL)
        mGraphDrawer = new DUETCustomDrawer();
    mGraph->AddCustomDrawer(mGraphDrawer);

    if (mGraphControl == NULL)
        mGraphControl = new DUETCustomControl(this);
    mGraph->AddCustomControl(mGraphControl);

    CreateSpectrogramDisplay(false);
    CreateImageDisplay(false);
    
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
    { "BLUE", "WASP", "GREEN", "RAINBOW", "DAWN", "SWEET", "GREY" };
    
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
    //
    mGUIHelper->CreateKnob(pGraphics,
                           kThrsFloorX, kThrsFloorY + offset,
                           KNOB_SMALL_FN,
                           kThrsFloorFrames,
                           kThrsFloor,
                           TEXTFIELD_FN,
                           "THRS FLOOR",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kThrsPeaksX, kThrsPeaksY + offset,
                           KNOB_SMALL_FN,
                           kThrsPeaksFrames,
                           kThrsPeaks,
                           TEXTFIELD_FN,
                           "THRS PEAKS",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kThrsPeaksWidthX, kThrsPeaksWidthY + offset,
                           KNOB_SMALL_FN,
                           kThrsPeaksWidthFrames,
                           kThrsPeaksWidth,
                           TEXTFIELD_FN,
                           "THRS P WIDTH",
                           GUIHelper12::SIZE_DEFAULT);
    
    mGUIHelper->CreateKnob(pGraphics,
                           kSmoothX, kSmoothY + offset,
                           KNOB_SMALL_FN,
                           kSmoothFrames,
                           kSmooth,
                           TEXTFIELD_FN,
                           "TIME SMOOTH",
                           GUIHelper12::SIZE_DEFAULT);
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseKernelSmoothX,
                                   kUseKernelSmoothY + offset,
                                   CHECKBOX_FN, kUseKernelSmooth, "K SMOOTH");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kDispThrsX,
                                   kDispThrsY + offset,
                                   CHECKBOX_FN, kDispThrs, "DISP THRS");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kDispMaxX,
                                   kDispMaxY + offset,
                                   CHECKBOX_FN, kDispMax, "DISP MAX");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kDispMasksX,
                                   kDispMasksY + offset,
                                   CHECKBOX_FN, kDispMasks, "DISP MASKS");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseSoftMasksX,
                                   kUseSoftMasksY + offset,
                                   CHECKBOX_FN, kUseSoftMasks, "SOFT MASKS");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseSoftMasksCompX,
                                   kUseSoftMasksCompY + offset,
                                   CHECKBOX_FN, kUseSoftMasksComp, "SOFT MASKS COMP");

    mGUIHelper->CreateKnob(pGraphics,
                           kSoftMaskSizeX, kSoftMaskSizeY + offset,
                           KNOB_SMALL_FN,
                           kSoftMaskSizeFrames,
                           kSoftMaskSize,
                           TEXTFIELD_FN,
                           "SOFT MSK SZ",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseGradientMasksX,
                                   kUseGradientMasksY + offset,
                                   CHECKBOX_FN, kUseGradientMasks, "GRADIENT MASKS");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kThresholdAllX,
                                   kThresholdAllY + offset,
                                   CHECKBOX_FN, kThresholdAll, "THRS ALL");

    mGUIHelper->CreateKnob(pGraphics,
                           kHistoSizeX, kHistoSizeY + offset,
                           KNOB_SMALL_FN,
                           kHistoSizeFrames,
                           kHistoSize,
                           TEXTFIELD_FN,
                           "HISTO SIZE",
                           GUIHelper12::SIZE_DEFAULT);
    
    mGUIHelper->CreateKnob(pGraphics,
                           kAlphaZoomX, kAlphaZoomY + offset,
                           KNOB_SMALL_FN,
                           kAlphaZoomFrames,
                           kAlphaZoom,
                           TEXTFIELD_FN,
                           "ALPHA ZOOM",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kDeltaZoomX, kDeltaZoomY + offset,
                           KNOB_SMALL_FN,
                           kDeltaZoomFrames,
                           kDeltaZoom,
                           TEXTFIELD_FN,
                           "DELTA ZOOM",
                           GUIHelper12::SIZE_DEFAULT);

    // Phase Aliasing Correction
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kPACX,
                                   kPACY + offset,
                                   CHECKBOX_FN, kPAC, "PAC");
    
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
DUET::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    //if (sampleRate != mPrevSampleRate) // For DUET
    {
        // Frame rate have changed
        mPrevSampleRate = sampleRate;
        
        if (mDUETObj != NULL)
            mDUETObj->Reset(BUFFER_SIZE, OVERSAMPLING, 1, sampleRate);

        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->Reset();
    }

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
DUET::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;

    mParamChanged = true;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mDUETObj != NULL)
            {
                BLSpectrogram4 *spectro = mDUETObj->GetSpectrogram();
                spectro->SetRange(mRange);
                
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }

                if (mImage != NULL)
                    mImage->SetRange(mRange);
	    
                if (mImageDisplay != NULL)
                {
                    mImageDisplay->UpdateImage(false);
                    mImageDisplay->UpdateColormap(true);
                }
            }
        }
        break;
            
        case kContrast:
        {
            mContrast = GetParam(paramIdx)->Value();
            
            if (mDUETObj != NULL)
            {
                BLSpectrogram4 *spectro = mDUETObj->GetSpectrogram();
                spectro->SetContrast(mContrast);
            
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                    mSpectrogramDisplay->UpdateColormap(true);
                }

                if (mImage != NULL)
                    mImage->SetContrast(mContrast);

                if (mImageDisplay != NULL)
                {
                    mImageDisplay->UpdateImage(false);
                    mImageDisplay->UpdateColormap(true);
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

                if (mImageDisplay != NULL)
                    mImageDisplay->UpdateColormap(true);
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

        case kSmooth:
        {
            BL_FLOAT value = GetParam(kSmooth)->Value();
            BL_FLOAT smooth = value/100.0;
            mSmooth = smooth;
	  
            if (mDUETObj != NULL)
                mDUETObj->SetTimeSmooth(mSmooth);
        }
        break;
      
        case kThrsFloor:
        {
            BL_FLOAT value = GetParam(kThrsFloor)->Value();
            BL_FLOAT threshold = value/100.0;
            mThresholdFloor = threshold;
	  
            if (mDUETObj != NULL)
                mDUETObj->SetThresholdFloor(mThresholdFloor);
        }
        break;

        case kThrsPeaks:
        {
            BL_FLOAT value = GetParam(kThrsPeaks)->Value();
            BL_FLOAT threshold = value/100.0;
            mThresholdPeaks = threshold;
	  
            if (mDUETObj != NULL)
                mDUETObj->SetThresholdPeaks(mThresholdPeaks);
        }
        break;
    
        case kThrsPeaksWidth:
        {
            BL_FLOAT value = GetParam(kThrsPeaksWidth)->Value();
            BL_FLOAT threshold = value/100.0;
            mThresholdPeaksWidth = threshold;
	  
            if (mDUETObj != NULL)
                mDUETObj->SetThresholdPeaksWidth(mThresholdPeaksWidth);
        }
        break;
          
        case kUseKernelSmooth:
        {
            int value = GetParam(kUseKernelSmooth)->Value();
            bool useKernelSmoothFlag = (value == 1);
            mUseKernelSmooth = useKernelSmoothFlag;
	
            if (mDUETObj != NULL)
                mDUETObj->SetUseKernelSmooth(mUseKernelSmooth);
        }
        break;
    
        case kDispThrs:
        {
            int value = GetParam(kDispThrs)->Value();
            bool dispThrs = (value == 1);
            mDispThreshold = dispThrs;
	  
            if (mDUETObj != NULL)
                mDUETObj->SetDisplayThresholded(mDispThreshold);
        }
        break;
    
        case kDispMax:
        {
            int value = GetParam(kDispMax)->Value();
            bool dispMax = (value == 1);
            mDispMax = dispMax;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetDisplayMaxima(mDispMax);
        }
        break;
       
        case kDispMasks:
        {
            int value = GetParam(kDispMasks)->Value();
            bool dispMasks = (value == 1);
            mDispMasks = dispMasks;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetDisplayMasks(mDispMasks);
        }
        break;
    
        case kUseSoftMasks:
        {
            int value = GetParam(kUseSoftMasks)->Value();
            bool useSoftMasks = (value == 1);
            mUseSoftMasks = useSoftMasks;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetUseSoftMasks(mUseSoftMasks);
        }
        break;
       
        case kUseSoftMasksComp:
        {
            int value = GetParam(kUseSoftMasksComp)->Value();
            bool useSoftMasksComp = (value == 1);
            mUseSoftMasksComp = useSoftMasksComp;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetUseSoftMasksComp(mUseSoftMasksComp);
        }
        break;
         
        case kSoftMaskSize:
        {
            int size = GetParam(kSoftMaskSize)->Int();
            mSoftMasksSize = size;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetSoftMaskSize(mSoftMasksSize);
        }
        break;
       
        case kUseGradientMasks:
        {
            int value = GetParam(kUseGradientMasks)->Value();
            bool useGradientMasks = (value == 1);
            mUseGradientMasks = useGradientMasks;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetUseGradientMasks(mUseGradientMasks);
        }
        break;
       
        case kThresholdAll:
        {
            int value = GetParam(kThresholdAll)->Value();
            bool thresholdAll = (value == 1);
            mThresholdAll = thresholdAll;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetThresholdAll(mThresholdAll);
        }
        break;
      
        case kHistoSize:
        {
            int histoSize = GetParam(kHistoSize)->Int();
            mHistoSize = histoSize;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetHistogramSize(mHistoSize);
        }
        break;
    
        case kAlphaZoom:
        {
            BL_FLOAT alphaZoom = GetParam(kAlphaZoom)->Value();
            mAlphaZoom = alphaZoom;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetAlphaZoom(mAlphaZoom);
        }
        break;
    
        case kDeltaZoom:
        {
            BL_FLOAT deltaZoom = GetParam(kDeltaZoom)->Value();
            mDeltaZoom = deltaZoom;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetDeltaZoom(mDeltaZoom);
        }
        break;
        
        case kPAC:
        {
            int value = GetParam(kPAC)->Value();
            bool usePhaseAliasingCorrection = (value == 1);
            mUsePhaseAliasingCorrection = usePhaseAliasingCorrection;
	 
            if (mDUETObj != NULL)
                mDUETObj->SetUsePhaseAliasingCorrection(mUsePhaseAliasingCorrection);
        }
        break;
   
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
DUET::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
DUET::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph

    mGraph = NULL;
    
    mDUETObj->SetGraph(NULL);
    
    mSpectrogramDisplay = NULL;
    if (mDUETObj != NULL)
        mDUETObj->SetSpectrogramDisplay(NULL);
    
    mImageDisplay = NULL;
    if (mDUETObj != NULL)
        mDUETObj->SetImageDisplay(NULL);

    //mGraphControl = NULL;
    //mGraphDrawer = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizePortraitButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
DUET::SetColorMap(int colorMapNum)
{
    if (mDUETObj != NULL)
    {
        // Take the best 5 colormaps
        ColorMapFactory::ColorMap colorMapNums[7] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_WASP,
            ColorMapFactory::COLORMAP_GREEN,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_DAWN_FIXED,
            ColorMapFactory::COLORMAP_SWEET,
            ColorMapFactory::COLORMAP_GREY
        };
        ColorMapFactory::ColorMap colorMapNum0 = colorMapNums[colorMapNum];
    
        BLSpectrogram4 *spec = mDUETObj->GetSpectrogram();
        spec->SetColorMap(colorMapNum0);
    
        // FIX: set to false,false to fix
        // BUG: play data, stop, change colormap
        // => there is sometimes a small jumps of the data to the left
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
        }
        
        BLImage *img = mDUETObj->GetImage();
        img->SetColorMap(colorMapNum0);
        
        if (mImageDisplay != NULL)
        {
            mImageDisplay->UpdateImage(false);
        }
    }
}

void
DUET::GetNewGUISize(int guiSizeIdx, int *width, int *height)
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
DUET::PreResizeGUI(int guiSizeIdx,
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
    mDUETObj->SetSpectrogramDisplay(NULL);

    mImageDisplay = NULL;
    mDUETObj->SetImageDisplay(NULL);
    
    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
DUET::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium, kGUISizeBig };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
    { mGUISizeSmallButton, mGUISizeMediumButton,
      mGUISizeBigButton, mGUISizePortraitButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   NUM_GUI_SIZES);
}

void
DUET::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mDUETObj == NULL))
        return;
    
    //BL_FLOAT sampleRate = GetSampleRate();
    
    mSpectrogram = mDUETObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    //dispSpec->SetYLogScale(false, 1.0);
    //mSpectrogram->SetYLogScale(true); //, Y_LOG_SCALE_FACTOR);
    mSpectrogram->SetYScale(Scale::LOG);
    mSpectrogram->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        mSpectrogramDisplay = new SpectrogramDisplay2(mSpectrogramState);
        mSpectrogramState = mSpectrogramDisplay->GetState();
        
        mSpectrogramDisplay->SetBounds(0.0, 0.0, 1.0, 1.0);
        mSpectrogramDisplay->SetSpectrogram(mSpectrogram);
    
        mDUETObj->SetSpectrogramDisplay(mSpectrogramDisplay);
    
        mGraph->AddCustomDrawer(mSpectrogramDisplay);
    }
}


void
DUET::CreateImageDisplay(bool createFromInit)
{
    if (!createFromInit && (mDUETObj == NULL))
        return;
    
    mImage = mDUETObj->GetImage();
    
    if (mGraph != NULL)
    {
        mImageDisplay = new ImageDisplay2(ImageDisplay2::MODE_NEAREST);
        mImageDisplay->SetBounds(0.0, 0.0, 0.5, 1.0);
        mImageDisplay->SetImage(mImage);
    
        mDUETObj->SetImageDisplay(mImageDisplay);
        
        mGraph->AddCustomDrawer(mImageDisplay);
    }
}

void
DUET::SetPickCursorActive(bool flag)
{
    mGraphDrawer->SetPickCursorActive(flag);
    
    if (mDUETObj != NULL)
        mDUETObj->SetPickingActive(flag);
}

void
DUET::SetPickCursor(int x, int y)
{
    SetPickCursorActive(true);
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    double xf = ((double)x)/width;
    double yf = ((double)y)/height;
    
    mGraphDrawer->SetPickCursor(xf, yf);
    
    if (mDUETObj != NULL)
        mDUETObj->SetPickPosition(xf, yf);
}

void
DUET::SetInvertPickSelection(bool flag)
{
    if (mDUETObj != NULL)
        mDUETObj->SetInvertPickSelection(flag);
}
