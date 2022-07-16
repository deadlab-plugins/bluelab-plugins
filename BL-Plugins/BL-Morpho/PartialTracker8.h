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
//  PartialTracker8.h
//  BL-SASViewer
//
//  Created by applematuer on 2/2/19.
//
//

#ifndef __BL_SASViewer__PartialTracker8__
#define __BL_SASViewer__PartialTracker8__

#include <vector>
#include <deque>
using namespace std;

#include <bl_queue.h>

#include <SimpleKalmanFilter.h>
#include <Scale.h>

#include <PeakDetector.h>

#include <Partial2.h>

#include "IPlug_include_in_plug_hdr.h"

#include "../../WDL/fft.h"

// Use PeakDetector class, original BlueLab implementation
#define USE_BL_PEAK_DETECTOR 0 //1

// Use smart peak detection from http://billauer.co.il/peakdet.html
// See also: https://github.com/xuphys/peakdetect/blob/master/peakdetect.c
//
// NOTE: this must be define in header, because some GUI controls need it
#define USE_BILLAUER_PEAK_DETECTOR 1 // 0

#define USE_PARTIAL_FILTER_MARCHAND 0 //1
#define USE_PARTIAL_FILTER_AMFM 1 //0

// Detect partials
//
#if USE_BL_PEAK_DETECTOR
#define DETECT_PARTIALS_START_INDEX 2
#else
// ORIGIN
//#define DETECT_PARTIALS_START_INDEX 1
// Better, in order to not discard low freq peaks with Billauer real prominence
#define DETECT_PARTIALS_START_INDEX 0
#endif

#define CODE_CLEAN_MORPHO 1

// PartialTracker2
// - from PartialTracker
//
// For association, sort by biggest amplitudes,
// and use a freqency threshold instead of taking the nearest
//
// Compute predicted next ppartial and associate using prediction
//
//
// PartialTracker3:
// - amps in Db, and magns scaled to Mel
//
// PartialTracker4:
// - fix partial amp computation
// (was sometimes really under the correct value for Infra, tested on
// "Bass-065_keep-the-bass-dubby"

// PartialTracker5: improvement of trakcng quality, for new SASViewer
// NOTE: previously test FreqAdjustObj. this was not efficient (wobbling)
//class FreqAdjustObj3;
class PartialFilter2;
class PartialTracker8
{
public:
    PartialTracker8(int bufferSize, BL_FLOAT sampleRate,
                    BL_FLOAT overlapping);
    
    virtual ~PartialTracker8();
    
    void Reset();
    
    void Reset(int bufferSize, BL_FLOAT sampleRate);
    
    //BL_FLOAT GetMinAmpDB();
    
    void SetThreshold(BL_FLOAT threshold);
    void SetThreshold2(BL_FLOAT threshold2);

    // Use one or the other following methods
    //
    
    // Magn/phase
    void SetData(const WDL_TypedBuf<BL_FLOAT> &magns,
                 const WDL_TypedBuf<BL_FLOAT> &phases);

    // Complex
    void SetData(const WDL_TypedBuf<WDL_FFT_COMPLEX> &data);
    
    void GetPreProcessedMagns(WDL_TypedBuf<BL_FLOAT> *magns);
    
    //
    void DetectPartials();
    void FilterPartials();
    
    void GetPartials(vector<Partial2> *partials);

    // For getting current partials before filtering
    void GetRawPartials(vector<Partial2> *partials);
    
    void ClearResult();
    
    // Maximum frequency we try to detect (limit for BL-Infra for example)
    void SetMaxDetectFreq(BL_FLOAT maxFreq);
    
    // Preprocess time smooth
    void SetTimeSmoothCoeff(BL_FLOAT coeff);
    
    // For processing result warping for example
    void PreProcessDataX(WDL_TypedBuf<BL_FLOAT> *data);

    //
    void PreProcessDataY(WDL_TypedBuf<BL_FLOAT> *data);
    
