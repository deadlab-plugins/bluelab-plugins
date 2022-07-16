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

#include <FftProcessObj16.h>
#include <StereoDeReverbProcess.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include <ParamSmoother2.h>

#include "StereoDeReverb.h"

#include "IPlug_include_in_plug_src.h"

//
#define USE_VARIABLE_BUFFER_SIZE 1

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

// CHECK: maybe it could work well with 1024 buffer ?
#define BUFFER_SIZE 2048

// With 32, result is better (less low freq vibrations in noise)
#define OVERSAMPLING 4 //4 //32

#define FREQ_RES 1

#define KEEP_SYNTHESIS_ENERGY 0

// When set to 0, we are accurate for frequencies
#define VARIABLE_HANNING 0

//
#if 0
TODO: test if it works to supress bleeding

NOTE: when reverb only, and treshold is 0, there is still sound!
=> this should be samples that are out of bounds of the spectrogram!

NOTE: NOTE: BlueLab-StereoDeReverb-TestReaper-VST2-DeReverb.RPP => beginning of the sound removed when selecting dry signal => but it is because it is reverb signal ! (this is not a bug!)

NOTE: AUTOTESTS: There is sometimes isolated formants at the early beginning of the bounce.
But this is due to the Reaper autotest: we bounce while we are still playing (in loop)
then reset is not called, and it draws some additional formants. This shouldn t be a problem for normal use.

IDEA: record IR corresponding to the removed reverb, and save it!


======= NOTES TO BE REMOVED AFTER PLUGIN RELEASE ========
TODO: write a very simple quick start guide on the plugin page

"this plugin removes the reverb of a stereo sound recorded in a reverberant environment"
- very simple GUI, but in-depth sound processing
"sounds focused in the stereo field, and sounds widely spread in the stereo field"

"it can remove breath an sibiliance if threshold is too high" => "set up the threshold well before all other thing"

"the sound must have been recorded in stereo" "won t work on mono sounds (played on two channels)"

" multiple sources ?"

"the plugin can also be used to remove hiss on stereo sounds!"
=> test with 2 sound sources + reverb

"the plugin is more efficient on reverb tails than on early reflexions"

"to remove hiss: either "reverb only" + set yhe threshold, or turn mix to 0%, then increase progressively the thredhold"
until the hiss disappears.

add spectrograms and panograms in appendix in the doc, and add to the plug page too!
=============================================================
#endif

enum EParams
{
    kThreshold = 0,
    kMix,
    kOutGain,
    kReverbOnly,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,
    
    kThresholdX = 80,
    kThresholdY = 41,
    kThresholdFrames = 180,
    
    kMixX = 192,
    kMixY = 52,
    kMixFrames = 180,
    
    kOutGainX = 336,
    kOutGainY = 88,
    kOutGainFrames = 180,
    
    kReverbOnlyX = 90,
    kReverbOnlyY = 134,
    
    kLogoAnimFrames = 31
};

//
StereoDeReverb::StereoDeReverb(const InstanceInfo &info)
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

StereoDeReverb::~StereoDeReverb()
{
    if (mFftObj != NULL)
        delete mFftObj;

    if (mDeReverbObj != NULL)
        delete mDeReverbObj;

    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
StereoDeReverb::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
StereoDeReverb::MyMakeLayout(IGraphics *pGraphics)
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
StereoDeReverb::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;

    mFftObj = NULL;

    mDeReverbObj = NULL;
        
    mOutGainSmoother = NULL;
  
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
StereoDeReverb::Init(int oversampling, int freqRes)
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    BL_FLOAT defaultGain = 1.0; // 1 is 0dB
    mOutGain = defaultGain;

    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultGain);
  
    int bufferSize = BUFFER_SIZE;
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);

    // OLD (failed)
    //BLUtilsPlug::PlugUpdateLatency(this, BUFFER_SIZE, PLUG_LATENCY, sampleRate);
#endif
    
    if (mFftObj == NULL)
    {
        int numChannels = 2;
        int numScInputs = 0;
    
        vector<ProcessObj *> processObjs;
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
      
        //
        mDeReverbObj = new StereoDeReverbProcess(BUFFER_SIZE, oversampling, freqRes,
                                                 sampleRate);
      
        // Must artificially call reset because MultiProcessObjs does not reset
        // when FftProcessObj resets
        mDeReverbObj->Reset(BUFFER_SIZE, oversampling, freqRes,
                            sampleRate);
      
        mFftObj->AddMultichannelProcess(mDeReverbObj);
    }
    else
    {
        mFftObj->Reset(bufferSize, oversampling, freqRes, sampleRate);
    }

    UpdateLatency();
  
    mIsInitialized = true;
}

