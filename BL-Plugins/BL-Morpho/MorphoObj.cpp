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
 
#include <SoSourceManager.h>
#include <SoSourcesView.h>
#include <SoSource.h>

#include <SySourceManager.h>
#include <SySourcesView.h>
#include <SySource.h>

#include <MorphoSyPipeline.h>

#include <BLUtils.h>
#include <BLUtilsFile.h>

#include <BLDebug.h>

#include <Morpho_defs.h>

#include "MorphoObj.h"


MorphoObj::MorphoObj(Plugin *plug,
                     View3DPluginInterface *view3DListener,
                     BL_FLOAT xyPadRatio)
{
    mPlug = plug;
    
    mMode = MORPHO_PLUG_MODE_SYNTH;
    
    mSoSourceManager = new SoSourceManager();
    mSySourceManager = new SySourceManager();

    mSoSourcesView = new SoSourcesView(plug, view3DListener, this, mSoSourceManager);
    mSySourcesView = new SySourcesView(plug, view3DListener, this, mSySourceManager);

    mSoListener = NULL;
    mSyListener = NULL;

    mPipeline = new MorphoSyPipeline(mSoSourceManager,
                                     mSySourceManager,
                                     xyPadRatio);
    
    memset(mCurrentLoadPath, '\0', FILENAME_SIZE);

    for (int i = 0; i < MAX_NUM_SOURCES; i++)
    {
        mXYPadHandleCoords[i][0] = 0.0;
        mXYPadHandleCoords[i][1] = 0.0;
    }

    SetSyWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
}

MorphoObj::~MorphoObj()
{
    delete mSoSourceManager;
    delete mSySourceManager;

    delete mSoSourcesView;
    delete mSySourcesView;

    delete mPipeline;
}

void
MorphoObj::SetSoSourcesViewListener(SoSourcesViewListener *listener)
{
    mSoSourcesView->SetListener(listener);

    mSoListener = listener;
}

void
MorphoObj::SetSySourcesViewListener(SySourcesViewListener *listener)
{
    mSySourcesView->SetListener(listener);

    mSyListener = listener;
}

void
MorphoObj::SetGUIHelper(GUIHelper12 *guiHelper)
{
    mSoSourcesView->SetGUIHelper(guiHelper);
    mSySourcesView->SetGUIHelper(guiHelper);
}

void
MorphoObj::SetSoSpectroGUIParams(int graphX, int graphY,
                                 const char *graphBitmapFN)
{
    mSoSourcesView->SetSpectroGUIParams(graphX, graphY, graphBitmapFN);
}

void
MorphoObj::SetSoWaterfallGUIParams(int graphX, int graphY,
                                 const char *graphBitmapFN)
{
    mSoSourcesView->SetWaterfallGUIParams(graphX, graphY, graphBitmapFN);
}

void
MorphoObj::SetSyWaterfallGUIParams(int graphX, int graphY,
                                 const char *graphBitmapFN)
{
    mSySourcesView->SetWaterfallGUIParams(graphX, graphY, graphBitmapFN);
}

void
MorphoObj::SetMode(MorphoPlugMode mode)
{
    mMode = mode;
}

void
MorphoObj::ClearGUI()
{
    mSoSourcesView->ClearGUI();
 
    mSySourcesView->ClearGUI();
}

void
MorphoObj::RefreshGUI()
{
    if (mMode == MORPHO_PLUG_MODE_SOURCES)
    {
        mSoSourcesView->Refresh();
        mSoSourcesView->RecreateControls();
    }
    else if (mMode == MORPHO_PLUG_MODE_SYNTH)
    {
        mSySourcesView->Refresh();
        mSySourcesView->RecreateControls();
    }

    RefreshCurrentSource();
}

void
MorphoObj::CreateNewLiveSource()
{
    CreateNewSoLiveSource();
    CreateNewSyLiveSource();

    // If we created the first source, set the master flag
    if (mSoSourceManager->GetNumSources() == 1)
        SetCurrentSourceMaster();
    
    RefreshCurrentSource();
}

