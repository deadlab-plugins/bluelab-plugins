//
//  SpectrogramView2.cpp
//  BL-Ghost
//
//  Created by Pan on 02/06/18.
//
//
#ifdef IGRAPHICS_NANOVG

#include <BLSpectrogram4.h>
#include <FftProcessObj16.h>

#include <SamplesToMagnPhases.h>

#include <BLUtils.h>

#include "SpectrogramView2.h"

#define MIN_ZOOM 1.0
//#define MAX_ZOOM 20.0
#define MAX_ZOOM 800.0 // Big zoom

// NOTE: We can see some "wobbling" due to the windowing
// in the case of step > 1 (whatever the min overlap)
#define MIN_OVERLAP 1

SpectrogramView2::SpectrogramView2(BLSpectrogram4 *spectro,
                                   FftProcessObj16 *fftObj,
                                   SpectroEditFftObj3 *spectroEditObjs[2],
                                   int maxNumCols,
                                   BL_FLOAT x0, BL_FLOAT y0,
                                   BL_FLOAT x1, BL_FLOAT y1,
                                   BL_FLOAT sampleRate)
{
    mSpectrogram = spectro;
    
    mFftObj = fftObj;
    
    mMaxNumCols = maxNumCols;
    
    mViewBarPos = 0.5;
    
    mStartDataPos = 0.0;
    mEndDataPos = 0.0;
    
    // Warning: y is reversed
    mBounds[0] = x0;
    mBounds[1] = y0;
    mBounds[2] = x1;
    mBounds[3] = y1;
    
    mSampleRate = sampleRate;
    
    mSamples = NULL;

    // For the moment, use only the left channel
    SpectroEditFftObj3 *spectroEditObjs0[2] = { spectroEditObjs[0], NULL };
    mSamplesToMagnPhases = new SamplesToMagnPhases(NULL, // samples
                                                   fftObj, spectroEditObjs0,
                                                   NULL); // pyramid
    
    Reset();
}

SpectrogramView2::~SpectrogramView2()
{
    delete mSamplesToMagnPhases;
}

void
SpectrogramView2::Reset()
{
    mZoomFactor = 1.0;
    
    mAbsZoomFactor = 1.0;
    
    mTranslation = 0.0;

    mZoomAdjustFactor = 1.0;
    mZoomAdjustOffset = 0.0;
}

BLSpectrogram4 *
SpectrogramView2::GetSpectrogram()
{
    return mSpectrogram;
}

void
SpectrogramView2::SetData(vector<WDL_TypedBuf<BL_FLOAT> > *samples)
{
    mSamples = samples;
    
    mSamplesToMagnPhases->SetSamples(mSamples);
    
    mStartDataPos = 0.0;
    mEndDataPos = 0.0;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    if (!mSamples->empty())
    {
        long numSamples = (*mSamples)[0].GetSize();
        mEndDataPos = ((BL_FLOAT)numSamples)/bufferSize;
    }

    ComputeZoomAdjustFactor(0.0, 1.0);
}

void
SpectrogramView2::SetViewBarPosition(BL_FLOAT pos)
{
    if ((mSamples == NULL) || mSamples->empty())
        return;
    
    mViewBarPos = pos;
}

void
SpectrogramView2::SetViewSelection(BL_FLOAT x0, BL_FLOAT y0,
                                   BL_FLOAT x1, BL_FLOAT y1)
{
    mSelection[0] = (x0 - mBounds[0])/(mBounds[2] - mBounds[0]);
    mSelection[1] = (y0 - mBounds[1])/(mBounds[3] - mBounds[1]);
    mSelection[2] = (x1 - mBounds[0])/(mBounds[2] - mBounds[0]);
    mSelection[3] = (y1 - mBounds[1])/(mBounds[3] - mBounds[1]);
    
    // Hack
    // Avoid out of bounds selection(due to graph + miniview)
    // TODO: manage coordinates better
#if 1
    if (mSelection[1] < 0.0)
        mSelection[1] = 0.0;
    if (mSelection[3] > 1.0)
        mSelection[3] = 1.0;
#endif
    
    mSelectionActive = true;
}

void
SpectrogramView2::GetViewSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                   BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = mSelection[0]*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y0 = mSelection[1]*(mBounds[3] - mBounds[1]) + mBounds[1];
    *x1 = mSelection[2]*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y1 = mSelection[3]*(mBounds[3] - mBounds[1]) + mBounds[1];
    
    //
    mSelectionActive = true;
}

