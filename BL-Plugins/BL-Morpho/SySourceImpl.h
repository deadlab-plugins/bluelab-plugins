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
 
#ifndef SY_SOURCE_IMPL_H
#define SY_SOURCE_IMPL_H

#include <BLTypes.h>
#include <BLUtilsFile.h>

#include <Morpho_defs.h>

class MorphoFrame7;
class SySourceImpl
{
 public:
    SySourceImpl();
    virtual ~SySourceImpl();

    virtual void GetName(char name[FILENAME_SIZE]) = 0;

    virtual void ComputeCurrentMorphoFrame(MorphoFrame7 *frame) = 0;

    void SetAmpFactor(BL_FLOAT amp);
    void SetPitchFactor(BL_FLOAT pitch);
    void SetColorFactor(BL_FLOAT color);
    void SetWarpingFactor(BL_FLOAT warping);
    void SetNoiseFactor(BL_FLOAT noise);
    
protected:
    BL_FLOAT mAmpFactor;
    BL_FLOAT mFreqFactor;
    BL_FLOAT mColorFactor;
    BL_FLOAT mWarpingFactor;
    BL_FLOAT mNoiseFactor;
};

#endif
