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
 
#ifndef GHOST_TRACK_H
#define GHOST_TRACK_H

#include <BLTypes.h>

#include <BLUtilsFile.h>

#include <BlaTimer.h>

#include <SpectrogramDisplay3.h>
#include <GhostCustomDrawer.h>
#include <SpectroEditFftObj3.h>

#include <MiniView2.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace igraphics;

class GUIHelper12;

class GraphControl12;

class FftProcessObj16;
class SpectrogramFftObj2;
class BLSpectrogram4;

class SpectrogramView2;
class GhostCustomControl;

class MiniView2;

class GraphCurve5;

class SamplesPyramid3;

class GraphTimeAxis6;
class GraphFreqAxis2;
class GraphAxis2;

class GhostSamplesToSpectro;

class GhostMeter;

class GhostCommandHistory;

class GhostTrack
{
 public:
    GhostTrack(GhostPluginInterface *ghostPlug,
               int bufferSize, BL_FLOAT sampleRate,
               Scale::Type yScale);
    virtual ~GhostTrack();

    void Reset(int bufferSize, BL_FLOAT sampleRate);
    void Reset();

    // Legacy mechanism
    void Lock();
    void Unlock();
    
    // Lock Free
    void PushAllData();
    
    void InitNull();
    void ApplyParams();

    void CreateControls(GUIHelper12 *guiHelper,
                        Plugin *plug,
                        IGraphics *pGraphics,
                        int graphX, int graphY,
                        int offsetX, int offsetY,
                        int graphParamIdx = kNoParameter);
    void OnUIOpen();
    void OnUIClose();
    void PreResizeGUI();
    void PostResizeGUI();

    void Init();
    void Init(int oversampling, int freqRes);

    void ProcessBlock(const vector<WDL_TypedBuf<BL_FLOAT> > &in,
                      const vector<WDL_TypedBuf<BL_FLOAT> > &scIn,
                      vector<WDL_TypedBuf<BL_FLOAT> > *out,
                      bool isTransportPlaying,
                      BL_FLOAT transportSamplePos);
    
    // Parameters (track)
    void UpdateParamRange(BL_FLOAT range);
    void UpdateParamContrast(BL_FLOAT contrast);
    void UpdateParamColorMap(int colorMapNum);
    void UpdateParamWaveformScale(BL_FLOAT waveformScale);
    void UpdateParamSpectWaveformRatio(BL_FLOAT spectWaveformRatio);

    // Parameters (global)
    void UpdateParamPlugMode(int plugMode);
    void UpdateParamMonitorEnabled(bool monitorEnabled);
    void UpdateParamSelectionType(int selectionType);

    void SetGraphEnabled(bool flag);
    void GetGraphSize(int *width, int *height);

    void ResetAllData(bool callReset = true);
        
    const char *GetCurrentFileName();
    bool GetFileModified();
    void SetFileModified(bool flag);
    bool OpenFile(const char *fileName);
    bool OpenCurrentFile();
    bool SaveFile(const char *fileName, bool updateFileName = true);
    bool SaveCurrentFile();
    bool SaveFile(const char *fileName,
                  const vector<WDL_TypedBuf<BL_FLOAT> > &channels,
                  bool updateFileName = true);
    bool ExportSelection(const char *fileName);
    void CloseFile();
    
    int GetNumChannels();
    int GetNumSamples();
    
    void StartPlay();
    void StopPlay();
    bool IsPlaying();
    void TogglePlayStop();
    bool PlayStarted();
    
    BL_FLOAT GetBarPos();
    void SetBarPos(BL_FLOAT x);
    void ClearBar();

    void ResetPlayBar();
    bool PlayBarOutsideSelection();
    void UpdatePlayBar();

    void BarSetSelection(int x);
    bool IsBarActive();
    void SetBarActive(bool flag);
    BL_FLOAT GetNormDataBarPos();
    void SetDataBarPos(BL_FLOAT barPos);

    //
    void RewindView();
    void Translate(int dX);
    
