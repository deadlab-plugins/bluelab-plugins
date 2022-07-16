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

#include <GUIHelper12.h>

#include <SecureRestarter.h>

#include <GhostTriggerControl.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLUtilsFile.h>

#include <BLDebug.h>

#include <IGUIResizeButtonControl.h>
#include <IRadioButtonsControl.h>

#include <IBLSwitchControl.h>

#include <IRadioButtonsControlCustom.h>

#include <GhostCommandCut.h>
#include <GhostCommandGain.h>
#include <GhostCommandReplace.h>
#include <GhostCommandCopyPaste.h>
#include <GhostCommandHistory.h>

#include <ParamSmoother2.h>

#include <GhostTrack.h>

#include <GhostPluginInterface.h>

#include "config.h"
#include "bl_config.h"

#ifdef APP_API
#include "resources/resource.h"
#endif

#include "Ghost.h"

#include "IPlug_include_in_plug_src.h"

// DEBUG: Compiled plug version as app
// For debugging memory
#define DEBUG_MEMORY 0 //1
#if DEBUG_MEMORY
#define APP_API 0
#endif


// GUI Size
#define PLUG_WIDTH_MEDIUM 1100
#define PLUG_HEIGHT_MEDIUM 650

#define PLUG_WIDTH_BIG 1400
#define PLUG_HEIGHT_BIG 750

#define PLUG_WIDTH_HUGE 2500
#define PLUG_HEIGHT_HUGE 1340

// Since the GUI is very full, reduce the offset after
// the radiobuttons title text
#define RADIOBUTTONS_TITLE_OFFSET -5

// 0: GOOD
// If set to 1, the undo will have problems and the undo will only be partial
// (and now, the replace seems good by default - with 1 time - )
//
//
// Hack to apply two times the replace command
// There was a bug since slice and SpectroEdit2:
// the first replace kept dark zones, and it was necessary
// to push the button two times
//
// NOTE: Making inpaint two times didn't work,
// it was necessary to reconstruct the samples, then
// re-extract them as magns, and re-apply
#define APPLY_TWO_TIMES_REPLACE_HACK 0

// When undo, either display the bar, or the selection of the undo zone
#define UNDO_PREFER_DISPLAY_BAR 0

// ORIGIN: set to 1
#define FIX_AUDIOSUITE 1

// NOTE:
// - works with standalone
// - freezes Reaper if too low res
// - works on Logic
//
// See: https://everymac.com/systems/apple/imac/imac-aluminum-tapered-edge-faq/how-to-run-imac-retina-5k-at-full-resolution.html
#define RETINA_RESIZE_FEATURE 1
#define RETINA_FEATURE_LIMIT_RES 1

// Avoid a crash
#if GHOST_LITE_VERSION
#define RETINA_RESIZE_FEATURE 0
#define RETINA_FEATURE_LIMIT_RES 0
#endif

// Hide the file open, save etc. from the UI
// because they are now in theFile menu
#define HIDE_FILE_BUTTONS 1

#define COMMAND_FADE_NUM_SAMPLES 50 //10 //100 //1000

// Special version, with improvements
// Not to be released
#define DEV_SPECIAL_VERSION 0 //1

#if APP_API
#define USE_TABS_BAR 1
#else
#define USE_TABS_BAR 0
#endif

#define USE_DROP_DOWN_MENU 1

#define USE_BUTTON_ICONS 1


#if 0 
/*
Mike Perry <mkperry37@gmail.com>
"The copy and paste feature in Ghost-X is awesome, but what about a feature that allowed you to possibly export an isolated section (an instrument,ect) from the program, process it s to taste' then re-insert it into the program."
Niko:
"With the application version, the workflow would be: - make a selection, cut, and export it to a new tab. - save this new tab and process it with an external tool, for adding effects. - re-import the file with effects in the tab. - merge the tabs to a single tab"

Niko: "will emplemented L/R stereo separation => 2 spectrograms"
Niko: "Will implemente tabs (in a long time)": possibility to export selection to a new tab, possibility to merge tabs, posisibility to play all tabs at the same time, possibility to duplicate a tab.

TODO: Standalone: remember the place where we opened a file, so next time open this folder instead of the default folder.
Standalone: Save preferences (for example GUI size, and last folder), so next time
we won t have to make thechange again at each launch.

TODO: try to make it work on Audacity for Mike Perry <mkperry37@gmail.com>, and contact him if success.
BUG (crash): pspiralife@gmail.com
Cash/not working: "WIndows 10 / Bitwig Studio 3.2.8"
And: "After closing the DAW and reopening, the plug is now working."
And: "Actually, it crashed again once I put the plugin in record mode. "

BUG: mickeywilsonsfx@gmail.com :" If I have it running on my Stereo Output channel, it will playback instruments but if I try playing with my keyboard on any of my patches, it will not show any audio signal through the chain and I can t hear anything.It s weird because it plays back what I have already recorded, but will not play anything live unless I remove Ghost-X from the chain."

TODO: display phase DERIVATIVES
=> this was done line that in the first Spectogram plugin, and
was very good
TODO: consider soft masking complex => to have a better sound when playing region

BUG: with the standalone version, if the sound input is not defines (In 1 (L) and In 2 (R)) is not defined, the sound doesn t play (and the play bar doesn t advance. Support: Herve-Ghost.
=> TODO: in WDL, make the call to ProcessDoubleReplacing in SA even if input is not defined.

IDEA: inpaint in complex numbers domain, so we will manage the phases well too.

IDEA: choose the frequency range freely (Maybe_mae@yahoo.com requested it)
TODO: before release, re-pass profiler to check potential new optimizations 
=> when done, implement "zoom on selection rectangle"
    - draw a selection rectangle
    - zoom automatically so the rectangle almost fills all the window (choose to zoom in time or freq or both)

IDEA: mickeywilsonsfx@gmail.com
Change the zoom and the speed of the "real time" mode.

IDEA: Ole Boenigk <boenigk@audiomint.de> (support):
- set the frequency range
- zoom adaptive into the freqency range (perhaps with more bad
                                         resolution in time domain)?
(for processing / viewing 5Hz to 20Hz)

PROBLEM: StudioOne Sierra crackles
=> set buffer size to 1024 to fix

PROBLEM (?): can t undo if we leave and return to the edit mode (but this may be normal...)

BUG: Protools only (Mac): choose acquire and launch the playback, then render and bounce
=> latency !
There is no such problem when acquiring + bouncing
This is a problem on Protools: IsPlaying() seems not correct when the transport just starts
=> this leads to some zeros in the input buffers at the beginning
(the number of zeros is totally random).


NOTE: (for support): shift when renderd/bounced
=> it may be due to capture that failed very quickly,
then succeed (e.g. in Protools), without resetting the data
Solution: be sure to reset the spectrogram by switching option
before capture

WARNING: Reaper: capture with SR 44100, then change to SR 88200
and play => the sound is pitched (SR is not applied)

BUG: VST3: crashes Reaper when quitting ?

IMPROV (for Thierry Guyon, support): light crackles when moving the selection rectangle while playing, in standalone mode

BUG(Windows): for ctrl + return, either the two keys must be pressed at the same time,
or return pressed before control

Test on Reaper an Protools. On Reaper, this zone has no fixed size.
WARNING: with 80Hz.wav: spectrogram display bug (color band on the left)
WARNING: plugins, view mode: the spectrogram scrolling is a bit jerky
WARNING: when cutting large zones, the cut button should turn hilighted during the waiting (same for all other edit modes) => this is too hard to solve !
WARNING: plug mode, Logic: can not play in edit mode ! (processDoubleReplacing is not called)
WARNING: tiny shift of 6ms when acquire + render in plug mode (Protools)
WARNING: bug (not reproductible): sound crackle (plug and app)
- set buffer size to 1024 (plug and app)
- restart playback engine
- protools: uncheck "ignore error"
- and most of all: updated the sound driver !
=> crackles when launched under Xcode debugger with malloc tools

IDEA: re-do feature (then moving the cut zone too)
IDEA: when we copy to do a paste, if sound is playing, play the new position of the
buffer to paste in real time (to adjust by ear) ! => no, too slow
IDEA: (todo future) re-generate the background image (in part), after each edition
For the moment, the edition re-appears after waiting image is refined

NOTE: Modification in IPlugAAX for meta parameters (RolloverButtons)
*/
#endif

static char *tooltipHelp = "Help - Display help";
static char *tooltipRange = "Brightness - Colormap brightness";
static char *tooltipContrast = "Contrast - Colormap contrast";
static char *tooltipColormap = "Colormap";
static char *tooltipOutGain = "Out Gain - Output gain";
static char *tooltipSpectWaveformRatio = "Spectrogram/Waveform";
static char *tooltipWaveformScale = "Waveform Scale";
static char *tooltipSelectionType =
    "Selection Direction: Rectangular, horizontal or vertical";
static char *tooltipInpaintDir =
    "Inpaint Direction: Both, horizontal or vertical";
static char *tooltipEditCopy = "Copy - Copy selected region";
static char *tooltipEditPaste = "Paste - Paste copied region";
static char *tooltipEditCut = "Cut - Cut selected region";
static char *tooltipEditGain = "Gain - Apply gain to selected region";
static char *tooltipEditReplace = "Inpaint - Inpaint over selected region";
static char *tooltipEditUndo = "Undo - Undo last action";
static char *tooltipEditGainValue = "Gain Value - Set the gain value to apply";
static char *tooltipEditPlayStop = "Play/Stop - Play selected region";
static char *tooltipLowFreqZoom = "Zoom - Zoom on low frequencies";

static char *tooltipGUISizeSmall = "GUI Size: Small";
static char *tooltipGUISizeMedium = "GUI Size: Medium";
static char *tooltipGUISizeBig = "GUI Size: Big";
static char *tooltipGUISizeHuge = "GUI Size: Huge";

static char *tooltipMonitor = "Monitor - Toggle monitor on/off";
static char *tooltipPlugMode = "Mode: View, capture, edit or render";

enum EParams
{
    kPlugMode = 0,
    //kQuality = 0,
    kRange,
    kContrast,

    kColorMap,
    kSpectWaveformRatio,
    kWaveformScale,
    
    //kDisplayMagns,
#ifdef APP_API
    kFileOpenParam,
    kFileSaveParam,
    kFileSaveAsParam,
    kFileExportSelectionParam,
#endif

    //kTriggerParam,
  
    // Edition
    kEditCopy,
    kEditPaste,
    kEditCut,
    kEditGain,
    kEditReplace,
    kEditUndo,

#ifdef APP_API
#if DEV_SPECIAL_VERSION
    // Link tracks transformations
    kLinkTracks,
#endif
#endif
    
    kPlayStop,

    kSelectionType,
    kInpaintDir,
    kEditGainValue,
    
    kLowFreqZoom,
    
    kMonitor,
    kOutGain,

    kSpectroMeterTimeMode,
    kSpectroMeterFreqMode,
    
    // From here, the parameters won't be reset by "Reset Preferences" menu option
    kGUISizeSmall,
    kGUISizeMedium,
    kGUISizeBig,
    kGUISizeHuge,
    
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

#if USE_TABS_BAR
    kGraphY = 20,
#else
    kGraphY = 0,
#endif
    
    kRangeX = 659,
    kRangeY = 157,
  
    kContrastX = 759,
    kContrastY = 157,
    
    kFileOpenX = 714,
    kFileOpenY = 8,
    
    kFileSaveX = 714,
    kFileSaveY = 34,
    
    kFileSaveAsX = 714,
    kFileSaveAsY = 60,
    
    kOutGainX = 758,
    kOutGainY = 416,

#if !USE_DROP_DOWN_MENU
    kRadioButtonsColorMapX = 606,
    kRadioButtonsColorMapY = 140,
    kRadioButtonColorMapVSize = 160,
    kRadioButtonColorMapNumButtons = 10,
#else
    kColorMapX = 534,
    kColorMapY = 169,
    kColorMapWidth = 80,
#endif
    
    kSpectWaveformRatioX = 659,
    kSpectWaveformRatioY = 253,
    
    kWaveformScaleX = 759,
    kWaveformScaleY = 253,
    
    kRadioButtonsSelectionTypeX = 528,
    kRadioButtonsSelectionTypeY = 308,
    kRadioButtonsSelectionTypeVSize = 70,
    kRadioButtonsSelectionTypeNumButtons = 3,
    kRadioButtonsSelectionTypeFrames = 3,
    
    // Edition
    kEditUndoX = 526,
    kEditUndoY = 362,

    kEditCutX = 526,
    kEditCutY = 388,
    
    kEditCopyX = 526,
    kEditCopyY = 412,
    
    kEditPasteX = 526,
    kEditPasteY = 436,
    
    kEditGainX = 526,
    kEditGainY = 460,
    
    kEditReplaceX = 526,
    kEditReplaceY = 484,
    
    //
    kEditGainValueX = 659,
    kEditGainValueY = 368,
    
    kRadioButtonsInpaintDirX = 642,
    kRadioButtonsInpaintDirY = 484,
    kRadioButtonsInpaintDirVSize = 70,
    kRadioButtonsInpaintDirNumButtons = 3,
    
    // GUI size
    kGUISizeSmallX = 527,
    kGUISizeSmallY = 6,

    kGUISizeMediumX = 550,
    kGUISizeMediumY = 6,
    
    kGUISizeBigX = 573,
    kGUISizeBigY = 6,
    
#if RETINA_RESIZE_FEATURE
    kGUISizeHugeX = 597,
    kGUISizeHugeY = 6,
#endif
    
    //
    kPlayStopX = 526,
    kPlayStopY = 516,

    // For plugin only
    kRadioButtonsPlugModeX = 740,
    kRadioButtonsPlugModeY = 44,
    kRadioButtonsPlugModeVSize = 90,
    kRadioButtonsPlugModeNumButtons = 4,
    
    // Export selection
    kFileExportSelectionX = 714,
    kFileExportSelectionY = 86,
    
    kCheckboxMonitorX = 747,
    kCheckboxMonitorY = 373,

    // For plugin only
    kSpectroMeterX = 527,
    kSpectroMeterY = 57, //55, //49,
    kSpectroMeterTextWidth = 91,
    
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    kLinkTracksX = 780,
    kLinkTracksY = 8,
#endif
#endif

    kCheckboxLowFreqZoomX = 527,
    kCheckboxLowFreqZoomY = 224,

#if USE_TABS_BAR
    kTabsBarX = 0,
    kTabsBarY = 0,
    kTabsBarW = 516,
    kTabsBarH = 18
#endif
};