void
MorphoObj::TryCreateNewFileSource()
{
    char fileName[FILENAME_SIZE];
    bool created = TryCreateNewSoFileSource(fileName);
    if (created)
    {
        // Use short name for Sy
        char *shortName = BLUtilsFile::GetFileName(fileName);
            
        CreateNewSyFileSource(shortName);
        
        // If we created the first source, set the master flag
        if (mSoSourceManager->GetNumSources() == 1)
            SetCurrentSourceMaster();
    }

    // If we created the first source, set the master flag
    if (mSoSourceManager->GetNumSources() == 1)
        SetCurrentSourceMaster();

    RefreshCurrentSource();
}

void
MorphoObj::RemoveSource(int sourceNum)
{
    // Check of the current source has the master flag
    bool masterFlag = false;
    SoSource *source = mSoSourceManager->GetCurrentSource();
    if (source != NULL)
        masterFlag = source->GetSourceMaster();

    // Remove the source
    mSoSourceManager->RemoveSource(sourceNum);
    mSoSourcesView->Refresh();

    mSySourceManager->RemoveSource(sourceNum + 1);
    mSySourcesView->Refresh();
    
    // If the removed source had the master flag,
    // or if we have only one source left, set the master flag
    if (masterFlag || (mSoSourceManager->GetNumSources() == 1))
        SetCurrentSourceMaster();
    
    RefreshCurrentSource();
}

int
MorphoObj::GetNumSources() const
{
    int numSources = mSoSourceManager->GetNumSources();
    
    return numSources;
}

void
MorphoObj::SetTabsBar(ITabsBarControl *control)
{
    mSoSourcesView->SetTabsBar(control);

    if (control != NULL)
        GenerateTabsBar();
}

void
MorphoObj::SetCurrentSoSourceIdx(int index)
{
    mSoSourceManager->SetCurrentSourceIdx(index);
    mSySourceManager->SetCurrentSourceIdx(index + 1); //  // manage the "mix" source

    RefreshCurrentSource();
}

void
MorphoObj::SetCurrentSySourceIdx(int index)
{
    if (index > 0)
        mSoSourceManager->SetCurrentSourceIdx(index - 1); // manage the "mix" source
    mSySourceManager->SetCurrentSourceIdx(index);

    RefreshCurrentSource();
}

void
MorphoObj::SetCurrentSourceMaster()
{
    // So
    int soCurrentIdx = mSoSourceManager->GetCurrentSourceIdx();
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        if (source != NULL)
        {
            if (i == soCurrentIdx)
                source->SetSourceMaster(true);
            else
                source->SetSourceMaster(false);
        }
    }
    //mSoSourcesView->Refresh();

    // Sy
    int syCurrentIdx = mSySourceManager->GetCurrentSourceIdx();
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);
        if (source != NULL)
        {
            if (i == syCurrentIdx)
                source->SetSourceMaster(true);
            else
                source->SetSourceMaster(false);
        }
    }
}

// "Apply"
void
MorphoObj::SoComputeFileFrames()
{
    SoSource *soSource = mSoSourceManager->GetCurrentSource();
    if (soSource != NULL)
    {
        vector<MorphoFrame7> frames;
        soSource->ComputeFileFrames(&frames);
        
        int currentSourceIdx = mSoSourceManager->GetCurrentSourceIdx();
        SySource *sySource = mSySourceManager->GetSource(currentSourceIdx + 1);
        if (sySource != NULL)
            sySource->SetFileMorphoFrames(frames);

        mSySourcesView->Refresh();
    }
}

void
MorphoObj::CreateNewSoLiveSource()
{
    mSoSourcesView->NewTab();
    
    SoSource *source = mSoSourceManager->GetCurrentSource();
    if (source != NULL)
        source->SetTypeLiveSource();
    
    mSoSourcesView->Refresh();
}

