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
//  RebalanceProcessFftObjComp4.cpp
//  BL-Rebalance
//
//  Created by applematuer on 5/17/20.
//
//

#include <RebalanceMaskPredictor8.h>
#include <RebalanceMaskProcessor.h>

#include <BLUtils.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>
#include <BLUtilsMath.h>

#include <BLDebug.h>
#include <Scale.h>

#include <SoftMaskingComp4.h>

#include <BLSpectrogram4.h>
#include <SpectrogramDisplayScroll4.h>

#include <Rebalance_defs.h>

#include "RebalanceProcessFftObjComp4.h"

// Post normalize, so that when everything is set to default,
// the plugin is transparent
#define POST_NORMALIZE 1

//#define SPECTRO_NUM_COLS 2048/4 //64
// Reduce a bit, since we have a small graph
#define SPECTRO_HEIGHT 256 //2048/4

#define SOFT_MASKING_HISTO_SIZE 8

#define USE_SOFT_MASKING 1 //0

// Origin: was 0
#define SMOOTH_SPECTRO_DISPLAY 1 // 0

// With this, it will be possible to increase the gain a lot
// when setting mix at 200%
// (Otherwise, the gain effect was very small)
#define SOFT_MASKING_HACK 1

RebalanceProcessFftObjComp4::
RebalanceProcessFftObjComp4(int bufferSize, int oversampling,
                            BL_FLOAT sampleRate,
                            RebalanceMaskPredictor8 *maskPred,
                            int numInputCols,
                            int softMaskHistoSize)
: ProcessObj(bufferSize)
{
    mSampleRate = sampleRate;
    
    mMaskPred = maskPred;
    
    mNumInputCols = numInputCols;

    mSpectrogram = new BLSpectrogram4(sampleRate, SPECTRO_HEIGHT, -1);
    
    mSpectroDisplay = NULL;
    
    mScale = new Scale();
    
    ResetSamplesHistory();
    
    // Soft masks
    mSoftMasking = new SoftMaskingComp4(bufferSize, oversampling,
                                        SOFT_MASKING_HISTO_SIZE);
    
    mMaskProcessor = new RebalanceMaskProcessor();
    
    ResetMixColsComp();
}

RebalanceProcessFftObjComp4::~RebalanceProcessFftObjComp4()
{
    if (mSoftMasking != NULL)
        delete mSoftMasking;
 
    delete mMaskProcessor;
    
    delete mScale;
    delete mSpectrogram;
}

void
RebalanceProcessFftObjComp4::Reset(int bufferSize, int oversampling,
                                   int freqRes, BL_FLOAT sampleRate)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);

    //mSampleRate = sampleRate;
    
    if (mSoftMasking != NULL)
        mSoftMasking->Reset(bufferSize, oversampling);
    
    mMaskPred->Reset();
    
    ResetSamplesHistory();
    ResetMixColsComp();

    ResetMasksHistory();
    ResetSignalHistory();

    ResetRawSignalHistory();
    
    int numCols = ComputeSpectroNumCols();
    mSpectrogram->Reset(sampleRate, SPECTRO_HEIGHT, numCols);
}

void
RebalanceProcessFftObjComp4::Reset()
{
    if (mSoftMasking != NULL)
        mSoftMasking->Reset();
   
    mMaskPred->Reset();
    
    ResetSamplesHistory();
    ResetMixColsComp();

    ResetMasksHistory();
    ResetSignalHistory();

    ResetRawSignalHistory();
 
    int numCols = ComputeSpectroNumCols();
    mSpectrogram->Reset(mSampleRate, SPECTRO_HEIGHT, numCols);
}

BLSpectrogram4 *
RebalanceProcessFftObjComp4::GetSpectrogram()
{
    return mSpectrogram;
}

void
RebalanceProcessFftObjComp4::
SetSpectrogramDisplay(SpectrogramDisplayScroll4 *spectroDisplay)
{
    mSpectroDisplay = spectroDisplay;
}

void
RebalanceProcessFftObjComp4::SetVocal(BL_FLOAT vocal)
{
    mMaskProcessor->SetVocalMix(vocal);
}

void
RebalanceProcessFftObjComp4::SetBass(BL_FLOAT bass)
{
    mMaskProcessor->SetBassMix(bass);
}

void
RebalanceProcessFftObjComp4::SetDrums(BL_FLOAT drums)
{
    mMaskProcessor->SetDrumsMix(drums);
}

void
RebalanceProcessFftObjComp4::SetOther(BL_FLOAT other)
{
    mMaskProcessor->SetOtherMix(other);
}

void
RebalanceProcessFftObjComp4::SetVocalSensitivity(BL_FLOAT vocal)
{
    mMaskProcessor->SetVocalSensitivity(vocal);
}

