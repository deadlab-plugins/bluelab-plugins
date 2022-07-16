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
 
#ifndef MORPHO_OBJ_H
#define MORPHO_OBJ_H

#include <BLTypes.h>
#include <BLUtilsFile.h>
#include <Morpho_defs.h>

#include <SySource.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;

class SoSourceManager;
class SySourceManager;
class SoSourcesView;
class SoSourcesViewListener;
class SySourcesView;
class SySourcesViewListener;
class IXYPadControlExt;
class ITabsBarControl;
class IIconLabelControl;
class GUIHelper12;
class MorphoSyPipeline;
class MorphoObj
{
public:
    MorphoObj(Plugin *plug, View3DPluginInterface *view3DListener,
              BL_FLOAT xyPadRatio);
    virtual ~MorphoObj();

    void SetSoSourcesViewListener(SoSourcesViewListener *listener);
    void SetSySourcesViewListener(SySourcesViewListener *listener);

    void SetGUIHelper(GUIHelper12 *guiHelper);
    void SetSoSpectroGUIParams(int graphX, int graphY,
                               const char *graphBitmapFN);
    void SetSoWaterfallGUIParams(int graphX, int graphY,
                                 const char *graphBitmapFN);
    void SetSyWaterfallGUIParams(int graphX, int graphY,
                                 const char *graphBitmapFN);
    
    void SetMode(MorphoPlugMode mode);
    void ClearGUI();
    void RefreshGUI();

    void Reset(BL_FLOAT sampleRate);

