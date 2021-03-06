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
//  RebalanceProcessFftObjComp4.h
//  BL-Rebalance
//
//  Created by applematuer on 5/17/20.
//
//

#ifndef __BL_Rebalance__RebalanceProcessFftObjCompStereo__
#define __BL_Rebalance__RebalanceProcessFftObjCompStereo__

#include "IPlug_include_in_plug_hdr.h"

#include "../../WDL/fft.h"

#include <bl_queue.h>
#include <FftProcessObj16.h>

// RebalanceProcessFftObj
//
// Apply the masks to the current channels
//
// RebalanceProcessFftObjComp: from RebalanceProcessFftObj
// - use RebalanceMaskPredictorComp
//
// RebalanceProcessFftObjComp2:
// for RebalanceMaskPredictorComp3
//
// RebalanceProcessFftObjComp3: for ResampProcessObj
// (define target sample rate, more corret for any source sample rate,
// more correct than just downsacaling the mask)
//
// RebalanceProcessFftObjCompStereo: from RebalanceProcessFftObjComp4

class RebalanceMaskPredictor8;
class RebalanceMaskProcessor;
class Scale;
class SoftMaskingNComp4;
class BLSpectrogram4;
class SpectrogramDisplayScroll4;
class RebalanceProcessFftObjCompStereo : public MultichannelProcess
{
public:
    RebalanceProcessFftObjCompStereo(int bufferSize, int oversampling,
                                     BL_FLOAT sampleRate,
                                     RebalanceMaskPredictor8 *maskPred,
                                     int numInputCols,
                                     int softMaskHistoSize);
    
    virtual ~RebalanceProcessFftObjCompStereo();
    
    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate) override;
    
    void Reset();

    BLSpectrogram4 *GetSpectrogram();
    void SetSpectrogramDisplay(SpectrogramDisplayScroll4 *spectroDisplay);

    void SetWidthBoost(bool flag);
    
    // Separate the different parts and keep them
    void
    ProcessInputFft(vector<WDL_TypedBuf<WDL_FFT_COMPLEX> * > *ioFftSamples,
                    const vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > *scBuffer) override;


    // Get separated parts, convert them to samples, then process them
    // (mix, width, stereo). this way we get the best sound
    void
    ProcessResultSamples(vector<WDL_TypedBuf<BL_FLOAT> * > *ioSamples,
                         const vector<WDL_TypedBuf<BL_FLOAT> > *scBuffer) override;

    // Mix
    void SetVocal(BL_FLOAT vocal);
    void SetBass(BL_FLOAT bass);
    void SetDrums(BL_FLOAT drums);
    void SetOther(BL_FLOAT other);

    // Not used...
    void SetVocalSensitivity(BL_FLOAT vocal);
    void SetBassSensitivity(BL_FLOAT bass);
    void SetDrumsSensitivity(BL_FLOAT drums);
    void SetOtherSensitivity(BL_FLOAT other);

    // Stereo
    //
    void SetWidthVocal(BL_FLOAT widthVocal);
    void SetWidthBass(BL_FLOAT widthBass);
    void SetWidthDrums(BL_FLOAT widthDrums);
    void SetWidthOther(BL_FLOAT widthOther);

    void SetPanVocal(BL_FLOAT panVocal);
    void SetPanBass(BL_FLOAT panBass);
    void SetPanDrums(BL_FLOAT panDrums);
    void SetPanOther(BL_FLOAT panOther);
    
    // Global precision (previous soft/hard)
    void SetContrast(BL_FLOAT contrast);

    int GetLatency();

    void RecomputeSpectrogram(bool recomputeMasks = false);
    
