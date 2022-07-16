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

#include <WavesProcess.h>

// Include camera angles bounds
#include <WavesRender.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <IGUIResizeButtonControl.h>

#include <IBLSwitchControl.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "Wav3s.h"

#include "IPlug_include_in_plug_src.h"

#define BUFFER_SIZE 2048
#define OVERSAMPLING 4
#define FREQ_RES 1
#define VARIABLE_HANNING 0
#define KEEP_SYNTHESIS_ENERGY 0

// GUI Size
#define NUM_GUI_SIZES 3

#define PLUG_WIDTH_MEDIUM 920
#define PLUG_HEIGHT_MEDIUM 640

#define PLUG_WIDTH_BIG 1040
#define PLUG_HEIGHT_BIG 722

#define DB_SCALE_MIN_DB -60.0

// Optimization
#define OPTIM_SKIP_IFFT 1

#define USE_DROP_DOWN_MENU 1

#if 0
NOTE: the performances look very bad now (very high cpu with high density)

ADD on website: fully automatable view camera movements!

TODO: use the new CustomDrawer::NeedRedraw() mechanism (in LinesRender2)

VERY IMPORTANT BEFORE RELEASING!!!
TODO: => fix some curve jitttering with low densities
(for the moment, some tests have been done WIP,
 and curves are jittering also with higher densities)
And check the performances (near 100% on Mac1, 60fps, density max, speed max)

TODO: add a parameter for line depth (to make the 80s grid / neromancer effect)
                                      
TODO: add Mel scale (real) for frequencies

TODO: optimize the points rendering, by rendring all the points at the same time (nvgQuadS)

TODO/OPTIM: make a full float version, to see if it optimizes

CRASH: Tony Nekrews from Facebook group: "Crashed Bitwig on my Win10"

TODO: update with a new option, to choose the color of the lines
(request from: ryannias@hotmail.com)
(white, red, green, blue, orange...)
=> green !! (like the color of his studio)

PROBLEM: in db mode, the amp axis is not totally well aligned with the signal
(there is a hack - HACK_DB_SCALE - , but it is not sufficient)

TODO: hide lines, manage data as faces, hiding lines

TODO: add 4th button for gui size retina

TODO: make a real ORXX game, with score (make canyons + tunnels - set floor and ceil data - )
=> make a cheat code "yoko" (like yoko ohno too), and send it only to some blogs

PROBLEM: Reason, Mac => when moving the view with the mouse, it jitters
=> the mouse event seem to be received hacked...

Comments on vst4free:
http://www.vst4free.com/free_vst.php?plugin=Wav3s&id=2968
- "Can't load in Studio One"
- "Smashes the CPU and causes audio drop outs in Ableton."
- "The ability to change the color, a recording function and a slightly lighter operation would be a wonder."
- "Does not work well in Cubase 10. A lot of noise,"

FLStudio: all the graphics plugins crackle on Windows 10 (Niko test). A bit better if FLStudio ASIO driver + high buffer size, but still crackles

- TEST: check automations with WaveForm Tracktion

Inspiration: ORXX / Joy Division / RadioHead Kinect

IDEA:
For low densities
- make a new algo:
- incomming slices marked with target slices indices
(so they stay associated with the same index)
- when changing mode, or changing density etc, indices are reset, and full assoc is redone
- when enough input slice arrived, the index is incremented by 1
- make a struct Slice(), with vector of points, and assoc index

#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";
static char *tooltipMonitor = "Monitor - Toggle monitor on/off";
static char *tooltipMode = "Mode - Display mode";
static char *tooltipSpeed = "Speed - Scroll speed";
static char *tooltipDensity = "Density - Display density";
static char *tooltipScale = "Scale - Amplitude scale";
static char *tooltipScrollDir = "Scroll - Scroll direction";
static char *tooltipShowAxes = "Axes - Show axes";
static char *tooltipDBScale = "DB Scale: Amplitude in DB";
static char *tooltipColor = "Color - Display color";

enum EParams
{
    kMode = 0,
    kSpeed,
    kDensity,
    kScale,
    
    kScrollDir,
    kShowAxes,
    
    kDBScale,
    kBLColor,
    
    kMonitor,

    kAngle0,
    kAngle1,
    kCamFov,
    
    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    
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
    
    kSpeedX = 177,
    kSpeedY = 435,
    
    kDensityX = 259,
    kDensityY = 435,
    
    kScaleX = 341,
    kScaleY = 435,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsModeX = 150,
    kRadioButtonsModeY = 441,
    kRadioButtonModeVSize = 72,
    kRadioButtonModeNumButtons = 4,
#else
    kModeX = 56, //66,
    kModeY = 438,
    kModeWidth = 100, //80,
#endif
    
