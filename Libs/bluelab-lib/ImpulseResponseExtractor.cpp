#include <BLUtils.h>

#include <ImpulseResponseSet.h>

#include "ImpulseResponseExtractor.h"

ImpulseResponseExtractor::ImpulseResponseExtractor(long responseSize,
                                                   BL_FLOAT sampleRate,
                                                   BL_FLOAT decimationFactor)
{
    mResponseSize = responseSize;
    mSampleRate = sampleRate;
    mDecimationFactor = decimationFactor;
}

ImpulseResponseExtractor::~ImpulseResponseExtractor() {}

void
ImpulseResponseExtractor::Reset(long responseSize, BL_FLOAT sampleRate,
                                BL_FLOAT decimationFactor)
{
    mResponseSize = responseSize;
    mSampleRate = sampleRate;
    mDecimationFactor = decimationFactor;
  
    mSamples.Resize(0);
}

void
ImpulseResponseExtractor::SetSampleRate(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
}

BL_FLOAT
ImpulseResponseExtractor::GetDecimationFactor()
{
    return mDecimationFactor;
}

// New method, which adds and extract
void
ImpulseResponseExtractor::AddSamples(const BL_FLOAT *samples, int nFrames,
                                     WDL_TypedBuf<BL_FLOAT> *outResponse)
{
    if (outResponse != NULL)
        outResponse->Resize(0);
  
    WDL_TypedBuf<BL_FLOAT> samp;
    samp.Add(samples, nFrames);
  
    AddWithDecimation(samp, mDecimationFactor, &mSamples);
  
    if (mSamples.GetSize() >= mResponseSize)
        // Wait that the buffer contains enough samples
        // to be sure to extract one response
    {
        long peakIndex = DetectImpulseResponsePeak(mSamples);
    
        if (peakIndex >= 0)
            // A peak was found !
        {
            long respSize = mResponseSize;
      
#if !ALIGN_IR_ABSOLUTE
            // Shift on the left to have the beginning of the response
            long startIndex = peakIndex - LEFT_IMPULSES_MARGIN*respSize;
#else
            long startIndex = peakIndex - ALIGN_IR_TIME*mSampleRate;
#endif
      
            if (startIndex < 0)
                startIndex = 0;
    
            // Check if we have enought samples to extract the response
            if (mSamples.GetSize() - startIndex > mResponseSize)
                // Ok, we have enough samples
            {
                if (outResponse != NULL)
                    outResponse->Add(&mSamples.Get()[startIndex], mResponseSize);
      
                BLUtils::ConsumeLeft(&mSamples, startIndex + mResponseSize);
            }
        }
    }
}

void
ImpulseResponseExtractor::AddSamples(const BL_FLOAT *samples, int nFrames)
{
    if (mDecimationFactor > 0.0)
    {
        WDL_TypedBuf<BL_FLOAT> samp;
        samp.Add(samples, nFrames);
        AddWithDecimation(samp, mDecimationFactor, &mSamples);
    }
}

long
ImpulseResponseExtractor::DetectResponseStartIndex()
{
    long startIndex = -1;
  
    if (mSamples.GetSize() >= mResponseSize)
        // Wait that the buffer contains enough samples
        // to be sure to extract one response
    {
        long peakIndex = DetectImpulseResponsePeak(mSamples);
    
        if (peakIndex >= 0)
            // A peak was found !
        {
            long respSize = mResponseSize;
      
#if !ALIGN_IR_ABSOLUTE
            // Shift on the left to have the beginning of the response
            startIndex = peakIndex - LEFT_IMPULSES_MARGIN*respSize;
#else
            startIndex = peakIndex - ALIGN_IR_TIME*mSampleRate;
#endif
      
            if (startIndex < 0)
                startIndex = 0;
        }
    }
  
    return startIndex;
}

void
ImpulseResponseExtractor::ExtractResponse(long startIndex, WDL_TypedBuf<BL_FLOAT> *outResponse)
{
    // Check if we have enought samples to extract the response
    if (mSamples.GetSize() - startIndex > mResponseSize)
        // Ok, we have enough samples
    {
        if (outResponse != NULL)
            outResponse->Add(&mSamples.Get()[startIndex], mResponseSize);
        
        BLUtils::ConsumeLeft(&mSamples, startIndex + mResponseSize);
    }
}

// Find an index common to the two extractors
// so they will stay correctly shifted relatively 
void
ImpulseResponseExtractor::AddSamples2(ImpulseResponseExtractor *ext0,
                                      ImpulseResponseExtractor *ext1,
                                      const BL_FLOAT *samples0,
                                      const BL_FLOAT *samples1,
                                      int nFrames,
                                      WDL_TypedBuf<BL_FLOAT> *outResponse0,
                                      WDL_TypedBuf<BL_FLOAT> *outResponse1)
{
    if (ext1 == NULL)
    {
        ext0->AddSamples(samples0, nFrames, outResponse0);
    
        return;
    }
  
    ext0->AddSamples(samples0, nFrames);
    ext1->AddSamples(samples1, nFrames);
  
    long index0 = ext0->DetectResponseStartIndex();
    long index1 = ext1->DetectResponseStartIndex();
  
    if ((index0 < 0) && (index1 < 0))
        // Nothing found
        return;
  
    if (index0 < 0)
        index0 = index1; 
    if (index1 < 0)
        index1 = index0;
  
    BL_FLOAT startIndex = MIN(index0, index1);
  
    ext0->ExtractResponse(startIndex, outResponse0);
    ext1->ExtractResponse(startIndex, outResponse1);
}

void
ImpulseResponseExtractor::AddWithDecimation(const WDL_TypedBuf<BL_FLOAT> &samples,
                                            BL_FLOAT decFactor,
                                            WDL_TypedBuf<BL_FLOAT> *outSamples)
{
    if (decFactor > 0.0)
    {
        WDL_TypedBuf<BL_FLOAT> decimSamples;
    
        // In Utils, decimation factor is inverted
        BLUtils::DecimateSamples(&decimSamples, samples, 1.0/decFactor);
    
        outSamples->Add(decimSamples.Get(), decimSamples.GetSize());
    }
}


long
ImpulseResponseExtractor::DetectImpulseResponsePeak(const WDL_TypedBuf<BL_FLOAT> &samples)
{
#define PEAK_FACTOR 10.0
#define PEAK_THRESHOLD 0.2
  
    // Find the max, and cut
    long maxIndex = ImpulseResponseSet::FindMaxValIndex(samples);
    BL_FLOAT maxValue = samples.Get()[maxIndex];
    maxValue = fabs(maxValue);
  
    // Too low signal
    if (maxValue < PEAK_THRESHOLD)
        return -1;
  
    BL_FLOAT avg = BLUtils::ComputeAvg(samples);
  
    if (maxValue > avg*PEAK_FACTOR)
        // Found
        return maxIndex;
  
    // Not found
    return -1;
}
