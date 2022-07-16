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
 
#include <GhostSamplesToSpectro2.h>

#include <AudioFile.h>
#include <SpectrogramFftObj2.h>
#include <SpectrogramView2.h>
#include <SpectrogramDisplay3.h>
#include <MiniView2.h>

#include <FftProcessObj16.h>
#include <GraphControl12.h>
#include <BLSpectrogram4.h>

#include <SamplesPyramid3.h>

#include <GhostCustomControl2.h>
#include <GhostCustomDrawer2.h>

#include <GraphTimeAxis6.h>
#include <GraphFreqAxis2.h>

#include <GraphAxis2.h>

#include <SpectroEditFftObj3.h>

#include <GhostCommandHistory.h>

#include <GUIHelper12.h>

#include <BLUtils.h>
#include <BLUtilsDecim.h>
#include <BLUtilsFade.h>
#include <BLUtilsMath.h>

#include "config.h"
#include "GhostTrack2.h"

#define OVERSAMPLING_0 4
#define OVERSAMPLING_1 8
#define OVERSAMPLING_2 16
#define OVERSAMPLING_3 32

// Overlapping used when editting
#define EDIT_OVERLAPPING  OVERSAMPLING_0

#define FREQ_RES 1
#define VARIABLE_HANNING 1
#define KEEP_SYNTHESIS_ENERGY 0

// better, smoother, and no perf loss
#define SPECTRO_GRAPH_UPDATE_RATE 1 // 4 (old)

#define SPECTRO_VIEW_MAX_NUM_COLS 1024 //512

// TEST: Interesting when set to 0
#define RECOMPUTE_DATA_DELAY 500 // Origin
//#define RECOMPUTE_DATA_DELAY 100

// GOOD: 1
#define ADAPTIVE_OVERSAMPLING 1 //0

#define SPECTRO_MINI_SIZE 0.075 //0.1
#define SPECTRO_MINI_OFFSET 0.005
#define MINIVIEW_CLIP_CURVE_VALUE -0.85 // Empirical

// When set to BUFFER_SIZE, it makes less flat areas at 0
#define GRAPH_CONTROL_NUM_POINTS 2048 //BUFFER_SIZE
//#define GRAPH_CONTROL_NUM_POINTS BUFFER_SIZE/4

#define DEFAULT_WAVEFORM_SCALE 0.25

#define FORCE_REFRESH_DATA 0

// IMPROV: display waveform in view mode
// (reply to user request)
#define VIEW_MODE_DISPLAY_WAVE 1

// Added during GHOST_OPTIM_GL
// Spectrogram scroll adaptive speed (depending on the sample rate)
#define CONSTANT_SPEED_FEATURE 1

// Added during GHOST_OPTIM_GL
// FIX: the samplerate was not transmitted correstly to the fft objects
#define FIX_FFT_SAMPLERATE 1

// FIX: view a bit, change GUI size: the miniview waveform sudently appears
#define FIX_VIEW_MODE_NO_MINIVIEW 1

// FIX: capture a piece of signal, change GUI size
// => the main waveform is not updated
#define FIX_ACQUIRE_MODE_UPDATE_WAVEFORM 1

// FIX: capture a bit, change GUI size: the miniview waveform sudently appears
#define FIX_CAPTURE_MODE_NO_MINIVIEW 1

// Skip ifft for display obj?
#define OPTIM_SKIP_IFFT 1 //0

// Skip ifft for disp gen obj?
#define OPTIM_SKIP_IFFT2 1

#define BEVEL_CURVES 1

#define NUM_TIME_AXIS_LABELS 10

// Default is 8
//
// Increase it since bigger spectro zoom
// (see also in SpectrogramView.cpp:MAX_ZOOM)
//
// 8  : for small files
// 12 : for 4mn file
// 16 : for 1h file (theoric)
// 18 : for 4h file (theoric)
#define SAMPLES_PYRAMID_MAX_LEVEL 18 //12 //8 //9

// NOTE: not used anymore!
// Did not fix well sometimes
// Now found a better solution: AudioFile::FixDataBounds()
//
// Previously, we clipped the waveform
// But when just editting, saving, then reloading, that made artifacts
// in th espectrogram and in the sound
#define CLIP_WAVEFORM 0 //1

// Command history
// Multiply by 2 because when we have 2 channels,
// we store the commands twice
#define COMMAND_HISTORY_SIZE 10*2

// Finally, do not use LockFree mechanism for Ghost
// (would need to much code refactoring)
#define USE_LEGACY_LOCK 1

// Update the whole background after any edition
// => so the background spectrogram is in synch with current edition
// Surprisingly, the seems to not take too much resource, even with
// a whole song loaded in Ghost App
#define UPDATE_BG_AFTER_EDIT 1

// Remove control correctly
#define FIX_REMOVE_CONTROL_MORPHO 1

// Correct spectrogram refresh after UI close and re-open 
#define FIX_MORPHO_RECREATE_CONTROLS 1


GhostTrack2::GhostTrack2(int bufferSize, BL_FLOAT sampleRate, Scale::Type yScale,
                         int startMode)
{    
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;
    mPrevSampleRate = mSampleRate;

    mYScale = yScale;
    mLowFreqZoom = false;
    mMustRecomputeLowFreqZoom = true;
            
    mMonitorEnabled = false;
    //mSelectionType = 0;
    
    mIsLoadingSaving = false;

    mIsResetting = false;

    mAppAPIHack = false;

    // Parameters (global)
#if !APP_API
    mMode = (int)VIEW;
#else // APP
    mMode = (int)EDIT;
#endif

    if (startMode != -1)
        mMode = startMode;
    
    InitNull();
    Init();
}

GhostTrack2::~GhostTrack2()
{
    ClearControls(mGraphics);
    
    // Delete it first, since it contains pointer to objects from Ghost,
    // but doesn't own them
    if (mSamplesToSpectro != NULL)
        delete mSamplesToSpectro;

    if (mSpectrogramView != NULL)
        delete mSpectrogramView;

    if (mSpectrogram != NULL)
        delete mSpectrogram;
    if (mSpectrogramBG != NULL)
        delete mSpectrogramBG;
    
    if (mSpectroDisplayObj != NULL)
        delete mSpectroDisplayObj;
    
    for (int i = 0; i < 2; i++)
    {
        if (mSpectroEditObjs[i] != NULL)
            delete mSpectroEditObjs[i];
    }

    if (mDispFftObj != NULL)
        delete mDispFftObj;
    if (mEditFftObj != NULL)
        delete mEditFftObj;
    if (mDispGenFftObj != NULL)
        delete mDispGenFftObj;
 
    if (mSpectroDisplayState != NULL)
        delete mSpectroDisplayState;
    
    if (mCustomDrawerState != NULL)
        delete mCustomDrawerState;

    if (mCustomControl != NULL)
        delete mCustomControl;

    if (mSamplesPyramid != NULL)
        delete mSamplesPyramid;

    if (mTimeAxis != NULL)
        delete mTimeAxis;
  
    if (mFreqAxis != NULL)
        delete mFreqAxis;
  
    if (mHAxis != NULL)
        delete mHAxis;
  
    if (mVAxis != NULL)
        delete mVAxis;

    if (mWaveformCurve != NULL)
        delete mWaveformCurve;
    if (mWaveformOverlayCurve != NULL)
        delete mWaveformOverlayCurve;
    if (mMiniViewWaveformCurve != NULL)
        delete mMiniViewWaveformCurve;

    if (mScale != NULL)
        delete mScale;

    if (mCommandHistory != NULL)
        delete mCommandHistory;

    if (mMiniViewState != NULL)
        delete mMiniViewState;
}

void
GhostTrack2::SetAppAPIHack(bool flag)
{
    mAppAPIHack = flag;
}

void
GhostTrack2::SetCursorListener(CursorListener *listener)
{
    if (mCustomControl != NULL)
        mCustomControl->SetCursorListener(listener);
}

void
GhostTrack2::Reset(int bufferSize, BL_FLOAT sampleRate)
{
    if (mIsResetting)
        return;
    mIsResetting = true;
    
    mBufferSize = bufferSize;
    mSampleRate = sampleRate;

    if (sampleRate != mPrevSampleRate)
    {
        // Frame rate have changed
        if (mFreqAxis != NULL)
            mFreqAxis->Reset(bufferSize, sampleRate);

        bool appAPI = false;
#ifdef APP_API
        appAPI = true;
#endif
        if (!(appAPI || mAppAPIHack))
        {
        // Reset all the buffered data if sample rate changes
        // (otherwise the buffered data would be pitched when playing)
            
        // Reset data
            ResetAllData(false);
            mCommandHistory->Clear();
            
            // RewindView();
            
            // Force a plug mode change to refresh everything well
            ModeChanged(mMode);        
        }

        if (appAPI || mAppAPIHack)
        {
            // Resample current opened tab
            ResampleCurrentData(mPrevSampleRate, sampleRate);
        }
        
        mPrevSampleRate = sampleRate;
    }

    // NOTE: must reset even in edit mode!
    // Otherwise we would have a lot of latency when playing selection
    // (until making an edit action)
    // This was because there is data still buffered in the mEditFftObj,
    // and this makes latency (we use queues)
#if 0
    // In the plugin version, is we press "space" key two times
	// while in Edit mode, the selection and inner playing would be in trouble
	if (mMode != EDIT)
#endif
	{
        // Called when we restart the playback
        // The cursor position may have changed
        // Then we must reset
        if (mSpectroDisplayObj != NULL)
            mSpectroDisplayObj->Reset(-1, mOversampling/*-1*/, -1, sampleRate);
    
#if !FIX_FFT_SAMPLERATE
        if (mDispFftObj != NULL)
            mDispFftObj->Reset();
#else
        if (mDispFftObj != NULL)
            mDispFftObj->Reset(-1, mOversampling/*-1*/, -1, sampleRate);
#endif
    
        for (int i = 0; i < 2; i++)
        {
            if (mSpectroEditObjs[i] != NULL)
                mSpectroEditObjs[i]->Reset(-1, mOversampling/*-1*/, -1, sampleRate);
        }

#if !FIX_FFT_SAMPLERATE
        if (mEditFftObj != NULL)
            mEditFftObj->Reset();

        if (mDispGenFftObj != NULL)
            mDispGenFftObj->Reset();
#else
        if (mEditFftObj != NULL)
            mEditFftObj->Reset(-1, mOversampling/*-1*/, -1, sampleRate);
        
        if (mDispGenFftObj != NULL)
            mDispGenFftObj->Reset(-1, mOversampling/*-1*/, -1, sampleRate);
#endif
    
        if (mSpectrogramView != NULL)
            mSpectrogramView->SetSampleRate(sampleRate);

        // With App, if we don't restrict to render mode,
        // when changing the block size,
        // the current selection disables and we play outside the selection 
        if (mMode == RENDER)
        {    
            for (int i = 0; i < 2; i++)
            {
                if (mSpectroEditObjs[i] != NULL)
                    mSpectroEditObjs[i]->ResetSamplesPos();
            }
        }
	}

    //
    if (mSpectroDisplayObj != NULL)
    {
        int addStep = mSpectroDisplayObj->GetAddStep();
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSpeedMod(addStep);
    }
        
    //  Not sure about that...
    if (mCommandHistory != NULL)
        mCommandHistory->Clear();

    mIsResetting = false;
}

void
GhostTrack2::Reset()
{
    Reset(mBufferSize, mSampleRate);
}
    
void
GhostTrack2::Lock()
{
    if (mGraph == NULL)
        return;
    
    mGraph->Lock();
}

void
GhostTrack2::Unlock()
{
    if (mGraph == NULL)
        return;
    
    mGraph->Unlock();
}

void
GhostTrack2::PushAllData()
{
    if (mGraph == NULL)
        return;
  
    mGraph->PushAllData();
}

void
GhostTrack2::InitNull()
{
    mIsInitialized = false;
    
    mSpectrogramView = NULL;
    mSpectrogramDisplay = NULL;
    mSpectroDisplayState = NULL;
    mSpectroDisplayObj = NULL;
  
    for (int i = 0; i < 2; i++)
    {
        mSpectroEditObjs[i] = NULL;
    }

    mDispFftObj = NULL;
    mEditFftObj = NULL;
    mDispGenFftObj = NULL;
    
    mCustomDrawer = NULL;
    mCustomDrawerState = NULL;
    
    mCustomControl = NULL;

    mTimeAxis = NULL;
    mFreqAxis = NULL;
    
    mHAxis = NULL;  
    mVAxis = NULL;
  
    mWaveformCurve = NULL;
    mWaveformOverlayCurve = NULL;
    mMiniViewWaveformCurve = NULL;
    
    mMiniView = NULL;
    mMiniViewState = NULL;
    
    //    
    mGraph = NULL;
    mPrevGraphWidth = 1;
    mPrevGraphHeight = 1;

    mOversampling = OVERSAMPLING_0;
    
    mSpectrogram = NULL;
    mSpectrogramBG = NULL;
    
    mNeedRecomputeData = false;
    mMustUpdateSpectrogram = true;
    mMustUpdateBGSpectrogram = false;
    
    mWaveformMax = 1.0;
    
    memset(mCurrentFileName, '\0', FILENAME_SIZE);
    mFileModified = false;

    mInternalFileFormat = -1;

    mLoadedChannelSize = 0;

    mSamplesPyramid = NULL;

    mTransportSamplePos = 0.0;

    mIsUpdatingZoom = false;

    mScale = NULL;
    mLowFreqZoom = false;
    mMustRecomputeLowFreqZoom = true;
        
    // Parameters (trask)
    mRange = 0.0;
    mContrast = 0.5;
    mColorMapNum = 0;
    mWaveformScale = 1.0;
    mSpectWaveformRatio = 0.5;
    
    mMonitorEnabled = false;
    //mSelectionType = 0; // RECTANGLE

    mIsPlaying = false;

    mCommandHistory = new GhostCommandHistory(COMMAND_HISTORY_SIZE);
    
    mGraphics = NULL;

    mNumToPopTotal = 0;

    mPrevSelectionActive = false;
}

