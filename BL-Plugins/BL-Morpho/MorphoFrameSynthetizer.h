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
 
#ifndef MORPHO_FRAME_SYNTHETIZER_H
#define MORPHO_FRAME_SYNTHETIZER_H

#include <vector>
using namespace std;

#include <BLTypes.h>

#include <MorphoFrameSynth2.h>
#include <MorphoFrame7.h>

class FftProcessObj16;
class MorphoFrameSynthetizerFftObj;
class MorphoFrameSynthetizer
{
 public:
    MorphoFrameSynthetizer(BL_FLOAT sampleRate);
    virtual ~MorphoFrameSynthetizer();

    void Reset(BL_FLOAT sampleRate);
    
    void ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &out);

    void SetSynthMode(MorphoFrameSynth2::SynthMode mode);

    void AddMorphoFrame(const MorphoFrame7 &frame);
    
 protected:
    FftProcessObj16 *mFftObj;

    MorphoFrameSynthetizerFftObj *mFrameSynthetizerObj;

private:
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
};

#endif
