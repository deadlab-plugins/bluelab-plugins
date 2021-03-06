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
 
//
//  SpectrogramView2.h
//  BL-Ghost
//
//  Created by Pan on 02/06/18.
//
//

#ifndef __BL_Ghost__SpectrogramView2__
#define __BL_Ghost__SpectrogramView2__

#ifdef IGRAPHICS_NANOVG

#include <vector>
using namespace std;

#include "IPlug_include_in_plug_hdr.h"

class BLSpectrogram4;
class FftProcessObj16;
class SamplesToMagnPhases;
class SpectroEditFftObj3;
class SpectrogramView2
{
public:
    SpectrogramView2(BLSpectrogram4 *spectro,
                     FftProcessObj16 *fftObj,
                     SpectroEditFftObj3 *spectroEditObjs[2],
                     int maxNumCols,
                     BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                     BL_FLOAT sampleRate);
    
    virtual ~SpectrogramView2();
    
    void Reset();

    BLSpectrogram4 *GetSpectrogram();

    // Multi channel samples
    // (possibilty an entiere stereo file)
    void SetSamples(vector<WDL_TypedBuf<BL_FLOAT> > *samples);
    
    void SetViewBarPosition(BL_FLOAT pos);
    
    void SetViewSelection(BL_FLOAT x0, BL_FLOAT y0,
                          BL_FLOAT x1, BL_FLOAT y1);
    
    void GetViewSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                          BL_FLOAT *x1, BL_FLOAT *y1);
    
    void ViewToDataRef(BL_FLOAT *x0, BL_FLOAT *y0,
                       BL_FLOAT *x1, BL_FLOAT *y1);

    
    void DataToViewRef(BL_FLOAT *x0, BL_FLOAT *y0,
                       BL_FLOAT *x1, BL_FLOAT *y1);

    
    void ClearViewSelection();
    
    // startDataPos and endDataPos are BL_FLOAT,
    // because we get the data bounds for spectrogram columns
    //
    // If we want it in samples, we must mult by BUFFER_SIZE
    // And to get precise sample data pos, we use BL_FLOAT here
    void GetViewDataBounds(BL_FLOAT *startDataPos, BL_FLOAT *endDataPos,
                           BL_FLOAT minNormX, BL_FLOAT maxNormX);
    
    bool GetDataSelection(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1);
    
    // Get the data selection, before the data has been recomputed
    bool GetDataSelection2(BL_FLOAT *x0, BL_FLOAT *y0, BL_FLOAT *x1, BL_FLOAT *y1,
                           BL_FLOAT minNormX, BL_FLOAT maxNormX);
    
    void SetDataSelection2(BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
                           BL_FLOAT minNormX, BL_FLOAT maxNormX);
    
    bool GetNormDataSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                              BL_FLOAT *x1, BL_FLOAT *y1);
    
    // Return true if the zoom had actually be done
    // (otherwise, we can have reached the zoom limits)
    bool UpdateZoomFactor(BL_FLOAT zoomChange);
    BL_FLOAT GetZoomFactor();
    BL_FLOAT GetAbsZoomFactor();
    // To make the spectrogram exactly aligned to waveform
    void GetZoomAdjust(BL_FLOAT *zoom, BL_FLOAT *offset);
    
    // Between 0 and 1 => for zoom between min and max
    BL_FLOAT GetNormZoom();
    
    void Translate(BL_FLOAT tX);
    
    BL_FLOAT GetTranslation();
    
    // Pass the spectrogram separately, so we can pass the BG spctrogram
    // and full range, to regenereate it
    void UpdateSpectrogramData(BL_FLOAT minNormX, BL_FLOAT maxNormX,
                               BLSpectrogram4 *spectrogram);
    
    void SetSampleRate(BL_FLOAT sampleRate);
    
protected:
    void ComputeZoomAdjustFactor(BL_FLOAT minXNorm, BL_FLOAT maxXNorm,
                                 BL_FLOAT step = 1.0);

    // Do not compute all the lines if the file is long
    // Compute and set the step, and return it
    BL_FLOAT UpdateStep();
    
    void DBG_AnnotateMagns(vector<WDL_TypedBuf<BL_FLOAT> > *ioMagns);
        
    BLSpectrogram4 *mSpectrogram;
    int mMaxNumCols;
    
    FftProcessObj16 *mFftObj;
    
    // Use a pointer, to avoid duplicating the file data,
    // to save memory on big files
    vector<WDL_TypedBuf<BL_FLOAT> > *mSamples;
    
    // Use BL_FLOAT ! => avoid small translations due to rounging
    BL_FLOAT mViewBarPos;
    BL_FLOAT mStartDataPos;
    BL_FLOAT mEndDataPos;
    
    BL_FLOAT mZoomFactor;
    
    // Total zoom factor
    BL_FLOAT mAbsZoomFactor;
    
    // Total translation
    BL_FLOAT mTranslation;
    
    // View selection
    bool mSelectionActive;
    BL_FLOAT mSelection[4];
    
    // Bounds
    BL_FLOAT mBounds[4];
    
    // Just for resampling
    BL_FLOAT mSampleRate;

    SamplesToMagnPhases *mSamplesToMagnPhases;

    // To make the spectrogram exactly aligned to waveform
    BL_FLOAT mZoomAdjustFactor;
    BL_FLOAT mZoomAdjustOffset;
};

#endif

#endif /* defined(__BL_Ghost__SpectrogramView2__) */
