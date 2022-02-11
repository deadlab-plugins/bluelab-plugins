//
//  StereoWidthProcess.h
//  BL-PitchShift
//
//  Created by Pan on 20/04/18.
//
//

#ifndef __BL_PitchShift__StereoWidthProcess__
#define __BL_PitchShift__StereoWidthProcess__

#include <SmoothAvgHistogram.h>
#include <CMA2Smoother.h>

#include "FftProcessObj16.h"

#define DEBUG_FREQS_DISPLAY 0

// Debug drawer: display circles, azymuth
#define DEBUG_DRAWER_DISPLAY 0
#define DEBUG_DRAWER_DISPLAY_FREQ 3229.98046875 //129.19921875

class DebugDrawer;

class StereoWidthProcess : public MultichannelProcess
{
public:
    enum DisplayMode
    {
        SIMPLE = 0,
        SOURCE,
        SCANNER
    };
    
    StereoWidthProcess(int bufferSize,
                       BL_FLOAT overlapping, BL_FLOAT oversampling,
                       BL_FLOAT sampleRate);
    
    virtual ~StereoWidthProcess();
    
    void Reset();
    
    void Reset(int overlapping, int oversampling, BL_FLOAT sampleRate);
    
    void SetWidthChange(BL_FLOAT widthChange);
    
    void SetFakeStereo(bool flag);
    
    void ProcessInputFft(vector<WDL_TypedBuf<WDL_FFT_COMPLEX> * > *ioFftSamples);
    
    // Get the result
    void GetWidthValues(WDL_TypedBuf<BL_FLOAT> *xValues,
                        WDL_TypedBuf<BL_FLOAT> *yValues,
                        WDL_TypedBuf<BL_FLOAT> *outColorWeights);
    
    void SetDisplayMode(enum DisplayMode mode);
    
protected:
    // We have to change the magns too
    // ... because if we decrease at the maximum, the two magns channels
    // must be similar
    // (otherwise it vibrates !)
    void ApplyWidthChange(WDL_TypedBuf<BL_FLOAT> *ioPhasesL,
                          WDL_TypedBuf<BL_FLOAT> *ioPhasesR,
                          WDL_TypedBuf<BL_FLOAT> *ioMagnsL,
                          WDL_TypedBuf<BL_FLOAT> *ioMagnsR,
                          WDL_TypedBuf<BL_FLOAT> *outSourceRs,
                          WDL_TypedBuf<BL_FLOAT> *outSourceThetas);
    
    void Stereoize(WDL_TypedBuf<BL_FLOAT> *ioPhasesL,
                   WDL_TypedBuf<BL_FLOAT> *ioPhasesR);
    
    void GenerateRandomCoeffs(int size);

    void MagnPhasesToSourcePos(WDL_TypedBuf<BL_FLOAT> *outSourceRs,
                               WDL_TypedBuf<BL_FLOAT> *outSourceThetas,
                               const WDL_TypedBuf<BL_FLOAT> &magnsL,
                               const WDL_TypedBuf<BL_FLOAT> &magnsR,
                               const WDL_TypedBuf<BL_FLOAT> &phasesL,
                               const WDL_TypedBuf<BL_FLOAT> &phasesR,
                               const WDL_TypedBuf<BL_FLOAT> &freqs,
                               const WDL_TypedBuf<BL_FLOAT> &timeDelays);

    void SourcePosToMagnPhases(const WDL_TypedBuf<BL_FLOAT> &sourceRs,
                               const WDL_TypedBuf<BL_FLOAT> &sourceThetas,
                               BL_FLOAT micsDistance,
                               BL_FLOAT widthFactor,
                               WDL_TypedBuf<BL_FLOAT> *ioMagnsL,
                               WDL_TypedBuf<BL_FLOAT> *ioMagnsR,
                               WDL_TypedBuf<BL_FLOAT> *ioPhasesL,
                               WDL_TypedBuf<BL_FLOAT> *ioPhasesR,
                               const WDL_TypedBuf<BL_FLOAT> &freqs,
                               const WDL_TypedBuf<BL_FLOAT> &timeDelays);
    
