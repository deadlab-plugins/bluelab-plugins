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
#include <algorithm>
using namespace std;

#include <FftProcessObj16.h>

#include <GUIHelper12.h>
#include <GraphControl12.h>
#include <SecureRestarter.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLUtilsFile.h>
#include <BLUtilsDecim.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <GraphTimeAxis6.h>
#include <GraphFreqAxis2.h>

#include <GraphAxis2.h>

#include <DiracGenerator.h>
#include <AudioFile.h>

#include <BLBitmap.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Impulses.h"

#include <FastRTConvolver3.h>

#include <ImpulseResponseExtractor.h>
#include <ImpulseResponseSet.h>

#include <ParamSmoother2.h>

#include "IPlug_include_in_plug_src.h"

// Tests to return to the previous button after reset is choosen
// Doesn't work well with Protools (crash, stuck on reset)
#define USE_RESET_TRIGGER 0

#define BUFFER_SIZE 1024

// Keep a margin, just in case
//#define DIRAC_VALUE 0.8
#define DIRAC_VALUE 1.0

// Wait a moment before bouncing
// because hosts often make fades at the beginning of the playing
// Wait one second
#define BOUNCE_DELAY_SEC 1.0

// Y axis

// Factor to avoid touching the limits with the peaks of the response
#define AMP_FACTOR 0.9
#define MIN_AMP -1.0/AMP_FACTOR
#define MAX_AMP 1.0/AMP_FACTOR


// FIX: set from 1024 to 4096
// With 1024, the current impulse was not displayed in the graph during processing
// if IR size was >= 5s
//
// 1024 will be sufficient considering the width of the graph in pixels
//#define MAX_SAMPLES_DECIMATED 1024
#define MAX_SAMPLES_DECIMATED 4096

// Set this to 1 to avoid freezing Protools when opening
// system file selector
#define USE_FILE_SELECTOR_CONTROL 1

// Notify the host to propose to save the project,
// after we have loaded IR
#define FIX_LOAD_IR_SAVE_PROJECT 1

// Make the fade on the native IR which has just been created
#define FADE_NATIVE_IR 1

// "Denoise" the IRs at the native level
#define DENOISE_NATIVE_IR 1

// Avoid having the second channel to 0
#define FIX_MONO_CHANNEL 1

#define FIX_GAIN_COEFF 1
#define REF_SAMPLE_RATE_GAIN 44100.0

// IMPROV: when capturing, the IR size seems good (near 1),
// but when switching to Apply, it decreases on the graph
//
// In case of stereo IRs, display the max of the two IRs
//
// In the case of 1 small left channel IR and 1 bug right channel IR,
// this avoids capturing and displaying a big IR, then
// when switching to apply, displaying a small IR
#define DISPLAY_MONO_MAX_RESPONSE 1

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

// FIX: Since GHOST_OPTIM_GL, the colors were swapped in the background image
// NOTE: for compatibility, the serialized images will still have the
// same color order as before
// NOTE2: when loading tyhe plugin, the preferences are unserialized,
// then re-serialized each time
#define FIX_BG_IMAGE_COLOR_SWAP 1

// FIX the gain that is 30x too high when using USE_FAST_RT_CONVOLVER
//#define OUTPUT_GAIN_COEFF 1.0
#define OUTPUT_GAIN_COEFF 1.0/30.0

// Avoid shifting start of IR by 1s on IR with length 10s
#define PROCESS_NATIVE_IR_NO_ALIGN 1

#define BEVEL_CURVES 1

#define MAX_NUM_TIME_AXIS_LABELS 10

// Tmp...
// Later: will have to measure the button width, and add width/2 to title x
#define TITLE_TEXT_OFFSET 35

// Hack!
// Must hard code, because we need it when unserializing,
// and the graph may have not yet been created
#define GRAPH_WIDTH 664 //648
#define GRAPH_HEIGHT 290

#define USE_DROP_DOWN_MENU 1

#define USE_LEGACY_LOCK 1

// NOTE: the following fix is useless (for vst2)
//
// This is now fixed directly in IRolloverButtonControl (FIX_HILIGHT_REOPEN)
// (the only way to fix it correctly)
// And if we set the following flag to 1, the fix FIX_HILIGHT_REOPEN
// won't work anymore

// BAD (for vst2)
// FIX: Linux, vst3: open file selector, click cancel
// => sometimes the file selector re-opened
#ifndef VST3_API
#define FIX_FILE_SELECTOR_REOPEN 0
#else
// Necessary for VST3
#define FIX_FILE_SELECTOR_REOPEN 1
#endif

// Win10/VST2/VST3: freeze when opening file selector
#ifdef WIN32
#define FIX_FILE_SELECTOR_FREEZE_WINDOWS 1
#else
#define FIX_FILE_SELECTOR_FREEZE_WINDOWS 0
#endif

#if 0
/*
TODO: re-tests everything very well!!

- BIG BUG: preferences are not saved (Mac/win only?)
(this is working on Linux!)

NOTE (not yet in the manual): to hae better performances when applying use
big buffer size e.g 20
(buffer size 2048 more than 2x more performant than buffer size 256)

TODO: check RT (what appends if block size is not power of two ?)

Crash: "BTW the plug crashed Cubase 9.5 and Ableton 9 on Windows 10. 
On MAC OSX 10.13 all seams to be fine"

NOTE: in the version with image (5.1.0), the presets are not compatible
with the previous presets. If we try to load an older preset, it just won t load,
keeping the previous active preset

PROBLEM: echo ghost, that remains and increases
Test with Impulses-Example2-2
This is only in "Tunnel" example, due to the sound captured in the place (cars passing etc.)                                    

NOTE: for Logic, there must be a dummy clip, as long as the total capture time
*/
#endif

static char *tooltipHelp = "Help - Display Help";
static char *tooltipAction = "Action - Current action";
static char *tooltipResponseLength = "IR Length - IR processing length";
static char *tooltipInGain = "In Gain - Input gain applied when processing input IRs";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipImageFileOpenParam = "Image File Open";
static char *tooltipImageFileResetParam = "Image Reset";
static char *tooltipIRFileOpenParam = "IR File Open";
static char *tooltipIRFileSaveParam = "IR File Save";

enum EParams
{
    kAction = 0,
    kResponseLength,
    kInGain,
    kOutGain,
    kImageFileOpenParam,
    kImageFileResetParam,
    kIRFileOpenParam,
    kIRFileSaveParam,

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
    
    kRadioButtonActionX = 43,
    kRadioButtonActionY = 335,
    kRadioButtonActionVSize = 92,
    kRadioButtonActionNumButtons = 4,

#if !USE_DROP_DOWN_MENU
    kRadioButtonResponseLengthX = 220,
    kRadioButtonResponseLengthY = 338,
    kRadioButtonResponseLengthVSize = 108,
    kRadioButtonResponseLengthNumButtons = 5,
#else
    kResponseLengthX = 189,
    kResponseLengthY = 327,
    kResponseLengthWidth = 80,
#endif
    
    kInGainX = 284,
    kInGainY = 344,

    kOutGainX = 591,
    kOutGainY = 344,
    
    // Image
    kImageFileOpenX = 463,
    kImageFileOpenY = 331,
  
    kImageFileResetX = 463,
    kImageFileResetY = 359,
  
    // IR
    kIRFileOpenX = 345,
    kIRFileOpenY = 331,
  
    kIRFileSaveX = 345,
    kIRFileSaveY = 359
};

//
Impulses::Impulses(const InstanceInfo &info)
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

Impulses::~Impulses()
{
    // Axes    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mVAxis != NULL)
        delete mVAxis;
    
    // Curves
    delete mInputSignalCurve;
    delete mInstantResponseCurve;
    delete mAvgResponseCurve;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    for (int i = 0; i < 2; i++)
    {
        if (mConvolvers[i] != NULL)
            delete mConvolvers[i];
    }
  
    for (int i = 0; i < 2; i++)
    {
        if (mDiracGens[i] != NULL)
            delete mDiracGens[i];
    }
 
    for (int i = 0; i < 2; i++)
    {
        if (mRespExtractors[i] != NULL)
            delete mRespExtractors[i];
    }
  
    if (mRespExtractorsDecimated != NULL)
        delete mRespExtractorsDecimated;
  
    for (int i = 0; i < 2; i++)
    {
        if (mResponseSets[i] != NULL)
            delete mResponseSets[i];
    }
  
    if (mResponseSetsDecimated != NULL)
        delete mResponseSetsDecimated;
  
    delete mInGainSmoother;
    delete mOutGainSmoother;

    if (mBgImageBitmap != NULL)
        delete mBgImageBitmap;
}

IGraphics *
Impulses::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    mGraphics = graphics;
    
    return graphics;
}

