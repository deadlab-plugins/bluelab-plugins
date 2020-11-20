//
//  StereoVizProcess3.cpp
//  BL-PitchShift
//
//  Created by Pan on 20/04/18.
//
//

#ifdef IGRAPHICS_NANOVG

#include <BLUtils.h>
#include <BLDebug.h>
#include <DebugGraph.h>

#include <SASViewerRender2.h>

#include <PartialTracker3.h>
#include <SASFrame3.h>

#include "SASViewerProcess2.h"

#define DEBUG_MUTE_NOISE 0 //1
#define DEBUG_MUTE_PARTIALS 0 //1

#define SHOW_ONLY_ALIVE 0 //1
#define MIN_AGE_DISPLAY 0 //10 // 4

SASViewerProcess2::SASViewerProcess2(int bufferSize,
                                     BL_FLOAT overlapping, BL_FLOAT oversampling,
                                     BL_FLOAT sampleRate)
: ProcessObj(bufferSize)
{
    mBufferSize = bufferSize;
    mOverlapping = overlapping;
    mOversampling = oversampling;
    
    mSampleRate = sampleRate;
    
    mSASViewerRender = NULL;
    
    mPartialTracker = new PartialTracker5(bufferSize, sampleRate, overlapping);
    
    BL_FLOAT minAmpDB = mPartialTracker->GetMinAmpDB();
    
    mSASFrame = new SASFrame3(bufferSize, sampleRate, overlapping);
    mSASFrame->SetMinAmpDB(minAmpDB);
    
    mThreshold = -60.0;
    mHarmonicFlag = false;
    
    mMode = TRACKING;
    
    mEnableOutHarmo = true;
    mEnableOutNoise = true;
    
    // For additional lines
    mAddNum = 0;
    
    mShowTrackingLines = true;
}

SASViewerProcess2::~SASViewerProcess2()
{
    delete mPartialTracker;
    delete mSASFrame;
}

void
SASViewerProcess2::Reset()
{
    Reset(mOverlapping, mOversampling, mSampleRate);
    
    mSASFrame->Reset(mSampleRate);
}

void
SASViewerProcess2::Reset(int overlapping, int oversampling,
                         BL_FLOAT sampleRate)
{
    mOverlapping = overlapping;
    mOversampling = oversampling;
    
    mSampleRate = sampleRate;
    
    mSASFrame->Reset(sampleRate);
}

void
SASViewerProcess2::ProcessFftBuffer(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer,
                                    const WDL_TypedBuf<WDL_FFT_COMPLEX> *scBuffer)

{
    WDL_TypedBuf<WDL_FFT_COMPLEX> fftSamples = *ioBuffer;
    
    // Take half of the complexes
    BLUtils::TakeHalf(&fftSamples);
    
    WDL_TypedBuf<BL_FLOAT> magns;
    WDL_TypedBuf<BL_FLOAT> phases;
    BLUtils::ComplexToMagnPhase(&magns, &phases, fftSamples);
    
    mCurrentMagns = magns;
    
    //
    DetectPartials(magns, phases);
    
    //
    if (mPartialTracker != NULL)
        mPartialTracker->GetPreProcessedData(&mCurrentMagns);
    
    if (mPartialTracker != NULL)
    {
        vector<PartialTracker5::Partial> normPartials;
        mPartialTracker->GetPartials(&normPartials);
        
        mCurrentNormPartials = normPartials;
        
        // Avoid sending garbage partials to the SASFrame
        // (would slow down a lot when many garbage partial
        // are used to compute the SASFrame)
        //PartialTracker3::RemoveRealDeadPartials(&partials);
        
        vector<PartialTracker5::Partial> partials = normPartials;
        mPartialTracker->DenormPartials(&partials);
        
        mSASFrame->SetPartials(partials);
        
        if (mEnableOutNoise)
            mPartialTracker->GetNoiseEnvelope(&magns);
        
        mSASFrame->SetNoiseEnvelope(magns);
        
        Display();
    }
    
    // For noise envelope
    BLUtils::MagnPhaseToComplex(ioBuffer, magns, phases);
    ioBuffer->Resize(ioBuffer->GetSize()*2);
    BLUtils::FillSecondFftHalf(ioBuffer);
}

