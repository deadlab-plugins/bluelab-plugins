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

#include <KemarHrtf.h>
#include <Hrtf.h>
#include <Window.h>

#include <SpatializerConvolver.h>

#include <ParamSmoother2.h>

#include "Spatializer.h"

#include <FftProcessObj16.h>

#include <BufProcessObjSmooth2.h>

#include <FixedBufferObj.h>

#include "IPlug_include_in_plug_src.h"

// FIX: Cubase 10, mac Sierra: mono track => crashes when playing
#define FIX_CUBASE_CRASH_MONO 1

// HACK: Only used for during win version dev !
// Generates a text rc file, to be include when building the Windows version
#define DUMP_WIN_RC_DATA 0

// BUG with PT : When automation is perfectly horizontal, the parameter
// seems not updated

// NOTE: compact Kemar dataset sounds better

// Set greater than 128, for phase shift
#define BUFFER_SIZE 1024

// 6 meters is about 1024 samples at 44100Hz
#define MAX_SOURCE_DISTANCE 6.0

// Multiply by 2 when after applying the response, to stay coherent with bypass
#define GAIN_FACTOR 2.0

// With that, the delay is too much increase, and rapidly overstates the BUFFER_SIZE
// (when normalization is disabled)
//#define OVERSTATE_DELAY_FACTOR 20.0

// EXAMPLE: sound file "morning"

// With that, the impulses always fit in the 1024 buffer
#define OVERSTATE_DELAY_FACTOR 3.0

// BAD (for the moment)
// When activated, makes clicks with high SR
//
// And latency compensation is not yet designed
// to compensate variable buffer size
//
// And the spetrogram is flattened at 192KHz
#define TEST_CONVOLVER_VARIABLE_BUFFER 0

// Do not use smoothers anymore
//
// To try to be more reactive when moving azimuth or elevation
// Should not be useful anymore, because we use minimum 1024
// buffer size, and we use BufProcessObjSmooth2
// So the smooth may be done another way
//
// NOTE: Maybe makes just a little more rumbles with 192KHz and low block size
//
#define DISABLE_SMOOTHERS 1

// VERY GOOD !
// FIX: play a short loop, move the azimuth quickly by hand (Reaper Mac)
// (not by automations)
// => clicks and short previouly buffered residual sound appear
#define FORCE_IMPULSE_UPDATE2 1

// FIX: problem in Reason (Mac, block size 64), when starting, it clicks a lot
// (this helps to fix, but the real fix is in FixedBufferObj)
// (and maybe another real fix is in FftConvolver6, "check zeros before fft")
#define FIX_START_CLICK_REASON 1

// FIX: remaining sound when stop playin in Logic 10.4
// (no problem on Logic 10.1)
//
// Same problem on Protools 2018 (Sierra)
#define FIX_LOGIC104_REMAIN_SOUND 1

// GUI
#define TITLE_OFFSET_Y 25

#define VALUE_X_OFFSET_AZIMUTH 6 //8
#define VALUE_X_OFFSET_ELEVATION 6 //8

#define VALUE_Y_OFFSET_AZIMUTH -6 //-30 //10
#define VALUE_Y_OFFSET_ELEVATION 6 //-18

#define AZIMUTH_CONTROL_RAD 80.0
#define ELEVATION_CONTROL_RAD 80.0

#define MIN_AZIMUTH_ANGLE -180.0
#define MAX_AZIMUTH_ANGLE 180.0
#define AZIMUTH_ANGLE_HANDLE_OFFSET -90.0

#define MIN_ELEVATION_ANGLE -90.0
#define MAX_ELEVATION_ANGLE 40.0

//
#if 0
BUG: at 192KHz, when changing azimuth very quickly, it rumbles a little
#endif
    
static char *tooltipHelp = "Help - Display help";
static char *tooltipAzimuth = "Azimuth - Azimuth angle";
static char *tooltipElevation = "Elevation - Elevation angle";
static char *tooltipSoureWidth = "Source Width - Sound source width";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipXYPad = "Azimuth/Elevation - Azimuth and elevation angles";

enum EParams
{
    kAzimuth = 0,
    kElevation,
    kSourceWidth,
    kOutGain,
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,
    
    kHeadAzimuthX = 70,
    kHeadAzimuthY = 36,
  
