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
 
//
//  Axis3DFactory2.h
//  BL-SoundMetaViewer
//
//  Created by applematuer on 4/5/20.
//
//

#ifndef __BL_SoundMetaViewer__Axis3DFactory2__
#define __BL_SoundMetaViewer__Axis3DFactory2__

#ifdef IGRAPHICS_NANOVG

#include <BLTypes.h>

// Axis3DFactory2: from Axis3DFactory
// - removed static (this is very dangerous for plugins)

class Axis3D;
class Axis3DFactory2
{
public:
    enum Orientation
    {
        ORIENTATION_X,
        ORIENTATION_Y,
        ORIENTATION_Z
    };
    
    Axis3DFactory2();
    virtual ~Axis3DFactory2();
    
    void Reset(int bufferSize, BL_FLOAT sampleRate);
    
    Axis3D *CreateAmpAxis(Orientation orientation);
    Axis3D *CreateAmpDbAxis(Orientation orientation);
    
    Axis3D *CreateFreqAxis(Orientation orientation);
    
    Axis3D *CreateChromaAxis(Orientation orientation);
    
    Axis3D *CreateAngleAxis(Orientation orientation);
    
    Axis3D *CreateLeftRightAxis(Orientation orientation);
    
    Axis3D *CreateMinusOneOneAxis(Orientation orientation);
    
    Axis3D *CreatePercentAxis(Orientation orientation);
    
    Axis3D *CreateEmptyAxis(Orientation orientation);
    
protected:
    void ComputeExtremities(Orientation orientation, BL_FLOAT p0[3], BL_FLOAT p1[3]);
    
    static BL_FLOAT FreqToMelNorm(BL_FLOAT freq, int bufferSize, BL_FLOAT sampleRate);
    
    int mBufferSize;
    BL_FLOAT mSampleRate;
};

#endif // IGRAPHICS_NANOVG

#endif /* defined(__BL_SoundMetaViewer__Axis3DFactory2__) */