    // GUI size
    kGUISizeSmallX = 12,
    kGUISizeSmallY = 430,
    
    kGUISizeMediumX = 12,
    kGUISizeMediumY = 453,
    
    kGUISizeBigX = 12,
    kGUISizeBigY = 476,
    
    kRadioButtonsScrollDirX = 419,
    kRadioButtonsScrollDirY = 449,
    kRadioButtonScrollDirVSize = 44,
    kRadioButtonScrollDirNumButtons = 2,
    
    kShowAxesX = 578,
    kShowAxesY = 432,
    
    kDBScaleX = 578,
    kDBScaleY = 476,

    // Color
#if !USE_DROP_DOWN_MENU
    kRadioButtonsColorX = 614,
    kRadioButtonsColorY = 441,
    kRadioButtonColorVSize = 108,
    kRadioButtonColorNumButtons = 6,
#else
    kBLColorX = 691,
    kBLColorY = 440,
    kBLColorWidth = 80,
#endif
    
    kMonitorX = 704,
    kMonitorY = 476
};


//
Wav3s::Wav3s(const InstanceInfo &info)
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

Wav3s::~Wav3s()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mWavesProcess != NULL)
        delete mWavesProcess;
    
    if (mWavesRender != NULL)
        delete mWavesRender;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Wav3s::MyMakeGraphics()
{
    int newGUIWidth;
    int newGUIHeight;
    GetNewGUISize(mGUISizeIdx, &newGUIWidth, &newGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            newGUIWidth, newGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);

    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       newGUIWidth, newGUIHeight,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
Wav3s::MyMakeLayout(IGraphics *pGraphics)
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
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);

    pGraphics->EnableTooltips(true);
    pGraphics->SetTooltipsDelay(TOOLTIP_DELAY);
    
    CreateControls(pGraphics, mGUIOffsetY);
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mFftObj = NULL;
    mWavesProcess = NULL;
    
    mGraph = NULL;
    
    mWavesRender = NULL;
    
    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    // Dummy values, to avoid undefine (just in case)
    mGraphWidthSmall = 256;
    mGraphHeightSmall = 256;
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
    mIsPlaying = false;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
Wav3s::InitParams()
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
    enum LinesRender2::Mode defaultMode = LinesRender2::LINES_FREQ;
    mMode = defaultMode;

#if !USE_DROP_DOWN_MENU
    GetParam(kMode)->InitInt("Mode", defaultMode, 0, 3);
#else
    GetParam(kMode)->InitEnum("Mode", 0, 4, "", IParam::kFlagsNone,
                                  "", "Lines Freq", "Lines Time", "Points", "Grid");
#endif
    
    // Scale
    BL_FLOAT defaultScale = 50.0;
    mScale = defaultScale;
    GetParam(kScale)->InitDouble("Scale", defaultScale, 0.0, 100.0, 0.1, "%");
    
    // Scroll direction
    enum LinesRender2::ScrollDirection defaultScrollDir = LinesRender2::BACK_FRONT;
    mScrollDir = defaultScrollDir;
    //GetParam(kScrollDir)->InitInt("ScrollDir", defaultScrollDir, 0, 1);
    GetParam(kScrollDir)->InitEnum("ScrollDir", defaultScrollDir, 2,
                                   "", IParam::kFlagsNone, "",
                                   "BackToFront", "FrontToBack");
    
    // Show axis
    int defaultShowAxes = 0;
    mShowAxes = defaultShowAxes;
    //GetParam(kShowAxes)->InitInt("ShowAxes", defaultShowAxes, 0, 1);
    GetParam(kShowAxes)->InitEnum("ShowAxes", defaultShowAxes, 2,
                                  "", IParam::kFlagsNone, "",
                                  "Off", "On");
    
    // DB scale
    int defaultDBScale = 0;
    mDBScale = defaultDBScale;
    //GetParam(kDBScale)->InitInt("dBScale", defaultDBScale, 0, 1);
    GetParam(kDBScale)->InitEnum("dBScale", defaultDBScale, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
    
    // Color
    enum Color defaultColor = COLOR_WHITE;
    mColor = defaultColor;
#if !USE_DROP_DOWN_MENU
    GetParam(kBLColor)->InitInt("Color", defaultColor, 0, 5);
#else
    GetParam(kBLColor)->InitEnum("Color", 0, 6, "", IParam::kFlagsNone,
                                 "", "White", "Blue", "Green",
                                 "Red", "Orange", "Purple");
#endif
    
    // Angle parameters
    //
    
    // Crashed Logic when moving an automation point
    // (utf8 coding or something, made crash Logic)
    //const char *degree = "°";
    
#if 0
    // BUG: crashes Studio One (Sierra, AU)
    // BETTER: Works: don't crash Logic (but Logic won't display the unit near the automation point)
    const char *degree = "\xB0";
#endif
    
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
    BL_FLOAT defaultAngle1 = MIN_CAM_ANGLE_1;
    mAngle1 = defaultAngle1;
    GetParam(kAngle1)->InitDouble("Angle1", defaultAngle1,
                                  MIN_CAM_ANGLE_1, MAX_CAM_ANGLE_1, 0.1, degree);
    
    // CamFov
    BL_FLOAT defaultCamFov = MAX_FOV;
    mCamFov = defaultCamFov;
    GetParam(kCamFov)->InitDouble("CamFov", defaultCamFov, MIN_FOV, MAX_FOV, 0.1, degree);
    
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
    
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
}