void
RebalanceProcessFftObjComp4::SetBassSensitivity(BL_FLOAT bass)
{
    mMaskProcessor->SetBassSensitivity(bass);
}

void
RebalanceProcessFftObjComp4::SetDrumsSensitivity(BL_FLOAT drums)
{
    mMaskProcessor->SetDrumsSensitivity(drums);
}

void
RebalanceProcessFftObjComp4::SetOtherSensitivity(BL_FLOAT other)
{
    mMaskProcessor->SetOtherSensitivity(other);
}

void
RebalanceProcessFftObjComp4::SetContrast(BL_FLOAT contrast)
{
    mMaskProcessor->SetContrast(contrast);
}

int
RebalanceProcessFftObjComp4::GetLatency()
{
#if !USE_SOFT_MASKING
    return 0;
#endif
    
    if (mSoftMasking == NULL)
        return 0;

    int latency = mSoftMasking->GetLatency();

    return latency;
}
    
void
RebalanceProcessFftObjComp4::AddSpectrogramLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                                                const WDL_TypedBuf<BL_FLOAT> &phases)
{
    // When disabled: the spectrogram display jitters less
    // even whn much resource is consumed
    // And also for updating whole spectrogram when param change
#if SMOOTH_SPECTRO_DISPLAY
    // Add for smooth scroll
    if (mSpectroDisplay != NULL)
        mSpectroDisplay->AddSpectrogramLine(magns, phases);
#else
    // Simple add
    mSpectrogram->AddLine(magns, phases);
#endif
    
    // For updating whole spectrogram when param change
    if (mSpectroDisplay != NULL)
        mSpectroDisplay->UpdateSpectrogram(true);
}

void
RebalanceProcessFftObjComp4::
ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                 const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)
{
    int numCols = ComputeSpectroNumCols();
    
    // Keep input signal history
    if (mRawSignalHistory.size() < numCols)
        mRawSignalHistory.push_back(*ioBuffer);
    else
    {
        mRawSignalHistory.freeze();
        mRawSignalHistory.push_pop(*ioBuffer);
    }
    
    // Mix
    WDL_TypedBuf<WDL_FFT_COMPLEX> &mixBuffer = mTmpBuf0;
    BLUtils::TakeHalf(*ioBuffer, &mixBuffer);
    
#if PROCESS_SIGNAL_DB
    WDL_TypedBuf<BL_FLOAT> &magns0 = mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> &phases0 = mTmpBuf2;
    
    BLUtilsComp::ComplexToMagnPhase(&magns0, &phases0, mixBuffer);

#if 0 // Origin
    for (int i = 0; i < magns0.GetSize(); i++)
    {
        BL_FLOAT val = magns0.Get()[i];
        val = mScale->ApplyScale(Scale::DB, val,
                                 (BL_FLOAT)PROCESS_SIGNAL_MIN_DB, (BL_FLOAT)0.0);
        magns0.Get()[i] = val;
    }
#endif

#if 1  // Optimized
    mScale->ApplyScaleForEach(Scale::DB, &magns0,
                              (BL_FLOAT)PROCESS_SIGNAL_MIN_DB, (BL_FLOAT)0.0);
#endif
    
    BLUtilsComp::MagnPhaseToComplex(&mixBuffer, magns0, phases0);
#endif
    
    // For soft masks
    // mMixCols is filled with zeros at the origin
    mMixColsComp.freeze();
    mMixColsComp.push_pop(mixBuffer);
    
    // History, to stay synchronized between input signal and masks
    mSamplesHistory.freeze();
    mSamplesHistory.push_pop(mixBuffer);
    
    int histoIndex = mMaskPred->GetHistoryIndex();
    if (histoIndex < mSamplesHistory.size())
        mixBuffer = mSamplesHistory[histoIndex];

    WDL_TypedBuf<BL_FLOAT> *masks = mTmpBuf3;
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
        mMaskPred->GetMask(i, &masks[i]);
    
    // Keep mask and signal histories
    if (mSignalHistory.size() < numCols)
        mSignalHistory.push_back(mixBuffer);
    else
    {
        mSignalHistory.freeze();
        mSignalHistory.push_pop(mixBuffer);
    }

    for (int i = 0; i < NUM_STEM_SOURCES; i++)
    {
        if (mMasksHistory[i].size() < numCols)
            mMasksHistory[i].push_back(masks[i]);
        else
        {
            mMasksHistory[i].freeze();
            mMasksHistory[i].push_pop(masks[i]);
        }
    }
    
    // Adjust and apply mask
    WDL_TypedBuf<WDL_FFT_COMPLEX> &result = mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> &magns1 = mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> &phases1 = mTmpBuf6;
    ComputeResult(mixBuffer, masks, &result, &magns1, &phases1);
    
    AddSpectrogramLine(magns1, phases1);
    
    // TODO: tmp buffers / memory optimization
    
    // Fill the result
    WDL_TypedBuf<WDL_FFT_COMPLEX> &fftSamples = mTmpBuf7;
    fftSamples = result;
    
    BLUtilsFft::FillSecondFftHalf(fftSamples, ioBuffer);
}

