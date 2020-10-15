//
//  RebalanceProcessFftObjComp3.cpp
//  BL-Rebalance
//
//  Created by applematuer on 5/17/20.
//
//

#include <RebalanceMaskPredictorComp6.h>

#include <DbgSpectrogram.h>

#include <BLUtils.h>
#include <SoftMaskingNComp.h>

#include "RebalanceProcessFftObjComp3.h"

// Post normalize, so that when everything is set to default, the plugin is transparent
#define POST_NORMALIZE 1


RebalanceProcessFftObjComp3::RebalanceProcessFftObjComp3(int bufferSize,
                                                         RebalanceMaskPredictorComp6 *maskPred,
                                                         int numInputCols,
                                                         int softMaskHistoSize)
: ProcessObj(bufferSize)
{
    mMaskPred = maskPred;
    
    mMode = RebalanceMode::SOFT;
    
    mNumInputCols = numInputCols;
    
    ResetSamplesHistory();
    
    // Soft masks
    mSoftMasking = new SoftMaskingNComp(softMaskHistoSize);
    mUseSoftMasks = true;
    
    ResetMixColsComp();
    
    // Mix parameters
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
        mMixes[i] = 0.0;
}

RebalanceProcessFftObjComp3::~RebalanceProcessFftObjComp3()
{
    delete mSoftMasking;
}

void
RebalanceProcessFftObjComp3::Reset(int bufferSize, int oversampling,
                                   int freqRes, BL_FLOAT sampleRate)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);
    
    mSoftMasking->Reset();
    
    // NEW
    mMaskPred->Reset();
    
    // NEW
    ResetSamplesHistory();
    ResetMixColsComp();
}

void
RebalanceProcessFftObjComp3::Reset()
{
    ProcessObj::Reset();
    
    mSoftMasking->Reset();
    
    // NEW
    mMaskPred->Reset();
    
    // NEW
    ResetSamplesHistory();
    ResetMixColsComp();
}

void
RebalanceProcessFftObjComp3::SetVocal(BL_FLOAT vocal)
{
    mMixes[0] = vocal;
}

void
RebalanceProcessFftObjComp3::SetBass(BL_FLOAT bass)
{
    mMixes[1] = bass;
}

void
RebalanceProcessFftObjComp3::SetDrums(BL_FLOAT drums)
{
    mMixes[2] = drums;
}

void
RebalanceProcessFftObjComp3::SetOther(BL_FLOAT other)
{
    mMixes[3] = other;
}

void
RebalanceProcessFftObjComp3::SetMasksContrast(BL_FLOAT contrast)
{
    mMaskPred->SetMasksContrast(contrast);
}

void
RebalanceProcessFftObjComp3::SetMode(RebalanceMode mode)
{
    mMode = mode;
}

void
RebalanceProcessFftObjComp3::ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                                             const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)
{
    // Mix
    WDL_TypedBuf<WDL_FFT_COMPLEX> mixBuffer = *ioBuffer;
    BLUtils::TakeHalf(&mixBuffer);
    
    // For soft masks
    // mMixCols is filled with zeros at the origin
    mMixColsComp.push_back(mixBuffer);
    mMixColsComp.pop_front();
    
    // History, to stay synchronized between input signal and masks
    mSamplesHistory.push_back(mixBuffer);
    mSamplesHistory.pop_front();
    
    int histoIndex = mMaskPred->GetHistoryIndex();
    if (histoIndex < mSamplesHistory.size())
        mixBuffer = mSamplesHistory[histoIndex];
    
    // Interpolate between soft and hard
    // TODO: smooth gamma between soft and hard
    WDL_TypedBuf<WDL_FFT_COMPLEX> dataSoft;
    ComputeMix(&dataSoft, mixBuffer);
    
    // Fill the result
    WDL_TypedBuf<WDL_FFT_COMPLEX> fftSamples = dataSoft;
    
    fftSamples.Resize(fftSamples.GetSize()*2);
    
    BLUtils::FillSecondFftHalf(&fftSamples);
    
    // Result
    *ioBuffer = fftSamples;
}