void
GhostTrack2::ApplyParams()
{
    if (mSpectroDisplayObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
        if (spectro != NULL)
            spectro->SetRange(mRange);
        if (mSpectrogramBG != NULL)
            mSpectrogramBG->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }

    if (mSpectroDisplayObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
        if (spectro != NULL)
            spectro->SetContrast(mContrast);
        if (mSpectrogramBG != NULL)
            mSpectrogramBG->SetContrast(mContrast);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }

    if (mGraph != NULL)
    {
        SetColorMap(mColorMapNum);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }

    if (mMonitorEnabled)
    {
        if (mTimeAxis != NULL)
            mTimeAxis->Update(0.0);
    }

    UpdateWaveform();
    UpdateSpectrogramAlpha();

#if 0
    if (mCustomControl != NULL)
        mCustomControl->
            SetSelectionType((SelectionType)mSelectionType);
#endif
    
    // Not sure...
    if (mMonitorEnabled)
    {
        if (mTimeAxis != NULL)
            mTimeAxis->Update(0.0);
    }

    SetLowFreqZoom(mLowFreqZoom);
}

bool
GhostTrack2::IsControlsCreated() const
{
    if (mGraph == NULL)
        return false;

    return true;
}

GraphControl12 *
GhostTrack2::CreateControls(GUIHelper12 *guiHelper,
                            Plugin *plug,
                            IGraphics *pGraphics,
                            int graphX, int graphY,
                            const char *graphBitmapFn,
                            int offsetX, int offsetY,
                            int graphParamIdx)
{    
    mGUIHelper = guiHelper;

    mGraphics = pGraphics;
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(plug, pGraphics,
                                     graphX, graphY,
                                     graphBitmapFn, graphParamIdx);

#if USE_LEGACY_LOCK
    mGraph->SetUseLegacyLock(true);
#endif
    
    int graphWidthSmall;
    int graphHeightSmall;
    mGraph->GetSize(&graphWidthSmall, &graphHeightSmall);

    // GUIResize
    int newGraphWidth = graphWidthSmall + offsetX;
    int newGraphHeight = graphHeightSmall + offsetY;
    
    mGraph->Resize(newGraphWidth, newGraphHeight);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);
    
    // Custom drawer and control
    mCustomDrawer = new GhostCustomDrawer2(this,
                                           0.0, 0.0, 1.0, 1.0 - SPECTRO_MINI_SIZE,
                                           mCustomDrawerState);
    mCustomDrawerState = mCustomDrawer->GetState();
    
    mGraph->AddCustomDrawer(mCustomDrawer);
    
    if (mCustomControl == NULL)
        mCustomControl = new GhostCustomControl2(this);
    mGraph->AddCustomControl(mCustomControl);
	
    CreateGraphAxes();
    CreateSpectrogramDisplay(false);
    CreateGraphCurves();

    // FIX: after having re-created controls (e.g UI close ane re-open),
    // we must do this in order to have the spectrogram displayed
    // (otherwise there is only the wavefrorm)
#if FIX_MORPHO_RECREATE_CONTROLS
    // do it only in EDIT mode, because in VIEW mode, this would reset the spectrogram
    if (mMode == EDIT)
        DoRecomputeData();
#endif
    
    return mGraph;
}

void
GhostTrack2::ClearControls(IGraphics *pGraphics)
{
    // NOTE: for plugin version, do not delete the graph here!
    // It is called from Ghost::OnUIClose(), and the GL context is not bound here!
    // (would make nvg/gl errors when opening/closing Ghost plugin GUI)

    // Hack to avoid a crash
    if (mGraph != NULL)
        mGraph->ClearLinkedObjects();

    //#ifdef APP_API
    // Remove control anyway, the good way to do
    // to avoid crashes
#if FIX_REMOVE_CONTROL_MORPHO || (defined APP_API)
    if ((mGraph != NULL) && (mGraphics != NULL))
        pGraphics->RemoveControl(mGraph);
#endif

    mGraph = NULL;
}
                           
void
GhostTrack2::OnUIOpen()
{    
    UpdateTimeAxis();
}

void
GhostTrack2::OnUIClose()
{
    // Keep the previous flag before closing
    // Useful if just after, we modify edit options (e.g cut) from host ui
    if (mCustomDrawer != NULL)
        mPrevSelectionActive = mCustomDrawer->IsSelectionActive();
    mCustomDrawer = NULL;

    // Hack to avoid a crash
    if (mGraph != NULL)
        mGraph->ClearLinkedObjects();

    ClearControls(mGraphics);
    
    // mSpectrogramDisplay is a custom drawer, it will be deleted in the graph
    mSpectrogramDisplay = NULL;
    mCustomControl->SetSpectrogramDisplay(NULL);
        
    mMiniView = NULL;
    mCustomControl->SetMiniView(NULL);
 
    mGraph = NULL;
}

void
GhostTrack2::PreResizeGUI()
{
    GetGraphSize(&mPrevGraphWidth, &mPrevGraphHeight);
}

void
GhostTrack2::PostResizeGUI()
{
    if (mCustomControl != NULL)
    {
        if (mGraph != NULL)
        {
            int width;
            int height;
            mGraph->GetSize(&width, &height);
            
            mCustomControl->Resize(mPrevGraphWidth, mPrevGraphHeight,
                                   width, height);
            
            UpdateMiniViewData();
        }
    }
}

void
GhostTrack2::Init()
{
    Init(mOversampling, FREQ_RES);
}

void
GhostTrack2::Init(int oversampling, int freqRes)
{
    if (mIsInitialized)
        return;
        
    BL_FLOAT sampleRate = mSampleRate;

    if (mScale != NULL)
        delete mScale;
    mScale = new Scale();

    if (mSpectrogram == NULL)
        mSpectrogram = new BLSpectrogram4(sampleRate, mBufferSize/4, -1);
    if (mSpectrogramBG == NULL)
        mSpectrogramBG = new BLSpectrogram4(sampleRate, mBufferSize/4, -1);
    
    if (mDispFftObj == NULL)
    {
        //
        // Disp Fft obj
        //
        
        vector<ProcessObj *> dispProcessObjs;
        mSpectroDisplayObj =
            new SpectrogramFftObj2(mBufferSize, oversampling,
                                   freqRes, sampleRate,
                                   mSpectrogram);
        
#if CONSTANT_SPEED_FEATURE
        mSpectroDisplayObj->SetConstantSpeed(true);
#endif
            
        dispProcessObjs.push_back(mSpectroDisplayObj);

        // Use one channel, convert input to mono if stereo data
        int numChannelsDisplay = 1;
        int numScInputs = 0;

        int numChannelsEdit = 2;
        
        mDispFftObj = new FftProcessObj16(dispProcessObjs,
                                          numChannelsDisplay, numScInputs,
                                          mBufferSize, oversampling, freqRes,
                                          sampleRate);

#if OPTIM_SKIP_IFFT
        mDispFftObj->SetSkipIFft(-1, true);
#endif
        
#if !VARIABLE_HANNING
        mDispFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowHanning);
        mDispFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowHanning);
#else
        mDispFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowVariableHanning);
        mDispFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowVariableHanning);
#endif
        
        mDispFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                            KEEP_SYNTHESIS_ENERGY);

        // Edit Fft obj
        //
        vector<ProcessObj *> editProcessObjs;
        for (int i = 0; i < 2; i++)
        {
            mSpectroEditObjs[i] = new SpectroEditFftObj3(mBufferSize, oversampling,
                                                         freqRes, sampleRate);
            
            editProcessObjs.push_back(mSpectroEditObjs[i]);
        }

        mEditFftObj = new FftProcessObj16(editProcessObjs,
                                          numChannelsEdit, numScInputs,
                                          mBufferSize, oversampling, freqRes,
                                          sampleRate);
#if !VARIABLE_HANNING
        mEditFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowHanning);
        mEditFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowHanning);
#else
        mEditFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                       FftProcessObj16::WindowVariableHanning);
        mEditFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                        FftProcessObj16::WindowVariableHanning);
#endif
        
        mEditFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                            KEEP_SYNTHESIS_ENERGY);
        // Disp gen Fft obj
        //
        vector<ProcessObj *> dispGenProcessObjs;
        for (int i = 0; i < 2; i++)
        {
            mSpectroDispGenObjs[i] = new SpectroEditFftObj3(mBufferSize, oversampling,
                                                            freqRes, sampleRate);
            
            dispGenProcessObjs.push_back(mSpectroDispGenObjs[i]);
        }
        
        mDispGenFftObj = new FftProcessObj16(dispGenProcessObjs,
                                             numChannelsDisplay, numScInputs,
                                             mBufferSize, oversampling, freqRes,
                                             sampleRate);

#if OPTIM_SKIP_IFFT2
        mDispGenFftObj->SetSkipIFft(-1, true);
#endif
        
#if !VARIABLE_HANNING
        mDispGenFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                          FftProcessObj16::WindowHanning);
        mDispGenFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                           FftProcessObj16::WindowHanning);
#else
        mDispGenFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                          FftProcessObj16::WindowVariableHanning);
        mDispGenFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                           FftProcessObj16::WindowVariableHanning);
#endif
        
        mDispGenFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                               KEEP_SYNTHESIS_ENERGY);
    }
    else
    {
        // Disp
        mDispFftObj->Reset(mBufferSize, oversampling, freqRes, sampleRate);

        mSpectroDisplayObj->Reset(mBufferSize, oversampling,
                                  freqRes, sampleRate);

        // Edit
        mEditFftObj->Reset(mBufferSize, oversampling, freqRes, sampleRate);
        
        for (int i = 0; i < 2; i++)
            mSpectroEditObjs[i]->Reset(mBufferSize, oversampling,
                                       freqRes, sampleRate);

        // Disp gen
        mDispGenFftObj->Reset(mBufferSize, oversampling, freqRes, sampleRate);
        
        for (int i = 0; i < 2; i++)
            mSpectroDispGenObjs[i]->Reset(mBufferSize, oversampling,
                                          freqRes, sampleRate);

        //
        mSpectrogramDisplay->ResetTransform();
        
        mSpectrogramView->SetSampleRate(sampleRate);
    }
    
    // Create the spectorgram display in any case
    // (first init, oar fater a windows close/open)
    CreateSpectrogramDisplay(true);

    if (mSamplesPyramid != NULL)
        delete mSamplesPyramid;
    
    mSamplesPyramid = new SamplesPyramid3(SAMPLES_PYRAMID_MAX_LEVEL);
    
    // For spectrograms
    mOversampling = oversampling;

    // 
    mSamplesToSpectro = new GhostSamplesToSpectro2(this, EDIT_OVERLAPPING);

    //
    if (mSpectroDisplayObj != NULL)
    {
        int addStep = mSpectroDisplayObj->GetAddStep();
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetSpeedMod(addStep);
    }
        
    ModeChanged(mMode);
    
    ApplyParams();
    
    UpdateTimeAxis();
    
    mIsInitialized = true;
}

