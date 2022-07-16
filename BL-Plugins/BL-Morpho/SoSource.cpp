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
 
#include <SoLiveSource.h>
#include <SoFileSource.h>
#include <SoSourceImpl.h>

#include <GhostTrack2.h>
#include <MorphoSoPipeline.h>

#include <MorphoWaterfallGUI.h>
#include <BorderCustomDrawer.h>

#include <BLUtils.h>

#include "SoSource.h"

SoSource::SoSource(BL_FLOAT sampleRate)
: WaterfallSource(sampleRate, MORPHO_PLUG_MODE_SOURCES)
{
    mType = NONE;

    mSourceImpl = NULL;

    // Parameters
    mIsPlaying = false;
    
    mBrightness = 0.0;
    mContrast = 0.5;
    mSpecWave = 50.0;
    mWaveScale = 1.0;
    
    mSourceMaster = false;
    mSourceType = MORPHO_SOURCE_TYPE_COMPLEX;
    mTimeSmooth = 50.0;
    mDetectThreshold = 1.0;
    mFreqThreshold = 25.0; //50.0;
    mSourceGain = 1.0;

    mSampleRate = sampleRate;

    // GhostTrack
    mGhostTrack = new GhostTrack2(BUFFER_SIZE, sampleRate, Scale::MEL,
                                  GhostTrack2::EDIT);

    // Same as in RebalanceStereo
    mGhostTrack->SetColorMap(ColorMapFactory::COLORMAP_PURPLE2);

    // Parameters
    mGhostTrack->UpdateParamRange(mBrightness);
    mGhostTrack->UpdateParamContrast(mContrast);
    mGhostTrack->UpdateParamSpectWaveformRatio(mSpecWave*0.01);
    mGhostTrack->UpdateParamWaveformScale(mWaveScale);
    
    // RTPipeline
    mPipeline = new MorphoSoPipeline(sampleRate);
    mPipeline->SetTimeSmoothCoeff(mTimeSmooth);
    mPipeline->SetDetectThreshold(mDetectThreshold);
    mPipeline->SetFreqThreshold(mFreqThreshold);
    mPipeline->SetGain(mSourceGain);

    mIsTouched = true;
}

SoSource::~SoSource()
{
    if (mSourceImpl != NULL)
        delete mSourceImpl;
    
    delete mGhostTrack;
    delete mPipeline;
}

SoSource::Type
SoSource::GetType() const
{
    return mType;
}

void
SoSource::SetTypeFileSource(const char *fileName)
{
    if (mSourceImpl != NULL)
        return;
    
    mSourceImpl = new SoFileSource(mGhostTrack);

    if (fileName != NULL)
        ((SoFileSource *)mSourceImpl)->SetFileName(fileName);

    mType = FILE;
}

void
SoSource::SetTypeLiveSource()
{
    if (mSourceImpl != NULL)
        return;
    
    mSourceImpl = new SoLiveSource(mGhostTrack);
    
    mType = LIVE;
}

void
SoSource::GetName(char name[FILENAME_SIZE])
{
    memset(name, '\0', FILENAME_SIZE);
    
    if ((mType == NONE) || (mSourceImpl == NULL))
    {
        strcpy(name, "");

        return;
    }

    mSourceImpl->GetName(name);
}

void
SoSource::ComputeFileFrames(vector<MorphoFrame7> *frames)
{
    if (mType == FILE)
    {
        mWaterfallGUI->Lock();

        vector<WDL_TypedBuf<BL_FLOAT> > samples;
        mGhostTrack->GetSamples(&samples);

        vector<WDL_TypedBuf<BL_FLOAT> > out;
            
        mPipeline->ProcessBlock(samples, out, frames);
    
        mWaterfallGUI->Unlock();
        mWaterfallGUI->PushAllData();

        mIsTouched = false;
    }
}