    void UpdateZoom(BL_FLOAT zoomChange);
    void SetZoomCenter(int x);
    void UpdateZoomAdjust(bool updateBGZoom = false);
        
    bool GetNormDataSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                              BL_FLOAT *x1, BL_FLOAT *y1);
    BL_FLOAT GetNormCenterPos();
    void SetDataSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1);
    void GetDataSelection(BL_FLOAT dataSelection[4]);
    void SetDataSelection(const BL_FLOAT dataSelection[4]);
            
    void UpdateSpectroEdit();
    void UpdateSpectroEditSamples();

    void ReadSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
                              vector<WDL_TypedBuf<BL_FLOAT> > *phases,
                              BL_FLOAT x0, BL_FLOAT x1);
    void WriteSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
                               vector<WDL_TypedBuf<BL_FLOAT> > *phases,
                               BL_FLOAT x0, BL_FLOAT x1,
                               int fadeNumSamples = 0);
        
    void UpdateMiniView();
    void UpdateMiniViewData();
            
    void SetNeedRecomputeData(bool flag);
    void CheckRecomputeData();
    void DoRecomputeData();

    void RefreshData();
    void ResetSpectrogram(BL_FLOAT smapleRate);
    void RefreshSpectrogramView();
        
    //
    void PlugModeChanged(int prevMode);

    // Called after a command has been done
    void CommandDone();

    bool IsMouseOverGraph() const;
    
    static void CopyTrackSelection(const GhostTrack &src, GhostTrack *dst);
    
protected:
    void ClearControls(IGraphics *pGraphics);
        
    //
    void UpdateWaveform();
    void UpdateSpectrogramAlpha();
    
    // Time axis
    void UpdateTimeAxis();
    
    void CreateSpectrogramDisplay(bool createFromInit);
    void CreateGraphAxes();
    void CreateGraphCurves();

    void SetColorMap(int colorMapNum);

    void ResetTransforms();

    void ResetSpectrogramZoomAdjust();
        
    void UpdateSpectrogramData(bool fromOpenFile = false);

    // Selection
    void UpdateSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                         bool updateCenterPos, bool activateDrawSelection = false,
                         bool updateCustomControl = false);
    void UpdateSelection();
    bool IsSelectionActive();
    void SetSelectionActive(bool flag);
    void ConvertSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                          BL_FLOAT *x1, BL_FLOAT *y1,
                          BL_FLOAT *x0f, BL_FLOAT *y0f,
                          BL_FLOAT *x1f, BL_FLOAT *y1f);
    void RescaleSelection(Scale::Type srcScale, Scale::Type dstScale);
    void DataToViewRef(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1);
                          
    void FileChannelQueueToVec();

    void SetSamplesPyramidSamples();

    BL_FLOAT GraphNormXToTime(BL_FLOAT normX);
    BL_FLOAT GraphNormYToFreq(BL_FLOAT normY);
    BL_FLOAT GraphFreqToNormY(BL_FLOAT freq); //

    BL_FLOAT GraphNormYToNormFreq(BL_FLOAT normY);
    BL_FLOAT GraphNormFreqToNormY(BL_FLOAT normFreq);
    
    void ClipWaveform(WDL_TypedBuf<BL_FLOAT> *samples);

    void AdjustSamplesPyramidLatencyViewMode();
    void AdjustSpectroLatencyCaptureMode();

    // Reset all after file data was loaded or cleared
    void UpdateAfterNewFile();

    void SetLowFreqZoom(bool lowFreqZoom);
    Scale::Type GetYScaleType();

    // Recompute the full background spectrogram
    void RecomputeBGSpectrogram();

    // After sample rate changed in Ghost-X app, resample current data accordingly
    void ResampleCurrentData(BL_FLOAT prevSampleRate, BL_FLOAT newSampleRate);
    