    kAzimuthX = 28,
    kAzimuthY = 29,
  
    kHeadElevationX = 219,
    kHeadElevationY = 36,
  
    kElevationX = 187,
    kElevationY = 17,
    
    kSourceWidthX = 361,
    kSourceWidthY = 209,
    
    kOutGainX = 361,
    kOutGainY = 308,

    kXYPadX = 59,
    kXYPadY = 242
};

//
Spatializer::Spatializer(const InstanceInfo &info)
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

Spatializer::~Spatializer()
{
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;

    if (mAzimSmoother != NULL)
        delete mAzimSmoother;
    if (mElevSmoother != NULL)
        delete mElevSmoother;
    if (mSourceWidthSmoother != NULL)
        delete mSourceWidthSmoother;
  
    for (int i = 0; i < 2; i++)
    {
        if (mConvolvers[i] != NULL)
            delete mConvolvers[i];
    }
    
#if USE_FIXED_BUFFER_OBJ
    if (mFixedBufObj != NULL)
        delete mFixedBufObj;
#endif
  
    if (mHrtf != NULL)
        delete mHrtf;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Spatializer::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
Spatializer::MyMakeLayout(IGraphics *pGraphics)
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

    //CreateHrtf(pGraphics);
    
    ApplyParams();
      
    // Demo mode
    mDemoManager.Init(this, pGraphics);
        
    mUIOpened = true;
        
    LEAVE_PARAMS_MUTEX;  
}

void
Spatializer::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    FftProcessObj16::Init();
    
    mUIOpened = false;
    mControlsCreated = false;

    mOutGainSmoother = NULL;

    mAzimSmoother = NULL;
    mElevSmoother = NULL;
    mSourceWidthSmoother = NULL;
  
    for (int i = 0; i < 2; i++)
    {
        mConvolvers[i] = NULL;
    }
    
#if USE_FIXED_BUFFER_OBJ
    mFixedBufObj = NULL;
#endif
  
    mHrtf = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
Spatializer::Init()
{ 
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // Smoothers
    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);

    BL_FLOAT defaultAzimuth = 0.0;
    BL_FLOAT defaultElevation = 0.0;
    BL_FLOAT defaultSourceWidth = 1.;
    
#if !DISABLE_SMOOTHERS
    // 0.7 is good too (60ms of late)
    mAzimSmoother =
        new ParamSmoother2(sampleRate, defaultAzimuth, 0.4); //0.4ms?
    mElevSmoother =
        new ParamSmoother2(sampleRate, defaultElevation, 0.4);
    mSourceWidthSmoother =
        new ParamSmoother2(sampleRate, defaultSourceWidth, 0.4);
#else
    mAzimSmoother =
        new ParamSmoother2(sampleRate, defaultAzimuth, 0.0001);
    mElevSmoother =
        new ParamSmoother2(sampleRate, defaultElevation, 0.0001);
    mSourceWidthSmoother =
        new ParamSmoother2(sampleRate, defaultSourceWidth, 0.0001);
#endif
    
    // WIN
#if DUMP_WIN_RC_DATA
    KemarHRTF::DumpWinRcFile("KEMAR-Hrtf.rc", 1000);
#endif

    CreateHrtf();

    for (int i = 0; i < 2; i++)
    {
#if USE_SMOOTH_CONVOLVER
        SpatializerConvolver *conv0 = new SpatializerConvolver(BUFFER_SIZE);
        SpatializerConvolver *conv1 = new SpatializerConvolver(BUFFER_SIZE);
    
        mConvolvers[i] = new BufProcessObjSmooth2(conv0, conv1, BUFFER_SIZE);
#else
        mConvolvers[i] = new FftConvolver6(BUFFER_SIZE, true, true, false);
#endif
    }
  
#if USE_FIXED_BUFFER_OBJ
    mFixedBufObj = new FixedBufferObj(BUFFER_SIZE);
#endif

    mImpulseUpdate = true;
  
    mIsInitialized = true;
}

