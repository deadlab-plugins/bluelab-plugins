//
//  PitchShiftPrusaFftObj.cpp
//  BL-PitchShift
//
//  Created by Pan on 16/04/18.
//
//

#include <algorithm>
using namespace std;

#include <BLUtils.h>
#include <BLUtilsMath.h>
#include <BLUtilsComp.h>
#include <BLUtilsFft.h>

#include <BLDebug.h>

#include "PitchShiftPrusaFftObj.h"

PitchShiftPrusaFftObj::PitchShiftPrusaFftObj(int bufferSize,
                                             int oversampling,
                                             int freqRes,
                                             BL_FLOAT sampleRate)
: ProcessObj(bufferSize)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);
    
    mFactor = 1.0;
}

PitchShiftPrusaFftObj::~PitchShiftPrusaFftObj() {}

void
PitchShiftPrusaFftObj::ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer0,
                                        const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)
{
    WDL_TypedBuf<WDL_FFT_COMPLEX> &ioBuffer = mTmpBuf6;
    BLUtils::TakeHalf(*ioBuffer0, &ioBuffer);
    
    WDL_TypedBuf<BL_FLOAT> &magns = mTmpBuf0;
    WDL_TypedBuf<BL_FLOAT> &phases = mTmpBuf1;
    BLUtilsComp::ComplexToMagnPhase(&magns, &phases, ioBuffer);
    
    //
    // Convert
    //
    Convert(&magns, &phases, mFactor);
        
    BLUtilsComp::MagnPhaseToComplex(&ioBuffer, magns, phases);
    
    BLUtils::SetBuf(ioBuffer0, ioBuffer);
    
    BLUtilsFft::FillSecondFftHalf(ioBuffer0);
}

void
PitchShiftPrusaFftObj::SetFactor(BL_FLOAT factor)
{
    mFactor = factor;
}

void
PitchShiftPrusaFftObj::Reset(int bufferSize, int oversampling,
                             int freqRes, BL_FLOAT sampleRate)
{
    ProcessObj::Reset(bufferSize, oversampling, freqRes, sampleRate);
    
    ResetPitchShift();
}

void
PitchShiftPrusaFftObj::Reset()
{
    //ProcessObj::Reset();
    ProcessObj::Reset(mBufferSize, mOverlapping, mFreqRes, mSampleRate);
    
    ResetPitchShift();
}


void
PitchShiftPrusaFftObj::Convert(WDL_TypedBuf<BL_FLOAT> *magns,
                               WDL_TypedBuf<BL_FLOAT> *phases,
                               BL_FLOAT factor)
{
#define TOL 1e-6
    
    // TODO: optimize with tmp buffers
    
    Frame inputFrame;
    inputFrame.mMagns = *magns;
    inputFrame.mPhases = *phases;

    // Test if we have prev data 
    if (mPrevFrame.mMagns.GetSize() == 0)
    {
        // Update prev data
        mPrevFrame = inputFrame;

        return;
    }

    const Frame &frame0 = mPrevFrame;
    Frame frame1 = inputFrame;
    
    // Pre-processing
    //

    // TODO: use PhasesUnwrapper ?
    
    // Phases time derivative
    WDL_TypedBuf<BL_FLOAT> phasesTimeDeriv;
    phasesTimeDeriv.Resize(magns->GetSize());
    BLUtils::ComputeDiff(&phasesTimeDeriv, frame0.mPhases, frame1.mPhases);

    // Phases freq derivative
    WDL_TypedBuf<BL_FLOAT> phasesFreqDeriv;
    phasesFreqDeriv.Resize(magns->GetSize());
    BLUtils::ComputeDerivative(frame1.mPhases, &phasesFreqDeriv);

    // Compute tolerance
    BL_FLOAT maxMagn0 = BLUtils::ComputeMax(frame0.mMagns);
    BL_FLOAT maxMagn1 = BLUtils::ComputeMax(frame1.mMagns);
    BL_FLOAT maxMagn = MAX(maxMagn0, maxMagn1);

    BL_FLOAT abstol = TOL*maxMagn;
    
    // Set random values for very small magns
    // and insert other values in the heap

    const BL_FLOAT randMaxInv = 1.0/RAND_MAX;
     
    vector<Tuple> tho;
    for (int i = 0; i < frame1.mMagns.GetSize(); i++)
    {
        BL_FLOAT magn1 = frame1.mMagns.Get()[i];
        if (magn1 > abstol)
        {            
            // Add to the heap if greater than threshold            
            Tuple t;
            t.mMagn = magn1;
            t.mBinIdx = i;
            t.mTimeIdx = 1;
            
            tho.push_back(t);
        }
        else
        {
            // Assign random phase value otherwise 
            BL_FLOAT rp = rand()*randMaxInv;
            frame1.mPhases.Get()[i] = rp;
        }
    }

    // Sort tho, for optimization
    sort(tho.begin(), tho.end(), Tuple::IndexSmaller);
         
    vector<Tuple> hp;
    for (int i = 0; i < tho.size(); i++)
    {
        const Tuple &thoT = tho[i];

        Tuple t;
        t.mMagn = frame0.mMagns.Get()[thoT.mBinIdx];
        t.mBinIdx = thoT.mBinIdx;
        t.mTimeIdx = 0;

        hp.push_back(t);
    }
    
    // Create the heap
    make_heap(hp.begin(), hp.end());

    // Test, to try to optimize
    // NOTE: after sort, this is not a heap anymore...
    //sort_heap(hp.begin(), hp.end());
    
    BL_FLOAT a = mFactor*2.0;
    BL_FLOAT b = mFactor*2.0;
    
    // Iterate
    while(!tho.empty())
    {
        pop_heap(hp.begin(), hp.end());
        
        Tuple t = hp.back();
        hp.pop_back();
        
        if (t.mTimeIdx == 0)
        {
            int idx = ContainsSorted(tho, t.mBinIdx, 1);
            if (idx >= 0)
            //if (Contains(tho, t.mBinIdx, 1))
            {
                frame1.mPhases.Get()[t.mBinIdx] =
                    frame0.mPhases.Get()[t.mBinIdx] +
                    a*0.5*(mPrevPhasesTimeDeriv.Get()[t.mBinIdx] +
                           phasesTimeDeriv.Get()[t.mBinIdx]);

                //Remove(&tho, t.mBinIdx, 1);
                RemoveIdx(&tho, idx);
                
                //hp.push_back(t);
                //push_heap(hp.begin(), hp.end());
                
                BL_FLOAT magn = frame1.mMagns.Get()[t.mBinIdx];
                AddHeap(&hp, t.mBinIdx, 1, magn);
            }
        }

        if (t.mTimeIdx == 1)
        {
            int idx0 = ContainsSorted(tho, t.mBinIdx + 1, 1);
            if (idx0 >= 0)
            //if (Contains(tho, t.mBinIdx + 1, 1))
            {
                frame1.mPhases.Get()[t.mBinIdx + 1] =
                    frame1.mPhases.Get()[t.mBinIdx + 1] +
                    b*0.5*(phasesTimeDeriv.Get()[t.mBinIdx] +
                           phasesTimeDeriv.Get()[t.mBinIdx + 1]);

                //Remove(&tho, t.mBinIdx + 1, 1);
                RemoveIdx(&tho, idx0);

                BL_FLOAT magn = frame1.mMagns.Get()[t.mBinIdx + 1];
                AddHeap(&hp, t.mBinIdx + 1, 1, magn);
            }

            int idx1 = ContainsSorted(tho, t.mBinIdx - 1, 1);
            if (idx1 >= 0)
            //if (Contains(tho, t.mBinIdx - 1, 1))
            {
                frame1.mPhases.Get()[t.mBinIdx - 1] =
                    frame1.mPhases.Get()[t.mBinIdx - 1] -
                    b*0.5*(phasesTimeDeriv.Get()[t.mBinIdx] +
                           phasesTimeDeriv.Get()[t.mBinIdx - 1]);

                //Remove(&tho, t.mBinIdx - 1, 1);
                RemoveIdx(&tho, idx1);

                BL_FLOAT magn = frame1.mMagns.Get()[t.mBinIdx - 1];
                AddHeap(&hp, t.mBinIdx - 1, 1, magn);
                
                //hp.push_back(t);
                //push_heap(hp.begin(), hp.end());
            }
        }
    }

    // Result
    *magns = frame1.mMagns;
    *phases = frame1.mPhases;
    
    // Update prev data
    mPrevFrame = inputFrame;

    mPrevPhasesTimeDeriv = phasesTimeDeriv;
}

