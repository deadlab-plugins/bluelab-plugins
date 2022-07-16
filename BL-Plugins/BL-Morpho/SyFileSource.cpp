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
 
#include <BLUtilsMath.h>

#include <MorphoFrame7.h>

#include "SyFileSource.h"

SyFileSource::SyFileSource()
{
    memset(mFileName, '\0', FILENAME_SIZE);

    // Parameters
    mReverse = false;
    mPingPong = false;

    mPlayPos = 0.0;

    mNormSelection[0] = 0.0;
    mNormSelection[1] = 1.0;
}

SyFileSource::~SyFileSource() {}

void 
SyFileSource::SetFileName(const char *fileName)
{
    strcpy(mFileName, fileName);
}

void
SyFileSource::GetFileName(char fileName[FILENAME_SIZE])
{
    strcpy(fileName, mFileName);
}

void
SyFileSource::GetName(char name[FILENAME_SIZE])
{
    GetFileName(name);
}

void
SyFileSource::SetFileMorphoFrames(const vector<MorphoFrame7> &frames)
{
    mMorphoFrames = frames;
}

void
SyFileSource::ComputeCurrentMorphoFrame(MorphoFrame7 *frame)
{
    if (mMorphoFrames.empty())
        return;
    
    BL_FLOAT t = mPlayPos;
    
    if (!mPingPong)
    {
        //t = t - (int)t;
        t = fmod(t, 1.0);
        if (t < 0.0)
            t += 1.0;
    }
    else
    {
        t = fmod(t, 2.0);
        if (t < 0.0)
            t += 2.0;
        
        if (t > 1.0)
            t = 2.0 - t;
    }

    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;

    //int i0f = t*(mMorphoFrames.size() - 1);
    int i0f = mNormSelection[0]*(mMorphoFrames.size() - 1) +
        t*(mNormSelection[1] - mNormSelection[0])*(mMorphoFrames.size() - 1);
    int i0 = (int)i0f;
    const MorphoFrame7 &frame0 = mMorphoFrames[i0];

    int i1 = i0 + 1;
    if (i1 >= mMorphoFrames.size())
    {
        *frame = frame0;

        // Since we don't mix, we must even apply the factor
        //frame->CommitMorphoFactors();

        frame->ApplyMorphoFactors(mAmpFactor, mFreqFactor,
                                  mColorFactor, mWarpingFactor,
                                  mNoiseFactor);
        
        return;
    }
    
    const MorphoFrame7 &frame1 = mMorphoFrames[i1];

    BL_FLOAT t0 = i0f - (int)i0f;
    MorphoFrame7::MixFrames(frame, frame0, frame1, t0, true);

    frame->ApplyMorphoFactors(mAmpFactor, mFreqFactor,
                              mColorFactor, mWarpingFactor,
                              mNoiseFactor);
}

bool
SyFileSource::CanOutputSound() const
{
    bool canOutputSound = !mMorphoFrames.empty();

    return canOutputSound;
}

BL_FLOAT
SyFileSource::GetPlayPos() const
{
    return mPlayPos;
}

void
SyFileSource::SetPlayPos(BL_FLOAT t)
{
    mPlayPos = t;
}

void
SyFileSource::PlayAdvance(BL_FLOAT timeStretchCoeff)
{
    BL_FLOAT step = 0.0;
    if (mMorphoFrames.size() > 1)
    {
        //step = 1.0/(mMorphoFrames.size() - 1);

        BL_FLOAT selectionWidth = (mNormSelection[1] - mNormSelection[0]);
        if (selectionWidth < BL_EPS)
            step = 0.0;
        else
            step = 1.0/(selectionWidth*(mMorphoFrames.size() - 1));
    }
    
    step *= timeStretchCoeff;
    
    if (!mReverse)
        mPlayPos += step;
    else
        mPlayPos -= step;
}

bool
SyFileSource::IsPlayFinished() const
{
    if (mReverse)
    {
        bool finished = (mPlayPos < 0.0);

        return finished;
    }
    
    if (mPingPong)
    {
        bool finished = (mPlayPos > 2.0);
        
        return finished;
    }

    // Normal
    bool finished = (mPlayPos > 1.0);

    return finished;
}

void
SyFileSource::SetReverse(bool flag)
{
    mReverse = flag;
}

bool
SyFileSource::GetReverse() const
{
    return mReverse;
}

void
SyFileSource::SetPingPong(bool flag)
{
    mPingPong = flag;
}

bool
SyFileSource::GetPingPong() const
{
    return mPingPong;
}   

void
SyFileSource::SetNormSelection(BL_FLOAT x0, BL_FLOAT x1)
{
    // Check bounds, in case we select ouside the selection 
    if (x0 < 0.0)
        x0 = 0.0;
    if (x0 > 1.0)
        x0 = 1.0;

    if (x1 < 0.0)
        x1 = 0.0;
    if (x1 > 1.0)
        x1 = 1.0;

    mNormSelection[0] = x0;
    mNormSelection[1] = x1;
}