bool
SoSource::ComputeLiveFrame(const vector<WDL_TypedBuf<BL_FLOAT> > &in,
                           MorphoFrame7 *frame)
{
    if (mType == LIVE)
    {
        mWaterfallGUI->Lock();

        vector<WDL_TypedBuf<BL_FLOAT> > out;
            
        vector<MorphoFrame7> frames;
        mPipeline->ProcessBlock(in, out, &frames);

        mWaterfallGUI->Unlock();
        mWaterfallGUI->PushAllData();

        if (frames.empty())
            return false;

        *frame = frames[frames.size() - 1];
        
        return true;
    }

    return false;
}

void
SoSource::SetPlaying(bool flag)
{
    mIsPlaying = flag;

    // Mutex?
    if (mIsPlaying)
        mGhostTrack->StartPlay();
    else
        mGhostTrack->StopPlay();
}

bool
SoSource::IsPlaying() const
{
    return mIsPlaying;
}

void
SoSource::SetSpectroBrightness(BL_FLOAT brightness)
{
    mBrightness = brightness;

    mGhostTrack->UpdateParamRange(mBrightness);
}

BL_FLOAT
SoSource::GetSpectroBrightness() const
{
    return mBrightness;
}

void
SoSource::SetSpectroContrast(BL_FLOAT contrast)
{
    mContrast = contrast;

    mGhostTrack->UpdateParamContrast(mContrast);
}

BL_FLOAT
SoSource::GetSpectroContrast() const
{
    return mContrast;
}

void
SoSource::SetSpectroSpecWave(BL_FLOAT specWave)
{
    mSpecWave = specWave;

    mGhostTrack->UpdateParamSpectWaveformRatio(mSpecWave*0.01);
}

BL_FLOAT
SoSource::GetSpectroSpecWave() const
{
    return mSpecWave;
}

void
SoSource::SetSpectroWaveformScale(BL_FLOAT waveScale)
{
    mWaveScale = waveScale;

    mGhostTrack->UpdateParamWaveformScale(mWaveScale);
}

BL_FLOAT
SoSource::GetSpectroWaveformScale() const
{
    return  mWaveScale;
}

void
SoSource::SetSpectroSelectionType(SelectionType type)
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        ((SoFileSource *)mSourceImpl)->SetSpectroSelectionType(type);
}

SelectionType
SoSource::GetSpectroSelectionType() const
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        return ((SoFileSource *)mSourceImpl)->GetSpectroSelectionType();
    
    return RECTANGLE;
}

void
SoSource::SetSourceMaster(bool sourceMaster)
{
    mSourceMaster = sourceMaster;
}

bool
SoSource::GetSourceMaster() const
{
    return mSourceMaster;
}

void
SoSource::SetSourceType(SoSourceType sourceType)
{
    mSourceType = sourceType;

    //
    MorphoFrameSynth2::SynthMode synthMode;
    if (mSourceType == MORPHO_SOURCE_TYPE_MONOPHONIC)
        synthMode = MorphoFrameSynth2::RESYNTH_PARTIALS;
    else if (mSourceType == MORPHO_SOURCE_TYPE_COMPLEX)
        synthMode = MorphoFrameSynth2::SOURCE_PARTIALS;
    
    mPipeline->SetSynthMode(synthMode);
}

SoSourceType
SoSource::GetSourceType() const
{
    return mSourceType;
}

void
SoSource::SetTimeSmoothCoeff(BL_FLOAT timeSmooth)
{
    mTimeSmooth = timeSmooth;

    mPipeline->SetTimeSmoothCoeff(timeSmooth);

    mIsTouched = true;
}

BL_FLOAT
SoSource::GetTimeSmoothCoeff() const
{
    return mTimeSmooth;
}

void
SoSource::SetDetectThreshold(BL_FLOAT detectThrs)
{
    mDetectThreshold = detectThrs;

    mPipeline->SetDetectThreshold(detectThrs);

    mIsTouched = true;
}

BL_FLOAT
SoSource::GetDetectThreshold() const
{
    return mDetectThreshold;
}

void
SoSource::SetFreqThreshold(BL_FLOAT freqThrs)
{
    mFreqThreshold = freqThrs;

    mPipeline->SetFreqThreshold(freqThrs);

    mIsTouched = true;
}

