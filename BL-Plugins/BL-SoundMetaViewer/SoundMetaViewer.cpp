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

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <SMVProcess4.h>
#include <SMVVolRender3.h>

#include <StereoWidenProcess.h>

#include <ParamSmoother2.h>

#include "IControl.h"
#include "config.h"
#include "bl_config.h"

#include "SoundMetaViewer.h"

#include "IPlug_include_in_plug_src.h"

// Apply stereo width on input samples (better sound)
#define STEREO_WIDEN_SAMPLES 1

#define EPS 1e-15

#define USE_DEBUG_GRAPH 0
#define DEBUG_HIDE_POLAR_DISPLAY 0

// CHECK: maybe it could work well with 1024 buffer ?
#define BUFFER_SIZE 2048

// With over 4, Stereoize makes wobbles
#define OVERSAMPLING_NORMAL 4 // 32 //4 //1 //4 => set to 4 since re-synthetize selection

// 32 gives good results, but is very resource consuming
// 16 has makes some very little artifacts in low frequencies
#define OVERSAMPLING_STEREOIZE 16 //32

#define FREQ_RES 1

#define KEEP_SYNTHESIS_ENERGY 0

// When set to 0, we are accurate for frequencies
#define VARIABLE_HANNING 0

// NOTE: good with or without
// Display angle on x, and distance on y
// => flatten the data, so it looks contained in a cube
#define USE_FLAT_POLAR 0 //1

#define BYPASS_FFT_PROCESS 0

#define DEBUG_CONTROLS 1

// FIX: Cubase 10, Mac: at startup, the input buffers can be empty,
// just at startup
#define FIX_CUBASE_STARTUP_CRASH 1

#define HIDE_UNUSED_CONTROLS 1

#define HIDE_UNUSED_CONTROLS2 1

#define HIDE_UNUSED_CONTROLS3 1

// Use a key catcher to handle Cmd release outside of the plugin window
// (The plugin must still have the focus to make it work)
#define USE_KEY_CATCHER 1

// 12 fps: 60% CPU
// 25 fps: 90% CPU
//#define FPS 25.0 //12.0

// Point size knob is not used anymore...
#define HIDE_POINT_SIZE_KNOB 1

// Show additional controls for debugging rendering
// Show rendring algo choice, and rendering algo param
#define DEBUG_SHOW_ALGO_RENDER_CONTROLS 0 //1

// Hide quality T knob (it is now automatic)
#define HIDE_QUALITY_T_KNOB 1

#if 0
- TODO: monitor button

IDEA: try to use the optimized cube renderer (stb_voxel_render.h)
here: https://github.com/nothings/stb

IDEA: later, with Iplug2, find an API that can render real 3D, and render the points using real 3D (raytracer/pyramid tracer).

TODO: try to optimize with something like
GraphControl10::SetCurveOptimSameColor(true) (like in UST) (OPTIM_QUADS_SAME_COLOR, nvgQuadS)

TODO: display phases derivative (instead of only phases)
=> this was done line that in the first Spectogram plugin, and
was very good

Make a blog article on the advance of the devs:
- Project the data on each side of the cube (summation on the innder data)
=> so we could have a high res real panogram, a high res spectrogram...
- make a test: y: frequencies, x: chroma features
=> This shoudl lead to good patterns and clues on the frequencies spread
- A spectrogram, seen from front (time on z), is a spectrum viewer !
- A panogram, seen from front, is a stereo width viewer !

"Some experimentations are in progress in the Lab! :)"

DEMO: SMVProcessXComputerLissajousEXP (without total psyche)
=> we see the waveform if we look at the side !

DEMO: "have you ever seen Lissajous in 3D => here it is !"

DEMO: make many examples with sound problems, and how to show the problem
- problem of phases in a sound

DEMO: "BlueLab R&D => KVR video !"
DEMO: warn mono2stereo

DEMO: music style electro (like C64 demo...)

DEMO: mono sine inside a stero white noise. Block of white noise. Thin cylindre of sine.
Listen to the sine only by selection, then the white noise only. Flat vetorscope.
Pan smoothly the sine left and right. Change the sine gain smoothly.
Change the frequency (for that, switch Y from amp to freq).

DEMO: absolutly make a youtube video (for visibility) in addition to the blog article

- SALE: "de-mix" = extract an instrument that is panned
- SALE: "stereo width": enlarge or narrow stereo field (with visual feedback)
- SALE: "inspect the sound": sound probe in freq or stereo field + "view" the sound

NOTE: with mode SPECTRO2, managed to exteract a guitar on the right ear (on "Midnight")
(warn the coeff 1000, with 500 not mamanged)

IDEA: "Re-Pan": isolate instrument in the stereo field, from spectrogram and stereo location (implement intellingent selection over time / tracking)

IDEA: ask gearslutz if I can post the article in "new products" (even if it is a WIP)
- for asking feedback of the users (and show this as a new thing)

DEMO: video: make a "flux nebula" view (increase the quality) => and then turn the view to see the spectrogram !!

======
OPTIM notes (Apr 2020):
----
prev we were near 100%
--
now we are near 85% (88%)
with first point lipo, we are near 76%(77%) !
with rgba[4] and int cast, we are at 75%
optim adaptive texture size: 70%
--
optim pixels (i, j) + unions => 68% (69%)
--
optim render front to back: 65% (was 77% without optimizations like early...)
--
optim pre compute point color: 63% (not sure...) (rather maybe 65%)
--
optim pre sort (well integrated): 60%
--
optim pre sort middle (wip): 58%/60%
--
//new simple presort: 64% (perf loss since last measurement...)
//premult alpha: 61%
new simple presort: 60%
premult alpha: 56%/57%
--
optim compute in sound thread + struct align + big refactor: 53%/55% (even sometimes 52/53%)
--
re-bench: 50% (49%->51%) (re-bench again: 53%)
--
utils AVX (+ some other optims): 54% (AVX gain: ~4%)
without AVX: ~54% too
======

TODO: mode to compute color over the time !

IDEA: try to threshold very low weights automatically (if possible) (just at insertion)

IDEA(later): make rendering all in OpenGL

OPTIM other ideas
- IDEA: try to insert in the grid without multiplication (just comparisons)
(for the moment, we multipy each point by a factor to find the grid cell)
- IDEA: multithreading ? (e.g std::thread)

IDEA: shade voxels using an illumination technique (and gradients)

TODO: "monitor option", to display in real-time while monitoring
-> check if we gan get "monitor" state from host !!! (an if so, do it for other plugins)

TODO: reduce a bit the knobs size (by setting the titles in small)
Then reduce the interface size
Then add gui resize buttons

BUG: sometimes selection looks to become buggy
TODO: check all axes values (check that they are not shifted, of false)



BUG: When changing the speed, the sound goes faster/slower
(since FIX_PLAY_THRESHOLDED, centerSlice...).
BUG: (to be confirmed): when selecting before playing anything,
then play, the sound is not good at the beginning

IDEA: take 1 single channel, display spectrogram, + phases (possibly unwrapped) on x axis

TODO: add 4th button for gui size retina

TODO: optional reverse mode: scroll from front to back
TODO: button to reverse the scroll of the slices (for newest slices in front)

IDEA: If we freeze the time, we can have an animated volume in place !

BUG: depending on the selection width along the z axis, there could be sound wobbles (need multiple of 4 slices ?)
TEST: test with a sine wave, and pan

IDEA: plug name: SOUNDSCAPER

TODO: remove garbage data on the line on the extreme left
TODO: in some modes, when volume is not possible to compute in mono, display nothing

TODO: implement key press for cmd or other modifiers on windows

