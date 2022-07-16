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

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>

#include <BlaTimer.h>

#include <Panel.h>
#include <ITabsBarControl.h>
#include <GraphControl12.h>
#include <IRadioButtonsControlCustom.h>
#include <IBLSwitchControl.h>
#include <IXYPadControlExt.h>
#include <IIconLabelControl.h>

#include <MorphoObj.h>
#include <SoSourcesView.h> // for listener

#include "Morpho.h"

#include "IPlug_include_in_plug_src.h"

// 428/193
#define XY_PAD_RATIO 2.217

static char *tooltipHelp = "Help - Display Help";

// Common
/*static char *tooltipSynthMode = "Synthesis panel";
  static char *tooltipSourcesMode = "Sources panel";*/
static char *tooltipPlugMode = "Synthesis mode / Sources mode";

// Sources
static char *tooltipSoNewLiveSource = "Create a new live source";
static char *tooltipSoNewFileSource = "Create a new file source";
static char *tooltipSoPlay = "Play the current source";
static char *tooltipSoApply = "Process the current source with the current settings";
static char *tooltipSoSpectroBrightness = "Spectrogram brightness";
static char *tooltipSoSpectroContrast = "Spectrogram contrast";
static char *tooltipSoSpectroSpecWave = "Spectrogram/Waveform";
static char *tooltipSoSpectroWaveScale = "Waveform scale";
static char *tooltipSoSpectroSelectionType =
    "Selection type: rectangular, horizontal or vertical";
static char *tooltipSoWaterfallViewMode = "View mode";
static char *tooltipSoSourceMaster = "Set the current source as master source";
static char *tooltipSoSourceType = "Source type";
static char *tooltipSoTimeSmooth = "Smooth input data over time";
static char *tooltipSoDetectThrs = "Peaks detection threshold";
static char *tooltipSoFreqThrs = "Partials frequency threshold";
static char *tooltipSoSourceGain = "Current source gain";
// Synthesis
static char *tooltipSyWaterfallLockMix = "Lock view to mix mode";
static char *tooltipSySourceLabel = "Source name";
static char *tooltipSySourceSolo = "Solo current source";
static char *tooltipSySourceMute = "Mute current source";
static char *tooltipSySourceMaster = "Set the current source as master source";
static char *tooltipSySourceAmp = "Source amplitude";
static char *tooltipSySourceAmpSolo = "Solo amplitude of current source";
static char *tooltipSySourceAmpMute = "Mute amplitude of current source";
static char *tooltipSySourcePitch = "Source pitch";
static char *tooltipSySourcePitchSolo = "Solo pitch of current source";
static char *tooltipSySourcePitchMute = "Mute pitch of current source";
static char *tooltipSySourceColor = "Source color";
static char *tooltipSySourceColorSolo = "Solo color of current source";
static char *tooltipSySourceColorMute = "Mute color of current source";
static char *tooltipSySourceWarping = "Source warping";
static char *tooltipSySourceWarpingSolo = "Solo warping of current source";
static char *tooltipSySourceWarpingMute = "Mute warping of current source";
static char *tooltipSySourceNoise = "Source noise";
static char *tooltipSySourceNoiseSolo = "Solo noise of current source";
static char *tooltipSySourceNoiseMute = "Mute noise of current source";
static char *tooltipSySourceReverse = "Play the source reversed";
static char *tooltipSySourcePingPong = "Play the source in ping pong mode";
static char *tooltipSySourceFreeze = "Freeze the source playback";
static char *tooltipSySourceSynthType = "Synthesis type for the current source";
static char *tooltipSyLoop = "Play in looping mode";
static char *tooltipSyPadTrack = "Mix pad";
static char *tooltipSyPadTrackX = "X position of the mix marker";
static char *tooltipSyPadTrackY = "Y position of the mix marker";
static char *tooltipSyTimeStretch = "Time stretch factor";
static char *tooltipSyOutGain = "Global output gain";

enum EParams
{
    // Common
    kPlugMode = 0,

    // Sources mode
    //
    
    // tabs bar?
    kSoNewLiveSource,
    kSoNewFileSource,
    
    kSoPlay,
    kSoApply,

    kSoSpectroBrightness,
    kSoSpectroContrast,
    kSoSpectroSpecWave,
    kSoSpectroWaveScale,

    kSoSpectroSelectionType,
    
    kSoWaterfallAngle0,
    kSoWaterfallAngle1,
    kSoWaterfallCamFov,

    kSoWaterfallViewMode,
    kSoSourceMaster,
    kSoSourceType,

    kSoTimeSmooth,
    kSoDetectThrs,
    kSoFreqThrs,
    kSoSourceGain,

    // Synthesys mode
    //
    
    kSyWaterfallAngle0,
    kSyWaterfallAngle1,
    kSyWaterfallCamFov,

    kSyWaterfallLockMix,

    // filename label => no need param

    kSySourceSolo,
    kSySourceMute,
    kSySourceMaster,

    kSySourceAmp,
    kSySourceAmpSolo,
    kSySourceAmpMute,

    kSySourcePitch,
    kSySourcePitchSolo,
    kSySourcePitchMute,

    kSySourceColor,
    kSySourceColorSolo,
    kSySourceColorMute,

    kSySourceWarping,
    kSySourceWarpingSolo,
    kSySourceWarpingMute,

    kSySourceNoise,
    kSySourceNoiseSolo,
    kSySourceNoiseMute,

    kSySourceReverse,
    kSySourcePingPong,
    kSySourceFreeze,

    kSySourceSynthType,

    kSyLoop,
    
    // Handle 0 is "mix", other are "sources"
    kSyPadHandle0X,
    kSyPadHandle0Y,
    
    kSyPadHandle1X,
    kSyPadHandle1Y,

    kSyPadHandle2X,
    kSyPadHandle2Y,

    kSyPadHandle3X,
    kSyPadHandle3Y,

    kSyPadHandle4X,
    kSyPadHandle4Y,

    kSyPadHandle5X,
    kSyPadHandle5Y,

    kSyTimeStretch,
    kSyOutGain,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,

    kKnobSmallWidth = 36,
    kKnobSmallHeight = 36,

    kPlugModeX = 12,
    kPlugModeY = 13,
    kPlugModeHSize = 1000,
    kPlugModeNumButtons = 2,
    kPlugModeFrames = 3,
    
    // Sources mode
    //
    
    // tabs bar?
    kSoTabsBarX = 12,
    kSoTabsBarY = 42,
    kSoTabsBarW = 800,
    kSoTabsBarH = 22,
    
    kSoNewFileSourceX = 919,
    kSoNewFileSourceY = 42,
    
    kSoNewLiveSourceX = 819,
    kSoNewLiveSourceY = 42,
    
    kSoPlayX = 465,
    kSoPlayY = 72,
    
    kSoApplyX = 919,
    kSoApplyY = 72,

    kSoSpectroGraphX = 12,
    kSoSpectroGraphY = 102,
    
    kSoSpectroBrightnessX = 34,
    kSoSpectroBrightnessY = 575,

    kSoSpectroContrastX = 134,
    kSoSpectroContrastY = 575,
    
    kSoSpectroSpecWaveX = 234,
    kSoSpectroSpecWaveY = 575,

    kSoSpectroWaveScaleX = 334,
    kSoSpectroWaveScaleY = 575,

    kSoSpectroSelectionTypeX = 416,
    kSoSpectroSelectionTypeY = 605,
    kSoSpectroSelectionTypeHSize = 70,
    kSoSpectroSelectionTypeNumButtons = 3,
    kSoSpectroSelectionTypeFrames = 3,
    
    kSoWaterfallGraphX = 516,
    kSoWaterfallGraphY = 102,
    
    kSoWaterfallViewModeX = 522,
    kSoWaterfallViewModeY = 536,
    kSoWaterfallViewModeWidth = 90,
    
    kSoSourceMasterX = 717,
    kSoSourceMasterY = 518,

    kSoSourceTypeX = 886,
    kSoSourceTypeY = 536,
    kSoSourceTypeWidth = 130,

    kSoTimeSmoothX = 606,
    kSoTimeSmoothY = 576,
    
    kSoDetectThrsX = 706,
    kSoDetectThrsY = 576,

    kSoFreqThrsX = 803,
    kSoFreqThrsY = 576,

    kSoSourceGainX = 903,
    kSoSourceGainY = 576,

    // Synthesys mode
    //
    
    kSyWaterfallGraphX = 8,
    kSyWaterfallGraphY = 42,
    
    kSyWaterfallLockMixX = 402,
    kSyWaterfallLockMixY = 416,