void
RebalanceProcessFftObjComp4::ResetSamplesHistory()
{
    mSamplesHistory.resize(mNumInputCols);
    
    for (int i = 0; i < mNumInputCols; i++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> &samples = mTmpBuf8;
        samples.Resize(mBufferSize/2);
        BLUtils::FillAllZero(&samples);
        
        mSamplesHistory[i] = samples;
    }
}

void
RebalanceProcessFftObjComp4::
ApplyMask(const WDL_TypedBuf<WDL_FFT_COMPLEX> &inData,
          WDL_TypedBuf<WDL_FFT_COMPLEX> *outData,
          const WDL_TypedBuf<BL_FLOAT> &mask)
{
    // TODO: implement method in BLUtils: multvalues(in, out, mask) in complex
    *outData = inData;
    BLUtils::MultValues(outData, mask);
}

void
RebalanceProcessFftObjComp4::ResetMixColsComp()
{
    mMixColsComp.resize(mNumInputCols);
    
    for (int i = 0; i < mNumInputCols; i++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> &col = mTmpBuf9;
        col.Resize(mBufferSize/2);
        BLUtils::FillAllZero(&col);
        
        mMixColsComp[i] = col;
    }
}

void
RebalanceProcessFftObjComp4::ResetMasksHistory()
{
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
    {
        mMasksHistory[i].unfreeze();
        mMasksHistory[i].clear();
    }
}

void
RebalanceProcessFftObjComp4::ResetSignalHistory()
{
    mSignalHistory.unfreeze();
    mSignalHistory.clear();
}

void
RebalanceProcessFftObjComp4::ResetRawSignalHistory()
{
    mRawSignalHistory.unfreeze();
    mRawSignalHistory.clear();
}
    
void
RebalanceProcessFftObjComp4::ApplySoftMasking(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioData,
                                              const WDL_TypedBuf<BL_FLOAT> &mask0)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> &softMaskedResult = mTmpBuf10;

    WDL_TypedBuf<BL_FLOAT> &mask = mTmpBuf19;
    mask = mask0;

#if SOFT_MASKING_HACK
    // s = input * HM
    // s = (input * alpha) * HM/alpha
    //
    // here, alpha = 4.0
    //
    // This is a hack, but it works well! :)
    //
    BLUtils::MultValues(&mask, (BL_FLOAT)(1.0/4.0));
    BLUtils::MultValues(ioData, (BL_FLOAT)4.0);
#endif
    
    mSoftMasking->ProcessCentered(ioData, mask, &softMaskedResult);

#if SOFT_MASKING_HACK
    // Empirical coeff
    //
    // Compared the input hard mask here, and the soft mask inside mSoftMasking
    // When maks i 1.0 here (all at 100%), the soft mask is 0.1
    // So adjust here with a factor 10.0, plus fix the previous gain of 4.0
    //
    // NOTE: this makes the plugin transparent when all is at 100%
    //
    BLUtils::MultValues(&softMaskedResult, (BL_FLOAT)(10.0/4.0));
#endif
    
    // Result
    if (mSoftMasking->IsProcessingEnabled())
        *ioData = softMaskedResult;
}

void
RebalanceProcessFftObjComp4::ComputeInverseDB(WDL_TypedBuf<BL_FLOAT> *magns)
{
#if 0 // Origin
    for (int i = 0; i < magns->GetSize(); i++)
    {
        BL_FLOAT val = magns->Get()[i];
        val = mScale->ApplyScaleInv(Scale::DB, val,
                                    (BL_FLOAT)PROCESS_SIGNAL_MIN_DB, (BL_FLOAT)0.0);
        
        // Noise floor
        BL_FLOAT db = BLUtils::AmpToDB(val);
        if (db < PROCESS_SIGNAL_MIN_DB + 1)
            val = 0.0;
        
        magns->Get()[i] = val;
    }
#endif

#if 1 // Optimized
    // NOTE: do we need the noise floor test like above?
    mScale->ApplyScaleInvForEach(Scale::DB, magns,
                                 (BL_FLOAT)PROCESS_SIGNAL_MIN_DB, (BL_FLOAT)0.0);
#endif
}

