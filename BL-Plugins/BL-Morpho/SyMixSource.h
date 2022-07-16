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
 
#ifndef SY_MIX_SOURCE_H
#define SY_MIX_SOURCE_H

#include <MorphoFrame7.h>

#include <SySourceImpl.h>

class SyMixSource : public SySourceImpl
{
 public:
    SyMixSource();
    virtual ~SyMixSource();

    void GetName(char name[FILENAME_SIZE]) override;

    void SetMixMorphoFrame(const MorphoFrame7 &frame);
    void ComputeCurrentMorphoFrame(MorphoFrame7 *frame) override;
    
protected:
    MorphoFrame7 mCurrentMorphoFrame;
};

#endif
