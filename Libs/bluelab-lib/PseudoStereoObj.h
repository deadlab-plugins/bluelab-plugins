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
//  PseudoStereoObj.h
//  UST
//
//  Created by applematuer on 12/28/19.
//
//

#ifndef __UST__PseudoStereoObj__
#define __UST__PseudoStereoObj__

#include "IPlug_include_in_plug_hdr.h"

//#define NUM_FILTERS 4 //1 //2

#define MIN_MODE 0
#define MAX_MODE 7

#define DEFAULT_MODE 7 //0 //3

// Gerzon method
//
// (not finished: colors the sound too much compared to the Waves plugin)
class FilterRBJNX;
class USTStereoWidener;

class PseudoStereoObj
{
public:
    PseudoStereoObj(BL_FLOAT sampleRate,
                    BL_FLOAT width = 1.0, //0.25, //0.5, //1.0,
                    int mode = DEFAULT_MODE);
    
    virtual ~PseudoStereoObj();
    
    void Reset(BL_FLOAT sampleRate);
    
    void SetWidth(BL_FLOAT width);
    
    // For debugging
    void SetCutoffFreq(BL_FLOAT freq);
    void SetMode(BL_FLOAT mode);
    
    void ProcessSample(BL_FLOAT sampIn, BL_FLOAT *sampOut0, BL_FLOAT *sampOut1);
    
    void ProcessSamples(const WDL_TypedBuf<BL_FLOAT> &sampsIn,
                        WDL_TypedBuf<BL_FLOAT> *sampsOut0,
                        WDL_TypedBuf<BL_FLOAT> *sampsOut1);
    
    void ProcessSamples(vector<WDL_TypedBuf<BL_FLOAT> > *samplesVec);
    
protected:
    void InitFilters();
    
    BL_FLOAT GetCutoffFrequency(int filterNum);

    
    //
    BL_FLOAT mSampleRate;
    
    BL_FLOAT mWidth;
    int mMode; // Num filters ?
    
    BL_FLOAT mCutoffFreq;
    
    FilterRBJNX *mFilter0;
    FilterRBJNX *mFilter1;
    
    FilterRBJNX *mModeFilters[MAX_MODE+1][2];
    
    USTStereoWidener *mStereoWidener;
};

#endif /* defined(__UST__PseudoStereoObj__) */