bool
MorphoObj::TryCreateNewSoFileSource(char outFileName[FILENAME_SIZE])
{
    WDL_String fileNameStr;

    bool fileOk =
        BLUtilsFile::PromptForFileOpenAudio(mPlug, mCurrentLoadPath, &fileNameStr);

    if (!fileOk)
        return false;

    const char *fileName = fileNameStr.Get();
    sprintf(outFileName, fileName);

    // Keep the load path
    BLUtilsFile::GetFilePath(fileName, mCurrentLoadPath, true);
    
    mSoSourcesView->NewTab();

    // Set file name
    SoSource *soSource = mSoSourceManager->GetCurrentSource();
    if (soSource != NULL)
        soSource->SetTypeFileSource(fileName);
    
    mSoSourcesView->Refresh();

    return true;
}

void
MorphoObj::SetSoSourcePlaying(bool flag)
{ 
    SoSource *source = mSoSourceManager->GetCurrentSource();
    if (source != NULL)
        source->SetPlaying(flag);
}

bool
MorphoObj::GetSoSourcePlaying() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->IsPlaying();

    return false;
}

void
MorphoObj::SetSoSpectroBrightness(BL_FLOAT brightness)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();
    if (source != NULL)
        source->SetSpectroBrightness(brightness);
}

BL_FLOAT
MorphoObj::GetSoSpectroBrightness() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSpectroBrightness();

    return 0.0;
}

void
MorphoObj::SetSoSpectroContrast(BL_FLOAT contrast)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSpectroContrast(contrast);
}

BL_FLOAT
MorphoObj::GetSoSpectroContrast() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSpectroContrast();

    return 0.0;
}

void
MorphoObj::SetSoSpectroSpecWave(BL_FLOAT specWave)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSpectroSpecWave(specWave);
}

BL_FLOAT
MorphoObj::GetSoSpectroSpecWave() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSpectroSpecWave();

    return 0.0;
}

void
MorphoObj::SetSoSpectroWaveformScale(BL_FLOAT waveScale)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSpectroWaveformScale(waveScale);
}

BL_FLOAT
MorphoObj::GetSoSpectroWaveformScale() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSpectroWaveformScale();

    return 0.0;
}

void
MorphoObj::SetSoSpectroSelectionType(SelectionType type)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSpectroSelectionType(type);
}

SelectionType
MorphoObj::GetSoSpectroSelectionType() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSpectroSelectionType();

    return RECTANGLE;
}

void
MorphoObj::SetSoWaterfallViewMode(WaterfallViewMode mode)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetWaterfallViewMode(mode);
}

WaterfallViewMode
MorphoObj::GetSoWaterfallViewMode() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetWaterfallViewMode();

    return MORPHO_WATERFALL_VIEW_DETECTION;
}

void
MorphoObj::SetSoSourceMaster(bool masterSource)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSourceMaster(masterSource);
}

bool
MorphoObj::GetSoSourceMaster() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceMaster();

    return false;
}

void
MorphoObj::SetSoSourceType(SoSourceType sourceType)
{
    SoSource *soSource = mSoSourceManager->GetCurrentSource();
    if (soSource != NULL)
        soSource->SetSourceType(sourceType);

    // Transmit the type to sy source, so we can know if we resynth be "source"
    // or by standard Morpho
    SySource *sySource = mSySourceManager->GetCurrentSource();
    if (sySource != NULL)
        sySource->SetSoSourceType(sourceType);
    
    CheckSynthType();

    RefreshCurrentSource();
}

SoSourceType
MorphoObj::GetSoSourceType() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceType();

    return MORPHO_SOURCE_TYPE_BYPASS;
}

void
MorphoObj::SetSoTimeSmoothCoeff(BL_FLOAT coeff)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetTimeSmoothCoeff(coeff);

    // Notify the GUI for apply button (so source has been touched)
    RefreshCurrentSource();
}

