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
//  SMVProcessXComputerLissajous.h
//  BL-SoundMetaViewer
//
//  Created by applematuer on 3/24/20.
//
//

#ifndef __BL_SoundMetaViewer__SMVProcessXComputerLissajous__
#define __BL_SoundMetaViewer__SMVProcessXComputerLissajous__

#include <SMVProcessXComputer.h>

// Standard Lissajous
// Can not be scaled by Y computers
class Axis3DFactory2;
class SMVProcessXComputerLissajous : public SMVProcessXComputer
{
public:
    SMVProcessXComputerLissajous(Axis3DFactory2 *axisFactory);
    
    virtual ~SMVProcessXComputerLissajous();
    
    virtual void Reset(BL_FLOAT sampleRate) override {}
    
    void ComputeX(const WDL_TypedBuf<BL_FLOAT> samples[2],
                  const WDL_TypedBuf<BL_FLOAT> magns[2],
                  const WDL_TypedBuf<BL_FLOAT> phases[2],
                  WDL_TypedBuf<BL_FLOAT> *resultX,
                  WDL_TypedBuf<BL_FLOAT> *resultY = NULL,
                  bool *isPolar = NULL,
                  BL_FLOAT polarCenter[2] = NULL,
                  bool *isScalable = NULL) override;
    
    Axis3D *CreateAxis() override;
    
protected:
    void ComputeLissajous(const WDL_TypedBuf<BL_FLOAT> samples[2],
                          WDL_TypedBuf<BL_FLOAT> lissajousSamples[2],
                          bool fitInSquare);
    
    //
    Axis3DFactory2 *mAxisFactory;
};

#endif /* defined(__BL_SoundMetaViewer__SMVProcessXComputerLissajous__) */