void
RebalanceProcessFftObjComp4::RecomputeSpectrogram(bool recomputeMasks)
{
    if (recomputeMasks)
    {
        // Clear all
        ResetSpectrogram();
        
        // Reprocess globally the previous input signal 
        bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > &rawSignalHistoryCopy = mTmpBuf23;
        rawSignalHistoryCopy = mRawSignalHistory;

        // The reset the current one (it will be refilled during reocmputation) 
        ResetRawSignalHistory();
            
        for (int i = 0; i < rawSignalHistoryCopy.size(); i++)
        {
            WDL_TypedBuf<WDL_FFT_COMPLEX> &signal = mTmpBuf21;
            signal = rawSignalHistoryCopy[i];

            //
            vector<WDL_TypedBuf<WDL_FFT_COMPLEX> *> &signalVec = mTmpBuf20;
            signalVec.resize(1);
            signalVec[0] = &signal;

            mMaskPred->ProcessInputFft(&signalVec, NULL);

            //
            ProcessFftBuffer(&signal, NULL);
        }
        
        // We have finished with recompting everything
        return;
    }

    // Be sure to reset!
    if (mSoftMasking != NULL)
        mSoftMasking->Reset(mBufferSize, mOverlapping);
    
    // Keep lines, and add them all at once at the end 
    vector<WDL_TypedBuf<BL_FLOAT> > &magnsVec = mTmpBuf11;
    vector<WDL_TypedBuf<BL_FLOAT> > &phasesVec = mTmpBuf12;

    magnsVec.resize(mSignalHistory.size());
    phasesVec.resize(mSignalHistory.size());
    
    for (int i = 0; i < mSignalHistory.size(); i++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> &signal = mTmpBuf13;
        signal = mSignalHistory[i];

        WDL_TypedBuf<BL_FLOAT> *masks = mTmpBuf18;

        // Do not recompute masks, re-use current ones
        for (int j = 0; j < NUM_STEM_SOURCES; j++)
            masks[j] = mMasksHistory[j][i];
                
        WDL_TypedBuf<WDL_FFT_COMPLEX> &result = mTmpBuf14;
        WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf15;
        WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf16;
        ComputeResult(signal, masks, &result, &magns, &phases);
        
        magnsVec[i] = magns;
        phasesVec[i] = phases;
    }

    // Add all lines at once at the end
    mSpectrogram->SetLines(magnsVec, phasesVec);

    if (mSpectroDisplay != NULL)
        mSpectroDisplay->UpdateSpectrogram(true);
}

void
RebalanceProcessFftObjComp4::
ComputeResult(const WDL_TypedBuf<WDL_FFT_COMPLEX> &mixBuffer,
              const WDL_TypedBuf<BL_FLOAT> masks[NUM_STEM_SOURCES],
              WDL_TypedBuf<WDL_FFT_COMPLEX> *result,
              WDL_TypedBuf<BL_FLOAT> *resMagns,
              WDL_TypedBuf<BL_FLOAT> *resPhases)
{
    result->Resize(mixBuffer.GetSize());
    BLUtils::FillAllZero(result);
    
    WDL_TypedBuf<BL_FLOAT> &mask = mTmpBuf17;
    mMaskProcessor->Process(masks, &mask);
    
#if !USE_SOFT_MASKING
    ApplyMask(mixBuffer, result, mask);
#endif
    
#if USE_SOFT_MASKING
    *result = mixBuffer;
    ApplySoftMasking(result, mask);
#endif
    
    BLUtilsComp::ComplexToMagnPhase(resMagns, resPhases, *result);

#if PROCESS_SIGNAL_DB
    ComputeInverseDB(resMagns);
    
    BLUtilsComp::MagnPhaseToComplex(result, *resMagns, *resPhases);
#endif
}

int
RebalanceProcessFftObjComp4::ComputeSpectroNumCols()
{
    // Prefer this, so the scroll speed won't be modified when
    // the overlapping changes
    int numCols = mBufferSize/(32/mOverlapping);
    
    // Adjust to the sample rate to avoid scrolling
    // 2 times faster when we go from 44100 to 88200
    //
    BL_FLOAT srCoeff = mSampleRate/44100.0;
    srCoeff = bl_round(srCoeff);
    numCols *= srCoeff;
    
    return numCols;
}

// Reset everything except the raw buffered samples
void
RebalanceProcessFftObjComp4::ResetSpectrogram()
{
    if (mSoftMasking != NULL)
        mSoftMasking->Reset(mBufferSize, mOverlapping);

    mMaskPred->Reset();
    
    ResetSamplesHistory();
    ResetMixColsComp();

    ResetMasksHistory();
    ResetSignalHistory();

    // Don't reset raw samples
    
    int numCols = ComputeSpectroNumCols();
    mSpectrogram->Reset(mSampleRate, SPECTRO_HEIGHT, numCols);
}