BL_FLOAT
MorphoObj::GetSoTimeSmoothCoeff() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetTimeSmoothCoeff();

    return 0.0;
}

void
MorphoObj::SetSoDetectThreshold(BL_FLOAT detectThrs)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetDetectThreshold(detectThrs);

    // Notify the GUI for apply button (so source has been touched)
    RefreshCurrentSource();
}

BL_FLOAT
MorphoObj::GetSoDetectThreshold() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetDetectThreshold();

    return 0.0;
}

void
MorphoObj::SetSoFreqThreshold(BL_FLOAT freqThrs)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetFreqThreshold(freqThrs);

    // Notify the GUI for apply button (so source has been touched)
    RefreshCurrentSource();
}

BL_FLOAT
MorphoObj::GetSoFreqThreshold() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetFreqThreshold();

    return 0.0;
}

void
MorphoObj::SetSoSourceGain(BL_FLOAT sourceGain)
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSourceGain(sourceGain);
}

BL_FLOAT
MorphoObj::GetSoSourceGain() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceGain();

    return 0.0;
}

void
MorphoObj::SetSoWaterfallCameraAngle0(BL_FLOAT angle)
{
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraAngle0(angle);
    }
}

void
MorphoObj::SetSoWaterfallCameraAngle1(BL_FLOAT angle)
{
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraAngle1(angle);
    }
}

void
MorphoObj::SetSoWaterfallCameraFov(BL_FLOAT angle)
{
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraFov(angle);
    }
}

bool
MorphoObj::GetSoApplyEnabled() const
{
    SoSource *source = mSoSourceManager->GetCurrentSource();
    if (source == NULL)
        return false;

    if (source->GetType() != SoSource::FILE)
        return false;

    bool applyEnabled = source->IsTouched();

    return applyEnabled;
}

void
MorphoObj::GenerateTabsBar()
{
    int numTabs = mSoSourceManager->GetNumSources();
    mSoSourcesView->ReCreateTabs(numTabs);
    
    int sourceIdx = mSoSourceManager->GetCurrentSourceIdx();
    mSoSourcesView->SelectTab(sourceIdx);

    mSoSourcesView->Refresh();
}

void
MorphoObj::SetXYPad(IXYPadControlExt *control)
{
    mSySourcesView->SetXYPad(control);

    // Will call plug::OnParamChange() for every handle
    if (control != NULL)
        control->RefreshAllHandlesParams();
}

void
MorphoObj::SetIconLabel(IIconLabelControl *control)
{
    mSySourcesView->SetIconLabel(control);
}

void
MorphoObj::CreateNewSyLiveSource()
{
    // Get the mode of the current source
    WaterfallViewMode mode = GetSyWaterfallViewMode();
    
    mSySourceManager->NewSource();
    
    SySource *source = mSySourceManager->GetCurrentSource();
    if (source != NULL)
        source->SetTypeLiveSource();

    // Update to the current view wmode
    if (source != NULL)
        source->SetWaterfallViewMode(mode);
        
    mSySourcesView->Refresh();
}

void
MorphoObj::CreateNewSyFileSource(const char *fileName)
{
    // Get the mode of the current source
    WaterfallViewMode mode = GetSyWaterfallViewMode();
    
    mSySourceManager->NewSource();

    SySource *source = mSySourceManager->GetCurrentSource();
    if (source != NULL)
        source->SetTypeFileSource(fileName);

    // Update to the current view mode
    if (source != NULL)
        source->SetWaterfallViewMode(mode);
    
    mSySourcesView->Refresh();
}

void
MorphoObj::SetSySourceSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSourceSolo(flag);
}

SySource::Type
MorphoObj::GetSySourceType() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetType();

    return SySource::NONE;
}

bool
MorphoObj::GetSySourceSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceSolo();

    return false;
}
    
void
MorphoObj::SetSySourceMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSourceMute(flag);

    // Refresh handle (gray out or not?)
    mSySourcesView->Refresh();
}