//
Ghost::Ghost(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , ResizeGUIPluginInterface(this)
{
    TRACE;

    InitNull();
    InitParams();

#ifdef APP_API
    LoadAppPreferences();
#endif
        
    Init();
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif

    BL_PROFILE_RESET;
}

Ghost::~Ghost()
{
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;
  
    // Detach the key catcher
    // as it was also added to the controls list
    // Avoids trying to delete it two times 
    //GetGUI()->AttachKeyCatcher(NULL);

    if (mSpectroMeter != NULL)
        delete mSpectroMeter;

    for (int i = 0; i < mTracks.size(); i++)
    {
        if (mTracks[i] != NULL)
            delete mTracks[i];
    }
        
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
Ghost::MyMakeGraphics()
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

    mGraphics = graphics;
    
#if 0 // For debugging
    graphics->ShowAreaDrawn(true);
#endif
    
    return graphics;
}

void
Ghost::MyMakeLayout(IGraphics *pGraphics)
{
    ENTER_PARAMS_MUTEX;

    UpdateWindowTitle();
        
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
    
#if APP_API
    if (mGUISizeIdx == 0)
        pGraphics->AttachBackground(BACKGROUND_FN);
    else if (mGUISizeIdx == 1)
        pGraphics->AttachBackground(BACKGROUND_MED_FN);
    else if (mGUISizeIdx == 2)
        pGraphics->AttachBackground(BACKGROUND_BIG_FN);
    else if (mGUISizeIdx == 3)
        pGraphics->AttachBackground(BACKGROUND_HUGE_FN);
#else // Plug
    if (mGUISizeIdx == 0)
        pGraphics->AttachBackground(BACKGROUND_PLUG_FN);
    else if (mGUISizeIdx == 1)
        pGraphics->AttachBackground(BACKGROUND_PLUG_MED_FN);
    else if (mGUISizeIdx == 2)
        pGraphics->AttachBackground(BACKGROUND_PLUG_BIG_FN);
    else if (mGUISizeIdx == 3)
        pGraphics->AttachBackground(BACKGROUND_PLUG_HUGE_FN);
#endif
   
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif

#if 0 //1
    pGraphics->ShowFPSDisplay(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);
    
    pGraphics->EnableTooltips(true);
    pGraphics->SetTooltipsDelay(TOOLTIP_DELAY);
    
    CreateControls(pGraphics, mGUIOffsetX);
    
    PostResizeGUI();
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();

    mOutGainSmoother = NULL;
    
    mPrevSampleRate = GetSampleRate();

    SetAllControlsToNull();
        
    // From Waves
    //mGUISizeSmallButton = NULL;
    //mGUISizeMediumButton = NULL;
    //mGUISizeBigButton = NULL;
    
#if RETINA_RESIZE_FEATURE
    //mGUISizeHugeButton = NULL;
#endif
    
    mGUIOffsetX = 0;
    mGUIOffsetY = 0;
    
    mMonitorEnabled = false;
    mMonitorControl = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
    
    mCurrentCopyPasteCommands[0] = NULL;
    mCurrentCopyPasteCommands[1] = NULL;

    BL_FLOAT defaultOutGain = 1.0; // 1 is 0dB;
    BL_FLOAT sampleRate = GetSampleRate();
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);
    
    mIsLoadingSaving = false;

    mInpaintProcessHorizontal = true;
    mInpaintProcessVertical = true;
    
    // Edit controls
    //mSelectionTypeControl = NULL;
    //mEditGainControl = NULL;
    
    //mCopyButton = NULL;
    //mPasteButton = NULL;
    //mCutButton = NULL;
    //mGainButton = NULL;
    //mInpaintButton = NULL;
    //mUndoButton = NULL;
    
    //mInpaintDirControl = NULL;
    //mPlayButtonControl = NULL;
    
    mInpaintDir = InpaintDir::INPAINT_BOTH;

    // Set to true, so that the plug mode will be validated
    // (i.e set correct latency)
    mPlugModeChanged = true;

    mCurrentAction = ACTION_NO_ACTION;

    // Scale
    //
    mYScale = Scale::MEL;

    mLowFreqZoom = false;

    mSpectroMeter = NULL;
    mPrevMouseX = -1.0;
    mPrevMouseY = -1.0;
    mIsPlaying = false;
    
    mLinkTracks = false;

    memset(mCurrentLoadPath, '\0', FILENAME_SIZE);
    memset(mCurrentSavePath, '\0', FILENAME_SIZE);

    mAppStartupArgsChecked = false;

    mNumSamplesMonitor = 0;

    //mTabsBar = NULL;

    mTrackNum = 0;

    //mLowfreqZoomCheckbox = NULL;
    
    mGraphics = NULL;

    mNeedGrayOutControls = false;
}

void
Ghost::InitParams()
{
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    GetParam(kRange)->InitDouble("Brightness", defaultRange, -1.0, 1.0, 0.01, "");
    
    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
    
    // ColorMap num
    int  defaultColorMap = 0;

#if DEV_SPECIAL_VERSION
    // Set purple for debugging! :)
    defaultColorMap = 3;
#endif
    
    mColorMapNum = defaultColorMap;

#if !USE_DROP_DOWN_MENU

#if GHOST_LITE_VERSION && !GHOST_LITE_ALL_COLORMAPS
    // Lite version
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 3);
#else
    // Full version
    GetParam(kColorMap)->InitInt("ColorMap", defaultColorMap, 0, 9);
#endif
    
#else

#if GHOST_LITE_VERSION && !GHOST_LITE_ALL_COLORMAPS
    // Lite version
    GetParam(kColorMap)->InitEnum("ColorMap", 0, 5,
                                  "", IParam::kFlagsNone,
                                  "", "Blue", "Gray", "Green", "Purple", "Wasp");
#else
    // Full version
    GetParam(kColorMap)->InitEnum("ColorMap", 0, 10,
                                  "", IParam::kFlagsNone,
                                  "", "Blue", "Gray", "Green", "Purple", "Wasp",
                                  "Sky", "Dawn", "Rainbow", "Sweet", "Fire");
#endif
    
#endif
    
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
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI
    GetParam(kGUISizeBig)->
        InitInt("BigGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
#endif
#if RETINA_RESIZE_FEATURE
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI
    GetParam(kGUISizeHuge)->
        InitInt("HugeGUI", 0, 0, 1, "",
                IParam::kFlagMeta | IParam::kFlagCannotAutomate);
#endif
#endif

#if !HIDE_FILE_BUTTONS
#ifdef APP_API
    // File selector "open"
    GetParam(kFileOpenParam)->InitInt("FileOpen", 0, 0, 1, "", IParam::kFlagMeta);
    
    // File selector "save"
    GetParam(kFileSaveParam)->InitInt("FileSave", 0, 0, 1, "", IParam::kFlagMeta);
    
    // File selector "save as"
    GetParam(kFileSaveAsParam)->InitInt("FileSaveAs", 0, 0, 1, "", IParam::kFlagMeta);
    
    // File selector "export selection"
#if (GHOST_LITE_EXPORT_SELECTION || !GHOST_LITE_VERSION)
    GetParam(kFileExportSelectionParam)->InitInt("FileExportSelection", 0, 0, 1, "",
                                                 IParam::kFlagMeta);
#endif
#endif
#endif
    
    // Out gain
    BL_FLOAT defaultOutGain = 0.;
    mOutGain = defaultOutGain;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain,
                                   -16.0, 16.0, 0.01, "dB");

    // Spectrogram / waveform display ratio
    BL_FLOAT defaultSpectWaveform = 50.0;
    mSpectWaveformRatio = defaultSpectWaveform*0.01;
    GetParam(kSpectWaveformRatio)->InitDouble("SpectWaveformDisplay",
                                         defaultSpectWaveform, 0.0, 100.0, 0.1, "%");
    
    // Waveform scale
    BL_FLOAT defaultWaveformScale = 1.0;
    mWaveformScale = defaultWaveformScale;
    GetParam(kWaveformScale)->InitDouble("WaveformScale",
                                         defaultWaveformScale,
                                         //0.0,
                                         //
                                         // Keep a tiny non null value
                                         // (otherwise the waveform would
                                         // totally disapear)
                                         0.01, 
                                         //
                                         8.0, 0.1, "");
    
    // Selection type
    SelectionType defaultSelectionType = RECTANGLE;
    mSelectionType = defaultSelectionType;
    //GetParam(kSelectionType)->InitInt("SelectionType",
    //                                  (int)defaultSelectionType, 0, 2);
    GetParam(kSelectionType)->InitEnum("SelectionType",
                                       (int)defaultSelectionType, 3,
                                       "", IParam::kFlagsNone, "",
                                       "Rectangle", "Horizontal", "Vertical");
    
    // Out gain
    BL_FLOAT defaultEditGain = 0.0;
    mEditGain = 1.0; // 1 is 0Db
    GetParam(kEditGainValue)->InitDouble("EditGainValue",
                                         defaultEditGain, -24.0, 24.0, 0.1, "dB");
    
    // Edition
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
    GetParam(kEditCopy)->InitInt("EditCopy", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kEditPaste)->InitInt("EditPaste", 0, 0, 1, "", IParam::kFlagMeta);
#endif
    
    GetParam(kEditCut)->InitInt("EditCut", 0, 0, 1, "", IParam::kFlagMeta);
    
    GetParam(kEditGain)->InitInt("EditGain", 0, 0, 1, "", IParam::kFlagMeta);
    
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE
    //GetParam(kEditReplace)->InitInt("EditReplace", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kEditReplace)->InitInt("EditInpaint", 0, 0, 1, "", IParam::kFlagMeta);
#endif
    
    GetParam(kEditUndo)->InitInt("EditUndo", 0, 0, 1, "", IParam::kFlagMeta);
    
    //GetParam(kPlayStop)->InitInt("PlayStop", 0, 0, 1, "", IParam::kFlagMeta);
    GetParam(kPlayStop)->InitEnum("PlayStop", 0, 2,
                                  "", IParam::kFlagMeta, "",
                                  "Stop", "Play");
    
    // Inpaint direction
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE
    InpaintDir defaultInpaintDir = INPAINT_BOTH;
    //GetParam(kInpaintDir)->InitInt("InpaintDir", (int)defaultInpaintDir, 0, 2);
    GetParam(kInpaintDir)->InitEnum("InpaintDir", (int)defaultInpaintDir, 3,
                                    "", IParam::kFlagsNone, "",
                                    "Both", "Horizontal", "Vertical");
#endif
  
#ifndef APP_API
    // Plug mode
    GhostPluginInterface::PlugMode defaultPlugMode = GhostPluginInterface::VIEW;
#else // APP
    GhostPluginInterface::PlugMode defaultPlugMode = GhostPluginInterface::EDIT;
#endif
    
    mPlugMode = defaultPlugMode;
    mPrevPlugMode = mPlugMode;
    //GetParam(kPlugMode)->InitInt("Mode", (int)defaultPlugMode, 0, 3);
    GetParam(kPlugMode)->InitEnum("Mode", (int)defaultPlugMode, 4,
                                  "", IParam::kFlagsNone, "",
                                  "View", "Capture", "Edit", "Render");

#ifndef APP_API
    int defaultMonitor = 0;
    mMonitorEnabled = defaultMonitor;
    //GetParam(kMonitor)->InitInt("Monitor", defaultMonitor, 0, 1);
    GetParam(kMonitor)->InitEnum("Monitor", defaultMonitor, 2,
                                 "", IParam::kFlagsNone, "",
                                 "Off", "On");
#endif

    // Spectro meter modes
    mMeterTimeMode = SpectroMeter::SPECTRO_METER_TIME_HMS;
    //GetParam(kSpectroMeterTimeMode)->InitInt("MeterTimeMode", mMeterTimeMode, 0, 1);
    GetParam(kSpectroMeterTimeMode)->InitEnum("MeterTimeMode", mMeterTimeMode, 2,
                                              "", IParam::kFlagsNone, "",
                                              "Samples", "HMS");

    mMeterFreqMode = SpectroMeter::SPECTRO_METER_FREQ_HZ;
    //GetParam(kSpectroMeterFreqMode)->InitInt("MeterFreqMode", mMeterFreqMode, 0, 1);
    GetParam(kSpectroMeterFreqMode)->InitEnum("MeterFreqMode", mMeterFreqMode, 2,
                                              "", IParam::kFlagsNone, "",
                                              "Hz", "Bins");

#ifdef APP_API
#if DEV_SPECIAL_VERSION
    int defaultLinkTracks = 0;
    mLinkTracks = defaultLinkTracks;
    GetParam(kLinkTracks)->InitInt("LinkTracks", defaultLinkTracks, 0, 1);
#endif
#endif

    bool defaultLowFreqZoom = false;
    mLowFreqZoom = defaultLowFreqZoom;
    //GetParam(kLowFreqZoom)->InitInt("LowFreqZoom", defaultLowFreqZoom, 0, 1);
    GetParam(kLowFreqZoom)->InitEnum("LowFreqZoom", defaultLowFreqZoom, 2,
                                     "", IParam::kFlagsNone, "",
                                      "Off", "On");
}

void
Ghost::ApplyParams()
{
    SetParametersToAllTracks(true);
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    SetInpaintDir(mInpaintDir);

    SetSelectionType(mSelectionType); //
    
    if (mSpectroMeter != NULL)
        mSpectroMeter->SetTimeMode(mMeterTimeMode);    
    if (mSpectroMeter != NULL)
        mSpectroMeter->SetFreqMode(mMeterFreqMode);
    
    ActivateTrack(mTrackNum);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
    
    ApplyGrayOutControls();

    // FIX: Reaper, change low freq zoom in host UI, show plug UI => refresh problem
    if ((mPlugMode == Ghost::ACQUIRE) ||
        (mPlugMode == Ghost::EDIT) ||
        (mPlugMode == Ghost::RENDER))
        // Do not do it in VIEW mode, otherwise spectro would reset when resize gui
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            GhostTrack* track = mTracks[i];
            if (track != NULL)
                track->DoRecomputeData();
        }
    }
}