void
StereoDeReverb::InitParams()
{
    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 1 is 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");
    
    // Threshold
    BL_FLOAT defaultThreshold = 0.0;
    mThreshold = defaultThreshold;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, 0.0, 100.0, 0.01, "%");

    // Mix
    BL_FLOAT defaultMix = 0.0;
    mMix = defaultMix;
    GetParam(kMix)->InitDouble("Mix", defaultMix, -100.0, 100.0, 0.1, "%");

    mReverbOnly = false;
    GetParam(kReverbOnly)->InitInt("ReverbOnly", 0, 0, 1);
}

void
StereoDeReverb::ProcessBlock(iplug::sample **inputs,
                             iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
    if (!mIsInitialized)
        return;
    
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
        return;
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
    
    mSecureRestarter.Process(in);
    
    mFftObj->Process(in, scIn, &out);
    
    // Apply output gain
    for (int i = 0; i < nFrames; i++)
    {
        BL_FLOAT gain = mOutGainSmoother->Process();
          
        out[0].Get()[i] *= gain;
        
        if (out.size() > 1)
            out[1].Get()[i] *= gain;
    }
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
StereoDeReverb::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);

    // Threshold
    mGUIHelper->CreateKnob(pGraphics,
                           kThresholdX, kThresholdY,
                           KNOB_SMALL_FN,
                           kThresholdFrames,
                           kThreshold,
                           TEXTFIELD_FN,
                           "THRS",
                           GUIHelper12::SIZE_SMALL);

    // 
    mGUIHelper->CreateKnob(pGraphics,
                           kMixX, kMixY,
                           KNOB_FN,
                           kMixFrames,
                           kMix,
                           TEXTFIELD_FN,
                           "MIX",
                           GUIHelper12::SIZE_SMALL);

    // Out gain
    mGUIHelper->CreateKnob(pGraphics,
                           kOutGainX, kOutGainY,
                           KNOB_SMALL_FN,
                           kOutGainFrames,
                           kOutGain,
                           TEXTFIELD_FN,
                           "OUT GAIN",
                           GUIHelper12::SIZE_SMALL);

    // Our reverb only
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kReverbOnlyX,
                                   kReverbOnlyY,
                                   CHECKBOX_FN,
                                   kReverbOnly,
                                   "REVERB ONLY",
                                   GUIHelper12::SIZE_SMALL);
  
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
StereoDeReverb::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    int bufferSize = BUFFER_SIZE;
    BL_FLOAT sampleRate = GetSampleRate();
    
#if USE_VARIABLE_BUFFER_SIZE
    bufferSize = BLUtilsPlug::PlugComputeBufferSize(BUFFER_SIZE, sampleRate);
    
    // OLD (fails a little sometimes)
    //BLUtilsPlug::PlugUpdateLatency(this, BUFFER_SIZE, PLUG_LATENCY, sampleRate);
#endif
    
    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset
    if (mFftObj != NULL)
        mFftObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);

    if (mDeReverbObj != NULL)
        mDeReverbObj->Reset(bufferSize, OVERSAMPLING, FREQ_RES, sampleRate);
  
    UpdateLatency();
  
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
StereoDeReverb::OnParamChange(int paramIdx)
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
        
            // Better to set the param shape here
            threshold = BLUtils::ApplyParamShape(threshold, 1.0/4.0);
            mThreshold = threshold;

            if (mDeReverbObj != NULL)
                mDeReverbObj->SetThreshold(threshold);
        }
        break;
    
        case kMix:
        {
            BL_FLOAT mix = GetParam(kMix)->Value();
            mix /= 100.0;
            mMix = mix;

            if (mDeReverbObj != NULL)
                mDeReverbObj->SetMix(mix);
        }
        break;

        case kReverbOnly:
        {
            int value = GetParam(kReverbOnly)->Value();
            bool outputReverbOnly = (value == 1);
            mReverbOnly = outputReverbOnly;

            if (mDeReverbObj != NULL)
                mDeReverbObj->SetOutputReverbOnly(outputReverbOnly);
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
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoDeReverb::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    LEAVE_PARAMS_MUTEX;
}

void
StereoDeReverb::OnUIClose()
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
StereoDeReverb::ApplyParams()
{
    if (mDeReverbObj != NULL)
        mDeReverbObj->SetThreshold(mThreshold);
    
    if (mDeReverbObj != NULL)
        mDeReverbObj->SetMix(mMix);
    
    if (mDeReverbObj != NULL)
        mDeReverbObj->SetOutputReverbOnly(mReverbOnly);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
        
}

void
StereoDeReverb::UpdateLatency()
{
    int blockSize = GetBlockSize();
    int latency = mFftObj->ComputeLatency(blockSize);
    
    int lat2 = mDeReverbObj->GetAdditionalLatency();
    
    latency += lat2;
    
    SetLatency(latency);
}