bool
MorphoObj::GetSySourceMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceMute();

    return false;
}
    
void
MorphoObj::SetSySourceMaster(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSourceMaster(flag);
}

bool
MorphoObj::GetSySourceMaster() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSourceMaster();

    return false;
}

void
MorphoObj::SetSyAmp(BL_FLOAT amp)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetAmp(amp);
}

BL_FLOAT
MorphoObj::GetSyAmp() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetAmp();

    return 1.0;
}

void
MorphoObj::SetSyAmpSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetAmpSolo(flag);
}

bool
MorphoObj::GetSyAmpSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetAmpSolo();

    return false;
}
    
void
MorphoObj::SetSyAmpMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetAmpMute(flag);
}

bool
MorphoObj::GetSyAmpMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetAmpMute();

    return false;
}
    
void
MorphoObj::SetSyPitch(BL_FLOAT pitch)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetPitch(pitch);
}

BL_FLOAT
MorphoObj::GetSyPitch() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetPitch();

    return 1.0;
}

void
MorphoObj::SetSyPitchSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetPitchSolo(flag);
}

bool
MorphoObj::GetSyPitchSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetPitchSolo();

    return false;
}
    
void
MorphoObj::SetSyPitchMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetPitchMute(flag);
}

bool
MorphoObj::GetSyPitchMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetPitchMute();

    return false;
}
    
void
MorphoObj::SetSyColor(BL_FLOAT color)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetColor(color);
}

BL_FLOAT
MorphoObj::GetSyColor() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetColor();

    return 1.0;
}

void
MorphoObj::SetSyColorSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetColorSolo(flag);
}

bool
MorphoObj::GetSyColorSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetColorSolo();

    return false;
}
    
void
MorphoObj::SetSyColorMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetColorMute(flag);
}

bool
MorphoObj::GetSyColorMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetColorMute();

    return false;
}

void
MorphoObj::SetSyWarping(BL_FLOAT warping)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetWarping(warping);
}

BL_FLOAT
MorphoObj::GetSyWarping() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetWarping();

    return 1.0;
}

void
MorphoObj::SetSyWarpingSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetWarpingSolo(flag);
}

bool
MorphoObj::GetSyWarpingSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetWarpingSolo();

    return false;
}
    
void
MorphoObj::SetSyWarpingMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetWarpingMute(flag);
}

bool
MorphoObj::GetSyWarpingMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetWarpingMute();

    return false;
}

void
MorphoObj::SetSyNoise(BL_FLOAT noise)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetNoise(noise);
}

BL_FLOAT
MorphoObj::GetSyNoise() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetNoise();

    return 1.0;
}

void
MorphoObj::SetSyNoiseSolo(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetNoiseSolo(flag);
}

bool
MorphoObj::GetSyNoiseSolo() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetNoiseSolo();

    return false;
}
    
void
MorphoObj::SetSyNoiseMute(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetNoiseMute(flag);
}

bool
MorphoObj::GetSyNoiseMute() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetNoiseMute();

    return false;
}

void
MorphoObj::SetSyReverse(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetReverse(flag);
}

bool
MorphoObj::GetSyReverse() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetReverse();

    return false;
}

void
MorphoObj::SetSyPingPong(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetPingPong(flag);
}

bool
MorphoObj::GetSyPingPong() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetPingPong();

    return false;
}

void
MorphoObj::SetSyFreeze(bool flag)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetFreeze(flag);
}

bool
MorphoObj::GetSyFreeze() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetFreeze();

    return false;
}

void
MorphoObj::SetSySynthType(SySourceSynthType type)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetSynthType(type);
}

SySourceSynthType
MorphoObj::GetSySynthType() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetSynthType();

    return MORPHO_SOURCE_ALL_PARTIALS;
}

