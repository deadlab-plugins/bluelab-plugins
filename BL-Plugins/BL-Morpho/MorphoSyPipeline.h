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
 
#ifndef MORPHO_SY_PIPELINE_H
#define MORPHO_SY_PIPELINE_H

#include <vector>
using namespace std;

#include <BLTypes.h>

#include <MorphoFrameSynth2.h>

#include <Morpho_defs.h>

class SySourceManager;
class SySourcesView;
class MorphoFrameSynthetizer;
class MorphoMixer;
class MorphoSyPipeline
{
 public:
    MorphoSyPipeline(SoSourceManager *soSourceManager,
                     SySourceManager *sySourceManager,
                     BL_FLOAT xyPadRatio);
    virtual ~MorphoSyPipeline();

    void Reset(BL_FLOAT sampleRate);
    
    void ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &out);

    //
    void SetSynthMode(MorphoFrameSynth2::SynthMode synthMode);
    
    // Parameters
    void SetLoop(bool flag);
    bool GetLoop() const;
    
    void SetTimeStretchFactor(BL_FLOAT factor);
    BL_FLOAT GetTimeStretchFactor() const;
    
    // Out gain
    void SetGain(BL_FLOAT gain);
    BL_FLOAT GetGain() const;
    
 protected:
    MorphoFrameSynthetizer *mFrameSynthetizer;

    MorphoMixer *mMixer;
    
    //
    bool mLoop;
    BL_FLOAT mTimeStretchFactor;
    BL_FLOAT mGain;
};

#endif