void
SASViewerProcess2::ProcessSamplesBuffer(WDL_TypedBuf<BL_FLOAT> *ioBuffer,
                                       WDL_TypedBuf<BL_FLOAT> *scBuffer)
{
    if (!mSASFrame->ComputeSamplesFlag())
        return;
    
#if DEBUG_MUTE_NOISE
    // For the moment, empty the io buffer
    BLUtils::FillAllZero(ioBuffer);
#endif
    
#if !DEBUG_MUTE_PARTIALS
    // Create a separate buffer for samples synthesis from partials
    // (because it doesn't use overlap)
    WDL_TypedBuf<BL_FLOAT> samplesBuffer;
    BLUtils::ResizeFillZeros(&samplesBuffer, ioBuffer->GetSize());
    
    // Compute the samples from partials
    mSASFrame->ComputeSamples(&samplesBuffer);
#endif
}

// Use this to synthetize directly 1/4 of the samples from partials
// (without overlap in internal)
void
SASViewerProcess2::ProcessSamplesBufferWin(WDL_TypedBuf<BL_FLOAT> *ioBuffer,
                                          const WDL_TypedBuf<BL_FLOAT> *scBuffer)
{
    if (!mSASFrame->ComputeSamplesWinFlag())
        return;
    
#if DEBUG_MUTE_NOISE
    // For the moment, empty the io buffer
    BLUtils::FillAllZero(ioBuffer);
#endif
    
#if !DEBUG_MUTE_PARTIALS
    // Create a separate buffer for samples synthesis from partials
    // (because it doesn't use overlap)
    WDL_TypedBuf<BL_FLOAT> samplesBuffer;
    BLUtils::ResizeFillZeros(&samplesBuffer, ioBuffer->GetSize());
    
    // Compute the samples from partials
    mSASFrame->ComputeSamplesWin(&samplesBuffer);
#endif
}

void
SASViewerProcess2::SetSASViewerRender(SASViewerRender2 *sasViewerRender)
{
    mSASViewerRender = sasViewerRender;
}

void
SASViewerProcess2::SetMode(Mode mode)
{
    if (mSASViewerRender != NULL)
        mSASViewerRender->SetMode(mode);
}

void
SASViewerProcess2::SetShowTrackingLines(bool flag)
{
    mShowTrackingLines = flag;
    
    if (mSASViewerRender != NULL)
        mSASViewerRender->ShowTrackingLines(TRACKING, flag);
}

void
SASViewerProcess2::SetThreshold(BL_FLOAT threshold)
{
    mThreshold = threshold;
    mPartialTracker->SetThreshold(threshold);
}

void
SASViewerProcess2::SetPitch(BL_FLOAT pitch)
{
    mSASFrame->SetPitch(pitch);
}

void
SASViewerProcess2::SetHarmonicSoundFlag(bool flag)
{
    mHarmonicFlag = flag;
    mSASFrame->SetHarmonicSoundFlag(flag);
}

void
SASViewerProcess2::SetSynthMode(SASFrame3::SynthMode mode)
{
    mSASFrame->SetSynthMode(mode);
}

void
SASViewerProcess2::SetEnableOutHarmo(bool flag)
{
    mEnableOutHarmo = flag;
}

void
SASViewerProcess2::SetEnableOutNoise(bool flag)
{
    mEnableOutNoise = flag;
}

void
SASViewerProcess2::DBG_SetDbgParam(BL_FLOAT param)
{
    if (mPartialTracker != NULL)
        mPartialTracker->DBG_SetDbgParam(param);
}

void
SASViewerProcess2::SetTimeSmoothCoeff(BL_FLOAT coeff)
{
    if (mPartialTracker != NULL)
        mPartialTracker->SetTimeSmoothCoeff(coeff);
}