TODO: retry the "fuzzy" mode (smooth like smoke), with actual correct spectrogram implementation

BUG: crash in Windows 7 when scanning with Reaper

TODO: un-transform the points from 1 spatial to left + right: to play more hardly left/right selection

TODO:
- make a progressive refinement (button "auto")
- change the name: call it "INSPECT" => "SOUNDSCAPER"
- SALE: sale it as a viewer, an "inspector"
- SALE: make good examples and videos
- SALE: "raycasting", a technologie widely used in old video games

  
TODO: make a shader (maybe style raytracer), to render all the points (search articles "advanced point rendering, efficient point splalting rendering")

NOTE: Left/right + selection: doesn't make a neat left/right separation
NOTE: spectrogram mode ok, but:

- performances must be improved !!
    IDEA:   - keep all the fft values, and re-transform and decimate on the fly
            - make a decimate slice with 2 parameters for width and height, not only one for "size"
            - when in "side view", lower the "decimate sample" algo to work on few bands horizontally
              (at the maximum, 1 band of 1024 height)
            - when in front view, lower the time steps, and adjust alpha
            - and for the intermediate, use intermediate settings

TODO: try ortho projection: to have a good spectrogram alignment (and maybe better other alignements)

NOTE: a un moment, avec speed a 0, dans une precendente version, le son etait bon, sans glitch
(sur la gauche, des sons de batterie equalises)
puis en essayant de l'integrer (sans avaoir a regeler display speed a zero,
le bon son a displaru.
- flat polar ?
- oversampling 32 ?
=> c était en skippant 3 buffers sur 4 en overlapping 4

TODO: slice  rendering: delete (or re-use) the previous textures (for the moment: memory leak)
TODO: slice rendering: may have a bug (the sound image is moving more than before)
TODO: slice rendering: bug of filling quad => with certain angles, there is a "wrap"
TODO: slice rendering: on extreme L & R angles, we see slices too much
TODO: manage the speed depending on the T resolution (to have constant speed when we move T quality)
TODO: slice rendering: bench

TODO: texture slice rendering: adjust (center of rotation, display quality and artifacts)
- essayer en interp linear

IDEA: optimize: do not multiply matrix in soft but in GPU (pass the matrix to the nanovg shader)
IDEA: apply colormap in GPU (vertex shader) (would not optimize a lot AddSlice())

IDEA: test with the derivatives of the values (over time ?)
=> maybe more consistent display ?

NOTE: overlapping at 1 => seems correct (for viewing only)
NOTE: spatial smoothing (128) => seems correct (for viewing)

IDEA: display the shadows of the points on the floor (to better handle space visually)
#endif


enum EParams
{
    kGraph = 0,
    kDisplayMode,
    kColormap,
    kInvertColormap,
    kAngularMode,
    kQualityXY,
    kQualityT,
    kSpeed,
    kRange,
    kContrast,
    kPointSize,
    kAlphaCoeff,
    
    kQuality,
    kThreshold,
    kThresholdCenter,
    
    kStereoWidth,
    kClip,
    
    // Modes
    kModeX,
    kModeY,
    kModeColor,
    
    //
    kSpeedT,
    
    kPlayStop,
    kFreeze,
    
    kRenderAlgo,
    kRenderAlgoParam,
    
    kAutoQuality,
    
    //
    kPan,
    
    //
    kInGain,
    kOutGain,
    
    // Camera angles
    kAngle0,
    kAngle1,
    kCamFov,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,
    
    kGraphX = 0,
    kGraphY = 0,
    
    kSpeedX = 285,
    kSpeedY = 447,
    kSpeedFrames = 180,
    
    kQualityXYX = 355,
    kQualityXYY = 447,
    kQualityXYFrames = 180,
  
    kQualityTX = 188,
    kQualityTY = 608,
    kQualityTFrames = 180,
    
    kInvertColormapX = 244,
    kInvertColormapY = 454,
    
    kAngularModeX = 530,
    kAngularModeY = 454,
    
    kClipX = 398,
    kClipY = 608,
    
    kRadioButtonsDisplayModeX = 90,
    kRadioButtonsDisplayModeY = 454,
    kRadioButtonDisplayModeVSize = 100,
    kRadioButtonDisplayModeNumButtons = 5,
    
    kRadioButtonsColormapX = 60,
    kRadioButtonsColormapY = 460,
    kRadioButtonColormapVSize = 112,
    kRadioButtonColormapNumButtons = 7,
  
    kStereoWidthX = 834,
    kStereoWidthY = 460,
    kStereoWidthFrames = 180,
    
#if DEBUG_CONTROLS
    kRangeX = 188,
    kRangeY = 460,
    kRangeFrames = 180,
    
    kContrastX = 288,
    kContrastY = 460,
    kContrastFrames = 180,
    
    // Point size not used anymore
    kPointSizeX = 188,
    kPointSizeY = 608,
    kPointSizeFrames = 180,
    
    // Modifed with new behavior
    kAlphaCoeffX = 388,
    kAlphaCoeffY = 460,
    kAlphaCoeffFrames = 180,
    
    kQualityX = 188,
    kQualityY = 608,
    kQualityFrames = 180,
    
    kRenderAlgoParamX = 74,
    kRenderAlgoParamY = 608,
    kRenderAlgoParamFrames = 180,
    
    kRadioButtonsRenderAlgoX = 30,
    kRadioButtonsRenderAlgoY = 590,
    kRadioButtonRenderAlgoVSize = 100,
    kRadioButtonRenderAlgoNumButtons = 5,
#endif
    
    kThresholdX = 509,
    kThresholdY = 608,
    kThresholdFrames = 180,
    
    kThresholdCenterX = 615,
    kThresholdCenterY = 608,
    kThresholdCenterFrames = 180,
    
    // Modes
    kRadioButtonsModeXX = 546,
    kRadioButtonsModeXY = 460,
    kRadioButtonModeXVSize = 96,
    kRadioButtonModeXNumButtons = 6,

    kRadioButtonsModeYX = 646,
    kRadioButtonsModeYY = 460,
    kRadioButtonModeYVSize = 80,
    kRadioButtonModeYNumButtons = 5,
  
    kRadioButtonsModeColorX = 746,
    kRadioButtonsModeColorY = 460,
    kRadioButtonModeColorVSize = 112,
    kRadioButtonModeColorNumButtons = 7,
    
    kSpeedTX = 288,
    kSpeedTY = 608,
    kSpeedTFrames = 180,
    
    //
    kFreezeX = 730,
    kFreezeY = 608,
    
    kPlayStopX = 694,
    kPlayStopY = 650,
    //kPlayStopFrames = 3,
    
    kAutoQualityX = 97,
    kAutoQualityY = 608,
    
    kPanX = 834,
    kPanY = 608,
    kPanFrames = 180,
    
    // In gain
    kInGainX = 934,
    kInGainY = 460,
    kInGainFrames = 180,
    
    // Out gain
    kOutGainX = 934,
    kOutGainY = 608,
    kOutGainFrames = 180,
    
    kLogoAnimFrames = 31
};


