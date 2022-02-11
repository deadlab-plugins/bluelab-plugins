//
//  GhostViewerFftObjSubSonic.h
//  BL-GhostViewer
//
//  Created by Pan on 02/06/18.
//
//

#ifndef __BL_GhostViewer__GhostViewerFftObjSubSonic__
#define __BL_GhostViewer__GhostViewerFftObjSubSonic__

#include "FftProcessObj16.h"

// From ChromaFftObj
//

// Disable for debugging
#define USE_SPECTRO_SCROLL 1

class BLSpectrogram3;
class SpectrogramDisplayScroll;

class GhostViewerFftObjSubSonic : public ProcessObj
{
public:
    GhostViewerFftObjSubSonic(int bufferSize, int oversampling, int freqRes,
                      BL_FLOAT sampleRate);
    
    virtual ~GhostViewerFftObjSubSonic();
    
    void ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                          const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer = NULL);
    
    void Reset(int bufferSize, int oversampling, int freqRes, BL_FLOAT sampleRate);
    
    BLSpectrogram3 *GetSpectrogram();
    
#if USE_SPECTRO_SCROLL
    void SetSpectrogramDisplay(SpectrogramDisplayScroll *spectroDisplay);
#else
    void SetSpectrogramDisplay(SpectrogramDisplay *spectroDisplay);
#endif
    
    BL_FLOAT GetMaxFreq();
    
    void SetSpeed(BL_FLOAT mSpeed);
    
protected:
    void AddSpectrogramLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                            const WDL_TypedBuf<BL_FLOAT> &phases);
    
    void SelectSubSonic(WDL_TypedBuf<BL_FLOAT> *magns,
                        WDL_TypedBuf<BL_FLOAT> *phases);

    int ComputeLastBin(BL_FLOAT freq);
    
    BLSpectrogram3 *mSpectrogram;
    
#if USE_SPECTRO_SCROLL
    SpectrogramDisplayScroll *mSpectroDisplay;
#else
    SpectrogramDisplay *mSpectroDisplay;
#endif
    
    long mLineCount;
    
    deque<WDL_TypedBuf<BL_FLOAT> > mOverlapLines;
    
    BL_FLOAT mSpeed;
};

#endif /* defined(__BL_GhostViewer__GhostViewerFftObjSubSonic__) */