    // For processing result color for example, just before display
    void PreProcessDataXY(WDL_TypedBuf<BL_FLOAT> *data);
    
    // Unwrap phases for interpolatin in Mel
    void PreProcessUnwrapPhases(WDL_TypedBuf<BL_FLOAT> *magns,
                                WDL_TypedBuf<BL_FLOAT> *phases);
    
    void DenormPartials(vector<Partial2> *partials);
    void DenormData(WDL_TypedBuf<BL_FLOAT> *data);
    
    void PartialsAmpToAmpDB(vector<Partial2> *partials);

    BL_FLOAT PartialScaleToQIFFTScale(BL_FLOAT ampDbNorm);
    BL_FLOAT QIFFTScaleToPartialScale(BL_FLOAT ampLog);

    void SetNeriDelta(BL_FLOAT delta);
    
protected:
    // Pre process
    //
    void PreProcess(WDL_TypedBuf<BL_FLOAT> *magns,
                    WDL_TypedBuf<BL_FLOAT> *phases);
    
    // Do it in the complex domain (to be compatible with AM/FM parameters)
    void PreProcessTimeSmooth(WDL_TypedBuf<WDL_FFT_COMPLEX> *data);
    
    // Apply A-Weighting, so the peaks at highest frequencies will not be small
    void PreProcessAWeighting(WDL_TypedBuf<BL_FLOAT> *magns, bool reverse = false);
    
    BL_FLOAT ProcessAWeighting(int binNum, int numBins,
                               BL_FLOAT magn, bool reverse);


    void ComputePartials(const vector<PeakDetector::Peak> &peaks,
                         const WDL_TypedBuf<BL_FLOAT> &magns,
                         const WDL_TypedBuf<BL_FLOAT> &phases,
                         vector<Partial2> *partials);

    void RemoveRealDeadPartials(vector<Partial2> *partials);
        
    // Detection
    //
    void DetectPartials(const WDL_TypedBuf<BL_FLOAT> &magns,
                        const WDL_TypedBuf<BL_FLOAT> &phases,
                        vector<Partial2> *partials);
    
    // Peak frequency computation
    //
    BL_FLOAT ComputePeakIndexAvg(const WDL_TypedBuf<BL_FLOAT> &magns,
                                 int leftIndex, int rightIndex);
    
    BL_FLOAT ComputePeakIndexParabola(const WDL_TypedBuf<BL_FLOAT> &magns,
                                      int peakIndex);
    
    // Peak amp
    //
    BL_FLOAT ComputePeakAmpInterp(const WDL_TypedBuf<BL_FLOAT> &magns,
                                  BL_FLOAT peakFreq);
    
    void ComputePeakMagnPhaseInterp(const WDL_TypedBuf<BL_FLOAT> &magns,
                                    const WDL_TypedBuf<BL_FLOAT> &unwrappedPhases,
                                    BL_FLOAT peakFreq,
                                    BL_FLOAT *peakAmp, BL_FLOAT *peakPhase);
    
    
    // Glue the barbs to the main partial
    // Return true if some barbs have been glued
    bool GluePartialBarbs(const WDL_TypedBuf<BL_FLOAT> &magns,
                          vector<Partial2> *partials);
     
    // Discard partials which are almost flat
    // (compare height of the partial, and width in the middle
    bool DiscardFlatPartial(const WDL_TypedBuf<BL_FLOAT> &magns,
                            int peakIndex, int leftIndex, int rightIndex);
    
    void DiscardFlatPartials(const WDL_TypedBuf<BL_FLOAT> &magns,
                             vector<Partial2> *partials);
    
    // Suppress partials with zero frequencies
    void SuppressZeroFreqPartials(vector<Partial2> *partials);

