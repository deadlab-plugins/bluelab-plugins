//
//  RebalanceProcessor.hpp
//  BL-Rebalance-macOS
//
//  Created by applematuer on 10/14/20.
//
//

#ifndef RebalanceProcessor_hpp
#define RebalanceProcessor_hpp

#include <BLTypes.h>
#include <ResampProcessObj.h>
#include <Rebalance_defs.h>


class FftProcessObj16;
class RebalanceDumpFftObj2;
class RebalanceProcessFftObjComp3;
class RebalanceMaskPredictorComp6;

class RebalanceProcessor : public ResampProcessObj
{
public:
    RebalanceProcessor(BL_FLOAT sampleRate, BL_FLOAT targetSampleRate,
                       int bufferSize, int targetBufferSize,
                       int overlapping,
                       int numSpectroCols);
    
    virtual ~RebalanceProcessor();
    
    void InitDetect(const IPluginBase &plug);
    
    void InitDump();
    
    // Must be called at least once
    void Reset(BL_FLOAT sampleRate);
    void Reset(BL_FLOAT sampleRate, int blockSize);
    
    int GetLatency();
    
    // Predict
    //
    void SetVocal(BL_FLOAT vocal);
    void SetBass(BL_FLOAT bass);
    void SetDrums(BL_FLOAT drums);
    void SetOther(BL_FLOAT other);
    
    void SetMasksContrast(BL_FLOAT contrast);
    
    void SetVocalSensitivity(BL_FLOAT vocalSensitivity);
    void SetBassSensitivity(BL_FLOAT bassSensitivity);
    void SetDrumsSensitivity(BL_FLOAT drumsSensitivity);
    void SetOtherSensitivity(BL_FLOAT otherSensitivity);
    
    // Dump
    //
    bool HasEnoughDumpData();
    void GetDumpData(WDL_TypedBuf<BL_FLOAT> data[REBALANCE_NUM_SPECTRO_COLS]);
    
protected:
    bool ProcessSamplesBuffers(vector<WDL_TypedBuf<BL_FLOAT> > *ioBuffers,
                               vector<WDL_TypedBuf<BL_FLOAT> > *ioResampBuffers) override;
    
    //
    int mBufferSize;
    int mTargetBufferSize;
    int mOverlapping;
    
    int mBlockSize;
    
    int mNumSpectroCols;
    
    FftProcessObj16 *mNativeFftObj;
    FftProcessObj16 *mTargetFftObj;
    
    RebalanceDumpFftObj2 *mDumpObj;
    
    RebalanceMaskPredictorComp6 *mMaskPred;
    
    RebalanceProcessFftObjComp3 *mDetectProcessObjs[2];
};

#endif /* RebalanceProcessor_hpp */
