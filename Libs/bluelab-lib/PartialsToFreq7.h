//
//  PartialsToFreq7.h
//  BL-SASViewer
//
//  Created by applematuer on 2/25/19.
//
//

#ifndef __BL_SASViewer__PartialsToFreq7__
#define __BL_SASViewer__PartialsToFreq7__

#include <vector>
using namespace std;

#include <Partial.h>

// PartialsToFreq: original code, moved from SASFrame to here
// PartialsToFreq2: improved algorithm
//
// PartialsToFreq3: custom algo, use freq diffs and sigma
//
// PartialsToFreq4: keep only the structure and moved the freq diff algo
// to a new class
// (will be useful if need filtering frequency)
//
// PartialsToFreq6: compute freq from chroma, and adjust to the right octave,
// depending on the partials freqs
//
// VERY GOOD! (this is a real improvement over the standardly used algorithm!)
// => the freq from chroma is really more smooth than the tracked partial data
// => and it represents the pitch rellay more accurately than partial tracking
class ChromagramObj;
class PartialsToFreq7
{
public:
    PartialsToFreq7(int bufferSize, int oversampling,
                    int freqRes, BL_FLOAT sampleRate);
    
    virtual ~PartialsToFreq7();

    void Reset(int bufferSize, int oversampling,
               int freqRes, BL_FLOAT sampleRate);
    
    void SetHarmonicSoundFlag(bool flag);
    
    BL_FLOAT ComputeFrequency(const WDL_TypedBuf<BL_FLOAT> &magns,
                              const WDL_TypedBuf<BL_FLOAT> &phases,
                              const vector<Partial> &partials);
    
protected:
    BL_FLOAT AdjustFreqToPartial(BL_FLOAT freq,
                                 const vector<Partial> &partials);

    BL_FLOAT
    AdjustFreqToPartialOctave(BL_FLOAT freq,
                              const vector<Partial> &partials);
    
    void ThresholdPartials(vector<Partial> *partials);
    void ThresholdPartialsRelative(vector<Partial> *partials);

    // From computed chroma freq, choose the best partial freq
    BL_FLOAT FindClosestPartialFreq(BL_FLOAT inFreq,
                                    const vector<Partial> &partials);

    // Keep the chroma freq, but adjust to the right octave,
    // depending on the partials freqs
    //
    // VERY GOOD!
    // => the freq from chroma is really more smooth than the tracked partial data
    // => and it represents the pitch rellay more accurately than partial tracking
    BL_FLOAT FindBestOctave(BL_FLOAT inFreq,
                            const vector<Partial> &partials);

    // Keep the chroma freq, but adjust to the right octave,
    // depending on roughtly detected partials (on the fly)
    //
    // VERY GOOD!
    // => the freq from chroma is really more smooth than the tracked partial data
    // => and it represents the pitch rellay more accurately than partial tracking
    BL_FLOAT FindBestOctave2(BL_FLOAT inFreq,
                             const WDL_TypedBuf<BL_FLOAT> &magns);
    
    //
    int mBufferSize;
    BL_FLOAT mSampleRate;

    ChromagramObj *mChromaObj;

private:
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0;
};

#endif /* defined(__BL_SASViewer__PartialsToFreq7__) */