void
Impulses::MyMakeLayout(IGraphics *pGraphics)
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
Impulses::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;

    mGraphics = NULL;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mGraph = NULL;
    mGraphWidthPixels = 0;
    
    mHAxis = NULL;
    mVAxis = NULL;
    
    mPrevSampleRate = GetSampleRate();

    mHAxis = NULL;
    mVAxis = NULL;
    
    mInputSignalCurve = NULL;
    mInstantResponseCurve = NULL;
    mAvgResponseCurve = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
    
    mConvolvers[0] = NULL;
    mConvolvers[1] = NULL;
  
    mDiracGens[0] = NULL;
    mDiracGens[1] = NULL;
    
    mRespExtractors[0] = NULL;
    mRespExtractors[1] = NULL;
  
    mRespExtractorsDecimated = NULL;
  
    mResponseSets[0] = NULL;
    mResponseSets[1] = NULL;
  
    mResponseSetsDecimated = NULL;
    
    mBgImageBitmap = NULL;
  
    mMinXAxisValue = 0.0;
    mMaxXAxisValue = 1.0;
    
    // No more used ?
    mPrevSampleRate = -1.0;
  
    mGainCoeff = 1.0;

    mInGainSmoother = NULL;
    mInGainKnob = NULL;
    
    mOutGainSmoother = NULL;
    mOutGainKnob = NULL;
        
    // By default
    mNativeSampleRate = GetSampleRate();

    memset(mCurrentLoadPathIR, '\0', FILENAME_SIZE);
    memset(mCurrentSavePathIR, '\0', FILENAME_SIZE);
    memset(mCurrentFileNameIR, '\0', FILENAME_SIZE);
    memset(mCurrentLoadPathImg, '\0', FILENAME_SIZE);

    // Must update image in the GUI thread (OnIdle()) 
    mMustUpdateBGImage = false;

    // For FIX_FILE_SELECTOR_REOPEN
    mImageOpenControl = NULL;
    mIROpenControl = NULL;
    mIRSaveControl = NULL;

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
    mIsPromptingForFile = false;
#endif
}

void
Impulses::InitParams()
{
    // Action
    Action defaultAction = GEN_IMPULSES;
    mAction = defaultAction;
    //GetParam(kAction)->InitInt("Action", (int)defaultAction, 0, 3);
    GetParam(kAction)->InitEnum("Action", (int)defaultAction, 4,
                                "", IParam::kFlagsNone, "",
                                "Generate Impulses", "Process IR",
                                "Apply IR", "Reset");

    GrayOutGainKnobs();
    
    // Response Length
    ResponseLength defaultResponseLength = LENGTH_50MS;
    mResponseLength = defaultResponseLength;

#if !USE_DROP_DOWN_MENU
    GetParam(kResponseLength)->InitInt("ResponseLength",
                                       (int)defaultResponseLength, 0, 4);
#else
    GetParam(kResponseLength)->InitEnum("ResponseLength", 0, 5, "",
                                        IParam::kFlagsNone,
                                        "", "50 ms", "100 ms", "1 s", "5 s", "10 s");
#endif
    
    // Input gain
    BL_FLOAT defaultInGain = 0.0;
    mInGain = defaultInGain;
    GetParam(kInGain)->InitDouble("InGain", defaultInGain, -24.0, 24.0, 0.1, "dB");

    // Output gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain,
                                   -24.0, 24.0, 0.1, "dB");
    
    // Image file open
    GetParam(kImageFileOpenParam)->InitInt("ImageFileOpen", 0, 0, 1,
                                           "", IParam::kFlagMeta);
    
    // Image file "reset"
    GetParam(kImageFileResetParam)->InitInt("ImageFileReset", 0, 0, 1,
                                            "", IParam::kFlagMeta);
    
    // IR file open
    GetParam(kIRFileOpenParam)->InitInt("IRFileOpen", 0, 0, 1, "", IParam::kFlagMeta);
    
    // IR file "reset"
    GetParam(kIRFileSaveParam)->InitInt("IRFileSave", 0, 0, 1, "", IParam::kFlagMeta);
}

void
Impulses::ApplyParams()
{
    ActionChanged(mAction);

    ResponseLengthChanged();

    if (mInGainSmoother != NULL)
        mInGainSmoother->ResetToTargetValue(mInGain);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
    
    mMustUpdateBGImage = true;
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Impulses::Init()
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    for (int i = 0; i < 2; i++)
    {
        if (mConvolvers[i] == NULL)
        {
            const WDL_TypedBuf<BL_FLOAT> dummyIr;
            mConvolvers[i] = new FastRTConvolver3(BUFFER_SIZE, sampleRate, dummyIr);
        }
    }
  
    for (int i = 0; i < 2; i++)
        mDiracGens[i] = new DiracGenerator(sampleRate, mDiracFrequency,
                                           DIRAC_VALUE, 0);
  
    long numSamplesResponse = GetRespNumSamples();
    for (int i = 0; i < 2; i++)
    {
        mResponseSets[i] = new ImpulseResponseSet(numSamplesResponse, sampleRate);
    }
  
    mResponseSetsDecimated = new ImpulseResponseSet(MAX_SAMPLES_DECIMATED,
                                                    sampleRate);
  
    for (int i = 0; i < 2; i++)
        mRespExtractors[i] = new ImpulseResponseExtractor(numSamplesResponse,
                                                          sampleRate, 1.0);
  
    BL_FLOAT decimationFactor = ((BL_FLOAT)numSamplesResponse)/MAX_SAMPLES_DECIMATED;
    mRespExtractorsDecimated = new ImpulseResponseExtractor(MAX_SAMPLES_DECIMATED,
                                                            sampleRate,
                                                            decimationFactor);
  
    // Smoothers
    BL_FLOAT defaultInGain = 1.0;
    mInGainSmoother = new ParamSmoother2(sampleRate, defaultInGain);

    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
    
    // Response Length
    ResponseLength defaultResponseLength = LENGTH_50MS;
    SetConfig(defaultResponseLength);
    
    ApplyParams();
    
    UpdateTimeAxis();

    ApplyConfig();
    
    DoReset(true);
  
    mIsInitialized = true;
}

void
Impulses::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
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
        
        if (mGraph != NULL)
            mGraph->PushAllData();
        
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

    // Process the sound every time
    // Even when appying IR => so we behave as a standard reverb plugin
    // (reverb continues after stopping transport)
          
    // Gen impulses
    if (mAction == GEN_IMPULSES)
    {
        for (int i = 0; i < 2; i++)
        {
            if (out.size() > i)
            {
                if (mDiracGens[i] != NULL)
                    mDiracGens[i]->Process(out[i].Get(), nFrames);
            }
        }
    }
      
    if (mAction == PROCESS_RESPONSES)
    {
        if (!mProcessingStarted)
        {
            // First time we start processing, reset
            // (we must do it because response length may have changed)
            DoReset(true);
            
            mProcessingStarted = true;
        }
        
        // Apply input gain
        BLUtilsPlug::ApplyGain(in, &in, mInGainSmoother);
        
        WDL_TypedBuf<BL_FLOAT> &monoIn = mTmpBuf5;
        monoIn = in[0];
        if (in.size() > 1)
            BLUtils::StereoToMono(&monoIn, in[0], in[1]);
        
        // Draw the input signal in the graph
        UpdateInputSignalCurve(monoIn);
        
        // Output the sound of amplified input
        for (int i = 0; i < out.size(); i++)
        {
            if (i < in.size())
                out[i] = in[i];
        }
        
        //WDL_TypedBuf<BL_FLOAT> &responseReadyDecimated = mTmpBuf6;
        WDL_TypedBuf<BL_FLOAT> responseReadyDecimated;
        if (mRespExtractorsDecimated != NULL)
            mRespExtractorsDecimated->AddSamples(monoIn.Get(),
                                                 nFrames,
                                                 &responseReadyDecimated);
        
        WDL_TypedBuf<BL_FLOAT> responsesReady[2];
        //WDL_TypedBuf<BL_FLOAT> *responsesReady = mTmpBuf4;
        if (in.size() == 1)
        {
            // Single
            if (mRespExtractors[0] != NULL)
                mRespExtractors[0]->AddSamples(in[0].Get(),
                                               nFrames,
                                               &responsesReady[0]);
        }
        else
        {
            ImpulseResponseExtractor::AddSamples2(mRespExtractors[0],
                                                  mRespExtractors[1],
                                                  in[0].Get(), in[1].Get(),
                                                  nFrames,
                                                  &responsesReady[0],
                                                  &responsesReady[1]);
        }
        
        bool newResponseDecimAvail = (responseReadyDecimated.GetSize() > 0);
        
        if (newResponseDecimAvail)
        {
            // Decimated
            if (responseReadyDecimated.GetSize() > 0)
            {
                if (responseReadyDecimated.GetSize() > MAX_SAMPLES_DECIMATED)
                    // Limit the size !
                    responseReadyDecimated.Resize(MAX_SAMPLES_DECIMATED);
                
                // Add
                if (mResponseSetsDecimated != NULL)
                    mResponseSetsDecimated->
                        AddImpulseResponse(responseReadyDecimated);
            }
            
            BL_FLOAT sampleRate = GetSampleRate();
            
            //
            // Avg response
            //
            BL_FLOAT decimFactor =
                mRespExtractorsDecimated->GetDecimationFactor();
            long responseSize = GetRespNumSamples();
            ImpulseResponseSet::AlignImpulseResponsesAll(mResponseSetsDecimated,
                                                         responseSize,
                                                         decimFactor,
                                                         sampleRate);
            
            bool lastRespValid = true;
            bool iterate = true;
            ImpulseResponseSet::DiscardBadResponses(mResponseSetsDecimated,
                                                    &lastRespValid,
                                                    iterate);
            
            // Display the avg response
            WDL_TypedBuf<BL_FLOAT> avgDecimResp;
            if (mResponseSetsDecimated != NULL)
                mResponseSetsDecimated->GetAvgImpulseResponse(&avgDecimResp);
            
            // Update the color of the instant response curve
            // (red if it has been discarded)
            UpdateGraphInstantResponseValid(lastRespValid);
            
            // Normalize here...
            ImpulseResponseSet::NormalizeImpulseResponse(&avgDecimResp);
            
            // And re-make fades
            // (for the end, because we have decimated)
            ImpulseResponseSet::MakeFades(&avgDecimResp, MAX_SAMPLES_DECIMATED,
                                          decimFactor);
            
            UpdateGraphAvgResponse(avgDecimResp);
            
            //
            // Instant response
            //
            WDL_TypedBuf<BL_FLOAT> instantDecimResp;
            if (mResponseSetsDecimated != NULL)
                mResponseSetsDecimated->GetLastImpulseResponse(&instantDecimResp);
            
            // Normalize here...
            ImpulseResponseSet::NormalizeImpulseResponse(&instantDecimResp);
            
            // And make fades
            ImpulseResponseSet::MakeFades(&instantDecimResp,
                                          MAX_SAMPLES_DECIMATED,
                                          decimFactor);
            
            UpdateGraphInstantResponse(instantDecimResp);
        }

        // Full responses
        bool newResponseAvail = (responsesReady[0].GetSize() > 0);
        if (newResponseAvail)
        {
            // Resize and add the response(s)
            for (int i = 0; i < 2; i++)
            {
                // Full length
                if (responsesReady[i].GetSize() > 0)
                {
                    // Limit the size !
                    long respNumSamples = GetRespNumSamples();
                    if (responsesReady[i].GetSize() > respNumSamples)
                        responsesReady[i].Resize((int)respNumSamples);
                    
                    if (mResponseSets[i] != NULL)
                        mResponseSets[i]->AddImpulseResponse(responsesReady[i]);
                }
            }
        }
        
        if (newResponseDecimAvail || newResponseAvail)
        {
            // We have new data, we will have to precess
            mResponseNeedsProcess = true;
        }
    }

    // Apply
    if (mAction == APPLY_RESPONSE)
    {
        // NOTE: with that, the IR will be resampled only if we play the sound
        // (which could make inconsisteny when loading/saving prefs
        // with different SRs)
        
        // Apply the impulse response to signal, possibly stereo
        for (int i = 0; i < 2; i++)
        {
            WDL_TypedBuf<BL_FLOAT> *inBuf;
            WDL_TypedBuf<BL_FLOAT> *outBuf;
            bool success = BLUtilsPlug::GetIOBuffers(i, in, out, &inBuf, &outBuf);
            if (success)
            {
                if (mConvolvers[i] != NULL)
                    mConvolvers[i]->Process(*inBuf, outBuf);
                
                BLUtils::MultValues(outBuf, OUTPUT_GAIN_COEFF);
                
                // Multiply by a gain coeff, if the impulse has been resampled
                BLUtils::MultValues(outBuf, mGainCoeff);
            }
        }
    }

    // Apply output gain
    if ((mAction == GEN_IMPULSES) ||
        (mAction == APPLY_RESPONSE))
    {
        BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    }
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    
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
Impulses::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN/*kGraph*/);

    mGraph->SetUseLegacyLock(true);
    
    int height;
    mGraph->GetSize(&mGraphWidthPixels, &height);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);

    // Separator
    IColor sepIColor;
    mGUIHelper->GetGraphSeparatorColor(&sepIColor);
    int sepColor[4] = { sepIColor.R, sepIColor.G, sepIColor.B, sepIColor.A };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    CreateGraphAxes();
    CreateGraphCurves();

    mMustUpdateBGImage = true;
    
    const char *radioLabelsAction[kRadioButtonActionNumButtons] =
        { "GEN IMPULSES", "PROCESS RESPONSES",
          "APPLY AVG RESPONSE", "RESET" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonActionX,
                                   kRadioButtonActionY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonActionNumButtons,
                                   kRadioButtonActionVSize,
                                   kAction,
                                   false,
                                   "ACTION",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsAction,
                                   tooltipAction);

