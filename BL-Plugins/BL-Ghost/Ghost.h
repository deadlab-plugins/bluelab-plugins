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
 
#ifndef __GHOST__
#define __GHOST__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include "../../WDL/fastqueue.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsFile.h>
#include <BLUtilsPlug.h>

#include <Scale.h>

#include <ResizeGUIPluginInterface.h>
#include <GhostPluginInterface.h>

#include <SpectroMeter.h>

#include <ITabsBarControl.h>

#include "IControl.h"

// With 1024, we miss some frequencies
#define BUFFER_SIZE 2048

// TODO: decide if we provide several tracks for Ghost Lite
#ifdef APP_API
#if !GHOST_LITE_VERSION
#define MAX_NUM_TRACKS 16
#else
#define MAX_NUM_TRACKS 4
#endif
#else
#define MAX_NUM_TRACKS 1
#endif

using namespace iplug;
using namespace igraphics;

class GhostCommandCopyPaste;
class GhostCommand;

class IGUIResizeButtonControl;

class GUIHelper12;

class ParamSmoother2;

class GhostTrack;

// New version: re-create the graph each time we close/open the windows, or resize gui
// and keep the data in other objects, not in the graph.
class Ghost final : public Plugin,
    public ResizeGUIPluginInterface,
    public GhostPluginInterface,
    public ITabsBarListener
{
 public:
    Ghost(const InstanceInfo &info);
    virtual ~Ghost();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;
    
    void OnUIOpen() override;
    void OnUIClose() override;

    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;
  
    void PreResizeGUI(int guiSizeIdx,
                      int *outNewGUIWidth, int *outNewGUIHeight) override;

    void OnIdle() override;

#ifdef APP_API
    // Menus
    bool OnHostRequestingProductHelp() override;
    void OnHostRequestingMenuAction(int actionId) override;
#endif

    void OnParamReset(EParamSource source) override;

    // Tabs bar
    void OnTabSelected(int tabNum) override;
    void OnTabClose(int tabNum) override;
    
 protected:
    enum Quality
    {
        STANDARD = 0,
        HIGH,
        VERY_HIGH,
        OFFLINE
    };

    enum Action
    {
        ACTION_NO_ACTION = 0,

        // File menu
        ACTION_FILE_OPEN,
        ACTION_FILE_SAVE,
        ACTION_FILE_SAVE_AS,
        ACTION_FILE_EXPORT_SELECTION,
        ACTION_FILE_RELOAD,
        ACTION_FILE_CLOSE,
        
        // Edit menu
        ACTION_EDIT_UNDO,
        ACTION_EDIT_CUT,
        ACTION_EDIT_COPY,
        ACTION_EDIT_PASTE,
        ACTION_EDIT_GAIN,
        ACTION_EDIT_INPAINT,
        ACTION_RESET_PREFERENCES,
        // ID_PREFERENCES -> Audio Settings
        
        // Transport menu
        ACTION_TRANSPORT_PLAY_STOP,
        ACTION_TRANSPORT_RESET
    };

    enum InpaintDir
    {
        INPAINT_BOTH,
        INPAINT_HORIZONTAL,
        INPAINT_VERTICAL
    };
    
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics, int offset);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init();

    void GUIResizeParamChange(int guiSizeIdx);
    void GetNewGUISize(int guiSizeIdx, int *width, int *height);
    void PostResizeGUI();
 
    void GetGraphSize(int *width, int *height) override;

    // File drop handler
    void OnDrop(const char *filenames);
    
    // Key handler
    bool OnKey(const IKeyPress& key, bool isUp);

    void ProcessCurrentMenuAction();
    
    void UpdateWindowTitle();

    void ActivateTrack(int trackNum);
    void SetParametersToTrack(GhostTrack *track, bool setOnlyGlobalParams);
    void SetParametersFromTrack(GhostTrack *track);
    void SetParametersToAllTracks(bool setOnlyGlobalParams);
        
    void SetScrollSpeed(int scrollSpeedNum);
    void UpdateSpectrogramData(bool fromOpenFile = false);

    void ApplyGrayOutControls();
    void GrayOutEditSection(bool flag);
    
    void QualityChanged();
    
    void PlugModeChanged(PlugMode prevMode);

    void PromptForFileOpen();
    void PromptForFileSaveAs();
    void PromptForFileExportSelection();
    
    void OpenFile(const char *fileName) override;
    void SaveFile(const char *fileName);
    void CloseFile(int trackNum);
    
    // For ExportSelection
    bool SaveFile(const char *fileName,
                  const vector<WDL_TypedBuf<BL_FLOAT> > &channels);
    void ExportSelection(const char *fileName);

    void SaveAppPreferences();
    void LoadAppPreferences();
    void ResetAppPreferences();
        
    // Reset position to start play position
    void ResetPlayBar() override;
    bool PlayBarOutsideSelection() override;
    void UpdatePlayBar();

    void SetBarPos(BL_FLOAT x) override;
    BL_FLOAT GetBarPos() override;

    void ClearBar() override;

    // Set the play selection just after the bar has been put
    void BarSetSelection(int x) override;

    BL_FLOAT GetNormDataBarPos();
    void SetDataBarPos(BL_FLOAT normDataBarPos);

    bool IsBarActive() override;
    void SetBarActive(bool flag) override;
    
    void StartPlay() override;
    void StopPlay() override;
    bool PlayStarted() override;
    // Shortcut
    void TogglePlayStop();
    void SetPlayStopParameter(int value) override;

    BL_FLOAT GetNormCenterPos();
    void SetZoomCenter(int x) override;
    void UpdateZoom(BL_FLOAT zoomChange) override;
    void UpdateZoomAdjust(bool updateBGZoom = false);
    
    void SetNeedRecomputeData(bool flag) override;
    void CheckRecomputeData() override;
    
    void Translate(int dX) override;

    void RewindView() override;

    void ResetTransforms();

    void CursorMoved(BL_FLOAT x, BL_FLOAT y) override;
    void CursorOut() override;
    
    // Selection
    void UpdateSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                         bool updateCenterPos,
                         bool activateDrawSelection = false,
                         bool updateCustomControl = false) override;

    void UpdateSelectionAux(int trackNum,
                            BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                            bool updateCenterPos,
                            bool activateDrawSelection = false,
                            bool updateCustomControl = false);
    
    void ConvertSelection(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1,
                          BL_FLOAT *x0f, BL_FLOAT *y0f, BL_FLOAT *x1f, BL_FLOAT *y1f);
        
    bool IsSelectionActive() override;
    void SetSelectionActive(bool flag) override;

    // Set the GUI selection from the data selection
    void SetDataSelection(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1);

    void GetDataSelection(BL_FLOAT dataSelection[4]);
    void SetDataSelection(const BL_FLOAT dataSelection[4]);
    
    // Call this when the selection rectangle changed
    // Or when we go from selection rectangle to bar
    void SelectionChanged() override;
    void BeforeSelTranslation() override;
    void AfterSelTranslation() override;

    // Updates
    //
    void UpdateSpectroEdit();
    
    // To call after channels changed
    void UpdateSpectroEditSamples();
    
    void UpdateMiniView();
    void UpdateMiniViewData();
    
    void UpdateWaveform();
    
    void UpdateSpectrogramAlpha();
    
    void UpdateLogScale();
    
    // Commands
    void DoCutCommand() override;
    void DoCutCopyCommand() override;
    
    void DoGainCommand() override;

    void SetInpaintDir(InpaintDir dir);
    void SetSelectionType(SelectionType selectionType);
        
    void DoReplaceCommand() override;
    void DoReplaceCopyCommand() override;
    
    void DoCopyCommand() override;
    
    void DoPasteCommand() override;
    
    // Generic
    void DoCommand(GhostCommand *commands[2], int numTimes = 1);
    
    void UndoLastCommand() override;
    
    // Refresh the data after a command has mofied it 
    void RefreshData();
    
    // Refresh the data, but only in the command selection area
    void RefreshData(GhostCommand *command);
    
    enum PlugMode GetMode() override;
    
    // FIX: bad freq axis at high sample rates
    void UpdateFrequencyScale();
    
    // Reset all data when mode changed
    void ResetAllData();

    void CheckAppStartupArgs();

    // Tabs
    void NewTab(const char *fileName);

    // Generate the full tabs bar from existing tracks
    void GenerateTabsBar();