//
SoundMetaViewer::SoundMetaViewer(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    TRACE;
    
    InitNull();
    InitParams();

    Init(OVERSAMPLING_NORMAL, FREQ_RES);
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

SoundMetaViewer::~SoundMetaViewer()
{
    if (mFftObj != NULL)
        delete mFftObj;
  
    if (mSMVProcess != NULL)
        delete mSMVProcess;

    if (mVolRender != NULL)
        delete mVolRender;

    if (mPanSmoother != NULL)
        delete mPanSmoother;

    if (mInGainSmoother != NULL)
        delete mInGainSmoother;
    if (mOutGainSmoother != NULL)
        delete mOutGainSmoother;

    if (mStereoWidthSmoother != NULL)
        delete mStereoWidthSmoother;
    
#if USE_KEY_CATCHER
    // Detach the key catcher
    // as it was also added to the controls list
    // Avoids trying to delete it two times
    //GetGUI()->AttachKeyCatcher(NULL); // TODO ?
#endif
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
SoundMetaViewer::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
SoundMetaViewer::MyMakeLayout(IGraphics *pGraphics)
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
    
    CreateControls(pGraphics);
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
SoundMetaViewer::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();

    mFreeze = false;
    mPlayStop = false;
    
    mGraph = NULL;
    
    mFftObj = NULL;
  
    mSMVProcess = NULL;

    mVolRender = NULL;

    mPanSmoother = NULL;

    mInGainSmoother = NULL;
    mOutGainSmoother = NULL;

    mStereoWidthSmoother = NULL;
    
    mQualityKnob = NULL;
    mPlayButton = NULL;
    
    mIsInitialized = false;
    
    mGUIHelper = NULL;
}

void
SoundMetaViewer::InitParams()
{
#if !HIDE_UNUSED_CONTROLS
    // Quality XY
    BL_FLOAT defaultQualityXY = 10.0;
    mQualityXY = defaultQualityXY;
    GetParam(kQualityXY)->InitDouble("QualityXY", defaultQualityXY, 0.0, 100.0, 0.1, "%");
#endif

#if !HIDE_QUALITY_T_KNOB
    // Quality T
    BL_FLOAT defaultQualityT = 10.0;
    mQualityT = defaultQualityT;
    GetParam(kQualityT)->InitDouble("QualityT", defaultQualityT, 0.0, 100.0, 0.1, "%");
#endif

    BL_FLOAT defaultSpeedT = 50.0;
    mSpeedT = defaultSpeedT/100.0;
    GetParam(kSpeedT)->InitDouble("SpeedT", defaultSpeedT, 0.0, 100.0, 0.1, "%");
    mPrevSpeed = -1.0;

#if !HIDE_UNUSED_CONTROLS
    // Speed
    BL_FLOAT defaultSpeed = 50.0;
    mSpeed = defaultSpeed;
    GetParam(kSpeed)->InitDouble("Speed", defaultSpeed, 0.0, 100.0, 0.1, "%");
#endif

    BL_FLOAT defaultThreshold = 0.0;
    mThreshold = defaultThreshold;
    mPrevThreshold = -1.0;
    GetParam(kThreshold)->InitDouble("Threshold", defaultThreshold, 0.0, 100.0, 0.1, "%");

    // Threshold center
    BL_FLOAT defaultThresholdCenter = 0.0;
    mThresholdCenter = defaultThresholdCenter;
    mPrevThresholdCenter = -1.0;
    GetParam(kThresholdCenter)->InitDouble("ThresholdCenter",
                                           defaultThresholdCenter, 0.0, 100.0, 0.1, "%");

#if !HIDE_UNUSED_CONTROLS2
    // Invert colormap flag
    int defaultInvertColormap = 0;
    mInvertColormap = defaultInvertColormap;
    GetParam(kInvertColormap)->InitInt("InvertColormap", defaultInvertColormap, 0, 1);

    // Angular mode
    int defaultAngularMode = 0;
    mAngularMode = defaultAngularMode;
    GetParam(kAngularMode)->InitInt("Angular", defaultAngularMode, 0, 1);
#endif

    // Clip outside selection
    int defaultClip = 0;
    mClipFlag = defaultClip;
    mPrevClipFlag = -1;
    GetParam(kClip)->InitInt("Clip", defaultClip, 0, 1);

#if !HIDE_UNUSED_CONTROLS3
    // DisplayMode
    SMVProcess3::DisplayMode defaultDisplayMode = SMVProcess3::SIMPLE;
    mDisplayMode = defaultDisplayMode;
    GetParam(kDisplayMode)->InitInt("DisplayMode", (int)defaultDisplayMode, 0, 4);
#endif

    // Colormap
    int defaultColormap = 0;
    mColormap = defaultColormap;
    mPrevColormap = -1;
    GetParam(kColormap)->InitInt("Colormap", defaultColormap, 0, 6);

    // Stereo width
    BL_FLOAT defaultStereoWidth = 0.0;
    mStereoWidth = defaultStereoWidth;
    GetParam(kStereoWidth)->InitDouble("StereoWidth", defaultStereoWidth,
                                       -100.0, 100.0, 0.01, "%");
    mPrevStereoWidth = -1.0;

#if DEBUG_CONTROLS
    // Range
    BL_FLOAT defaultRange = 0.;
    mRange = defaultRange;
    GetParam(kRange)->InitDouble("Range", defaultRange, -1.0, 1.0, 0.01, "");
    mPrevRange = -1.0;
#endif

    // Contrast
    BL_FLOAT defaultContrast = 0.5;
    mContrast = defaultContrast;
    GetParam(kContrast)->InitDouble("Contrast", defaultContrast, 0.0, 1.0, 0.01, "");
    mPrevContrast = -1.0;

#if !HIDE_POINT_SIZE_KNOB
    // Point size
    BL_FLOAT defaultPointSize = 0.5;
    GetParam(kPointSize)->InitDouble("PointSize", defaultPointSize, 0.0, 1.0/*2.0*/, 0.01, "");
#endif

#if DEBUG_SHOW_ALGO_RENDER_CONTROLS
    // Render algo
    //RayCaster::RenderAlgo defaultRenderAlgo = RayCaster::RENDER_ALGO_GRID;
    int defaultRenderAlgo = 0;
    GetParam(kRenderAlgo)->InitInt("RenderAlgo", (int)defaultRenderAlgo, 0, 4);
#endif

    // Render algo param
    BL_FLOAT defaultRenderAlgoParam = 0.0;
    mRenderAlgoParam = defaultRenderAlgoParam/100.0;
    GetParam(kRenderAlgoParam)->InitDouble("RenderAlgoParam",
                                           defaultRenderAlgoParam,
                                           0.0, 100.0/*2.0*/, 0.01, "%");

    BL_FLOAT defaultAlphaCoeff = 25.0;
    mAlphaCoeff = defaultAlphaCoeff/100.0;
    GetParam(kAlphaCoeff)->InitDouble("AlphaCoeff", defaultAlphaCoeff, 0.0, 100.0, 0.01, "");    
    mPrevAlphaCoeff = -1.0;

    // Auto quality
    int defaultAutoQuality = 1;
    mAutoQuality = defaultAutoQuality;
    GetParam(kAutoQuality)->InitInt("AutoQuality", defaultAutoQuality, 0, 1);
    mPrevAutoQuality = -1;

    // Quality (3d)
    double defaultQuality = 50.0;
    mQuality = defaultQuality;
    mPrevQuality = -1.0;
    GetParam(kQuality)->InitDouble("Quality", defaultQuality, 0.0, 100.0, 0.1, "%",
                                   0, "", IParam::ShapePowCurve(2.0));

    // Mode X
    SMVProcess4::ModeX defaultModeX = SMVProcess4::MODE_X_SCOPE;
    mModeX = defaultModeX;
    GetParam(kModeX)->InitInt("ModeX", (int)defaultModeX, 0, 5);
    mPrevModeX = -1;

    // Mode Y
    SMVProcess4::ModeY defaultModeY = SMVProcess4::MODE_Y_MAGN;
    mModeY = defaultModeY;
    GetParam(kModeY)->InitInt("ModeY", (int)defaultModeY, 0, 4);
    mPrevModeY = -1;

    // Mode color
    SMVProcess4::ModeColor defaultModeColor = SMVProcess4::MODE_COLOR_MAGN;
    mModeColor = defaultModeColor;
    GetParam(kModeColor)->InitInt("ModeColor", (int)defaultModeColor, 0, 6);
    mPrevModeCol = -1;

    mFreeze = false;
    GetParam(kFreeze)->InitInt("Freeze", 0, 0, 1, "", IParam::kFlagMeta);
    mPrevFreeze = -1;

    mPlayStop = false;
    GetParam(kPlayStop)->InitInt("PlayStop", 0, 0, 1, "", IParam::kFlagMeta);  
    mPrevPlayStop = -1;
    
    BL_FLOAT defaultPan = 0.0;
    mPan = defaultPan;    
    GetParam(kPan)->InitDouble("Pan", defaultPan, -100.0, 100.0, 0.01, "%");

    // In gain
    BL_FLOAT defaultInGain = 0.0;
    mInGain = 1.0;
    GetParam(kInGain)->InitDouble("InGain", defaultInGain, -12.0, 12.0, 0.1, "dB");

    // Out gain
    BL_FLOAT defaultOutGain = 0.0;
    mOutGain = 1.0;
    GetParam(kOutGain)->InitDouble("OutGain", defaultOutGain, -12.0, 12.0, 0.1, "dB");

#if 1 // See Wav3s
    // Do not take the risk of making a non-tested host crash...
    const char *degree = " ";
#endif
    
    // Angle0
    BL_FLOAT defaultAngle0 = DEFAULT_ANGLE_0;
    mAngle0 = defaultAngle0;
    GetParam(kAngle0)->InitDouble("Angle0", defaultAngle0,
                                  -MAX_CAMERA_ANGLE_0, MAX_CAMERA_ANGLE_0, 0.1, degree);
    
    // Angle1
    BL_FLOAT defaultAngle1 = DEFAULT_ANGLE_1;
    mAngle1 = defaultAngle1;
    GetParam(kAngle1)->InitDouble("Angle1", defaultAngle1,
                                  MIN_CAMERA_ANGLE_1, MAX_CAMERA_ANGLE_1, 0.1, degree);
    
    // CamFov
    BL_FLOAT defaultCamFov = DEFAULT_CAMERA_FOV;
    mCamFov = defaultCamFov;
    GetParam(kCamFov)->InitDouble("CamFov", defaultCamFov,
                                  MIN_CAMERA_FOV, MAX_CAMERA_FOV, 0.1, degree);
}

void
SoundMetaViewer::ApplyParams()
{
    //if (mVolRender != NULL)
    //    mVolRender->SetQualityXY(mQualityXY);

    //if (mVolRender != NULL)
    //    mVolRender->SetQualityT(mQualityT);

    if (mSMVProcess != NULL)
    {
        bool isPlaying = IsTransportPlaying();
        mSMVProcess->UpdateTimeAxis(isPlaying);
    }
    
    if (mVolRender != NULL)
        mVolRender->SetSpeedT(mSpeedT);

    //if (mVolRender != NULL)
    //    mVolRender->SetSpeed(mSpeed);

    if (mVolRender != NULL)
        mVolRender->SetThreshold(mThreshold);

    if (mVolRender != NULL)
        mVolRender->SetThresholdCenter(mThresholdCenter);

    //if (mVolRender != NULL)
    //    mVolRender->SetInvertColormap(mInvertColormap);

    //if (mSMVProcess != NULL)
    //    mSMVProcess->SetAngularMode(mAngularMode);

    if (mVolRender != NULL)
        mVolRender->SetClipFlag(mClipFlag);

    if (mSMVProcess != NULL)
        mSMVProcess->SetModeX(mModeX);

    if (mSMVProcess != NULL)
        mSMVProcess->SetModeY(mModeY);

    if (mSMVProcess != NULL)
        mSMVProcess->SetModeColor(mModeColor);

    if (mVolRender != NULL)
        mVolRender->SetColormap(mColormap);

    if (mStereoWidthSmoother != NULL)
        //mStereoWidthSmoother->SetNewValue(mWidth);
        mStereoWidthSmoother->ResetToTargetValue(mStereoWidth);
    
    if (mSMVProcess != NULL)
    {
        bool hostIsPlaying = IsTransportPlaying();
        mSMVProcess->SetStereoWidth(mStereoWidth, hostIsPlaying);
    }

    if (mVolRender != NULL)
        mVolRender->SetColormapRange(mRange);

    if (mVolRender != NULL)
        mVolRender->SetColormapContrast(mContrast);

    //if (mVolRender != NULL)
    //    mVolRender->SetPointSize(mPointSize);

    //if (mVolRender != NULL)
    //    mVolRender->SetRenderAlgo(mAlgo);

    if (mVolRender != NULL)
        mVolRender->SetRenderAlgoParam(mRenderAlgoParam);

    if (mVolRender != NULL)
        mVolRender->SetAlphaCoeff(mAlphaCoeff);

    if (mVolRender != NULL)
        mVolRender->SetQuality(mQuality);
    
    if (mVolRender != NULL)
        mVolRender->SetAutoQuality(mAutoQuality);

    if (mQualityKnob != NULL)
        mQualityKnob->SetDisabled(mAutoQuality);

    if (mPanSmoother != NULL)
        mPanSmoother->ResetToTargetValue(mPan);

    if (mInGainSmoother != NULL)
        mInGainSmoother->ResetToTargetValue(mInGain);

    if (mOutGainSmoother != NULL)
        mOutGainSmoother->ResetToTargetValue(mOutGain);

    if (mVolRender != NULL)
        mVolRender->SetCamAngle0(mAngle0);

    if (mVolRender != NULL)
        mVolRender->SetCamAngle1(mAngle1);

    if (mVolRender != NULL)
        mVolRender->SetCamFov(mCamFov);
    
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
SoundMetaViewer::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;

    BL_FLOAT sampleRate = GetSampleRate();
    
    BL_FLOAT defaultStereoWidth = 0.0;
    mStereoWidthSmoother = new ParamSmoother2(sampleRate, defaultStereoWidth);

    // Pan
    BL_FLOAT defaultPan = 0.0;
    mPanSmoother = new ParamSmoother2(sampleRate, defaultPan);

    // In gain
    BL_FLOAT defaultInGain = 1.0;
    mInGainSmoother = new ParamSmoother2(sampleRate, defaultInGain);

    // Out gain
    BL_FLOAT defaultOutGain = 1.0;
    mOutGainSmoother = new ParamSmoother2(sampleRate, defaultOutGain);

    mVolRender = new SMVVolRender3(this, mGraph, BUFFER_SIZE, sampleRate);
  
    //
    if (mFftObj == NULL)
    {
        int numChannels = 2;
        int numScInputs = 0;
    
        vector<ProcessObj *> processObjs;
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      BUFFER_SIZE, oversampling, freqRes,
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
    
        mSMVProcess = new SMVProcess4(BUFFER_SIZE,
                                      oversampling, freqRes,
                                      sampleRate);
    
        // New...
        mSMVProcess->SetVolRender(mVolRender);
      
        mFftObj->AddMultichannelProcess(mSMVProcess);
    }
    else
    {
        BL_FLOAT sampleRate = GetSampleRate();
      
        mFftObj->Reset(oversampling, freqRes, 1, sampleRate);
    }
    
    ApplyParams();
    
    mIsInitialized = true;
}

void
SoundMetaViewer::ProcessBlock(iplug::sample **inputs,
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
    
    if (isPlaying) // Avoid scrolling when no sound
    {
        // If we have only one channel, duplicate it
        // This is simpler like that, for later stereoize
        if (in.size() == 1)
        {
            in.resize(2);
            in[1] = in[0];
        }
    
        // Can't call SoundMetaViewerProcess3::SimpleStereoWiden() here,
        // even if it processes samples
        // because in all cases, we need fft for Stereoize() (mono -> stereo)
        //SoundMetaViewerProcess3::SimpleStereoWiden(&samples, mWidthChange);
      
#if STEREO_WIDEN_SAMPLES
        // Stereo widen
        vector<WDL_TypedBuf<BL_FLOAT> *> stereoInputs;
        stereoInputs.resize(2);
        stereoInputs[0] = &in[0];
        stereoInputs[1] = &in[1];
        
        StereoWidenProcess::StereoWiden(&stereoInputs, mStereoWidthSmoother);
#endif
        
        //
        //StereoWidenProcess::StereoWiden(&samples0, mStereoWidthSmoother);
        // NOTE: manage pan here, later manage it in SMVProcess4, to be able to
        // change the pan on the whole data, when not playing.
        StereoWidenProcess::Balance(&stereoInputs, mPanSmoother);
        
        // Apply input gain
        ApplyGain(&in, mInGainSmoother);
      
        // Play
        //
        //if (mFreeze)
        //    mSMVProcess->SetIsPlaying(!mFreeze);
        if (mSMVProcess != NULL)
            mSMVProcess->SetIsPlaying(mPlayStop);
        
        if (isPlaying && !mFreeze)
        {
            if (mSMVProcess != NULL)
                mSMVProcess->SetPlayMode(SMVProcess4::RECORD);
        }
        
        if (mFreeze)
        {
            if (mSMVProcess != NULL)
                mSMVProcess->SetPlayMode(SMVProcess4::PLAY);
        }
        
        if (!isPlaying && !mFreeze)
        {
            if (mSMVProcess != NULL)
                mSMVProcess->SetPlayMode(SMVProcess4::BYPASS);
        }

        if (mSMVProcess != NULL)
            mSMVProcess->SetHostIsPlaying(isPlaying);
        
#if 0 // TODO ??
        bool enablePanoFftObj = !mFreeze && isPlaying;
        mPanogramFftObj->SetEnabled(enablePanoFftObj);
#endif

      
#if 0 // TODO ??
      // Test, to avoid playing a single line in loop when play button is off
        if (!(mFreeze && !mPlay))
            out = playOut; // Normal behavior
        
        if (mPlay)
        {
            UpdatePlayBar();
        }
        else
        {
            mCustomDrawer->SetPlayBarActive(false);
        }
        
        // Use bar only in freeze mode
        //SetBarActive(mFreeze);
        
        if (mPanogramPlayFftObjs[0]->SelectionPlayFinished())
        {
            ResetPlayBar();
        }
#endif
        
        // Process
        mFftObj->Process(in, scIn, &out);
        
        //WDL_TypedBuf<double> polarX;
        //WDL_TypedBuf<double> polarY;
        //WDL_TypedBuf<double> colorWeights;
        //mSoundMetaViewerProcess->GetWidthValues(&polarX, &polarY, &colorWeights);
        
#if USE_FLAT_POLAR
        CartesianToPolarFlat(&polarX, &polarY);
#endif
        
        // It is now done internally
        //mVolRender->AddCurveValuesWeight(polarX, polarY, colorWeights);
        
        // Apply out gain
        ApplyGain(&out, mOutGainSmoother);
        
        // Should be useless...
        //BLUtils::PlugCopyOutputs(out, outputs, nFrames);
        
#if BYPASS_FFT_PROCESS
        // Bypass (since overlap is 1, we want a good sound output
        BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
#else
        BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
#endif
    }

    UpdateTimeAxis(isPlaying);
    
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
SoundMetaViewer::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN,
                                     kGraph);
    
    if (mVolRender != NULL)
        mVolRender->SetGraph(mGraph);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);
    int sepColor[4] = { 24, 24, 24, 255 };
    mGraph->SetSeparatorY0(2.0, sepColor);
    
    //