void
Spatializer::InitParams()
{
    // Safe method (we are sure to not crash, even non-tested hosts)
    const char *degree = " ";
  
    BL_FLOAT defaultAzimuth = 0.0;
    mAzimuth = defaultAzimuth;
    GetParam(kAzimuth)->InitDouble("Azimuth", defaultAzimuth,
                                   MIN_AZIMUTH_ANGLE, MAX_AZIMUTH_ANGLE,
                                   0.1, degree);
  
    BL_FLOAT defaultElevation = 0.0;
    mElevation = defaultElevation;
    GetParam(kElevation)->InitDouble("Elevation", defaultElevation,
                                     MIN_ELEVATION_ANGLE, MAX_ELEVATION_ANGLE,
                                     0.1, degree);
  
    BL_FLOAT defaultSourceWidth = 1.0;
    mSourceWidth = defaultSourceWidth;
    GetParam(kSourceWidth)->InitDouble("SourceWidth", defaultSourceWidth,
                                       1., 90.0, 0.1, degree);
  
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0; // 0dB
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");
}

void
Spatializer::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
    if (!mIsInitialized)
        return;

    if (mHrtf == NULL)
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

    // Set the outputs to 0
    //
    // FIX: on Protools, in render mode, after play is finished,
    // there is a buzz sound
    //
    // Other notes:
    //
    // With Logic 4 (Sierra), when stop playing, the still input contains
    // some sound for a moment (this sound is a sort of distorted echo)
    //
    // Same problem on Protools 2018 (Sierra), so comment the host test)
    for (int i = 0; i < out.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &out0 = out[i];
        BLUtils::FillAllZero(&out0);
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

#if USE_FIXED_BUFFER_OBJ
    if (nFrames < BUFFER_SIZE)
    {
        mFixedBufObj->SetInputs(in);
        
        bool inputsReady = mFixedBufObj->GetInputs(&in);
        if (!inputsReady)
        {
            mFixedBufObj->GetOutputs(&out, nFrames);
            
            // Before return...
            int numSamples = 0;
            if (!out.empty())
                numSamples = out[0].GetSize();
            BLUtilsPlug::PlugCopyOutputs(out, outputs, numSamples);
      
            // Demo mode
            if (mDemoManager.MustProcess())
            {
                mDemoManager.Process(outputs, nFrames);
            }
            
            BL_PROFILE_END;
            
            return;
        }
        
        mFixedBufObj->ResizeOutputs(&out);
    }
#endif

    BL_FLOAT delays[2] = { 0.0, 0.0 };
    
    mAzimuth = mAzimSmoother->Process();
    mElevation = mElevSmoother->Process();
    mSourceWidth = mSourceWidthSmoother->Process();
  
#if 1
    // Use this instead of FORCE_IMPULSE_UPDATE !
    //
    // If azimuth or elevation change due to smoother update,
    // we must update the responses !
    if (!mAzimSmoother->IsStable())
        mImpulseUpdate = true;
    if (!mElevSmoother->IsStable())
        mImpulseUpdate = true;
    if (!mSourceWidthSmoother->IsStable())
        mImpulseUpdate = true;
#endif
    
    BL_FLOAT azimuth = mAzimuth;
  
    // In the plugin, facing forward is 180 degrees,
    // in the HRTF, this is 0 degrees
    azimuth -= 180.0;
    if (azimuth < 0.0)
        azimuth += 360.0;
    
    azimuth = fmod(azimuth, 360.0);
      
#if 0 // Old version, with source width very lightly audible
    delays[0] = ComputeDelayWidth(mElevation, azimuth, 0, mSourceWidth);
    delays[1] = ComputeDelayWidth(mElevation, azimuth, 1, mSourceWidth);
#endif
    
#if 1 // Current version, with "source width" well audible (because overstated)
    delays[0] = ComputeDelayDistance(mElevation, azimuth, 0, KEMAR_SOURCE_DISTANCE);
    delays[1] = ComputeDelayDistance(mElevation, azimuth, 1, KEMAR_SOURCE_DISTANCE);
#endif
    
    // Increase artificially the delays
    IncreaseDelays(delays, (mSourceWidth - 1.0)/(90.0 - 1.0));
  
    // We want only one input, for two outputs
    WDL_TypedBuf<BL_FLOAT> &inSamplesBuf = mTmpBuf4;
    inSamplesBuf = in[0];
    
    if (in.size() > 1)
        BLUtils::StereoToMono(&inSamplesBuf, in[0], in[1]);
    
    for (int i = 0; i < 2; i++)
    {
        if ((inSamplesBuf.GetSize() > 0) && (out.size() > i))
        {
#if FORCE_IMPULSE_UPDATE2
            // Force impulse update
            mImpulseUpdate = true;
#endif
      
            if (mImpulseUpdate)
            {
                // Get the impulse each time
                WDL_TypedBuf<BL_FLOAT> &impulse = mTmpBuf5;
                
                // Ok for v3.0 (no phase shift)
                // bool found = mHrtf->GetImpulseResponse(&impulse,
                // mElevation, azimuth , i);
          
                int bufferSize = BUFFER_SIZE;
          
                bool overFlag = false;
                bool found = mHrtf->GetImpulseResponseInterp(&impulse,
                                                             mElevation, azimuth , i,
                                                             delays[i],
                                                             bufferSize, &overFlag);
          
                if (found)
                {
                    ResampleImpulse(&impulse);
          
#if USE_SMOOTH_CONVOLVER
                    WDL_TypedBuf<BL_FLOAT> *copyResp = new WDL_TypedBuf<BL_FLOAT>();
                    *copyResp = impulse;
                    mConvolvers[i]->SetParameter(copyResp);
#else
                    // Here, if FORCE_IMPULSE_UPDATE, makes blank zones in the result
                    // => this is because the internal result is reset when we set the response
                    mConvolvers[i]->SetResponse(&impulse);
#endif
                }
            }
      
            int numSamples = 0;
            if (!in.empty())
                numSamples = in[0].GetSize();
        
            // inSamples my be in[0] if we have only one input
            mConvolvers[i]->Process(inSamplesBuf.Get(), out[i].Get(), numSamples);
        }
    }
  
    // Apply gain due to resampled responses
#if !FIX_CUBASE_CRASH_MONO
    for (int i = 0; i < 2; i++)
#else
    for (int i = 0; i < out.size(); i++)
#endif
    {
        // Same as scaling the impulse response just after rescaling
        
        BL_FLOAT sampleRate = GetSampleRate();
        BL_FLOAT impSampleRate = mHrtf->GetSampleRate();
        
        // Compensate the volume scale after applying scaled impulse response
        BL_FLOAT coeff = impSampleRate/sampleRate;
        BLUtils::MultValues(&out[i], coeff);
        
        int numSamples = 0;
        if (!out.empty())
            numSamples = out[0].GetSize();
        
        // Multiply by a factor, to stay coherent compared to bypass
        for (int j = 0; j < numSamples; j++)
            out[i].Get()[j] *= GAIN_FACTOR;
    }
  
    mImpulseUpdate = false;
  
    // Apply gain
    //int numSamples = 0;
    //if (!out.empty())
    //    numSamples = out[0].GetSize();

    // Apply output gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
#if USE_FIXED_BUFFER_OBJ
    if (nFrames < BUFFER_SIZE)
    {
        mFixedBufObj->SetOutputs(out);
    
        mFixedBufObj->GetOutputs(&out, nFrames);
    }
#endif
  
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
Spatializer::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
    
    // Azimuth circle handle
    mGUIHelper->
        CreateSpatializerHandle(pGraphics,
                                kAzimuthX, kAzimuthY,
                                AZIMUTH_CONTROL_RAD,
                                MIN_AZIMUTH_ANGLE + AZIMUTH_ANGLE_HANDLE_OFFSET,
                                MAX_AZIMUTH_ANGLE + AZIMUTH_ANGLE_HANDLE_OFFSET,
                                true,
                                SPATIALIZER_HANDLE_FN,
                                TEXTFIELD_FN,
                                kAzimuth,
                                VALUE_X_OFFSET_AZIMUTH,
                                VALUE_Y_OFFSET_AZIMUTH,
                                tooltipAzimuth);

    // Head bitmap
    mGUIHelper->CreateBitmap(pGraphics,
                             kHeadAzimuthX, kHeadAzimuthY,
                             HEAD_AZIMUTH_FN);

#if 0
    // Title
    int knobAzimuthBmpW = AZIMUTH_CONTROL_RAD*2; //164;
    int knobAzimuthBmpH = AZIMUTH_CONTROL_RAD*2; //164;
    mGUIHelper->CreateTitle(pGraphics,
                            kAzimuthX + knobAzimuthBmpW/2,
                            kAzimuthY + knobAzimuthBmpH + TITLE_OFFSET_Y,
                            "AZIMUTH", GUIHelper12::SIZE_DEFAULT);
#endif
    
    // Elevation circle handle
    mGUIHelper->CreateSpatializerHandle(pGraphics,
                                        kElevationX, kElevationY,
                                        ELEVATION_CONTROL_RAD,
                                        MIN_ELEVATION_ANGLE, MAX_ELEVATION_ANGLE,
                                        false,
                                        SPATIALIZER_HANDLE_FN,
                                        TEXTFIELD_FN,
                                        kElevation,
                                        VALUE_X_OFFSET_ELEVATION,
                                        VALUE_Y_OFFSET_ELEVATION,
                                        tooltipElevation);
    
    // Head bitmap
    mGUIHelper->CreateBitmap(pGraphics,
                             kHeadElevationX, kHeadElevationY,
                             HEAD_ELEVATION_FN);

#if 0
    // Title
    int knobElevationBmpW = ELEVATION_CONTROL_RAD*2;
    int knobElevationBmpH = ELEVATION_CONTROL_RAD*2;
    mGUIHelper->CreateTitle(pGraphics,
                            kElevationX + knobElevationBmpW/2,
                            kElevationY + knobElevationBmpH + TITLE_OFFSET_Y,
                            "ELEVATION", GUIHelper12::SIZE_DEFAULT);
#endif
    
    //
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSourceWidthX, kSourceWidthY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSourceWidth,
                              TEXTFIELD_FN,
                              "WIDTH",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSoureWidth);
  
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

    mGUIHelper->CreateXYPad(pGraphics,
                            kXYPadX, kXYPadY,
                            XYPAD_TRACK_FN,
                            XYPAD_HANDLE_FN,
                            kAzimuth, kElevation,
                            XYPAD_BORDER_SIZE,
                            true,
                            tooltipXYPad);
    
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

