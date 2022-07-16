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
 
#ifndef __IMPULSES__
#define __IMPULSES__

#include "IPlug_include_in_plug_hdr.h"
#include "IGraphics_include_in_plug_hdr.h"

#include <BLProfiler.h>

#include <SecureRestarter.h>
#include <DemoModeManager.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#define AXIS_SHIFT_ZERO 1
#if !AXIS_SHIFT_ZERO
#include "Configs.h"
#else
#include "Configs2.h"
#endif

#include "config.h"
#include "bl_config.h"

using namespace iplug;
using namespace igraphics;


class FastRTConvolver3;

class DiracGenerator;
class ImpulseResponseSet;
class ImpulseResponseExtractor;

class GraphTimeAxis6;
class GraphAxis2;
class GraphCurve5;

class BLBitmap;

class ParamSmoother2;

// New version: re-create the graph each time we close/open the windows, or resize gui
// and keep the data in other objects, not in the graph.
class Impulses final : public Plugin
{
 public:
    // Actions for Capture mode
    enum Action
    {
        GEN_IMPULSES = 0,
        PROCESS_RESPONSES,
        APPLY_RESPONSE,
        RESET
    };
  
    Impulses(const InstanceInfo &info);
    virtual ~Impulses();

    void OnHostIdentified() override;
    
    void OnReset() override;
    void OnParamChange(int paramIdx) override;
    
    void OnUIOpen() override;
    void OnUIClose() override;
    
    void ProcessBlock(iplug::sample **inputs,
                      iplug::sample **outputs, int nFrames) override;

    // Used to update from the GUI thread
    void OnIdle() override;
    
 protected:
    IGraphics *MyMakeGraphics();
    void MyMakeLayout(IGraphics *pGraphics);

    void CreateControls(IGraphics *pGraphics);
    
    void InitNull();
    void InitParams();
    void ApplyParams();
    
    void Init();
    
    // Time axis
    void CreateGraphAxes();
    void InitTimeAxis();

    void UpdateTimeAxis();
    
    void CreateGraphCurves();
    
    //
    bool SerializeState(IByteChunk &pChunk) const override;
    int UnserializeState(const IByteChunk &pChunk, int startPos) override;
  
    void DoReset(bool clearNativeImpulses);

    // Take native IRs, and create mImpulses
    // (resample, fade etc.), and set them to the convolvers
    void ProcessNativeImpulses();
    
    void ClearGraphInstantResponse();
    void ClearGraphAvgResponse();
    
    void UpdateGraphInstantResponse(const WDL_TypedBuf<BL_FLOAT> &
                                    instantImpulseResponse);
    void UpdateGraphAvgResponse(const WDL_TypedBuf<BL_FLOAT> &avgImpulseResponse);
    
    // Decimate the input, then add to the graph
    // This is for use with the full data
    void UpdateGraphAvgResponseDecim(const WDL_TypedBuf<BL_FLOAT> &
                                     avgImpulseResponse);
    
    void UpdateGraphInstantResponseValid(bool validFlag);
    
    void UpdateInputSignalCurve(const WDL_TypedBuf<BL_FLOAT> &signal);
    
    
    // Set to true if called from unserialization
    void ActionChanged(Action prevAction);
    
    void ResponseLengthChanged();
    
    BL_FLOAT ComputeTimeLimit();
  
    long GetRespNumSamples();
    
    long GetNumGraphValues();
    
    enum ResponseLength
    {
        LENGTH_50MS = 0,
        LENGTH_100MS = 1,
        LENGTH_1S = 2,
        LENGTH_5S = 3,
        LENGTH_10S = 4
    };
    
    void SetConfig(ResponseLength length);
    void ApplyConfig();
    
    void ProcessImpulseResponse();
    
    void OpenImageFile(const char *fileName);
    
    void ResetImageFile();
    
    void OpenIRFile(const char *fileName);
    
    void SaveIRFile(const char *fileName);
    
    void ResampleImpulse(WDL_TypedBuf<BL_FLOAT> *impulse);
    
    // Unused ?
    void GetAvgImpulseResponse(WDL_TypedBuf<BL_FLOAT> *result,
                               const WDL_TypedBuf<BL_FLOAT> impulseResponses[2]);
    