protected:
    void AddSpectrogramLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                            const WDL_TypedBuf<BL_FLOAT> &phases);
    
    void ResetSamplesHistory();
    void ResetMixColsComp();
    void ResetMasksHistory();
    void ResetSignalHistory();
    void ResetRawSignalHistory();

    // 
    void ApplySoftMaskingStereo(WDL_TypedBuf<WDL_FFT_COMPLEX> ioData[2],
                                const WDL_TypedBuf<BL_FLOAT> masks[NUM_STEM_SOURCES]);

    void ComputeResult(const WDL_TypedBuf<WDL_FFT_COMPLEX> mixBuffer[2],
                       const WDL_TypedBuf<BL_FLOAT> masks[NUM_STEM_SOURCES],
                       WDL_TypedBuf<WDL_FFT_COMPLEX> result[2],
                       WDL_TypedBuf<BL_FLOAT> resMagns[2],
                       WDL_TypedBuf<BL_FLOAT> resPhases[2]);
    
    int ComputeSpectroNumCols();

    // Reset everything except the raw buffered samples
    void ResetSpectrogram();

    // Stereo
    //
    void ProcessStereoSamples(int partNum, WDL_TypedBuf<BL_FLOAT> samples[2]);

    void ApplyWidthBoost(BL_FLOAT width[NUM_STEM_SOURCES]);
    void ApplyMixBoost(BL_FLOAT mixes[NUM_STEM_SOURCES]);
        
    //
    int mBufferSize;
    int mOverlapping;
    BL_FLOAT mSampleRate;
    
    BLSpectrogram4 *mSpectrogram;
    SpectrogramDisplayScroll4 *mSpectroDisplay;
    
    //
    RebalanceMaskPredictor8 *mMaskPred;
    RebalanceMaskProcessor *mMaskProcessor;
    
    int mNumInputCols;
    
    // Keep the history of input data
    // So we can get exactly the same corresponding the the
    // correct location of the mask
    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mSamplesHistory[2];

    // N = 4
    SoftMaskingNComp4 *mSoftMaskingN[2];
    
    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mMixColsComp[2];

    Scale *mScale;

    // Keep masks history, so when chaging parameters, all the spectrogram changes
    bl_queue<WDL_TypedBuf<BL_FLOAT> > mMasksHistory[NUM_STEM_SOURCES];
    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mSignalHistory[2];

    // For recomputing spectrogram when also mask changes
    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mRawSignalHistory[2];

    // Mix params
    BL_FLOAT mVocalMix;
    BL_FLOAT mBassMix;
    BL_FLOAT mDrumsMix;
    BL_FLOAT mOtherMix;
    
    // Stereo
    //
    BL_FLOAT mWidthVocal;
    BL_FLOAT mWidthBass;
    BL_FLOAT mWidthDrums;
    BL_FLOAT mWidthOther;

    BL_FLOAT mPanVocal;
    BL_FLOAT mPanBass;
    BL_FLOAT mPanDrums;
    BL_FLOAT mPanOther;

    bool mWidthBoost;
    
    // Store the separated fft samples, then re-use them when processing samples
    WDL_TypedBuf<WDL_FFT_COMPLEX> mCurrentFftSamples[NUM_STEM_SOURCES][2];
    
private:
    // Tmp buffers
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3[NUM_STEM_SOURCES];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf7;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf8;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf9;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf10;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf11;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf12;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf13;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf14;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf15;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf16;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf17;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf18[NUM_STEM_SOURCES];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf19;
    vector<WDL_TypedBuf<WDL_FFT_COMPLEX> *> mTmpBuf20;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf21;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf22;
    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mTmpBuf23;

    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf24[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf25[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf26[2];

    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf27[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf28[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf29[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf30;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf31[2];

    bl_queue<WDL_TypedBuf<WDL_FFT_COMPLEX> > mTmpBuf32[2];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf33[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf34[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf35[2];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf36[2];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf37[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf38[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf39[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf40;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf41[NUM_STEM_SOURCES];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf42[NUM_STEM_SOURCES];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf43[4][2]; // NUM_STEM_SOURCES x 2 channels
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf44[4][2]; // NUM_STEM_SOURCES x 2 channels
    WDL_TypedBuf<BL_FLOAT> mTmpBuf45[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf46[2];
    
    WDL_TypedBuf<BL_FLOAT> mTmpBuf47[2];
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf48[2];

    vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > mTmpBuf49;
    vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > mTmpBuf50;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf51;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf52;
};

#endif /* defined(__BL_Rebalance__RebalanceProcessFftObjStereo__) */