#if APP_API
    // Return true if we can open the file
    bool CheckMaxNumFiles();
#endif

    void SetLowFreqZoom(bool flag);

    void SetAllControlsToNull();

    void UpdatePrevMouse(float newGUIWidth, float newGUIHeight);
        
    // Secure starters
    SecureRestarter mSecureRestarter;
    ParamSmoother2 *mOutGainSmoother;
    
    IControl *mOpenFileSelector;
    IControl *mSaveFileSelector;
    IControl *mSaveAsFileSelector;
    IControl *mExportSelectionFileSelector;

    // Command
    GhostCommandCopyPaste *mCurrentCopyPasteCommands[2];
    
    // Set to true during loading or saving
    bool mIsLoadingSaving;
    
    // "Hack" for avoiding infinite loop + restoring correct size at initialization
    //
    // NOTE: imported from Waves
    //
    IGUIResizeButtonControl *mGUISizeSmallButton;
    IGUIResizeButtonControl *mGUISizeMediumButton;
    IGUIResizeButtonControl *mGUISizeBigButton;
    
    IGUIResizeButtonControl *mGUISizeHugeButton;

    GUIHelper12 *mGUIHelper;
    
    IBLSwitchControl *mMonitorControl;
    
    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;

    bool mIsInitialized;
    
    // For GUI resize
    int mGUIOffsetX;
    int mGUIOffsetY;
    
    // Edit controls
    IControl *mSelectionTypeControl;
    IControl *mEditGainControl;
    
    IControl *mCopyButton;
    IControl *mPasteButton;
    IControl *mCutButton;
    IControl *mGainButton;
    IControl *mInpaintButton;
    IControl *mUndoButton;
    
    IControl *mInpaintDirControl;
    IControl *mPlayButtonControl;

    Scale::Type mYScale;
    Action mCurrentAction;

    SpectroMeter *mSpectroMeter;
    BL_FLOAT mPrevMouseX;
    BL_FLOAT mPrevMouseY;
    bool mIsPlaying;
    
    vector<GhostTrack *> mTracks;
    
    bool mLinkTracks;

    // Parameters (shared with tracks)
    //
    BL_FLOAT mRange;
    BL_FLOAT mContrast;
    int mColorMapNum;
    BL_FLOAT mWaveformScale;
    BL_FLOAT mSpectWaveformRatio;

    // Parameter (global)
    int mGUISizeIdx;
    BL_FLOAT mOutGain;
    SelectionType mSelectionType;
    BL_FLOAT mEditGain;
    InpaintDir mInpaintDir;
    GhostPluginInterface::PlugMode mPlugMode;
    bool mMonitorEnabled;
    SpectroMeter::TimeMode mMeterTimeMode;
    SpectroMeter::FreqMode mMeterFreqMode;
    int mTrackNum;

    bool mInpaintProcessHorizontal;
    bool mInpaintProcessVertical; 
    BL_FLOAT mEditGainFactor;
    enum Quality mQuality;

    enum PlugMode mPrevPlugMode;
    bool mPlugModeChanged;
    
    BL_FLOAT mPrevSampleRate;

    char mCurrentLoadPath[FILENAME_SIZE];
    char mCurrentSavePath[FILENAME_SIZE];

    // Be sure not to check startup args more than 1 time
    bool mAppStartupArgsChecked;

    bool mLowFreqZoom;
    IControl *mLowfreqZoomCheckbox;
        
    // To update correctly time axis when monitor
    long int mNumSamplesMonitor;
    
    ITabsBarControl *mTabsBar;
    IGraphics *mGraphics;

    bool mNeedGrayOutControls;

    BLUtilsPlug mBLUtilsPlug;
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf7;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf9[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf10[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf11[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf12[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf13[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf14[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf15[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf16[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf17;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf18;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf19;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf20;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf21;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf22;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf23;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf24;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf25;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf30;
    
    BL_PROFILE_DECLARE;
};

#endif