void
SASViewerProcess2::SetTimeSmoothNoiseCoeff(BL_FLOAT coeff)
{
    if (mPartialTracker != NULL)
        mPartialTracker->SetTimeSmoothNoiseCoeff(coeff);
}

void
SASViewerProcess2::Display()
{
    DisplayTracking();
    
    DisplayHarmo();
    
    DisplayNoise();
    
    DisplayAmplitude();
    
    DisplayFrequency();
    
    DisplayColor();
    
    DisplayWarping();
}

void
SASViewerProcess2::DetectPartials(const WDL_TypedBuf<BL_FLOAT> &magns,
                                  const WDL_TypedBuf<BL_FLOAT> &phases)
{
    mPartialTracker->SetData(magns, phases);
    mPartialTracker->DetectPartials();
    
#if !DEBUG_MUTE_NOISE
    mPartialTracker->ExtractNoiseEnvelope();
#endif
    
    mPartialTracker->FilterPartials();
}

void
SASViewerProcess2::IdToColor(int idx, unsigned char color[3])
{
    if (idx == -1)
    {
        color[0] = 255;
        color[1] = 0;
        color[2] = 0;
        
        return;
    }
    
    int r = (678678 + idx*12345)%255;
    int g = (3434345 + idx*123345435345)%255;
    int b = (997867 + idx*12345114222)%255;
    
    color[0] = r;
    color[1] = g;
    color[2] = b;
}

void
SASViewerProcess2::PartialToColor(const PartialTracker5::Partial &partial,
                                  unsigned char color[4])
{
    if (partial.mId == -1)
    {
        // Green
        color[0] = 0;
        color[1] = 255;
        color[2] = 0;
        color[3] = 255;
        
        return;
    }
    
    int deadAlpha = 255; //128;
    
#if SHOW_ONLY_ALIVE
    deadAlpha = 0;
#endif
    
    if (partial.mState == PartialTracker5::Partial::ZOMBIE)
    {
        // Green
        color[0] = 255;
        color[1] = 0;
        color[2] = 255;
        color[3] = deadAlpha;
        
        return;
    }
    
    if (partial.mState == PartialTracker5::Partial::DEAD)
    {
        // Green
        color[0] = 255;
        color[1] = 0;
        color[2] = 0;
        color[3] = deadAlpha;
        
        return;
    }
    
    int alpha = 255;
    if (partial.mAge < MIN_AGE_DISPLAY)
    {
        alpha = 0;
    }
    
    IdToColor((int)partial.mId, color);
    color[3] = alpha; //255;
}

void
SASViewerProcess2::DisplayTracking()
{
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->ShowTrackingLines(TRACKING, mShowTrackingLines);
        
        // Add the magnitudes
        mSASViewerRender->AddData(TRACKING, mCurrentMagns);
        
        mSASViewerRender->SetLineMode(TRACKING, LinesRender2::LINES_FREQ);
        
        // Add lines corresponding to the well tracked partials
        vector<PartialTracker5::Partial> partials = mCurrentNormPartials;
        
        // Create blue lines from trackers
        vector<LinesRender2::Point> line;
        for (int i = 0; i < partials.size(); i++)
        {
            const PartialTracker5::Partial &partial = partials[i];
            
            LinesRender2::Point p;
            
            BL_FLOAT partialX = partial.mFreq;
            
            p.mX = partialX - 0.5;
            p.mY = partial.mAmp;
            
            p.mZ = 0.0;
            
            p.mId = (int)partial.mId;
            
            line.push_back(p);
        }
        
        int numSlices = mSASViewerRender->GetNumSlices();
        int speed = mSASViewerRender->GetSpeed();
        
        bool skipAdd = (mAddNum++ % speed != 0);
        if (!skipAdd)
        {
            // Keep track of the points we pop
            vector<LinesRender2::Point> prevPoints;
            
            mPartialsPoints.push_back(line);
            while(mPartialsPoints.size() > numSlices)
            {
                prevPoints = mPartialsPoints[0];
                
                mPartialsPoints.pop_front();
            }
            
            CreateLines(prevPoints);
            
            // It is cool like that: lite blue with alpha
            //unsigned char color[4] = { 64, 64, 255, 255 };
            
            // Set color
            for (int j = 0; j < mPartialLines.size(); j++)
            {
                LinesRender2::Line &line = mPartialLines[j];
                
                if (!line.mPoints.empty())
                {
                    // Default
                    //line.mColor[0] = color[0];
                    //line.mColor[1] = color[1];
                    //line.mColor[2] = color[2];
                    
                    // Debug
                    IdToColor(line.mPoints[0].mId, line.mColor);
                    
                    line.mColor[3] = 255; // alpha
                }
            }
            
            //BL_FLOAT lineWidth = 4.0;
            BL_FLOAT lineWidth = 1.5;
            mSASViewerRender->SetAdditionalLines(TRACKING, mPartialLines, lineWidth);
        }
    }
}