// Not checked !
// Should do (1.0 - mBounds[3]) for offset, asjust below
void
SpectrogramView2::ViewToDataRef(BL_FLOAT *x0, BL_FLOAT *y0,
                                BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = (*x0 - mBounds[0])/(mBounds[2] - mBounds[0]);
    *y0 = (*y0 - mBounds[1])/(mBounds[3] - mBounds[1]);
    *x1 = (*x1 - mBounds[0])/(mBounds[2] - mBounds[0]);
    *y1 = (*y1 - mBounds[1])/(mBounds[3] - mBounds[1]);
}

// GOOD !
void
SpectrogramView2::DataToViewRef(BL_FLOAT *x0, BL_FLOAT *y0,
                                BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = *x0*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y0 = *y0*(mBounds[3] - mBounds[1]) + 1.0 - mBounds[3];
    *x1 = *x1*(mBounds[2] - mBounds[0]) + mBounds[0];
    *y1 = *y1*(mBounds[3] - mBounds[1]) + 1.0 - mBounds[3];
}

void
SpectrogramView2::ClearViewSelection()
{
    mSelectionActive = false;
}

void
SpectrogramView2::GetViewDataBounds(BL_FLOAT *startDataPos, BL_FLOAT *endDataPos,
                                    BL_FLOAT minNormX, BL_FLOAT maxXNorm)
{
    if ((mSamples == NULL) || mSamples->empty())
    {
        *startDataPos = 0;
        *endDataPos = 0;
        
        return;
    }
    
    long numSamples = (*mSamples)[0].GetSize();
    if (numSamples == 0)
    {
        *startDataPos = 0;
        *endDataPos = 0;
        
        return;
    }
    
    int bufferSize = mFftObj->GetBufferSize();
    
    *startDataPos = minNormX*((BL_FLOAT)numSamples)/bufferSize;
    *endDataPos = maxXNorm*((BL_FLOAT)numSamples)/bufferSize;;
}

bool
SpectrogramView2::GetDataSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                   BL_FLOAT *x1, BL_FLOAT *y1)
{
    if (!mSelectionActive)
        return false;
    
    if ((mSamples == NULL) || mSamples->empty())
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    *x0 = mStartDataPos + mSelection[0]*(mEndDataPos - mStartDataPos);
    
    // Warning, y is reversed !
    *y0 = (1.0 - mSelection[3])*bufferSize/2.0;
    
    *x1 = mStartDataPos + mSelection[2]*(mEndDataPos - mStartDataPos);
    
    // Warning, y is reversed !
    *y1 = (1.0 - mSelection[1])*bufferSize/2.0;
    
    return true;
}

bool
SpectrogramView2::GetDataSelection2(BL_FLOAT *x0, BL_FLOAT *y0,
                                    BL_FLOAT *x1, BL_FLOAT *y1,
                                    BL_FLOAT minNormX, BL_FLOAT maxXNorm)
{
    if (!mSelectionActive)
        return false;
    
    if ((mSamples == NULL) || mSamples->empty())
        return false;
    
    long numSamples = (*mSamples)[0].GetSize();
    if (numSamples == 0)
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    BL_FLOAT startDataPos = minNormX*((BL_FLOAT)numSamples)/bufferSize;

    BL_FLOAT endDataPos = maxXNorm*((BL_FLOAT)numSamples)/bufferSize;
    
    *x0 = startDataPos + mSelection[0]*(endDataPos - startDataPos);
    
    // Warning, y is reversed !
    *y0 = (1.0 - mSelection[3])*bufferSize/2.0;
    
    *x1 = startDataPos + mSelection[2]*(endDataPos - startDataPos);
    
    // Warning, y is reversed !
    *y1 = (1.0 - mSelection[1])*bufferSize/2.0;
    
    return true;
}