#if !HIDE_UNUSED_CONTROLS
    mGUIHelper->CreateKnob(pGraphics,
                           kQualityXYX, kQualityXYY,
                           KNOB_SMALL_FN,
                           kQualityXYFrames,
                           kQualityXY,
                           TEXTFIELD_FN,
                           "# QUALITY XY",
                           GUIHelper12::SIZE_DEFAULT);
#endif

#if !HIDE_QUALITY_T_KNOB
    mGUIHelper->CreateKnob(pGraphics,
                           kQualityTX, kQualityTY,
                           KNOB_SMALL_FN,
                           kQualityTFrames,
                           kQualityT,
                           TEXTFIELD_FN,
                           "# QUALITY T",
                           GUIHelper12::SIZE_DEFAULT);
#endif

    mGUIHelper->CreateKnob(pGraphics,
                           kSpeedTX, kSpeedTY,
                           KNOB_SMALL_FN,
                           kSpeedTFrames,
                           kSpeedT,
                           TEXTFIELD_FN,
                           "SPEED",
                           GUIHelper12::SIZE_DEFAULT);

#if !HIDE_UNUSED_CONTROLS
    mGUIHelper->CreateKnob(pGraphics,
                           kSpeedX, kSpeedY,
                           KNOB_SMALL_FN,
                           kSpeedFrames,
                           kSpeed,
                           TEXTFIELD_FN,
                           "#SPEED",
                           GUIHelper12::SIZE_DEFAULT);