void
RebalanceProcessFftObjComp3::SetUseSoftMasks(bool flag)
{
    mUseSoftMasks = flag;
    
    Reset();
}

// Previously named ComputeMixSoft()
void
RebalanceProcessFftObjComp3::ComputeMix(WDL_TypedBuf<WDL_FFT_COMPLEX> *dataResult,
                                        const WDL_TypedBuf<WDL_FFT_COMPLEX> &dataMix)
{
    BLUtils::ResizeFillZeros(dataResult, dataMix.GetSize());
    
    if (mMaskPred == NULL)
        return;
    
    WDL_TypedBuf<BL_FLOAT> masks0[NUM_STEM_SOURCES];
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
        mMaskPred->GetMask(i, &masks0[i]);
    
    WDL_TypedBuf<WDL_FFT_COMPLEX> masks[NUM_STEM_SOURCES];
    ApplySoftMasks(masks, masks0);
    
#if POST_NORMALIZE
    NormalizeMasks(masks);
#endif
    
    // Must apply mix after soft masks,
    // because if mix param is > 1,
    // soft masks will not manage well if mask is > 1
    // (mask will not have the same spectrogram "shape" at the end when > 1)
    //
    ApplyMix(masks);
    
    ApplyMask(dataMix, dataResult, masks);
}

void
RebalanceProcessFftObjComp3::ApplySoftMasks(WDL_TypedBuf<WDL_FFT_COMPLEX> masksResult[NUM_STEM_SOURCES],
                                            const WDL_TypedBuf<BL_FLOAT> masksSource[NUM_STEM_SOURCES])
{
    if (masksSource[0].GetSize() == 0)
        return;
    
    //
    vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > softMasks;
    if (mUseSoftMasks)
    {
        // Use history and soft masking
        int histoIndex = mMaskPred->GetHistoryIndex();
        if (histoIndex >= mMixColsComp.size())
            return;
        WDL_TypedBuf<WDL_FFT_COMPLEX> mix = mMixColsComp[histoIndex];
        
        vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > estimData;
        estimData.push_back(mix);
        estimData.push_back(mix);
        estimData.push_back(mix);
        estimData.push_back(mix);
        
        for (int i = 0; i < NUM_STEM_SOURCES; i++)
            BLUtils::MultValues(&estimData[i], masksSource[i]);
        
        mSoftMasking->Process(mix, estimData, &softMasks);
    }
    else
    {
        // Simply convert masks to complex
        softMasks.resize(NUM_STEM_SOURCES);
        for (int i = 0; i < NUM_STEM_SOURCES; i++)
            softMasks[i].Resize(masksSource[i].GetSize());
        
        for (int i = 0; i < NUM_STEM_SOURCES; i++)
        {
            for (int j = 0; j < softMasks[i].GetSize(); j++)
            {
                softMasks[i].Get()[j].re = masksSource[i].Get()[j];
                softMasks[i].Get()[j].im = 0.0;
            }
        }
    }
    
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
        masksResult[i] = softMasks[i];
}

void
RebalanceProcessFftObjComp3::CompDiv(vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > *estim,
                                     const WDL_TypedBuf<WDL_FFT_COMPLEX> &mix)
{
    for (int k = 0; k < NUM_STEM_SOURCES; k++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> &est = (*estim)[k];
        for (int i = 0; i < est.GetSize(); i++)
        {
            WDL_FFT_COMPLEX est0 = est.Get()[i];
            WDL_FFT_COMPLEX mix0 = mix.Get()[i];
            
            WDL_FFT_COMPLEX res;
            COMP_DIV(est0, mix0, res);
            
            est.Get()[i] = res;
        }
    }
}

void
RebalanceProcessFftObjComp3::ResetSamplesHistory()
{
    mSamplesHistory.clear();
    
    for (int i = 0; i < mNumInputCols; i++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> samples;
        BLUtils::ResizeFillZeros(&samples, mBufferSize/2);
        
        mSamplesHistory.push_back(samples);
    }
}

void
RebalanceProcessFftObjComp3::ResetMixColsComp()
{
    mMixColsComp.clear();
    
    for (int i = 0; i < mNumInputCols; i++)
    {
        WDL_TypedBuf<WDL_FFT_COMPLEX> col;
        BLUtils::ResizeFillZeros(&col, mBufferSize/2);
        
        mMixColsComp.push_back(col);
    }
}

