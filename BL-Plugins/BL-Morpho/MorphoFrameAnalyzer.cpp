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
 
#include <MorphoFrameAnalyzerFftObj.h>
#include <FftProcessObj16.h>

#include "MorphoFrameAnalyzer.h"

#define BUFFER_SIZE 2048
#define OVERSAMPLING 4
#define FREQ_RES 1

MorphoFrameAnalyzer::MorphoFrameAnalyzer(BL_FLOAT sampleRate,
                                         bool storeDetectDataInFrames)
{
    int numChannels = 1;
    int numScInputs = 0;
    
    vector<ProcessObj *> processObjs;
    mFrameAnalyzerObj = new MorphoFrameAnalyzerFftObj(BUFFER_SIZE, OVERSAMPLING,
                                                      FREQ_RES, sampleRate,
                                                      storeDetectDataInFrames);
    processObjs.push_back(mFrameAnalyzerObj);

    mFftObj = new FftProcessObj16(processObjs,
                                  numChannels, numScInputs,
                                  BUFFER_SIZE, OVERSAMPLING,
                                  FREQ_RES, sampleRate);

    // Gaussian windows, for QIFFT
    mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                               FftProcessObj16::WindowGaussian);
    // Synth window could be set to Hanning later if necessary 
    mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                FftProcessObj16::WindowGaussian);

    // Do not synthetize
    mFftObj->SetSkipIFft(FftProcessObj16::ALL_CHANNELS, true);
}

MorphoFrameAnalyzer::~MorphoFrameAnalyzer()
{
    delete mFftObj;
    delete mFrameAnalyzerObj;
}

void
MorphoFrameAnalyzer::Reset(BL_FLOAT sampleRate)
{
    mFftObj->Reset(BUFFER_SIZE, OVERSAMPLING, FREQ_RES, sampleRate);
    mFrameAnalyzerObj->Reset(sampleRate);
}

void
MorphoFrameAnalyzer::ProcessBlock(const vector<WDL_TypedBuf<BL_FLOAT> > &in)
{
    vector<WDL_TypedBuf<BL_FLOAT> > scIn;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf0;
    out = in;
    
    mFftObj->Process(in, scIn, &out);
}

void
MorphoFrameAnalyzer::GetMorphoFrames(vector<MorphoFrame7> *frames)
{
    mFrameAnalyzerObj->GetMorphoFrames(frames);
}

void
MorphoFrameAnalyzer::SetDetectThreshold(BL_FLOAT detectThrs)
{
    mFrameAnalyzerObj->SetThreshold(detectThrs);
}

void
MorphoFrameAnalyzer::SetFreqThreshold(BL_FLOAT freqThrs)
{
    mFrameAnalyzerObj->SetThreshold2(freqThrs);
}

void
MorphoFrameAnalyzer::SetTimeSmoothCoeff(BL_FLOAT coeff)
{
    mFrameAnalyzerObj->SetTimeSmoothCoeff(coeff);
}