void
Spatializer::CreateHrtf()
{
    if (mHrtf != NULL)
        return;
    
    const char* resourcePath = NULL;

#ifndef WIN32
    WDL_String resPath;
    BLUtilsPlug::GetFullPlugResourcesPath(*this, &resPath);
    
    resourcePath = resPath.Get();
#endif
    
    // For fft convolution
    KemarHRTF::Load(resourcePath, &mHrtf);
}

void
Spatializer::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Spatializer::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    for (int i = 0; i < 2; i++)
    {
        if (mConvolvers[i] != NULL)
            mConvolvers[i]->Reset();
    
        // Avoid crackle with width=100% and sr=88100,
        // when turning the azimuth knob while playing
        BL_FLOAT sampleRate = GetSampleRate();
        BL_FLOAT coeff = sampleRate/44100.0;
        coeff = ceil(coeff);
        int bufferSize = BUFFER_SIZE*coeff;
    
#if USE_SMOOTH_CONVOLVER
        if (mConvolvers[i] != NULL)
            mConvolvers[i]->SetBufferSize(bufferSize);
    
        // NOTE: should also change the buffer size of BufProcessObjSmooth2
#endif
    }

    BL_FLOAT sampleRate = GetSampleRate();
        
    // This is necessary !
    // 
    // Do reset the smoothers with the current value
    //
    // Otherwise, with the default Reset(),
    // they would be reset with 0.0
    // Which would made problem (e.g when playing loops)
    //
    BL_FLOAT azimuth = GetParam(kAzimuth)->Value();
  
    // For new heads: reverse
    azimuth = -azimuth;


    if (mAzimSmoother != NULL)
    {
        mAzimSmoother->Reset(sampleRate);
        mAzimSmoother->ResetToTargetValue(azimuth);
    }
    
    BL_FLOAT elevation = GetParam(kElevation)->Value();
    elevation *= -1.0;

    if (mElevSmoother != NULL)
    {
        mElevSmoother->Reset(sampleRate);
        mElevSmoother->ResetToTargetValue(elevation);
    }
    
    BL_FLOAT sourceWidth = GetParam(kSourceWidth)->Value();
    if (mSourceWidthSmoother != NULL)
    {
        mSourceWidthSmoother->Reset(sampleRate);
        mSourceWidthSmoother->ResetToTargetValue(sourceWidth);
    }
    
    mImpulseUpdate = true;
  