#if !USE_DROP_DOWN_MENU
    const char *radioLabelsResponseLength[kRadioButtonResponseLengthNumButtons] =
        { "50 ms", "100 ms", "1 s", "5 s", "10 s" };

    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonResponseLengthX,
                                   kRadioButtonResponseLengthY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonResponseLengthNumButtons,
                                   kRadioButtonResponseLengthVSize,
                                   kResponseLength,
                                   false,
                                   "RESP. LENGTH",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsResponseLength);
#else
    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kResponseLengthX, kResponseLengthY,
                                   kResponseLengthWidth,
                                   kResponseLength,
                                   "RESP. LENGTH",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipResponseLength);
#endif
    
    // Input gain
    mInGainKnob = mGUIHelper->CreateKnobSVG(pGraphics,
                                            kInGainX, kInGainY,
                                            kKnobSmallWidth, kKnobSmallHeight,
                                            KNOB_SMALL_FN,
                                            kInGain,
                                            TEXTFIELD_FN,
                                            "IN GAIN",
                                            GUIHelper12::SIZE_DEFAULT,
                                            NULL, true,
                                            tooltipInGain);

    // Output gain
    mOutGainKnob = mGUIHelper->CreateKnobSVG(pGraphics,
                                             kOutGainX, kOutGainY,
                                             kKnobSmallWidth, kKnobSmallHeight,
                                             KNOB_SMALL_FN,
                                             kOutGain,
                                             TEXTFIELD_FN,
                                             "OUT GAIN",
                                             GUIHelper12::SIZE_DEFAULT,
                                             NULL, true,
                                             tooltipOutGain);

    GrayOutGainKnobs();
    
    // Image
    mImageOpenControl =
        mGUIHelper->CreateRolloverButton(pGraphics,
                                         kImageFileOpenX, kImageFileOpenY,
                                         BUTTON_OPEN_FN,
                                         kImageFileOpenParam,
                                         NULL, false, true, false,
                                         tooltipImageFileOpenParam);

    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kImageFileResetX, kImageFileResetY,
                                     BUTTON_RESET_FN,
                                     kImageFileResetParam,
                                     NULL, false, true, false,
                                     tooltipImageFileResetParam);

#if 0
    mGUIHelper->CreateTitle(pGraphics,
                            kImageFileLabelX + TITLE_TEXT_OFFSET, kImageFileLabelY,
                            "IMAGE", GUIHelper12::SIZE_DEFAULT, EAlign::Center);
#endif
    
    // IR
    mIROpenControl =
        mGUIHelper->CreateRolloverButton(pGraphics,
                                         kIRFileOpenX, kIRFileOpenY,
                                         BUTTON_OPEN_FN,
                                         kIRFileOpenParam,
                                         NULL, false, true, false,
                                         tooltipIRFileOpenParam);

    mIRSaveControl =
        mGUIHelper->CreateRolloverButton(pGraphics,
                                         kIRFileSaveX, kIRFileSaveY,
                                         BUTTON_SAVE_FN,
                                         kIRFileSaveParam,
                                         NULL, false, true, false,
                                         tooltipIRFileSaveParam); //"Save...");

#if 0
    mGUIHelper->CreateTitle(pGraphics,
                            kIRFileLabelX + TITLE_TEXT_OFFSET, kIRFileLabelY,
                            "IR", GUIHelper12::SIZE_DEFAULT, EAlign::Center);
#endif
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
    // Logo
    //mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
    //                           kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN,
                               GUIHelper12::BOTTOM);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM,
                                 tooltipHelp);
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    mControlsCreated = true;
}

void
Impulses::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Impulses::OnReset()
{    
    TRACE;
    ENTER_PARAMS_MUTEX;
        
    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    // Reset sample rate
    BL_FLOAT sampleRate = GetSampleRate();

    if (mInGainSmoother != NULL)
        mInGainSmoother->Reset(sampleRate);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    
    // Response sets
    for (int i = 0; i < 2; i++)
    {
        if (mResponseSets[i] != NULL)
            mResponseSets[i]->SetSampleRate(sampleRate);
    }
    if (mResponseSetsDecimated != NULL)
        mResponseSetsDecimated->SetSampleRate(sampleRate);;
  
    // Extractors
    for (int i = 0; i < 2; i++)
    {
        if (mRespExtractors[i] != NULL)
            mRespExtractors[i]->SetSampleRate(sampleRate);
    }
    if (mRespExtractorsDecimated != NULL)
        mRespExtractorsDecimated->SetSampleRate(sampleRate);

#if FIX_GAIN_COEFF
    mGainCoeff = REF_SAMPLE_RATE_GAIN/sampleRate;
#endif
  
    // Without test
    ProcessNativeImpulses();

    WDL_TypedBuf<BL_FLOAT> &graphResp = mTmpBuf9;
    graphResp = mImpulses[0];
    
#if DISPLAY_MONO_MAX_RESPONSE
    GetMaxImpulseResponse(&graphResp, mImpulses);
#endif
  
    UpdateGraphAvgResponseDecim(graphResp);
  
    // Logic is calling Reset() when the playback stops
    // We don't want to reset the data when we stop the playback
    // and we are in process mode
    if (mAction == PROCESS_RESPONSES)
    {
        LEAVE_PARAMS_MUTEX;
        
        return;
    }

    if (mAction == APPLY_RESPONSE)
    {
        // We must reset if we apply response
        // Otherwise when bouncing a second time, there
        // will be residual data at the beginning of the
        // new bounced file
        for (int i = 0; i < 2; i++)
        {
            int blockSize = GetBlockSize();
            if (mConvolvers[i] != NULL)
                mConvolvers[i]->Reset(sampleRate, blockSize);
      
            int latency = 0;
            if (mConvolvers[0] != NULL)
                mConvolvers[0]->GetLatency();
            SetLatency(latency);
        }
    
        LEAVE_PARAMS_MUTEX;
        
        return;
    }
    
    DoReset(true);
  
    LEAVE_PARAMS_MUTEX;
        
    BL_PROFILE_RESET;
}

