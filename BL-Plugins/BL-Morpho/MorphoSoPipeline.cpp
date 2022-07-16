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
 
#include <MorphoFrameAnalyzer.h>
#include <MorphoFrameSynthetizer.h>

#include <MorphoFrame7.h>

#include "MorphoSoPipeline.h"


MorphoSoPipeline::MorphoSoPipeline(BL_FLOAT sampleRate)
{
    // Store detection data in MorphoFrames!
    mFrameAnalyzer = new MorphoFrameAnalyzer(sampleRate, true);
    mFrameSynthetizer = new MorphoFrameSynthetizer(sampleRate);
    
    mGain = 1.0;
}

MorphoSoPipeline::~MorphoSoPipeline()
{    
    delete mFrameAnalyzer;
    delete mFrameSynthetizer;
}

void
MorphoSoPipeline::Reset(BL_FLOAT sampleRate)
{
    mFrameAnalyzer->Reset(sampleRate);
    mFrameSynthetizer->Reset(sampleRate);
}

void
MorphoSoPipeline::ProcessBlock(const vector<WDL_TypedBuf<BL_FLOAT> > &in,
                               vector<WDL_TypedBuf<BL_FLOAT> > &out,
                               vector<MorphoFrame7> *resultFrames)
{
    mFrameAnalyzer->ProcessBlock(in);

    vector<MorphoFrame7> frames;
    mFrameAnalyzer->GetMorphoFrames(&frames);
    
    // Here, we are in So mode
    // Apply gain
    for (int i = 0; i < frames.size(); i++)
    {
        MorphoFrame7 &f = frames[i];
        f.SetAmpFactor(mGain);
    }
        
    *resultFrames = frames;
    
    for (int i = 0; i < frames.size(); i++)
    {
        const MorphoFrame7 &f = frames[i];
        mFrameSynthetizer->AddMorphoFrame(f);
    }

    // Set same size
    out = in;
    mFrameSynthetizer->ProcessBlock(out);
}

void
MorphoSoPipeline::SetSynthMode(MorphoFrameSynth2::SynthMode mode)
{
    mFrameSynthetizer->SetSynthMode(mode);
}

void
MorphoSoPipeline::SetTimeSmoothCoeff(BL_FLOAT coeff)
{
    mFrameAnalyzer->SetTimeSmoothCoeff(coeff);
}

void
MorphoSoPipeline::SetDetectThreshold(BL_FLOAT detectThrs)
{
    mFrameAnalyzer->SetDetectThreshold(detectThrs);
}

void
MorphoSoPipeline::SetFreqThreshold(BL_FLOAT freqThrs)
{
    mFrameAnalyzer->SetFreqThreshold(freqThrs);
}

void
MorphoSoPipeline::SetGain(BL_FLOAT gain)
{
    mGain = gain;
}