BL_FLOAT
SoSource::GetFreqThreshold() const
{
    return mFreqThreshold;
}

void
SoSource::SetSourceGain(BL_FLOAT sourceGain)
{
    mSourceGain = sourceGain;

    if (mPipeline != NULL)
        mPipeline->SetGain(mSourceGain);
}

BL_FLOAT
SoSource::GetSourceGain() const
{
    return mSourceGain;
}

bool
SoSource::IsTouched() const
{
    if (mType != FILE)
        return false;

    if (!mGhostTrack->ContainsSamples())
        return false;
    
    return mIsTouched;
}

void
SoSource::GetNormSelection(BL_FLOAT *x0, BL_FLOAT *x1)
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        ((SoFileSource *)mSourceImpl)->GetNormSelection(x0, x1);
}

void
SoSource::Reset(BL_FLOAT sampleRate)
{
    WaterfallSource::Reset(sampleRate);
    
    mSampleRate = sampleRate;

    mGhostTrack->Reset(BUFFER_SIZE, sampleRate);
}

void
SoSource::ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &in,
                       vector<WDL_TypedBuf<BL_FLOAT> > &out,
                       bool isTransportPlaying,
                       BL_FLOAT transportSamplePos)
{
    // Feed the spectrogram, for file source or live source
    // For file source, we will also get the played output
    mGhostTrack->Lock();
    
    if (!mGhostTrack->IsPlaying())
    {
        mGhostTrack->Unlock();
        mGhostTrack->PushAllData();
        
        //out = in;
        
        return;
    }

    // Process
    vector<WDL_TypedBuf<BL_FLOAT> > scIn;
    mGhostTrack->ProcessBlock(in, scIn, &out,
                              isTransportPlaying,
                              transportSamplePos);
    
    mGhostTrack->Unlock();
    mGhostTrack->PushAllData();

    // Waterfall
    //
    mWaterfallGUI->Lock();


    // Set GhostTrack2 output to RTPipeline input
    vector<WDL_TypedBuf<BL_FLOAT> > &in2 = mTmp0;
    in2 = out;

    vector<WDL_TypedBuf<BL_FLOAT> > &out2 = mTmp1;
    
    // Process
    vector<MorphoFrame7> frames;
    mPipeline->ProcessBlock(in2, out2, &frames);

    mWaterfallGUI->AddMorphoFrames(frames);
    
    mWaterfallGUI->Unlock();
    mWaterfallGUI->PushAllData();

    if (mSourceType != MORPHO_SOURCE_TYPE_BYPASS)
        out = out2;
    
    if ((mType == LIVE) && !mIsPlaying)
    {
        // Live source not plating => silence!
        BLUtils::FillAllZero(&out);
    }
}

void
SoSource::OnUIOpen()
{
    WaterfallSource::OnUIOpen();
        
    mGhostTrack->OnUIOpen();
}

void
SoSource::OnUIClose()
{
    WaterfallSource::OnUIClose();
        
    mGhostTrack->OnUIClose();
}

void
SoSource::SetViewEnabled(bool flag)
{
    WaterfallSource::SetViewEnabled(flag);
    
    mGhostTrack->SetGraphEnabled(flag);
}

void
SoSource::CreateSpectroControls(GUIHelper12 *guiHelper,
                                Plugin *plug,
                                int grapX, int graphY,
                                const char *graphBitmapFN)
{
    if (mGhostTrack->IsControlsCreated())
        // Already created
        return;
        
    IGraphics *graphics = plug->GetUI();
    if (graphics == NULL)
        return;

    GraphControl12 *graph =
        mGhostTrack->CreateControls(guiHelper, plug, graphics,
                                    grapX, graphY, graphBitmapFN,
                                    0, 0);

    // Border
    float borderWidth = 1.0;
    IColor borderColor(255, 255, 255, 255);
    BorderCustomDrawer *borderDrawer =
        new BorderCustomDrawer(borderWidth, borderColor);
    graph->AddCustomDrawer(borderDrawer);
}

void
SoSource::CheckRecomputeSpectro()
{
    mGhostTrack->CheckRecomputeData();
}