void
SpectrogramView2::SetDataSelection2(BL_FLOAT x0, BL_FLOAT y0,
                                    BL_FLOAT x1, BL_FLOAT y1,
                                    BL_FLOAT minNormX, BL_FLOAT maxXNorm)
{
    if ((mSamples == NULL) || mSamples->empty())
        return;
    
    long numSamples = (*mSamples)[0].GetSize();
    if (numSamples == 0)
        return;
    
    int bufferSize = mFftObj->GetBufferSize();
    int overlapping = mFftObj->GetOverlapping();
    
    BL_FLOAT startDataPos = minNormX*((BL_FLOAT)numSamples)/bufferSize;

    BL_FLOAT endDataPos = maxXNorm*((BL_FLOAT)numSamples)/bufferSize;
    
    mSelection[0] = (x0 - startDataPos)/(endDataPos - startDataPos);
    
    // Warning, y is reversed !
    mSelection[3] = 1.0 - y0/(bufferSize/2.0);
    
    mSelection[2] = (x1 - startDataPos)/(endDataPos - startDataPos);
    
    // Warning, y is reversed !
    mSelection[1] = 1.0 -y1/(bufferSize/2.0);
}

bool
SpectrogramView2::GetNormDataSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                       BL_FLOAT *x1, BL_FLOAT *y1)
{    
    if ((mSamples == NULL) || mSamples->empty())
        return false;
    
    bool selectionActive = mSelectionActive;
    
    // To force getting the result
    mSelectionActive = true;
    
    BL_FLOAT dataSelection[4];
    bool res = GetDataSelection(&dataSelection[0], &dataSelection[1],
                                &dataSelection[2], &dataSelection[3]);
    
    mSelectionActive = selectionActive;
    
    if (!res)
        return false;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    int numSamples = (*mSamples)[0].GetSize();
    *x0 = ((BL_FLOAT)dataSelection[0]*bufferSize)/numSamples;
    *y0 = ((BL_FLOAT)dataSelection[1])/(bufferSize/2.0);

    *x1 = ((BL_FLOAT)dataSelection[2]*bufferSize)/numSamples;
    *y1 = ((BL_FLOAT)dataSelection[3])/(bufferSize/2.0);
    
    return true;
}

bool
SpectrogramView2::UpdateZoomFactor(BL_FLOAT zoomChange)
{
    if ((mSamples == NULL) || mSamples->empty())
        return false;
    
    // Avoid zooming too much
    BL_FLOAT prevAbsZoomFactor = mAbsZoomFactor;
    mAbsZoomFactor *= zoomChange;
    
    long numSamples = (*mSamples)[0].GetSize();
    BL_FLOAT maxZoom = MAX_ZOOM*((BL_FLOAT)numSamples)/100000.0;
    
    if ((mAbsZoomFactor < MIN_ZOOM) || (mAbsZoomFactor > maxZoom))
    {
        mAbsZoomFactor = prevAbsZoomFactor;
        
        return false;
    }
    
    // Else the zoom is reasonable, set it
    mZoomFactor *= zoomChange;
    
    return true;
}

BL_FLOAT
SpectrogramView2::GetZoomFactor()
{
    return mZoomFactor;
}

BL_FLOAT
SpectrogramView2::GetAbsZoomFactor()
{
    return mAbsZoomFactor;
}

void
SpectrogramView2::GetZoomAdjust(BL_FLOAT *zoom, BL_FLOAT *offset)
{
    *zoom = mZoomAdjustFactor;
    *offset = mZoomAdjustOffset;
}

BL_FLOAT
SpectrogramView2::GetNormZoom()
{
    if ((mSamples == NULL) || mSamples->empty())
        return 0.0;
    
    long numSamples = (*mSamples)[0].GetSize();
    BL_FLOAT maxZoom = MAX_ZOOM*((BL_FLOAT)numSamples)/100000.0;

    BL_FLOAT result = (mAbsZoomFactor - MIN_ZOOM)/(maxZoom - MIN_ZOOM);
    
    return result;
}

void
SpectrogramView2::Translate(BL_FLOAT tX)
{
    mTranslation += tX;
}

BL_FLOAT
SpectrogramView2::GetTranslation()
{
    return mTranslation;
}