void
GhostTrack2::ProcessBlock(const vector<WDL_TypedBuf<BL_FLOAT> > &in,
                          const vector<WDL_TypedBuf<BL_FLOAT> > &scIn,
                          vector<WDL_TypedBuf<BL_FLOAT> > *out,
                          bool isTransportPlaying,
                          BL_FLOAT transportSamplePos)
{    
    if (mMode == EDIT)
    {
        vector<WDL_TypedBuf<BL_FLOAT> > dummy;
        
        // If setting the host input as input, we would have some
        // host sound additionnaly to the inner sound played
        // in edit mode
        vector<WDL_TypedBuf<BL_FLOAT> > dummyIn;
            
        if (mEditFftObj != NULL)
            mEditFftObj->Process(dummyIn, dummy, out);
        
        if (mSamples.size() == 1)
            // Mono
        {
            // Copy the mono signal for the two outputs
            // (FIX: in Reaper, external editor, mono sound is heared only on the left
            if (out->size() > 1)
                (*out)[1] = (*out)[0];
        }

        if (mIsPlaying)
            // This test will avoid unnecessary refreshes when not playing
        {
            UpdatePlayBar();
      
            if (mSpectroEditObjs[0] != NULL)
            {
                if (mSpectroEditObjs[0]->SelectionPlayFinished())
                {
                    ResetPlayBar();
                }
            }
        }
    }
    
    if (mMode == VIEW)
    {
        // Only if it is playing (otherwise the waveform will continue to scroll
        // after playback stop, on Protools)
        if (isTransportPlaying || mMonitorEnabled)
        {
            vector<WDL_TypedBuf<BL_FLOAT> > dummy;
            if (mDispFftObj != NULL)
            {
                vector<WDL_TypedBuf<BL_FLOAT> > &mono = mTmpBuf32;
                mono.resize(1);
                BLUtils::StereoToMono(&mono[0], in);

                mDispFftObj->Process(mono, dummy, out);

                // Bypass
                *out = in;
            }
        
            mMustUpdateSpectrogram = true;
            mMustUpdateBGSpectrogram = true;
                
#if VIEW_MODE_DISPLAY_WAVE
            WDL_TypedBuf<BL_FLOAT> &monoIn = mTmpBuf35;
            BLUtils::StereoToMono(&monoIn, in);
                
            // Clip the waveform in case it is greater than 1
            // FIX: avoids waveform that looks right and sounds right
            // in Ghost-X, but that clips after exported and imported in another
            // software
            //
            // Make a copy of in, because we don't want to clip in view mode
            // (because we bypass)

#if CLIP_WAVEFORM
            ClipWaveform(&monoIn);
#endif
            
            int numSpectroCols = 0;
            int spectroHeight = 0;
            
            BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
            if (spectro != NULL)
            {
                numSpectroCols = spectro->GetMaxNumCols();
                spectroHeight = spectro->GetHeight();
            }

            bool samplesPyramidChanged = false;
            if ((numSpectroCols > 0) && (spectroHeight > 0))
            {
                // How many points must we keep ?
                int numCurveValues = spectroHeight*numSpectroCols;
            
                if (!in.empty())
                {
                    if (mSamplesQueue.empty()) 
                        // Fist time
                    {
                        // Initialize with zeros
                        //
                        
                        mSamplesQueue.resize(1);
                        
                        BLUtils::ResizeFillZeros(&mSamplesQueue[0], numCurveValues);

                        FileChannelQueueToVec();

                        // Add the zeros to the pyramid
                        SetSamplesPyramidSamples();

                        // Manage latency
                        AdjustSamplesPyramidLatencyViewMode();

                        samplesPyramidChanged = true;
                    }
                    
                    if (!mSamplesQueue.empty())
                    {
                        int nFrames = in[0].GetSize();
                        mSamplesQueue[0].Add(monoIn.Get(), nFrames);
                    
                        // Keep as many samples to fill the graph
                        int numToConsume =
                            mSamplesQueue[0].Available() - numCurveValues;
                
                        if (numToConsume > 0)
                            BLUtils::ConsumeLeft(&mSamplesQueue[0], numToConsume);
                            
#if CONSTANT_SPEED_FEATURE
                        int step = 1;
                            
                        BL_FLOAT sampleRate = mSampleRate;
                        BL_FLOAT srCoeff = sampleRate/44100.0;
                        srCoeff = round(srCoeff);
                        step *= srCoeff;

                        WDL_TypedBuf<BL_FLOAT> &monoInDecim = mTmpBuf31;
                        BLUtilsDecim::DecimateStep(monoIn, &monoInDecim, step);
                          
                        numToConsume /= step;
#endif
                            
                        // Samples pyramid
                        long numPushed = 0;
                        if (mSamplesPyramid != NULL)
                            numPushed = mSamplesPyramid->PushValues(monoInDecim);

                        if (numPushed > 0)
                            samplesPyramidChanged = true;
                        
                        mNumToPopTotal += numToConsume;

#if 1 // New: works with SamplesPyramid3::FIX_PUSH_POP_POW_TWO
                        if (mNumToPopTotal > 0)
                        {
                            if (numPushed > 0)
                            {
                                mSamplesPyramid->PopValues(mNumToPopTotal);
                                mNumToPopTotal = 0;
                            }
                        }
#endif
                        
#if 0 // Origin: BUG with block size e.g 32
                        if (numToConsume > 0)
                        {
                            if (numPushed > 0)
                                mSamplesPyramid->PopValues(numToConsume);
                        }
#endif
                    }
                }
            }

            // A bit artificial test, but it optimizes!
            if (samplesPyramidChanged) // Optim test for bs=32
                FileChannelQueueToVec();

            // Normal test (SamplesPyramid => Waveform)
            if (samplesPyramidChanged) // Optim test for bs=32
                UpdateWaveform();
                
            mTransportSamplePos = transportSamplePos;
            UpdateTimeAxis();
        }
#endif
    }
    
    if (mMode == ACQUIRE)
    {
#if !DEBUG_MEMORY
        if (isTransportPlaying)
#endif
        {
            if (mSpectrogramDisplay != NULL)
                mSpectrogramDisplay->ClearBGSpectrogram();
            
            vector<WDL_TypedBuf<BL_FLOAT> > dummy;
            if (mDispFftObj != NULL)
            {
                vector<WDL_TypedBuf<BL_FLOAT> > &mono = mTmpBuf33;
                mono.resize(1);
                BLUtils::StereoToMono(&mono[0], in);
                
                mDispFftObj->Process(mono, dummy, out);

                // Bypass
                *out = in;
            }
            
            mMustUpdateSpectrogram = true;
            mMustUpdateBGSpectrogram = true;
        }

#if CLIP_WAVEFORM
        // Clip the waveform in case it is greater than 1
        // FIX: avoids waveform that looks right and sounds right
        // in Ghost-X, but that clips after exported and imported in another
        // software
        for (int i = 0; i < in.size(); i++)
        {
            ClipWaveform(&in[i]);
        }
#endif
        
#if !DEBUG_MEMORY
        if (isTransportPlaying)
#endif
        {
            vector<WDL_TypedBuf<BL_FLOAT> > dummyOut;
            vector<WDL_TypedBuf<BL_FLOAT> > dummy;
                
            // Fill the edit obj with the data
            // Then after, when switching to edit mode,
            // we will have the data !
            if (mEditFftObj != NULL)
                mEditFftObj->Process(in, dummy, &dummyOut);
            
            // Adapt the buffer to the number of inputs
            // This will be used after, if we switch to edit or render mode
            // Then we will need to know the number of channels to generate them
            if (mSamplesQueue.size() != in.size())
                mSamplesQueue.resize(in.size());
            
            for (int i = 0; i < mSamplesQueue.size(); i++)
            {
                mSamplesQueue[i].Add(in[i].Get(), in[i].GetSize());
            }
                
            // Update the pyramid
            bool samplesPyramidChanged = false;
            if (mSamplesPyramid != NULL)
            {
                WDL_TypedBuf<BL_FLOAT> &mono = mTmpBuf34;
                BLUtils::StereoToMono(&mono, in);
                
                int numPushedSamples = mSamplesPyramid->PushValues(mono);
                if (numPushedSamples > 0)
                    samplesPyramidChanged = true;
            }
            
            // Set samples pointers to spectro edit objs
            UpdateSpectroEditSamples();

            FileChannelQueueToVec();
            
            // Update the waveform
            if (mSpectrogramView != NULL)
                mSpectrogramView->SetSamples(&mSamples);
                
#if 1 // This is too slow to update the waveform in real time when acquiring
      // (we must decimate many samples if we acquire long parts)
      // So just don't display it while acquiring !
            if (samplesPyramidChanged)
                UpdateWaveform();
#endif
            
            mTransportSamplePos = transportSamplePos;
            UpdateTimeAxis();
        }
    }
      
    if (mMode == RENDER)
    {
        if (isTransportPlaying)
        {
            vector<WDL_TypedBuf<BL_FLOAT> > dummy;
            if (mEditFftObj != NULL)
                mEditFftObj->Process(in, dummy, out);
          
            UpdatePlayBar();

            // update time axis only when playing
            // To avoid unnecessary graph ref
            mTransportSamplePos = transportSamplePos;
            UpdateTimeAxis();
        }
    }

    // In App mode, the spectrogram doesn't change when playing with ProcessBlock()
    // So save some resource by not recomputing the spectrogram
    bool appAPI = false;
#ifdef APP_API
    appAPI = true;
#endif
    if (!(appAPI || mAppAPIHack))
    {    
        if (isTransportPlaying || mMonitorEnabled)
            // With this test, this avoids unnecessary refreshes
            //in View and Capture modes
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
            }
            else
            {
                // Do not update the spectrogram texture
                if (mSpectrogramDisplay != NULL)
                {
                    mSpectrogramDisplay->UpdateSpectrogram(false);
                }
            }
        }
    }
}

void
GhostTrack2::UpdateParamRange(BL_FLOAT range)
{
    mRange = range;

    if (mSpectroDisplayObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
        if (spectro != NULL)
            spectro->SetRange(mRange);
        if (mSpectrogramBG != NULL)
            mSpectrogramBG->SetRange(mRange);
        
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }
}

void
GhostTrack2::UpdateParamContrast(BL_FLOAT contrast)
{
    mContrast = contrast;

    if (mSpectroDisplayObj != NULL)
    {
        BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
        if (spectro != NULL)
            spectro->SetContrast(mContrast);
        if (mSpectrogramBG != NULL)
            mSpectrogramBG->SetContrast(mContrast);
            
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateSpectrogram(false);
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }
}

void
GhostTrack2::UpdateParamColorMap(int colorMapNum)
{
    mColorMapNum = colorMapNum;

    if (mGraph != NULL)
    {
#if GHOST_LITE_VERSION && !GHOST_LITE_ALL_COLORMAPS
        if (colorMapNum > 3)
            colorMapNum = 0;
#endif
        SetColorMap(mColorMapNum);
            
        if (mSpectrogramDisplay != NULL)
        {
            mSpectrogramDisplay->UpdateColorMap(true);
        }
    }
}

void
GhostTrack2::UpdateParamWaveformScale(BL_FLOAT waveformScale)
{
    mWaveformScale = waveformScale;

    UpdateWaveform();
}

void
GhostTrack2::UpdateParamSpectWaveformRatio(BL_FLOAT spectWaveformRatio)
{
    mSpectWaveformRatio = spectWaveformRatio;

    UpdateWaveform();
    UpdateSpectrogramAlpha();
}

void
GhostTrack2::UpdateParamMode(int mode)
{
    mMode = mode;
}

GhostTrack2::Mode
GhostTrack2::GetMode()
{
    return (GhostTrack2::Mode)mMode;
}

void
GhostTrack2::UpdateParamMonitorEnabled(bool monitorEnabled)
{
    mMonitorEnabled = monitorEnabled;

    if (mMonitorEnabled)
    {
        if (mTimeAxis != NULL)
            mTimeAxis->Update(0.0);
    }
}

void
GhostTrack2::UpdateParamSelectionType(int selectionType)
{
    //mSelectionType = selectionType;

    if (mCustomControl != NULL)
        mCustomControl->
            SetSelectionType((SelectionType)selectionType);
}

void
GhostTrack2::UpdateWaveform()
{
    if (mSpectrogramDisplay == NULL)
        return;
    if (mSpectrogramView == NULL)
        return;
    
#define EPS 1e-3
    if (mSpectWaveformRatio < EPS)
        // We don't display the waveform, so don't compute it
    {
        if (mWaveformCurve != NULL)
            mWaveformCurve->ClearValues();
        if (mWaveformOverlayCurve != NULL)
            mWaveformOverlayCurve->ClearValues();
      
        return;
    }
    
    BL_FLOAT waveScale = (mWaveformScale/mWaveformMax)*DEFAULT_WAVEFORM_SCALE;
    if (waveScale < EPS)
    {
        if (mWaveformCurve != NULL)
            mWaveformCurve->ClearValues();
        if (mWaveformOverlayCurve != NULL)
            mWaveformOverlayCurve->ClearValues();
      
        return;
    }
    
    // Quick fix for shift between waveform and spectrogram
    // (due to latency)
    // => this is not super-accurate, but it's better
    //#define WAVEFORM_LATENCY -BUFFER_SIZE/2.0

    // NOTE: Modified after pyramid
    // Latency is not useful anymore when using samples pyramid
    //
    // FIX: If we kept latency, when editing, the modified waveform is
    // outside the selection (when zooming at ~50ms level)
    //
#define WAVEFORM_LATENCY 0.0
    
    if (mSamples.empty())
    {
        if (mWaveformCurve != NULL)
            mWaveformCurve->ClearValues();
      
        if (mWaveformOverlayCurve != NULL)
            mWaveformOverlayCurve->ClearValues();
      
        return;
    }
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    BL_FLOAT startDataPos;
    BL_FLOAT endDataPos;
    mSpectrogramView->GetViewDataBounds(&startDataPos, &endDataPos,
                                        minNormX, maxNormX);
    
    startDataPos *= mBufferSize;
    endDataPos *= mBufferSize;
    
    startDataPos += WAVEFORM_LATENCY;
    endDataPos += WAVEFORM_LATENCY;
    
    //WDL_TypedBuf<BL_FLOAT> &samples = mTmpBuf8;
        
    BL_FLOAT waveformAlpha = 1.0;
    if (mSpectWaveformRatio < 0.5)
    {
        waveformAlpha = mSpectWaveformRatio*2.0;
    }
    
    // Then update the graph
    if (mWaveformCurve != NULL)
        mWaveformCurve->SetAlpha(waveformAlpha);
    if (mWaveformOverlayCurve != NULL)
        mWaveformOverlayCurve->SetAlpha(waveformAlpha);
    
    int numCurveValues = GRAPH_CONTROL_NUM_POINTS;
    WDL_TypedBuf<BL_FLOAT> &decimValues = mTmpBuf4;
    if (mSamplesPyramid != NULL)
        mSamplesPyramid->GetValues(startDataPos, endDataPos,
                                   numCurveValues, &decimValues);
    
    // Mult values after, for optimization
    BLUtils::MultValues(&decimValues, waveScale);

    // Clip curve values, so the main waveform won't be displayed over the miniview
    if (mMiniView != NULL)
        BLUtils::ClipMin(&decimValues, MINIVIEW_CLIP_CURVE_VALUE);
    
    if (mWaveformCurve != NULL)
        mWaveformCurve->SetValues4(decimValues);
    if (mWaveformOverlayCurve != NULL)
        mWaveformOverlayCurve->SetValues4(decimValues);
}

void
GhostTrack2::UpdateSpectrogramAlpha()
{
    BL_FLOAT spectAlpha = 1.0;
    if (mSpectWaveformRatio > 0.5)
    {
        spectAlpha = 1.0 - (mSpectWaveformRatio - 0.5)*2.0;
    }

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetAlpha(spectAlpha);
}

void
GhostTrack2::GetGraphSize(int *width, int *height)
{
    if (mGraph != NULL)
    {
        mGraph->GetSize(width, height);
    }
}

void
GhostTrack2::SetGraphEnabled(bool flag)
{
    if (mGraph != NULL)
    {
        mGraph->SetEnabled(flag);
        mGraph->SetInteractionDisabled(!flag);

        // To refresh well when changed track 
        if (flag)
            mGraph->SetDataChanged();
    }
}

