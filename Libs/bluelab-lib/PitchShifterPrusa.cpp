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
 
#include <stdio.h>
#include <math.h>

#include <FftProcessObj16.h>
#include <PitchShiftPrusaFftObj.h>
#include <StereoPhasesProcess.h>

#include <BLUtilsPlug.h>
#include <BLUtilsMath.h>

#include "PitchShifterPrusa.h"

// Better for drums
#define BUFFER_SIZE_0 2048
// Better for Thumb Piano
// Keeps better defined frequencies
#define BUFFER_SIZE_1 4096

// Not used anymore
#define OVERSAMPLING_0 4
#define OVERSAMPLING_1 8
#define OVERSAMPLING_2 16
#define OVERSAMPLING_3 32

#define FREQ_RES 1

// Doesn't change anything, at least for small pitches
// Indeed, we only "shift" the mangnitude bins, we don't
// increase or decrease, add or remove.
#define KEEP_SYNTHESIS_ENERGY 0


// When set to 0, we are accurate for frequencies
#define VARIABLE_HANNING 0

// GOOD
// Avoid phasing effect when stereo processing
//
// BAD: Finally, when tested on Protools on real stereo file
// (Midning extract), it wobbles a little, the sound is less clear
// (compared with prev version 5.0.2: it has these defects too)
//
// That seem to only improved with fake stereo i.e a duplicated
// mono channel
//
// Was useful for Smb! Not tested for Prusa (seems to we don't need it)
#define ADJUST_STEREO_PHASES 0 //1

// Fix bad sound in mono
// (due to try to fix phases between two stereo channels)
//
// Was for Smb (not tested for Prusa (seems we don't need it)
#define FIX_BAD_SOUND_MONO 0 //1

// TODO: maybe use new PhasesEstimPrusa
//
PitchShifterPrusa::PitchShifterPrusa()
{
    mFftObj = NULL;
    mPhasesProcess = NULL;
    
    mPitchObjs[0] = NULL;
    mPitchObjs[1] = NULL;
    
    //
    mBufferSize = BUFFER_SIZE_0;
    mOversampling = OVERSAMPLING_0;
    mSampleRate = 44100.0;
    mFactor = 1.0;

    mQuality = 0;
    UpdateQuality();
    
    InitFft(mSampleRate);
}

PitchShifterPrusa::~PitchShifterPrusa()
{
    if (mFftObj != NULL)
        delete mFftObj;
    
    if (mPhasesProcess != NULL)
        delete mPhasesProcess;
    
    for (int i = 0; i < 2; i++)
    {
        if (mPitchObjs[i] != NULL)
            delete mPitchObjs[i];
    }
}

void
PitchShifterPrusa::Reset(BL_FLOAT sampleRate, int blockSize)
{
    mSampleRate = sampleRate;

    UpdateQuality();
    
    // Sample rate has changed, and we can have variable buffer size
    InitFft(mSampleRate);

    // Called when we restart the playback
    // The cursor position may have changed
    // Then we must reset

    if (mFftObj != NULL)
        mFftObj->Reset(mBufferSize, mOversampling, FREQ_RES, sampleRate);
  
    for (int i = 0; i < 2; i++)
    {
        if (mPitchObjs[i] != NULL)
            mPitchObjs[i]->Reset(mBufferSize, mOversampling, FREQ_RES, sampleRate);
    }
  
    if (mPhasesProcess != NULL)
        mPhasesProcess->Reset(mBufferSize, mOversampling, FREQ_RES, sampleRate);
}

void
PitchShifterPrusa::Process(vector<WDL_TypedBuf<BL_FLOAT> > &in,
                           vector<WDL_TypedBuf<BL_FLOAT> > *out)
{
    if (in.empty())
        return;

#if FIX_BAD_SOUND_MONO
    // FIX: bad sound in mono
    if (in.size() < 2)
    {
        // Mono, disable to avoid bad sound
        if (mPhasesProcess != NULL)
            mPhasesProcess->SetActive(false);
    }
    else
    {
        // Stero: enable to fix phase diff problems
        if (mPhasesProcess != NULL)
            mPhasesProcess->SetActive(true);
    }
#endif

    vector<WDL_TypedBuf<BL_FLOAT> > dummyScIn;
    mFftObj->Process(in, dummyScIn, out);
}
    
