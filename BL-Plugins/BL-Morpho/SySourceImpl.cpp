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
 
#include "SySourceImpl.h"

SySourceImpl::SySourceImpl()
{
    mAmpFactor = 1.0;
    mFreqFactor = 1.0;
    mColorFactor = 1.0;
    mWarpingFactor = 1.0;
    mNoiseFactor = 0.0;
}

SySourceImpl::~SySourceImpl() {}

void
SySourceImpl::SetAmpFactor(BL_FLOAT amp)
{
    mAmpFactor = amp;
}

void
SySourceImpl::SetPitchFactor(BL_FLOAT pitch)
{
    mFreqFactor = pitch;
}

void
SySourceImpl::SetColorFactor(BL_FLOAT color)
{
    mColorFactor = color;
}

void
SySourceImpl::SetWarpingFactor(BL_FLOAT warping)
{
    mWarpingFactor = warping;
}

void
SySourceImpl::SetNoiseFactor(BL_FLOAT noise)
{
    mNoiseFactor = noise;
}