void
GhostTrack2::ResetAllData(bool callReset)
{
    mSamples.resize(0);
    mSamplesQueue.resize(0);
    
    if (mSamplesPyramid != NULL)
        mSamplesPyramid->Reset();
    
    // Reset spectrogram
    if (callReset)
        Reset();
    
    if (mSpectrogramDisplay != NULL)
    {
        // Also update "full" data, i.e background spectrogram
        mSpectrogramDisplay->UpdateSpectrogram(true, true);
    }
}

const char *
GhostTrack2::GetCurrentFileName()
{
    return mCurrentFileName;
}

bool
GhostTrack2::GetFileModified()
{
    return mFileModified;
}

void
GhostTrack2::SetFileModified(bool flag)
{
    mFileModified = flag;
}

bool
GhostTrack2::OpenFile(const char *fileName)
{
    mIsLoadingSaving = true;
    
    // Clear previous loaded file if any
    mSamples.clear();
    
    if (mSamplesPyramid != NULL)
        mSamplesPyramid->Reset();
    
    // Set samples pointers to spectro edit objs
    UpdateSpectroEditSamples();
    
    // Open the audio file
    BL_FLOAT sampleRate = mSampleRate;
    AudioFile *audioFile = AudioFile::Load(fileName, &mSamples);
    if (audioFile == NULL)
    {
        mIsLoadingSaving = false;
        
        return false;
    }

#if CLIP_WAVEFORM
    // Clip the waveform in case it is greater than 1
    // FIX: avoids waveform that looks right and sounds right
    // in Ghost-X, but that clips after exported and imported in another software
    for (int i = 0; i < mSamples.size(); i++)
    {
        ClipWaveform(&mSamples[i]);
    }
#endif
    
    mInternalFileFormat = audioFile->GetInternalFormat();
    
    // Store the file name (for later "save")

    // valgrind warning (this can be dangerous if fileName==mCurrentFileName)
    if (fileName != mCurrentFileName)
        sprintf(mCurrentFileName, "%s", fileName);
    
    mFileModified = false;
    //UpdateWindowTitle();

    // Resample() failed e.g file sr=44100Hz, Ghost-X app sr=88200Hz 
    //audioFile->Resample(sampleRate);
    audioFile->Resample2(sampleRate);

#if 0 // Debug
    int numChannels = audioFile->GetNumChannels();
#endif
    
    delete audioFile;

    UpdateAfterNewFile();
    
    mIsLoadingSaving = false;

    return true;
}

bool
GhostTrack2::OpenCurrentFile()
{
    // Reset some stuffs
    //
    
    ResetAllData();
    
    // Clear previous command history
    // (just in case it remained commands in the history)
    mCommandHistory->Clear();
    
    bool res = OpenFile(mCurrentFileName);

    // NEW
    RewindView();
    
    return res;
}

bool
GhostTrack2::SaveFile(const char *fileName, bool updateFileName)
{
    mIsLoadingSaving = true;
    
    // Added reference, to save memory
    vector<WDL_TypedBuf<BL_FLOAT> > &saveChannels = mSamples;
    
    int numChannels = (int)saveChannels.size();
    if (numChannels == 0)
    {
        mIsLoadingSaving = false;
        
        return false;
    }
    
    // Create the audio file
    BL_FLOAT sampleRate = mSampleRate;
    AudioFile *audioFile = new AudioFile(numChannels, sampleRate,
                                         mInternalFileFormat);
    
    // Save here
    for (int i = 0; i < saveChannels.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &chan = saveChannels[i];
        audioFile->SetData(i, chan, mLoadedChannelSize);

        audioFile->FixDataBounds();
    }
    
    /*bool saved =*/ audioFile->Save(fileName);
    
    delete audioFile;

    // Store the file name (for later "save")
    if (updateFileName)
    {
        sprintf(mCurrentFileName, "%s", fileName);
        mFileModified = false;
    }
    
    mIsLoadingSaving = false;
    
    return true;
}

bool
GhostTrack2::SaveCurrentFile()
{
    bool res = SaveFile(mCurrentFileName);

    return res;
}

bool
GhostTrack2::SaveFile(const char *fileName,
                      const vector<WDL_TypedBuf<BL_FLOAT> > &channels,
                      bool updateFileName)
{
    if (strlen(fileName) < 2)
        return false;

    int numChannels = (int)channels.size();
    if (numChannels == 0)
        return false;

    // Store the file name (for later "save")
    if (updateFileName)
        sprintf(mCurrentFileName, "%s", fileName);
    
    // Create the audio file
    BL_FLOAT sampleRate = mSampleRate;
    AudioFile *audioFile = new AudioFile(numChannels, sampleRate,
                                         mInternalFileFormat);
    
    // Save here
    for (int i = 0; i < channels.size(); i++)
    {
        const WDL_TypedBuf<BL_FLOAT> &chan = channels[i];
        long channelSize = chan.GetSize();
        audioFile->SetData(i, chan, channelSize);

        audioFile->FixDataBounds();
    }
    
    /*bool saved =*/ audioFile->Save(fileName);
    
    delete audioFile;

    return true;
}

bool
GhostTrack2::ExportSelection(const char *fileName)
{
    if ((mSamplesToSpectro == NULL) ||
        (mSpectroEditObjs[0] == NULL))
    {
        return false;
    }
    
    // No selection ? => save all
    if (!IsSelectionActive())
    {
        bool success = SaveFile(fileName, false);
    
        return success;
    }

    BL_FLOAT spectroEditSels[4];
    mSpectroEditObjs[0]->GetNormSelection(spectroEditSels);

    BL_FLOAT minXNorm = spectroEditSels[0];
    BL_FLOAT maxXNorm = spectroEditSels[2];
        
    vector<WDL_TypedBuf<BL_FLOAT> > samples;
    mSamplesToSpectro->ReadSelectedSamples(&samples, minXNorm, maxXNorm);

    // Fade to avoid clicks at the beginning and end
    //
#define FADE_NUM_SAMPLES 64 //100 //10
    BL_FLOAT fadeT = ((BL_FLOAT)FADE_NUM_SAMPLES)/samples[0].GetSize();
    if (fadeT < 1.0)
    {
        for (int i = 0; i < samples.size(); i++)
        {
            BLUtilsFade::Fade(&samples[i], 0.0, fadeT, true);
            BLUtilsFade::Fade(&samples[i], 1.0 - fadeT, 1.0, false);
        }
    }
    
    // Save
    bool success = SaveFile(fileName, samples, false);

    return success;
}

void
GhostTrack2::CloseFile()
{
    mIsLoadingSaving = true;
    
    mSamples.clear();
    
    if (mSamplesPyramid != NULL)
        mSamplesPyramid->Reset();

    UpdateSpectroEditSamples();

    UpdateAfterNewFile();
    
    memset(mCurrentFileName, '\0', FILENAME_SIZE);
    mFileModified = false;

    mIsLoadingSaving = false;
}

int
GhostTrack2::GetNumChannels()
{
    int res = (int)mSamples.size();

    return res;
}

int
GhostTrack2::GetNumSamples()
{
    if (mSamples.empty())
        return 0;

    int res = mSamples[0].GetSize();

    return res;
}

BL_FLOAT
GhostTrack2::GetBarPos()
{
    if (mGraph == NULL)
        return 0.0;
    if (mCustomDrawer == NULL)
        return 0.0;

    BL_FLOAT pos = mCustomDrawer->GetBarPos();

    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    pos *= width;
    
    return pos;
}