// From Ghost to track
void
Ghost::SetParametersToTrack(GhostTrack *track, bool setOnlyGlobalParams)
{
    if (!setOnlyGlobalParams)
    // We will set only global parameters
    {
        // Track
        track->mRange = mRange;
        track->mContrast = mContrast;
        track->mColorMapNum = mColorMapNum;
        track->mWaveformScale = mWaveformScale;
        track->mSpectWaveformRatio = mSpectWaveformRatio;
        //track->mSelectionType = mSelectionType;
    }
    
    // Global
    track->mPlugMode = mPlugMode;
    track->mMonitorEnabled = mMonitorEnabled;

    if (track->mLowFreqZoom != mLowFreqZoom)
        track->mMustRecomputeLowFreqZoom = true;
    track->mLowFreqZoom = mLowFreqZoom;

    // This also set low freq zoom...
    track->ApplyParams();
    track->UpdateParamSelectionType(mSelectionType);

    // Y scale may have changed
    if (!mTracks.empty())
        mYScale = mTracks[0]->GetYScaleType();
}

// From track to Ghost
void
Ghost::SetParametersFromTrack(GhostTrack *track)
{
    // Track
    mRange = track->mRange;
    mContrast = track->mContrast;
    mColorMapNum = track->mColorMapNum;
    mWaveformScale = track->mWaveformScale;
    mSpectWaveformRatio = track->mSpectWaveformRatio;
    //mSelectionType = (SelectionType)track->mSelectionType;
    
    // Track
    BLUtilsPlug::SetParameterValue(this, kRange, track->mRange, true);
    BLUtilsPlug::SetParameterValue(this, kContrast, track->mContrast, true);
    BLUtilsPlug::SetParameterValue(this, kColorMap, track->mColorMapNum, true);
    BLUtilsPlug::SetParameterValue(this, kWaveformScale, track->mWaveformScale, true);
    BLUtilsPlug::SetParameterValue(this, kSpectWaveformRatio,
                                   track->mSpectWaveformRatio*100.0, true);
    //BLUtilsPlug::SetParameterValue(this, kSelectionType, track->mSelectionType, true);
    
    // Must do this additional step to apply well
    GetParam(kRange)->Set(track->mRange);
    GetParam(kContrast)->Set(track->mContrast);
    GetParam(kColorMap)->Set(track->mColorMapNum);
    GetParam(kWaveformScale)->Set(track->mWaveformScale);
    GetParam(kSpectWaveformRatio)->Set(track->mSpectWaveformRatio*100.0);
    //GetParam(kSelectionType)->Set(track->mSelectionType);
    
    // Synchronize the play button state
    SetPlayStopParameter((int)track->mIsPlaying);
}

void
Ghost::SetParametersToAllTracks(bool setOnlyGlobalParams)
{
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];
        if (track != NULL)
        {
            SetParametersToTrack(track, setOnlyGlobalParams);

            track->ApplyParams();
            track->UpdateParamSelectionType(mSelectionType);
        }
    }
}

void
Ghost::GetGraphSize(int *width, int *height)
{
    if (mTrackNum < mTracks.size())
        mTracks[mTrackNum]->GetGraphSize(width, height);
}

void
Ghost::Init()
{
    if (mIsInitialized)
        return;

#if !APP_API
    // Plugin
    BL_FLOAT sampleRate = GetSampleRate();
    GhostTrack *track = new GhostTrack(this, BUFFER_SIZE, sampleRate, mYScale);
    mTracks.push_back(track);
    mTrackNum = 0;
    
    SetParametersToTrack(track, false);
    track->Init();
#endif
    
    mIsInitialized = true;
}

void
Ghost::ProcessBlock(iplug::sample **inputs, iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.

    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    mBLUtilsPlug.CheckReset(this);
    
#ifdef __linux__
    // On Linux, process the menu action in ProcessBlock(),
    // to be "in the right thread".
    ProcessCurrentMenuAction();
#endif
    
    if (!mIsInitialized)
        return;
    
    if (!mTracks.empty())
    {
        // Use legacy mechanism
        mTracks[0]->Lock();
    }

    mIsPlaying = IsTransportPlaying();

    
    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT()

    // NOTE: not sure that calling PlugModeChanged() in the GUI size was a problem
    // (called it previously in OnParamChange())
    // But this way this is maybe better (not well tested comparison)
    
    // Do it in the right thread!
    if (mPlugModeChanged)
    {
        PlugModeChanged(mPrevPlugMode);
        
        mPlugModeChanged = false;
    }
    
#if FIX_AUDIOSUITE
#if AAX_API
    // Detect begin render with AudioSuite
    // In this case, we must rewind if we are in RENDER mode.
    // (fixes for several successive renders)
    //
    // Must do that because in AudioSuite mode, the "transport"
    // gives no info, and CheckNeedReset() won't work.
   
    // Take a delay of 250ms
    bool playbackRestarted = GhostPluginInterface::PlaybackWasRestarted(500);
    if (mPlugMode == RENDER)
    {
        if (playbackRestarted)
            OnReset();
    }
#endif
#endif
        
    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > &scIn = mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf2;
    BLUtilsPlug::GetPlugIOBuffers(this, inputs, outputs, nFrames,
                                  &in, &scIn, &out);
  

    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
    {
        if (!mTracks.empty())
        {
            mTracks[0]->Unlock();
            mTracks[0]->PushAllData();
        }
        
        return;
    }
    
    if (mIsLoadingSaving)
    {
        BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

        if (!mTracks.empty())
        {
            mTracks[0]->Unlock();
            mTracks[0]->PushAllData();
        }
        
        return;
    }

    // Set the outputs to 0
    //
    // FIX: on Protools, in render mode, after play is finished,
    // there is a buzz sound
    for (int i = 0; i < out.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &out0 = out[i];
        BLUtils::FillAllZero(&out0);
    }

#ifdef APP_API
    // Optim: avoid consuming 5/10% CPU with app when nothing happens
    if (mTrackNum < mTracks.size())
    {
        if (!mTracks[mTrackNum]->IsPlaying())
        {
            if (mTracks[0] != NULL)
            {
                mTracks[0]->Unlock();
                mTracks[0]->PushAllData();
            }

            // Set the prev zero outputs to the result
            // => This avoids an echo in app mode,
            // coming from the microphone, and going to the speaker 
            BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
            
            return;
        }
    }
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

    bool isTransportPlaying = IsTransportPlaying();
    BL_FLOAT transportSamplePos = GetTransportSamplePos();

    if (!isTransportPlaying && mMonitorEnabled)
    {
        mNumSamplesMonitor += nFrames;

        transportSamplePos += mNumSamplesMonitor;
    }
    
    if (mTrackNum < mTracks.size())
        mTracks[mTrackNum]->ProcessBlock(in, scIn, &out,
                                         isTransportPlaying,
                                         transportSamplePos);

    // Apply out gain
    BLUtilsPlug::ApplyGain(out, &out, mOutGainSmoother);
    
    BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    if (!mTracks.empty())
    {
        mTracks[0]->Unlock();
        mTracks[0]->PushAllData();
    }
    
    BL_PROFILE_END;
}

void
Ghost::CreateControls(IGraphics *pGraphics, int offset)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB_V3);

    mGUIHelper->AttachToolTipControl(pGraphics);
    mGUIHelper->AttachTextEntryControl(pGraphics);
        
    // Range
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kRangeX + offset, kRangeY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kRange,
                              TEXTFIELD_FN,
                              "BRIGHTNESS",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipRange);
    
    // Contrast
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kContrastX + offset, kContrastY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kContrast,
                              TEXTFIELD_FN,
                              "CONTRAST",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipContrast);
    

#if !USE_DROP_DOWN_MENU
    // ColorMap num
#define NUM_COLORMAP_RADIO_LABELS 10
    const char *ColorMapRadioLabels[] =
    { "BLUE", "GRAY", "GREEN", "PURPLE", "WASP", "SKY",
      "DAWN", "RAINBOW", "SWEET", "FIRE" };
    
    int numRadioButtons = kRadioButtonColorMapNumButtons;
    int vSize = kRadioButtonColorMapVSize;
    
#if GHOST_LITE_VERSION && !GHOST_LITE_ALL_COLORMAPS
    // Version 5: limited to the first 4 colormaps
    // Now, limit to the 5 first colormaps
    
    // Limit to the first 5 colormaps
    numColorMapsLabels = 5;
    numRadioButtons = 5;
    vSize /= 2;
#else
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsColorMapX + offset,
                                   kRadioButtonsColorMapY,
                                   RADIOBUTTON_FN,
                                   numRadioButtons,
                                   vSize,
                                   kColorMap,
                                   false,
                                   "COLORMAP",
                                   EAlign::Far,
                                   EAlign::Far,
                                   ColorMapRadioLabels);
#endif
    
#else
    mGUIHelper->CreateDropDownMenu(pGraphics,
                                   kColorMapX + offset, kColorMapY,
                                   kColorMapWidth,
                                   kColorMap,
                                   "COLORMAP",
                                   GUIHelper12::SIZE_DEFAULT,
                                   tooltipColormap);
#endif
    
    // GUI resize
    mGUISizeSmallButton = (IGUIResizeButtonControl *)
        mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                          kGUISizeSmallX + offset, kGUISizeSmallY,
                                          BUTTON_RESIZE_SMALL_FN,
                                          kGUISizeSmall,
                                          "", 0,
                                          tooltipGUISizeSmall);
    
    mGUISizeMediumButton = (IGUIResizeButtonControl *)
        mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                          kGUISizeMediumX + offset, kGUISizeMediumY,
                                          BUTTON_RESIZE_MEDIUM_FN,
                                          kGUISizeMedium,
                                          "", 1,
                                          tooltipGUISizeMedium);

    mGUISizeBigButton = (IGUIResizeButtonControl *)
        mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                          kGUISizeBigX + offset, kGUISizeBigY,
                                          BUTTON_RESIZE_BIG_FN,
                                          kGUISizeBig,
                                          "", 2,
                                          tooltipGUISizeBig);
#if !(!GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI)
    mGUISizeBigButton->SetDisabled(true);
#endif

#if RETINA_RESIZE_FEATURE
    mGUISizeHugeButton = (IGUIResizeButtonControl *)
        mGUIHelper->CreateGUIResizeButton(this, pGraphics,
                                          kGUISizeHugeX + offset, kGUISizeHugeY,
                                          BUTTON_RESIZE_HUGE_FN,
                                          kGUISizeHuge,
                                          "", 3,
                                          tooltipGUISizeHuge);

    int screenWidth;
    int screenHeight;
    pGraphics->GetScreenResolution(&screenWidth, &screenHeight);
    
    if ((PLUG_WIDTH_HUGE > screenWidth) ||
        (PLUG_HEIGHT_HUGE > screenHeight))
    {
        mGUISizeHugeButton->SetDisabled(true);
    }
    
#if !(!GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI)
    mGUISizeHugeButton->SetDisabled(true);
#endif
#endif
    
    // Spectrogram / waveform display ratio
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kSpectWaveformRatioX + offset, kSpectWaveformRatioY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kSpectWaveformRatio,
                              TEXTFIELD_FN,
                              "SPEC/WAV",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipSpectWaveformRatio);

    // Waveform scale
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kWaveformScaleX + offset, kWaveformScaleY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kWaveformScale,
                              TEXTFIELD_FN,
                              "WAV SCALE",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipWaveformScale);

#if !USE_BUTTON_ICONS
    // Selection type
    const char *radioLabelsSelectionType[] = { "RECT.", "HORIZ.", "VERT."};
    mSelectionTypeControl =
        mGUIHelper->CreateRadioButtons(pGraphics,
                                       kRadioButtonsSelectionTypeX + offset,
                                       kRadioButtonsSelectionTypeY,
                                       RADIOBUTTON_FN,
                                       kRadioButtonsSelectionTypeNumButtons,
                                       kRadioButtonsSelectionTypeVSize,
                                       kSelectionType,
                                       false,
                                       "SELECT",
                                       EAlign::Near,
                                       EAlign::Near,
                                       radioLabelsSelectionType);
#else
    const char *selectionDirs[] = { BUTTON_SELDIR_BOTH_FN,
                                    BUTTON_SELDIR_HORIZ_FN,
                                    BUTTON_SELDIR_VERT_FN };
    mSelectionTypeControl =
        mGUIHelper->CreateRadioButtonsCustom(pGraphics,
                                             kRadioButtonsSelectionTypeX + offset,
                                             kRadioButtonsSelectionTypeY,
                                             selectionDirs,
                                             // 3 buttons
                                             kRadioButtonsSelectionTypeNumButtons,
                                             // 3 frames
                                             kRadioButtonsSelectionTypeFrames,
                                             kRadioButtonsSelectionTypeVSize,
                                             kSelectionType,
                                             true,
                                             tooltipSelectionType);
#endif
    
    // Edit gain
    mEditGainControl =
        mGUIHelper->CreateKnobSVG(pGraphics,
                                  kEditGainValueX + offset, kEditGainValueY,
                                  kKnobSmallWidth, kKnobSmallHeight,
                                  KNOB_SMALL_FN,
                                  kEditGainValue,
                                  TEXTFIELD_FN,
                                  "GAIN",
                                  GUIHelper12::SIZE_DEFAULT,
                                  NULL, true,
                                  tooltipEditGainValue);

    // Edition
    mCopyButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                   kEditCopyX + offset, kEditCopyY,
                                                   BUTTON_COPY_FN,
                                                   kEditCopy,
                                                   NULL, false, true, false,
                                                   tooltipEditCopy);
    
    mPasteButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                    kEditPasteX + offset, kEditPasteY,
                                                    BUTTON_PASTE_FN,
                                                    kEditPaste,
                                                    NULL, false, true, false,
                                                   tooltipEditPaste);
#if !(!GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE)
    mCopyButton->SetDisabled(true);
    mPasteButton->SetDisabled(true);
#endif

    mCutButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                  kEditCutX + offset, kEditCutY,
                                                  BUTTON_CUT_FN,
                                                  kEditCut,
                                                  NULL, false, true, false,
                                                  tooltipEditCut);

    mGainButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                   kEditGainX + offset, kEditGainY,
                                                   BUTTON_APPLY_GAIN_FN,
                                                   kEditGain,
                                                   NULL, false, true, false,
                                                   tooltipEditGain);

    mInpaintButton =
        mGUIHelper->CreateRolloverButton(pGraphics,
                                         kEditReplaceX + offset, kEditReplaceY,
                                         BUTTON_INPAINT_FN,
                                         kEditReplace,
                                         NULL, false, true, false,
                                         tooltipEditReplace);
#if !(!GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE)
    mInpaintButton->SetDisabled(true);