    void GetMaxImpulseResponse(WDL_TypedBuf<BL_FLOAT> *result,
                               const WDL_TypedBuf<BL_FLOAT> impulseResponses[2]);
    
    // For AXIS_SHIFT_ZERO
    void SetGraphIR(GraphCurve5 *curve,
                    const WDL_TypedBuf<BL_FLOAT> &ir);

    BLBitmap *ProcessLoadedBgBmp(const BLBitmap *bmp);
    void UpdateGraphBgImage();

    void GrayOutGainKnobs();
        
    //
    GraphControl12 *mGraph;
    
    // Secure starters
    SecureRestarter mSecureRestarter;
    
    GUIHelper12 *mGUIHelper;

    // Axes
    GraphAxis2 *mHAxis;
    bool mMustUpdateTimeAxis;
    
    GraphAxis2 *mVAxis;

    // Curves
    GraphCurve5 *mInputSignalCurve;
    GraphCurve5 *mInstantResponseCurve;
    GraphCurve5 *mAvgResponseCurve;

    DemoModeManager mDemoManager;
    
    bool mUIOpened;
    bool mControlsCreated;
    
    bool mIsInitialized;
    
    //
    // Dirac generators and captured responses
    DiracGenerator *mDiracGens[2];
    
    ImpulseResponseExtractor *mRespExtractors[2];
    ImpulseResponseExtractor *mRespExtractorsDecimated;
    
    ImpulseResponseSet *mResponseSets[2];
    
    // We keep a decimated version of all the data
    // To be more efficient for displaying and for temporary processing
    // during capture
    // Otherwise, it seemed that when sending and entiere long (1s or 10s) impulse
    // response to the graph, it was too costly
    // And same thing when coputing the avg and discarding bad responses
    // during capture
    //
    // NOTE: the perf problem after 3 dirac might have come from another point:
    // The discard process was computed at each new nFrames
    // (not sure at all if it was right...)
    ImpulseResponseSet *mResponseSetsDecimated;
    
    // Convolver and current responses to apply
    FastRTConvolver3 *mConvolvers[2];
    
    // Impulses to be set to the convolvers
    WDL_TypedBuf<BL_FLOAT> mImpulses[2];
    
    // Loaded or computed impulses, with full size
    //
    // Won't be touched later, so we will load and save
    // IRs while keeping them as original
    WDL_TypedBuf<BL_FLOAT> mNativeImpulses[2];
    BL_FLOAT mNativeSampleRate;
    
    // For bounce mode
    long mBounceSampleIndex;
    
    int mGraphWidthPixels;
    
    // Action
    Action mAction;
    
    // Response Length
    ResponseLength mResponseLength;
    
    BL_FLOAT mMinXAxisValue;
    BL_FLOAT mMaxXAxisValue;
    BL_FLOAT mDiracFrequency;
    char *(*mTimeAxisData)[NUM_AXIS_DATA][2];
    
    int mConvBufferSize;
    
    bool mResponseNeedsProcess;
    
    bool mProcessingStarted;
    
    // Input gain
    ParamSmoother2 *mInGainSmoother;
    BL_FLOAT mInGain;
    IControl *mInGainKnob;
    
    // Output gain
    ParamSmoother2 *mOutGainSmoother;
    BL_FLOAT mOutGain;
    IControl *mOutGainKnob;
    
    // Background image
    BLBitmap *mBgImageBitmap;
    bool mMustUpdateBGImage;
    IGraphics *mGraphics;
        
    // Detect sample rate change, for resampling responses
    BL_FLOAT mPrevSampleRate;
  
    // Output gain coeff: to compensate when IR sample rate is high
    BL_FLOAT mGainCoeff;

    char mCurrentLoadPathIR[FILENAME_SIZE];
    char mCurrentSavePathIR[FILENAME_SIZE];
    char mCurrentFileNameIR[FILENAME_SIZE];
    char mCurrentLoadPathImg[FILENAME_SIZE];

    // For FIX_FILE_SELECTOR_REOPEN
    IControl *mImageOpenControl;
    IControl *mIROpenControl;
    IControl *mIRSaveControl;

    BLUtilsPlug mBLUtilsPlug;

    // For FIX_FILE_SELECTOR_FREEZE_WINDOWS
    bool mIsPromptingForFile;

 private:
    // Tmp buffers
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf9;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11;
    
    BL_PROFILE_DECLARE;
};

#endif