void
GhostTrack2::SetBarPos(BL_FLOAT x)
{
    if (mGraph == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    // Custom drawer
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    bool barActive = mCustomDrawer->IsBarActive();
    
    BL_FLOAT xf = ((BL_FLOAT)x)/width;
    mCustomDrawer->SetBarPos(xf);
    
    // Spectrogram view
    if (mSpectrogramView != NULL)
    {
        mSpectrogramView->SetViewBarPosition(xf);
        mSpectrogramView->ClearViewSelection();
    }
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetCenterPos(xf);
    
    if (barActive)
        BarSetSelection(x);
    
    // Set the center correctly (because it was set to the
    // center of selection by UpdateSelection() )
    if (mSpectrogramView != NULL)
        mSpectrogramView->SetViewBarPosition(xf);
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->SetCenterPos(xf);
    
    // But don't display this selection in the drawer
    // (we still have the bar displayed)
    if (mCustomDrawer != NULL)
        mCustomDrawer->ClearSelection();
    
    // Update for playing
    UpdateSpectroEdit();
}

void
GhostTrack2::ClearBar()
{
    if (mCustomDrawer == NULL)
        return;
    
    mCustomDrawer->ClearBar();
}

void
GhostTrack2::StartPlay()
{
    for (int i = 0; i < 2; i++)
    {
        if (mSpectroEditObjs[i] == NULL)
            continue;

        // Reset the FftObj, so we won't play some few previously buffered sound
        mEditFftObj->Reset();
            
        mSpectroEditObjs[i]->RewindToStartSelection();
        
        if (mMode != RENDER)
            mSpectroEditObjs[i]->SetMode(SpectroEditFftObj3::PLAY);
        else
            mSpectroEditObjs[i]->SetMode(SpectroEditFftObj3::PLAY_RENDER);
            
    }

    mIsPlaying = true;
}

void
GhostTrack2::StopPlay()
{    
    for (int i = 0; i < 2; i++)
    {
        if (mSpectroEditObjs[i] == NULL)
            continue;
        
        mSpectroEditObjs[i]->SetMode(SpectroEditFftObj3::BYPASS);
        
        // Set the play bar to origin
        mSpectroEditObjs[i]->RewindToStartSelection();
    }

    mIsPlaying = false;
}

bool
GhostTrack2::IsPlaying()
{
    return mIsPlaying;
}

bool
GhostTrack2::PlayStarted()
{    
    if (mSpectroEditObjs[0] == NULL)
        return false;
    
    bool res = ((mSpectroEditObjs[0]->GetMode() == SpectroEditFftObj3::PLAY) ||
                (mSpectroEditObjs[0]->GetMode() == SpectroEditFftObj3::PLAY_RENDER));
    
    return res;
}

void
GhostTrack2::ResetPlayBar()
{
    // Set the play bar to origin
    for (int i = 0; i < 2; i++)
    {
        if (mSpectroEditObjs[i] != NULL)
            mSpectroEditObjs[i]->RewindToStartSelection();
    }
}

void
GhostTrack2::UpdatePlayBar()
{
    if (mSpectroEditObjs[0] == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    if (mSamples.empty())
    {
        return;
    }
    
    // Set play bar pos every time
    // because when we stop playing, we want
    // to retrive it at the origin
    BL_FLOAT playPos = mSpectroEditObjs[0]->GetSelPlayPosition();
    if (mCustomDrawer != NULL)
        mCustomDrawer->SetSelPlayBarPos(playPos);
}

void
GhostTrack2::BarSetSelection(int x)
{
    if (mGraph == NULL)
        return;
    
    if (mSpectrogramDisplay == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    // Update the selection, to be able to play
    // from the bar
    // (choose everything after the bar)
    
    // Set the end selection to the end of the data
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    BL_FLOAT normViewWidth = maxNormX - minNormX;
    
    BL_FLOAT endSelection = ((BL_FLOAT)width)/normViewWidth;
    
    int selection[4] = { x, 0, (int)endSelection,
                         (int)((1.0 - SPECTRO_MINI_SIZE)*height) };
    UpdateSelection(selection[0], selection[1],
                    selection[2], selection[3],
                    false);
    
    if (mCustomControl != NULL)
        mCustomControl->SetSelection(selection[0], selection[1],
                                     selection[2], selection[3]);
    
    if (mCustomDrawer != NULL)
    {
        BL_FLOAT selectionDrawer[4] = { ((BL_FLOAT)x)/width, 0, 1.0, 1.0 };
        mCustomDrawer->SetSelection(selectionDrawer[0], selectionDrawer[1],
                                    selectionDrawer[2], selectionDrawer[3]);
    }
}

bool
GhostTrack2::IsBarActive()
{
    if (mSpectrogramDisplay == NULL)
        return false;
    if (mCustomDrawer == NULL)
        return false;
    
    bool barActive = mCustomDrawer->IsBarActive();
    
    return barActive;
}

void
GhostTrack2::SetBarActive(bool flag)
{
    if (mCustomDrawer != NULL)
        mCustomDrawer->SetBarActive(flag);
}

BL_FLOAT
GhostTrack2::GetNormDataBarPos()
{
    if (mCustomDrawer == NULL)
        return 0.0;
    if (mSpectrogramDisplay == NULL)
        return 0.0;
    
    BL_FLOAT barPos = mCustomDrawer->GetBarPos();
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    barPos = barPos*(maxNormX - minNormX) + minNormX;
    
    return barPos;
}

void
GhostTrack2::SetDataBarPos(BL_FLOAT barPos)
{
    if (mGraph == NULL)
        return;
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    barPos = (barPos - minNormX)/(maxNormX - minNormX);
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    barPos *= width;
    
    SetBarPos(barPos);
}

bool
GhostTrack2::PlayBarOutsideSelection()
{
    if (mSpectroEditObjs[0] == NULL)
        return false;
    
    BL_FLOAT playPos = mSpectroEditObjs[0]->GetPlayPosition();
    BL_FLOAT normSelection[4];
    mSpectroEditObjs[0]->GetNormSelection(normSelection);
    if ((playPos < normSelection[0]) ||
        (playPos > normSelection[2]))
        return true;
    
    return false;
}

void
GhostTrack2::RewindView()
{
    if (mSpectrogramDisplay == NULL)
        return;
    if (mSpectrogramView == NULL)
        return;
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    
    // Rewind
#if 0 // Rewind only translation
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    BL_FLOAT size = maxNormX - minNormX;
    minNormX = 0.0;
    maxNormX = size;
#endif

#if 1 // Rewind translation and also reset zoom
    minNormX = 0.0;
    maxNormX = 1.0;

    ResetTransforms();

    // Refresh the spectrogram instantly
    DoRecomputeData();
#endif
    
    mSpectrogramDisplay->SetVisibleNormBounds(minNormX, maxNormX);
    mSpectrogramDisplay->ResetTranslation();
    
    BL_FLOAT trans = mSpectrogramView->GetTranslation();
    mSpectrogramView->Translate(-trans);
    
    // FIX (1/2): select, dble click miniview, play => the play didn't start
    SetBarActive(true);
    
    SetBarPos(0.0);
    
    // FIX (2/2)
    ResetPlayBar();
    UpdatePlayBar();
    
    // FIX: after view reset, had to click one time to put the bar before
    // beeing able to draw a selection
    // (and play bar moved strangely)
    mCustomControl->SetSelectionActive(false);
    if (mCustomDrawer != NULL)
        mCustomDrawer->SetSelectionActive(false);
    
    // Call this to "refresh"
    // Avoid jumps of the background when starting translation
    UpdateZoom(1.0);
    
    UpdateSpectroEdit();
    UpdateMiniView();
    UpdateWaveform();
    
    UpdateSpectrogramData();
    
    SetNeedRecomputeData(true);
}

void
GhostTrack2::Translate(int dX)
{
    if (mGraph == NULL)
        return;
    if (mSpectrogramView == NULL)
        return;
    if (mSpectrogramDisplay == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    BL_FLOAT tX = ((BL_FLOAT)dX)/width;
    
    // Get sum of all the translations
    mSpectrogramView->Translate(tX);
    BL_FLOAT trans = mSpectrogramView->GetTranslation();
    
    // Translate the spectrogram
    bool inBounds = mSpectrogramDisplay->SetTranslation(trans);
    if (!inBounds)
    {
        // Revert the view translation...
        mSpectrogramView->Translate(-tX);
        
        // ... and abort
        return;
    }
    
    // Save
    bool barActive = mCustomDrawer->IsBarActive();
    bool selectionActive = mCustomDrawer->IsSelectionActive();
    
    // Get the sselection before changing the bar pos
    BL_FLOAT selection[4];
    mCustomDrawer->GetSelection(&selection[0], &selection[1],
                                &selection[2], &selection[3]);
    
    // Bar
    BL_FLOAT barPos = mCustomDrawer->GetBarPos();
    BL_FLOAT newBarPos = barPos*width + dX;
    SetBarPos(newBarPos);
    
    // Selection
    selection[0] = selection[0]*width + dX;
    selection[1] = selection[1]*height;
    
    selection[2] = selection[2]*width + dX;
    selection[3] = selection[3]*height;
            
    UpdateSelection(selection[0], selection[1],
                    selection[2], selection[3],
                    !barActive);
    
    mCustomControl->SetSelection(selection[0], selection[1],
                                 selection[2], selection[3]);
                                 
    // Restore
    if (!barActive)
        mCustomDrawer->ClearBar();
    
    if (!selectionActive)
    {
        mCustomDrawer->ClearSelection();
    }
    else
    {
        mCustomDrawer->SetSelectionActive(true);
    }
    
    // Call this to "refresh"
    // Avoid jumps of the background when starting translation
    UpdateZoom(1.0);
    
    UpdateWaveform();
    
    UpdateTimeAxis();
}

void
GhostTrack2::UpdateZoom(BL_FLOAT zoomChange)
{
    if (mGraph == NULL)
        return;
    
    if (mIsUpdatingZoom)
        return;
    
    mIsUpdatingZoom = true;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
#if ZOOM_ON_POINTER
    BL_FLOAT normDataBarPos = GetNormDataBarPos();
    
    BL_FLOAT dataSelection[4];
    bool dataSelectionOK = GetDataSelection(dataSelection);
#endif
    
    bool zoomDone = mSpectrogramView->UpdateZoomFactor(zoomChange);
    if (zoomDone)
        // Apply the new zoom
    {
        if ((mSpectrogramView != NULL) && (mSpectrogramDisplay != NULL))
        {
            UpdateZoomAdjust();
            
            BL_FLOAT zoom = mSpectrogramView->GetZoomFactor();           
            mSpectrogramDisplay->SetZoom(zoom);
            
            BL_FLOAT absZoom = mSpectrogramView->GetAbsZoomFactor();
            mSpectrogramDisplay->SetAbsZoom(absZoom);
        }
        
        // Zoom the current selection
        bool isBarActive = false;
        if ((mCustomDrawer != NULL) && (mCustomControl != NULL))
        {
            mCustomDrawer->UpdateZoom(zoomChange);
            mCustomControl->UpdateZoom(zoomChange);
            
            isBarActive = mCustomDrawer->IsBarActive();
            
            // Refresh selection, to get correct data selection
            // (to get correct end of playing)
            BL_FLOAT selection[4];
            mCustomDrawer->GetSelection(&selection[0], &selection[1],
                                        &selection[2], &selection[3]);
            selection[0] *= width;
            selection[1] *= height;
            selection[2] *= width;
            selection[3] *= height;
            
            UpdateSelection(selection[0], selection[1],
                            selection[2], selection[3],
                            !isBarActive);
            
            if (isBarActive)
            {
#if ZOOM_ON_POINTER
                SetDataBarPos(normDataBarPos);
#endif
            }
        }
        
#if ZOOM_ON_POINTER
        // NOTE: maybe remove some duplicate code above
        if (dataSelectionOK)
            SetDataSelection(dataSelection);
        
        if (mSpectrogramView != NULL)
        {
            BL_FLOAT viewSel[4];
            mSpectrogramView->GetViewSelection(&viewSel[0], &viewSel[1],
                                               &viewSel[2], &viewSel[3]);

            viewSel[0] *= width;
            viewSel[1] *= height;
            viewSel[2] *= width;
            viewSel[3] *= height;
            
            UpdateSelection(viewSel[0], viewSel[1], viewSel[2], viewSel[3],
                            !isBarActive, false, true);
        }
        
#endif
    }
    
    mRecomputeDataTimer.Reset();
    mRecomputeDataTimer.Start();
    
    UpdateWaveform();
    
    UpdateTimeAxis();
    
    mIsUpdatingZoom = false;
}

void
GhostTrack2::SetZoomCenter(int x)
{    
    if (mGraph == NULL)
        return;
    if (mSpectrogramDisplay == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    BL_FLOAT xf = ((BL_FLOAT)x)/width; // This fixes selection drift bug
    mSpectrogramDisplay->SetCenterPos(xf);
}

void
GhostTrack2::UpdateZoomAdjust(bool updateBGZoom)
{
    if ((mSpectrogramView == NULL) || (mSpectrogramDisplay == NULL))
        return;
        
    BL_FLOAT zoomAdjustZoom;
    BL_FLOAT zoomAdjustOffset;
    mSpectrogramView->GetZoomAdjust(&zoomAdjustZoom, &zoomAdjustOffset);
    mSpectrogramDisplay->SetZoomAdjust(zoomAdjustZoom, zoomAdjustOffset);

    if (updateBGZoom)
        mSpectrogramDisplay->SetZoomAdjustBG(zoomAdjustZoom, zoomAdjustOffset);
}

bool
GhostTrack2::GetNormDataSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                  BL_FLOAT *x1, BL_FLOAT *y1)
{
    if (mSpectrogramView == NULL)
        return false;
    
    bool res = mSpectrogramView->GetNormDataSelection(x0, y0, x1, y1);

    return res;
}

BL_FLOAT
GhostTrack2::GetNormCenterPos()
{
    if (mSpectrogramView == NULL)
        return 0.0;
    
    BL_FLOAT selection[4];
    bool res = mSpectrogramView->GetNormDataSelection(&selection[0], &selection[1],
                                                      &selection[2], &selection[3]);
    if (!res)
        return 0.0;
    
    // dummy
    return selection[0];
}

void
GhostTrack2::SetDataSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1)
{
    if (mGraph == NULL)
        return;
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);

    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    BL_FLOAT viewSelX0 = (x0 - minNormX)/(maxNormX - minNormX);
    BL_FLOAT viewSelX1 = (x1 - minNormX)/(maxNormX - minNormX);
    
    // Set the rectangular selection
    UpdateSelection(viewSelX0*width, (1.0 - y0)*height,
                    viewSelX1*width, (1.0 - y1)*height,
                    false, true, true);
}

bool
GhostTrack2::GetDataSelection(BL_FLOAT dataSelection[4])
{
    if (mSpectrogramDisplay == NULL)
        return false;
    if (mSpectrogramView == NULL)
        return false;
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    // Spectro edit
    BL_FLOAT dataX0;
    BL_FLOAT dataY0;
    BL_FLOAT dataX1;
    BL_FLOAT dataY1;
    bool dataSelectionOk =
        mSpectrogramView->GetDataSelection2(&dataX0, &dataY0,
                                            &dataX1, &dataY1,
                                            minNormX, maxNormX);

    if (!dataSelectionOk)
        return false;
    
    dataSelection[0] = dataX0;
    dataSelection[1] = dataY0;
    dataSelection[2] = dataX1;
    dataSelection[3] = dataY1;
    
    return true;
}

void
GhostTrack2::SetDataSelection(const BL_FLOAT dataSelection[4])
{
    if (mSpectrogramDisplay == NULL)
        return;
    if (mSpectrogramView == NULL)
        return;
    
    BL_FLOAT minNormX;
    BL_FLOAT maxNormX;
    mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    mSpectrogramView->SetDataSelection2(dataSelection[0], dataSelection[1],
                                        dataSelection[2], dataSelection[3],
                                        minNormX, maxNormX);
}

void
GhostTrack2::UpdateSpectroEdit()
{
    if (mSpectrogramView == NULL)
        return;
    
    BL_FLOAT minNormX = 0.0;
    BL_FLOAT maxNormX = 1.0;
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    // Spectro edit
    BL_FLOAT dataX0;
    BL_FLOAT dataY0;
    BL_FLOAT dataX1;
    BL_FLOAT dataY1;
    bool selectionOk = mSpectrogramView->GetDataSelection2(&dataX0, &dataY0,
                                                           &dataX1, &dataY1,
                                                           minNormX, maxNormX);

    if (!selectionOk)
    {
        for (int i = 0; i < 2; i++)
        {
            if (mSpectroEditObjs[i] != NULL)
                mSpectroEditObjs[i]->SetSelectionEnabled(false);
        }
    }
    else
    {
        // Data selection is now in samples in spectro edit obj,
        // not in lines anymore
        dataX0 *= mBufferSize;
        dataX1 *= mBufferSize;
    
        BL_FLOAT sampleRate = mSampleRate;
        dataY0 /= mBufferSize/2;
        dataY1 /= mBufferSize/2;
        
        dataY0 = mScale->ApplyScaleInv(mYScale, dataY0, 0.0, sampleRate*0.5);
        dataY1 = mScale->ApplyScaleInv(mYScale, dataY1, 0.0, sampleRate*0.5);
        
        dataY0 *= mBufferSize/2;
        dataY1 *= mBufferSize/2;
        
        for (int i = 0; i < 2; i++)
        {
            if (mSpectroEditObjs[i] != NULL)
                mSpectroEditObjs[i]->SetDataSelection(dataX0, dataY0, dataX1, dataY1);
        }
    }
}

void
GhostTrack2::UpdateSpectroEditSamples()
{
    // Edit 
    // Set samples pointers to spectro edit objs
    for (int i = 0; i < 2; i++)
    {
        if (mSpectroEditObjs[i] != NULL)
            mSpectroEditObjs[i]->SetSamples(NULL);
    }
    
    for (int i = 0; i < mSamples.size(); i++)
    {
        if (mSpectroEditObjs[i] != NULL)
            mSpectroEditObjs[i]->SetSamples(&mSamples[i]);
    }

    if (mSpectrogramView != NULL)
    {
        mSpectrogramView->SetSamples(&mSamples);
    }
}

void
GhostTrack2::ReadSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
                                  vector<WDL_TypedBuf<BL_FLOAT> > *phases,
                                  BL_FLOAT x0, BL_FLOAT x1)
{
    if (mSamplesToSpectro == NULL)
        return;
    
    mSamplesToSpectro->ReadSpectroDataSlice(magns, phases, x0, x1);
}

void
GhostTrack2::WriteSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
                                   vector<WDL_TypedBuf<BL_FLOAT> > *phases,
                                   BL_FLOAT x0, BL_FLOAT x1,
                                   int fadeNumSamples)
{
    if (mSamplesToSpectro == NULL)
        return;
    
    mSamplesToSpectro->WriteSpectroDataSlice(magns, phases, x0, x1, fadeNumSamples);
}

void
GhostTrack2::UpdateMiniView()
{
    BL_FLOAT minNormX = 0.0;
    BL_FLOAT maxNormX = 1.0;
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);

#if 1
    // SpectrogramDisplay3::SetVisibleNormBounds strangely
    // negates mAbsMinX and mAbsMaxX
    //
    // So in the case of empty data, set the miniview bounds to max
    //
    // FIX: plug mode: play, change sample rate (we then reset the view)
    // => the miniview bounds get stuck on the left
    if (mSamples.empty())
    {
        minNormX = 0.0;
        maxNormX = 1.0;
    }
#endif
    
    if (mMiniView != NULL)
        mMiniView->SetBounds(minNormX, maxNormX);
}

void
GhostTrack2::UpdateMiniViewData()
{
    if ((mMiniView != NULL) && !mSamples.empty())
    {
        mMiniView->SetSamples(mSamples[0]);
      
        WDL_TypedBuf<BL_FLOAT> &miniWaveform = mTmpBuf5;
        mMiniView->GetWaveForm(&miniWaveform);
        
        if (mMiniViewWaveformCurve != NULL)
            mMiniViewWaveformCurve->SetValues4(miniWaveform);
    }
    else
    {
        if (mMiniViewWaveformCurve != NULL)
            mMiniViewWaveformCurve->ClearValues();
    }
}

