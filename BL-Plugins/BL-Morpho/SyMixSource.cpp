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
 
#include "SyMixSource.h"

SyMixSource::SyMixSource() {}

SyMixSource::~SyMixSource() {}

void
SyMixSource::GetName(char name[FILENAME_SIZE])
{
    strcpy(name, "Mix");
}

void
SyMixSource::ComputeCurrentMorphoFrame(MorphoFrame7 *frame)
{
    *frame = mCurrentMorphoFrame;
}

void
SyMixSource::SetMixMorphoFrame(const MorphoFrame7 &frame)
{
    mCurrentMorphoFrame = frame;

    mCurrentMorphoFrame.ApplyMorphoFactors(mAmpFactor, mFreqFactor,
                                           mColorFactor, mWarpingFactor,
                                           mNoiseFactor);
}
