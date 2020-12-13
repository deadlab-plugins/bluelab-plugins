//
//  BL_BatFftObj2.h
//  BL-Ghost
//
//  Created by Pan on 02/06/18.
//
//

#ifndef __BL_Pano__BatFftObj2__
#define __BL_Pano__BatFftObj2__

#ifdef IGRAPHICS_NANOVG

#include "FftProcessObj16.h"

// From ChromaFftObj
//

class BLSpectrogram4;
class SpectrogramDisplay;
class HistoMaskLine2;

class SourceLocalisationSystem2;

class BatFftObj2 : public MultichannelProcess
{
public:
    BatFftObj2(int bufferSize, int oversampling, int freqRes,
                   BL_FLOAT sampleRate);
    
    virtual ~BatFftObj2();
    
    void ProcessInputFft(vector<WDL_TypedBuf<WDL_FFT_COMPLEX> * > *ioFftSamples,
                         const vector<WDL_TypedBuf<WDL_FFT_COMPLEX> > *scBuffer);
    
    void Reset(int bufferSize, int oversampling, int freqRes, BL_FLOAT sampleRate);
    
    BLSpectrogram4 *GetSpectrogram();
    
    void SetSpectrogramDisplay(SpectrogramDisplay *spectroDisplay);
    
    void SetSharpness(BL_FLOAT sharpness);
    
    void SetTimeSmooth(BL_FLOAT smooth);
    
    void SetFftSmooth(BL_FLOAT smooth);
    
    void SetEnabled(bool flag);
    
protected:
    void ComputeCoords(const WDL_TypedBuf<BL_FLOAT> &localization,
                       WDL_TypedBuf<BL_FLOAT> *coords);

    void ComputeAmplitudes(const WDL_TypedBuf<BL_FLOAT> &localization,
                           WDL_TypedBuf<BL_FLOAT> *amps);

    //
    void ApplySharpnessXY(vector<WDL_TypedBuf<BL_FLOAT> > *lines);
    void ReverseXY(vector<WDL_TypedBuf<BL_FLOAT> > *lines);
    
    void ApplySharpness(vector<WDL_TypedBuf<BL_FLOAT> > *lines);
    
    void ApplySharpness(WDL_TypedBuf<BL_FLOAT> *line);
    
    void TimeSmooth(vector<WDL_TypedBuf<BL_FLOAT> > *lines);

    
    BLSpectrogram4 *mSpectrogram;
    
    SpectrogramDisplay *mSpectroDisplay;
    
    long mLineCount;
    
    // Sharpness
    WDL_TypedBuf<BL_FLOAT> mSmoothWin;
    
    BL_FLOAT mSharpness;
    
    BL_FLOAT mTimeSmooth;
    vector<WDL_TypedBuf<BL_FLOAT> > mPrevLines;
    
    BL_FLOAT mFftSmooth;
    WDL_TypedBuf<BL_FLOAT> mPrevMagns[2];
    WDL_TypedBuf<BL_FLOAT> mPrevScMagns[2];
    
    SourceLocalisationSystem2 *mSourceLocSystem;
    
    //
    int mBufferSize;
    int mOverlapping;
    int mFreqRes;
    int mSampleRate;
    
    // For FIX_BAD_DISPLAY_HIGH_SAMPLERATES
    int mAddLineCount;
    
    bool mIsEnabled;
};

#endif

#endif /* defined(__BL_BL_Pano__BL_BatFftObj2__) */