void
RebalanceProcessFftObjComp3::ApplyMix(WDL_FFT_COMPLEX masks[NUM_STEM_SOURCES])
{
    // Apply mix
    for (int j = 0; j < NUM_STEM_SOURCES; j++)
    {
        masks[j].re *= mMixes[j];
        masks[j].im *= mMixes[j];
    }
}

void
RebalanceProcessFftObjComp3::ApplyMix(WDL_TypedBuf<WDL_FFT_COMPLEX> masks[NUM_STEM_SOURCES])
{
    for (int i = 0; i < masks[0].GetSize(); i++)
    {
        WDL_FFT_COMPLEX vals[NUM_STEM_SOURCES];
        for (int j = 0; j < NUM_STEM_SOURCES; j++)
            vals[j] = masks[j].Get()[i];
        
        ApplyMix(vals);
        
        for (int j = 0; j < NUM_STEM_SOURCES; j++)
            masks[j].Get()[i] = vals[j];
    }
}

void
RebalanceProcessFftObjComp3::ApplyMask(const WDL_TypedBuf<WDL_FFT_COMPLEX> &inData,
                                       WDL_TypedBuf<WDL_FFT_COMPLEX> *outData,
                                       const WDL_TypedBuf<WDL_FFT_COMPLEX> masks[NUM_STEM_SOURCES])
{
    for (int i = 0; i < outData->GetSize(); i++)
    {
        // Mask values
        WDL_FFT_COMPLEX coeffs[NUM_STEM_SOURCES];
        for (int j = 0; j < NUM_STEM_SOURCES; j++)
            coeffs[j] = masks[j].Get()[i];
        
        // NOTE: no need to convert this line to complex
        // (don't know how to do this, and OTHER_IS_REST is 0!)
#if OTHER_IS_REST
        coeffs[3] = 1.0 - (coeffs[0] + coeffs[1] + coeffs[2]);
#endif
        
        // Final coeff
        WDL_FFT_COMPLEX coeff;
        coeff.re = coeffs[0].re + coeffs[1].re + coeffs[2].re + coeffs[3].re;
        coeff.im = coeffs[0].im + coeffs[1].im + coeffs[2].im + coeffs[3].im;
        
        WDL_FFT_COMPLEX val = inData.Get()[i];
        
        // Result
        WDL_FFT_COMPLEX res;
        COMP_MULT(val, coeff, res);
        outData->Get()[i] = res;
    }
}

void
RebalanceProcessFftObjComp3::NormalizeMasks(WDL_TypedBuf<WDL_FFT_COMPLEX> masks[NUM_STEM_SOURCES])
{
    WDL_FFT_COMPLEX vals[NUM_STEM_SOURCES];
    for (int i = 0; i < masks[0].GetSize(); i++)
    {
        for (int k = 0; k < NUM_STEM_SOURCES; k++)
        {
            vals[k] = masks[k].Get()[i];
        }
        
        NormalizeMaskVals(vals);
        
        for (int k = 0; k < NUM_STEM_SOURCES; k++)
        {
            masks[k].Get()[i] = vals[k];
        }
    }
}

void
RebalanceProcessFftObjComp3::NormalizeMaskVals(WDL_FFT_COMPLEX maskVals[NUM_STEM_SOURCES])
{
    // Compute sum magns
    BL_FLOAT sumMagns = 0.0;
    for (int k = 0; k < NUM_STEM_SOURCES; k++)
    {
        const WDL_FFT_COMPLEX &val = maskVals[k];
        BL_FLOAT magn = COMP_MAGN(val);
        
        sumMagns += magn;
    }
    
    if (sumMagns < BL_EPS)
        return;
    
    BL_FLOAT invSum = 1.0/sumMagns;
    
    for (int k = 0; k < NUM_STEM_SOURCES; k++)
    {
        WDL_FFT_COMPLEX val = maskVals[k];
        
        val.re *= invSum;
        val.im *= invSum;
        
        maskVals[k] = val;
    }
}