#if USE_FIXED_BUFFER_OBJ
    if (mFixedBufObj != NULL)
        mFixedBufObj->Reset();
    UpdateLatency();
#endif
  
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Spatializer::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kAzimuth:
        {
            BL_FLOAT azimuth = GetParam(kAzimuth)->Value();
            // For new heads: reverse
            azimuth = -azimuth;
            mAzimuth = azimuth;

            if (mAzimSmoother != NULL)
                mAzimSmoother->SetTargetValue(azimuth);
      
            mImpulseUpdate = true;
        }
        break;
    
        case kElevation:
        {
            // -90 -> 40
            BL_FLOAT elevation = GetParam(kElevation)->Value();
            // 90 -> -40
            elevation *= -1.0;
            mElevation = elevation;

            if (mElevSmoother != NULL)
                mElevSmoother->SetTargetValue(elevation);
      
            mImpulseUpdate = true;
        }
        break;
    
        case kSourceWidth:
        {
            BL_FLOAT sourceWidth = GetParam(kSourceWidth)->Value();
            mSourceWidth = sourceWidth;

            if (mSourceWidthSmoother != NULL)
                mSourceWidthSmoother->SetTargetValue(sourceWidth);
      
            // Must get the impulse again since it will be phase-shifted
            mImpulseUpdate = true;
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
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Spatializer::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
Spatializer::OnUIClose()
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
Spatializer::ApplyParams()
{          
    if (mAzimSmoother != NULL)
        mAzimSmoother->ResetToTargetValue(mAzimuth);
    
    if (mElevSmoother != NULL)
        mElevSmoother->ResetToTargetValue(mElevation);
    
    if (mSourceWidthSmoother != NULL)
        mSourceWidthSmoother->ResetToTargetValue(mSourceWidth);
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);
    
    mImpulseUpdate = true;
}