#endif
    
    mUndoButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                   kEditUndoX + offset, kEditUndoY,
                                                   BUTTON_UNDO_FN,
                                                   kEditUndo,
                                                   NULL, false, true, false,
                                                   tooltipEditUndo);

    // Inpaint direction    
#if !USE_BUTTON_ICONS 
    const char *radioLabelsInpaintDir[] = { "BOTH", "HORIZ.", "VERT."};
    mInpaintDirControl =
        mGUIHelper->CreateRadioButtons(pGraphics,
                                       kRadioButtonsInpaintDirX + offset,
                                       kRadioButtonsInpaintDirY,
                                       RADIOBUTTON_FN,
                                       kRadioButtonsInpaintDirNumButtons,
                                       kRadioButtonsInpaintDirVSize,
                                       kInpaintDir,
                                       false,
                                       "INPAINT",
                                       EAlign::Near,
                                       EAlign::Near,
                                       radioLabelsInpaintDir);
#else
    const char *inpaintDirs[] = { BUTTON_INPAINT_BOTH_FN,
                                  BUTTON_INPAINT_HORIZ_FN,
                                  BUTTON_INPAINT_VERT_FN };
    mInpaintDirControl =
        mGUIHelper->CreateRadioButtonsCustom(pGraphics,
                                             kRadioButtonsInpaintDirX + offset,
                                             kRadioButtonsInpaintDirY,
                                             inpaintDirs,
                                             kRadioButtonsInpaintDirNumButtons,
                                             3, // 3 frames
                                             kRadioButtonsInpaintDirVSize,
                                             kInpaintDir,
                                             true/*false*/,
                                             tooltipInpaintDir);
#endif

#if !(!GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE)
    mInpaintDirControl->SetDisabled(true);
#endif

    // Play button
    mPlayButtonControl =
        mGUIHelper->CreateRolloverButton(pGraphics,
                                         kPlayStopX + offset, kPlayStopY,
                                         BUTTON_PLAY_FN,
                                         kPlayStop,
                                         "", true,
                                         true, false,
                                         tooltipEditPlayStop);
    
#ifndef APP_API // Not app, plugin !
    const char *radioLabelsPlugMode[] = { "VIEW", "CAPTURE", "EDIT", "RENDER" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsPlugModeX + offset,
                                   kRadioButtonsPlugModeY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonsPlugModeNumButtons,
                                   kRadioButtonsPlugModeVSize,
                                   kPlugMode,
                                   false,
                                   "MODE",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsPlugMode,
                                   tooltipPlugMode);
#endif

    // Edit gain
    mGUIHelper->CreateKnobSVG(pGraphics,
                              kOutGainX + offset, kOutGainY,
                              kKnobSmallWidth, kKnobSmallHeight,
                              KNOB_SMALL_FN,
                              kOutGain,
                              TEXTFIELD_FN,
                              "OUT GAIN",
                              GUIHelper12::SIZE_DEFAULT,
                              NULL, true,
                              tooltipOutGain);

#if 0 // No need
    // Trigger
    IRECT bounds2; // TODO
    GhostTriggerControl *trigger =
        new GhostTriggerControl(this, kTriggerParam, bounds2);

    //guiHelper.AddTrigger(trigger);
    GetUI()->AttachControl(trigger);
#endif
    
#ifndef APP_API
    // Monitor button
    mMonitorControl = mGUIHelper->CreateToggleButton(pGraphics,
                                                     kCheckboxMonitorX + offset,
                                                     kCheckboxMonitorY,
                                                     CHECKBOX_FN, kMonitor, "MON",
                                                     GUIHelper12::SIZE_DEFAULT, true,
                                                     tooltipMonitor);
#endif

#if !HIDE_FILE_BUTTONS
#ifdef APP_API
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kFileOpenX + offset, kFileOpenY,
                                     ROLLOVER_BUTTON_FN,
                                     kFileOpenParam,
                                     "Open...");
    
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kFileSaveX + offset, kFileSaveY,
                                     ROLLOVER_BUTTON_FN,
                                     kFileSaveParam,
                                     "Save");
    
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kFileSaveAsX + offset, kFileSaveAsY,
                                     ROLLOVER_BUTTON_FN,
                                     kFileSaveAsParam,
                                     "Save as...");
    
#if (GHOST_LITE_EXPORT_SELECTION || !GHOST_LITE_VERSION)
    mGUIHelper->CreateRolloverButton(pGraphics,
                                     kFileExportSelectionX + offset,
                                     kFileExportSelectionY,
                                     ROLLOVER_BUTTON_FN,
                                     kFileExportSelectionParam,
                                     "Export sel...");
#endif
#endif
#endif
    
#ifdef APP_API
    pGraphics->SetDropFunc([&](const char *filenames)
                           {
                               Ghost::OnDrop(filenames);
                           });
    
    pGraphics->SetKeyHandlerFunc([&](const IKeyPress& key, bool isUp)
                                 {
                                     return Ghost::OnKey(key, isUp);
                                 });
#endif

    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mSpectroMeter == NULL)
    {
        mSpectroMeter = new SpectroMeter(kSpectroMeterX, kSpectroMeterY,
                                         kSpectroMeterTextWidth,
                                         kSpectroMeterTimeMode,
                                         kSpectroMeterFreqMode,
                                         BUFFER_SIZE, sampleRate);
        mSpectroMeter->SetTextFieldHSpacing(14/*6*/);
        mSpectroMeter->SetTextFieldVSpacing(7);
        mSpectroMeter->SetTextFieldVSpacing1(25/*24*/);

        int bgColor[4];
        mGUIHelper->GetGraphCurveDarkBlue(bgColor);
        IColor bgIColor(bgColor[3], bgColor[0], bgColor[1], bgColor[2]);
        
        mSpectroMeter->SetBackgroundColor(bgIColor);
    
        IColor borderColor;
        mGUIHelper->GetValueTextColor(&borderColor);
        
        float borderWidth = 1.5;
        mSpectroMeter->SetBorderColor(borderColor);
        mSpectroMeter->SetBorderWidth(borderWidth);
    }
    
    if (mSpectroMeter != NULL)
        mSpectroMeter->GenerateUI(mGUIHelper, pGraphics, offset, 0.0);
    
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kLinkTracksX + offset,
                                   kLinkTracksY,
                                   CHECKBOX_FN, kLinkTracks, "LINK",
                                   GUIHelper12::SIZE_SMALL);
#endif
#endif

    // Low freq zoom
    mLowfreqZoomCheckbox =
        mGUIHelper->CreateToggleButton(pGraphics,
                                       kCheckboxLowFreqZoomX + offset,
                                       kCheckboxLowFreqZoomY,
                                       CHECKBOX_FN, kLowFreqZoom, "LF ZOOM",
                                       GUIHelper12::SIZE_DEFAULT, true,
                                       tooltipLowFreqZoom);
    if (mPlugMode == VIEW)
    {
        if (mLowfreqZoomCheckbox != NULL)
            mLowfreqZoomCheckbox->SetDisabled(true);
    }
    
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];
        
        track->CreateControls(mGUIHelper, this, mGraphics,
                              kGraphX, kGraphY,
                              mGUIOffsetX, mGUIOffsetY /*kGraph*/);
    }
    
#if USE_TABS_BAR
    float tabsBarWidth = kTabsBarW;

    /*if (!mTracks.empty())
      {
      int width;
      int height;
      if (mTracks[0] != NULL)
      {
      mTracks[0]->GetGraphSize(&width, &height);
      tabsBarWidth = width;
      }
      }*/
    
    // Compute tabs bar width even when there is not track
    int plugWidth = pGraphics->Width();
    tabsBarWidth += plugWidth - PLUG_WIDTH;
    
    mTabsBar = mGUIHelper->CreateTabsBar(pGraphics,
                                         kTabsBarX, kTabsBarY,
                                         tabsBarWidth, kTabsBarH);
    mTabsBar->SetListener(this);
    
    GenerateTabsBar();
#endif

    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR);

    // NOTE: at the end, make it not animated
    
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
    
    //
    mControlsCreated = true;
}

void
Ghost::UpdateWindowTitle()
{
#ifdef APP_API
    const char *currentFileName = NULL;
    if (mTrackNum < mTracks.size())
        currentFileName = mTracks[mTrackNum]->GetCurrentFileName();
        
    char title[512];
    memset(title, '\0', 512);

#if GHOST_LITE_VERSION
    const char *plugAppName = "BlueLab Ghost";
#else
    const char *plugAppName = "BlueLab Ghost-X";
#endif

    if (currentFileName != NULL)
    {
        const char *shortFileName = BLUtilsFile::GetFileName(currentFileName);
        bool fileModified = false;
        if (mTrackNum < mTracks.size())
            fileModified = mTracks[mTrackNum]->GetFileModified();
        
        const char *starSymbol = fileModified ? "*" : "";
        
        char fileName[256];
        memset(fileName, '\0', 256);
        if (strlen(shortFileName) > 0)
        {
            sprintf(fileName, "%s%s - ", starSymbol, shortFileName);
        }
        
        sprintf(title, "%s%s", fileName, plugAppName);    
        
        IPlugAPP::SetWindowTitle(title);
    }
    else
    {
        IPlugAPP::SetWindowTitle(plugAppName);
    }
#endif
}

void
Ghost::OnHostIdentified()
{
    BLUtilsPlug::SetPlugResizable(this, true);
}

void
Ghost::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();
    
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);

    if (mSpectroMeter != NULL)
        mSpectroMeter->Reset(BUFFER_SIZE, sampleRate);

    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];
        if (track != NULL)
            track->Reset(BUFFER_SIZE, sampleRate);
    }

    mNumSamplesMonitor = 0;
    
    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
Ghost::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kRange:
        {
            mRange = GetParam(paramIdx)->Value();
            
            if (mTrackNum < mTracks.size())
                mTracks[mTrackNum]->UpdateParamRange(mRange);
        }
        break;
            
        case kContrast:
        {
            mContrast = GetParam(paramIdx)->Value();
            
            if (mTrackNum < mTracks.size())
                mTracks[mTrackNum]->UpdateParamContrast(mContrast);
        }
        break;
            
        case kColorMap:
        {
            mColorMapNum = GetParam(paramIdx)->Int();
            
            if (mTrackNum < mTracks.size())
                mTracks[mTrackNum]->UpdateParamColorMap(mColorMapNum);
        }
        break;

        case kSpectWaveformRatio:
        {
            BL_FLOAT spectWaveform = GetParam(paramIdx)->Value();
            mSpectWaveformRatio = spectWaveform/100.0;

            if (mTrackNum < mTracks.size())
                mTracks[mTrackNum]->
                    UpdateParamSpectWaveformRatio(mSpectWaveformRatio);
        }
        break;
    
        case kWaveformScale:
        {
            BL_FLOAT waveformScale = GetParam(paramIdx)->Value();
            mWaveformScale = waveformScale;

            if (mTrackNum < mTracks.size())
                mTracks[mTrackNum]->UpdateParamWaveformScale(mWaveformScale);
        }
        break;
        
#ifndef APP_API
        case kPlugMode:
        {
            PlugMode mode = (PlugMode)GetParam(paramIdx)->Int();
      
            PlugMode prevMode = mPlugMode;
            mPrevPlugMode = prevMode;
      
            if (mode != mPlugMode)
            {
                mPlugMode = mode;

                mPlugModeChanged = true;
            }

            // Global
            for (int i = 0; i < mTracks.size(); i++)
            {
                mTracks[i]->UpdateParamPlugMode(mPlugMode);
            }
        }
        break;
#endif

#ifndef APP_API
        case kMonitor:
        {
            int value = GetParam(paramIdx)->Int();
            bool valueBool = (value == 1);

            if (valueBool != mMonitorEnabled)
            {
                mMonitorEnabled = valueBool;

                // Global
                for (int i = 0; i < mTracks.size(); i++)
                {
                    mTracks[i]->UpdateParamMonitorEnabled(mMonitorEnabled);
                }
            }
        }
        break;
#endif
        
        case kSelectionType:
        {
            SelectionType type = (SelectionType)GetParam(paramIdx)->Int();
            
            mSelectionType = type;

            SetSelectionType(mSelectionType);
            
#if 0
            if (!mLinkTracks)
                // Track by track
            {
                if (mTrackNum < mTracks.size())
                    mTracks[mTrackNum]->UpdateParamSelectionType(mSelectionType);
            }
            else
                // Global
            {
                for (int i = 0; i < mTracks.size(); i++)
                {
                    if (i == mTrackNum)
                        // Will process the current track at the end
                        continue;
                    
                    if (i < mTracks.size())
                        mTracks[i]->UpdateParamSelectionType(mSelectionType);
                }

                // Process the current track at the end
                mTracks[mTrackNum]->UpdateParamSelectionType(mSelectionType);
            }
#endif
        }
        break;

        case kOutGain:
        {
            BL_FLOAT gain = GetParam(paramIdx)->DBToAmp();
            mOutGain = gain;
            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(mOutGain);
        }
        break;
        
#if !HIDE_FILE_BUTTONS
        case kFileOpenParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
                PromptForFileOpen();
        }
        break;
    
        case kFileSaveParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
                SaveFile(mCurrentFileName);
        }
        break;
    
        case kFileSaveAsParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
                PromptForFileSaveAs();
        }
        break;
    
#if (GHOST_LITE_EXPORT_SELECTION || !GHOST_LITE_VERSION)
        case kFileExportSelectionParam:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
                PromptForFileExportSelection();
        }
        break;
#endif

#endif // HIDE_FILE_BUTTONS
        
        // Edition
        //
        case kEditCopy:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                // Edition only in edit mode !
                if (mPlugMode == Ghost::EDIT)
                    DoCopyCommand();

                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
    
        case kEditPaste:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                if (mPlugMode == Ghost::EDIT)
                    DoPasteCommand();

                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
    
        case kEditCut:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
                // Full version
                if (mPlugMode == Ghost::EDIT)
                    DoCutCopyCommand();
#else
                // Lite version
                if (mPlugMode == Ghost::EDIT)
                    DoCutCommand();