void
PitchShifterPrusa::SetFactor(BL_FLOAT factor)
{
    mFactor = factor;

    for (int i = 0; i < 2; i++)
        mPitchObjs[i]->SetFactor(mFactor);

    if (mFftObj != NULL)
        mFftObj->SetOutTimeStretchFactor(mFactor);
}

void
PitchShifterPrusa::SetQuality(int quality)
{
#if 0 // Not used anymore: change the overlapping
    switch(quality)
    {
        case 0:
            mOversampling = 4;
            break;
            
        case 1:
            mOversampling = 8;
            break;
            
        case 2:
            mOversampling = 16;
            break;
            
        case 3:
            mOversampling = 32;
            break;
            
        default:
            break;
    }
#endif

    mQuality = quality;
    UpdateQuality();
    
    //InitFft(mSampleRate);
    Reset(mSampleRate, -1); // Block size is not used anyway
}

int
PitchShifterPrusa::ComputeLatency(int blockSize)
{
    int latency = mFftObj->ComputeLatency(blockSize);

    return latency;
}

void
PitchShifterPrusa::InitFft(BL_FLOAT sampleRate)
{
    if (mFftObj == NULL)
    {      
        vector<ProcessObj *> processObjs;
        for (int i = 0; i < 2; i++)
        {
            mPitchObjs[i] = new PitchShiftPrusaFftObj(mBufferSize, mOversampling,
                                                      FREQ_RES, mSampleRate);
      
            processObjs.push_back(mPitchObjs[i]);
        }
      
        int numChannels = 2;
        int numScInputs = 0;
      
        mFftObj = new FftProcessObj16(processObjs,
                                      numChannels, numScInputs,
                                      mBufferSize, mOversampling, FREQ_RES,
                                      mSampleRate);
      
#if !VARIABLE_HANNING
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowHanning);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowHanning);
#else
        mFftObj->SetAnalysisWindow(FftProcessObj16::ALL_CHANNELS,
                                   FftProcessObj16::WindowVariableHanning);
        mFftObj->SetSynthesisWindow(FftProcessObj16::ALL_CHANNELS,
                                    FftProcessObj16::WindowVariableHanning);
#endif
      
        mFftObj->SetKeepSynthesisEnergy(FftProcessObj16::ALL_CHANNELS,
                                        KEEP_SYNTHESIS_ENERGY);
    
        // Moreover, this seems to avoids phase drift !
#if ADJUST_STEREO_PHASES
        mPhasesProcess = new StereoPhasesProcess(mBufferSize);
        mFftObj->AddMultichannelProcess(mPhasesProcess);
#endif
    }
    else
    {
        mFftObj->Reset(mBufferSize, mOversampling, FREQ_RES, sampleRate);
        
#if ADJUST_STEREO_PHASES
        mPhasesProcess->Reset(mBufferSize, mOversampling, FREQ_RES, sampleRate);
#endif
    }
}

void
PitchShifterPrusa::UpdateQuality()
{
    BL_FLOAT sampleRateCoeff = mSampleRate/44100.0;
    int sampleRateCoeffI = (int)sampleRateCoeff;
    sampleRateCoeffI = BLUtilsMath::NextPowerOfTwo(sampleRateCoeffI);
    
    // Change buffer size
    switch(mQuality)
    {
        case 0:
            mBufferSize = BUFFER_SIZE_0*sampleRateCoeffI;
            break;
            
        case 1:
            mBufferSize = BUFFER_SIZE_1*sampleRateCoeffI;
            break;
            
        default:
            break;
    }
}