#endif

    mGUIHelper->CreateKnob(pGraphics,
                           kThresholdX, kThresholdY,
                           KNOB_SMALL_FN,
                           kThresholdFrames,
                           kThreshold,
                           TEXTFIELD_FN,
                           "THRS AMOUNT",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kThresholdCenterX, kThresholdCenterY,
                           KNOB_SMALL_FN,
                           kThresholdCenterFrames,
                           kThresholdCenter,
                           TEXTFIELD_FN,
                           "THRS CENTER",
                           GUIHelper12::SIZE_DEFAULT);

#if 0 // Should depend on a macro
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kInvertColormapX,
                                   kInvertColormapY,
                                   CHECKBOX_FN, kInvertColormap, "INV");

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kAngularModeX,
                                   kAngularModeY,
                                   CHECKBOX_FN, kAngularMode, "ANGULAR");
#endif
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kClipX,
                                   kClipY,
                                   CHECKBOX_FN, kClip, "CLIP SELECTION");
    
#if !HIDE_UNUSED_CONTROLS3
    const char *radioLabels[] = { "SIMPLE", "SOURCE", "SCANNER", "SPECTRO", "SPECTRO2" };

    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeX,
                                   kRadioButtonsModeY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeNumButtons,
                                   kRadioButtonModeVSize,
                                   kMode,
                                   false,
                                   "DISPLAY",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsMode);
#endif

    const char *radioLabelsColormap[] = { "BLUE", "OCEAN", "PURPLE", "WASP",
                                          "DAWN", "RAINBOW",  "SWEET" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsColormapX,
                                   kRadioButtonsColormapY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonColormapNumButtons,
                                   kRadioButtonColormapVSize,
                                   kColormap,
                                   false,
                                   "COLORMAP",
                                   EAlign::Near,
                                   EAlign::Near,
                                   radioLabelsColormap);

    mGUIHelper->CreateKnob(pGraphics,
                           kStereoWidthX, kStereoWidthY,
                           KNOB_SMALL_FN,
                           kStereoWidthFrames,
                           kStereoWidth,
                           TEXTFIELD_FN,
                           "STEREO WIDTH",
                           GUIHelper12::SIZE_DEFAULT);