// Computation with 3d coordinates
BL_FLOAT
Spatializer::ComputeEarDistance(BL_FLOAT elevation, BL_FLOAT azimuth,
                                BL_FLOAT interauralDistance,
                                BL_FLOAT sourceDistance, int earNum)
{
    // Plane of the azimuth
    BL_FLOAT leftEarPos[3] = { -interauralDistance/2.0, 0.0, 0.0 };
    BL_FLOAT rightEarPos[3] = { interauralDistance/2.0, 0.0, 0.0 };
  
    // NOTE: This is 90 ! (it has been well checked)
    BL_FLOAT az0 = azimuth - 90.0;
  
    az0 = az0*M_PI/180.0;
  
    BL_FLOAT elev0 = elevation - 90.0;
    elev0 = elev0*M_PI/180.0;
  
    // See: http://electron9.phys.utk.edu/vectors/3dcoordinates.htm
    BL_FLOAT sourcePos[3] = { -sourceDistance*cos(az0)*sin(elev0),
                              sourceDistance*sin(az0)*sin(elev0),
                              sourceDistance*cos(elev0) };
  
    BL_FLOAT leftDistance =
        sqrt((leftEarPos[0] - sourcePos[0])*(leftEarPos[0] - sourcePos[0]) +
             (leftEarPos[1] - sourcePos[1])*(leftEarPos[1] - sourcePos[1]) +
             (leftEarPos[2] - sourcePos[2])*(leftEarPos[2] - sourcePos[2]));

    BL_FLOAT rightDistance =
        sqrt((rightEarPos[0] - sourcePos[0])*(rightEarPos[0] - sourcePos[0]) +
             (rightEarPos[1] - sourcePos[1])*(rightEarPos[1] - sourcePos[1]) +
             (rightEarPos[2] - sourcePos[2])*(rightEarPos[2] - sourcePos[2]));
    
    if (earNum == 0)
        return leftDistance;
    else
        return rightDistance;
}