protected:
    bool mIsInitialized;
    
    friend class Ghost;
    
    // Parameters (track)
    BL_FLOAT mRange;
    BL_FLOAT mContrast;
    int mColorMapNum;
    BL_FLOAT mWaveformScale;
    BL_FLOAT mSpectWaveformRatio;

    // Extra parameters
    bool mIsPlaying;
    
    // Parameters (global)
    int mPlugMode;
    bool mMonitorEnabled;
    //int mSelectionType;

    bool mLowFreqZoom;
    bool mMustRecomputeLowFreqZoom;
    
    // Command history
    GhostCommandHistory *mCommandHistory;
    
 protected:
    friend class GhostSamplesToSpectro;
    //
    int mBufferSize;
    BL_FLOAT mSampleRate;
    BL_FLOAT mPrevSampleRate;
    
    GraphControl12 *mGraph;

    // Used for plugin?
    FftProcessObj16 *mDispFftObj;
    SpectrogramFftObj2 *mSpectroDisplayObj;

    // Used for editing
    FftProcessObj16 *mEditFftObj;
    SpectroEditFftObj3 *mSpectroEditObjs[2];

    // Used for display
    // e.g when generating spectro image data when zooming or translating
    FftProcessObj16 *mDispGenFftObj;
    SpectroEditFftObj3 *mSpectroDispGenObjs[2];
    
    SpectrogramView2 *mSpectrogramView;
    SpectrogramDisplay3 *mSpectrogramDisplay;
    SpectrogramDisplay3::SpectrogramDisplayState *mSpectroDisplayState;
    
    BLSpectrogram4 *mSpectrogram;
    BLSpectrogram4 *mSpectrogramBG;
    
    MiniView2 *mMiniView;
    MiniView2::State *mMiniViewState;
    
    BL_FLOAT mZoomX;
    BL_FLOAT mZoomY;

    // Avoid infinite loop
    bool mIsUpdatingZoom;

    BlaTimer mRecomputeDataTimer;
    
    int mOversampling;
    
    BL_FLOAT mWaveformMax;
        
    vector<WDL_TypedBuf<BL_FLOAT> > mSamples;
    vector<WDL_TypedFastQueue<BL_FLOAT> > mSamplesQueue;

    // Keep the exact channel size when loaded
    // (because we can resize mSamples)
    long mLoadedChannelSize;
    
    GhostCustomDrawer *mCustomDrawer;
    GhostCustomDrawer::State *mCustomDrawerState;
    
    GhostCustomControl *mCustomControl;

    // When set to true, the edit zone will be shifted when
    // dragging the selection rectangle, and applied to the new zone
    //bool mCanMoveEdit;
    
    bool mNeedRecomputeData;
  
    bool mMustUpdateSpectrogram;
    bool mMustUpdateBGSpectrogram;
    
    // For save and save as
    char mCurrentFileName[FILENAME_SIZE];
    // For displaying a star near the filename if modified
    bool mFileModified;

    int mInternalFileFormat;

    SamplesPyramid3 *mSamplesPyramid;
    long mNumToPopTotal;
    
    GraphAxis2 *mHAxis;
    GraphTimeAxis6 *mTimeAxis;
    bool mMustUpdateTimeAxis;
    
    GraphAxis2 *mVAxis;
    GraphFreqAxis2 *mFreqAxis;

    GraphCurve5 *mWaveformCurve;
    GraphCurve5 *mWaveformOverlayCurve;
    GraphCurve5 *mMiniViewWaveformCurve;

    BL_FLOAT mTransportSamplePos;

    GhostSamplesToSpectro *mSamplesToSpectro;

    Scale::Type mYScale;
    Scale *mScale;

    bool mIsLoadingSaving;

    // For resizing
    int mPrevGraphWidth;
    int mPrevGraphHeight;

    GhostPluginInterface *mGhostPlug;

    // Origin
    GUIHelper12 *mGUIHelper;

    IGraphics *mGraphics;

    // Hack
    bool mIsResetting;

    bool mPrevSelectionActive;
    
 private:
    // Tmp buffers
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf31;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf32;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf33;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf34;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf35;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf36;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf37;
};

#endif