    kSySourceLabelX = 512,
    kSySourceLabelY = 42,
    kSySourceLabelIconOffsetX = 6,
    kSySourceLabelIconOffsetY = 4,
    kSySourceLabelIconFrames = 18,
    kSySourceLabelTextOffsetX = 24,
    kSySourceLabelTextOffsetY = 2,
    
    kSySourceSoloX = 707,
    kSySourceSoloY = 76,
    
    kSySourceMuteX = 735,
    kSySourceMuteY = 76,

    kSySourceMasterX = 797,
    kSySourceMasterY = 76,

    kSySourceAmpX = 543,
    kSySourceAmpY = 146,
    
    kSySourceAmpSoloX = 537,
    kSySourceAmpSoloY = 230,

    kSySourceAmpMuteX = 565,
    kSySourceAmpMuteY = 230,

    kSySourcePitchX = 643,
    kSySourcePitchY = 146,
    
    kSySourcePitchSoloX = 637,
    kSySourcePitchSoloY = 230,
    
    kSySourcePitchMuteX = 665,
    kSySourcePitchMuteY = 230,

    kSySourceColorX = 741,
    kSySourceColorY = 146,
    
    kSySourceColorSoloX = 735,
    kSySourceColorSoloY = 230,
    
    kSySourceColorMuteX = 763,
    kSySourceColorMuteY = 230,

    kSySourceWarpingX = 842,
    kSySourceWarpingY = 146,
    
    kSySourceWarpingSoloX = 836,
    kSySourceWarpingSoloY = 230,
    
    kSySourceWarpingMuteX = 864,
    kSySourceWarpingMuteY = 230,

    kSySourceNoiseX = 941,
    kSySourceNoiseY = 146,
    
    kSySourceNoiseSoloX = 935,
    kSySourceNoiseSoloY = 230,
    
    kSySourceNoiseMuteX = 963,
    kSySourceNoiseMuteY = 230,
    
    kSySourceReverseX = 539,
    kSySourceReverseY = 374,
    
    kSySourcePingPongX = 539,
    kSySourcePingPongY = 400,
    
    kSySourceFreezeX = 539,
    kSySourceFreezeY = 426,

    kSySourceSynthTypeX = 892,
    kSySourceSynthTypeY = 391,
    kSySourceSynthTypeWidth = 130,

    kSyLoopX = 539,
    kSyLoopY = 573,
    
    // Handle 0 is "mix", other are "sources"
    kSyPadTrackX = 8,
    kSyPadTrackY = 459,
    kSyPadTrackBorderSize = 2,
    
    // Same as "handle 0"
    kSyKnobHandle0X_X = 456,
    kSyKnobHandle0X_Y = 466,
    
    kSyKnobHandle0Y_X = 456,
    kSyKnobHandle0Y_Y = 574,

    kSyTimeStretchX = 842,
    kSyTimeStretchY = 575,
    
    kSyOutGainX = 941,
    kSyOutGainY = 575
};

//
Morpho::Morpho(const InstanceInfo &info)
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

Morpho::~Morpho()
{
    if (mGUIHelper != NULL)
        delete mGUIHelper;

    if (mSourcesPanel != NULL)
        delete mSourcesPanel;
    if (mSynthPanel != NULL)
        delete mSynthPanel;
}

IGraphics *
Morpho::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics =
        MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, fps,
                     GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));

    return graphics;
}

void
Morpho::MyMakeLayout(IGraphics *pGraphics)
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

    pGraphics->LoadFont("OpenSans-Bold", FONT_OPENSANS_BOLD_FN);
    
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    
#if 0 // 1 // Debug
    pGraphics->ShowControlBounds(true);
    pGraphics->ShowAreaDrawn(true);
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
Morpho::InitNull()
{
    // Init WDL FFT
    FftProcessObj16::Init();
    
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;

    mSourcesPanel = NULL;
    mSynthPanel = NULL;

    mPlugMode = MORPHO_PLUG_MODE_SYNTH;

    mSoApplyButtonControl = NULL;

    mMorphoObj = NULL;
}

void
Morpho::Init()
{ 
    if (mIsInitialized)
        return;

    //
    mMorphoObj = new MorphoObj(this, this, XY_PAD_RATIO);
    mMorphoObj->SetSoSourcesViewListener(this);
    mMorphoObj->SetSySourcesViewListener(this);
    
    mIsInitialized = true;
}