void
SASViewerProcess2::DisplayHarmo()
{
    WDL_TypedBuf<BL_FLOAT> noise;
    mSASFrame->GetNoiseEnvelope(&noise);
    
    WDL_TypedBuf<BL_FLOAT> harmo = mCurrentMagns;
    BLUtils::SubstractValues(&harmo, noise);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(HARMO, harmo);
        mSASViewerRender->SetLineMode(HARMO, LinesRender2::LINES_FREQ);
        
        mSASViewerRender->ShowTrackingLines(HARMO, false);
    }
}

void
SASViewerProcess2::DisplayNoise()
{
    WDL_TypedBuf<BL_FLOAT> noise;
    mSASFrame->GetNoiseEnvelope(&noise);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(NOISE, noise);
        mSASViewerRender->SetLineMode(NOISE, LinesRender2::LINES_FREQ);
        
        mSASViewerRender->ShowTrackingLines(NOISE, false);
    }
}

void
SASViewerProcess2::DisplayAmplitude()
{
#define AMP_Y_COEFF 10.0 //20.0 //1.0
    
    BL_FLOAT amp = mSASFrame->GetAmplitude();
    
    WDL_TypedBuf<BL_FLOAT> amps;
    amps.Resize(mBufferSize/2);
    BLUtils::FillAllValue(&amps, amp);
    
    BLUtils::MultValues(&amps, (BL_FLOAT)AMP_Y_COEFF);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(AMPLITUDE, amps);
        
        // WARNING: Won't benefit from straight lines optim
        mSASViewerRender->SetLineMode(AMPLITUDE, LinesRender2::LINES_TIME);
        
        mSASViewerRender->ShowTrackingLines(AMPLITUDE, false);
    }
}

void
SASViewerProcess2::DisplayFrequency()
{
#define FREQ_Y_COEFF 40.0 //10.0
    
    BL_FLOAT freq = mSASFrame->GetFrequency();
  
    freq /= mSampleRate*0.5;

    WDL_TypedBuf<BL_FLOAT> freqs;
    freqs.Resize(mBufferSize/2);
    BLUtils::FillAllValue(&freqs, freq);
    
    BLUtils::MultValues(&freqs, (BL_FLOAT)FREQ_Y_COEFF);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(FREQUENCY, freqs);
        
        // WARNING: Won't benefit from straight lines optim
        mSASViewerRender->SetLineMode(FREQUENCY, LinesRender2::/*LINES_FREQ*/LINES_TIME);
        
        mSASViewerRender->ShowTrackingLines(FREQUENCY, false);
    }
}

void
SASViewerProcess2::DisplayColor()
{
    WDL_TypedBuf<BL_FLOAT> color;
    mSASFrame->GetColor(&color);
    
    BL_FLOAT amplitude = mSASFrame->GetAmplitude();
    
    BLUtils::MultValues(&color, amplitude);
    
    if (mPartialTracker != NULL)
        mPartialTracker->PreProcessDataXY(&color);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(COLOR, color);
        mSASViewerRender->SetLineMode(COLOR, LinesRender2::LINES_FREQ);
        
        mSASViewerRender->ShowTrackingLines(COLOR, false);
    }
}

