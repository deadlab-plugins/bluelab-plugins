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
 
#include <MorphoFrameSynthetizerFftObj.h>
#include <FftProcessObj16.h>

#include <BLDebug.h>

#include "MorphoFrameSynthetizer.h"

#define BUFFER_SIZE 2048
#define OVERSAMPLING 4
#define FREQ_RES 1

MorphoFrameSynthetizer::MorphoFrameSynthetizer(BL_FLOAT sampleRate)
{
    int numChannels = 1;
    int numScInputs = 0;
    
    vector<ProcessObj *> processObjs;
    mFrameSynthetizerObj = new MorphoFrameSynthetizerFftObj(BUFFER_SIZE, OVERSAMPLING,
                                                         FREQ_RES, sampleRate);
    processObjs.push_back(mFrameSynthetizerObj);

    mFftObj = new FftProcessObj16(processObjs,
                                  numChannels, numScInputs,
                                  BUFFER_SIZE, OVERSAMPLING,
                                  FREQ_RES, sampleRate);

    mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                               //FftProcessObj16::WindowGaussian); // Was for QIFFT
                               FftProcessObj16::WindowHanning); // TODO: check this
    
    mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                //FftProcessObj16::WindowGaussian);
                                FftProcessObj16::WindowHanning);

    // Do not analyze!
    mFftObj->SetSkipFftProcess(FftProcessObj16::ALL_CHANNELS, true);
}

MorphoFrameSynthetizer::~MorphoFrameSynthetizer()
{
    delete mFftObj;
    delete mFrameSynthetizerObj;
}

void
MorphoFrameSynthetizer::Reset(BL_FLOAT sampleRate)
{
    mFftObj->Reset(BUFFER_SIZE, OVERSAMPLING, FREQ_RES, sampleRate);
    mFrameSynthetizerObj->Reset(sampleRate);
}

void
MorphoFrameSynthetizer::ProcessBlock(vector<WDL_TypedBuf<BL_FLOAT> > &out)
{
    vector<WDL_TypedBuf<BL_FLOAT> > scIn;
    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    in = out;
    
    mFftObj->Process(in, scIn, &out);
}

void
MorphoFrameSynthetizer::SetSynthMode(MorphoFrameSynth2::SynthMode mode)
{
    mFrameSynthetizerObj->SetSynthMode(mode);
}

void
MorphoFrameSynthetizer::AddMorphoFrame(const MorphoFrame7 &frame)
{
    mFrameSynthetizerObj->AddMorphoFrame(frame);
}
