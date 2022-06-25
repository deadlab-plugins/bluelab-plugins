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
//  ImageSmootherKernel.h
//  BL-DUET
//
//  Created by applematuer on 5/4/20.
//
//

#ifndef __BL_DUET__ImageSmootherKernel__
#define __BL_DUET__ImageSmootherKernel__

#include "IPlug_include_in_plug_hdr.h"

class ImageSmootherKernel
{
public:
    ImageSmootherKernel(int kernelSize);
    
    virtual ~ImageSmootherKernel();
    
    void SetKernelSize(int kernelSize);
    
    void SmoothImage(int imgWidth, int imgHeight, WDL_TypedBuf<BL_FLOAT> *imageData);
    
protected:
    int mKernelSize;
    
    WDL_TypedBuf<BL_FLOAT> mHanningKernel;

private:
    // Tmp buffers
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0;
};

#endif /* defined(__BL_DUET__ImageSmootherKernel__) */