void
SASViewerProcess2::DisplayWarping()
{
#define WARPING_COEFF 1.0
    
    WDL_TypedBuf<BL_FLOAT> warping;
    mSASFrame->GetNormWarping(&warping);
    
    if (mPartialTracker != NULL)
        mPartialTracker->PreProcessDataX(&warping);
    
    BLUtils::AddValues(&warping, (BL_FLOAT)-1.0);
    
    BLUtils::MultValues(&warping, (BL_FLOAT)WARPING_COEFF);
    
    if (mSASViewerRender != NULL)
    {
        mSASViewerRender->AddData(WARPING, warping);
        mSASViewerRender->SetLineMode(WARPING, LinesRender2::LINES_FREQ);
        
        mSASViewerRender->ShowTrackingLines(WARPING, false);
    }
}

int
SASViewerProcess2::FindIndex(const vector<int> &ids, int idx)
{
    if (idx == -1)
        return -1;
    
    for (int i = 0; i < ids.size(); i++)
    {
        if (ids[i] == idx)
            return i;
    }
    
    return -1;
}

int
SASViewerProcess2::FindIndex(const vector<LinesRender2::Point> &points, int idx)
{
    if (idx == -1)
        return -1;
    
    for (int i = 0; i < points.size(); i++)
    {
        if (points[i].mId == idx)
            return i;
    }
    
    return -1;
}

// Optimized version
void
SASViewerProcess2::CreateLines(const vector<LinesRender2::Point> &prevPoints)
{
    if (mSASViewerRender == NULL)
        return;
    
    if (mPartialsPoints.empty())
        return;
    
    // Update z for the current line points
    //
    int divisor = mSASViewerRender->GetNumSlices() - 1;
    if (divisor <= 0)
        divisor = 1;
    BL_FLOAT incrZ = 1.0/divisor;
    for (int i = 0; i < mPartialLines.size(); i++)
    {
        LinesRender2::Line &line = mPartialLines[i];
        for (int j = 0; j < line.mPoints.size(); j++)
        {
            LinesRender2::Point &p = line.mPoints[j];
            
            p.mZ -= incrZ;
        }
    }
    
    // Shorten the lines if they are too long
    vector<LinesRender2::Line> newLines;
    for (int i = 0; i < mPartialLines.size(); i++)
    {
        const LinesRender2::Line &line = mPartialLines[i];
        
        LinesRender2::Line newLine;
        for (int j = 0; j < line.mPoints.size(); j++)
        {
            const LinesRender2::Point &p = line.mPoints[j];
            if (p.mZ > 0.0)
                newLine.mPoints.push_back(p);
        }
        
        if (!newLine.mPoints.empty())
            newLines.push_back(newLine);
    }
    
    // Update the current partial lines
    mPartialLines = newLines;
    newLines.clear();
    
    // Create the new lines
    const vector<LinesRender2::Point> &newPoints =
                    mPartialsPoints[mPartialsPoints.size() - 1];
    for (int i = 0; i < newPoints.size(); i++)
    {
        LinesRender2::Point newPoint = newPoints[i];
        newPoint.mZ = 1.0;
        
        bool pointAdded = false;
        
        // Search for previous lines to be continued
        for (int j = 0; j < mPartialLines.size(); j++)
        {
            LinesRender2::Line &prevLine = mPartialLines[j];
        
            if (!prevLine.mPoints.empty())
            {
                int lineIdx = prevLine.mPoints[0].mId;
                
                if (lineIdx == newPoint.mId)
                {
                    // Add the point to prev line
                    prevLine.mPoints.push_back(newPoint);
                    
                    // We are done
                    pointAdded = true;
                    
                    break;
                }
            }
        }
        
        // Create a new line ?
        if (!pointAdded)
        {
            LinesRender2::Line newLine;
            newLine.mPoints.push_back(newPoint);
            
            mPartialLines.push_back(newLine);
        }
    }
}

#endif // IGRAPHICS_NANOVG
