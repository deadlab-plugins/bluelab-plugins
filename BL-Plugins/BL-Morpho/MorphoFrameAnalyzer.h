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
 
#ifndef MORPHO_FRAME_ANALYZER_H
#define MORPHO_FRAME_ANALYZER_H

#include <vector>
using namespace std;

#include <BLTypes.h>

#include <MorphoFrame7.h>

class FftProcessObj16;
class MorphoFrameAnalyzerFftObj;
class PartialTracker8; // TMP
class MorphoFrameAnalyzer
{
 public:
    MorphoFrameAnalyzer(BL_FLOAT sampleRate, bool storeDetectDataInFrames);
    virtual ~MorphoFrameAnalyzer();

    void Reset(BL_FLOAT sampleRate);
    
    void ProcessBlock(const vector<WDL_TypedBuf<BL_FLOAT> > &in);

    void GetMorphoFrames(vector<MorphoFrame7> *frames);

    // Parameters
    void SetDetectThreshold(BL_FLOAT detectThrs);
    void SetFreqThreshold(BL_FLOAT freqThrs);
    void SetTimeSmoothCoeff(BL_FLOAT coeff);
    
 protected:
    FftProcessObj16 *mFftObj;

    MorphoFrameAnalyzerFftObj *mFrameAnalyzerObj;

private:
    vector<WDL_TypedBuf<BL_FLOAT> > mTmpBuf0;
};

#endif