void
SpectrogramView2::UpdateSpectrogramData(BL_FLOAT minXNorm, BL_FLOAT maxXNorm)
{
    // Test for input
    if (mSpectrogram == NULL)
        return;

    // Save for step    
    int prevOverlap = mFftObj->GetOverlapping();
    
    BL_FLOAT sampleRate = mSpectrogram->GetSampleRate();
    mSpectrogram->Reset(sampleRate);

    if ((mSamples == NULL) || mSamples->empty())
        return;
    
    long numSamples = (*mSamples)[0].GetSize();
    if (numSamples == 0)
        return;
    
    int bufferSize = mFftObj->GetBufferSize();
    
    // Recompute data pos
    mStartDataPos = ((BL_FLOAT)minXNorm*numSamples)/bufferSize;
    mEndDataPos = ((BL_FLOAT)maxXNorm*numSamples)/bufferSize;
            
    // Update step.
    // Usefull to avoid recomputing all the data when the file is long
    BL_FLOAT step = UpdateStep();
    
    // Compute magns and phases
    vector<WDL_TypedBuf<BL_FLOAT> > magns[2];
    vector<WDL_TypedBuf<BL_FLOAT> > phases[2];
    mSamplesToMagnPhases->ReadSpectroDataSlice(magns, phases, minXNorm, maxXNorm);

    //DBG_AnnotateMagns(&magns[0]);
    
    // Update spectrogram
    mSpectrogram->SetLines(magns[0], phases[0]);

    // Initial zoom
    mZoomFactor = 1.0;

    ComputeZoomAdjustFactor(minXNorm, maxXNorm, step);

    // Restore for step
    mFftObj->SetOverlapping(prevOverlap);
    mSamplesToMagnPhases->SetStep(1.0);
}

void
SpectrogramView2::SetSampleRate(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
}

void
SpectrogramView2::DBG_AnnotateMagns(vector<WDL_TypedBuf<BL_FLOAT> > *ioMagns)
{
#define NUM_SCALES 16
#define SIZE_COEFF 0.75
    
    for (int i = 0; i < ioMagns->size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &magns = (*ioMagns)[i];

        int mod = i % NUM_SCALES;
        int size = magns.GetSize()*(1.0/NUM_SCALES)*mod*SIZE_COEFF;

        for (int j = magns.GetSize() - size - 1; j < magns.GetSize(); j++)
            magns.Get()[j] = mod*(1.0/NUM_SCALES);
    }
}

void
SpectrogramView2::ComputeZoomAdjustFactor(BL_FLOAT minXNorm, BL_FLOAT maxXNorm,
                                          BL_FLOAT step)
{
    // If step > 1, it means that we would get much data (> 1024 lines)
    // => Do not bother to adjust the zoom factor, simply ignore it
    if (step > 1.0)
    {
        mZoomAdjustFactor = 1.0;
        mZoomAdjustOffset = 0.0;
        
        return;
    }
    
    int dataSize = mSpectrogram->GetNumCols();
    
    if (mSamples->empty() || (dataSize == 0))
    {
        mZoomAdjustFactor = 1.0;
        mZoomAdjustOffset = 0.0;
        
        return;
    }
        
    long numSamples = (*mSamples)[0].GetSize();
    int bufferSize = mFftObj->GetBufferSize();
        
    // Compute zoom adjust factor,
    // so that the spectrogram will be axactly aligned to the waveform
    int overlapping = mFftObj->GetOverlapping();
    
    BL_FLOAT selSize = ((maxXNorm - minXNorm)*numSamples)/(bufferSize/overlapping);
    
    mZoomAdjustFactor = dataSize/selSize;
    mZoomAdjustOffset = ((BL_FLOAT)overlapping)/selSize;
}

BL_FLOAT
SpectrogramView2::UpdateStep()
{
    int overlap = mFftObj->GetOverlapping();
    BL_FLOAT viewNumLines = mEndDataPos - mStartDataPos;
    viewNumLines *= overlap;
    if (viewNumLines < 1.0)
        viewNumLines = 1.0;

    // Be sure we never go over mMaxNumCols
    viewNumLines = ceil(viewNumLines) + 1;
    
    // Step. If it is > 1, this means that we would get too much data
    BL_FLOAT step = ((BL_FLOAT)viewNumLines)/mMaxNumCols;
    
    if (step > 1.0)
    // We would get too much data
    {
        // First, try to reduce the overlap and check if it is sufficient
        int overlap0 = overlap;
        while(overlap0 > MIN_OVERLAP)
        {
            overlap0 /= 2;
            step /= 2;

            // Be sure we never go over mMaxNumCols
            viewNumLines = step*mMaxNumCols;
            viewNumLines = ceil(viewNumLines) + 1;
            step = ((BL_FLOAT)viewNumLines)/mMaxNumCols;
            
            if (step <= 1.0)
                break;
        }
        
        // Set overlap
        mFftObj->SetOverlapping(overlap0);
        
        // Set step if we need to reduce
        if (step > 1.0)
        {
            mSamplesToMagnPhases->SetStep(step);
        }

        *newOverlap = overlap0;
    }
    
    return step;
}

#endif