void
GhostTrack2::SetNeedRecomputeData(bool flag)
{
    mNeedRecomputeData = flag;
}

void
GhostTrack2::CheckRecomputeData()
{
    unsigned long int t = mRecomputeDataTimer.Get();
    
    if (mNeedRecomputeData && (t > RECOMPUTE_DATA_DELAY))
    {
        mNeedRecomputeData = false;
        
        DoRecomputeData();
    }
}

void
GhostTrack2::DoRecomputeData()
{ 
    // Reset, to avoid previously buffer data from
    // other zoom value
    if (mDispFftObj != NULL)
        mDispFftObj->Reset();
    
    UpdateSpectrogramData();
    
    if (mSpectrogramDisplay != NULL)
    {
        mSpectrogramDisplay->ResetZoomAndTrans();
        mSpectrogramDisplay->UpdateSpectrogram();
    }
    
    // Lazy evaluation
    mMustUpdateSpectrogram = true;
    
    UpdateWaveform();
    UpdateMiniViewData();
}

void
GhostTrack2::RefreshData()
{
    BL_FLOAT sampleRate = mSampleRate;
    
    if (mSpectroDisplayObj == NULL)
        return;
    
    BLSpectrogram4 *dispSpec = mSpectroDisplayObj->GetSpectrogram();
    if (dispSpec != NULL)
        dispSpec->Reset(sampleRate);
    
    // Set samples pointers to spectro edit objs
    UpdateSpectroEditSamples();
    
    if (mSpectrogramView != NULL)
        mSpectrogramView->SetSamples(&mSamples);
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->Reset();
    
    DoRecomputeData();
}

void
GhostTrack2::ResetSpectrogram(BL_FLOAT sampleRate)
{
    BLSpectrogram4 *dispSpec = mSpectroDisplayObj->GetSpectrogram();
    if (dispSpec != NULL)
        dispSpec->Reset(sampleRate);
}

void
GhostTrack2::RefreshSpectrogramView()
{
    if (mSpectrogramView != NULL)
        mSpectrogramView->SetSamples(&mSamples);
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->Reset();
}

void
GhostTrack2::UpdateAfterNewFile()
{
    SetSamplesPyramidSamples();
    
    // Set samples pointers to spectro edit objs
    UpdateSpectroEditSamples();
    
    // Fill the spectrogram
    mDispFftObj->Reset();

    if (mCustomDrawer != NULL)
    {
        mCustomDrawer->ClearBar();
        mCustomDrawer->ClearSelection();
        mCustomDrawer->SetBarPos(0.0);
    }

    if (mSpectrogramView != NULL)
    {
        mSpectrogramView->ClearViewSelection();
        mSpectrogramView->SetViewBarPosition(0.5);
        mSpectrogramView->Reset();
    }

    if (mSpectrogramDisplay != NULL)
    {
        mSpectrogramDisplay->ClearBGSpectrogram();

#if 1
        mSpectrogramDisplay->ResetTransform();
        mSpectrogramDisplay->SetZoom(1.0);
        mSpectrogramDisplay->SetAbsZoom(1.0);
        mSpectrogramDisplay->SetCenterPos(0.5);
#endif
        
        mSpectrogramDisplay->UpdateSpectrogram(true, true);
    }

    if (mSpectrogramView != NULL)
        mSpectrogramView->SetSamples(&mSamples);
    
    UpdateSpectrogramData(true);

    //
    ResetSpectrogramZoomAdjust();
    
    // Previously, we recorded in the edit object (took a lot of time !)
    for (int i = 0; i < 2/*numChannels*/; i++)
    {
        if (mSpectroEditObjs[i] != NULL)
            mSpectroEditObjs[i]->SetMode(SpectroEditFftObj3::BYPASS);
    }
    
    // Empty, to avoid playing the file just after loading
    mEditFftObj->Reset();

    mDispGenFftObj->Reset();
    
    mMustUpdateSpectrogram = true;
    mMustUpdateBGSpectrogram = true;

    // NOTE: should compute max on mono signal instead
    
    // Compute waveform max
    mWaveformMax = 0.0;
    if (!mSamples.empty())
        mWaveformMax = BLUtils::ComputeMaxAbs(mSamples[0]);
    
    UpdateWaveform();
    UpdateMiniViewData();
    UpdateTimeAxis();

    UpdateMiniView();
    
    // OPTIM: This doesn't seem necessary anymore
    // And if activated, it take half the time of Ghost::OpenFile
#if FORCE_REFRESH_DATA
    
    // Without that, the spectrogram is slightly shifted
    // when doing the first cut-paste
    RefreshData();
#endif

    // Show play bar
    SetBarActive(true);
    SetBarPos(0.0);
}

void
GhostTrack2::SetLowFreqZoom(bool lowFreqZoom)
{
    // Do not recompute if no change
    // Save much resource for example when resizing GUI
    if ((lowFreqZoom == mLowFreqZoom) && !mMustRecomputeLowFreqZoom)
        return;
    
    mLowFreqZoom = lowFreqZoom;

    Scale::Type prevScale = mYScale;
    
    mYScale = !mLowFreqZoom ? Scale::MEL : Scale::LOW_ZOOM;

    // Spectrogram
    if (mSpectrogram != NULL)
        mSpectrogram->SetYScale(mYScale);
    if (mSpectrogramBG != NULL)
        mSpectrogramBG->SetYScale(mYScale);
 
    // Axis
    if (mFreqAxis != NULL)
        mFreqAxis->SetScale(mYScale);

    RecomputeBGSpectrogram();

    // Spectrogram data
    //DoRecomputeData();
    
    // Update selection if any
    if (mMode == EDIT)
    {
        if (mCustomDrawer != NULL)
        {
            if (mCustomDrawer->IsSelectionActive())
                RescaleSelection(prevScale, mYScale);
        }
    }

    mMustRecomputeLowFreqZoom = false;
}

Scale::Type
GhostTrack2::GetYScaleType()
{
    return mYScale;
}

void
GhostTrack2::ModeChanged(int prevMode)
{
    // Reset the oversampling (just in case)
    // Maybe helped to fix "edit, zoom, view => view transformas problem"
    mOversampling = OVERSAMPLING_0;
    
    // VIEW => any mode : Reset
    //
    // Empty the data if the prev mode was view
    // This avoids editing data that was displayed in view mode
    // (because it is buggy for the moment)
    if (prevMode == VIEW)
    {
        ResetAllData();
    }
    
    // Any mode => VIEW: Reset
    if (mMode == VIEW)
    {
        ResetAllData();
    }
    
    // Any mode => CAPTURE: Reset
    if (mMode == ACQUIRE)
    {
        ResetAllData();
    }

    switch(mMode)
    {
        case EDIT:
        {
            // FIX (part 2): view scrolling too fast in view mode
            UpdateSpectrogramData();
            
            if (mSpectroDisplayObj != NULL)
                mSpectroDisplayObj->SetMode(SpectrogramFftObj2::EDIT);
            
            for (int i = 0; i < 2; i++)
            {
                if (mSpectroEditObjs[i] != NULL)
                    mSpectroEditObjs[i]->SetMode(SpectroEditFftObj3::BYPASS);
            }
            
            // FIX: on Reaper, capture, switch to edit.
            // When played, there was a latency of ~1s
            Reset(mBufferSize, mSampleRate);
            
            // Generate waveform and so on
            RefreshData();
            
            if (mSpectrogramDisplay != NULL)
            {
                // Also update "full" data, i.e background spectrogram
                mSpectrogramDisplay->UpdateSpectrogram(true, true);
            }

            // Be sure the samples are well aligned
            SetSamplesPyramidSamples();
            UpdateWaveform();

            if (mHAxis != NULL)
                mHAxis->SetAlignBorderLabels(false);
        }
        break;
            
        case VIEW:
        {
            // FIX(part 1): select view, play, select acquire, play, select edit
            // then select view again => the spectrogram scrolls too fast
            //
            // Reset the oversampling because it can have been modified in edit mode
            // with adaptive overlapping
            BL_FLOAT sampleRate = mSampleRate;

            if (mDispFftObj != NULL)
                mDispFftObj->Reset(mBufferSize, OVERSAMPLING_0, FREQ_RES, sampleRate);
            
            if (mSpectroDisplayObj != NULL)
                mSpectroDisplayObj->Reset(mBufferSize, OVERSAMPLING_0,
                                          FREQ_RES, sampleRate);
            
            
            // Empty at startup
            mSamples.resize(0);
            
            if (mSamplesPyramid != NULL)
                mSamplesPyramid->Reset();
            
            // Set samples pointers to spectro edit objs
            UpdateSpectroEditSamples();
            
            // Update the waveform
            if (mSpectrogramView != NULL)
                mSpectrogramView->SetSamples(&mSamples);
            UpdateWaveform();
            
            //
            UpdateMiniViewData();
            
            if (mSpectroDisplayObj != NULL)
                mSpectroDisplayObj->SetMode(SpectrogramFftObj2::VIEW);
            
            if (mCustomDrawer != NULL)
            {
                mCustomDrawer->SetBarActive(false);
                mCustomDrawer->SetSelectionActive(false);
            
                // Hide play bar
                mCustomDrawer->SetPlayBarActive(false);
            }
            
            ResetTransforms();
        
            Reset(mBufferSize, mSampleRate);

            if (mHAxis != NULL)
                mHAxis->SetAlignBorderLabels(false);
        }
        break;
            
        case ACQUIRE:
        {
            // Empty before acquiring
            mSamples.resize(0);
            
            if (mSamplesPyramid != NULL)
                mSamplesPyramid->Reset();
            
            // Set samples pointers to spectro edit objs
            UpdateSpectroEditSamples();
            
            // Update the waveform
            if (mSpectrogramView != NULL)
                mSpectrogramView->SetSamples(&mSamples);
            UpdateWaveform();
            UpdateMiniViewData();
            
            if (mSpectroDisplayObj != NULL)
                mSpectroDisplayObj->SetMode(SpectrogramFftObj2::ACQUIRE);
            
            if (mCustomDrawer != NULL)
            {
                mCustomDrawer->SetBarActive(false);
                mCustomDrawer->SetSelectionActive(false);
            
                // Hide play bar
                mCustomDrawer->SetPlayBarActive(false);
            }
            
            if (mEditFftObj != NULL)
                mEditFftObj->Reset();

            if (mDispGenFftObj != NULL)
                mDispGenFftObj->Reset();
            
            ResetTransforms();
            
            Reset(mBufferSize, mSampleRate);

            AdjustSpectroLatencyCaptureMode();

            if (mHAxis != NULL)
                mHAxis->SetAlignBorderLabels(true);
        }
        break;
            
        case RENDER:
        {
            if (mSpectroDisplayObj != NULL)
                mSpectroDisplayObj->SetMode(SpectrogramFftObj2::RENDER);
            
            if (mCustomDrawer != NULL)
            {
                mCustomDrawer->SetBarActive(false);
                mCustomDrawer->SetSelectionActive(false);
            }
            
            ResetTransforms();
            
            Reset(mBufferSize, mSampleRate);
            
            // Generate waveform and so on
            //
            // Must do that to update the spectrogram
            // (but it is slow and not necessary to update the waveform)
            RefreshData();

            if (mHAxis != NULL)
                mHAxis->SetAlignBorderLabels(true);
        }
        break;
            
        default:
            break;
    };
    
    UpdateTimeAxis();

    mNumToPopTotal = 0;
}

void
GhostTrack2::CommandDone()
{
#if UPDATE_BG_AFTER_EDIT
    RecomputeBGSpectrogram();
#endif
}

bool
GhostTrack2::IsMouseOverGraph() const
{
    if ((mCustomControl != NULL) &&
        mCustomControl->IsMouseOver())
        return true;

    return false;
}

void
GhostTrack2::CopyTrackSelection(const GhostTrack2 &src, GhostTrack2 *dst)
{
    if ((src.mCustomDrawer != NULL) && (dst->mCustomDrawer != NULL))
    {
        BL_FLOAT x0;
        BL_FLOAT y0;
        BL_FLOAT x1;
        BL_FLOAT y1;
            
        src.mCustomDrawer->GetSelection(&x0, &y0, &x1, &y1);
        dst->mCustomDrawer->SetSelection(x0, y0, x1, y1);
    }

    if ((src.mCustomControl != NULL) && (dst->mCustomControl != NULL))
    {
        BL_FLOAT selection[4];
        src.mCustomControl->GetSelection(selection);
        dst->mCustomControl->SetSelection(selection[0], selection[1],
                                          selection[2], selection[3]);
    }

    for (int i = 0; i < 2; i++)
    {
        if ((src.mSpectroEditObjs[i] != NULL) && (dst->mSpectroEditObjs[i] != NULL))  
        {
            BL_FLOAT selection[4];
            src.mSpectroEditObjs[i]->GetNormSelection(selection);
            dst->mSpectroEditObjs[i]->SetNormSelection(selection);
        }
    }

    for (int i = 0; i < 2; i++)
    {
        if ((src.mSpectroDispGenObjs[i] != NULL) &&
            (dst->mSpectroDispGenObjs[i] != NULL))  
        {
            BL_FLOAT selection[4];
            src.mSpectroDispGenObjs[i]->GetNormSelection(selection);
            dst->mSpectroDispGenObjs[i]->SetNormSelection(selection);
        }
    }
}