#endif
                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
    
        case kEditGain:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                if (mPlugMode == Ghost::EDIT)
                    DoGainCommand();

                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
    
        case kEditReplace:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                if (mPlugMode == Ghost::EDIT)
                    //DoReplaceCommand();
                    DoReplaceCopyCommand();

                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
    
        case kEditUndo:
        {
            int value = GetParam(paramIdx)->Value();
            if (value == 1)
            {
                if (mPlugMode == Ghost::EDIT)
                    UndoLastCommand();

                // NOTE: not sure it's still useful since
                // IRolloverButtonControl : FIX_HILIGHT_REOPEN
                
                // The action is done, reset the button
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;
       
        case kEditGainValue:
        {
            BL_FLOAT editGain = GetParam(paramIdx)->DBToAmp();
            mEditGainFactor = editGain;
        }
        break;
        
        case kInpaintDir:
        {
            InpaintDir dir = (InpaintDir)GetParam(paramIdx)->Int();
            mInpaintDir = dir;
      
            SetInpaintDir(mInpaintDir);
        }
        break;
          
        // Play button
        case kPlayStop:
        {
            if (mPlugMode == EDIT)
            {
                int val = GetParam(paramIdx)->Int();

                if (val == 1)
                    StartPlay();
                else
                    StopPlay();
            }
            else
            {
                GUIHelper12::ResetParameter(this, paramIdx, false);
            }
        }
        break;

        // GUI sizes
        //
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
            
        case kGUISizeHuge:
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

        // Spectro meter
        case kSpectroMeterTimeMode:
        {
            int value = GetParam(paramIdx)->Int();
            mMeterTimeMode = (SpectroMeter::TimeMode)value;
            
            if (mSpectroMeter != NULL)
                mSpectroMeter->SetTimeMode(mMeterTimeMode);
        }
        break;
            
        case kSpectroMeterFreqMode:
        {
            int value = GetParam(paramIdx)->Int();
            mMeterFreqMode = (SpectroMeter::FreqMode)value;
            
            if (mSpectroMeter != NULL)
                mSpectroMeter->SetFreqMode(mMeterFreqMode);
        } 
        break;

#ifdef APP_API
#if DEV_SPECIAL_VERSION
        case kLinkTracks:
        {
            int value = GetParam(paramIdx)->Int();

            mLinkTracks = value;
        }
        break;
#endif
#endif
        
        case kLowFreqZoom:
        {
            if (mPlugMode != VIEW)
            {
                int value = GetParam(paramIdx)->Int();
                
                mLowFreqZoom = (value == 1);
                
                SetLowFreqZoom(mLowFreqZoom);
            }
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];

        track->OnUIOpen();
    }

#if APP_API
    CheckAppStartupArgs();
#endif
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    //mMonitorControl = NULL;

    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];

        track->OnUIClose();
    }

    mGraphics = NULL;

    SetAllControlsToNull();
        
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;

    if (mSpectroMeter != NULL)
        mSpectroMeter->ClearUI();

#ifdef APP_API
    SaveAppPreferences();
#endif
    
    LEAVE_PARAMS_MUTEX;
}

// NOTE: Would crash when opening very large files
// under the debugger, and using Scheme -> Guard Edge/ Guard Malloc
void
Ghost::OpenFile(const char *fileName)
{
    ENTER_PARAMS_MUTEX;

    mIsLoadingSaving = true;

    NewTab(fileName);
        
    if (mTrackNum < mTracks.size())
    {
        GhostTrack *track = mTracks[mTrackNum];
        
        bool success = track->OpenFile(fileName);

        if (success)
        {
            SetPlayStopParameter(0);

            // Clear previous command history
            // (just in case it remained commands in the history)
            track->mCommandHistory->Clear();

            int width;
            int height;
            track->GetGraphSize(&width, &height);
            SetBarPos(0.0*width);

            // Keep the load path
            BLUtilsFile::GetFilePath(fileName, mCurrentLoadPath, true);
        }
        else
        {
            // Open file failed
            // Remove the new created tab
            OnTabClose(mTrackNum);
        }
    }

    UpdateWindowTitle();
    
    mIsLoadingSaving = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::SaveFile(const char *fileName)
{
    ENTER_PARAMS_MUTEX;
    
    mIsLoadingSaving = true;
    
    if (strlen(fileName) < 2)
    {
        mIsLoadingSaving = false;
     
        LEAVE_PARAMS_MUTEX;
        
        return;
    }

    if (mTrackNum < mTracks.size())
    {
        GhostTrack *track = mTracks[mTrackNum];
        
        bool success = track->SaveFile(fileName);

        if (success)
        {
            UpdateWindowTitle();

            // Keep save path
            BLUtilsFile::GetFilePath(fileName, mCurrentSavePath, true);
        }
    }
    
    mIsLoadingSaving = false;
    
    LEAVE_PARAMS_MUTEX;
}

bool
Ghost::SaveFile(const char *fileName,
                const vector<WDL_TypedBuf<BL_FLOAT> > &channels)
{
    ENTER_PARAMS_MUTEX;
    
    mIsLoadingSaving = true;

    if (mTrackNum < mTracks.size())
    {
        GhostTrack *track = mTracks[mTrackNum];
            
        bool success = track->SaveFile(fileName, channels);
    
        if (success)    
            UpdateWindowTitle();
    }
    
    mIsLoadingSaving = false;
    
    LEAVE_PARAMS_MUTEX;
    
    return true;
}

void
Ghost::CloseFile(int trackNum)
{
    if (trackNum >= mTracks.size())
        return;
    
    GhostTrack *track = mTracks[trackNum];
    
    track->CloseFile();
    
    UpdateWindowTitle();
}

// New method, using SamplesToMagnPhases
void
Ghost::ExportSelection(const char *fileName)
{
    ENTER_PARAMS_MUTEX;
    
    mIsLoadingSaving = true;

    if (mTrackNum < mTracks.size())
    {
        GhostTrack *track = mTracks[mTrackNum];
        
        bool success = track->ExportSelection(fileName);

        if (success)
        {
            // Keep save path
            BLUtilsFile::GetFilePath(fileName, mCurrentSavePath, true);
        }
    }
    
    mIsLoadingSaving = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::SaveAppPreferences()
{    
    IByteChunk chunk;
    /*bool success = */SerializeParams(chunk);
            
    // Data
    int dataSize = chunk.Size();
    const uint8_t *data = chunk.GetData();

    char settingsFileName[2048];
    BLUtilsFile::GetPreferencesFileName(BUNDLE_NAME, settingsFileName);
        
    // Save
    FILE *file = fopen(settingsFileName, "wb+");
    if (file == NULL)
        return;
    fwrite(data, 1, dataSize, file);
    fclose(file);
}

void
Ghost::LoadAppPreferences()
{
    // Save some parameters
    //
    
    // Save play button state
    BL_FLOAT playButtonState = GetParam(kPlayStop)->Value();

    // Reload
    char settingsFileName[2048];
    BLUtilsFile::GetPreferencesFileName(BUNDLE_NAME, settingsFileName);

    long dataSize = BLUtilsFile::GetFileSize(settingsFileName);
    if (dataSize == 0)
        return;
    
    IByteChunk chunk;
    chunk.Resize(dataSize);

    uint8_t *data = chunk.GetData();
    
    FILE *file = fopen(settingsFileName, "rb+");
    if (file == NULL)
        return;
    fread(data, 1, dataSize, file);
    fclose(file);

    // Unserialize
    int startPos = 0;
    /*int pos = */UnserializeParams(chunk, startPos);

    // Restore some parameters
    //

    // Rsetore play button state
    GetParam(kPlayStop)->Set(playButtonState);
}

void
Ghost::ResetAppPreferences()
{
    int lastParamIdx = kMonitor;
    
    // Reset all params to default
    //DefaultParamValues();
    DefaultParamValues(0, lastParamIdx);

    // With this, the parameter is well applied to the the plug
    for (int i = 0; i <= lastParamIdx/*kNumParams*/; i++)
        BLUtilsPlug::TouchPlugParam(this, i);

    // With this, the GUI controls will move according to param reset to default
    ApplyParams();
    
    // Apply to all tracks
    SetParametersToAllTracks(false);

    // Save
    SaveAppPreferences();
}

void
Ghost::GetNewGUISize(int guiSizeIdx, int *width, int *height)
{
    int guiSizes[][2] = {
        { PLUG_WIDTH, PLUG_HEIGHT },
        { PLUG_WIDTH_MEDIUM, PLUG_HEIGHT_MEDIUM },
        { PLUG_WIDTH_BIG, PLUG_HEIGHT_BIG },
        { PLUG_WIDTH_HUGE, PLUG_HEIGHT_HUGE }
    };
    
    *width = guiSizes[guiSizeIdx][0];
    *height = guiSizes[guiSizeIdx][1];
}

void
Ghost::PreResizeGUI(int guiSizeIdx,
                    int *outNewGUIWidth, int *outNewGUIHeight)
{
    IGraphics *pGraphics = GetUI();
    if (pGraphics == NULL)
        return;
    
    ENTER_PARAMS_MUTEX;
    
    GetNewGUISize(guiSizeIdx, outNewGUIWidth, outNewGUIHeight);

    UpdatePrevMouse(*outNewGUIWidth, *outNewGUIHeight);
    
    GUIResizeComputeOffsets(PLUG_WIDTH, PLUG_HEIGHT,
                            *outNewGUIWidth, *outNewGUIHeight,
                            &mGUIOffsetX, &mGUIOffsetY);
    
    //mMonitorControl = NULL;
    SetAllControlsToNull();
    
    mControlsCreated = false;

    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];
                        
        track->PreResizeGUI();
    }
    
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];

        track->OnUIClose();
    }
    
    // Controls will be re-created automatically
    pGraphics->SetLayoutOnResize(true);
    
    // Edit controls
    //mSelectionTypeControl = NULL;
    //mEditGainControl = NULL;
    
    //mCopyButton = NULL;
    //mPasteButton = NULL;
    //mCutButton = NULL;
    //mGainButton = NULL;
    //mInpaintButton = NULL;
    //mUndoButton = NULL;
    
    //mInpaintDirControl = NULL;
    //mPlayButtonControl = NULL;
    
    LEAVE_PARAMS_MUTEX;
}


#if RETINA_RESIZE_FEATURE
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI
void
Ghost::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium,
                              kGUISizeBig, kGUISizeHuge };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
        { mGUISizeSmallButton, mGUISizeMediumButton,
          mGUISizeBigButton, mGUISizeHugeButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   4);
}
#endif
#endif

#if !RETINA_RESIZE_FEATURE
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_BIG_GUI
void
Ghost::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium, kGUISizeBig };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
        { mGUISizeSmallButton, mGUISizeMediumButton, mGUISizeBigButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   3);
}
#endif
#endif

#if GHOST_LITE_VERSION && !GHOST_LITE_ENABLE_BIG_GUI
void
Ghost::GUIResizeParamChange(int guiSizeIdx)
{
    int guiResizeParams[] = { kGUISizeSmall, kGUISizeMedium };
    
    IGUIResizeButtonControl *guiResizeButtons[] =
        { mGUISizeSmallButton, mGUISizeMediumButton };
    
    ResizeGUIPluginInterface::GUIResizeParamChange(guiSizeIdx,
                                                   guiResizeParams, guiResizeButtons,
                                                   2);
}
#endif

void
Ghost::PlugModeChanged(PlugMode prevMode)
{
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];
        
        track->PlugModeChanged(prevMode);
    }
    
    // Change mode ? => Toggle off the play button!
    SetPlayStopParameter(false);
    
    //
    switch(mPlugMode)
    {
        case EDIT:
        {
            // Set the bar
            SetBarActive(true);
            
            int width = 0;
            int height = 0;
            if (!mTracks.empty())
                mTracks[0]->GetGraphSize(&width, &height);
            
            SetBarPos(0.0*width);
            
            ResetPlayBar();

            SetLatency(PLUG_LATENCY);

            if (mLowfreqZoomCheckbox != NULL)
                mLowfreqZoomCheckbox->SetDisabled(false);
            SetLowFreqZoom(mLowFreqZoom);
        }
        break;
            
        case VIEW:
        {
            OnReset();

            SetLatency(0);

            if (mLowfreqZoomCheckbox != NULL)
                mLowfreqZoomCheckbox->SetDisabled(true);
            SetLowFreqZoom(false);
        }
        break;
            
        case ACQUIRE:
        {
            OnReset();

            SetLatency(0);

            if (mLowfreqZoomCheckbox != NULL)
                mLowfreqZoomCheckbox->SetDisabled(false);
            SetLowFreqZoom(mLowFreqZoom);
        }
        break;
            
        case RENDER:
        {
            OnReset();
            
            // Set the bar
            SetBarActive(true);
            
            int width = 0;
            int height = 0;
            if (mTracks[0] != NULL)
                mTracks[0]->GetGraphSize(&width, &height);
            
            SetBarPos(0.0*width);
            
            ResetPlayBar();
            
            StartPlay();

            SetLatency(PLUG_LATENCY);

            if (mLowfreqZoomCheckbox != NULL)
                mLowfreqZoomCheckbox->SetDisabled(false);
            SetLowFreqZoom(mLowFreqZoom);
        }
        break;
            
        default:
            break;
    };

    // Will do this in the GUI thread (in OnIdle())
    mNeedGrayOutControls = true;
    //ApplyGrayOutControls();
}

BL_FLOAT
Ghost::GetBarPos()
{
    if (mTrackNum >= mTracks.size())
        return 0.0;

    BL_FLOAT pos = mTracks[mTrackNum]->GetBarPos();

    return pos;
}

void
Ghost::SetBarPos(BL_FLOAT x)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->SetBarPos(x);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetBarPos(x);
}
  
void
Ghost::UpdateZoom(BL_FLOAT zoomChange)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->UpdateZoom(zoomChange);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdateZoom(zoomChange);
}

void
Ghost::SetNeedRecomputeData(bool flag)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->SetNeedRecomputeData(flag);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetNeedRecomputeData(flag);
}

void
Ghost::CheckRecomputeData()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    ENTER_PARAMS_MUTEX;

#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->CheckRecomputeData();
        }

        LEAVE_PARAMS_MUTEX;
        
        return;
    }
#endif
#endif
    
    mTracks[mTrackNum]->CheckRecomputeData();
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::UpdateSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                       bool updateCenterPos, bool activateDrawSelection,
                       bool updateCustomControl)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            UpdateSelectionAux(i,
                               x0, y0, x1, y1,
                               updateCenterPos, activateDrawSelection,
                               updateCustomControl);
        }

        return;
    }
#endif
#endif
    
    UpdateSelectionAux(mTrackNum,
                       x0, y0, x1, y1,
                       updateCenterPos, activateDrawSelection,
                       updateCustomControl);
}