void
Wav3s::ApplyParams()
{
    if (mWavesRender != NULL)
    {
        BL_FLOAT speed = mSpeed;

        AdjustSpeedSR(&speed);
        
        mWavesRender->SetSpeed(speed);
    }
    
    if (mWavesRender != NULL)
        mWavesRender->SetDensity(mDensity);
    
    if (mWavesRender != NULL)
        mWavesRender->SetScale(mScale);
    
    if (mWavesRender != NULL)
        mWavesRender->SetMode(mMode);
    
    if (mWavesRender != NULL)
        mWavesRender->SetScrollDirection(mScrollDir);
    
    if (mWavesRender != NULL)
        mWavesRender->SetShowAxes(mShowAxes);
    
    if (mWavesRender != NULL)
        mWavesRender->SetDBScale(mDBScale, DB_SCALE_MIN_DB);
    
    if (mWavesRender != NULL)
        mWavesRender->SetCamAngle0(mAngle0);
    
    if (mWavesRender != NULL)
        mWavesRender->SetCamAngle1(mAngle1);
    
    if (mWavesRender != NULL)
        mWavesRender->SetCamFov(mCamFov);
    
    SetColor(mColor);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Wav3s::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mFftObj == NULL)
    {
        int numChannels = 1;
        int numScInputs = 0;
        
        mWavesProcess = new WavesProcess(BUFFER_SIZE,
                                         oversampling, freqRes,
                                         sampleRate);
        
        vector<ProcessObj *> processObjs;
        processObjs.push_back(mWavesProcess);
        
        
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
    }
    else
    {
        mFftObj->Reset(BUFFER_SIZE, oversampling, freqRes, sampleRate);
    }
    
    CreateWavesRender(true);
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
Wav3s::ProcessBlock(iplug::sample **inputs,
                    iplug::sample **outputs, int nFrames)
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
    
    // FIX for Logic (no scroll)
    // IsPlaying() should be called from the audio thread
    bool isPlaying = IsTransportPlaying();
    mIsPlaying = isPlaying;
    
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
    
    if (isPlaying || mMonitorEnabled)
    {
        vector<WDL_TypedBuf<BL_FLOAT> > &in0 = mTmpBuf5;
        in0.resize(1);
        in0[0] = in[0];
    
        if (in.size() == 2)
        {
            WDL_TypedBuf<BL_FLOAT> &mono = mTmpBuf4;
            BLUtils::StereoToMono(&mono, in[0], in[1]);
            
            in0[0] = mono;
        }
        
        vector<WDL_TypedBuf<BL_FLOAT> > dummy;
        mFftObj->Process(in0, dummy, &out);
    }
    
    // The plugin does not modify the sound, so bypass
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    
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
Wav3s::CreateControls(IGraphics *pGraphics, int offset)
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
    
    if (mWavesRender != NULL)
        mWavesRender->SetGraph(mGraph);
    
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
    
    // Speed
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSpeedX, kSpeedY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSpeed,
                              TEXTFIELD_FN,
                              "SPEED",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSpeed);
    
    // Density
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kDensityX, kDensityY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kDensity,
                              TEXTFIELD_FN,
                              "DENSITY",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipDensity);
    
    
    // Mode
#if !USE_DROP_DOWN_MENU
    const char *radioLabelsMode[kRadioButtonModeNumButtons] =
    { "LINES FREQ", "LINES TIME", "POINTS", "GRID" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeX,
                                   kRadioButtonsModeY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeNumButtons,
                                   kRadioButtonModeVSize,
                                   kMode,
                                   false,
                                   "MODE",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsMode);
