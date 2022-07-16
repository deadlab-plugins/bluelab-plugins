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
 
#include <SyLiveSource.h>
#include <SyFileSource.h>
#include <SyMixSource.h>

#include <MorphoWaterfallGUI.h>

#include "SySource.h"

SySource::SySource(BL_FLOAT sampleRate)
: WaterfallSource(sampleRate, MORPHO_PLUG_MODE_SYNTH)
{
    mType = NONE;

    mSoSourceType = SoSourceType::MORPHO_SOURCE_TYPE_COMPLEX;
        
    mSourceImpl = NULL;

    // Parameters
    mSourceSolo = false;
    mSourceMute = false;
    mSourceMaster = false;

    mAmp = 1.0;;
    mAmpSolo = false;
    mAmpMute = false;

    mPitch = 1.0;
    mPitchSolo = false;
    mPitchMute = false;

    mColor = 1.0;
    mColorSolo = false;
    mColorMute = false;

    mWarping = 1.0;
    mWarpingSolo = false;
    mWarpingMute = false;

    mNoise = 0.0;
    mNoiseSolo = false;
    mNoiseMute = false;

    mFreeze = false;

    mSynthType = MORPHO_SOURCE_ALL_PARTIALS;

    mMixPosX = 0.0;
    mMixPosY = 0.0;

    // WaterfallSource
    SetWaterfallViewMode(MORPHO_WATERFALL_VIEW_AMP);
}

SySource::~SySource()
{
    if (mSourceImpl != NULL)
        delete mSourceImpl;
}

SySource::Type
SySource::GetType() const
{
    return mType;
}

void
SySource::SetTypeMixSource()
{
    if (mSourceImpl != NULL)
        return;
    
    mSourceImpl = new SyMixSource();
    UpdatNewSourceImplFactors();
    
    mType = MIX;
}

void
SySource::SetTypeFileSource(const char *fileName)
{
    if (mSourceImpl != NULL)
        return;
    
    mSourceImpl = new SyFileSource();
    UpdatNewSourceImplFactors();
 
    if (fileName != NULL)
        ((SyFileSource *)mSourceImpl)->SetFileName(fileName);

    mType = FILE;
}

void
SySource::SetTypeLiveSource()
{
    if (mSourceImpl != NULL)
        return;
    
    mSourceImpl = new SyLiveSource();
    UpdatNewSourceImplFactors();
 
    mType = LIVE;
}

void
SySource::GetName(char name[FILENAME_SIZE])
{
    memset(name, '\0', FILENAME_SIZE);
    
    if ((mType == NONE) || (mSourceImpl == NULL))
    {
        strcpy(name, "");

        return;
    }

    mSourceImpl->GetName(name);
}

bool
SySource::CanOutputSound() const
{
    if (mType == FILE)
        return ((SyFileSource *)mSourceImpl)->CanOutputSound();

    return true;
}

BL_FLOAT
SySource::GetPlayPos() const
{
    if (mType == FILE)
    {
        BL_FLOAT t =  ((SyFileSource *)mSourceImpl)->GetPlayPos();

        return t;
    }
    
    return 0.0;
}

void
SySource::SetPlayPos(BL_FLOAT t)
{
    if (mFreeze)
        return;

    if (mType == FILE)
        ((SyFileSource *)mSourceImpl)->SetPlayPos(t);
}

void
SySource::PlayAdvance(BL_FLOAT timeStretchCoeff)
{
    if (mFreeze)
        return;
    
    if (mType == FILE)
        ((SyFileSource *)mSourceImpl)->PlayAdvance(timeStretchCoeff);
}

bool
SySource::IsPlayFinished() const
{
    if (mType == FILE)
        return ((SyFileSource *)mSourceImpl)->IsPlayFinished();

    return false;
}

void
SySource::ComputeCurrentMorphoFrame(MorphoFrame7 *frame)
{
    mSourceImpl->ComputeCurrentMorphoFrame(frame);

    UpdateGUI(*frame);
}

void
SySource::SetFileMorphoFrames(const vector<MorphoFrame7> &frames)
{
    if (mType == FILE)
    {
        ((SyFileSource *)mSourceImpl)->SetFileMorphoFrames(frames);
        
        // Don't update GUI, it will be updated by MorphoMorphoMixer::Mix()
    }
}

void
SySource::SetMixMorphoFrame(const MorphoFrame7 &frame)
{
    if (mType == MIX)
        // Called from MorphoMorphoMixer
    {
        ((SyMixSource *)mSourceImpl)->SetMixMorphoFrame(frame);

        // Don't update GUI, it will be updated by MorphoMorphoMixer::Mix()
        //MorphoFrame7 mixFrame;
        //mSourceImpl->ComputeCurrentMorphoFrame(&mixFrame);
        //UpdateGUI(mixFrame);
    }
}

void
SySource::SetLiveMorphoFrame(const MorphoFrame7 &frame)
{
    if (mType == LIVE)
        // Called by SoLiveSource
    {
        if (!mFreeze) // If frozen, keep the prev frame forever
            ((SyLiveSource *)mSourceImpl)->SetLiveMorphoFrame(frame);

        // Don't update GUI, it will be updated by MorphoMorphoMixer::Mix()
    }
}

void
SySource::SetSourceSolo(bool flag)
{
    mSourceSolo = flag;
}

bool
SySource::GetSourceSolo() const
{
    return mSourceSolo;
}
    
void
SySource::SetSourceMute(bool flag)
{
    mSourceMute = flag;
}

bool
SySource::GetSourceMute() const
{
    return mSourceMute;
}
    