    void ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &in,
                      vector<WDL_TypedBuf<BL_FLOAT> > &out,
                      bool isTransportPlaying,
                      BL_FLOAT transportSamplePos);

    void OnUIOpen();
    void OnUIClose();

    void OnIdle();
    
    // Sources
    //
    
    void CreateNewLiveSource();
    void TryCreateNewFileSource();
    void RemoveSource(int sourceNum);
    int GetNumSources() const;
    
    void SetTabsBar(ITabsBarControl *control);

    void SetCurrentSoSourceIdx(int index);
    void SetCurrentSySourceIdx(int index);

    void SetCurrentSourceMaster();

    // Per SoSource parameters
    //
    void SoComputeFileFrames(); // "Apply"
    
    void SetSoSourcePlaying(bool flag);
    bool GetSoSourcePlaying() const;
    
    void SetSoSpectroBrightness(BL_FLOAT brightness);
    BL_FLOAT GetSoSpectroBrightness() const;
    
    void SetSoSpectroContrast(BL_FLOAT contrast);
    BL_FLOAT GetSoSpectroContrast() const;
    
    void SetSoSpectroSpecWave(BL_FLOAT specWave);
    BL_FLOAT GetSoSpectroSpecWave() const;

    void SetSoSpectroWaveformScale(BL_FLOAT waveScale);
    BL_FLOAT GetSoSpectroWaveformScale() const;

    void SetSoSpectroSelectionType(SelectionType type);
    SelectionType GetSoSpectroSelectionType() const;
    
    void SetSoWaterfallViewMode(WaterfallViewMode mode);
    WaterfallViewMode GetSoWaterfallViewMode() const;
    
    void SetSoSourceMaster(bool masterSource);
    bool GetSoSourceMaster() const;
    
    void SetSoSourceType(SoSourceType sourceType);
    SoSourceType GetSoSourceType() const;
    
    void SetSoTimeSmoothCoeff(BL_FLOAT coeff);
    BL_FLOAT GetSoTimeSmoothCoeff() const;
    
    void SetSoDetectThreshold(BL_FLOAT detectThrs);
    BL_FLOAT GetSoDetectThreshold() const;
    
    void SetSoFreqThreshold(BL_FLOAT freqThrs);
    BL_FLOAT GetSoFreqThreshold() const;
    
    void SetSoSourceGain(BL_FLOAT sourceGain);
    BL_FLOAT GetSoSourceGain() const;
    
    // Waterfall
    void SetSoWaterfallCameraAngle0(BL_FLOAT angle);
    void SetSoWaterfallCameraAngle1(BL_FLOAT angle);
    void SetSoWaterfallCameraFov(BL_FLOAT angle);

    bool GetSoApplyEnabled() const;
    
    // Synth
    //
    void SetXYPad(IXYPadControlExt *control);
    void SetIconLabel(IIconLabelControl *control);
    
    SySource::Type GetSySourceType() const;
    
    // Per SySource parameters
    //
    void SetSySourceSolo(bool flag);
    bool GetSySourceSolo() const;
    
    void SetSySourceMute(bool flag);
    bool GetSySourceMute() const;
    
    void SetSySourceMaster(bool flag);
    bool GetSySourceMaster() const;

    void SetSyAmp(BL_FLOAT amp);
    BL_FLOAT GetSyAmp() const;

    void SetSyAmpSolo(bool flag);
    bool GetSyAmpSolo() const;
    
    void SetSyAmpMute(bool flag);
    bool GetSyAmpMute() const;
    
    void SetSyPitch(BL_FLOAT pitch);
    BL_FLOAT GetSyPitch() const;

    void SetSyPitchSolo(bool flag);
    bool GetSyPitchSolo() const;
    
    void SetSyPitchMute(bool flag);
    bool GetSyPitchMute() const;
    
    void SetSyColor(BL_FLOAT color);
    BL_FLOAT GetSyColor() const;

    void SetSyColorSolo(bool flag);
    bool GetSyColorSolo() const;
    
    void SetSyColorMute(bool flag);
    bool GetSyColorMute() const;

    void SetSyWarping(BL_FLOAT warping);
    BL_FLOAT GetSyWarping() const;

    void SetSyWarpingSolo(bool flag);
    bool GetSyWarpingSolo() const;
    
    void SetSyWarpingMute(bool flag);
    bool GetSyWarpingMute() const;

    void SetSyNoise(BL_FLOAT noise);
    BL_FLOAT GetSyNoise() const;

    void SetSyNoiseSolo(bool flag);
    bool GetSyNoiseSolo() const;
    
    void SetSyNoiseMute(bool flag);
    bool GetSyNoiseMute() const;

    void SetSyReverse(bool flag);
    bool GetSyReverse() const;

    void SetSyPingPong(bool flag);
    bool GetSyPingPong() const;

    void SetSyFreeze(bool flag);
    bool GetSyFreeze() const;
    
    void SetSySynthType(SySourceSynthType type);
    SySourceSynthType GetSySynthType() const;

    // Will be triggered when corresponding knobs turn
    void SetSyWaterfallViewMode(WaterfallViewMode mode);
    WaterfallViewMode GetSyWaterfallViewMode() const;
    
    // Waterfall
    void SetSyWaterfallCameraAngle0(BL_FLOAT angle);
    void SetSyWaterfallCameraAngle1(BL_FLOAT angle);
    void SetSyWaterfallCameraFov(BL_FLOAT angle);

    // XY pad handles
    void SetSyPadHandleX(int handleNum, BL_FLOAT value);
    void SetSyPadHandleY(int handleNum, BL_FLOAT value);
    
    // SyPipeline
    void SetSyLoop(bool flag);
    bool GetSyLoop() const;

    void SetSyTimeStretchFactor(BL_FLOAT factor);
    BL_FLOAT GetSyStimeStretchFactor() const;

    void SetSyOutGain(BL_FLOAT gain);
    BL_FLOAT GetSyOutGain() const;

    SoSourceType GetSySourceSynthType() const;
    
protected:
    // Sources
    
    // SoSource
    void CreateNewSoLiveSource();
    bool TryCreateNewSoFileSource(char outFileName[FILENAME_SIZE]);

    // SySource
    void CreateNewSyLiveSource();
    void CreateNewSyFileSource(const char *fileName);

    void GenerateTabsBar();

    void RefreshCurrentSource();

    void UpdateSyLiveSources(const vector<WDL_TypedBuf<BL_FLOAT> > &in);

    void CheckSynthType();
        
    //
    Plugin *mPlug;
    
    MorphoPlugMode mMode;
    
    SoSourceManager *mSoSourceManager;
    SySourceManager *mSySourceManager;

    SoSourcesView *mSoSourcesView;
    SySourcesView *mSySourcesView;

    // Take listeners too. So can be notified not only when
    // tabs or handles change
    SoSourcesViewListener *mSoListener;
    SySourcesViewListener *mSyListener;

    MorphoSyPipeline *mPipeline;
    
    // For file open
    char mCurrentLoadPath[FILENAME_SIZE];

    // Include the "mix" source
    BL_FLOAT mXYPadHandleCoords[MAX_NUM_SOURCES + 1][2];
};

#endif