#if DEBUG_CONTROLS
    mGUIHelper->CreateKnob(pGraphics,
                           kRangeX, kRangeY,
                           KNOB_SMALL_FN,
                           kRangeFrames,
                           kRange,
                           TEXTFIELD_FN,
                           "BRIGHTNESS",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kContrastX, kContrastY,
                           KNOB_SMALL_FN,
                           kContrastFrames,
                           kContrast,
                           TEXTFIELD_FN,
                           "CONTRAST",
                           GUIHelper12::SIZE_DEFAULT);
#endif

#if !HIDE_POINT_SIZE_KNOB
    mGUIHelper->CreateKnob(pGraphics,
                           kPointSizeX, kPointSizeY,
                           KNOB_SMALL_FN,
                           kPointSizeFrames,
                           kPointSize,
                           TEXTFIELD_FN,
                           "# POINT SIZE",
                           GUIHelper12::SIZE_DEFAULT);
#endif

#if DEBUG_SHOW_ALGO_RENDER_CONTROLS
    const char *radioLabelsRenderAlgo[] = { "GRID(1)", "GRID(2)", "QUAD TREE",
                                            "KD TREE(1)", "KD TREE(2)" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsRnderAlgoX,
                                   kRadioButtonsRnderAlgoY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonRnderAlgoNumButtons,
                                   kRadioButtonRnderAlgoVSize,
                                   kRnderAlgo,
                                   false,
                                   "RC ALGO",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsRnderAlgo);
#endif

#if 0 // should depend on a macro
    mGUIHelper->CreateKnob(pGraphics,
                           kRenderAlgoParamX, kRenderAlgoParamY,
                           KNOB_SMALL_FN,
                           kRenderAlgoParamFrames,
                           kRenderAlgoParam,
                           TEXTFIELD_FN,
                           "RC PARAM",
                           GUIHelper12::SIZE_DEFAULT);