void
Morpho::InitParams()
{
    GetParam(kPlugMode)->InitEnum("Mode", mPlugMode, 2,
                                  "", IParam::kFlagsNone, "",
                                  "Synthesis", "Sources");

    // Sources mode
    //
    
    // tabs bar?
    
    GetParam(kSoNewLiveSource)->InitInt("SoNewLiveSource", 0, 0, 1,
                                        "", IParam::kFlagMeta);

    GetParam(kSoNewFileSource)->InitInt("SoNewFileSource", 0, 0, 1,
                                        "", IParam::kFlagMeta);
    
    GetParam(kSoPlay)->InitEnum("SoPlay", 0, 2,
                                "", IParam::kFlagMeta, "",
                                "Stop", "Play");

    GetParam(kSoApply)->InitInt("SoApply", 0, 0, 1,
                                "", IParam::kFlagMeta);
    
    BL_FLOAT defaultBrightness = 0.0;
    GetParam(kSoSpectroBrightness)->InitDouble("kSoSpectroBrightness",
                                               defaultBrightness,
                                               -1.0, 1.0, 0.01, "");
    
    BL_FLOAT defaultContrast = 0.5;
    GetParam(kSoSpectroContrast)->InitDouble("SoSpectroContrast",
                                             defaultContrast,
                                             0.0, 1.0, 0.01, "");
    

    BL_FLOAT defaultSpectWave = 50.0;
    GetParam(kSoSpectroSpecWave)->InitDouble("kSoSpectroSpecWave",
                                             defaultSpectWave,
                                             0.0, 100.0, 0.1, "%");
    

    BL_FLOAT defaultWaveformScale = 1.0;
    GetParam(kSoSpectroWaveScale)->InitDouble("SoSpectroWaveScale",
                                              defaultWaveformScale,
                                              0.01, 8.0, 0.1, "x");
    
    SelectionType defaultSelectionType = RECTANGLE;
    GetParam(kSoSpectroSelectionType)->InitEnum("SoSpectroSelectionType",
                                       (int)defaultSelectionType, 3,
                                       "", IParam::kFlagsNone, "",
                                       "Rectangle", "Horizontal", "Vertical");

    const char *degree = " ";
    
    BL_FLOAT defaultSoAngle0 = WATERFALL_DEFAULT_CAM_ANGLE0;
    GetParam(kSoWaterfallAngle0)->InitDouble("SoWaterfallAngle0", defaultSoAngle0,
                                             -WATERFALL_MAX_CAM_ANGLE_0,
                                             WATERFALL_MAX_CAM_ANGLE_0,
                                             0.1, degree);
    
    BL_FLOAT defaultSoAngle1 = WATERFALL_DEFAULT_CAM_ANGLE1;
    GetParam(kSoWaterfallAngle1)->InitDouble("SoWaterfallAngle1",
                                             defaultSoAngle1,
                                             WATERFALL_MIN_CAM_ANGLE_1,
                                             WATERFALL_MAX_CAM_ANGLE_1, 0.1, degree);
    
    BL_FLOAT defaultSoCamFov = WATERFALL_DEFAULT_CAM_FOV;
    GetParam(kSoWaterfallCamFov)->InitDouble("kSoWaterfallCamFov",
                                             defaultSoCamFov,
                                             WATERFALL_MIN_FOV, WATERFALL_MAX_FOV,
                                             0.1, degree);
    
    GetParam(kSoWaterfallViewMode)->InitEnum("SoWaterfallViewMode",
                                             1, 7, "", IParam::kFlagsNone, "",
                                             "Input", "Harmonic", "Noise",
                                             "Detection", "Tracking",
                                             "Color", "Warping");
    
    GetParam(kSoSourceMaster)->InitEnum("kSoSourceMaster", 0, 2,
                                        "", IParam::kFlagsNone, "",
                                        "Off", "On");
    

    GetParam(kSoSourceType)->InitEnum("SoSourceType",
                                      2/*0*/, 3, "", IParam::kFlagsNone, "",
                                      "Bypass",
                                      "Monophonic",
                                      "Complex");
                                      //"Polyphonic/Inharmonic");
    
    GetParam(kSoTimeSmooth)->InitDouble("SoTimeSmooth", 50.0,
                                        0.0, 100.0, 0.1, "%");

    BL_FLOAT defaultDetectThreshold = 1.0; // 1%
    GetParam(kSoDetectThrs)->
        InitDouble("SoDetectThrs", defaultDetectThreshold, 0.0, 100.0, 0.1, "%");

    BL_FLOAT defaultFreqThreshold = 25.0; // 50.0
    GetParam(kSoFreqThrs)->
        InitDouble("SoFreqThrs", defaultFreqThreshold, 0.0, 100.0, 0.1, "%");
    
    BL_FLOAT defaultSourceGain = 0.0;
    GetParam(kSoSourceGain)->InitDouble("SoSourceGain", defaultSourceGain,
                                        -16.0, 16.0, 0.1, "dB");

    // Synthesys mode
    //
    BL_FLOAT defaultSyAngle0 = 0.0;
    GetParam(kSyWaterfallAngle0)->InitDouble("SyWaterfallAngle0", defaultSyAngle0,
                                             -WATERFALL_MAX_CAM_ANGLE_0,
                                             WATERFALL_MAX_CAM_ANGLE_0,
                                             0.1, degree);
    
    BL_FLOAT defaultSyAngle1 = WATERFALL_MIN_CAM_ANGLE_1;
    GetParam(kSyWaterfallAngle1)->InitDouble("SyWaterfallAngle1",
                                             defaultSyAngle1,
                                             WATERFALL_MIN_CAM_ANGLE_1,
                                             WATERFALL_MAX_CAM_ANGLE_1, 0.1, degree);
    
    BL_FLOAT defaultSyCamFov = WATERFALL_MAX_FOV;
    GetParam(kSyWaterfallCamFov)->InitDouble("kSyWaterfallCamFov",
                                             defaultSyCamFov,
                                             WATERFALL_MIN_FOV, WATERFALL_MAX_FOV,
                                             0.1, degree);

    GetParam(kSyWaterfallLockMix)->InitEnum("SyWaterfallLockMix", 0, 2,
                                            "", IParam::kFlagsNone, "",
                                            "Off", "On");

    // filename label => no need param

    GetParam(kSySourceSolo)->InitEnum("SySourceSolo", 0, 2,
                                      "", IParam::kFlagMeta, "",
                                      "Off", "On");

    GetParam(kSySourceMute)->InitEnum("SySourceMute", 0, 2,
                                      "", IParam::kFlagMeta, "",
                                      "Off", "On");

    GetParam(kSySourceMaster)->InitEnum("SySourceMaster", 0, 2,
                                        "", IParam::kFlagMeta, "",
                                        "Off", "On");

    GetParam(kSySourceAmp)->InitDouble("SySourceAmp", 0.0,
                                       -100.0, 100.0, 0.1, "%");
    
    GetParam(kSySourceAmpSolo)->InitEnum("SySourceAmpSolo", 0, 2,
                                         "", IParam::kFlagMeta, "",
                                         "Off", "On");

    GetParam(kSySourceAmpMute)->InitEnum("SySourceAmpMute", 0, 2,
                                         "", IParam::kFlagMeta, "",
                                         "Off", "On");
    
    //GetParam(kSySourcePitch)->InitDouble("SySourcePitch", 0.0,
    //                                     -100.0, 100.0, 0.1, "%");
    GetParam(kSySourcePitch)->InitDouble("SySourcePitch", 0.01,
                                         -12.0, 12.0, 0.1, "ht");
                                         
    GetParam(kSySourcePitchSolo)->InitEnum("SySourcePitchSolo", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");

    GetParam(kSySourcePitchMute)->InitEnum("SySourcePitchMute", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");
       
    GetParam(kSySourceColor)->InitDouble("SySourceColor", 0.0,
                                         -100.0, 100.0, 0.1, "%");
 
    GetParam(kSySourceColorSolo)->InitEnum("SySourceColorSolo", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");

    GetParam(kSySourceColorMute)->InitEnum("SySourceColorMute", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");

    GetParam(kSySourceWarping)->InitDouble("SySourceWarping", 0.0,
                                           -100.0, 100.0, 0.1, "%");
 
    GetParam(kSySourceWarpingSolo)->InitEnum("SySourceWarpingSolo", 0, 2,
                                             "", IParam::kFlagMeta, "",
                                             "Off", "On");

    GetParam(kSySourceWarpingMute)->InitEnum("SySourceWarpingMute", 0, 2,
                                             "", IParam::kFlagMeta, "",
                                             "Off", "On");
    
    GetParam(kSySourceNoise)->InitDouble("SySourceNoise", 0.0,
                                         -100.0, 100.0, 0.1, "%");
 
    GetParam(kSySourceNoiseSolo)->InitEnum("SySourceNoiseSolo", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");

    GetParam(kSySourceNoiseMute)->InitEnum("SySourceNoiseMute", 0, 2,
                                           "", IParam::kFlagMeta, "",
                                           "Off", "On");
    
    GetParam(kSySourceReverse)->InitEnum("SySourceReverse", 0, 2,
                                         "", IParam::kFlagsNone, "",
                                         "Off", "On");

    GetParam(kSySourcePingPong)->InitEnum("SySourcePingPong", 0, 2,
                                          "", IParam::kFlagsNone, "",
                                          "Off", "On");
    
    GetParam(kSySourceFreeze)->InitEnum("SySourceFreeze", 0, 2,
                                        "", IParam::kFlagsNone, "",
                                        "Off", "On");

    GetParam(kSySourceSynthType)->InitEnum("SySourceSynthType", 0, 3, "",
                                           IParam::kFlagsNone, "",
                                           "All partials",
                                           "Even partials",
                                           "Odd partials");
    
    GetParam(kSyLoop)->InitEnum("SyLoop", 0, 2,
                                "", IParam::kFlagsNone, "",
                                "Off", "On");
    
    // Handle 0 is "mix", other are "sources"
    GetParam(kSyPadHandle0X)->InitDouble("SyPadHandle0X", 0.0,
                                         -100.0, 100.0, 0.1, "%",
                                         // Meta because shared with knobs
                                         IParam::kFlagMeta);

    GetParam(kSyPadHandle0Y)->InitDouble("SyPadHandle0Y", 0.0,
                                         -100.0, 100.0, 0.1, "%",
                                         // Meta because shared with knobs
                                         IParam::kFlagMeta);
    
    GetParam(kSyPadHandle1X)->InitDouble("SyPadHandle1X", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle1Y)->InitDouble("SyPadHandle1Y", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle2X)->InitDouble("SyPadHandle2X", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle2Y)->InitDouble("SyPadHandle2Y", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle3X)->InitDouble("SyPadHandle3X", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle3Y)->InitDouble("SyPadHandle3Y", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle4X)->InitDouble("SyPadHandle4X", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle4Y)->InitDouble("SyPadHandle4Y", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle5X)->InitDouble("SyPadHandle5X", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyPadHandle5Y)->InitDouble("SyPadHandle5Y", 0.0,
                                         -100.0, 100.0, 0.1, "%");

    GetParam(kSyTimeStretch)->InitDouble("SyTimeStretch", 0.0,
                                         -MAX_TIME_STRETCH,
                                         MAX_TIME_STRETCH, 0.1, "x");
    
    BL_FLOAT defaultOutGain = 0.0;
    GetParam(kSyOutGain)->InitDouble("SyOutGain", defaultOutGain,
                                     -16.0, 16.0, 0.1, "dB");
}

void
Morpho::OnIdle()
{
    // Make sure that mSpectrogramDisplay is up to date here
    // (because after, we use its params to set the time axis)
    ENTER_PARAMS_MUTEX;

    mMorphoObj->OnIdle();
    
    LEAVE_PARAMS_MUTEX;
}

bool
Morpho::OnKeyDown(const IKeyPress& key)
{
    if (key.VK == 32) // space bar
    {
        if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
        {
            // Toggle play
            bool sourcePlaying = mMorphoObj->GetSoSourcePlaying();
            
            sourcePlaying = !sourcePlaying;
            
            mMorphoObj->SetSoSourcePlaying(sourcePlaying);
            
            GetParam(kSoPlay)->Set(sourcePlaying);
            GUIHelper12::RefreshParameter(this, kSoPlay);
            
            return true;
        }
    }

    return false;
}

void
Morpho::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
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
    // This is the same for Protools, and if the plugin consumes,
    // this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
  
    mSecureRestarter.Process(in);

    bool isTransportPlaying = IsTransportPlaying();
    BL_FLOAT transportSamplePos = GetTransportSamplePos();
    
    // Processing
    mMorphoObj->ProcessBlock(in, out, isTransportPlaying, transportSamplePos);
      
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
  
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    BL_PROFILE_END;
}

void
Morpho::SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1)
{
    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
    {
        GetParam(kSoWaterfallAngle0)->Set(angle0);
        GetParam(kSoWaterfallAngle1)->Set(angle1);

        mMorphoObj->SetSoWaterfallCameraAngle0(angle0);
        mMorphoObj->SetSoWaterfallCameraAngle1(angle1);
    }
    else if (mPlugMode == MORPHO_PLUG_MODE_SYNTH)
    {
        GetParam(kSyWaterfallAngle0)->Set(angle0);
        GetParam(kSyWaterfallAngle1)->Set(angle1);

        mMorphoObj->SetSyWaterfallCameraAngle0(angle0);
        mMorphoObj->SetSyWaterfallCameraAngle1(angle1);
    }
}

void
Morpho::SetCameraFov(BL_FLOAT angle)
{
    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
    {
        GetParam(kSoWaterfallCamFov)->Set(angle);

        mMorphoObj->SetSoWaterfallCameraFov(angle);
    }
    else if (mPlugMode == MORPHO_PLUG_MODE_SYNTH)
    {
        GetParam(kSyWaterfallCamFov)->Set(angle);

        mMorphoObj->SetSyWaterfallCameraFov(angle);
    }
}

void
Morpho::SoSourceChanged(const SoSource *source)
{
    // Update all the parameters

    bool sourcePlaying = mMorphoObj->GetSoSourcePlaying();
    GetParam(kSoPlay)->Set(sourcePlaying);
    
    BL_FLOAT spectroBrightness = mMorphoObj->GetSoSpectroBrightness();
    GetParam(kSoSpectroBrightness)->Set(spectroBrightness);
        
    BL_FLOAT spectroContrast = mMorphoObj->GetSoSpectroContrast();
    GetParam(kSoSpectroContrast)->Set(spectroContrast);
    
    BL_FLOAT spectroSpecWave = mMorphoObj->GetSoSpectroSpecWave();
    GetParam(kSoSpectroSpecWave)->Set(spectroSpecWave);
    
    BL_FLOAT spectroWaveScale = mMorphoObj->GetSoSpectroWaveformScale();
    GetParam(kSoSpectroWaveScale)->Set(spectroWaveScale);
    
    SelectionType selType = mMorphoObj->GetSoSpectroSelectionType();
    GetParam(kSoSpectroSelectionType)->Set(selType);
    
    WaterfallViewMode waterfallMode = mMorphoObj->GetSoWaterfallViewMode();
    GetParam(kSoWaterfallViewMode)->Set(waterfallMode);
        
    bool masterSource = mMorphoObj->GetSoSourceMaster();
    GetParam(kSoSourceMaster)->Set(masterSource);
    
    SoSourceType sourceType = mMorphoObj->GetSoSourceType();
    GetParam(kSoSourceType)->Set(sourceType);
    
    BL_FLOAT timeSmooth = mMorphoObj->GetSoTimeSmoothCoeff();
    GetParam(kSoTimeSmooth)->Set(timeSmooth);
    
    BL_FLOAT detectThreshold = mMorphoObj->GetSoDetectThreshold();
    GetParam(kSoDetectThrs)->Set(detectThreshold);
    
    BL_FLOAT freqThreshold = mMorphoObj->GetSoFreqThreshold();
    GetParam(kSoFreqThrs)->Set(freqThreshold);
    
    BL_FLOAT sourceGain = mMorphoObj->GetSoSourceGain();
    GetParam(kSoSourceGain)->Set(AmpToDB(sourceGain));

    SoCurrentSourceApplyChanged();
    
    // Finally update all the controls
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
Morpho::OnRemoveSource(int sourceNum)
{
    mMorphoObj->RemoveSource(sourceNum);

    CheckSoCurrentSourceControlsDisabled();
}
    
void
Morpho::SySourceChanged(const SySource *source)
{
    // Update all the parameters
    
    bool sourceSolo = mMorphoObj->GetSySourceSolo();
    GetParam(kSySourceSolo)->Set(sourceSolo);

    bool sourceMute = mMorphoObj->GetSySourceMute();
    GetParam(kSySourceMute)->Set(sourceMute);

    bool masterSource = mMorphoObj->GetSySourceMaster();
    GetParam(kSySourceMaster)->Set(masterSource);
    
    BL_FLOAT amp = mMorphoObj->GetSyAmp();
    amp = FactorToParam(amp);
    GetParam(kSySourceAmp)->Set(amp);

    bool ampSolo = mMorphoObj->GetSyAmpSolo();
    GetParam(kSySourceAmpSolo)->Set(ampSolo);

    bool ampMute = mMorphoObj->GetSyAmpMute();
    GetParam(kSySourceAmpMute)->Set(ampMute);

    BL_FLOAT pitch = mMorphoObj->GetSyPitch();
    pitch = PitchFactorToHt(pitch);
    GetParam(kSySourcePitch)->Set(pitch);

    bool pitchSolo = mMorphoObj->GetSyPitchSolo();
    GetParam(kSySourcePitchSolo)->Set(pitchSolo);

    bool pitchMute = mMorphoObj->GetSyPitchMute();
    GetParam(kSySourcePitchMute)->Set(pitchMute);
    
    BL_FLOAT color = mMorphoObj->GetSyColor();
    color = FactorToParam(color);
    GetParam(kSySourceColor)->Set(color);

    bool colorSolo = mMorphoObj->GetSyColorSolo();
    GetParam(kSySourceColorSolo)->Set(colorSolo);

    bool colorMute = mMorphoObj->GetSyColorMute();
    GetParam(kSySourceColorMute)->Set(colorMute);

    BL_FLOAT warping = mMorphoObj->GetSyWarping();
    warping = FactorToParam(warping);
    GetParam(kSySourceWarping)->Set(warping);

    bool warpingSolo = mMorphoObj->GetSyWarpingSolo();
    GetParam(kSySourceWarpingSolo)->Set(warpingSolo);

    bool warpingMute = mMorphoObj->GetSyWarpingMute();
    GetParam(kSySourceWarpingMute)->Set(warpingMute);

    BL_FLOAT noise = mMorphoObj->GetSyNoise();
    noise *= 100.0;
    
    GetParam(kSySourceNoise)->Set(noise);

    bool noiseSolo = mMorphoObj->GetSyNoiseSolo();
    GetParam(kSySourceNoiseSolo)->Set(noiseSolo);

    bool noiseMute = mMorphoObj->GetSyNoiseMute();
    GetParam(kSySourceNoiseMute)->Set(noiseMute);

    bool reverse = mMorphoObj->GetSyReverse();
    GetParam(kSySourceReverse)->Set(reverse);

    bool pingPong = mMorphoObj->GetSyPingPong();
    GetParam(kSySourcePingPong)->Set(pingPong);

    bool freeze = mMorphoObj->GetSyFreeze();
    GetParam(kSySourceFreeze)->Set(freeze);

    SySourceSynthType synthType = mMorphoObj->GetSySynthType();
    GetParam(kSySourceSynthType)->Set(synthType);
        
    // Finally update all the controls
    GUIHelper12::RefreshAllParameters(this, kNumParams);

    // Other things to do...
    //

    // If we the source type is mix source, hide some unused controls
    SyCurrentSourceTypeMixChanged();
    SyCurrentSourceMutedChanged();
    SyCurrentSourceSynthTypeChanged();
    SyCurrentSourceTypeChanged();
}

void
Morpho::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
    {
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

        mMorphoObj->SetGUIHelper(mGUIHelper);
        mMorphoObj->SetSoSpectroGUIParams(kSoSpectroGraphX, kSoSpectroGraphY,
                                          GRAPH_SPECTRO_SO_FN);
        mMorphoObj->SetSoWaterfallGUIParams(kSoWaterfallGraphX, kSoWaterfallGraphY,
                                            GRAPH_WATERFALL_SO_FN);
        mMorphoObj->SetSyWaterfallGUIParams(kSyWaterfallGraphX, kSyWaterfallGraphY,
                                            GRAPH_WATERFALL_SY_FN);
    }
    
    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);

    if (mSourcesPanel == NULL)
        mSourcesPanel = new Panel(pGraphics);
    else
        mSourcesPanel->SetGraphics(pGraphics);
    
    if (mSynthPanel == NULL)
        mSynthPanel = new Panel(pGraphics);
    else
        mSynthPanel->SetGraphics(pGraphics);

    //
    mSyCurrentSourceControls.clear();
    mSyCurrentSourceMixHide.clear();
    mSoCurrentSourceControls.clear();
    mSyCurrentSourceSynthTypeControls.clear();
    mSyCurrentSourceTypeFileControls.clear();
    mSoApplyButtonControl = NULL;
    
    //
    const char *plugModes[] = { BUTTON_SYNTHESIS_FN,
                                BUTTON_SOURCES_FN };
    mGUIHelper->CreateRadioButtonsCustom(pGraphics,
                                         kPlugModeX,
                                         kPlugModeY,
                                         plugModes,
                                         // 2 buttons
                                         kPlugModeNumButtons,
                                         // 3 frames
                                         kPlugModeFrames,
                                         kPlugModeHSize,
                                         kPlugMode,
                                         true,
                                         tooltipPlugMode);
    
    // Create sources panel, or synth panel, depending on the mode
    PlugModeChanged();
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);
    
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
Morpho::CreateSourcesControls(IGraphics *pGraphics)
{
    // Background
    pGraphics->AttachBackground(BACKGROUND_SO_FN); //
    {
        IControl *c = pGraphics->GetBackgroundControl();
        mSourcesPanel->Add(c);
    }
    
    // Controls collection
    mGUIHelper->StartCollectCreatedControls();

    // Controls creation
    //
    
    {
        ITabsBarControl *c =
            mGUIHelper->CreateTabsBar(pGraphics,
                                      kSoTabsBarX, kSoTabsBarY,
                                      kSoTabsBarW, kSoTabsBarH);
        SetTabsBarStyle(c);
        
        if (mMorphoObj != NULL)
            mMorphoObj->SetTabsBar(c);
    }

    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kSoNewLiveSourceX, kSoNewLiveSourceY,
                                     BUTTON_LIVE_SOURCE_FN,
                                     kSoNewLiveSource,
                                     NULL, false, true, false,
                                     tooltipSoNewLiveSource);
    
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kSoNewFileSourceX, kSoNewFileSourceY,
                                     BUTTON_FILE_SOURCE_FN,
                                     kSoNewFileSource,
                                     NULL, false, true, false,
                                     tooltipSoNewFileSource);

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSoPlayX, kSoPlayY,
                                             BUTTON_PLAY_FN,
                                             kSoPlay,
                                             NULL, true, true, false,
                                             tooltipSoPlay);
           mSoCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSoApplyX, kSoApplyY,
                                             BUTTON_APPLY_FN,
                                             kSoApply,
                                             NULL, false, true, false,
                                             tooltipSoApply);
        mSoCurrentSourceControls.push_back(c);
        mSoApplyButtonControl = c;
    }

    {
        // Display the graph background bitmap even when there is no graph,
        // so we have the borders drawn at startup
        IControl *c =
            mGUIHelper->CreateBitmap(pGraphics,
                                     kSoSpectroGraphX, kSoSpectroGraphY,
                                     GRAPH_SPECTRO_SO_FN);

        // NOTE: no need!
        // Re-enable it, otherwise it will be displayed grayed out?
        //c->SetDisabled(false);
    }
    
    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoSpectroBrightnessX,
                                      kSoSpectroBrightnessY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoSpectroBrightness,
                                      TEXTFIELD_FN,
                                      "BRIGHTNESS",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoSpectroBrightness);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoSpectroContrastX,
                                      kSoSpectroContrastY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoSpectroContrast,
                                      TEXTFIELD_FN,
                                      "CONTRAST",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoSpectroContrast);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoSpectroSpecWaveX,
                                      kSoSpectroSpecWaveY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoSpectroSpecWave,
                                      TEXTFIELD_FN,
                                      "SPEC/WAVE",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoSpectroSpecWave);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoSpectroWaveScaleX,
                                      kSoSpectroWaveScaleY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoSpectroWaveScale,
                                      TEXTFIELD_FN,
                                      "WAVE SCALE",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoSpectroWaveScale);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        const char *selectionDirs[] = { BUTTON_SELDIR_BOTH_FN,
                                        BUTTON_SELDIR_HORIZ_FN,
                                        BUTTON_SELDIR_VERT_FN };

        IControl *c = 
            mGUIHelper->CreateRadioButtonsCustom(pGraphics,
                                                 kSoSpectroSelectionTypeX,
                                                 kSoSpectroSelectionTypeY,
                                                 selectionDirs,
                                                 // 3 buttons
                                                 kSoSpectroSelectionTypeNumButtons,
                                                 // 3 frames
                                                 kSoSpectroSelectionTypeFrames,
                                                 kSoSpectroSelectionTypeHSize,
                                                 kSoSpectroSelectionType,
                                                 true,
                                                 tooltipSoSpectroSelectionType);
        mSoCurrentSourceControls.push_back(c);
    }

    {
        // Display the graph background bitmap even when there is no graph,
        // so we have the borders drawn at startup
        IControl *c =
            mGUIHelper->CreateBitmap(pGraphics,
                                     kSoWaterfallGraphX, kSoWaterfallGraphY,
                                     GRAPH_WATERFALL_SO_FN);

        // NOTE: no need!
        // Re-enable it, otherwise it will be displayed grayed out?
        //c->SetDisabled(false);
    }

    {
        IControl *c = 
            mGUIHelper->CreateDropDownMenu(pGraphics,
                                           kSoWaterfallViewModeX,
                                           kSoWaterfallViewModeY,
                                           kSoWaterfallViewModeWidth,
                                           kSoWaterfallViewMode,
                                           "VIEW MODE",
                                           GUIHelper12::SIZE_DEFAULT,
                                           tooltipSoWaterfallViewMode);
        mSoCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSoSourceMasterX,
                                           kSoSourceMasterY,
                                           CHECKBOX_FN,
                                           kSoSourceMaster,
                                           "SOURCE MASTER",
                                           GUIHelper12::SIZE_DEFAULT,
                                           false, //true,
                                           tooltipSoSourceMaster);
        mSoCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateDropDownMenu(pGraphics,
                                           kSoSourceTypeX,
                                           kSoSourceTypeY,
                                           kSoSourceTypeWidth,
                                           kSoSourceType,
                                           "SOURCE TYPE",
                                           GUIHelper12::SIZE_DEFAULT,
                                           tooltipSoSourceType);
        mSoCurrentSourceControls.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoTimeSmoothX, kSoTimeSmoothY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoTimeSmooth,
                                      TEXTFIELD_FN,
                                      "TIME SMOOTH",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoTimeSmooth);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoDetectThrsX, kSoDetectThrsY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoDetectThrs,
                                      TEXTFIELD_FN,
                                      "DETECT THRS",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoDetectThrs);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoFreqThrsX, kSoFreqThrsY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoFreqThrs,
                                      TEXTFIELD_FN,
                                      "FREQ THRS",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoFreqThrs);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSoSourceGainX, kSoSourceGainY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSoSourceGain,
                                      TEXTFIELD_FN,
                                      "SOURCE GAIN",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSoSourceGain);
        mSoCurrentSourceControls.push_back(c);
        mSoCurrentSourceControls.push_back(cap);
    }
    
    // Controls collection
    vector<IControl *> newControls;
    mGUIHelper->EndCollectCreatedControls(&newControls);
    mSourcesPanel->Add(newControls);
}