// OnIdle() is called from the GUI thread
void
Impulses::OnIdle()
{
    // Lock, to be sure not to update the image as it is currently serialized
    ENTER_PARAMS_MUTEX;

    if (mMustUpdateBGImage)
    {
        UpdateGraphBgImage();
        
        mMustUpdateBGImage = false;
    }

    LEAVE_PARAMS_MUTEX;
}

void
Impulses::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
    // Avoid a dead lock on Win10/VST3, when opening IR file while playing
    // (the file selector opens, and it is frozen)
    if (mIsPromptingForFile)
        return;
#endif
    
    ENTER_PARAMS_MUTEX;

    switch (paramIdx)
    {
        case kAction:
        {
            Action action = (Action)GetParam(paramIdx)->Int();
      
            Action prevAction = mAction;
            if (action != mAction)
            {
                mAction = action;
                
                ActionChanged(prevAction);
            }
        }
        break;
    
        case kResponseLength:
        {
            ResponseLength length = (ResponseLength)GetParam(paramIdx)->Int();
            
            if (length != mResponseLength)
            {
                mResponseLength = length;
                
                ResponseLengthChanged();
            }
        }
        break;
      
        case kInGain:
        {
            BL_FLOAT gain = GetParam(kInGain)->DBToAmp();
            mInGain = gain;
            if (mInGainSmoother != NULL)
                mInGainSmoother->SetTargetValue(gain);
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
                 
        case kImageFileOpenParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
#if FIX_FILE_SELECTOR_REOPEN
                // Ensure the value of the button is turned to 0
                // (because otherwise, it stays at 1 until we close the file selector,
                // and OnParamChange() could be called again during a refresh,
                // with value at 1
                if (mImageOpenControl != NULL)
                {
                    mImageOpenControl->SetValue(0.0);

                    // Necessary, for informing VST3 host
                    GUIHelper12::ResetParameter(this, kImageFileOpenParam, true);
                }
                //GetParam(kImageFileOpenParam)->Set(0.0);
#endif

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                mIsPromptingForFile = true;
                
                // Avoid a dead lock(maybe) (Win10/VST3)
                // Unlock the mutex the while the file selector is opened
                LEAVE_PARAMS_MUTEX;
#endif
                    
                WDL_String resFileName;
                bool fileOk =
                    BLUtilsFile::PromptForFileOpenImage(this,
                                                        mCurrentLoadPathImg,
                                                        &resFileName);

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                // Re-lock the mutex
                ENTER_PARAMS_MUTEX;

                mIsPromptingForFile = false;
#endif
                    
                if (fileOk)
                    OpenImageFile(resFileName.Get());
            }
        }
        break;
      
        // Image file reset
        case kImageFileResetParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                ResetImageFile();
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
      
        case kIRFileOpenParam:
        {
            int value = GetParam(paramIdx)->Value();

            if (value == 1)
            {
#if FIX_FILE_SELECTOR_REOPEN
                // Ensure the value of the button is turned to 0
                // (because otherwise, it stays at 1 until we close the file selector,
                // and OnParamChange() could be called again during a refresh,
                // with value at 1
                if (mIROpenControl != NULL)
                {
                    mIROpenControl->SetValue(0.0);

                    // Necessary, for informing VST3 host
                    GUIHelper12::ResetParameter(this, kIRFileOpenParam, true);
                }
                //GetParam(kIRFileOpenParam)->Set(0.0);
#endif

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                mIsPromptingForFile = true;
                
                // Avoid a dead lock (sure!)(Win10/VST3)
                // Unlock the mutex the while the file selector is opened
                LEAVE_PARAMS_MUTEX;
#endif
                    
                WDL_String resFileName;
                bool fileOk =
                    BLUtilsFile::PromptForFileOpenAudio(this,
                                                        mCurrentLoadPathIR,
                                                        &resFileName);

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                // Re-lock the mutex
                ENTER_PARAMS_MUTEX;

                mIsPromptingForFile = false;
#endif
                if (fileOk)
                    OpenIRFile(resFileName.Get());
            }
        }
        break;
        
        case kIRFileSaveParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
#if FIX_FILE_SELECTOR_REOPEN
                // Ensure the value of the button is turned to 0
                // (because otherwise, it stays at 1 until we close the file selector,
                // and OnParamChange() could be called again during a refresh,
                // with value at 1
                if (mIRSaveControl != NULL)
                {
                    mIRSaveControl->SetValue(0.0);

                    // Necessary, for informing VST3 host
                    GUIHelper12::ResetParameter(this, kIRFileSaveParam, true);
                }
                
                //GetParam(kIRFileSaveParam)->Set(0.0);
#endif

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                mIsPromptingForFile = true;
                
                // Avoid a dead lock(maybe) (Win10/VST3)
                // Unlock the mutex the while the file selector is opened
                LEAVE_PARAMS_MUTEX;
#endif
                WDL_String resFileName;
                bool fileOk =
                    BLUtilsFile::PromptForFileSaveAsAudio(this,
                                                          mCurrentLoadPathIR,
                                                          mCurrentSavePathIR,
                                                          mCurrentFileNameIR,
                                                          &resFileName);

#if FIX_FILE_SELECTOR_FREEZE_WINDOWS
                // Re-lock the mutex
                ENTER_PARAMS_MUTEX;

                mIsPromptingForFile = false;
#endif
                    
                if (fileOk)
                    SaveIRFile(resFileName.Get());
            }
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    UpdateTimeAxis();
    
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraphics = NULL;
    
    mGraph = NULL;

    mInGainKnob = NULL;
    mOutGainKnob = NULL;

    // For FIX_FILE_SELECTOR_REOPEN
    mImageOpenControl = NULL;
    mIROpenControl = NULL;
    mIRSaveControl = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::UpdateTimeAxis()
{    
    if (mHAxis != NULL)
    {
        mHAxis->SetMinMaxValues(mMinXAxisValue, mMaxXAxisValue);
        mHAxis->SetData(*mTimeAxisData, NUM_AXIS_DATA);
    }
}

void
Impulses::CreateGraphCurves()
{
    long numGraphValues = GetNumGraphValues();

    int curveDescriptionColor[4];
    mGUIHelper->GetGraphCurveDescriptionColor(curveDescriptionColor);
    
    if (mInputSignalCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int inputSignalCurveColor[4];
        mGUIHelper->GetGraphCurveColorBlue(inputSignalCurveColor);
        
        mInputSignalCurve = new GraphCurve5((int)numGraphValues);
        mInputSignalCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mInputSignalCurve->SetDescription("input signal", curveDescriptionColor);
        mInputSignalCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mInputSignalCurve->SetYScale(Scale::LINEAR, MIN_AMP, MAX_AMP);
        mInputSignalCurve->SetColor(inputSignalCurveColor[0],
                                    inputSignalCurveColor[1],
                                    inputSignalCurveColor[2]);
        mInputSignalCurve->SetLineWidth(1.0);
      
#if BEVEL_CURVES
        mInputSignalCurve->SetBevel(true);
#endif
    }

    if (mInstantResponseCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int instantResponseCurveColor[4];
        mGUIHelper->GetGraphCurveColorBlue(instantResponseCurveColor);
        
        mInstantResponseCurve = new GraphCurve5((int)numGraphValues);
        mInstantResponseCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mInstantResponseCurve->SetDescription("response (instant)",
                                              curveDescriptionColor);
        mInstantResponseCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mInstantResponseCurve->SetYScale(Scale::LINEAR, MIN_AMP, MAX_AMP);
        mInstantResponseCurve->SetColor(instantResponseCurveColor[0],
                                        instantResponseCurveColor[1],
                                        instantResponseCurveColor[2]);
        mInstantResponseCurve->SetLineWidth(1.0);
      
#if BEVEL_CURVES
        mInstantResponseCurve->SetBevel(true);
#endif
    }

    if (mAvgResponseCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int avgResponseCurveColor[4];
        mGUIHelper->GetGraphCurveColorLightBlue(avgResponseCurveColor);

        float fillAlpha = mGUIHelper->GetGraphCurveFillAlphaLight();
        
        mAvgResponseCurve = new GraphCurve5((int)numGraphValues);
        mAvgResponseCurve->SetUseLegacyLock(USE_LEGACY_LOCK);
        
        mAvgResponseCurve->SetDescription("response (avg)", curveDescriptionColor);
        mAvgResponseCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mAvgResponseCurve->SetYScale(Scale::LINEAR, MIN_AMP, MAX_AMP);
        mAvgResponseCurve->SetColor(avgResponseCurveColor[0],
                                    avgResponseCurveColor[1],
                                    avgResponseCurveColor[2]);
        mAvgResponseCurve->SetLineWidth(1.0);
        mAvgResponseCurve->SetFill(true);
        mAvgResponseCurve->SetFillAlpha(fillAlpha);
      
#if BEVEL_CURVES
        mAvgResponseCurve->SetBevel(true);
#endif
    }

    if (mGraph == NULL)
        return;
    
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
    
    mInputSignalCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mInputSignalCurve);
    
    mInstantResponseCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mInstantResponseCurve);
    
    mAvgResponseCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mAvgResponseCurve);
}

