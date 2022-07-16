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
 
#ifndef MORPHO_FRAME_SYNTHETIZER_FFT_OBJ
#define MORPHO_FRAME_SYNTHETIZER_FFT_OBJ

#include <FftProcessObj16.h>
#include <MorphoFrameSynth2.h>

#include <BLTypes.h>

class MorphoFrame7;
class MorphoFrameSynth2;
class MorphoFrameSynthetizerFftObj : public ProcessObj
{
 public:
    MorphoFrameSynthetizerFftObj(int bufferSize,
                              BL_FLOAT overlapping, BL_FLOAT oversampling,
                              BL_FLOAT sampleRate);
    
    virtual ~MorphoFrameSynthetizerFftObj();
    
    void Reset(BL_FLOAT sampleRate);

    void ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                          const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer) override;
    
    // Use this to synthetize directly samples from partials
    void ProcessSamplesPost(WDL_TypedBuf<BL_FLOAT> *ioBuffer) override;
    
    void AddMorphoFrame(const MorphoFrame7 &frame);

    // Parameters
    void SetSynthMode(MorphoFrameSynth2::SynthMode mode);
    void SetSynthEvenPartials(bool flag);
    void SetSynthOddPartials(bool flag);
    
    void SetHarmoNoiseMix(BL_FLOAT mix);
    
 protected:
    MorphoFrameSynth2 *mMorphoFrameSynth;

private:
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf0;
    WDL_TypedBuf<WDL_FFT_COMPLEX> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
};

#endif