void
Morpho::CreateSynthesisControls(IGraphics *pGraphics)
{
    // Background
    pGraphics->AttachBackground(BACKGROUND_SY_FN); //
    {
        IControl *c = pGraphics->GetBackgroundControl();
        mSynthPanel->Add(c);
    }
    
    // Controls collection
    mGUIHelper->StartCollectCreatedControls();
    
    {
        // Display the graph background bitmap even when there is no graph,
        // so we have the borders drawn at startup
        IControl *c =
            mGUIHelper->CreateBitmap(pGraphics,
                                     kSyWaterfallGraphX, kSyWaterfallGraphY,
                                     GRAPH_WATERFALL_SY_FN);

        // NOTE: no need!
        // Re-enable it, otherwise it will be displayed grayed out?
        //c->SetDisabled(false);
    }
    
    /*mGUIHelper->CreateBitmap(pGraphics,
      kSySourceLabelX, kSySourceLabelY,
      LABEL_SOURCE_NAME_FN);*/
    {
        IIconLabelControl *c =
            mGUIHelper->CreateIconLabel(pGraphics,
                                        kSySourceLabelX, kSySourceLabelY,
                                        kSySourceLabelIconOffsetX,
                                        kSySourceLabelIconOffsetY,
                                        kSySourceLabelTextOffsetX,
                                        kSySourceLabelTextOffsetY,
                                        LABEL_SOURCE_NAME_FN,
                                        LABEL_SOURCE_ICONS_FN,
                                        kSySourceLabelIconFrames);
            
            if (mMorphoObj != NULL)
                mMorphoObj->SetIconLabel(c);
    }
    
    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceSoloX,
                                             kSySourceSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourceSolo, "",
                                             true, true, false,
                                             tooltipSySourceSolo);
        mSyCurrentSourceMixHide.push_back(c);
    }
    
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kSySourceMuteX,
                                     kSySourceMuteY,
                                     BUTTON_MUTE_FN,
                                     kSySourceMute, "",
                                     true, true, false,
                                     tooltipSySourceMute);

    {
        IControl *c = 
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSySourceMasterX,
                                           kSySourceMasterY,
                                           CHECKBOX_FN,
                                           kSySourceMaster,
                                           "SOURCE MASTER",
                                           GUIHelper12::SIZE_DEFAULT,
                                           false, // true,
                                           tooltipSySourceMaster);
        mSyCurrentSourceMixHide.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c =
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSySourceAmpX, kSySourceAmpY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSySourceAmp,
                                      TEXTFIELD_FN,
                                      "SOURCE AMP",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSySourceAmp);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceControls.push_back(cap);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceAmpSoloX,
                                             kSySourceAmpSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourceAmpSolo, "",
                                             true, true, false,
                                             tooltipSySourceAmpSolo);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceAmpMuteX,
                                             kSySourceAmpMuteY,
                                             BUTTON_MUTE_FN,
                                             kSySourceAmpMute, "",
                                             true, true, false,
                                             tooltipSySourceAmpMute);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSySourcePitchX, kSySourcePitchY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSySourcePitch,
                                      TEXTFIELD_FN,
                                      "SOURCE PITCH",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSySourcePitch);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceControls.push_back(cap);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourcePitchSoloX,
                                             kSySourcePitchSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourcePitchSolo, "",
                                             true, true, false,
                                             tooltipSySourcePitchSolo);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourcePitchMuteX,
                                             kSySourcePitchMuteY,
                                             BUTTON_MUTE_FN,
                                             kSySourcePitchMute, "",
                                             true, true, false,
                                             tooltipSySourcePitchMute);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSySourceColorX, kSySourceColorY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSySourceColor,
                                      TEXTFIELD_FN,
                                      "SOURCE COLOR",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSySourceColor);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceControls.push_back(cap);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceColorSoloX,
                                             kSySourceColorSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourceColorSolo, "",
                                             true, true, false,
                                             tooltipSySourceColorSolo);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceColorMuteX,
                                             kSySourceColorMuteY,
                                             BUTTON_MUTE_FN,
                                             kSySourceColorMute, "",
                                             true, true, false,
                                             tooltipSySourceColorMute);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSySourceWarpingX, kSySourceWarpingY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSySourceWarping,
                                      TEXTFIELD_FN,
                                      "SOURCE WARPING",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSySourceWarping);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceControls.push_back(cap);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceWarpingSoloX,
                                             kSySourceWarpingSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourceWarpingSolo, "",
                                             true, true, false,
                                             tooltipSySourceWarpingSolo);
        mSyCurrentSourceControls.push_back(c);
    }
    
    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceWarpingMuteX,
                                             kSySourceWarpingMuteY,
                                             BUTTON_MUTE_FN,
                                             kSySourceWarpingMute, "",
                                             true, true, false,
                                             tooltipSySourceWarpingMute);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        ICaptionControl *cap;
        IControl *c = 
            mGUIHelper->CreateKnobSVG(pGraphics,
                                      kSySourceNoiseX, kSySourceNoiseY,
                                      kKnobSmallWidth, kKnobSmallHeight,
                                      KNOB_SMALL_FN,
                                      kSySourceNoise,
                                      TEXTFIELD_FN,
                                      "SOURCE NOISE",
                                      GUIHelper12::SIZE_DEFAULT,
                                      &cap, true,
                                      tooltipSySourceNoise);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceControls.push_back(cap);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceNoiseSoloX,
                                             kSySourceNoiseSoloY,
                                             BUTTON_SOLO_FN,
                                             kSySourceNoiseSolo, "",
                                             true, true, false,
                                             tooltipSySourceNoiseSolo);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateRolloverButton(pGraphics,
                                             kSySourceNoiseMuteX,
                                             kSySourceNoiseMuteY,
                                             BUTTON_MUTE_FN,
                                             kSySourceNoiseMute, "",
                                             true, true, false,
                                             tooltipSySourceNoiseMute);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSySourceReverseX,
                                           kSySourceReverseY,
                                           CHECKBOX_FN,
                                           kSySourceReverse,
                                           "SOURCE REVERSE",
                                           GUIHelper12::SIZE_DEFAULT, true,
                                           tooltipSySourceReverse);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceTypeFileControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSySourcePingPongX,
                                           kSySourcePingPongY,
                                           CHECKBOX_FN,
                                           kSySourcePingPong,
                                           "SOURCE PING-PONG",
                                           GUIHelper12::SIZE_DEFAULT, true,
                                           tooltipSySourcePingPong);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceTypeFileControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSySourceFreezeX,
                                           kSySourceFreezeY,
                                           CHECKBOX_FN,
                                           kSySourceFreeze,
                                           "SOURCE FREEZE",
                                           GUIHelper12::SIZE_DEFAULT, true,
                                           tooltipSySourceFreeze);
        mSyCurrentSourceControls.push_back(c);
    }

    {
        IControl *c = 
            mGUIHelper->CreateDropDownMenu(pGraphics,
                                           kSySourceSynthTypeX,
                                           kSySourceSynthTypeY,
                                           kSySourceSynthTypeWidth,
                                           kSySourceSynthType,
                                           "SOURE SYNTH TYPE",
                                           GUIHelper12::SIZE_DEFAULT,
                                           tooltipSySourceSynthType);
        mSyCurrentSourceControls.push_back(c);
        mSyCurrentSourceMixHide.push_back(c);
        mSyCurrentSourceSynthTypeControls.push_back(c);
    }

    {
        IControl *c =
            mGUIHelper->CreateToggleButton(pGraphics,
                                           kSyLoopX,
                                           kSyLoopY,
                                           CHECKBOX_FN,
                                           kSyLoop,
                                           "LOOP",
                                           GUIHelper12::SIZE_DEFAULT, true,
                                           tooltipSyLoop);
    }

    {
        IXYPadControlExt *c = 
            mGUIHelper->CreateXYPadExt(this, pGraphics,
                                       kSyPadTrackX, kSyPadTrackY,
                                       PAD_TRACK_SY_FN,
                                       kSyPadHandle0X, kSyPadHandle0Y,
                                       kSyPadTrackBorderSize,
                                       false,
                                       tooltipSyPadTrack);

        c->AddHandle(pGraphics, PAD_HANDLE0_FN, { kSyPadHandle0X, kSyPadHandle0Y });
        c->AddHandle(pGraphics, PAD_HANDLE1_FN, { kSyPadHandle1X, kSyPadHandle1Y });
        c->AddHandle(pGraphics, PAD_HANDLE2_FN, { kSyPadHandle2X, kSyPadHandle2Y });
        c->AddHandle(pGraphics, PAD_HANDLE3_FN, { kSyPadHandle3X, kSyPadHandle3Y });
        c->AddHandle(pGraphics, PAD_HANDLE4_FN, { kSyPadHandle4X, kSyPadHandle4Y });
        c->AddHandle(pGraphics, PAD_HANDLE5_FN, { kSyPadHandle5X, kSyPadHandle5Y });

        if (mMorphoObj != NULL)
            mMorphoObj->SetXYPad(c);
    }
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSyKnobHandle0X_X, kSyKnobHandle0X_Y,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              //kSyKnobHandle0X,
                              kSyPadHandle0X,
                              TEXTFIELD_FN,
                              "MIX X POS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSyPadTrackX);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSyKnobHandle0Y_X, kSyKnobHandle0Y_Y,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              //kSyKnobHandle0Y,
                              kSyPadHandle0Y,
                              TEXTFIELD_FN,
                              "MIX X POS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSyPadTrackY);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSyTimeStretchX, kSyTimeStretchY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSyTimeStretch,
                              TEXTFIELD_FN,
                              "TIME STRETCH",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSyTimeStretch);
    
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSyOutGainX, kSyOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSyOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSyOutGain);
    
    // Control collection
    vector<IControl *> newControls;
    mGUIHelper->EndCollectCreatedControls(&newControls);
    mSynthPanel->Add(newControls);
}