void
SySource::SetSourceMaster(bool flag)
{
    mSourceMaster = flag;
}

bool
SySource::GetSourceMaster() const
{
    return mSourceMaster;
}

void
SySource::SetAmp(BL_FLOAT amp)
{
    mAmp = amp;

    if (mSourceImpl != NULL)
        mSourceImpl->SetAmpFactor(mAmp);
}

BL_FLOAT
SySource::GetAmp() const
{
    return mAmp;
}

void
SySource::SetAmpSolo(bool flag)
{
    mAmpSolo = flag;
}

bool
SySource::GetAmpSolo() const
{
    return mAmpSolo;
}
    
void
SySource::SetAmpMute(bool flag)
{
    mAmpMute = flag;
}

bool
SySource::GetAmpMute() const
{
    return mAmpMute;
}
    
void
SySource::SetPitch(BL_FLOAT pitch)
{
    mPitch = pitch;

    if (mSourceImpl != NULL)
        mSourceImpl->SetPitchFactor(mPitch);
}

BL_FLOAT
SySource::GetPitch() const
{
    return mPitch;
}

void
SySource::SetPitchSolo(bool flag)
{
    mPitchSolo = flag;
}

bool
SySource::GetPitchSolo() const
{
    return mPitchSolo;
}
    
void
SySource::SetPitchMute(bool flag)
{
    mPitchMute = flag;
}

bool
SySource::GetPitchMute() const
{
    return mPitchMute;
}
    
void
SySource::SetColor(BL_FLOAT color)
{
    mColor = color;

    if (mSourceImpl != NULL)
        mSourceImpl->SetColorFactor(mColor);
}

BL_FLOAT
SySource::GetColor() const
{
    return mColor;
}

void
SySource::SetColorSolo(bool flag)
{
    mColorSolo = flag;
}

bool
SySource::GetColorSolo() const
{
    return mColorSolo;
}
    
void
SySource::SetColorMute(bool flag)
{
    mColorMute = flag;
}

bool
SySource::GetColorMute() const
{
    return mColorMute;
}

void
SySource::SetWarping(BL_FLOAT warping)
{
    mWarping = warping;

    if (mSourceImpl != NULL)
        mSourceImpl->SetWarpingFactor(mWarping);
}

BL_FLOAT
SySource::GetWarping() const
{
    return mWarping;
}

void
SySource::SetWarpingSolo(bool flag)
{
    mWarpingSolo = flag;
}

bool
SySource::GetWarpingSolo() const
{
    return mWarpingSolo;
}

void
SySource::SetWarpingMute(bool flag)
{
    mWarpingMute = flag;
}

bool
SySource::GetWarpingMute() const
{
    return mWarpingMute;
}

void
SySource::SetNoise(BL_FLOAT noise)
{
    mNoise = noise;

    if (mSourceImpl != NULL)
        mSourceImpl->SetNoiseFactor(mNoise);
}

BL_FLOAT
SySource::GetNoise() const
{
    return mNoise;
}

void
SySource::SetNoiseSolo(bool flag)
{
    mNoiseSolo = flag;
}

bool
SySource::GetNoiseSolo() const
{
    return mNoiseSolo;
}
    
void
SySource::SetNoiseMute(bool flag)
{
    mNoiseMute = flag;
}

bool
SySource::GetNoiseMute() const
{
    return mNoiseMute;
}

void
SySource::SetReverse(bool flag)
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        ((SyFileSource *)mSourceImpl)->SetReverse(flag);
}

bool
SySource::GetReverse() const
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        return ((SyFileSource *)mSourceImpl)->GetReverse();

    return false;
}

void
SySource::SetPingPong(bool flag)
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        return ((SyFileSource *)mSourceImpl)->SetPingPong(flag);
}

bool
SySource::GetPingPong() const
{
    if ((mType == FILE) && (mSourceImpl != NULL))
        return ((SyFileSource *)mSourceImpl)->GetPingPong();
    
    return false;
}   

void
SySource::SetFreeze(bool flag)
{
    mFreeze = flag;
}

bool
SySource::GetFreeze() const
{
    return mFreeze;
}   

void
SySource::SetSynthType(SySourceSynthType type)
{
    mSynthType = type;
}

SySourceSynthType
SySource::GetSynthType() const
{
    return mSynthType;
}

void
SySource::SetMixPos(BL_FLOAT tx, BL_FLOAT ty)
{
    mMixPosX = tx;
    mMixPosY = ty;
}

void
SySource::GetMixPos(BL_FLOAT *tx, BL_FLOAT *ty) const
{
    *tx = mMixPosX;
    *ty = mMixPosY;
}

void
SySource::SetSoSourceType(SoSourceType type)
{
    mSoSourceType = type;
}

SoSourceType
SySource::GetSoSourceType() const
{
    return mSoSourceType;
}

void
SySource::SetNormSelection(BL_FLOAT x0, BL_FLOAT x1)
{
    ((SyFileSource *)mSourceImpl)->SetNormSelection(x0, x1);
}

void
SySource::UpdateGUI(const MorphoFrame7 &frame)
{
    vector<MorphoFrame7> frames;
    frames.push_back(frame);
    
    mWaterfallGUI->AddMorphoFrames(frames);
}

void
SySource::UpdatNewSourceImplFactors()
{
    if (mSourceImpl == NULL)
        return;

    mSourceImpl->SetAmpFactor(mAmp);
    mSourceImpl->SetPitchFactor(mPitch);
    mSourceImpl->SetColorFactor(mColor);
    mSourceImpl->SetWarpingFactor(mWarping);
    mSourceImpl->SetNoiseFactor(mNoise);
}
