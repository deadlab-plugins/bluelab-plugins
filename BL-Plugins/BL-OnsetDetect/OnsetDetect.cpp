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
using namespace std;

#include <GUIHelper12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include <FftProcessObj16.h>
#include <OnsetDetectProcess.h>

#include "OnsetDetect.h"

#include "IPlug_include_in_plug_src.h"

//
#define USE_VARIABLE_BUFFER_SIZE 1

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

// CHECK: maybe it could work well with 1024 buffer ?
#define BUFFER_SIZE 2048

// With 32, result is better (less low freq vibrations in noise)
#define OVERSAMPLING 8 //4

#define FREQ_RES 1

#define KEEP_SYNTHESIS_ENERGY 0

// When set to 0, we are accurate for frequencies
#define VARIABLE_HANNING 0

#if 0
TODO: the protools signe GUID is not up to date, need to generate a wrap config
#endif
 
enum EParams
{
    kThreshold = 0,
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kThresholdX = 192,
    kThresholdY = 52,
    kThresholdFrames = 180,
    
    kLogoAnimFrames = 31,
};

//
OnsetDetect::OnsetDetect(const InstanceInfo &info)
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
    
    //MakeDefaultPreset((char *) "-", kNumPrograms);
    
    BL_PROFILE_RESET;
}

OnsetDetect::~OnsetDetect()
{
    if (mFftObj != NULL)
        delete mFftObj;
  
    if (mOnsetDetectProcessObj != NULL)
        delete mOnsetDetectProcessObj;
}

IGraphics *
OnsetDetect::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
OnsetDetect::MyMakeLayout(IGraphics *pGraphics)
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

#ifdef __linux__
    pGraphics->AttachTextEntryControl();
#endif
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
        
    CreateControls(pGraphics);

    ApplyParams();
      
    // Demo mode
    mDemoManager.Init(this, pGraphics);
        
    mUIOpened = true;
        
    LEAVE_PARAMS_MUTEX;  
}

void
OnsetDetect::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    // Init WDL FFT
    FftProcessObj16::Init();
  
    mUIOpened = false;
    mControlsCreated = false;

    mFftObj = NULL;
    mOnsetDetectProcessObj = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
OnsetDetect::Init(int oversampling, int freqRes)
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
  
    int bufferSize = BUFFER_SIZE;
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        int numChannels = 1;
        int numScInputs = 0;
    
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < numChannels; i++)
        {
            mOnsetDetectProcessObj = new OnsetDetectProcess(bufferSize,
                                                            oversampling, freqRes,
                                                            sampleRate);
            mOnsetDetectProcessObj->SetThreshold(0.94);
        
            processObjs.push_back(mOnsetDetectProcessObj);
        }
      
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      bufferSize, oversampling, freqRes,
                                      sampleRate);
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
        //BL_FLOAT sampleRate = GetSampleRate();
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }

    // NEW
    int blockSize = GetBlockSize();
    int latency = mFftObj->ComputeLatency(blockSize);
    SetLatency(latency);
    
    mIsInitialized = true;
}

void
OnsetDetect::InitParams()
{
    // Threshold
    BL_FLOAT defaultThreshold = 94.0;
    mThreshold = defaultThreshold/100.0;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, 0.0, 100.0, 0.1, "%");
}

void
OnsetDetect::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
    if (!mIsInitialized)
        return;

    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();
    
    // For midi
    //GetTime(&mTimeInfo);

    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > &scIn = mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf2;
    BLUtilsPlug::GetPlugIOBuffers(this, inputs, outputs, nFrames,
                              &in, &scIn, &out);
  
    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
        return;
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
  
    mSecureRestarter.Process(in);

    //
    mFftObj->Process(in, scIn, &out);
  
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
OnsetDetect::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);

    mGUIHelper->CreateKnob(pGraphics,
                           kThresholdX, kThresholdY,
                           KNOB_FN,
                           kThresholdFrames,
                           kThreshold,
                           TEXTFIELD_FN,
                           "THRESHOLD",
                           GUIHelper12::SIZE_BIG);

    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR); //, GUIHelper12::BOTTOM);
    
    // Logo
    //guiHelper.CreateLogo(this, pGraphics, LOGO_FN, GUIHelper11::BOTTOM);
    mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
                               kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN, GUIHelper12::BOTTOM);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM);
  
    mGUIHelper->CreateDemoMessage(pGraphics);
  
    mControlsCreated = true;
}

void
OnsetDetect::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    int bufferSize = BUFFER_SIZE;
    double sampleRate = GetSampleRate();
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
#endif
    
    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset
    mFftObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
  
    mOnsetDetectProcessObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
  
    // NEW
    int blockSize = GetBlockSize();
    int latency = mFftObj->ComputeLatency(blockSize);
    SetLatency(latency);

  
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
OnsetDetect::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kThreshold:
        {
            BL_FLOAT value = GetParam(kThreshold)->Value();
            BL_FLOAT threshold = value/100.0;
            mThreshold = threshold;
            
            if (mOnsetDetectProcessObj != NULL)
                mOnsetDetectProcessObj->SetThreshold(threshold);
        }       
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
OnsetDetect::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
OnsetDetect::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
OnsetDetect::ApplyParams()
{
    if (mOnsetDetectProcessObj != NULL)
        mOnsetDetectProcessObj->SetThreshold(mThreshold);
}