void
Ghost::UpdateSelectionAux(int trackNum,
                          BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                          bool updateCenterPos, bool activateDrawSelection,
                          bool updateCustomControl)
{
    if (trackNum >= mTracks.size())
        return;

    mTracks[trackNum]->UpdateSelection(x0, y0, x1, y1,
                                       updateCenterPos, activateDrawSelection,
                                       updateCustomControl);

    if (mSpectroMeter != NULL)
    {
        // Display selection values only if a selection is drawn on the view
        // Do not display values if selection is set, but only with the play bar
        // (and not with displayed rectangle)
        bool selActive = mTracks[trackNum]->IsSelectionActive();
        if (selActive)
        {
            BL_FLOAT x0f;
            BL_FLOAT y0f;
    
            BL_FLOAT x1f;
            BL_FLOAT y1f;
            mTracks[trackNum]->ConvertSelection(&x0, &y0, &x1, &y1,
                                                &x0f, &y0f, &x1f, &y1f);
    
            BL_FLOAT timeX0 = mTracks[trackNum]->GraphNormXToTime(x0f);
            BL_FLOAT timeX1 = mTracks[trackNum]->GraphNormXToTime(x1f);
            
            BL_FLOAT freqY0 = mTracks[trackNum]->GraphNormYToFreq(y0f);
            BL_FLOAT freqY1 = mTracks[trackNum]->GraphNormYToFreq(y1f);
        
            mSpectroMeter->SetSelectionValues(timeX0, freqY0,
                                              std::fabs(timeX1 - timeX0),
                                              std::fabs(freqY1 - freqY0));
        }
        else
        {
            // If no selection rectangle is drawn, don't display values
            mSpectroMeter->ResetSelectionValues();
        }
    }
}

bool
Ghost::IsSelectionActive()
{
    if (mTrackNum >= mTracks.size())
        return false;

    bool res = mTracks[mTrackNum]->IsSelectionActive();

    return res;
}

void
Ghost::SetSelectionActive(bool flag)
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetSelectionActive(flag);

    if (mSpectroMeter != NULL)
    {
        if (!flag)
            mSpectroMeter->ResetSelectionValues(); 
    }
}

void
Ghost::CursorMoved(BL_FLOAT x, BL_FLOAT y)
{
    if (mTrackNum >= mTracks.size())
        return;

    if ((x < 0.0) || (y < 0.0))
        return;
    
    //
    BL_FLOAT x0 = x;
    BL_FLOAT y0 = y;
    
    BL_FLOAT dummyX1 = x;
    BL_FLOAT dummyY1 = y;

    //
    BL_FLOAT x0f;
    BL_FLOAT y0f;
    
    BL_FLOAT x1f;
    BL_FLOAT y1f;
    mTracks[mTrackNum]->ConvertSelection(&x0, &y0, &dummyX1, &dummyY1,
                                         &x0f, &y0f, &x1f, &y1f);
    
    if (mSpectroMeter != NULL)
    {
        BL_FLOAT timeX = mTracks[mTrackNum]->GraphNormXToTime(x0f);
        BL_FLOAT freqY = mTracks[mTrackNum]->GraphNormYToFreq(y0f);
        
        mSpectroMeter->SetCursorPosition(timeX, freqY);
    }

    mPrevMouseX = x;
    mPrevMouseY = y;
}

void
Ghost::CursorOut()
{
    mPrevMouseX = -1.0;
    mPrevMouseY = -1.0;
}

void
Ghost::ClearBar()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->ClearBar();
}

void
Ghost::StartPlay()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    ENTER_PARAMS_MUTEX;

    mTracks[mTrackNum]->StartPlay();
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::StopPlay()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    ENTER_PARAMS_MUTEX;

    mTracks[mTrackNum]->StopPlay();
    
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::TogglePlayStop()
{
    if (!PlayStarted())
    {
        StartPlay();
            
        // Synchronize the play button state
        SetPlayStopParameter(1);
    }
    else
    {
        StopPlay();
            
        // Synchronize the play button state
        SetPlayStopParameter(0);
    }
}

bool
Ghost::PlayStarted()
{
    if (mTrackNum >= mTracks.size())
        return false;;

    bool res = mTracks[mTrackNum]->PlayStarted();
    
    return res;
}

void
Ghost::RewindView()
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->RewindView();
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->RewindView();
}

void
Ghost::Translate(int dX)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->Translate(dX);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->Translate(dX);
}

void
Ghost::ResetPlayBar()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->ResetPlayBar();
}

bool
Ghost::PlayBarOutsideSelection()
{
    if (mTrackNum >= mTracks.size())
        return false;

    bool res = mTracks[mTrackNum]->PlayBarOutsideSelection();

    return res;
}

void
Ghost::UpdatePlayBar()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdatePlayBar();
}

BL_FLOAT
Ghost::GetNormCenterPos()
{
    if (mTrackNum >= mTracks.size())
        return 0.0;

    BL_FLOAT res = mTracks[mTrackNum]->GetNormCenterPos();

    return res;
}

void
Ghost::UpdateSpectroEdit()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdateSpectroEdit();
}

// Warning: only the pointer is passed, no copy
void
Ghost::UpdateSpectroEditSamples()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdateSpectroEditSamples();
}

void
Ghost::UpdateMiniView()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdateMiniView();
}

void
Ghost::UpdateWaveform()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    mTracks[mTrackNum]->UpdateWaveform();
}

void
Ghost::UpdateMiniViewData()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    mTracks[mTrackNum]->UpdateMiniViewData();
}

void
Ghost::UpdateSpectrogramAlpha()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    mTracks[mTrackNum]->UpdateSpectrogramAlpha();
}

void
Ghost::BarSetSelection(int x)
{
    if (mTrackNum >= mTracks.size())
        return;
    
    mTracks[mTrackNum]->BarSetSelection(x);
}

void
Ghost::SetInpaintDir(InpaintDir dir)
{
    switch(dir)
    {
        case INPAINT_BOTH:
        {
            mInpaintProcessHorizontal = true;
            mInpaintProcessVertical = true;
        }
        break;
            
        case INPAINT_HORIZONTAL:
        {
            mInpaintProcessHorizontal = true;
            mInpaintProcessVertical = false;
        }
        break;
            
        case INPAINT_VERTICAL:
        {
            mInpaintProcessHorizontal = false;
            mInpaintProcessVertical = true;
        }
        break;
            
        default:
            break;
    }
}

void
Ghost::SetSelectionType(SelectionType selectionType)
{
    if (mTracks.empty())
        return;
            
    for (int i = 0; i < mTracks.size(); i++)
    {
        if (i == mTrackNum)
            // Will process the current track at the end
            continue;
        
        if (i < mTracks.size())
            mTracks[i]->UpdateParamSelectionType(mSelectionType);
    }
    
    // Process the current track at the end
    mTracks[mTrackNum]->UpdateParamSelectionType(mSelectionType);
}

void
Ghost::DoCutCommand()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    // Cut only if we have currently selected a rectangle
    if (!mTracks[mTrackNum]->IsSelectionActive())
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    // 2, for 2 channels
    GhostCommand *commands[2] = { NULL, NULL };
    commands[0] = new GhostCommandCut(sampleRate);

    int numChannels = mTracks[mTrackNum]->GetNumChannels();
    if (numChannels == 2)
        commands[1] = new GhostCommandCut(sampleRate);
    
    DoCommand(commands);
}

void
Ghost::DoCutCopyCommand()
{
    DoCopyCommand();

    // Save last copy paste commands
    // Because the will be set to NULL by DoCutCommand()
    GhostCommandCopyPaste *saveCopyPasteCommands[2] =
        { mCurrentCopyPasteCommands[0], mCurrentCopyPasteCommands[1] };
    
    DoCutCommand();

    // Restore the previous copy paste command
    mCurrentCopyPasteCommands[0] = saveCopyPasteCommands[0];
    mCurrentCopyPasteCommands[1] = saveCopyPasteCommands[1];
}

void
Ghost::DoGainCommand()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    // Cut only if we have currently selected a rectangle
    if (!mTracks[mTrackNum]->IsSelectionActive())
        return;
        
    BL_FLOAT sampleRate = GetSampleRate();
    
    // 2, for 2 channels
    GhostCommand *commands[2] = { NULL, NULL };
    commands[0] = new GhostCommandGain(sampleRate, mEditGainFactor);
    
    int numChannels = mTracks[mTrackNum]->GetNumChannels();
    if (numChannels == 2)
        commands[1] = new GhostCommandGain(sampleRate, mEditGainFactor);
    
    DoCommand(commands);
}

void
Ghost::DoReplaceCommand()
{
    bool selActive = false;
    if (mTrackNum < mTracks.size())
        selActive = mTracks[mTrackNum]->IsSelectionActive();
    
    // Cut only if we have currently selected a rectangle
    if (!selActive)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    // 2, for 2 channels
    GhostCommand *commands[2] = { NULL, NULL };
    commands[0] = new GhostCommandReplace(sampleRate,
                                          mInpaintProcessHorizontal,
                                          mInpaintProcessVertical);
    
    int numChannels = mTracks[mTrackNum]->GetNumChannels();
    if (numChannels == 2)
        commands[1] =
        new GhostCommandReplace(sampleRate,
                                mInpaintProcessHorizontal,
                                mInpaintProcessVertical);
    
    int numTimesDoCommand = 1;
    
#if APPLY_TWO_TIMES_REPLACE_HACK
    // HACK: to apply two times the replace command
    // otherwise it is not well replaced
    numTimesDoCommand = 2;
#endif
    
    DoCommand(commands, numTimesDoCommand);
}

void
Ghost::DoReplaceCopyCommand()
{
    // Copy, to make possible "inpaint->paste"
    DoCopyCommand();

    // Save last copy paste commands
    // Because the will be set to NULL by replace DoReplace() command
    GhostCommandCopyPaste *saveCopyPasteCommands[2] =
        { mCurrentCopyPasteCommands[0], mCurrentCopyPasteCommands[1] };

    DoReplaceCommand();

    // Restore the previous copy paste command
    mCurrentCopyPasteCommands[0] = saveCopyPasteCommands[0];
    mCurrentCopyPasteCommands[1] = saveCopyPasteCommands[1];
}

void
Ghost::DoCopyCommand()
{
    int numChannels = 0;
    if (mTrackNum < mTracks.size())
        numChannels = mTracks[mTrackNum]->GetNumChannels();
    
    BL_FLOAT sampleRate = GetSampleRate();
    
    GhostCommandCopyPaste *commands[2] = { NULL, NULL };
    commands[0] = new GhostCommandCopyPaste(sampleRate);
    
    if (numChannels == 2)
        commands[1] = new GhostCommandCopyPaste(sampleRate);
    
    BL_FLOAT x0;
    BL_FLOAT y0;
    BL_FLOAT x1;
    BL_FLOAT y1;
    if (mTrackNum >= mTracks.size())
        return;
    bool res = mTracks[mTrackNum]->GetNormDataSelection(&x0, &y0, &x1, &y1);
    if (!res)
        return;
    
    // Don't clip x: allow partially out of bounds selection
    
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
            commands[i]->SetSelection(x0, y0, x1, y1, mYScale);
    }
    
    vector<WDL_TypedBuf<BL_FLOAT> > *magns = mTmpBuf9;
    vector<WDL_TypedBuf<BL_FLOAT> > *phases = mTmpBuf10;
    if (mTrackNum < mTracks.size())
        mTracks[mTrackNum]->ReadSpectroDataSlice(magns, phases, x0, x1);
    
    int trackNumSamples = mTracks[mTrackNum]->GetNumSamples();
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
            commands[i]->Copy(magns[i], phases[i], trackNumSamples);
    }
    
    mCurrentCopyPasteCommands[0] = commands[0];
    mCurrentCopyPasteCommands[1] = commands[1];
}

void
Ghost::DoPasteCommand()
{
    if (mTrackNum >= mTracks.size())
        return;   
    if (mTracks[mTrackNum]->mCommandHistory == NULL)
        return;
    if (mCurrentCopyPasteCommands[0] == NULL)
        return;
    
    GhostCommandCopyPaste *commands[2];
    commands[0] = mCurrentCopyPasteCommands[0];
    commands[1] = mCurrentCopyPasteCommands[1];

    if (commands[1] == NULL)
    {
        if (mTracks[mTrackNum]->GetNumChannels() > 1)
        {
            // Copied from mono track, pasted to stereo track
            
            // Duplicate the left channel command
            commands[1] =
                new GhostCommandCopyPaste(*mCurrentCopyPasteCommands[0]);
        }
    }
    
    int trackNumSamples = mTracks[mTrackNum]->GetNumSamples();
    if (commands[0] != NULL)
        commands[0]->SetDstTrackNumSamples(trackNumSamples);
    if (commands[1] != NULL)
        commands[1]->SetDstTrackNumSamples(trackNumSamples);
            
    if (mCurrentCopyPasteCommands[0]->IsPasteDone())
        // Paste has been already done
    {
        // TODO: free the overriden command ?
        
        // Create a copy of the command, for undo mechanisme
        commands[0] = new GhostCommandCopyPaste(*mCurrentCopyPasteCommands[0]);
        
        if (mCurrentCopyPasteCommands[1] != NULL)
            commands[1] = new GhostCommandCopyPaste(*mCurrentCopyPasteCommands[1]);
        
        mCurrentCopyPasteCommands[0] = commands[0];
        mCurrentCopyPasteCommands[1] = commands[1];
    }
    
    // Apply
    BL_FLOAT x0;
    BL_FLOAT y0;
    BL_FLOAT x1;
    BL_FLOAT y1;

    bool res = mTracks[mTrackNum]->GetNormDataSelection(&x0, &y0, &x1, &y1);
    if (!res)
        return;
    
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
        {
            commands[i]->SetSelection(x0, y0, x1, y1, mYScale);
            commands[i]->ComputePastedSelection();
        }
    }
    
    BL_FLOAT pasteSelTmp[4];
    commands[0]->GetPastedSelection(pasteSelTmp, Scale::LINEAR);
    
    BL_FLOAT pasteSelX0 = pasteSelTmp[0];
    BL_FLOAT pasteSelX1 = pasteSelTmp[2];
    
    vector<WDL_TypedBuf<BL_FLOAT> > *magns = mTmpBuf11;
    vector<WDL_TypedBuf<BL_FLOAT> > *phases = mTmpBuf12;
    mTracks[mTrackNum]->ReadSpectroDataSlice(magns, phases, pasteSelX0, pasteSelX1);
 
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
        {
            commands[i]->ApplySlice(&magns[i], &phases[i]);
        }
    }

    mTracks[mTrackNum]->WriteSpectroDataSlice(magns, phases,
                                              pasteSelX0, pasteSelX1,
                                              COMMAND_FADE_NUM_SAMPLES);
    
    RefreshData(commands[0]);
    
    mTracks[mTrackNum]->mCommandHistory->AddCommand(commands[0]);
    
    if (commands[1] != NULL)
        mTracks[mTrackNum]->mCommandHistory->AddCommand(commands[1]);
    
    // Update selection
    //
    
    // Deactivate bar (we will create a rectangular selection)
    mTracks[mTrackNum]->ClearBar();
    
    // Activate the selection, as if the used made it with mouse
    mTracks[mTrackNum]->SetSelectionActive(true);
    
    // Set the selection to the pasted block
    BL_FLOAT pastedSelection[4];
    commands[0]->GetPastedSelection(pastedSelection, mYScale);
    
    mTracks[mTrackNum]->DataToViewRef(&pastedSelection[0], &pastedSelection[1],
                                      &pastedSelection[2], &pastedSelection[3]);
    
    
    SetDataSelection(pastedSelection[0], pastedSelection[1],
                     pastedSelection[2], pastedSelection[3]);
}