#else
    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kModeX, kModeY + offset,
                                   kModeWidth,
                                   kMode,
                                   "MODE",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipMode);
#endif
    
    // Scale
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kScaleX, kScaleY + offset,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kScale,
                              TEXTFIELD_FN,
                              "SCALE",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipScale);
    
    
    // Scroll direction
    const char *radioLabelsScrollDir[kRadioButtonScrollDirNumButtons] =
        { "BACK->FRONT", "FRONT->BACK" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsScrollDirX,
                                   kRadioButtonsScrollDirY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonScrollDirNumButtons,
                                   kRadioButtonScrollDirVSize,
                                   kScrollDir,
                                   false,
                                   "SCROLL",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsScrollDir,
                                   tooltipScrollDir);
    
    // Show axes
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kShowAxesX,
                                   kShowAxesY + offset,
                                   CHECKBOX_FN, kShowAxes, "AXES",
                                   GUIHelper12::SIZE_DEFAULT, true,
                                   tooltipShowAxes);
    
    // DB scale
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kDBScaleX,
                                   kDBScaleY + offset,
                                   CHECKBOX_FN, kDBScale, "DB SCALE",
                                   GUIHelper12::SIZE_DEFAULT, true,
                                   tooltipDBScale);
    
    // Color
#if !USE_DROP_DOWN_MENU
    const char *radioLabelsColor[kRadioButtonColorNumButtons] =
    { "WHITE", "BLUE", "GREEN", "RED", "ORANGE", "PURPLE" };
    
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsColorX,
                                   kRadioButtonsColorY + offset,
                                   RADIOBUTTON_FN,
                                   kRadioButtonColorNumButtons,
                                   kRadioButtonColorVSize,
                                   kBLColor,
                                   false,
                                   "COLOR",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsColor);
#else
    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kBLColorX, kBLColorY + offset,
                                   kBLColorWidth,
                                   kBLColor,
                                   "COLOR",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipColor);
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
    
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kMonitorX,
                                                     kMonitorY + offset,
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
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    CreateWavesRender(false);
    
    mControlsCreated = true;
}