#if 0 // Set it for the current source
void
MorphoObj::SetSyWaterfallViewMode(WaterfallViewMode mode)
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        source->SetWaterfallViewMode(mode);
}
#endif
#if 1 // Set it for all the sources
void
MorphoObj::SetSyWaterfallViewMode(WaterfallViewMode mode)
{
    for (int i = 0; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);

        if (source != NULL)
            source->SetWaterfallViewMode(mode);
    }
}
#endif

WaterfallViewMode
MorphoObj::GetSyWaterfallViewMode() const
{
    SySource *source = mSySourceManager->GetCurrentSource();

    if (source != NULL)
        return source->GetWaterfallViewMode();

    return MORPHO_WATERFALL_VIEW_DETECTION;
}

void
MorphoObj::SetSyWaterfallCameraAngle0(BL_FLOAT angle)
{
    for (int i = 0; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraAngle0(angle);
    }
}

void
MorphoObj::SetSyWaterfallCameraAngle1(BL_FLOAT angle)
{
    for (int i = 0; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraAngle1(angle);
    }
}

void
MorphoObj::SetSyWaterfallCameraFov(BL_FLOAT angle)
{
    for (int i = 0; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);
        if (source != NULL)
            source->SetWaterfallCameraFov(angle);
    }
}

void
MorphoObj::SetSyPadHandleX(int handleNum, BL_FLOAT value)
{
    mXYPadHandleCoords[handleNum][0] = value;

    if (handleNum < mSySourceManager->GetNumSources())
    {
        SySource *source = mSySourceManager->GetSource(handleNum);
        source->SetMixPos(mXYPadHandleCoords[handleNum][0],
                          mXYPadHandleCoords[handleNum][1]);
    }
}

void
MorphoObj::SetSyPadHandleY(int handleNum, BL_FLOAT value)
{
    mXYPadHandleCoords[handleNum][1] = value;

    if (handleNum < mSySourceManager->GetNumSources())
    {
        SySource *source = mSySourceManager->GetSource(handleNum);
        source->SetMixPos(mXYPadHandleCoords[handleNum][0],
                          mXYPadHandleCoords[handleNum][1]);
    }
}

void
MorphoObj::SetSyLoop(bool flag)
{
    mPipeline->SetLoop(flag);
}

bool
MorphoObj::GetSyLoop() const
{
    return mPipeline->GetLoop();
}

void
MorphoObj::SetSyTimeStretchFactor(BL_FLOAT factor)
{
    // input: [-10, 10]
    
    BL_FLOAT t = factor/MAX_TIME_STRETCH;
    if (factor >= 0.0) // [0, 10] -> [1, 10]
        factor = 1.0 + t*(MAX_TIME_STRETCH - 1);
    else
        // [-10, 0] -> [-10, -1]
        factor = -1.0 + t*(MAX_TIME_STRETCH - 1);
            
    if (factor < 0.0)
        factor = -1.0/factor;
    
    mPipeline->SetTimeStretchFactor(factor);
}

BL_FLOAT
MorphoObj::GetSyStimeStretchFactor() const
{
    BL_FLOAT factor = mPipeline->GetTimeStretchFactor();

    if (factor < 1.0)
        factor = -1.0/factor;

    return factor;
}

void
MorphoObj::SetSyOutGain(BL_FLOAT gain)
{
    mPipeline->SetGain(gain);
}

BL_FLOAT
MorphoObj::GetSyOutGain() const
{
    return mPipeline->GetGain();
}

SoSourceType
MorphoObj::GetSySourceSynthType() const
{
    SySource *source = mSySourceManager->GetCurrentSource();
    if (source != NULL)
        return source->GetSoSourceType();

    return SoSourceType::MORPHO_SOURCE_TYPE_COMPLEX;
}

void
MorphoObj::Reset(BL_FLOAT sampleRate)
{
    mSoSourceManager->Reset(sampleRate);
    mSySourceManager->Reset(sampleRate);

    mPipeline->Reset(sampleRate);
}

