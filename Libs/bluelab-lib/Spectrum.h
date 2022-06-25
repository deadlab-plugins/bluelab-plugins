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
//  Spectrum.h
//  Denoiser
//
//  Created by Apple m'a Tuer on 16/05/17.
//
//

#ifndef __Denoiser__Spectrum__
#define __Denoiser__Spectrum__

#include <vector>
using namespace std;

#include <BLTypes.h>

#include "IPlug_include_in_plug_hdr.h"
//#include "../../WDL/IPlug/Containers.h"

class Spectrum
{
public:
    Spectrum(int width);
    
    virtual ~Spectrum();
    
    void SetInputMultiplier(BL_FLOAT mult);
    
    static Spectrum *Load(const char *fileName);
    
    void AddLine(const WDL_TypedBuf<BL_FLOAT> &newLine);
    
    const WDL_TypedBuf<BL_FLOAT> *GetLine(int index);
    
    void Save(const char *filename);
    
protected:
    int mWidth;
    BL_FLOAT mInputMultiplier;
    
    vector<WDL_TypedBuf<BL_FLOAT> > mLines;
};

#endif /* defined(__Denoiser__Spectrum__) */
