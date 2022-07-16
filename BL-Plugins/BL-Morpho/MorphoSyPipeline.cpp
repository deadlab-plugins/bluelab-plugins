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
 
#include <MorphoFrameSynthetizer.h>
#include <MorphoMixer.h>
#include <MorphoFrame7.h>

#include "MorphoSyPipeline.h"


MorphoSyPipeline::MorphoSyPipeline(SoSourceManager *soSourceManager,
                                   SySourceManager *sySourceManager,
                                   BL_FLOAT xyPadRatio)
{
    mLoop = false;
    mTimeStretchFactor = 1.0;
    mGain = 1.0;

    BL_FLOAT sampleRate = 44100.0;
    mFrameSynthetizer = new MorphoFrameSynthetizer(sampleRate);

    mMixer = new MorphoMixer(soSourceManager,
                             sySourceManager,
                             xyPadRatio);
}

MorphoSyPipeline::~MorphoSyPipeline()
{    
    delete mFrameSynthetizer;
    delete mMixer;
}

void
MorphoSyPipeline::Reset(BL_FLOAT sampleRate)
{
    mFrameSynthetizer->Reset(sampleRate);
}

void
MorphoSyPipeline::ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &out)
{
    MorphoFrame7 frame;
    mMixer->Mix(&frame);

    // Apply gain
    BL_FLOAT ampFactor = frame.GetAmpFactor();
    ampFactor *= mGain;
    frame.SetAmpFactor(ampFactor);
    
    mFrameSynthetizer->AddMorphoFrame(frame);
    mFrameSynthetizer->ProcessBlock(out);
}

void
MorphoSyPipeline::SetSynthMode(MorphoFrameSynth2::SynthMode synthMode)
{
    mFrameSynthetizer->SetSynthMode(synthMode);
}

void
MorphoSyPipeline::SetLoop(bool flag)
{
    mMixer->SetLoop(flag);
}

bool
MorphoSyPipeline::GetLoop() const
{
    return mMixer->GetLoop();
}

void
MorphoSyPipeline::SetTimeStretchFactor(BL_FLOAT factor)
{
    mTimeStretchFactor = factor;

    mMixer->SetTimeStretchFactor(mTimeStretchFactor);
}

BL_FLOAT
MorphoSyPipeline::GetTimeStretchFactor() const
{
    return mTimeStretchFactor;
}

void
MorphoSyPipeline::SetGain(BL_FLOAT gain)
{
    mGain = gain;
}

BL_FLOAT
MorphoSyPipeline::GetGain() const
{
    return mGain;
}