void
Impulses::CreateGraphAxes()
{
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);
    
    InitTimeAxis();
    
    // Freq axis
    //
    int axisColor[4];
    mGUIHelper->GetGraphAxisColor(axisColor);
    
    int axisLabelColor[4];
    mGUIHelper->GetGraphAxisLabelColor(axisLabelColor);
    
    int axisLabelOverlayColor[4];
    mGUIHelper->GetGraphAxisLabelOverlayColor(axisLabelOverlayColor);
    
    int width = 1;
    int height = 1;
    if (mGraph != NULL)
        mGraph->GetSize(&width, &height);
    
    float lineWidth = 1.0;
    mVAxis->InitVAxis(Scale::LINEAR,
                      0.0, 1.0,
                      axisColor, axisLabelColor,
                      lineWidth,
                      6.0/((BL_FLOAT)width),
                      0.0,
                      axisLabelOverlayColor);

#define NUM_SIG_AXIS_DATA 5
    static char *SIG_AXIS_DATA [NUM_SIG_AXIS_DATA][2] =
    {
        { "0.0", "-1.0" },
        { "0.25", "-0.5" },
        { "0.5", "0.0" },
        { "0.75", "0.5" },
        { "1.0", "1.0" }
    };

    mVAxis->SetMinMaxValues(0.0, 1.0);
    mVAxis->SetData(SIG_AXIS_DATA, NUM_SIG_AXIS_DATA);
}

bool
Impulses::SerializeState(IByteChunk &pChunk) const
{
    TRACE;
    
    ((Impulses *)this)->ENTER_PARAMS_MUTEX;
    
    // If we save, and we have not computed the resulting response yet
    ((Impulses *)this)->ProcessImpulseResponse();
    
    pChunk.Put(&mNativeSampleRate);
    
    // Serialize the two apply impulses responses
    for (int i = 0; i < 2; i++)
    {
        int size = mNativeImpulses[i].GetSize();
        pChunk.Put(&size);
        
        for (int j = 0; j < size; j++)
        {
            BL_FLOAT v = mNativeImpulses[i].Get()[j];
            pChunk.Put(&v);
        }
    }
    
    // Serialize image
    
    // First, create it if not already
    if (mBgImageBitmap == NULL)
    {
        int graphWidth = GRAPH_WIDTH;
        int graphHeight = GRAPH_HEIGHT;
        
        ((Impulses *)this)->mBgImageBitmap =
            new BLBitmap(graphWidth, graphHeight, 4);
    }

    // Secondly, serialize
    if (mBgImageBitmap != NULL)
    {        
        int imageWidth = mBgImageBitmap->GetWidth();
        pChunk.Put(&imageWidth);
  
        int imageHeight = mBgImageBitmap->GetHeight();
        pChunk.Put(&imageHeight);
  
        unsigned char *imgData = (unsigned char *)mBgImageBitmap->GetData();
        for (int j = 0; j < imageHeight; j++)
            for (int i = 0; i < imageWidth; i++)
            {
                int pix = *((int *)&imgData[(i + j*imageWidth)*4]);
            
                pChunk.Put(&pix);
            }
    }
    
    ((Impulses *)this)->LEAVE_PARAMS_MUTEX;
    
    return /*IPluginBase::*/SerializeParams(pChunk);
}

int
Impulses::UnserializeState(const IByteChunk &pChunk, int startPos)
{
    TRACE;
    
    ENTER_PARAMS_MUTEX;
  
    // Unserialize the two apply impulses responses
    startPos = pChunk.Get(&mNativeSampleRate, startPos);
  
    WDL_TypedBuf<BL_FLOAT> impulses[2];
    for (int i = 0; i < 2; i++)
    {
        int size;
        startPos = pChunk.Get(&size, startPos);
    
        impulses[i].Resize(size);
        
        for (int j = 0; j < size; j++)
        {
            BL_FLOAT v;
            startPos = pChunk.Get(&v, startPos);
            
            impulses[i].Get()[j] = v;
        }
    }
  
    // Keep the raw impulses
    mNativeImpulses[0] = impulses[0];
    mNativeImpulses[1] = impulses[1];
    
    // Takes 40MB with 16s
    ProcessNativeImpulses();
    
    // 
    WDL_TypedBuf<BL_FLOAT> graphResp = mImpulses[0];
#if DISPLAY_MONO_MAX_RESPONSE
    GetMaxImpulseResponse(&graphResp, mImpulses);
#endif
    
    // TAKE 30MB with 16s
    // Update the graph
    UpdateGraphAvgResponseDecim(graphResp);
 
    //
    // Unserialize image
    //
  
    // For checking compat
    int graphWidth = GRAPH_WIDTH;
    int graphHeight = GRAPH_HEIGHT;
      
    // In all cases, allocate the image
    if (mBgImageBitmap != NULL)
        delete mBgImageBitmap;
    mBgImageBitmap = new BLBitmap(graphWidth, graphHeight, 4);
    
    int imageWidth;
    startPos = pChunk.Get(&imageWidth, startPos);

    if (imageWidth != graphWidth)
        // Compat failure
    {
        // Rewind
        startPos -= sizeof(imageWidth);
        
        int res = /*IPluginBase::*/UnserializeParams(pChunk, startPos);

        // Must set black image
        if (mBgImageBitmap != NULL)
            mBgImageBitmap->Clear();
        mMustUpdateBGImage = true;
        
        LEAVE_PARAMS_MUTEX;

        return res;
    }
        
    int imageHeight;
    startPos = pChunk.Get(&imageHeight, startPos);

    if (imageHeight != graphHeight)
        // Compat failure
    {
        // Rewind
        startPos -= sizeof(imageHeight);
        
        int res = /*IPluginBase::*/UnserializeParams(pChunk, startPos);

        // Must set black image
        if (mBgImageBitmap != NULL)
            mBgImageBitmap->Clear();
        mMustUpdateBGImage = true;
        
        LEAVE_PARAMS_MUTEX;
        
        return res;
    }
    
    // The dimensions of the image read from the preferences are ok,
    // so read the pixels
    unsigned char *imgData = (unsigned char *)mBgImageBitmap->GetData();
    for (int j = 0; j < imageHeight; j++)
        for (int i = 0; i < imageWidth; i++)
        {
            int pix;
            startPos = pChunk.Get(&pix, startPos);

            *((int *)&imgData[(i + j*imageWidth)*4]) = pix;
        }

    // Update the graph
    mMustUpdateBGImage = true;
    
    int res = /*IPluginBase::*/UnserializeParams(pChunk, startPos);
    
    LEAVE_PARAMS_MUTEX;

    return res;
}