void
Morpho::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, false);
}

void
Morpho::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    if (mMorphoObj != NULL)
        mMorphoObj->Reset(sampleRate);
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

#define CHECK_XYPAD_HANDLE_PARAMS(__HANDLE_NUM__)       \
    case kSyPadHandle##__HANDLE_NUM__##X:               \
    {                                                   \
    BL_FLOAT value = GetParam(paramIdx)->Value();       \
    value = value*0.01;                                 \
    mMorphoObj->SetSyPadHandleX(__HANDLE_NUM__, value); \
    }                                                   \
    break;                                              \
    case kSyPadHandle##__HANDLE_NUM__##Y:               \
    {                                                   \
    BL_FLOAT value = GetParam(paramIdx)->Value();       \
    value = value*0.01;                                 \
    mMorphoObj->SetSyPadHandleY(__HANDLE_NUM__, value); \
    }                                                   \
    break;

void
Morpho::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
        
    ENTER_PARAMS_MUTEX;
  
    switch (paramIdx)
    {
        case kPlugMode:
        {
            MorphoPlugMode newPlugMode = (MorphoPlugMode)GetParam(paramIdx)->Int();

            // Take care of rollover!
            if (newPlugMode != mPlugMode)
            {
                mPlugMode = newPlugMode;

                mMorphoObj->SetMode(mPlugMode);
                
                PlugModeChanged();
            }
        }
        break;

        case kSoNewLiveSource:
        {
            int value = GetParam(paramIdx)->Int();
            if (value == 1)
            {
                mMorphoObj->CreateNewLiveSource();

                CheckSoCurrentSourceControlsDisabled();
                RefreshWaterfallAngles();
            }
        }
        break;

        case kSoNewFileSource:
        {
            int value = GetParam(paramIdx)->Int();
            if (value == 1)
            {
                mMorphoObj->TryCreateNewFileSource();

                CheckSoCurrentSourceControlsDisabled();
                RefreshWaterfallAngles();
            }
        }
        break;
        
        // Per SoSource parameters
        //
        case kSoApply:
        {
            int value = GetParam(paramIdx)->Int();
            if (value == 1)
            {
                mMorphoObj->SoComputeFileFrames();

                SoCurrentSourceApplyChanged();
            }
        }
        
        case kSoSpectroBrightness:
        {
            BL_FLOAT brightness = GetParam(paramIdx)->Value();
            mMorphoObj->SetSoSpectroBrightness(brightness);
        }
        break;

        case kSoSpectroContrast:
        {
            BL_FLOAT contrast = GetParam(paramIdx)->Value();
            mMorphoObj->SetSoSpectroContrast(contrast);
        }
        break;

        case kSoSpectroSpecWave:
        {
            BL_FLOAT specWave = GetParam(paramIdx)->Value();
            specWave = specWave;
            
            mMorphoObj->SetSoSpectroSpecWave(specWave);
        }
        break;
        
        case kSoSpectroWaveScale:
        {
            BL_FLOAT waveScale = GetParam(paramIdx)->Value();
            
            mMorphoObj->SetSoSpectroWaveformScale(waveScale);
        }
        break;

        case kSoSpectroSelectionType:
        {
            SelectionType type = (SelectionType)GetParam(paramIdx)->Int();
            
            mMorphoObj->SetSoSpectroSelectionType(type);
        }
        break;
        
        case kSoWaterfallViewMode:
        {
            WaterfallViewMode mode = (WaterfallViewMode)GetParam(paramIdx)->Int();
            
            mMorphoObj->SetSoWaterfallViewMode(mode);
        }
        break;

        case kSoSourceMaster:
        {
            int value = GetParam(paramIdx)->Int();
            if (value == 1)
                mMorphoObj->SetCurrentSourceMaster();
        }
        break;

        case kSoSourceType:
        {
            SoSourceType sourceType = (SoSourceType)GetParam(paramIdx)->Int();
            
            mMorphoObj->SetSoSourceType(sourceType);
        }
        break;

        case kSoTimeSmooth:
        {
            BL_FLOAT timeSmooth = GetParam(paramIdx)->Value();
            
            mMorphoObj->SetSoTimeSmoothCoeff(timeSmooth);
        }
        break;

        case kSoDetectThrs:
        {
            BL_FLOAT detectThrs = GetParam(paramIdx)->Value();
            
            mMorphoObj->SetSoDetectThreshold(detectThrs);
        }
        break;

        case kSoFreqThrs:
        {
            BL_FLOAT freqThrs = GetParam(paramIdx)->Value();
            
            mMorphoObj->SetSoFreqThreshold(freqThrs);
        }
        break;
        
        case kSoSourceGain:
        {
            BL_FLOAT sourceGain = GetParam(paramIdx)->DBToAmp();
            mMorphoObj->SetSoSourceGain(sourceGain);
        }
        break;

        //
        case kSoPlay:
        {
            bool value = GetParam(paramIdx)->Int();

            mMorphoObj->SetSoSourcePlaying(value);
        }
        break;

        case kSoWaterfallAngle0:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSoWaterfallCameraAngle0(value);
        }

        case kSoWaterfallAngle1:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSoWaterfallCameraAngle1(value);
        }

        case kSoWaterfallCamFov:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSoWaterfallCameraFov(value);
        }

        // Per SySource parameters
        //
        
        case kSySourceSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSySourceSolo(value);
        }
        break;

        // Synthesis, mute current source
        case kSySourceMute:
        {
            bool value = GetParam(paramIdx)->Int();

            mMorphoObj->SetSySourceMute(value);

            // Also manage other flags
            SyCurrentSourceTypeMixChanged();

            // Mute
            SyCurrentSourceMutedChanged();

            // Also manage other flags
            SyCurrentSourceSynthTypeChanged();
            SyCurrentSourceTypeChanged();
        }
        break;
        
        case kSySourceMaster:
        {
            int value = GetParam(paramIdx)->Int();
            if (value == 1)
                mMorphoObj->SetCurrentSourceMaster();
        }
        break;

        case kSySourceAmp:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            BL_FLOAT amp = ParamToFactor(value);
            
            mMorphoObj->SetSyAmp(amp);
            mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
        }
        break;
        
        case kSySourceAmpSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyAmpSolo(value);
            if (value)
                mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
        }
        break;
        
        case kSySourceAmpMute:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyAmpMute(value);
        }
        break;

        case kSySourcePitch:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            
            BL_FLOAT pitch = PitchHtToFactor(value);
            
            mMorphoObj->SetSyPitch(pitch);
            mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
        }
        break;
        
        case kSySourcePitchSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyPitchSolo(value);
            if (value)
                mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
        }
        break;
        
        case kSySourcePitchMute:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyPitchMute(value);
        }
        break;

        case kSySourceColor:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            BL_FLOAT color = ParamToFactor(value);
            
            mMorphoObj->SetSyColor(color);
            mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_COLOR);
        }
        break;
        
        case kSySourceColorSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyColorSolo(value);
            if (value)
                mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_COLOR);
        }
        break;
        
        case kSySourceColorMute:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyColorMute(value);
        }
        break;

        case kSySourceWarping:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            BL_FLOAT warping = ParamToFactor(value);
            
            mMorphoObj->SetSyWarping(warping);
            mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_WARPING);
        }
        break;
        
        case kSySourceWarpingSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyWarpingSolo(value);
            if (value)
                mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_WARPING);
        }
        break;
        
        case kSySourceWarpingMute:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyWarpingMute(value);
        }
        break;

        case kSySourceNoise:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();

            BL_FLOAT noise = value*0.01;
            
            mMorphoObj->SetSyNoise(noise);
            
            mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_NOISE);
        }
        break;
        
        case kSySourceNoiseSolo:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyNoiseSolo(value);
            if (value)
                mMorphoObj->SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_NOISE);
        }
        break;
        
        case kSySourceNoiseMute:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyNoiseMute(value);
        }
        break;

        case kSySourceReverse:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyReverse(value);
        }
        break;
        
        case kSySourcePingPong:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyPingPong(value);
        }
        break;
        
        case kSySourceFreeze:
        {
            bool value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyFreeze(value);
        }
        break;

        case kSySourceSynthType:
        {
            SySourceSynthType value = (SySourceSynthType)GetParam(paramIdx)->Int();
            mMorphoObj->SetSySynthType(value);
        }
        break;

        case kSyWaterfallAngle0:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSyWaterfallCameraAngle0(value);
        }

        case kSyWaterfallAngle1:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSyWaterfallCameraAngle1(value);
        }

        case kSyWaterfallCamFov:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSyWaterfallCameraFov(value);
        }

        case kSyLoop:
        {
            BL_FLOAT value = GetParam(paramIdx)->Int();
            mMorphoObj->SetSyLoop(value);
        }
        break;
        
        case kSyTimeStretch:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            mMorphoObj->SetSyTimeStretchFactor(value);
        }
        break;

        case kSyOutGain:
        {
            BL_FLOAT value = GetParam(paramIdx)->DBToAmp();
            mMorphoObj->SetSyOutGain(value);
        }
        break;

        // XYPad handles
        //
        CHECK_XYPAD_HANDLE_PARAMS(0);
        CHECK_XYPAD_HANDLE_PARAMS(1);
        CHECK_XYPAD_HANDLE_PARAMS(2);
        CHECK_XYPAD_HANDLE_PARAMS(3);
        CHECK_XYPAD_HANDLE_PARAMS(4);
        CHECK_XYPAD_HANDLE_PARAMS(5);
            
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Morpho::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mMorphoObj->OnUIOpen();
    
    LEAVE_PARAMS_MUTEX;
}