void
MorphoObj::ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &in,
                        vector<WDL_TypedBuf<BL_FLOAT> > &out,
                        bool isTransportPlaying,
                        BL_FLOAT transportSamplePos)
{
    int numOutputs = out.size();
    
    // Output silence by default
    BLUtils::FillAllZero(&out);

    SoSource *source = mSoSourceManager->GetCurrentSource();
    bool currentSoSourcePlaying = ((source != NULL) && source->IsPlaying());
    if ((mMode == MORPHO_PLUG_MODE_SOURCES) &&
        currentSoSourcePlaying) // make possible to play sy while selecting on so
    {
        BLUtils::StereoToMono(&in, false);

        out.resize(1);
        
        //SoSource *source = mSoSourceManager->GetCurrentSource();
        if (source != NULL)
            source->ProcessBlock(in, out,
                                 isTransportPlaying, transportSamplePos);
    }
    else if ((mMode == MORPHO_PLUG_MODE_SYNTH) ||
             // make possible to play sy while selecting on so
             ((mMode == MORPHO_PLUG_MODE_SOURCES) &&
              !currentSoSourcePlaying))
    {
        BLUtils::StereoToMono(&in, false);
        out.resize(1);
        
        UpdateSyLiveSources(in);
        
        mPipeline->ProcessBlock(out);
    }

    // Mono to bi-mono?
    if (numOutputs == 2)
    {
        out.resize(2);
        out[1] = out[0];
    }
}

void
MorphoObj::OnUIOpen()
{
    mSoSourcesView->OnUIOpen();
    mSySourcesView->OnUIOpen();
}

void
MorphoObj::OnUIClose()
{
    mSoSourcesView->OnUIClose();
    mSySourcesView->OnUIClose();

    // Destroy all the nanovg objects, since we are still in the OpenGL context
    // (if we do it after UI is closed, it would crash because there won't be
    // any OpenGL context)
    mSoSourcesView->ClearGUI();
    mSySourcesView->ClearGUI();
}

void
MorphoObj::OnIdle()
{
    if (mMode == MORPHO_PLUG_MODE_SOURCES)
        mSoSourcesView->OnIdle();
    else if (mMode == MORPHO_PLUG_MODE_SYNTH)
        mSySourcesView->OnIdle();
}

void
MorphoObj::RefreshCurrentSource()
{
    if (mSoListener != NULL)
    {
        SoSource *source = mSoSourceManager->GetCurrentSource();
        mSoListener->SoSourceChanged(source);
    }

    if (mSyListener != NULL)
    {
        SySource *source = mSySourceManager->GetCurrentSource();
        mSyListener->SySourceChanged(source);
    }
}

void
MorphoObj::UpdateSyLiveSources(const vector<WDL_TypedBuf<BL_FLOAT> > &in)
{
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *soSource = mSoSourceManager->GetSource(i);
        if (soSource == NULL)
            continue;
        
        if (soSource->GetType() != SoSource::LIVE)
            continue;

        MorphoFrame7 frame;
        bool frameComputed = soSource->ComputeLiveFrame(in, &frame);

        if (frameComputed)
        {
            SySource *sySource = mSySourceManager->GetSource(i + 1);
            if (sySource == NULL)
                continue;

            sySource->SetLiveMorphoFrame(frame);
        }
    }
}

void
MorphoObj::CheckSynthType()
{
    // If any source is of type "resynth", set the pipeline synth type to "resynth"

    // TODO later: mix "source" and Morpho synthesis

    MorphoFrameSynth2::SynthMode synthMode = MorphoFrameSynth2::SOURCE_PARTIALS;
    for (int i = 0; i < mSySourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        if (source != NULL)
        {
            if (source->GetSourceType() ==
                SoSourceType::MORPHO_SOURCE_TYPE_MONOPHONIC)
            {
                synthMode = MorphoFrameSynth2::RESYNTH_PARTIALS;
            }
        }
    }

    mPipeline->SetSynthMode(synthMode);
}
