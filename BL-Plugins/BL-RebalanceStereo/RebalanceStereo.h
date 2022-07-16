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
 
#ifndef __REBALANCE_STEREO__
#define __REBALANCE_STEREO__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <Rebalance_defs.h>

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtilsPlug.h>

// Use resampler for sample rates > 48000Hz
// (which makes bad detection, and scrolling too fast if sr >= 88200Hz
//
#if !APP_API
#define USE_RESAMPLER 1
#else
#define USE_RESAMPLER 0
#endif

#if USE_RESAMPLER
#include "../../WDL/resample.h"
#endif

// Rebalance is designed to work with sr=44100Hz
#define REF_SAMLERATE0 44100.0
// But with sr=48000Hz, results are good too (acceptable)
#define REF_SAMLERATE1 48000.0

// It now avoids clicks, except if we turn the knob very quickly
#define USE_BASS_FOCUS_SMOOTHER 1 //0

using namespace iplug;
using namespace igraphics;

// See: "Deep Karaoke: Extracting Vocals from Musical Mixtures
//       Using a Convolutional Deep Neural Network"
// https://www.researchgate.net/publication/275279991_Deep_Karaoke_Extracting_Vocals_from_Musical_Mixtures_Using_a_Convolutional_Deep_Neural_Network


// When compiled as standalone application, the plugin is used to
// dump audio data and binary masks
// When compiled as plugin, it is used to process the sound and rebalance it

class RebalanceProcessor2;
class GUIHelper12;
class GraphControl12;
class GraphTimeAxis6;
class GraphFreqAxis2;
class GraphAxis2;
class SpectrogramDisplayScroll4;
class ParamSmoother2;
class BLTransport;
// RebalanceStereo
class PseudoStereoObj2;
class ParamSmoother2;
class CrossoverSplitterNBands4;
class RebalanceStereo final : public Plugin
{
 public:
    RebalanceStereo(const InstanceInfo &info);
    virtual ~RebalanceStereo();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;

    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);
    
    void CreateControls(IGraphics *pGraphics);

    void InitSoloMuteButtonsParams();
    void CreateSoloMuteButtons(IGraphics *pGraphics);
    
    void DoReset();
    
    void InitNull();
    void InitParams();
    void Init();
    void ApplyParams();
	
    void InitPlug(BL_FLOAT sampleRate);
  
    void UpdateLatency();

    void CreateSpectrogramDisplay(bool createFromInit);
    void CreateGraphAxes();

    void UpdateTimeAxis();
    
    void SetScrollSpeed();
    void SetColorMap();

    void UpdateSoloMute();

    BL_FLOAT ComputeMixParam(BL_FLOAT valuePercent);

    void CheckRecomputeSpectrogram(bool recomputeMasks = false);
    void CheckRecomputeSpectrogramIdle();

    void SetModelNum(int modelNum);

    // RebalanceStereo
    void SetBassFocusFreq(BL_FLOAT freq);
        
    //
    GraphControl12 *mGraph;

    BLSpectrogram4 *mSpectrogram;
    SpectrogramDisplayScroll4 *mSpectrogramDisplay;
    SpectrogramDisplayScroll4::SpectrogramDisplayScrollState *
        mSpectroDisplayScrollState;
 
    bool mNeedRecomputeData;
    bool mMustUpdateSpectrogram;
    
    GraphAxis2 *mHAxis;
    GraphTimeAxis6 *mTimeAxis;
    bool mMustUpdateTimeAxis;
    
    GraphAxis2 *mVAxis;
    GraphFreqAxis2 *mFreqAxis;

    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;
    
    BL_FLOAT mPrevSampleRate;
    bool mWasPlaying;
        
    RebalanceProcessor2 *mPredictProcessor;
    // Stems + mix
    RebalanceProcessor2 *mDumpProcessors[NUM_STEM_SOURCES + 1];
    
    // Parameters
  
    // Mix
    BL_FLOAT mVocal;
    BL_FLOAT mBass;
    BL_FLOAT mDrums;
    BL_FLOAT mOther;
  
    // Precision
    BL_FLOAT mVocalSensitivity;
    BL_FLOAT mBassSensitivity;
    BL_FLOAT mDrumsSensitivity;
    BL_FLOAT mOtherSensitivity;
    
    // Masks contrasts, relative one to each other (previous soft to hard)
    BL_FLOAT mMasksContrast;
    
    // Files data
    WDL_TypedBuf<BL_FLOAT> mCurrentMixChannels[2];
    WDL_TypedBuf<BL_FLOAT> mCurrentSourceChannels0[NUM_STEM_SOURCES];
    
    // For updating latency
    bool mQualityChanged;
    
    DemoModeManager mDemoManager;
  
    // Secure starters
    SecureRestarter mSecureRestarter;
  
    // Don't dump at each fft step
    int mDumpCount;
    
    bool mUIOpened;
    bool mControlsCreated;
    
    bool mIsInitialized;
    IGraphics *mGraphics;

    GUIHelper12 *mGUIHelper;

    bool mSolos[4];
    bool mMutes[4];

    BLTransport *mTransport;

    bool mIsPlaying;
    
    // For debugging
    BL_FLOAT mRange;
    BL_FLOAT mContrast;

    bool mHardSolo;

    bool mNeedRecomputeSpectrogram;
    long int mPrevSpectroUpdateTime;
    bool mNeedRecomputeMasks;

    int mModelNum;

#if USE_RESAMPLER
    void CheckSampleRate(BL_FLOAT *ioSampleRate);
    
    // Set to true if sample rate is > 48000Hz
    bool mUseResampler;
    
    WDL_Resampler mResamplersIn[2];
    WDL_Resampler mResamplersOut[2];
#endif

    BLUtilsPlug mBLUtilsPlug;

    // RebalanceStereo
    bool mMonoToStereo;
    BL_FLOAT mBassFocusFreq;

    BL_FLOAT mWidthVocal;
    BL_FLOAT mWidthBass;
    BL_FLOAT mWidthDrums;
    BL_FLOAT mWidthOther;

    BL_FLOAT mPanVocal;
    BL_FLOAT mPanBass;
    BL_FLOAT mPanDrums;
    BL_FLOAT mPanOther;

    CrossoverSplitterNBands4 *mBassFocusBandSplitters[2];
    
#if USE_BASS_FOCUS_SMOOTHER
    ParamSmoother2 *mBassFocusSmoother;
#endif
    
    PseudoStereoObj2 *mPseudoStereoObj;

    ParamSmoother2 *mStereoWidthSmoothers[4];
    ParamSmoother2 *mPanSmoothers[4];
    
 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf5;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf6;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf7;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11[2];
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf12;
    
    BL_PROFILE_DECLARE;
};

#endif