void
Morpho::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mMorphoObj->OnUIClose();
    
    LEAVE_PARAMS_MUTEX;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;

    // NOTE: checked that OnUIClose() is well called juste before
    // destroying the plugin: ok!
    // So the panels resources are well freed
    
    if (mSourcesPanel != NULL)
    {
        mSourcesPanel->Clear();
        mSourcesPanel->SetGraphics(NULL);
    }
    if (mSynthPanel != NULL)
    {
        mSynthPanel->Clear();
        mSynthPanel->SetGraphics(NULL);
    }

    //
    mSyCurrentSourceControls.clear();
    mSyCurrentSourceMixHide.clear();
    mSoCurrentSourceControls.clear();
    mSyCurrentSourceSynthTypeControls.clear();
    mSyCurrentSourceTypeFileControls.clear();
    mSoApplyButtonControl = NULL;
    
    if (mMorphoObj != NULL)
        mMorphoObj->SetTabsBar(NULL);
    if (mMorphoObj != NULL)
        mMorphoObj->SetXYPad(NULL);
    if (mMorphoObj != NULL)
        mMorphoObj->SetIconLabel(NULL);

    mSoApplyButtonControl = NULL;
}

// At startup OnParamChange() is called after mPredictProcessor is initialized.
// mPredictProcessor is allocated after IGraphics is created
// (because it need IGraphics for resources on Windows)
// It is allocated after OnParamChange() calls at startup.
void
Morpho::ApplyParams()
{
    // TODO
}