void
GhostTrack2::UpdateTimeAxis()
{
    if (mSpectrogramDisplay == NULL)
        return;
    
    BL_FLOAT sampleRate = mSampleRate;
    
    // Axis
    
    if (mMode == VIEW)
    {
        BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
        int numBuffers = spectro->GetMaxNumCols();

        if (mSpectrogramDisplay != NULL)
        {
            int speedMod = mSpectrogramDisplay->GetSpeedMod();
            numBuffers *= speedMod;
        }
        
        BL_FLOAT timeDuration =
            GraphTimeAxis6::ComputeTimeDuration(numBuffers,
                                                mBufferSize,
                                                mOversampling,
                                                sampleRate);

        //fprintf(stderr, "time duration: %g\n", timeDuration);
        //timeDuration *= 2.0;
        
        // Manage to have always around 10 labels
        if (mTimeAxis != NULL)
            mTimeAxis->Reset(mBufferSize, timeDuration, NUM_TIME_AXIS_LABELS);
        
        if (mTransportSamplePos >= 0.0)
        {
            BL_FLOAT currentTime = mTransportSamplePos/sampleRate;
            if (mTimeAxis != NULL)
                mTimeAxis->Update(currentTime);
        }
    }
    
    if ((mMode == ACQUIRE) || (mMode == EDIT) || (mMode == RENDER))
    {
        // Make a cool axis, with values sliding as we zoom
        // and units changing when necessary
        // (the border values have decimals, the middle values are rounded and slide)
        BL_FLOAT minNormX;
        BL_FLOAT maxNormX;
        mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
        
        BL_FLOAT startDataPos;
        BL_FLOAT endDataPos;
        mSpectrogramView->GetViewDataBounds(&startDataPos, &endDataPos,
                                             minNormX, maxNormX);
        
        //BL_FLOAT sampleRate = mSampleRate;
        BL_FLOAT startTime = (startDataPos*mBufferSize)/sampleRate;
        BL_FLOAT endTime = (endDataPos*mBufferSize)/sampleRate;
        
        BL_FLOAT timeDuration = endTime - startTime;

        // Hack
        // FIX: at app startup, all the time axis labels
        // are stacked on the left
        // (because we have a 0s duration)
        // So use a dummy duration when there is no data.
        if (timeDuration < BL_EPS)
        {
            // Set by 1 second by default
            timeDuration = 1.0;
            endTime = timeDuration;
        }
            
        // Manage to have always around 10 labels
        if (mTimeAxis != NULL)
            mTimeAxis->Reset(mBufferSize, timeDuration, NUM_TIME_AXIS_LABELS);
        
        if (mTimeAxis != NULL)
            mTimeAxis->Update(endTime);
    }
}

void
GhostTrack2::CreateSpectrogramDisplay(bool createFromInit)
{
    if (!createFromInit && (mSpectroDisplayObj == NULL))
        return;
    
    BL_FLOAT sampleRate = mSampleRate;
    
    mSpectrogram = mSpectroDisplayObj->GetSpectrogram();
    
    mSpectrogram->SetDisplayPhasesX(false);
    mSpectrogram->SetDisplayPhasesY(false);
    mSpectrogram->SetDisplayMagns(true);
    mSpectrogram->SetYScale(mYScale);
    mSpectrogram->SetDisplayDPhases(false);

    // BG Spectrogram
    mSpectrogramBG->SetDisplayPhasesX(false);
    mSpectrogramBG->SetDisplayPhasesY(false);
    mSpectrogramBG->SetDisplayMagns(true);
    mSpectrogramBG->SetYScale(mYScale);
    mSpectrogramBG->SetDisplayDPhases(false);
    
    if (mGraph != NULL)
    {
        mSpectrogramDisplay = new SpectrogramDisplay3(mSpectroDisplayState);
        mSpectroDisplayState = mSpectrogramDisplay->GetState();

        mSpectrogramDisplay->SetSpectrogram(mSpectrogram);
        mSpectrogramDisplay->SetSpectrogramBG(mSpectrogramBG); // NEW
        mSpectrogramDisplay->SetBounds(0.0, 0.0, 1.0, 1.0 - SPECTRO_MINI_SIZE);
    
        mGraph->AddCustomDrawer(mSpectrogramDisplay);

        mMiniView = new MiniView2(GRAPH_CONTROL_NUM_POINTS,
                                  0.0, 1.0 - SPECTRO_MINI_SIZE,
                                  1.0 - SPECTRO_MINI_OFFSET, 1.0,
                                  mMiniViewState);
        mMiniViewState = mMiniView->GetState();
        
        mGraph->AddCustomDrawer(mMiniView);
        
        mCustomControl->SetMiniView(mMiniView);
    }

    if (mCustomControl == NULL)
        mCustomControl = new GhostCustomControl2(this);
    mCustomControl->SetSpectrogramDisplay(mSpectrogramDisplay);

    if (mSpectrogramView == NULL)
    {
        mSpectrogramView = new SpectrogramView2(mSpectrogram,
                                                mDispGenFftObj,
                                                mSpectroDispGenObjs,
                                                SPECTRO_VIEW_MAX_NUM_COLS,
                                                0.0, 0.0,
                                                1.0, 1.0 - SPECTRO_MINI_SIZE,
                                                sampleRate);
    }
}

void
GhostTrack2::CreateGraphAxes()
{
    // Create
    if (mHAxis == NULL)
    {
        mHAxis = new GraphAxis2();
        mTimeAxis = new GraphTimeAxis6(false, false);
    }
    
    if (mVAxis == NULL)
    {
        mVAxis = new GraphAxis2();
        mFreqAxis = new GraphFreqAxis2(false, mYScale);
    }
    
    // Update
    mGraph->SetHAxis(mHAxis);
    mGraph->SetVAxis(mVAxis);
    
    BL_FLOAT sampleRate = mSampleRate;
    
    int graphWidth;
    int graphHeight;
    mGraph->GetSize(&graphWidth, &graphHeight);

    BL_FLOAT timeAxisDuration = mTimeAxis->GetTimeDuration();
    
    BL_FLOAT offsetY = SPECTRO_MINI_SIZE;
    mTimeAxis->Init(mGraph, mHAxis, mGUIHelper, mBufferSize,
                    //1.0,
                    timeAxisDuration,
                    1.0, offsetY);

    bool alignBorderLabels = ((mMode == ACQUIRE) || (mMode == RENDER));
                              
    mHAxis->SetAlignBorderLabels(alignBorderLabels);
        
    mFreqAxis->Init(mVAxis, mGUIHelper, false, mBufferSize, sampleRate, graphWidth);
    mFreqAxis->Reset(mBufferSize, sampleRate);
    
    BL_FLOAT freqAxisBounds[2] = { SPECTRO_MINI_SIZE, 1.0 };
    mFreqAxis->SetBounds(freqAxisBounds);
}

void
GhostTrack2::CreateGraphCurves()
{
    if (mWaveformCurve == NULL)
        // Not yet created
    {    
        // Waveform curve
        int waveformColor[4];
        mGUIHelper->GetGraphCurveColorBlue(waveformColor);
    
        mWaveformCurve = new GraphCurve5(GRAPH_CONTROL_NUM_POINTS);
        mWaveformCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mWaveformCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mWaveformCurve->SetColor(waveformColor[0],
                                 waveformColor[1],
                                 waveformColor[2]);
        //mWaveformCurve->SetLineWidth(1.0);
        // 1.5 is better with black overlay
        // (avoid hacked lines)
        mWaveformCurve->SetLineWidth(1.5);
        
#if BEVEL_CURVES
        mWaveformCurve->SetBevel(true);
#endif
    }

    if (mWaveformOverlayCurve == NULL)
        // Not yet created
    {    
        // WaveformOverlay curve
        int waveformOverlayColor[4];
        //mGUIHelper->GetGraphCurveColorGray(waveformOverlayColor);
        // Black is more visible over "purple" colormap
        mGUIHelper->GetGraphCurveColorBlack(waveformOverlayColor);

        // NOTE: for long files, maybe do not use black overlay
        // when minimum zoom (this makes some slight dark bars
        // on the waveform, but maybe this is not a problem)
        
        mWaveformOverlayCurve = new GraphCurve5(GRAPH_CONTROL_NUM_POINTS);
        mWaveformOverlayCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mWaveformOverlayCurve->SetYScale(Scale::LINEAR, -1.0, 1.0);
        mWaveformOverlayCurve->SetColor(waveformOverlayColor[0],
                                        waveformOverlayColor[1],
                                        waveformOverlayColor[2]);
        mWaveformOverlayCurve->SetLineWidth(2.0);
      
#if BEVEL_CURVES
        mWaveformOverlayCurve->SetBevel(true);
#endif
    }

    if (mMiniViewWaveformCurve == NULL)
        // Not yet created
    {
        // MiniViewWaveform curve
        int miniViewWaveformColor[4];
        mGUIHelper->GetGraphCurveColorBlue(miniViewWaveformColor);
      
        mMiniViewWaveformCurve = new GraphCurve5(GRAPH_CONTROL_NUM_POINTS);
        mMiniViewWaveformCurve->SetXScale(Scale::LINEAR, 0.0, 1.0);
        mMiniViewWaveformCurve->SetYScale(Scale::LINEAR, 0.0, 1.0);
        mMiniViewWaveformCurve->SetColor(miniViewWaveformColor[0],
                                         miniViewWaveformColor[1],
                                         miniViewWaveformColor[2]);
        mMiniViewWaveformCurve->SetLineWidth(1.0);
      
#if BEVEL_CURVES
        mMiniViewWaveformCurve->SetBevel(true);
#endif
    }
  
    int graphSize[2];
    mGraph->GetSize(&graphSize[0], &graphSize[1]);
  
    // Add overlay first
    mWaveformOverlayCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mWaveformOverlayCurve);
    
    mWaveformCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mWaveformCurve);
    
    mMiniViewWaveformCurve->SetViewSize(graphSize[0], graphSize[1]);
    mGraph->AddCurve(mMiniViewWaveformCurve);
}

void
GhostTrack2::SetColorMap(int colorMapNum)
{
    // Take the best 5 colorMaps
    ColorMapFactory::ColorMap colorMapNums[] =
        {
            ColorMapFactory::COLORMAP_BLUE,
            ColorMapFactory::COLORMAP_GREY,
            ColorMapFactory::COLORMAP_GREEN,
            ColorMapFactory::COLORMAP_PURPLE,
            ColorMapFactory::COLORMAP_WASP,
            ColorMapFactory::COLORMAP_SKY,
            ColorMapFactory::COLORMAP_DAWN_FIXED,
            ColorMapFactory::COLORMAP_RAINBOW,
            ColorMapFactory::COLORMAP_SWEET,
            //ColorMapFactory::COLORMAP_GREEN_RED,
            //ColorMapFactory::COLORMAP_SWEET2,
            //ColorMapFactory::COLORMAP_OCEAN,
            ColorMapFactory::COLORMAP_FIRE,
        };
    ColorMapFactory::ColorMap colorMapNum0 = colorMapNums[colorMapNum];

    SetColorMap(colorMapNum0);
}

void
GhostTrack2::SetColorMap(ColorMapFactory::ColorMap colormap)
{
    if (mSpectroDisplayObj != NULL)
    {
        BLSpectrogram4 *spec = mSpectroDisplayObj->GetSpectrogram();
    
        if (spec != NULL)
        {
            spec->SetColorMap(colormap);
            if (mSpectrogramBG != NULL)
                mSpectrogramBG->SetColorMap(colormap);
            
            // FIX: set to false,false to fix
            // BUG: play data, stop, change colormap
            // => there is sometimes a small jumps of the data to the left
            if (mSpectrogramDisplay != NULL)
            {
                mSpectrogramDisplay->UpdateSpectrogram(false);
            }
        }
    }
}

bool
GhostTrack2::ContainsSamples() const
{
    if (mSamples.empty())
        return false;

    long numSamples = mSamples[0].GetSize();
    if (numSamples > 0)
        return true;

    return false;
}

void
GhostTrack2::GetSamples(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    *samples = mSamples;
}

void
GhostTrack2::ResetTransforms()
{
    if (mSpectrogramView != NULL)
        mSpectrogramView->Reset();
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->ResetTransform();
    
    UpdateMiniView();

    // FIX: this fixes: "capture, edit, zoom a lot, view => view transform problems"
    ResetSpectrogramZoomAdjust();
}

void
GhostTrack2::UpdateSpectrogramData(bool fromOpenFile)
{    
    BL_FLOAT minNormX = 0.0;
    BL_FLOAT maxNormX = 0.0;
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->GetVisibleNormBounds(&minNormX, &maxNormX);
    
    BL_FLOAT startDataPos = 0.0;
    BL_FLOAT endDataPos = 0.0;
    if (mSpectrogramView != NULL)
        mSpectrogramView->GetViewDataBounds(&startDataPos, &endDataPos,
                                            minNormX, maxNormX);
    
    
#if ADAPTIVE_OVERSAMPLING
    BL_FLOAT numBufs = endDataPos - startDataPos;
    
    // How many buffers to trigger max overlap ?
    // Increase it to trigger higer overlap with few zoom
#define MAX_OVERLAP_NUM_BUFS 4
    
    int overlap = OVERSAMPLING_0;
    if (numBufs > 0.0)
        overlap = ((BL_FLOAT)MAX_OVERLAP_NUM_BUFS*OVERSAMPLING_3)/numBufs;
    
    overlap = BLUtilsMath::NextPowerOfTwo(overlap);
    
    if (overlap < OVERSAMPLING_0)
        overlap = OVERSAMPLING_0;
    
    if (overlap > OVERSAMPLING_3)
        overlap = OVERSAMPLING_3;
        
    // FIX(part 3): view spectrogram scrolling too fast
    if (mMode == VIEW)
        overlap = OVERSAMPLING_0;
        
    // Reset only display
    BL_FLOAT sampleRate = mSampleRate;
    
    if (mDispFftObj != NULL)
        mDispFftObj->Reset(mBufferSize, overlap, FREQ_RES, sampleRate);
    
    if (mSpectroDisplayObj != NULL)
        mSpectroDisplayObj->Reset(mBufferSize, overlap, FREQ_RES, sampleRate);

    // Also set adaptive oversample to Edit obj
    // (because it is now used to display in SpectrogramView2)
    //
    // For editing, it is necessay to keep a constant overlap
    // (e.g: cut, zoom, then paste => problem)
    // So for editing, the overlap will be forced to constant,
    // then restored after edition.
    if (mDispGenFftObj != NULL)
        mDispGenFftObj->Reset(mBufferSize, overlap, FREQ_RES, sampleRate);
#endif
    
    if (mSpectrogramView != NULL)
        mSpectrogramView->UpdateSpectrogramData(minNormX, maxNormX, mSpectrogram);

    // Must update BG zoom adjust just once, when opening the file
    // After it must be kept untouched
    bool updateZommAdjustBG = fromOpenFile;
    UpdateZoomAdjust(updateZommAdjustBG);
}