    BL_FLOAT ComputePeakHeight(const WDL_TypedBuf<BL_FLOAT> &magns,
                               int peakIndex, int leftIndex, int rightIndex);
#if !CODE_CLEAN_MORPHO
    void ThresholdPartialsPeakHeight(const WDL_TypedBuf<BL_FLOAT> &magns,
                                     vector<Partial2> *partials);
#endif
    
    // Peaks
    //
    
    // Fixed
    BL_FLOAT ComputePeakProminence(const WDL_TypedBuf<BL_FLOAT> &magns,
                                   int peakIndex, int leftIndex, int rightIndex);

    BL_FLOAT ComputePeakHigherFoot(const WDL_TypedBuf<BL_FLOAT> &magns,
                                 int leftIndex, int rightIndex);

    BL_FLOAT ComputePeakLowerFoot(const WDL_TypedBuf<BL_FLOAT> &magns,
                                  int leftIndex, int rightIndex);
  
    //    

#if !CODE_CLEAN_MORPHO
    // Adaptive threshold, depending on bin num;
    BL_FLOAT GetThreshold(int binNum);
#endif
    
    // Optim: pre-compute a weights
    void ComputeAWeights(int numBins, BL_FLOAT sampleRate);
        
    // Debug
    // First method
    void DBG_DumpPartials(const char *fileName,
                          const vector<Partial2> &partials,
                          int bufferSize);

    int DenormBinIndex(int idx);

    void PostProcessPartials(const WDL_TypedBuf<BL_FLOAT> &magns,
                             vector<Partial2> *partials);

    void DBG_DumpPeaks(const WDL_TypedBuf<BL_FLOAT> &data,
                       const vector<PeakDetector::Peak> &peaks);

    // Second method
    void DBG_DumpPartials(const WDL_TypedBuf<BL_FLOAT> &magns,
                          const vector<Partial2> &partials);
        
    //
    int mBufferSize;
    BL_FLOAT mSampleRate;
    int mOverlapping;

#if !CODE_CLEAN_MORPHO
    BL_FLOAT mThreshold;
#endif
    
    //
    WDL_TypedBuf<BL_FLOAT> mCurrentMagns;
    WDL_TypedBuf<BL_FLOAT> mCurrentPhases;

    WDL_TypedBuf<BL_FLOAT> mLinearMagns;
    WDL_TypedBuf<BL_FLOAT> mLogMagns;
    
    vector<Partial2> mResult;
    
    //
    BL_FLOAT mMaxDetectFreq;
    
    // For Pre-Process
    BL_FLOAT mTimeSmoothCoeff;
    // Smooth only magns (tried smooth complex, but that was bad)
    WDL_TypedBuf<BL_FLOAT> mPrevMagns;
    // When using complexex...
    WDL_TypedBuf<WDL_FFT_COMPLEX> mPrevComp;
    
    // Scales
    Scale *mScale;
    Scale::Type mXScale;
    Scale::Type mYScale;
    Scale::Type mYScale2; // For log
    
    Scale::Type mXScaleInv;
    Scale::Type mYScaleInv;
    Scale::Type mYScaleInv2; // for log

    // Optim
    WDL_TypedBuf<BL_FLOAT> mAWeights;

    PeakDetector *mPeakDetector;
    PartialFilter2 *mPartialFilter;
    
private:
    // Tmp buffers
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf11;
    
    vector<Partial2> mTmpPartials0;
    vector<Partial2> mTmpPartials1;
    vector<Partial2> mTmpPartials2;
    vector<Partial2> mTmpPartials3;
    vector<Partial2> mTmpPartials4;
    vector<Partial2> mTmpPartials5;
    vector<Partial2> mTmpPartials6;
    vector<Partial2> mTmpPartials7;
    vector<Partial2> mTmpPartials8;
    vector<Partial2> mTmpPartials9;
    vector<Partial2> mTmpPartials10;
    vector<Partial2> mTmpPartials11;
};

#endif /* defined(__BL_SASViewer__PartialTracker8__) */