void
Wav3s::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
Wav3s::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    if (mFftObj != NULL)
        mFftObj->Reset();
    if (mWavesProcess != NULL)
        mWavesProcess->Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    if (mWavesRender != NULL)
        mWavesRender->Reset(sampleRate);

    if (mWavesRender != NULL)
    {
        BL_FLOAT speed = mSpeed;
        AdjustSpeedSR(&speed);
        mWavesRender->SetSpeed(speed);
    }
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Wav3s::SetColor(enum Color col)
{
    switch (col)
    {
        case COLOR_WHITE:
        {
            unsigned char color0[4] = { 128, 128, 255, 255 };
            unsigned char color1[4] = { 255, 255, 255, 255 };
            
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        case COLOR_BLUE:
        {
            unsigned char color0[4] = { 0, 0, 255, 255 };
            unsigned char color1[4] = { 190, 190, 255, 255 };
        
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        case COLOR_GREEN:
        {
            unsigned char color0[4] = { 0, 255, 0, 255 };
            unsigned char color1[4] = { 255, 255, 255, 255 };
            
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        case COLOR_RED:
        {
            unsigned char color0[4] = { 255, 0, 0, 255 };
            unsigned char color1[4] = { 255, 255, 255, 255 };
            
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        case COLOR_ORANGE:
        {
            unsigned char color0[4] = { 255, 128, 0, 255 };
            unsigned char color1[4] = { 255, 255, 255, 255 };
            
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        case COLOR_PURPLE:
        {
            unsigned char color0[4] = { 120, 0, 200, 255 };
            unsigned char color1[4] = { 230, 190, 255, 255 };
            
            if (mWavesRender != NULL)
                mWavesRender->SetColors(color0, color1);
        }
        break;
            
        default:
            break;
    }
}

void
Wav3s::OnParamChange(int paramIdx)
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

            AdjustSpeedSR(&speed);
            
            if (mWavesRender != NULL)
                mWavesRender->SetSpeed(speed);
        }
        break;
            
        case kDensity:
        {
            BL_FLOAT density = GetParam(kDensity)->Value();
            density = density/100.0;
            mDensity = density;
            
            if (mWavesRender != NULL)
                mWavesRender->SetDensity(density);
        }
        break;
            
        case kScale:
        {
            BL_FLOAT scale = GetParam(kScale)->Value();
            scale = scale/100.0;
            mScale = scale;
            
            if (mWavesRender != NULL)
                mWavesRender->SetScale(scale);
        }
        break;
            
        case kMode:
        {
            enum LinesRender2::Mode mode =
                (enum LinesRender2::Mode)GetParam(kMode)->Int();
            mMode = mode;
            
            if (mWavesRender != NULL)
                mWavesRender->SetMode(mode);
        }
        break;
            
        case kScrollDir:
        {
            enum LinesRender2::ScrollDirection dir =
            (enum LinesRender2::ScrollDirection)GetParam(kScrollDir)->Int();
            mScrollDir = dir;
            
            if (mWavesRender != NULL)
                mWavesRender->SetScrollDirection(dir);
        }
        break;
            
        case kShowAxes:
        {
            int showAxes = GetParam(kShowAxes)->Value();
            mShowAxes = showAxes;
            
            if (mWavesRender != NULL)
                mWavesRender->SetShowAxes(showAxes);
        }
        break;
            
        case kDBScale:
        {
            int dBScale = GetParam(kDBScale)->Value();
            mDBScale = dBScale;
            
            if (mWavesRender != NULL)
                mWavesRender->SetDBScale(dBScale, DB_SCALE_MIN_DB);
        }
        break;
            
        case kAngle0:
        {
            BL_FLOAT angle = GetParam(kAngle0)->Value();
            mAngle0 = angle;
            
            if (mWavesRender != NULL)
                mWavesRender->SetCamAngle0(angle);
        }
        break;
            
        case kAngle1:
        {
            BL_FLOAT angle = GetParam(kAngle1)->Value();
            mAngle1 = angle;
            
            if (mWavesRender != NULL)
                mWavesRender->SetCamAngle1(angle);
        }
        break;
            
        case kCamFov:
        {
            BL_FLOAT angle = GetParam(kCamFov)->Value();
            mCamFov = angle;
            
            if (mWavesRender != NULL)
                mWavesRender->SetCamFov(angle);
        }
        break;
            
        case kBLColor:
        {
            enum Color col = (enum Color)GetParam(kBLColor)->Int();
            mColor = col;
            
            SetColor(col);
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
            
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            
            mMonitorEnabled = (value == 1);
        }
        break;
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    ApplyParams();
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;
    
    mMonitorControl = NULL;
    
    if (mWavesRender != NULL)
        mWavesRender->SetGraph(NULL);

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::PreResizeGUI(int guiSizeIdx,
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
    
    if (mWavesRender != NULL)
        mWavesRender->SetGraph(NULL);
    
    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::GUIResizeParamChange(int guiSizeIdx)
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
Wav3s::GetNewGUISize(int guiSizeIdx, int *width, int *height)
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
Wav3s::SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1)
{
    BL_FLOAT normAngle0 = (angle0 + MAX_CAM_ANGLE_0)/(MAX_CAM_ANGLE_0*2.0);
    GetParam(kAngle0)->SetNormalized(normAngle0);

    mAngle0 = angle0;
    
    BL_FLOAT normAngle1 =
        (angle1 - MIN_CAM_ANGLE_1)/(MAX_CAM_ANGLE_1 - MIN_CAM_ANGLE_1);
    GetParam(kAngle1)->SetNormalized(normAngle1);

    mAngle1 = angle1;
}

void
Wav3s::SetCameraFov(BL_FLOAT angle)
{
    BL_FLOAT normAngle = (angle - MIN_FOV)/(MAX_FOV - MIN_FOV);
    GetParam(kCamFov)->SetNormalized(normAngle);

    mCamFov = angle;
}

void
Wav3s::CreateWavesRender(bool createFromInit)
{
    if (!createFromInit && (mWavesProcess == NULL))
        return;
    
    if (mWavesRender != NULL)
    {
        if (mGraph != NULL)
            mWavesRender->SetGraph(mGraph);
        
        return;
    }
    
    BL_FLOAT sampleRate = GetSampleRate();
    mWavesRender = new WavesRender(this, mGraph, BUFFER_SIZE, sampleRate);
    mWavesProcess->SetWavesRender(mWavesRender);
}

void
Wav3s::OnIdle()
{
    ENTER_PARAMS_MUTEX;
    
    if (mUIOpened)
    {
        if (mMonitorControl != NULL)
        {
            bool disabled = mMonitorControl->IsDisabled();
            if (mIsPlaying != disabled)
                mMonitorControl->SetDisabled(mIsPlaying);
        }
        
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Wav3s::AdjustSpeedSR(BL_FLOAT *speed)
{
    BL_FLOAT coeff = 44100.0/GetSampleRate();

    *speed = *speed*coeff;
}