void
Morpho::PlugModeChanged()
{
    IGraphics *graphics = GetUI();
    if (graphics == NULL)
        return;

    mSyCurrentSourceControls.clear();
    mSyCurrentSourceMixHide.clear();
    mSoCurrentSourceControls.clear();
    mSyCurrentSourceSynthTypeControls.clear();
    mSyCurrentSourceTypeFileControls.clear();
    mSoApplyButtonControl = NULL;
    
    mMorphoObj->ClearGUI();
    
    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
    {
        mSynthPanel->Clear();

        if (mMorphoObj != NULL)
            mMorphoObj->SetXYPad(NULL);
        if (mMorphoObj != NULL)
            mMorphoObj->SetIconLabel(NULL);
        
        CreateSourcesControls(graphics);
        
        CheckSoCurrentSourceControlsDisabled();
        SoCurrentSourceApplyChanged();
    }
    else if (mPlugMode == MORPHO_PLUG_MODE_SYNTH)
    {
        mSourcesPanel->Clear();

        if (mMorphoObj != NULL)
            mMorphoObj->SetTabsBar(NULL);
        
        CreateSynthesisControls(graphics);

        // Also manage other flags
        SyCurrentSourceTypeMixChanged();

        // Mute
        SyCurrentSourceMutedChanged();
        
            // Also manage other flags
        SyCurrentSourceSynthTypeChanged();
        SyCurrentSourceTypeChanged();
    }
    
    // Refresh all parameters so the controls get updated 
    GUIHelper12::RefreshAllParameters(this, kNumParams);

    mMorphoObj->RefreshGUI();
}