// Physically correct
BL_FLOAT
Spatializer::ComputeDelayWidth(BL_FLOAT elevation, BL_FLOAT azimuth,
                               int chan, BL_FLOAT apparentSourceWidth)
{
#define INF 1e15
  
    // 340 m/s
#define SOUND_SPEED_AIR     340.0
  
    // 22 cm
#define INTERAURAL_DISTANCE 0.22
  
    // Compute the distance to the source in meters
  
    // Fixed width: 1m
    BL_FLOAT width = 1.0;
  
    BL_FLOAT sourceWidthRad = apparentSourceWidth*M_PI/180.0;
    BL_FLOAT tg = (apparentSourceWidth < 90.0) ? tan(sourceWidthRad) : INF;
  
    // Distance to the center of the head
    BL_FLOAT headDistanceMeters = (tg > 0.0) ? width/tg : INF;
  
    // Check the distance is not too far
    // Otherwise, we will overrun the process buffers
    if (headDistanceMeters > MAX_SOURCE_DISTANCE) 
        headDistanceMeters = MAX_SOURCE_DISTANCE;
  
    // Check that the distance is not too close
    // We can't be inside the head (just around...) !
    if (headDistanceMeters < INTERAURAL_DISTANCE/2.0)
        headDistanceMeters = INTERAURAL_DISTANCE/2.0;
  
    // Compute reference distance for the two ears
    BL_FLOAT earDistance =
        ComputeEarDistance(elevation, azimuth,
                           INTERAURAL_DISTANCE, headDistanceMeters, chan);
  
    // Compute the delay in seconds
    BL_FLOAT delaySeconds = earDistance/SOUND_SPEED_AIR;
  
    return delaySeconds;
}

// Overstated width
BL_FLOAT
Spatializer::ComputeDelayDistance(BL_FLOAT elevation, BL_FLOAT azimuth, int chan,
                                  BL_FLOAT sourceDistance)
{
    // 340 m/s
#define SOUND_SPEED_AIR     340.0
  
    // 22 cm
#define INTERAURAL_DISTANCE 0.22
  
    // Distance to the center of the head
    BL_FLOAT headDistanceMeters = sourceDistance;
  
    // Check that the distance is not too close
    // We can't be inside the head (just around...) !
    if (headDistanceMeters < INTERAURAL_DISTANCE/2.0)
        headDistanceMeters = INTERAURAL_DISTANCE/2.0;
  
    // Compute reference distance for the two ears
    BL_FLOAT earDistance =
        ComputeEarDistance(elevation, azimuth,
                           INTERAURAL_DISTANCE, headDistanceMeters, chan);
  
    // Compute the delay in seconds
    BL_FLOAT delaySeconds = earDistance/SOUND_SPEED_AIR;
  
    return delaySeconds;
}

void
Spatializer::NormalizeDelays(BL_FLOAT delays[2])
{  
    // Normalize the delays
  
    // Minimize the delays
    // to keep only the difference
    // => This looks correct in term of delay,
    // compared to the original dataset.
    if (delays[0] > delays[1])
    {
        delays[0] -= delays[1];
        delays[1] = 0.0;
    }
    else
    {
        delays[1] -= delays[0];
        delays[0] = 0.0;
    }
}

void
Spatializer::IncreaseDelays(BL_FLOAT delays[2], BL_FLOAT factor)
{
    BL_FLOAT coeff = factor*OVERSTATE_DELAY_FACTOR + 1.0;
  
    delays[0] *= coeff;
    delays[1] *= coeff;
}

void
Spatializer::ResampleImpulse(WDL_TypedBuf<BL_FLOAT> *impulse)
{
    BL_FLOAT sampleRate = GetSampleRate();
    BL_FLOAT impSampleRate = mHrtf->GetSampleRate();
  
    WDL_TypedBuf<BL_FLOAT> &resampImpulse = mTmpBuf3;
    resampImpulse = *impulse;
  
    // Use the version that dos not fill with zeros if there misses some
    // few samples in the result
    FftConvolver6::ResampleImpulse2(&resampImpulse, impSampleRate, sampleRate);
  
    *impulse = resampImpulse;
}

#if USE_FIXED_BUFFER_OBJ
void
Spatializer::UpdateLatency()
{
    int latency = BUFFER_SIZE;
  
    int blockSize = GetBlockSize();
    if (blockSize < BUFFER_SIZE)
    {
        // If block size if too small, we will use the FixedBufferObj
        // then we must add its latency
        latency = BUFFER_SIZE + BUFFER_SIZE;
    }
  
    SetLatency(latency);
}
#endif