    // Return true is the distance an agle have been actually computed
    bool MagnPhasesToSourcePos(BL_FLOAT *sourceR, BL_FLOAT *sourceTheta,
                               BL_FLOAT magnsL, BL_FLOAT magnsR,
                               BL_FLOAT phasesL, BL_FLOAT phasesR,
                               BL_FLOAT freq,
                               BL_FLOAT timeDelay);
    
    // d is the distance between microphones
    bool SourcePosToMagnPhases(BL_FLOAT sourceR, BL_FLOAT sourceTheta,
                               BL_FLOAT d,
                               BL_FLOAT *ioMagnL, BL_FLOAT *ioMagnR,
                               BL_FLOAT *ioPhaseL, BL_FLOAT *ioPhaseR,
                               BL_FLOAT freq,
                               BL_FLOAT timeDelay);
    
    // Uses second order equation
    // Well checked for identity transform !
    bool SourcePosToMagnPhases2(BL_FLOAT sourceR, BL_FLOAT sourceTheta,
                                BL_FLOAT d,
                                BL_FLOAT *ioMagnL, BL_FLOAT *ioMagnR,
                                BL_FLOAT *ioPhaseL, BL_FLOAT *ioPhaseR,
                                BL_FLOAT freq,
                                BL_FLOAT timeDelay);

    
    BL_FLOAT ComputeFactor(BL_FLOAT normVal, BL_FLOAT maxVal);

    // Unused ?
    void ComputeTimeDelays(WDL_TypedBuf<BL_FLOAT> *timeDelays,
                           const WDL_TypedBuf<BL_FLOAT> &phasesL,
                           const WDL_TypedBuf<BL_FLOAT> &phasesR);
    
    void ModifyPhase(BL_FLOAT *phaseL, BL_FLOAT *phaseR, BL_FLOAT factor);

    void TemporalSmoothing(WDL_TypedBuf<BL_FLOAT> *sourceRs,
                           WDL_TypedBuf<BL_FLOAT> *sourceThetas);

    void SpatialSmoothing(WDL_TypedBuf<BL_FLOAT> *sourceRs,
                          WDL_TypedBuf<BL_FLOAT> *sourceThetas);
    
    void NormalizeRs(WDL_TypedBuf<BL_FLOAT> *sourceRs, bool discardClip);
    
    void ComputeColorWeightsMagns(WDL_TypedBuf<BL_FLOAT> *outWeights,
                                  const WDL_TypedBuf<BL_FLOAT> &magnsL,
                                  const WDL_TypedBuf<BL_FLOAT> &magnsR);
    
    void ComputeColorWeightsFreqs(WDL_TypedBuf<BL_FLOAT> *outWeights,
                                  const WDL_TypedBuf<BL_FLOAT> &freqs);
    
    static int SecondOrderEqSolve(BL_FLOAT a, BL_FLOAT b, BL_FLOAT c, BL_FLOAT res[2]);

    
    int mBufferSize;
    BL_FLOAT mOverlapping;
    BL_FLOAT mOversampling;
    BL_FLOAT mSampleRate;
    
    BL_FLOAT mWidthChange;
    
    bool mFakeStereo;
    
    WDL_TypedBuf<BL_FLOAT> mWidthValuesX;
    WDL_TypedBuf<BL_FLOAT> mWidthValuesY;
    WDL_TypedBuf<BL_FLOAT> mColorWeights;
    
    WDL_TypedBuf<BL_FLOAT> mRandomCoeffs;
    
    // For smoothing (to improve the display)
    SmoothAvgHistogram *mSourceRsHisto;
    SmoothAvgHistogram *mSourceThetasHisto;
    
#if DEBUG_DRAWER_DISPLAY
    DebugDrawer *mDebugDrawer;
#endif
    
    enum DisplayMode mDisplayMode;
};

#endif /* defined(__BL_PitchShift__StereoPhasesProcess__) */