void
Ghost::DoCommand(GhostCommand *commands[2], int numTimes)
{
    if (mTrackNum >= mTracks.size())
        return;
    
    if (mTracks[mTrackNum]->mCommandHistory == NULL)
        return;
    
    BL_FLOAT x0;
    BL_FLOAT y0;
    BL_FLOAT x1;
    BL_FLOAT y1;
    bool res = mTracks[mTrackNum]->GetNormDataSelection(&x0, &y0, &x1, &y1);
    if (!res)
        return;
    
    // Do not check x: allow partially out of bounds selections
    
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
            commands[i]->SetSelection(x0, y0, x1, y1, mYScale);
    }
    
    for (int j = 0; j < numTimes; j++) // HACK, to apply to times the replace command
    {
        vector<WDL_TypedBuf<BL_FLOAT> > *magns = mTmpBuf13;
        vector<WDL_TypedBuf<BL_FLOAT> > *phases = mTmpBuf14;

        // NOTE: tested read and write slices without any transformation
        // => the signal is the same!
        // (except some very small variations at coeff = signal*2.5e-5)
        
        // Read data
        mTracks[mTrackNum]->ReadSpectroDataSlice(magns, phases, x0, x1);

#if 1 // Set to 0 to DEBUG
        // Apply
        for (int i = 0; i < 2; i++)
        {
            if (commands[i] != NULL)
                commands[i]->ApplySlice(&magns[i], &phases[i]);
        }
#endif
        
        // Write modified data
        mTracks[mTrackNum]->WriteSpectroDataSlice(magns, phases,
                                                  x0, x1,
                                                  COMMAND_FADE_NUM_SAMPLES);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (commands[i] != NULL)
            RefreshData(commands[i]);
    }
    
    // Re-apply the selection (usefull if editing while currently playing
    BL_FLOAT selection[4] = { x0, y0, x1, y1 };
    mTracks[mTrackNum]->DataToViewRef(&selection[0], &selection[1],
                                      &selection[2], &selection[3]);
    
    SetDataSelection(selection[0], selection[1],
                     selection[2], selection[3]);

    // Re-capture the playbar inside selection
    if (PlayBarOutsideSelection())
    {
        ResetPlayBar();
    }
    
    // Add the two commands to the history
    mTracks[mTrackNum]->mCommandHistory->AddCommand(commands[0]);
    
    if (commands[1] != NULL)
        mTracks[mTrackNum]->mCommandHistory->AddCommand(commands[1]);
    
    // Forget the last copy paste command if any
    mCurrentCopyPasteCommands[0] = NULL;
    mCurrentCopyPasteCommands[1] = NULL;

    mTracks[mTrackNum]->SetFileModified(true);
    UpdateWindowTitle();

    mTracks[mTrackNum]->CommandDone();
}

void
Ghost::UndoLastCommand()
{
    if (mTrackNum >= mTracks.size())
        return;
    
    if (mTracks[mTrackNum]->mCommandHistory == NULL)
        return;
        
    int numChannels = mTracks[mTrackNum]->GetNumChannels();
    
    GhostCommand *lastCommand = NULL;
    
    // Assumme the selection is the same for the two channels/commands
    BL_FLOAT x0;
    BL_FLOAT y0;
    BL_FLOAT x1;
    BL_FLOAT y1;
    
    for (int i = 0; i < numChannels; i++)
    {
        // Get the selection of the last command
        lastCommand = mTracks[mTrackNum]->mCommandHistory->GetLastCommand();
        if (lastCommand == NULL)
            continue;
        
        lastCommand->GetSelection(&x0, &y0, &x1, &y1, mYScale);
    }
    
    if (lastCommand == NULL)
        return;
    
    vector<WDL_TypedBuf<BL_FLOAT> > *magns = mTmpBuf15;
    vector<WDL_TypedBuf<BL_FLOAT> > *phases = mTmpBuf16;
    mTracks[mTrackNum]->ReadSpectroDataSlice(magns, phases, x0, x1);
    
    for (int i = 0; i < numChannels; i++)
    {
        // Duplicate the current copy paste command if necessary
        // before the history deletes it
        //
        // Used in the case "copy, paste, undo, paste"
        // To keep the copied buffer for further paste
        //
        if (mCurrentCopyPasteCommands[i] != NULL)
        {
            mCurrentCopyPasteCommands[i] =
                new GhostCommandCopyPaste(*mCurrentCopyPasteCommands[i]);
        }
    }
    
    // NOTE: we must reverse count if we want to undo on the correct magns and phases
    // FIX: this fixes load mono, cut, undo, load stereo, cut, undo
    // (that was not undone)
    for (int i = numChannels - 1; i >= 0; i--)
    {
        // Commands are stored in the history by pairs (2 channels)
        // So, use the loop to remove a pair
        mTracks[mTrackNum]->mCommandHistory->UndoLastCommand(&magns[i], &phases[i]);
    }

    // Do not fade for undo!
    // So the continuity stays perfect!
    mTracks[mTrackNum]->WriteSpectroDataSlice(magns, phases, x0, x1);
    
    BL_FLOAT prevSelection[4] = { x0, y0, x1, y1 };
    // Set the selection corresponding to the last command
    mTracks[mTrackNum]->DataToViewRef(&prevSelection[0], &prevSelection[1],
                                      &prevSelection[2], &prevSelection[3]);
        
    SetDataSelection(prevSelection[0], prevSelection[1],
                     prevSelection[2], prevSelection[3]);

    mTracks[mTrackNum]->SetSelectionActive(true);

    // Re-capture the playbar inside selection
    if (PlayBarOutsideSelection())
    {
        ResetPlayBar();
    }
    
    if (lastCommand != NULL)
    {
        RefreshData(lastCommand);
    
        delete lastCommand;
    
#if UNDO_PREFER_DISPLAY_BAR
        if (prevBarActive)
        {
            SetBarPos(prevBarPos);
            SetBarActive(true);
        
            SetPlayBarPos(prevPlayBarPos, prevLineCount);
        }
#endif
    }

    mTracks[mTrackNum]->SetFileModified(true);
    
    UpdateWindowTitle();

    mTracks[mTrackNum]->CommandDone();
}

// Refresh all data
void
Ghost::RefreshData()
{
    if (mTrackNum < mTracks.size())
        mTracks[mTrackNum]->RefreshData();
}

void
Ghost::RefreshData(GhostCommand *command)
{
    BL_FLOAT commandSelection[4];
    if (command != NULL)
    {
        command->GetSelection(&commandSelection[0], &commandSelection[1],
                              &commandSelection[2], &commandSelection[3],
                              mYScale);
    }
    else
    {
        commandSelection[0] = 0.0;
        commandSelection[1] = 0.0;
        commandSelection[2] = 1.0;
        commandSelection[3] = 1.0;
    }
    
    BL_FLOAT sampleRate = GetSampleRate();
    mTracks[mTrackNum]->ResetSpectrogram(sampleRate);
    
    // Set samples pointers to spectro edit objs
    UpdateSpectroEditSamples();

    mTracks[mTrackNum]->RefreshSpectrogramView();
    
    mTracks[mTrackNum]->DoRecomputeData();
}

void
Ghost::SetDataSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1)
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetDataSelection(x0, y0, x1, y1);
}

// Unused
void
Ghost::SelectionChanged() {}

// Unused
void
Ghost::BeforeSelTranslation() {}

// Uunsed
void
Ghost::AfterSelTranslation() {}

bool
Ghost::IsBarActive()
{
    if (mTrackNum >= mTracks.size())
        return false;

    bool res = mTracks[mTrackNum]->IsBarActive();

    return res;
}

void
Ghost::SetBarActive(bool flag)
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetBarActive(flag);
}

void
Ghost::SetZoomCenter(int x)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->SetZoomCenter(x);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetZoomCenter(x);
}

BL_FLOAT
Ghost::GetNormDataBarPos()
{
    if (mTrackNum >= mTracks.size())
        return 0.0;

    BL_FLOAT res = mTracks[mTrackNum]->GetNormDataBarPos();

    return res;
}

void
Ghost::SetDataBarPos(BL_FLOAT barPos)
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetDataBarPos(barPos);
}

void
Ghost::GetDataSelection(BL_FLOAT dataSelection[4])
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->GetDataSelection(dataSelection);
}

void
Ghost::SetDataSelection(const BL_FLOAT dataSelection[4])
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->SetDataSelection(dataSelection);
}

enum Ghost::PlugMode
Ghost::GetMode()
{
    return mPlugMode;
}

void
Ghost::UpdateZoomAdjust(bool updateBGZoom)
{
#ifdef APP_API
#if DEV_SPECIAL_VERSION
    if (mLinkTracks)
    {
        for (int i = 0; i < mTracks.size(); i++)
        {
            mTracks[i]->UpdateZoomAdjust(updateBGZoom);
        }

        return;
    }
#endif
#endif
    
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->UpdateZoomAdjust(updateBGZoom);
}

void
Ghost::SetPlayStopParameter(int value)
{
    SetParameterValue(kPlayStop, value);
    
    if (mPlayButtonControl != NULL)
    {
        mPlayButtonControl->SetValue(value);
        mPlayButtonControl->SetDirty(false);
    }

    // For VST3, to reset the param on the host side
    if (value == 0)
        GUIHelper12::ResetParameter(this, kPlayStop, true);
}

void
Ghost::PostResizeGUI()
{
    for (int i = 0; i < mTracks.size(); i++)
    {
        GhostTrack *track = mTracks[i];

        track->PostResizeGUI();
    }
}

void
Ghost::ApplyGrayOutControls()
{
    switch(mPlugMode)
    {
#ifndef APP_API
        case VIEW:
        {
            if (mMonitorControl != NULL)
                mMonitorControl->SetDisabled(false);

            GrayOutEditSection(true);
        }
        break;
#endif

#ifndef APP_API
        case ACQUIRE:
        {
            if (mMonitorControl != NULL)
                mMonitorControl->SetDisabled(true);

            GrayOutEditSection(true);
        }
        break;
#endif
      
#ifndef APP_API
        case EDIT:
        {
            if (mMonitorControl != NULL)
                mMonitorControl->SetDisabled(true);
      
            GrayOutEditSection(false);
        }
        break;
#endif

#ifndef APP_API
        case RENDER:
        {
            if (mMonitorControl != NULL)
                mMonitorControl->SetDisabled(true);

            GrayOutEditSection(true);
        }
        break;
#endif
            
        default:
            break;
    }
}

void
Ghost::GrayOutEditSection(bool flag)
{
    if (mSelectionTypeControl != NULL)
        mSelectionTypeControl->SetDisabled(flag);
    
    if (mEditGainControl != NULL)
    {
        //mEditGainControl->SetDisabled(flag);
        
        // Also disable knob value automatically
        IGraphics *graphics = GetUI();
        if (graphics != NULL)
            graphics->DisableControl(kEditGainValue, flag);
    }
    
    if (mCopyButton != NULL)
        mCopyButton->SetDisabled(flag);
    if (mPasteButton != NULL)
        mPasteButton->SetDisabled(flag);
    if (mCutButton != NULL)
        mCutButton->SetDisabled(flag);
    if (mGainButton != NULL)
        mGainButton->SetDisabled(flag);
    if (mInpaintButton != NULL)
        mInpaintButton->SetDisabled(flag);
    if (mUndoButton != NULL)
        mUndoButton->SetDisabled(flag);
    
    if (mInpaintDirControl != NULL)
        mInpaintDirControl->SetDisabled(flag);
    if (mPlayButtonControl != NULL)
        mPlayButtonControl->SetDisabled(flag);
}

void
Ghost::ResetAllData()
{
    if (mTrackNum >= mTracks.size())
        return;

    mTracks[mTrackNum]->ResetAllData();

    OnReset();
}

void
Ghost::OnDrop(const char *filenames)
{
#if APP_API
    bool canOpen = CheckMaxNumFiles();
    if (!canOpen)
        return;
    
    OpenFile(filenames);
#endif
}