void
Morpho::SyCurrentSourceMutedChanged()
{
    bool disabled = mMorphoObj->GetSySourceMute();
    
    for (int i = 0; i < mSyCurrentSourceControls.size(); i++)
    {
        IControl *control = mSyCurrentSourceControls[i];

        // Avoid re-enabling a control that was just disabled by
        // SyCurrentSourceTypeMixChanged()
        if (!disabled)
        {
            bool mixDisabled = false;
            for (int j = 0; j < mSyCurrentSourceMixHide.size(); j++)
            {
                IControl *control0 = mSyCurrentSourceMixHide[j];
                if ((control0 == control) && (control0->IsDisabled()))
                {
                    mixDisabled = true;
                    break;
                }
            }
            if (mixDisabled)
                continue;
        }
            
        control->SetDisabled(disabled);
    }
}

void
Morpho::SyCurrentSourceTypeMixChanged()
{
    bool disabled = (mMorphoObj->GetSySourceType() == SySource::MIX);;

    for (int i = 0; i < mSyCurrentSourceMixHide.size(); i++)
    {
        IControl *control = mSyCurrentSourceMixHide[i];
        control->SetDisabled(disabled);
    }
}

void
Morpho::SyCurrentSourceSynthTypeChanged()
{
    bool disabled =  (mMorphoObj->GetSySourceSynthType() !=
                      SoSourceType::MORPHO_SOURCE_TYPE_MONOPHONIC);
    
    for (int i = 0; i < mSyCurrentSourceSynthTypeControls.size(); i++)
    {
        IControl *control = mSyCurrentSourceSynthTypeControls[i];
        control->SetDisabled(disabled);
    }
}

void
Morpho::SyCurrentSourceTypeChanged()
{
    bool disabled = (mMorphoObj->GetSySourceType() != SySource::FILE);
    
    for (int i = 0; i < mSyCurrentSourceTypeFileControls.size(); i++)
    {
        IControl *control = mSyCurrentSourceTypeFileControls[i];
        control->SetDisabled(disabled);
    }
}

void
Morpho::CheckSoCurrentSourceControlsDisabled()
{
    int numSources = mMorphoObj->GetNumSources();
    
    bool disabled = (numSources == 0);
        
    for (int i = 0; i < mSoCurrentSourceControls.size(); i++)
    {
        IControl *control = mSoCurrentSourceControls[i];
        control->SetDisabled(disabled);
    }

    // And check the apply button separately!
    if (!disabled)
        SoCurrentSourceApplyChanged();
}

void
Morpho::SoCurrentSourceApplyChanged()
{
    bool disabled = !mMorphoObj->GetSoApplyEnabled();
    if (mSoApplyButtonControl != NULL)
        mSoApplyButtonControl->SetDisabled(disabled);
}

void
Morpho::RefreshWaterfallAngles()
{
    BLUtilsPlug::TouchPlugParam(this, kSoWaterfallAngle0);
    BLUtilsPlug::TouchPlugParam(this, kSoWaterfallAngle1);
    BLUtilsPlug::TouchPlugParam(this, kSoWaterfallCamFov);

    BLUtilsPlug::TouchPlugParam(this, kSyWaterfallAngle0);
    BLUtilsPlug::TouchPlugParam(this, kSyWaterfallAngle1);
    BLUtilsPlug::TouchPlugParam(this, kSyWaterfallCamFov);
}

void
Morpho::SetTabsBarStyle(ITabsBarControl *tabsBar)
{
    if (mGUIHelper == NULL)
        return;

    char font[255];
    mGUIHelper->GetLabelTextFontMorpho(font);

    float fontSize = mGUIHelper->GetLabelTextSizeMorpho();
    
    tabsBar->SetFont(font);
    tabsBar->SetFontSize(fontSize);
}

// Semi-tones and "cents"
// See: http://www.sengpielaudio.com/calculator-centsratio.htm
BL_FLOAT
Morpho::PitchFactorToHt(BL_FLOAT factor)
{
    BL_FLOAT ht = (log(factor)/log(2.0))*(1200.0/100.0);

    return ht;
}

BL_FLOAT
Morpho::PitchHtToFactor(BL_FLOAT ht)
{
    // 1200 cents for 1 octave
    BL_FLOAT cents = ht*100.0;
  
    BL_FLOAT factor = pow(2.0, cents/1200);
  
    return factor;
}

BL_FLOAT
Morpho::FactorToParam(BL_FLOAT factor)
{
    // [ 0, 2 ] => [ -1, 1 ]
    factor = factor - 1.0;
    
    factor *= 100.0;

    return factor;
}

BL_FLOAT
Morpho::ParamToFactor(BL_FLOAT param)
{
    BL_FLOAT factor = param * 0.01;

    // [ -1, 1 ] => [ 0, 2 ]
    factor = factor + 1.0;

    return factor;
}