void
GhostTrack2::UpdateSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                             bool updateCenterPos, bool activateDrawSelection,
                             bool updateCustomControl)
{
    if (mGraph == NULL)
        return;
    
    BL_FLOAT x0f;
    BL_FLOAT y0f;
    
    BL_FLOAT x1f;
    BL_FLOAT y1f;
    ConvertSelection(&x0, &y0, &x1, &y1, &x0f, &y0f, &x1f, &y1f);
                     
    if (mCustomDrawer != NULL)
    {
        mCustomDrawer->SetSelection(x0f, y0f, x1f, y1f);
        if (activateDrawSelection)
            mCustomDrawer->SetSelectionActive(true);
    
        if (updateCustomControl)
            mCustomControl->SetSelection(x0, y0, x1, y1);
    }

    if (mSpectrogramView != NULL)
        mSpectrogramView->SetViewSelection(x0f, y0f, x1f, y1f);
    
    if (updateCenterPos)
    {
        // Spectrogram view
        BL_FLOAT xcenter = (x0f + x1f)/2.0;
    
        if (mSpectrogramView != NULL)
            mSpectrogramView->SetViewBarPosition(xcenter);
    
        if (mSpectrogramDisplay != NULL)
            mSpectrogramDisplay->SetCenterPos(xcenter);
    }
    
    UpdateSpectroEdit();
    UpdateMiniView();
}

void
GhostTrack2::ConvertSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                              BL_FLOAT *x1, BL_FLOAT *y1,
                              BL_FLOAT *x0f, BL_FLOAT *y0f,
                              BL_FLOAT *x1f, BL_FLOAT *y1f)
{
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    // Swap if necessary
    if (*x1 < *x0)
    {
        int tmp = *x1;
        *x1 = *x0;
        *x0 = tmp;
    }
    
    if (*y1 < *y0)
    {
        int tmp = *y1;
        *y1 = *y0;
        *y0 = tmp;
    }
    
    // NOTE: reorder here may be useless
    
    // Reorder if necessary
    if (*x0 <= *x1)
    {
        *x0f = ((BL_FLOAT)*x0)/width;
        *x1f = ((BL_FLOAT)*x1)/width;
    }
    else
    {
        *x0f = ((BL_FLOAT)*x1)/width;
        *x1f = ((BL_FLOAT)*x0)/width;
    }
    
    if (*y0 <= *y1)
    {
        *y0f = ((BL_FLOAT)*y0)/height;
        *y1f = ((BL_FLOAT)*y1)/height;
    }
    else
    {
        *y0f = ((BL_FLOAT)*y1)/height;
        *y1f = ((BL_FLOAT)*y0)/height;
    }
}

// Recompute the selection with the current coordinates
void
GhostTrack2::UpdateSelection()
{
    if (mGraph == NULL)
        return;
    if (mCustomDrawer == NULL)
        return;
    
    int width;
    int height;
    mGraph->GetSize(&width, &height);
    
    // Save
    bool barActive = mCustomDrawer->IsBarActive();
    bool selectionActive = mCustomDrawer->IsSelectionActive();
    
    // Get the selection before changing the bar pos
    BL_FLOAT selection[4];
    mCustomDrawer->GetSelection(&selection[0], &selection[1],
                                &selection[2], &selection[3]);
    
    // Selection
    selection[0] = selection[0]*width;
    selection[1] = selection[1]*height;
    
    selection[2] = selection[2]*width;
    selection[3] = selection[3]*height;
    
    UpdateSelection(selection[0], selection[1],
                    selection[2], selection[3],
                    !barActive);
    
    // Restore
    if (!barActive)
        mCustomDrawer->ClearBar();
    
    if (!selectionActive)
        mCustomDrawer->ClearSelection();
}

bool
GhostTrack2::IsSelectionActive()
{
    if (mCustomDrawer == NULL)
        return mPrevSelectionActive;
    
    bool res = mCustomDrawer->IsSelectionActive();
    
    return res;
}

void
GhostTrack2::SetSelectionActive(bool flag)
{
    if (mCustomDrawer == NULL)
        return;
    
    mCustomDrawer->SetSelectionActive(flag);

    if (mCustomControl != NULL)
        mCustomControl->SetSelectionActive(flag);
}

void
GhostTrack2::RescaleSelection(Scale::Type srcScale, Scale::Type dstScale)
{
    if (mCustomDrawer != NULL)
    {
        BL_FLOAT x[2];
        BL_FLOAT y[2];
        mCustomDrawer->GetSelection(&x[0], &y[0], &x[1], &y[1]);

        //
        y[0] = GraphNormYToNormFreq(y[0]);
        y[1] = GraphNormYToNormFreq(y[1]);
            
        // Apply invert prev scale
        y[0] = mScale->ApplyScaleInv(srcScale, y[0], 0.0, mSampleRate*0.5);
        y[1] = mScale->ApplyScaleInv(srcScale, y[1], 0.0, mSampleRate*0.5);

        // Apply current scale
        y[0] = mScale->ApplyScale(dstScale, y[0], 0.0, mSampleRate*0.5);
        y[1] = mScale->ApplyScale(dstScale, y[1], 0.0, mSampleRate*0.5);
        
        //
        y[0] = GraphNormFreqToNormY(y[0]);
        y[1] = GraphNormFreqToNormY(y[1]);
        
        //
        int width;
        int height;
        mGraph->GetSize(&width, &height);
            
        x[0] *= width;
        y[0] *= height;
        x[1] *= width;
        y[1] *= height;

        UpdateSelection(x[0], y[0], x[1], y[1], false, true, true);
    }
}

void
GhostTrack2::DataToViewRef(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1)
{
    if (mSpectrogramView == NULL)
        return;
    
    mSpectrogramView->DataToViewRef(x0, y0, x1, y1);
}

void
GhostTrack2::FileChannelQueueToVec()
{
    mSamples.resize(mSamplesQueue.size());
    for (int i = 0; i < mSamples.size(); i++)
    {
        BLUtils::FastQueueToBuf(mSamplesQueue[i], &mSamples[i]);
    }
}

void
GhostTrack2::SetSamplesPyramidSamples()
{
    if (mSamplesPyramid != NULL)
    {
        if (mSamples.size() > 0)
        {
            if (mSamples.size() == 1)
                mSamplesPyramid->SetValues(mSamples[0]);
            else if (mSamples.size() >= 2)
            {
                // Stereo to mono
                // NOT: think to modify in SamplesToMagnPhases too
                WDL_TypedBuf<BL_FLOAT> mono;
                BLUtils::StereoToMono(&mono, mSamples[0], mSamples[1]);
                
                mSamplesPyramid->SetValues(mono);
            }
        }
    }
}

BL_FLOAT
GhostTrack2::GraphNormXToTime(BL_FLOAT normX)
{
    BL_FLOAT timeX = 0.0;
    
    if (mTimeAxis != NULL)
    {
        BL_FLOAT minTimeSec;
        BL_FLOAT maxTimeSec;
        mTimeAxis->GetMinMaxTime(&minTimeSec, &maxTimeSec);
        
        timeX = minTimeSec + normX*(maxTimeSec - minTimeSec);
    }

    return timeX;
}

BL_FLOAT
GhostTrack2::GraphNormYToFreq(BL_FLOAT normY)
{
    if (mSpectroDisplayObj == NULL)
        return 0.0;

    normY = 1.0 - normY;
    
    normY = (normY - SPECTRO_MINI_SIZE)/(1.0 - SPECTRO_MINI_SIZE);

    if (normY < 0.0)
        normY = 0.0;
    
    BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
    
    BL_FLOAT freq = spectro->NormYToFreq(normY);

    return freq;
}

BL_FLOAT
GhostTrack2::GraphFreqToNormY(BL_FLOAT freq)
{
    if (mSpectroDisplayObj == NULL)
        return 0.0;

    BLSpectrogram4 *spectro = mSpectroDisplayObj->GetSpectrogram();
    
    BL_FLOAT normY = spectro->FreqToNormY(freq);

    normY = normY*(1.0 - SPECTRO_MINI_SIZE) + SPECTRO_MINI_SIZE;

    // clip ?
    
    normY = 1.0 - normY;

    return normY;
}

BL_FLOAT
GhostTrack2::GraphNormYToNormFreq(BL_FLOAT normY)
{
    normY = 1.0 - normY;
    
    normY = (normY - SPECTRO_MINI_SIZE)/(1.0 - SPECTRO_MINI_SIZE);

    if (normY < 0.0)
        normY = 0.0;

    return normY;
}

BL_FLOAT
GhostTrack2::GraphNormFreqToNormY(BL_FLOAT normFreq)
{
    BL_FLOAT normY = normFreq*(1.0 - SPECTRO_MINI_SIZE) + SPECTRO_MINI_SIZE;

    // clip ?
    
    normY = 1.0 - normY;

    return normY;
}

void
GhostTrack2::ClipWaveform(WDL_TypedBuf<BL_FLOAT> *samples)
{
    BLUtils::ClipMinMax(samples, -1.0, 1.0);
}

// Add some zero samples at the beginning, to compensate for latency
// (so the samples will be exactly aligned to the spectrogram
void
GhostTrack2::AdjustSamplesPyramidLatencyViewMode()
{
    if (mSamplesPyramid == NULL)
        return;

    if (mMode == VIEW)
    {
        //int latency = mBufferSize; //mDispFftObj->ComputeLatency();
        int latency = mBufferSize/mOversampling;
 
        WDL_TypedBuf<BL_FLOAT> zeroSamples;
        zeroSamples.Resize(latency);
        BLUtils::FillAllZero(&zeroSamples);
        
        mSamplesPyramid->PushValues(zeroSamples);
    }
}

// When capturing, add an empty buffer at the beginning
// so that the spectrogram is well aligned to the waveform
void
GhostTrack2::AdjustSpectroLatencyCaptureMode()
{
    if (mDispFftObj == NULL)
        return;

    if (mMode == ACQUIRE)
    {
        int latency = mBufferSize;
        //int latency = mBufferSize/mOversampling;
 
        WDL_TypedBuf<BL_FLOAT> zeroSamples;
        zeroSamples.Resize(latency);
        BLUtils::FillAllZero(&zeroSamples);
        
        if (mDispFftObj != NULL)
        {                
            vector<WDL_TypedBuf<BL_FLOAT> > &zeroVec = mTmpBuf36;
            zeroVec.resize(1);
            zeroVec[0] = zeroSamples;

            vector<WDL_TypedBuf<BL_FLOAT> > dummySc;
            vector<WDL_TypedBuf<BL_FLOAT> > dummyOut = mTmpBuf37;
            
            mDispFftObj->Process(zeroVec, dummySc, &dummyOut);
        }
    }
}

void
GhostTrack2::ResetSpectrogramZoomAdjust()
{
    if (mSpectrogramDisplay == NULL)
        return;
    if (mSpectrogramView == NULL)
        return;
    
    // Call SetZoom() after UpdateSpectrogramData(),
    // So the spectrogram is filled, and zoomAdjustFactor is correct
    
    mSpectrogramDisplay->ResetTransform();
    
    BL_FLOAT zoomAdjustZoom;
    BL_FLOAT zoomAdjustOffset;
    mSpectrogramView->GetZoomAdjust(&zoomAdjustZoom, &zoomAdjustOffset);
    mSpectrogramDisplay->SetZoomAdjust(zoomAdjustZoom, zoomAdjustOffset);
    mSpectrogramDisplay->SetZoom(1.0);
    mSpectrogramDisplay->SetAbsZoom(1.0);
    mSpectrogramDisplay->SetCenterPos(0.5);
}

void
GhostTrack2::RecomputeBGSpectrogram()
{
    // First generate BG spectrogram (for all the data) ...
    if (mSpectrogramView != NULL)
        mSpectrogramView->UpdateSpectrogramData(0.0, 1.0, mSpectrogramBG);
    // Trigger an update for both fg and bg (just in case)
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->UpdateSpectrogram(true, true);

    // ... then generate the FG spectrogram only for the view bounds
    
    // Spectrogram data
    DoRecomputeData();
}

void
GhostTrack2::ResampleCurrentData(BL_FLOAT prevSampleRate, BL_FLOAT newSampleRate)
{
    if (std::fabs(prevSampleRate - newSampleRate) < BL_EPS)
        // Same sample rate, do nothing
        return;

    if (mSamples.empty())
        return;

    // Clear previous command history
    // (just in case it remained commands in the history)
    mCommandHistory->Clear();

    //
    if (mSamplesPyramid != NULL)
        mSamplesPyramid->Reset();
    
    mIsLoadingSaving = true;

    int numChannels = mSamples.size();
    AudioFile *audioFile = new AudioFile(numChannels, prevSampleRate, -1, &mSamples);

    audioFile->Resample2(newSampleRate);

    delete audioFile;

    //
    SetSamplesPyramidSamples();
        
    mIsLoadingSaving = false;
    
    // NEW
    RewindView();
}