bool
Ghost::OnKey(const IKeyPress &key, bool isUp)
{
#ifndef APP_API
    // Non-standalone version
    // => Disable keyboard shortcuts on plugin versions !
    
    return false;
#endif
    
    if (isUp)
        return false;
    
#if 0
    fprintf(stderr, "key: %d\n", key.VK);
#endif
    
    if (key.VK == 32)
        // Spacebar
    {
        TogglePlayStop();
        
        return true;
    }
    
    if (key.VK == 13)
        // Return
    {
        if ((mTrackNum < mTracks.size()) &&
            mTracks[mTrackNum]->IsMouseOverGraph())
        {
            RewindView();
        }
    }
    
    if ((key.VK == 88) && key.C) // cmd-x
    {
        ENTER_PARAMS_MUTEX;
        
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
        // Full version
        // Additional copy command, for cut then paste
        DoCutCopyCommand();
#else
        // Lite version
        DoCutCommand();
#endif

        LEAVE_PARAMS_MUTEX;
    }
    
    if ((key.VK == 66) && key.C) // cmd-b
    {
        ENTER_PARAMS_MUTEX;
        
        DoGainCommand();

        LEAVE_PARAMS_MUTEX;
    }
    
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE
    // cmd-w was not transmitted here
    if ((key.VK == 78) && key.C) // cmd-n
    {
        ENTER_PARAMS_MUTEX;
        
        //DoReplaceCommand();
        DoReplaceCopyCommand();

        LEAVE_PARAMS_MUTEX;
    }
#endif
    
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
    if ((key.VK == 67) && key.C) // cmd-c
    {
        ENTER_PARAMS_MUTEX;
        
        DoCopyCommand();

        LEAVE_PARAMS_MUTEX;
    }
    
    if ((key.VK == 86) && key.C) // cmd-v
    {
        ENTER_PARAMS_MUTEX;
        
        DoPasteCommand();

        LEAVE_PARAMS_MUTEX;
    }
#endif
    
    if ((key.VK == 90) && key.C) // cmd-z
    {
        ENTER_PARAMS_MUTEX;
        
        UndoLastCommand();

        LEAVE_PARAMS_MUTEX;
    }

    // File menu
    //
    if ((key.VK == 79) && key.C) // cmd-o
    {
        ENTER_PARAMS_MUTEX;
        
        PromptForFileOpen();

        LEAVE_PARAMS_MUTEX; 
    }

    if ((key.VK == 83) && key.C) // cmd-s
    {
        ENTER_PARAMS_MUTEX;

        if (mTrackNum < mTracks.size())
        {
            bool success = mTracks[mTrackNum]->SaveCurrentFile();
            if (success)
                UpdateWindowTitle();
        }
        
        LEAVE_PARAMS_MUTEX;
    }

    if ((key.VK == 83) && key.C && key.S) // shift-cmd-s
    {
        PromptForFileSaveAs();
    }

    if ((key.VK == 69) && key.C) // cmd-s
    {
        ENTER_PARAMS_MUTEX;
        
        PromptForFileExportSelection();

        LEAVE_PARAMS_MUTEX;
    }
    
    return true;
}

void
Ghost::OnIdle()
{
    CheckRecomputeData();

    if ((mPlugMode == Ghost::VIEW) ||
        (mPlugMode == Ghost::ACQUIRE))
    {
        if (mIsPlaying)
        {
            // When scrolling SpectroMeter time values must change,
            // if we keep the cursor on the graph
            CursorMoved(mPrevMouseX, mPrevMouseY);
        }
    }

    // Do this in the GUI thread
    // FIX: plug, in release, when changing mode, sometimes controls were not
    // refreshed correctly until we move the mouse
    if (mNeedGrayOutControls)
    {
        ApplyGrayOutControls();
        mNeedGrayOutControls = false;
    }
}

#ifdef APP_API
bool
Ghost::OnHostRequestingProductHelp()
{
    if (mGUIHelper == NULL)
        return true;

    IGraphics *graphics = GetUI();
    if (graphics == NULL)
        return true;
    
    mGUIHelper->ShowHelp(this, graphics, MANUAL_FN);
    
    return true;
}
#endif

#ifdef APP_API
// Must process action later, not just while beeing called from the menu
// FIX: otherwise, the menu stayed displayed over the zenity file selector window
// So when called from the menu, we set a flag. And later, in ProcessBlock(),
// we open file selector if needed
void
Ghost::OnHostRequestingMenuAction(int actionId)
{        
    switch(actionId)
    {
        // File menu
        case ID_OPEN:
        {
            // Here we this in the right thread!
            bool canOpen = CheckMaxNumFiles();
            if (!canOpen)
                return;

            mCurrentAction = ACTION_FILE_OPEN;
        }
        break;

        case ID_SAVE:
            mCurrentAction = ACTION_FILE_SAVE;
            break;

        case ID_SAVE_AS:
            mCurrentAction = ACTION_FILE_SAVE_AS;
            break;

        case ID_EXPORT_SELECTION:
            mCurrentAction = ACTION_FILE_EXPORT_SELECTION;
            break;

        case ID_RELOAD:
            mCurrentAction = ACTION_FILE_RELOAD;
            break;

        case ID_CLOSE:
            mCurrentAction = ACTION_FILE_CLOSE;
            break;
            
        // Edit menu
        case ID_UNDO:
            mCurrentAction = ACTION_EDIT_UNDO;
            break;

        case ID_CUT:
            mCurrentAction = ACTION_EDIT_CUT;
            break;

        case ID_COPY:
            mCurrentAction = ACTION_EDIT_COPY;
            break;

        case ID_PASTE:
            mCurrentAction = ACTION_EDIT_PASTE;
            break;

        case ID_GAIN:
            mCurrentAction = ACTION_EDIT_GAIN;
            break;

        case ID_INPAINT:
            mCurrentAction = ACTION_EDIT_INPAINT;
            break;

        case ID_RESET_PREFERENCES:
            mCurrentAction = ACTION_RESET_PREFERENCES;
            break;
            
        // Transport menu
        case ID_PLAY_STOP:
            mCurrentAction = ACTION_TRANSPORT_PLAY_STOP;
            break;

        case ID_TRANSPORT_RESET:
            mCurrentAction = ACTION_TRANSPORT_RESET;
            break;
            
        default:
            break;
    }

#if (defined WIN32) || (defined __APPLE__)
    // On Windows, process the menu action directly!
    // Otherwise it will freeze the app
    ProcessCurrentMenuAction();
#endif
}
#endif

// Make sure the loaded parameters from preferences
// will be applied to all tracks
void
Ghost::OnParamReset(EParamSource source)
{
    IEditorDelegate::OnParamReset(source);

    SetParametersToAllTracks(false);
}

void
Ghost::OnTabSelected(int tabNum)
{
    ActivateTrack(tabNum);
}

void
Ghost::OnTabClose(int tabNum)
{
    ENTER_PARAMS_MUTEX;
    
    CloseFile(tabNum);

    GhostTrack *track = mTracks[tabNum];
    delete track;
    
    mTracks.erase(mTracks.begin() + tabNum);

    if (mTrackNum > mTracks.size() - 1)
        // We delete the last one
        mTrackNum = mTracks.size() - 1;
    else if (tabNum < mTrackNum)
        // We deleted a tab before the current tab => must shift
    {
        mTrackNum--;
        if (mTrackNum < 0)
            mTrackNum = 0;
    }

    ActivateTrack(mTrackNum);
        
    LEAVE_PARAMS_MUTEX;
}

void
Ghost::PromptForFileOpen()
{
    // Result
    WDL_String fileName;

    bool fileOk =
        BLUtilsFile::PromptForFileOpenAudio(this, mCurrentLoadPath, &fileName);
        
    if (fileOk)
        OpenFile(fileName.Get());
}

void
Ghost::PromptForFileSaveAs()
{
    // Result
    WDL_String resFileName;

    const char *currentFileName = NULL;
    if (mTrackNum < mTracks.size())
        currentFileName = mTracks[mTrackNum]->GetCurrentFileName();

    bool fileOk = BLUtilsFile::PromptForFileSaveAsAudio(this,
                                                        mCurrentLoadPath,
                                                        mCurrentSavePath,
                                                        currentFileName,
                                                        &resFileName);
        
    if (fileOk)
        SaveFile(resFileName.Get());
}

void
Ghost::PromptForFileExportSelection()
{
    WDL_String resFileName;
            
    bool fileOk = BLUtilsFile::PromptForFileSaveAsAudio(this,
                                                        mCurrentLoadPath,
                                                        mCurrentSavePath,
                                                        NULL,
                                                        &resFileName);
    
    if (fileOk)
        ExportSelection(resFileName.Get());
}

void
Ghost::ProcessCurrentMenuAction()
{
    switch(mCurrentAction)
    {
        // File menu
        case ACTION_FILE_OPEN:
            PromptForFileOpen();
            break;

        case ACTION_FILE_SAVE:
        {
            if (mTrackNum < mTracks.size())
            {
                bool success = mTracks[mTrackNum]->SaveCurrentFile();
                if (success)
                    UpdateWindowTitle();
            }
        }
        break;

        case ACTION_FILE_SAVE_AS:
            PromptForFileSaveAs();
            break;

        case ACTION_FILE_EXPORT_SELECTION:
            PromptForFileExportSelection();
            break;

        case ACTION_FILE_RELOAD:
        {    
            if (mTrackNum < mTracks.size())
            {
                bool success = mTracks[mTrackNum]->OpenCurrentFile();

                if (success)
                {
                    UpdateWindowTitle();
                    
                    SetPlayStopParameter(0);
                }
            }
        }
        break;

        case ACTION_FILE_CLOSE:
        {    
            if (mTrackNum < mTracks.size())
            {
                // Close the file and remove the current tab
                if (mTabsBar != NULL)
                    mTabsBar->CloseTab(mTrackNum);
            }
        }
        break;
        
         // Edit menu
        case ACTION_EDIT_UNDO:
            UndoLastCommand();
            break;

        case ACTION_EDIT_CUT:
        {
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
            // Full version
            // Additional copy command, for cut then paste
            DoCutCopyCommand();
#else
            // Lite version
            DoCutCommand();
#endif
        }
        break;

        case ACTION_EDIT_COPY:
        {
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_COPY_PASTE
            DoCopyCommand();
#endif
        }
        break;

        case ACTION_EDIT_PASTE:
            DoPasteCommand();
            break;

        case ACTION_EDIT_GAIN:
            DoGainCommand();
            break;

        case ACTION_EDIT_INPAINT:
        {
#if !GHOST_LITE_VERSION || GHOST_LITE_ENABLE_REPLACE
            //DoReplaceCommand();
            DoReplaceCopyCommand();
#endif
        }
        break;

        case ACTION_RESET_PREFERENCES:
            ResetAppPreferences();
            break;
            
        // Transport
        case ACTION_TRANSPORT_PLAY_STOP:
            TogglePlayStop();
            break;

        case ACTION_TRANSPORT_RESET:
            RewindView();
            break;
            
        default:
            break;
    }

    mCurrentAction = ACTION_NO_ACTION;
}

void
Ghost::ActivateTrack(int trackNum)
{
    if (trackNum >= mTracks.size())
        return;

    int prevTrackNum = mTrackNum;
    mTrackNum = trackNum;

    for (int i = 0; i < mTracks.size(); i++)
    {
        bool enabled = (i == mTrackNum);

        if (mTracks[i] != NULL)
            mTracks[i]->SetGraphEnabled(enabled);
    }

    if (mTrackNum != prevTrackNum)
    {
        SetParametersFromTrack(mTracks[mTrackNum]);

        if (mLinkTracks)
            GhostTrack::CopyTrackSelection(*mTracks[prevTrackNum],
                                           mTracks[mTrackNum]); 
    }
    
    UpdateWindowTitle();
}

void
Ghost::CheckAppStartupArgs()
{
#if APP_API
    if (mAppStartupArgsChecked)
        return;
    
    int argc = 0;
    char **argv = NULL;
    GetStartupArgs(&argc, &argv);

    //char message[1024];
    //sprintf(message, "argc: %d\n", message);
    //BLDebug::AppendMessage("debug.txt", message);
    
    if (argc > 1)
    {
        const char *fileName = argv[1];
        
        //char message[1024];
        //sprintf(message, "filename: %s\n", fileName);
        //BLDebug::AppendMessage("debug.txt", message);

        // NOTE: do not check for max num files here.
        
        OpenFile(fileName);
    }

    mAppStartupArgsChecked = true;
#endif
}

void
Ghost::NewTab(const char *fileName)
{
#ifdef APP_API
    if (mTabsBar == NULL)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    GhostTrack *track = new GhostTrack(this, BUFFER_SIZE, sampleRate, mYScale);
    mTracks.push_back(track);
    mTrackNum = mTracks.size() - 1;

    if (mGraphics != NULL)
    {
        track->CreateControls(mGUIHelper, this, mGraphics,
                              kGraphX, kGraphY,
                              mGUIOffsetX, mGUIOffsetY /*kGraph*/);
    }
    
    SetParametersToTrack(track, false);

    mTabsBar->NewTab(fileName);
#endif
}

// Generate the full tabs bar from existing tracks
void
Ghost::GenerateTabsBar()
{
    for (int i = 0; i < mTracks.size(); i++)
    {
        const char *fileName = mTracks[i]->GetCurrentFileName();
        
        mTabsBar->NewTab(fileName);
    }

    // Re-select the previously selected tab
    // (otherwise it would be the last tab selected automatically each time)
    mTabsBar->SelectTab(mTrackNum);
}

#if APP_API
bool
Ghost::CheckMaxNumFiles()
{
    if (mTracks.size() >= MAX_NUM_TRACKS)
    {
        IPlugAPP::ShowMessageBox("Max number of open files reached");
        
        return false;
    }
    
    return true;
}
#endif

void
Ghost::SetLowFreqZoom(bool flag)
{
    // Global
    for (int i = 0; i < mTracks.size(); i++)
    {
        mTracks[i]->SetLowFreqZoom(flag);
    }
    
    // Update the scale type
    // (will be use for example for edition)
    if (!mTracks.empty())
        mYScale = mTracks[0]->GetYScaleType();
}

void
Ghost::SetAllControlsToNull()
{
    // NEW
    mOpenFileSelector = NULL;
    mSaveFileSelector = NULL;
    mSaveAsFileSelector = NULL;
    mExportSelectionFileSelector = NULL;

    mGUISizeSmallButton = NULL;
    mGUISizeMediumButton = NULL;
    mGUISizeBigButton = NULL;
    mGUISizeHugeButton = NULL;

    mMonitorControl = NULL;
    
    mSelectionTypeControl = NULL;
    mEditGainControl = NULL;
    
    mCopyButton = NULL;
    mPasteButton = NULL;
    mCutButton = NULL;
    mGainButton = NULL;
    mInpaintButton = NULL;
    mUndoButton = NULL;

    mInpaintDirControl = NULL;
    mPlayButtonControl = NULL;

    mLowfreqZoomCheckbox = NULL;

    mTabsBar = NULL;
}

void
Ghost::UpdatePrevMouse(float newGUIWidth, float newGUIHeight)
{
    if (/*mPlug->*/GetUI() != NULL)
    {
        if ((mPrevMouseX >= 0) && (mPrevMouseY >= 0))
        {
            float currentGUIWidth = /*mPlug->*/GetUI()->Width();
            float currentGUIHeight = /*mPlug->*/GetUI()->Height();
            
            mPrevMouseX /= currentGUIWidth;
            mPrevMouseX *= newGUIWidth;
            
            mPrevMouseY /= currentGUIHeight;
            mPrevMouseY *= newGUIHeight;
        }
    }
}