bool
PitchShiftPrusaFftObj::Contains(const vector<Tuple> &hp, int binIdx, int timeIdx)
{
    // TODO: optimize this!
    for (int i = 0; i < hp.size(); i++)
    {
        const Tuple &t = hp[i];
        if ((t.mBinIdx == binIdx) && (t.mTimeIdx == timeIdx))
            return true;
    }

    return false;
}

// NOTE: lower_bound() doesn't like const vectors
int
PitchShiftPrusaFftObj::ContainsSorted(/*const*/ vector<Tuple> &hp,
                                      int binIdx, int timeIdx)
{
    Tuple t;
    t.mBinIdx = binIdx;
    t.mTimeIdx = timeIdx;
    
    vector<Tuple>::iterator it =
        lower_bound(hp.begin(), hp.end(), t, Tuple::IndexSmaller);
    if ((it != hp.end()) && Tuple::IndexEqual(*it, t))
    {
        //int idx = it - hp.begin(); // Should be correct also
        int idx = distance(hp.begin(), it); // More normalized

        return idx;
    }
 
    return -1;
}

void
PitchShiftPrusaFftObj::Remove(vector<Tuple> *hp, int binIdx, int timeIdx)
{
    int idx = -1;
    for (int i = 0; i < hp->size(); i++)
    {
        const Tuple &t = (*hp)[i];
        if ((t.mBinIdx == binIdx) && (t.mTimeIdx == timeIdx))
        {
            idx = i;
            break;
        }
    }

    if (idx >= 0)
        hp->erase(hp->begin() + idx);
}

void
PitchShiftPrusaFftObj::RemoveIdx(vector<Tuple> *hp, int idx)
{
    if (idx >= 0)
        hp->erase(hp->begin() + idx);
}

void
PitchShiftPrusaFftObj::AddHeap(vector<Tuple> *hp, int binIdx,
                               int timeIdx, BL_FLOAT magn)
{
    Tuple t;
    t.mMagn = magn;
    t.mBinIdx = binIdx;
    t.mTimeIdx = timeIdx;

    hp->push_back(t);
    push_heap(hp->begin(), hp->end());
}

void
PitchShiftPrusaFftObj::DBG_Dump(const char *fileName, const vector<Tuple> &hp)
{
    WDL_TypedBuf<BL_FLOAT> magns;
    magns.Resize(hp.size());

    for (int i = 0; i < hp.size(); i++)
    {
        magns.Get()[i] = hp[i].mMagn;
    }

    BLDebug::DumpData(fileName, magns);
}

void
PitchShiftPrusaFftObj::ResetPitchShift()
{
    // TODO
}