#endif
    
    mGUIHelper->CreateKnob(pGraphics,
                           kAlphaCoeffX, kAlphaCoeffY,
                           KNOB_SMALL_FN,
                           kAlphaCoeffFrames,
                           kAlphaCoeff,
                           TEXTFIELD_FN,
                           "TRANSPARENCY",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kAutoQualityX,
                                   kAutoQualityY,
                                   CHECKBOX_FN, kAutoQuality, "AUTO QUALITY");
    
    mQualityKnob = mGUIHelper->CreateKnob(pGraphics,
                                          kQualityX, kQualityY,
                                          KNOB_SMALL_FN,
                                          kQualityFrames,
                                          kQuality,
                                          TEXTFIELD_FN,
                                          "QUALITY",
                                          GUIHelper12::SIZE_DEFAULT);
    int defaultAutoQuality = 1;
    mQualityKnob->SetDisabled(defaultAutoQuality);

    const char *radioLabelsModeX[] = { "SCOPE", "SCOPE(FLAT)",
                                       //"DIFF",
                                       "LISSAJOUS",//"DIFF(FLAT)",
                                       "EXP",
                                       "FREQ", "CHROMA" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeXX,
                                   kRadioButtonsModeXY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeXNumButtons,
                                   kRadioButtonModeXVSize,
                                   kModeX,
                                   false,
                                   "MODE X",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsModeX);

    const char *radioLabelsModeY[] = { "AMP", "FREQ", "CHROMA",
                                       "PH. FREQ.", "PH. TIME." };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeYX,
                                   kRadioButtonsModeYY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeYNumButtons,
                                   kRadioButtonModeYVSize,
                                   kModeY,
                                   false,
                                   "MODE Y",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsModeY);

    const char *radioLabelsModeColor[] = { "AMP", "FREQ", "CHROMA", "PH. FREQ.", "PH. TIME.",
                                           "PAN", "M/S" };
    mGUIHelper->CreateRadioButtons(pGraphics,
                                   kRadioButtonsModeColorX,
                                   kRadioButtonsModeColorY,
                                   RADIOBUTTON_FN,
                                   kRadioButtonModeColorNumButtons,
                                   kRadioButtonModeColorVSize,
                                   kModeColor,
                                   false,
                                   "MODE C",
                                   EAlign::Far,
                                   EAlign::Far,
                                   radioLabelsModeColor);
    
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kFreezeX,
                                   kFreezeY,
                                   CHECKBOX_FN, kFreeze, "FREEZE");

    mPlayButton = mGUIHelper->CreateRolloverButton(pGraphics,
                                                   kPlayStopX, kPlayStopY,
                                                   PLAY_BUTTON_FN,
                                                   kPlayStop,
                                                   "", true);
    mPlayButton->SetDisabled(!mFreeze);
    
    mGUIHelper->CreateKnob(pGraphics,
                           kPanX, kPanY,
                           KNOB_SMALL_FN,
                           kPanFrames,
                           kPan,
                           TEXTFIELD_FN,
                           "PAN",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kInGainX, kInGainY,
                           KNOB_SMALL_FN,
                           kInGainFrames,
                           kInGain,
                           TEXTFIELD_FN,
                           "IN GAIN",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kOutGainX, kOutGainY,
                           KNOB_SMALL_FN,
                           kOutGainFrames,
                           kOutGain,
                           TEXTFIELD_FN,
                           "OUT GAIN",
                           GUIHelper12::SIZE_DEFAULT);
    
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
SoundMetaViewer::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();

    BL_FLOAT sampleRate = GetSampleRate();

    if (mPanSmoother != NULL)
        mPanSmoother->Reset(sampleRate);
    if (mInGainSmoother != NULL)
        mInGainSmoother->Reset(sampleRate);
    if (mOutGainSmoother != NULL)
        mOutGainSmoother->Reset(sampleRate);
    if (mStereoWidthSmoother != NULL)
        mStereoWidthSmoother->Reset(sampleRate);
    
    if (mFftObj != NULL)
        mFftObj->Reset();
    if (mSMVProcess != NULL)
        mSMVProcess->Reset();
    
    if (mVolRender != NULL)
        mVolRender->Reset(sampleRate);

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
SoundMetaViewer::OnParamChange(int paramIdx)
{
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
#if !HIDE_UNUSED_CONTROLS
        case kQualityXY:
        {
            BL_FLOAT qualityXY = GetParam(kQualityXY)->Value();
            qualityXY = qualityXY/100.0;      
            mQualityXY = qualityXY;

            if (mVolRender != NULL)
                mVolRender->SetQualityXY(mQualityXY);
        }
        break;
#endif
          
#if !HIDE_QUALITY_T_KNOB
        case kQualityT:
        {
            BL_FLOAT qualityT = GetParam(kQualityT)->Value();
            qualityT = qualityT/100.0;
            mQualityT = qualityT;

            if (mVolRender != NULL)
                mVolRender->SetQualityT(mQualityT);

            if (mSMVProcess != NULL)
                mSMVProcess->UpdateTimeAxis();
        }
        break;
#endif
          
        case kSpeedT:
        {
            BL_FLOAT speedT = GetParam(kSpeedT)->Value();
            speedT = speedT/100.0;
            mSpeedT = speedT;
            if (fabs(speedT - mPrevSpeed) > EPS)
            {
                if (mVolRender != NULL)
                    mVolRender->SetSpeedT(speedT);
                if (mSMVProcess != NULL)
                    mSMVProcess->ResetTimeAxis();
            
                mPrevSpeed = speedT;
            }
        }
        break;
          
#if !HIDE_UNUSED_CONTROLS
        case kSpeed:
        {
            BL_FLOAT speed = GetParam(kSpeed)->Value();
            speed = speed/100.0;
            mSpeed = speed;

            if (mVolRender != NULL)
                mVolRender->SetSpeed(mSpeed);
            if (mSMVProcess != NULL)
                mSMVProcess->ResetTimeAxis();
        }
        break;
#endif
          
        case kThreshold:
        {
            BL_FLOAT threshold = GetParam(kThreshold)->Value();
            threshold = threshold/100.0;
            if (fabs(threshold - mPrevThreshold) > EPS)
            {
                mThreshold = threshold;
                mPrevThreshold = mThreshold;

                if (mVolRender != NULL)
                    mVolRender->SetThreshold(threshold);
            }
        }
        break;
    
        case kThresholdCenter:
        {
            BL_FLOAT thresholdCenter = GetParam(kThresholdCenter)->Value();
            thresholdCenter = thresholdCenter/100.0;
        
            if (fabs(thresholdCenter - mPrevThresholdCenter) > EPS)
            {
                mThresholdCenter = thresholdCenter;
                mPrevThresholdCenter = mThresholdCenter;

                if (mVolRender != NULL)
                    mVolRender->SetThresholdCenter(thresholdCenter);
            }
        }
        break;
          
#if !HIDE_UNUSED_CONTROLS2
        case kInvertColormap:
        {
            int invertColormapFlag = GetParam(kInvertColormap)->Value();
            mInvertColormap = invertColormapFlag;

            if (mVolRender != NULL)
                mVolRender->SetInvertColormap(mInvertColormap);
        }
        break;
    
        case kAngularMode:
        {
            int angularModeFlag = GetParam(kAngularMode)->Value();
            mAngularMode = angularModeFlag;
        
            if (mSMVProcess != NULL)
                mSMVProcess->SetAngularMode(mAngularMode);
        }
        break;
#endif
          
        case kClip:
        {
            int clipFlag = GetParam(kClip)->Value();
            if (fabs(clipFlag - mPrevClipFlag) > EPS)
            {
                mClipFlag = clipFlag;
                mPrevClipFlag = mClipFlag;
            
                if (mVolRender != NULL)
                    mVolRender->SetClipFlag(mClipFlag);
            }
        }
        break;
    
        // Modes
        case kModeX:
        {
            SMVProcess4::ModeX mode = (SMVProcess4::ModeX)GetParam(kModeX)->Int();
            mModeX = mode;
            if (fabs(mode - mPrevModeX) > EPS)
            {
                if (mSMVProcess != NULL)
                    mSMVProcess->SetModeX(mode);
          
                mPrevModeX = mode;
            }
        }
        break;
    
        case kModeY:
        {
            SMVProcess4::ModeY mode = (SMVProcess4::ModeY)GetParam(kModeY)->Int();
            mModeY = mode;
            if (fabs(mode - mPrevModeY) > EPS)
            {
                mPrevModeY = mode;
            
                if (mSMVProcess != NULL)
                    mSMVProcess->SetModeY(mode);
            }
        }
        break;
    
        case kModeColor:
        {
            SMVProcess4::ModeColor mode = (SMVProcess4::ModeColor)GetParam(kModeColor)->Int();
            mModeColor = mode;
            if (fabs(mode - mPrevModeCol) > EPS)
            {
                if (mSMVProcess != NULL)
                    mSMVProcess->SetModeColor(mode);
            
                mPrevModeCol = mode;
            }
        }
        break;
          
        case kColormap:
        {
            int colormap = GetParam(kColormap)->Int();
            if (fabs(colormap - mPrevColormap) > EPS)
            {
                mColormap = colormap;
                mPrevColormap = mColormap;
            
                if (mVolRender != NULL)
                    mVolRender->SetColormap(mColormap);
            }
        }
        break;
      
        case kStereoWidth:
        {
            BL_FLOAT width = GetParam(kStereoWidth)->Value();
            width = width/100.0;
            mStereoWidth = width;
            if (fabs(width - mPrevStereoWidth) > EPS)
            {
                if (mStereoWidthSmoother != NULL)
                    mStereoWidthSmoother->SetTargetValue(width);
        
                if (mSMVProcess != NULL)
                {
                    bool hostIsPlaying = IsTransportPlaying();
                    mSMVProcess->SetStereoWidth(width/*mStereoWidth*/, hostIsPlaying);
                }
            
                mPrevStereoWidth = width;
            }
        }
        break;
          
#if DEBUG_CONTROLS
        case kRange:
        {
            BL_FLOAT range = GetParam(paramIdx)->Value();
            mRange = range;
            if (fabs(range - mPrevRange) > EPS)
            {
                if (mVolRender != NULL)
                    mVolRender->SetColormapRange(range);
            
                mPrevRange = range;
            }
        }
        break;
          
        case kContrast:
        {
            BL_FLOAT contrast = GetParam(paramIdx)->Value();
            mContrast = contrast;
            if (fabs(contrast - mPrevContrast) > EPS)
            {
                if (mVolRender != NULL)
                    mVolRender->SetColormapContrast(contrast);
            
                mPrevContrast = contrast;
            }
        }
        break;
         
#if !HIDE_POINT_SIZE_KNOB
        case kPointSize:
        {
            BL_FLOAT pointSize = GetParam(paramIdx)->Value();
            mPointSize = pointSize;
            if (mVolRender != NULL)
                mVolRender->SetPointSize(pointSize);
        }
        break;
#endif
    
#if DEBUG_SHOW_ALGO_RENDER_CONTROLS
        case kRenderAlgo:
        {
            int algo = GetParam(kRenderAlgo)->Int();
            mAlgo = algo;
            if (mVolRender != NULL)
                mVolRender->SetRenderAlgo(algo);
        }
        break;
          
        case kRenderAlgoParam:
        {
            BL_FLOAT value = GetParam(paramIdx)->Value();
            BL_FLOAT renderParam = value/100.0;
            mRenderAlgoParam = renderParam;

            if (mVolRender != NULL)
                mVolRender->SetRenderAlgoParam(renderParam);
        }
        break;
#endif
          
        //#if !HIDE_UNUSED_CONTROLS
        case kAlphaCoeff:
        {
            BL_FLOAT alphaCoeff = GetParam(paramIdx)->Value();
            // Was "opacity"
            alphaCoeff = 100.0 - alphaCoeff;
            mAlphaCoeff = alphaCoeff;
            
            if (fabs(alphaCoeff - mPrevAlphaCoeff) > EPS)
            {
                if (mVolRender != NULL)
                    mVolRender->SetAlphaCoeff(alphaCoeff);
            
                mPrevAlphaCoeff = alphaCoeff;
            }
        }
        break;
        //#endif
          
        case kQuality:
        {
            BL_FLOAT quality = GetParam(kQuality)->Value();
            quality = quality/100.0;
            if (fabs(quality - mPrevQuality) > EPS)
            {
                mQuality = quality;

                if (mVolRender != NULL)
                    mVolRender->SetQuality(mQuality);
            
                mPrevQuality = mQuality;
            }
        }
        break;
#endif

        case kFreeze:
        {
            int value = GetParam(kFreeze)->Value();
            mFreeze = value;
            if (fabs(value - mPrevFreeze) > EPS)
            {
                mPrevFreeze = value;
                
                bool freeze = (value == 1);
          
                // NEW
                mPlayStop = false;
                
                if (!freeze)
                {
                    // If we toggle off freeze, set the play button to "stop"
                    //SetParameterFromGUI(kPlayStop, 0.0);
                    SetParameterValue(kPlayStop, 0.0);
                    if (mPlayButton != NULL)
                        mPlayButton->SetValue(0.0);
                    
#if 0 // TODO
                    SetBarActive(false);
#endif
            
#if 0 // TODO
#if 1 // FIX: with no selection, freeze on, freeze off, play
                    // => the sound is played in the middle instead of on the right of the graph
                    if ((mCustomDrawer != NULL) && !mCustomDrawer->IsSelectionActive())
                    {
                        for (int i = 0; i < 2; i++)
                        {
                            if (mPanogramPlayFftObjs[i] != NULL)
                                mPanogramPlayFftObjs[i]->SetSelectionEnabled(false);
                        }
                    }
#endif
#endif
                }
          
                if (freeze)
                {
                    // Set the play button to "stop" even when toggleing freeze on
                    // This avoids having the playbar moving jut at startup
                    // For this, kPlayStop must be declared before kFreeze,
                    //SetParameterFromGUI(kPlayStop, 0.0);
                    SetParameterValue(kPlayStop, 0.0);
                    if (mPlayButton != NULL)
                        mPlayButton->SetValue(0.0);
                }
          
                mFreeze = freeze;

                if (mPlayButton != NULL)
                    mPlayButton->SetDisabled(!mFreeze);
          
                if (mFreeze)
                {
#if 0 // TODO
                    // If we don't have a current selection,
                    // reset the previous bar state
                    if ((mCustomDrawer != NULL) && !mCustomDrawer->IsSelectionActive())
                    {
                        for (int i = 0; i < 2; i++)
                        {
                            if (mPanogramPlayFftObjs[i] != NULL)
                                mPanogramPlayFftObjs[i]->RewindToNormValue(0.0);
                        }
                        
                        BarSetSelection(0.0);
                        SetBarPos(0.0);
                    }
              
                    // FIX: avoid a scroll jump after having freezed,
                    // at the moment we start to draw a selection
                    if (mSpectrogramDisplay != NULL)
                        mSpectrogramDisplay->UpdateSpectrogram(true);
#endif
                }
            }
        }
        break;

        case kPlayStop:
        {
            int val = GetParam(kPlayStop/*paramIdx*/)->Int();
            if (fabs(val - mPrevPlayStop) > EPS)
            {
                mPlayStop = (val == 1);
        
#if 0 // TODO
                for (int i = 0; i < 2; i++)
                {
                    if (mPanogramPlayFftObjs[i] != NULL)
                        mPanogramPlayFftObjs[i]->SetIsPlaying(mPlay);
                }
#endif
      
#if 0 // TODO
#if 1       // IMPROV: play, stop, play => the playbar now restarts at the beginning
            // of the selection of the bar
                if (mPlay)
                {
                    ResetPlayBar();
                }
            
                mPrevPlayStop = val;
#endif
#endif
            }
        }
        break;
        
        case kAutoQuality:
        {
            int autoQualityFlag = GetParam(kAutoQuality)->Value();
            mAutoQuality = autoQualityFlag;
            if (fabs(autoQualityFlag - mPrevAutoQuality) > EPS)
            {
                if (mVolRender != NULL)
                    mVolRender->SetAutoQuality(autoQualityFlag);

                if (mQualityKnob != NULL)
                    mQualityKnob->SetDisabled(autoQualityFlag);
            
                mPrevAutoQuality = autoQualityFlag;
            }
        }
        break;
     
        case kPan:
        {
            BL_FLOAT value = GetParam(kPan/*paramIdx*/)->Value();
            BL_FLOAT pan = value/100.0;
            mPan = pan;

            if (mPanSmoother != NULL)
                mPanSmoother->SetTargetValue(pan);
        }
        break;
       
        case kInGain:
        {
            BL_FLOAT gain = GetParam(paramIdx)->DBToAmp();
            mInGain = gain;
            if (mInGainSmoother != NULL)
                mInGainSmoother->SetTargetValue(gain);
        }
        break;
          
        case kOutGain:
        {
            BL_FLOAT gain = GetParam(paramIdx)->DBToAmp();
            mOutGain = gain;
            if (mOutGainSmoother != NULL)
                mOutGainSmoother->SetTargetValue(gain);
        }
        break;
       
        case kAngle0:
        {
            BL_FLOAT angle = GetParam(kAngle0)->Value();
            mAngle0 = angle;
            if (mVolRender != NULL)
                mVolRender->SetCamAngle0(angle);
        }
        break;
          
        case kAngle1:
        {
            BL_FLOAT angle = GetParam(kAngle1)->Value();
            mAngle1 = angle;
            if (mVolRender != NULL)
                mVolRender->SetCamAngle1(angle);
        }
        break;
          
        case kCamFov:
        {
            BL_FLOAT angle = GetParam(kCamFov)->Value();
            mCamFov = angle;
            if (mVolRender != NULL)
                mVolRender->SetCamFov(angle);
        }
        break;

        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
SoundMetaViewer::OnUIOpen()
{
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    
    ApplyParams();
    
    LEAVE_PARAMS_MUTEX;
}

void
SoundMetaViewer::OnUIClose()
{
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;
    
    if (mVolRender != NULL)
        mVolRender->SetGraph(NULL);

    mQualityKnob = NULL;
    mPlayButton = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

void
SoundMetaViewer::SetCameraAngles(BL_FLOAT angle0, BL_FLOAT angle1)
{
    BL_FLOAT normAngle0 = (angle0 + MAX_CAMERA_ANGLE_0)/(MAX_CAMERA_ANGLE_0*2.0);
    GetParam(kAngle0)->SetNormalized(normAngle0);
    //BLUtilsPlug::TouchPlugParam(this, kAngle0);
    
    BL_FLOAT normAngle1 =
    (angle1 - MIN_CAMERA_ANGLE_1)/(MAX_CAMERA_ANGLE_1 - MIN_CAMERA_ANGLE_1);
    GetParam(kAngle1)->SetNormalized(normAngle1);
    //BLUtilsPlug::TouchPlugParam(this, kAngle1);
}

void
SoundMetaViewer::SetCameraFov(BL_FLOAT angle)
{
    BL_FLOAT normAngle = (angle - MIN_CAMERA_FOV)/(MAX_CAMERA_FOV - MIN_CAMERA_FOV);
    GetParam(kCamFov)->SetNormalized(normAngle);
    //BLUtilsPlug::TouchPlugParam(this, kCamFov);
}

void
SoundMetaViewer::UpdateTimeAxis(bool isPlaying)
{
    if (isPlaying)
    {
        // Time axis
        BL_FLOAT sampleRate = GetSampleRate();
        BL_FLOAT samplePos = GetTransportSamplePos();
        if (samplePos >= 0.0)
        {
            BL_FLOAT currentTime = samplePos/sampleRate;
            
#if FIX_TIME_AXIS_LATENCY
            BL_FLOAT latencySeconds = ComputeLatencySeconds();
            currentTime -= latencySeconds;
#endif

            if (mSMVProcess != NULL)
                mSMVProcess->UpdateTimeAxis(currentTime);
        }
    }
}

void
SoundMetaViewer::ApplyGain(vector<WDL_TypedBuf<BL_FLOAT> > *samples,
                           ParamSmoother2 *gainSmoother)
{
    if (samples->empty())
        return;

    if (gainSmoother == NULL)
        return;
    
    // Signal processing
    for (int i = 0; i < (*samples)[0].GetSize(); i++)
    {
        // Parameters update
        BL_FLOAT gain = gainSmoother->Process();
            
        BL_FLOAT leftSample = (*samples)[0].Get()[i];
        (*samples)[0].Get()[i] = gain*leftSample;
            
        if ((samples->size() > 1))
        {
            BL_FLOAT rightSample = (*samples)[1].Get()[i];
            (*samples)[1].Get()[i] = gain*rightSample;
        }
    }
}