void
Impulses::DoReset(bool clearNativeImpulses)
{
    TRACE;
  
    ENTER_PARAMS_MUTEX;
  
    // Capture
    BL_FLOAT sampleRate = GetSampleRate();
    for (int i = 0; i < 2; i++)
    {
        if (mDiracGens[i] != NULL)
            mDiracGens[i]->Reset(sampleRate);
    }
  
    long numSamplesResponse = GetRespNumSamples();
    for (int i = 0; i < 2; i++)
    {
        if (mRespExtractors[i] != NULL)
            mRespExtractors[i]->Reset(numSamplesResponse, sampleRate, 1.0);
    }
  
    BL_FLOAT decimationFactor = ((BL_FLOAT)numSamplesResponse)/MAX_SAMPLES_DECIMATED;
    if (mRespExtractorsDecimated != NULL)
        mRespExtractorsDecimated->Reset(MAX_SAMPLES_DECIMATED,
                                        sampleRate, decimationFactor);
  
    if (mResponseSetsDecimated != NULL)
    {
        mResponseSetsDecimated->Reset(MAX_SAMPLES_DECIMATED, sampleRate);
        mResponseSetsDecimated->Clear();
    }
  
    for (int i = 0; i < 2; i++)
    {
        if (mResponseSets[i] != NULL)
        {
            mResponseSets[i]->Reset(numSamplesResponse, sampleRate);
            mResponseSets[i]->Clear();
        }
    }
  
    // Apply
    for (int i = 0; i < 2; i++)
    {
        int blockSize = GetBlockSize();
        if (mConvolvers[i] != NULL)
            mConvolvers[i]->Reset(sampleRate, blockSize);
    }
  
    for (int i = 0; i < 2; i++)
    {
        mImpulses[i].Resize((int)numSamplesResponse);
        BLUtils::FillAllZero(&mImpulses[i]);
    }
  
    if (clearNativeImpulses)
    {
        for (int i = 0; i < 2; i++)
        {
            mNativeImpulses[i].Resize((int)numSamplesResponse);
            BLUtils::FillAllZero(&mNativeImpulses[i]);
        }
    }
  
    // Clear the graph
    ClearGraphInstantResponse();
  
    ClearGraphAvgResponse();
  
    // For bounce
    mBounceSampleIndex = 0;
  
    // Graph
    if (mGraph != NULL)
    {
        int numGraphValues = (int)GetNumGraphValues();
        
        if (mInputSignalCurve != NULL)
            mInputSignalCurve->ResetNumValues(numGraphValues);
        if (mInstantResponseCurve != NULL)
            mInstantResponseCurve->ResetNumValues(numGraphValues);
        if (mAvgResponseCurve != NULL)
            mAvgResponseCurve->ResetNumValues(numGraphValues);;
    }
  
    mResponseNeedsProcess = false;
  
    mProcessingStarted = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::ProcessNativeImpulses()
{
    TRACE;
  
    WDL_TypedBuf<BL_FLOAT> *nativeImpulsesCopy = mTmpBuf3;
    nativeImpulsesCopy[0] = mNativeImpulses[0];
    nativeImpulsesCopy[1] = mNativeImpulses[1];
      
    for (int i = 0; i < 2; i++)
    {
        ResampleImpulse(&nativeImpulsesCopy[i]);
    }
  
    long responseSize = GetRespNumSamples();
  
    if (mNativeImpulses[0].GetSize() >= responseSize)
        // Reduce
    {    
        // Set the values (with security...)
        for (int i = 0; i < 2; i++)
        {
#if !PROCESS_NATIVE_IR_NO_ALIGN
            // Align the impulse response
            // This is necessary because the response may have been captured at 1s,
            // and the config may have changed to smaller time
            // (so for 1s, the align index was too big)
            ImpulseResponseSet::AlignImpulseResponse(&nativeImpulsesCopy[i],
                                                     responseSize, 1.0);
#endif
      
            mImpulses[i].Resize(responseSize);
            BLUtils::FillAllZero(&mImpulses[i]);
    
            BLUtils::CopyBuf(&mImpulses[i], nativeImpulsesCopy[i]);
    
            ImpulseResponseSet::MakeFades(&mImpulses[i], responseSize, 1.0);
        }
    }
    else
        // Process or increase
    {
        // Set the values (with security...)
        for (int i = 0; i < 2; i++)
        {
            if (nativeImpulsesCopy[i].GetSize() > 0)
                ImpulseResponseSet::MakeFades(&nativeImpulsesCopy[i],
                                              nativeImpulsesCopy[i].GetSize(), 1.0);
      
            mImpulses[i].Resize(responseSize);
            BLUtils::FillAllZero(&mImpulses[i]);
      
            BLUtils::CopyBuf(&mImpulses[i], nativeImpulsesCopy[i]);
      
#if !PROCESS_NATIVE_IR_NO_ALIGN
            ImpulseResponseSet::AlignImpulseResponse(&mImpulses[i],
                                                     responseSize, 1.0);
#endif
        }
    }
  
    // Do not denoise here, the native IR has already been denoised
    //
    // FIX: this fixes IR decrease when saving preference
    // (due to denoise of already denoised IR
#if !DENOISE_NATIVE_IR
    for (int i = 0; i < 2; i++)
    {
        ImpulseResponseSet::DenoiseImpulseResponse(&mImpulses[i]);
    }
#endif
  
    // If we loaded a mono response, copy it for the other channel
    // To have impulse application on the two channels
    if (mImpulses[1].GetSize() == 0)
        mImpulses[1] = mImpulses[0];
  
#if FIX_MONO_CHANNEL
    // Test if the second channel is empty
    bool chan1AllZero = BLUtils::IsAllZero(mImpulses[1]);
    if (chan1AllZero)
        mImpulses[1] = mImpulses[0];
#endif
  
    if (mConvolvers[0] != NULL)
        mConvolvers[0]->SetIR(mImpulses[0]);
    if (mConvolvers[1] != NULL)
        mConvolvers[1]->SetIR(mImpulses[1]);
}

void
Impulses::ClearGraphInstantResponse()
{
    TRACE;
  
    if (mGraph == NULL)
        return;
  
    mInstantResponseCurve->ClearValues();
}

void
Impulses::ClearGraphAvgResponse()
{
    TRACE;
  
    if (mGraph == NULL)
        return;
  
    mAvgResponseCurve->ClearValues();
}

void
Impulses::
UpdateGraphInstantResponse(const WDL_TypedBuf<BL_FLOAT> &instantImpulseResponse)
{
    TRACE;
  
    if (mGraph == NULL)
        return;
  
    //The cat says: ≤≤   :ml=TODO
    mInstantResponseCurve->ClearValues();
  
#if !AXIS_SHIFT_ZERO
    mGraph->SetCurveValuesDecimate2(INSTANT_RESPONSE_CURVE,
                                    &instantImpulseResponse, true);
#else
    SetGraphIR(mInstantResponseCurve, instantImpulseResponse);
#endif
}

void
Impulses::UpdateGraphAvgResponse(const WDL_TypedBuf<BL_FLOAT> &avgImpulseResponse)
{
    TRACE;
  
    if (mGraph == NULL)
        return;
  
    mAvgResponseCurve->ClearValues();
  
#if !AXIS_SHIFT_ZERO
    mGraph->SetCurveValuesDecimate2(AVG_RESPONSE_CURVE, &avgImpulseResponse, true);
#else

#if 1 // Decimate, to avoid enourmous number of values before 1st IR
    if (avgImpulseResponse.GetSize() > MAX_SAMPLES_DECIMATED)
    {
        // Decimate
        BL_FLOAT decimFactor =
            ((BL_FLOAT)MAX_SAMPLES_DECIMATED)/avgImpulseResponse.GetSize();
        
        WDL_TypedBuf<BL_FLOAT> &decimValues = mTmpBuf11;
        BLUtilsDecim::DecimateSamples(&decimValues, avgImpulseResponse, decimFactor);
        
        SetGraphIR(mAvgResponseCurve, decimValues);
    }
    else
    {
        SetGraphIR(mAvgResponseCurve, avgImpulseResponse);
    }
#else // No decim
    SetGraphIR(mAvgResponseCurve, avgImpulseResponse);
#endif
    
#endif
}

void
Impulses::
UpdateGraphAvgResponseDecim(const WDL_TypedBuf<BL_FLOAT> &avgImpulseResponse)
{
    TRACE;
  
    if (mGraph == NULL)
        // Just in the case, for startup
    {
        return;
    }
  
    if (mRespExtractorsDecimated == NULL)
        return;

#if 0 // Commented because not used at all
    BL_FLOAT decimFactor = mRespExtractorsDecimated->GetDecimationFactor();
    WDL_TypedBuf<BL_FLOAT> decimImpulses;
    ImpulseResponseExtractor::AddWithDecimation(avgImpulseResponse,
                                                decimFactor, &decimImpulses);
#endif
    
    UpdateGraphAvgResponse(avgImpulseResponse);
}

void
Impulses::UpdateGraphInstantResponseValid(bool validFlag)
{
    TRACE;
  
    if (mGraph == NULL)
        return;
  
    if (validFlag)
    {
        int instantCurveDefaultColor[4];
        mGUIHelper->GetGraphCurveColorBlue(instantCurveDefaultColor);
        mInstantResponseCurve->SetColor(instantCurveDefaultColor[0],
                                        instantCurveDefaultColor[1],
                                        instantCurveDefaultColor[2]);
    }
    else
    {
        int redColor[4];
        mGUIHelper->GetGraphCurveColorBlue(redColor);
        mInstantResponseCurve->SetColor(redColor[0],
                                        redColor[1],
                                        redColor[2]);
    }
}

void
Impulses::UpdateInputSignalCurve(const WDL_TypedBuf<BL_FLOAT> &signal)
{
    // No push/pull
    mInputSignalCurve->SetValues5(signal);
}

void
Impulses::ActionChanged(Action prevAction)
{
    TRACE;
  
    if (mAction == RESET)
    {
        // Reset IR by setting an empty IR
        WDL_TypedBuf<BL_FLOAT> ir;
        for (int i = 0; i < 2; i++)
        {
            if (mConvolvers[i] != NULL)
                mConvolvers[i]->SetIR(ir);
        }
        
        DoReset(true);
    }
  
    if (mAction == PROCESS_RESPONSES)
        mProcessingStarted = false;
  
    if ((prevAction == PROCESS_RESPONSES) &&
        (mAction != RESET))
        // We have stopped
        // We must compute our new impulse response
    {
        ProcessImpulseResponse();
    }

    GrayOutGainKnobs();
}

void
Impulses::ResponseLengthChanged()
{
    TRACE;
  
    if (mAction == PROCESS_RESPONSES)
        // Validate the current captured impulse responses
        ProcessImpulseResponse();
  
    SetConfig(mResponseLength);
    ApplyConfig();
}

BL_FLOAT
Impulses::ComputeTimeLimit()
{
    TRACE;
  
    BL_FLOAT sampleRate = GetSampleRate();
  
    BL_FLOAT timeLimitSeconds = (((BL_FLOAT)mConvBufferSize)/sampleRate);

    BL_FLOAT timeLimitNorm = timeLimitSeconds/(mMaxXAxisValue - mMinXAxisValue);
  
    return timeLimitNorm;
}

long
Impulses::GetRespNumSamples()
{
    TRACE;
  
    BL_FLOAT sampleRate = GetSampleRate();
    long numRespSamples = (mMaxXAxisValue - mMinXAxisValue)*sampleRate;
  
    return numRespSamples;
}

long
Impulses::GetNumGraphValues()
{
    TRACE;
  
    long numGraphSamples = GetRespNumSamples();
    long maxNumGraphValues = mGraphWidthPixels;
    long numGraphValues = (numGraphSamples < maxNumGraphValues) ?
    numGraphSamples : maxNumGraphValues;
  
    return numGraphValues;
}

void
Impulses::SetConfig(ResponseLength length)
{
    TRACE;
  
    switch(length)
    {
        case LENGTH_50MS:
        {
            mDiracFrequency = DIRAC_FREQUENCY_50MS;
            mMinXAxisValue = MIN_X_AXIS_VALUE_50MS;
            mMaxXAxisValue = MAX_X_AXIS_VALUE_50MS;
            mTimeAxisData = &AXIS_DATA_50MS;
      
            mConvBufferSize = 4096;
        }
        break;
      
        case LENGTH_100MS:
        {
            mDiracFrequency = DIRAC_FREQUENCY_100MS;
            mMinXAxisValue = MIN_X_AXIS_VALUE_100MS;
            mMaxXAxisValue = MAX_X_AXIS_VALUE_100MS;
            mTimeAxisData = &AXIS_DATA_100MS;
      
            mConvBufferSize = 8192;
        }
        break;
      
        case LENGTH_1S:
        {
            mDiracFrequency = DIRAC_FREQUENCY_1S;
            mMinXAxisValue = MIN_X_AXIS_VALUE_1S;
            mMaxXAxisValue = MAX_X_AXIS_VALUE_1S;
            mTimeAxisData = &AXIS_DATA_1S;
      
            mConvBufferSize = 131072;
        }
        break;
     
        case LENGTH_5S:
        {
            mDiracFrequency = DIRAC_FREQUENCY_5S;
            mMinXAxisValue = MIN_X_AXIS_VALUE_5S;
            mMaxXAxisValue = MAX_X_AXIS_VALUE_5S;
            mTimeAxisData = &AXIS_DATA_5S;
      
            mConvBufferSize = 262144;
        }
        break;
      
        case LENGTH_10S:
        {
            mDiracFrequency = DIRAC_FREQUENCY_10S;
            mMinXAxisValue = MIN_X_AXIS_VALUE_10S;
            mMaxXAxisValue = MAX_X_AXIS_VALUE_10S;
            mTimeAxisData = &AXIS_DATA_10S;
      
            mConvBufferSize = 524288;
        }
        break;
      
        default:
            break;
    }
}

void
Impulses::ApplyConfig()
{
    TRACE;
  
    for (int i = 0; i < 2; i++)
    {
        if (mDiracGens[i] != NULL)
            mDiracGens[i]->SetFrequency(mDiracFrequency);
    }
  
    ClearGraphInstantResponse();
    ClearGraphAvgResponse();
  
    UpdateTimeAxis();
  
    DoReset(false);
  
    ProcessNativeImpulses();

    WDL_TypedBuf<BL_FLOAT> graphResp = mImpulses[0];
#if DISPLAY_MONO_MAX_RESPONSE
    GetMaxImpulseResponse(&graphResp, mImpulses);
#endif
  
    // Update the graph
    UpdateGraphAvgResponseDecim(graphResp);
}

void
Impulses::ProcessImpulseResponse()
{
    TRACE;
  
    if (!mResponseNeedsProcess)
        return;
  
    if ((mResponseSets[0] == NULL) || (mResponseSets[1] == NULL))
        return;
  
    // Clear the instant responses if any
    ClearGraphInstantResponse();
  
    long responseSize = GetRespNumSamples();
  
    BL_FLOAT sampleRate = GetSampleRate();
  
    // Post-process all the full res data
  
    // Post-process all the impulses
    ImpulseResponseSet::AlignImpulseResponsesAll(mResponseSets,
                                                 responseSize, sampleRate);
  
    // Post-process eliminate bad responses
    bool dummy;
    bool iterate = true;
    ImpulseResponseSet::DiscardBadResponses(mResponseSets, &dummy, iterate);
  
    // Retrieve the average responses
    mResponseSets[0]->GetAvgImpulseResponse(&mNativeImpulses[0]);
    mResponseSets[1]->GetAvgImpulseResponse(&mNativeImpulses[1]);
  
    ImpulseResponseSet::NormalizeImpulseResponses(mNativeImpulses);
  
    mNativeSampleRate = GetSampleRate();
  
#if FADE_NATIVE_IR
    // Fade the native IR, to be able to save it with fades
    long numSamplesResponse = GetRespNumSamples();
    ImpulseResponseSet::MakeFades(&mNativeImpulses[0], numSamplesResponse, 1.0);
    ImpulseResponseSet::MakeFades(&mNativeImpulses[1], numSamplesResponse, 1.0);
#endif
  
#if DENOISE_NATIVE_IR
    for (int i = 0; i < 2; i++)
    {
        ImpulseResponseSet::DenoiseImpulseResponse(&mNativeImpulses[i]);
    }
#endif
  
    // Set from native
    mImpulses[0] = mNativeImpulses[0];
    mImpulses[1] = mNativeImpulses[1];
  
    // TODO: process native to mImpulses correctly ?
  
#if !FADE_NATIVE_IR
    // Make fades, just in case
    // To cut it cleanly.
    long numSamplesResponse = GetRespNumSamples();
    ImpulseResponseSet::MakeFades(&mImpulses[0], numSamplesResponse, 1.0);
    ImpulseResponseSet::MakeFades(&mImpulses[1], numSamplesResponse, 1.0);
#endif
  
    // The graph should have mechanismes to decimate by himself !
    // So don't decimate one more time here..
    WDL_TypedBuf<BL_FLOAT> &graphResp = mTmpBuf10;
    graphResp = mImpulses[0];
    
#if DISPLAY_MONO_MAX_RESPONSE
    GetMaxImpulseResponse(&graphResp, mImpulses);
#endif
  
    // Display only the left channel
    // (to avoid complicating the graph)
    UpdateGraphAvgResponseDecim(graphResp);
  
    mResponseNeedsProcess = false;
}

void
Impulses::OpenImageFile(const char *fileName)
{
    if (mGraphics == NULL)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    BLBitmap *bmp = BLBitmap::Load(fileName);
    if (bmp == NULL)
        return;
    
    mBgImageBitmap = ProcessLoadedBgBmp(bmp);
    delete bmp;
    
    mMustUpdateBGImage = true;

    // Keep load path
    BLUtilsFile::GetFilePath(fileName, mCurrentLoadPathImg, true);
    
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::ResetImageFile()
{
    int graphWidth = GRAPH_WIDTH;
    int graphHeight = GRAPH_HEIGHT;
      
    if (mBgImageBitmap != NULL)
        delete mBgImageBitmap;
    mBgImageBitmap = new BLBitmap(graphWidth, graphHeight, 4);

    mMustUpdateBGImage = true;
}

void
Impulses::OpenIRFile(const char *fileName)
{  
    ENTER_PARAMS_MUTEX;
    
    vector<WDL_TypedBuf<BL_FLOAT> > impulses;
  
    // Open the audio file
    AudioFile *audioFile = AudioFile::Load(fileName, &impulses);
    if (audioFile == NULL)
    {
        LEAVE_PARAMS_MUTEX;
        
        return;
    }
  
    // Do not resample, keep the original sample rate
    // We will resample on demand
    // (avoids saving resampled IRs)
  
    mNativeSampleRate = audioFile->GetSampleRate();
  
    int numChannels = audioFile->GetNumChannels();
  
    delete audioFile;
  
    // FIX: avoid having stereo effect when loading mono impulse
    // after having played with a stereo impulse
    mNativeImpulses[0].Resize(0);
    mNativeImpulses[1].Resize(0);
  
    if ((numChannels >= 1) && (impulses.size() >= 1))
        mNativeImpulses[0] = impulses[0];
  
    if ((numChannels >= 2) && (impulses.size() >= 2))
        mNativeImpulses[1] = impulses[1];
  
    // Takes 40MB with 16s
    ProcessNativeImpulses();
  
    WDL_TypedBuf<BL_FLOAT> graphResp = mImpulses[0];
#if DISPLAY_MONO_MAX_RESPONSE
    GetMaxImpulseResponse(&graphResp, mImpulses);
#endif
  
    // TAKE 30MB with 16s
    // Update the graph
    UpdateGraphAvgResponseDecim(graphResp);

#if 0 // Does nothing...
#if FIX_LOAD_IR_SAVE_PROJECT
    // Force the host to propose saving the project when quitting
    // For that, touch one of the parameter
  
    // Choose a parameter, for example kGraph
    BL_FLOAT norm = GetParam(kGraph)->GetNormalized();
  
    // Change the value
    // (because sometimes if the value is the same,
    // the host doesn't propose to save before quitting)
    if (norm < 0.5)
        norm = 1.0;
    else
        norm = 0.0;
#endif
#endif
    
    // Keep load path
    BLUtilsFile::GetFilePath(fileName, mCurrentLoadPathIR, true);
            
    LEAVE_PARAMS_MUTEX;
}

void
Impulses::SaveIRFile(const char *fileName)
{
    // Origin
    //#define IR_TO_SAVE mNativeImpulses
    //#define IR_SAMPLERATE mNativeSampleRate

    // New: save with the current sample rate, and the curren IR length
    BL_FLOAT sampleRate = GetSampleRate();
#define IR_TO_SAVE mImpulses
#define IR_SAMPLERATE sampleRate
    
    ENTER_PARAMS_MUTEX;
  
    // If we save, and we have not computed the resulting response yet
    ProcessImpulseResponse();
  
    // Does not save anything if we don't have data
    if (IR_TO_SAVE[0].GetSize() == 0)
    {
        LEAVE_PARAMS_MUTEX;
        
        return;
    }
    
    int numChannels = 1;
    if (IR_TO_SAVE[1].GetSize() > 0)
        numChannels = 2;
  
    // FIX: avod saving a stereo signal when the detected impulses series was mono
    if (numChannels == 2)
    {
        bool similarChannels = BLUtils::IsEqual(IR_TO_SAVE[0], IR_TO_SAVE[1]);
        if (similarChannels)
            // In fact, this is a mono signal duplicated on the second track
            numChannels = 1;
    }
  
    // FIX: avoid saving in stereo when the right channel is all zero
    // (Fixes in Logic with mono impulses)
    //
    if (BLUtils::IsAllZero(IR_TO_SAVE[1]))
        numChannels = 1;
  
    char *ext = BLUtilsFile::GetFileExtension(fileName);
    int format = SF_FORMAT_FLOAT;
    if ((ext == NULL) ||
        // By default, use wave format
        (strcmp(ext, "wav") == 0))
        format = format | SF_FORMAT_WAV;
    else
        if ((strcmp(ext, "aif") == 0) || (strcmp(ext, "aiff") == 0))
            format = format | SF_FORMAT_AIFF;
#if AUDIOFILE_USE_FLAC
        else
            if (strcmp(ext, "flac") == 0)
            {
                //format = format | SF_FORMAT_FLAC;
                format = SF_FORMAT_PCM_24 | SF_FORMAT_FLAC;
            }
#endif
            else
                // "Unknown" file format
                return;
  
    // Create the audio file
    AudioFile *audioFile = new AudioFile(numChannels, IR_SAMPLERATE, format);
  
    // Save here
    for (int i = 0; i < numChannels; i++)
    {
        audioFile->SetData(i, IR_TO_SAVE[i]);
    }

    /*bool saved =*/ audioFile->Save(fileName);

    delete audioFile;

    // Keep save path
    BLUtilsFile::GetFilePath(fileName, mCurrentSavePathIR, true);
        
    LEAVE_PARAMS_MUTEX;
}

// From Spatializer
void
Impulses::ResampleImpulse(WDL_TypedBuf<BL_FLOAT> *impulse)
{
    BL_FLOAT sampleRate = GetSampleRate();
  
    WDL_TypedBuf<BL_FLOAT> resampImpulse = *impulse;
  
    // Use the version that dos not fill with zeros if there misses some
    // few samples in the result
    FastRTConvolver3::ResampleImpulse2(&resampImpulse, mNativeSampleRate,
                                       sampleRate, false);
  
#if !FIX_GAIN_COEFF
    // Gain coefficient
    mGainCoeff = mNativeSampleRate/sampleRate;
#endif
  
    *impulse = resampImpulse;
}

void
Impulses::GetAvgImpulseResponse(WDL_TypedBuf<BL_FLOAT> *result,
                                const WDL_TypedBuf<BL_FLOAT> impulseResponses[2])
{
    bool allZero1 = BLUtils::IsAllZero(impulseResponses[1]);
    if (allZero1)
    {
        *result = impulseResponses[0];
    
        return;
    }
  
    bool allZero0 = BLUtils::IsAllZero(impulseResponses[0]);
    if (allZero0)
    {
        *result = impulseResponses[1];
    
        return;
    }
  
    result->Resize(impulseResponses[0].GetSize());
    BLUtils::FillAllZero(result);
  
    for (int i = 0; i < result->GetSize(); i++)
    {
        BL_FLOAT val0 = impulseResponses[0].Get()[i];
        BL_FLOAT val1 = 0.0;
        if (i < impulseResponses[1].GetSize())
            val1 = impulseResponses[1].Get()[i];
    
        BL_FLOAT res = (val0 + val1)/2.0;
        result->Get()[i] = res;
    }
}

void
Impulses::GetMaxImpulseResponse(WDL_TypedBuf<BL_FLOAT> *result,
                                const WDL_TypedBuf<BL_FLOAT> impulseResponses[2])
{
    BL_FLOAT max0 = BLUtils::ComputeMaxAbs(impulseResponses[0]);
    BL_FLOAT max1 = BLUtils::ComputeMaxAbs(impulseResponses[1]);
  
    if (max0 > max1)
    {
        *result = impulseResponses[0];
    
        return;
    }
  
    *result = impulseResponses[1];
}

void
Impulses::SetGraphIR(GraphCurve5 *curve,
                     const WDL_TypedBuf<BL_FLOAT> &ir)
{
    BL_FLOAT length = 0.0;
    BL_FLOAT shiftTime = 1.0;
    switch(mResponseLength)
    {
        case LENGTH_50MS:
        {
            length = 0.05;
            shiftTime = SHIFT_TIME_50MS;
        }
        break;
      
        case LENGTH_100MS:
        {
            length = 0.1;
            shiftTime = SHIFT_TIME_100MS;
        }
        break;
      
        case LENGTH_1S:
        {
            length = 1.0;
            shiftTime = SHIFT_TIME_1S;
        }
        break;
      
        case LENGTH_5S:
        {
            length = 5.0;
            shiftTime = SHIFT_TIME_5S;
        }
        break;
      
        case LENGTH_10S:
        {
            length = 10.0;
            shiftTime = SHIFT_TIME_10S;
        }
        break;
      
        default:
            break;
    };
  
    // With this, we are always centered padded to 0s
    BL_FLOAT shiftSamples = (shiftTime/length)*ir.GetSize();
    
    // Shift
    //
  
    // Left
    WDL_TypedBuf<BL_FLOAT> padIR = ir;
    BLUtils::PadZerosLeft(&padIR, (int)shiftSamples);
    
    // No push/pull
    curve->SetValues5(padIR);
}

void
Impulses::InitTimeAxis()
{
    BL_FLOAT yOffset = 0.0;
    bool displayLines = true;
    
    int axisColor[4];
    mGUIHelper->GetGraphAxisColor(axisColor);
        
    int axisLabelColor[4];
    mGUIHelper->GetGraphAxisLabelColor(axisLabelColor);
        
    int axisLabelOverlayColor[4];
    mGUIHelper->GetGraphAxisLabelOverlayColor(axisLabelOverlayColor);
        
    BL_GUI_FLOAT lineWidth = mGUIHelper->GetGraphAxisLineWidth();
        
    if (!displayLines)
    {
        axisColor[3] = 0;
    }
        
    // NOTE: should be InitHAxis() ?
    mHAxis->InitVAxis(Scale::LINEAR, 0.0, 1.0,
                      axisColor, axisLabelColor,
                      lineWidth,
                      0.0, yOffset,
                      axisLabelOverlayColor);
}

void
Impulses::UpdateGraphBgImage()
{
    if (mBgImageBitmap == NULL)
        return;
    
    if (mGraphics == NULL)
        return;
    
    IBitmap bmp = mGraphics->CreateBitmap(mBgImageBitmap->GetWidth(),
                                          mBgImageBitmap->GetHeight(),
                                          mBgImageBitmap->GetBpp(),
                                          (unsigned char *)mBgImageBitmap->GetData());
    
    mGraph->SetBackgroundImage(mGraphics, bmp);
}

BLBitmap *
Impulses::ProcessLoadedBgBmp(const BLBitmap *bmp)
{
    int bmpWidth = bmp->GetWidth();
    int bmpHeight = bmp->GetHeight();
    BL_FLOAT bmpRatio = ((BL_FLOAT)bmpWidth)/bmpHeight;

    int graphWidth = GRAPH_WIDTH;
    int graphHeight = GRAPH_HEIGHT;
    
#if 0 // Black borders left and right
    int newBmpWidth = graphHeight*bmpRatio;
    int newBmpHeight = graphHeight;
    
    int dstX = graphWidth/2 - newBmpWidth/2;
    int dstY = 0;
#endif
    
#if 1 // Crop to fill
    int newBmpWidth = graphWidth;
    int newBmpHeight = graphWidth/bmpRatio;
    
    int dstX = 0;
    int dstY = graphHeight/2 - newBmpHeight/2;
#endif
    
    BLBitmap *result = new BLBitmap(graphWidth, graphHeight, 4);
    
    BLBitmap::ScaledBlit(result, bmp,
                         dstX, dstY, newBmpWidth, newBmpHeight,
                         0, 0, bmpWidth, bmpHeight);
    
#if 1 // Effect: Black line dithering
    unsigned char *resultData = (unsigned char *)result->GetData();
    for (int j = 0; j < result->GetHeight(); j += 2)
    {
        for (int i = 0; i < result->GetWidth(); i++)
        {
            int offset = (i + j*result->GetWidth())*4;
            resultData[offset + 0] = 0; // r
            resultData[offset + 1] = 0; // g
            resultData[offset + 2] = 0; // b
        }
    }
#endif
    
    // NOTE: dod not use alpha
    // => without alpha it gives brighter and more beatiful background image!
#if 0
    float alpha = 1.0;
    
#if 0 // Effect: darker, to use alone
    alpha = 0.5;
#endif
    
    
#if 1 // Effect: darker, to use with black line dithering
    alpha = 0.75;
#endif
#endif
    
    return result;
}

void
Impulses::GrayOutGainKnobs()
{
    if (mAction == PROCESS_RESPONSES)
    {
        if (mInGainKnob != NULL)
        {
            //mInGainKnob->SetDisabled(false);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kInGain, false);
        }
        
        if (mOutGainKnob != NULL)
        {
            //mOutGainKnob->SetDisabled(true);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kOutGain, true);
        }
    }

    if ((mAction == GEN_IMPULSES) || (mAction == APPLY_RESPONSE))
    {
        if (mInGainKnob != NULL)
        {
            //mInGainKnob->SetDisabled(true);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kInGain, true);
        }
        
        if (mOutGainKnob != NULL)
        {
            //mOutGainKnob->SetDisabled(false);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kOutGain, false);
        }
    }

    if (mAction == RESET)
    {
        if (mInGainKnob != NULL)
        {
            //mInGainKnob->SetDisabled(true);
            
            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kInGain, true);
        }
        
        if (mOutGainKnob != NULL)
        {
            //mOutGainKnob->SetDisabled(true);

            // Also disable knob value automatically
            IGraphics *graphics = GetUI();
            if (graphics != NULL)
                graphics->DisableControl(kOutGain, true);
        }
    }
}
