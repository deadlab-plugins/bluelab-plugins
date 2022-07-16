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
 
#ifdef IGRAPHICS_NANOVG

#include <BLUtils.h>
#include <IdLinker.h>

#include <MorphoWaterfallRender.h>

#include <Morpho_defs.h>

#include "MorphoWaterfallView.h"


#define DBG_DISPLAY_BETA0 1
#define DBG_DISPLAY_ZOMBIES 1

// WIP
#define HARD_OPTIM 1 //0 //1

#if HARD_OPTIM
#define DBG_DISPLAY_BETA0 0
#define DBG_DISPLAY_ZOMBIES 0
#endif

// Empirical coeffs
//#define VIEW_ALPHA0_COEFF 1e4 //5e2
#define VIEW_BETA0_COEFF 1e3

//#if !FIND_PEAK_COMPAT
////#define VIEW_ALPHA0_COEFF 5.0/*50.0*//EMPIR_ALPHA0_COEFF // Red segments, for amps
//#define VIEW_BETA0_COEFF 0.02/EMPIR_BETA0_COEFF // Blue segments, for freqs
//#endif

// If not set to 1, we will skip steps, depending on the speed
// (for example we won't display every zombie point)
#define DISPLAY_EVERY_STEP 1 // 0

// Disable all display if uncheck "SHOW TRACK"
#define DEBUG_DISABLE_DISPLAY 1 // 0

#define OPTIM_PARTIAL_TRACKING_MEMORY 1

// All points green => optimizes a lot
#define POINTS_OPTIM_SAME_COLOR 1

#if DBG_DISPLAY_ZOMBIES
// Disable optimization, to make possible to have green and pink points, for zombies
#define POINTS_OPTIM_SAME_COLOR 0
#endif

MorphoWaterfallView::MorphoWaterfallView(BL_FLOAT sampleRate,
                                         MorphoPlugMode plugMode)
{
    mView3DListener = NULL;
        
    mSampleRate = sampleRate;
    
    mAddNum = 0;
    mSkipAdd = false;
    
    mShowTrackingLines = true;
    mShowDetectionPoints = true;

    // Data scale
    mViewScale = new Scale();
    mViewXScale = Scale::MEL_FILTER;
    mViewXScaleFB = Scale::FILTER_BANK_MEL;

    mWaterfallRender = NULL;

    mViewMode = TRACKING;

    mPlugMode = plugMode;
    
    mSpeedMod = 1;
    
    // Rainbow
    /*mIdColorMask[0] = 1.0;
      mIdColorMask[1] = 1.0;
      mIdColorMask[1] = 1.0;*/

    // Blue/greenish
    /*mIdColorMask[0] = 0.0;
      mIdColorMask[1] = 1.0;
      mIdColorMask[2] = 1.0;*/

    // Blue/greenish (more blue)
    mIdColorMask[0] = 0.0;
    mIdColorMask[1] = 1.0;
    mIdColorMask[2] = 2.0; // 1.5
}

MorphoWaterfallView::~MorphoWaterfallView()
{
    delete mViewScale;
}

void
MorphoWaterfallView::Reset(BL_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
}

MorphoWaterfallRender *
MorphoWaterfallView::CreateWaterfallRender(GraphControl12 *graphControl)
{
    mWaterfallRender = new MorphoWaterfallRender(graphControl);
    mWaterfallRender->SetSpeedMod(mSpeedMod);
    
    mWaterfallRender->SetView3DListener(mView3DListener);

    mWaterfallRender->SetMode(mViewMode);
        
    return mWaterfallRender;
}

void
MorphoWaterfallView::SetView3DListener(View3DPluginInterface *view3DListener)
{
    mView3DListener = view3DListener;

    if (mWaterfallRender != NULL)
        mWaterfallRender->SetView3DListener(mView3DListener);
}

void
MorphoWaterfallView::SetSpeedMod(int speedMod)
{
    mSpeedMod = speedMod;
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetSpeedMod(mSpeedMod);
}

void
MorphoWaterfallView::AddMorphoFrame(const MorphoFrame7 &frame)
{
    bool skipAdd = ((mAddNum++ % mSpeedMod) != 0);
    if (skipAdd)
        return;
    
    mMorphoFrame = frame;

    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    mMorphoFrame.GetInputMagns(&mCurrentMagns, applyFactor);
    mMorphoFrame.GetRawPartials(&mCurrentRawPartials, applyFactor);
    mMorphoFrame.GetNormPartials(&mCurrentNormPartials, applyFactor);
    
    Display();
}

void
MorphoWaterfallView::SetDisplayMode(DisplayMode mode)
{
    mViewMode = mode;
    
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetMode(mViewMode);
}

void
MorphoWaterfallView::SetShowDetectionPoints(bool flag)
{
    mShowDetectionPoints = flag;
    
    if (mWaterfallRender != NULL)
        mWaterfallRender->ShowAdditionalPoints(DETECTION, flag);
}

void
MorphoWaterfallView::SetShowTrackingLines(bool flag)
{
    mShowTrackingLines = flag;
    
    if (mWaterfallRender != NULL)
        mWaterfallRender->ShowAdditionalLines(TRACKING, flag);
}

void
MorphoWaterfallView::Display()
{
#if DEBUG_DISABLE_DISPLAY
    if (!mShowTrackingLines)
        return;
#endif
    
#if !DISPLAY_EVERY_STEP
    if (mWaterfallRender != NULL)
    {
        int speed = mWaterfallRender->GetSpeed();
        mSkipAdd = ((mAddNum++ % speed) != 0);
    }
    
    if (mSkipAdd)
        return;
#endif

    DisplayInput();
    
#if DBG_DISPLAY_BETA0
    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
        DisplayDetectionBeta0(false);
#endif

    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
        DisplayDetection();

#if DBG_DISPLAY_ZOMBIES
    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
        DisplayZombiePoints();
#endif

    if (mPlugMode == MORPHO_PLUG_MODE_SOURCES)
        DisplayTracking();
    
    DisplayHarmo();
    
    DisplayNoise();

    // Not used in Morphp
    //DisplayAmplitude();
    //DisplayFrequency();
    
    DisplayColor();
    
    DisplayWarping();
}

// TODO: use a tint for partial tracks, so they are less rainbow style
void
MorphoWaterfallView::IdToColor(int idx, unsigned char color[3])
{
    if (idx == -1)
    {
        /* Green
           color[0] = 255;
           color[1] = 0;
           color[2] = 0; */

        // Blueish
        color[0] = 0;
        color[1] = 112;
        color[2] = 247;
          
        return;
    }
    
    int r = (678678 + idx*12345)%255;
    int g = (3434345 + idx*123345435345)%255;
    int b = (997867 + idx*12345114222)%255;

    // Apply color mask
    r *= mIdColorMask[0];
    if (r > 255)
        r = 255;

    g *= mIdColorMask[1];
    if (g > 255)
        g = 255;

    b *= mIdColorMask[2];
    if (b > 255)
        b = 255;

    // Assigne the result
    color[0] = r;
    color[1] = g;
    color[2] = b;
}

void
MorphoWaterfallView::DisplayDetection()
{    
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->ShowAdditionalPoints(DETECTION, mShowDetectionPoints);

        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf2;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, mCurrentMagns,
                                         mSampleRate, mCurrentMagns.GetSize());
    
        mWaterfallRender->AddData(DETECTION, data);

        if (!mShowTrackingLines)
        {
            mWaterfallRender->ShowAdditionalPoints(DETECTION, false);
            
            return;
        }
        
        mWaterfallRender->SetLineMode(DETECTION, LinesRender2::LINES_FREQ);

        // Add points corresponding to raw detected partials
        vector<Partial2> partials = mCurrentRawPartials;

        // Create line
        vector<LinesRender2::Point> line;
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];
            
            LinesRender2::Point p;
            
            BL_FLOAT partialX = partial.mFreq;

            partialX =
                mViewScale->ApplyScale(mViewXScale, partialX,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                              
            p.mX = partialX - 0.5;
            p.mY = partial.mAmp;
            
            p.mZ = 1.0;
            
            p.mId = (int)partial.mId;
            
            line.push_back(p);
        }

        //
        int numSlices = mWaterfallRender->GetNumSlices();

        // Keep track of the points we pop
        vector<LinesRender2::Point> prevPoints;
        // Initialize, just in case
        if (!mPartialsPoints.empty())
            prevPoints = mPartialsPoints[0];
        
        mPartialsPoints.push_back(line);
        
        while(mPartialsPoints.size() > numSlices)
        {
            prevPoints = mPartialsPoints[0];
            mPartialsPoints.pop_front();
        }
        
        // It is cool like that: lite blue with alpha
        //unsigned char color[4] = { 64, 64, 255, 255 };
        
        // Green
        //unsigned char color[4] = { 0, 255, 0, 255 };
        // Bluish
        unsigned char color[4] = { 0, 112, 247, 255 };
        
        // Set color
        for (int j = 0; j < mPartialsPoints.size(); j++)
        {
            vector<LinesRender2::Point> &line2 = mPartialsPoints[j];

            for (int i = 0; i < line2.size(); i++)
            {
                LinesRender2::Point &p = line2[i];

                // Color
                p.mR = color[0];
                p.mG = color[1];
                p.mB = color[2];
                p.mA = color[3];
            }
        }

        // Update Z
        int divisor = mWaterfallRender->GetNumSlices();
        if (divisor <= 0)
            divisor = 1;
        BL_FLOAT incrZ = 1.0/divisor;
        for (int j = 0; j < mPartialsPoints.size(); j++)
        {
            vector<LinesRender2::Point> &line2 = mPartialsPoints[j];

            if (line2.empty())
                continue;
            
            BL_FLOAT z = line2[0].mZ;
            z -= incrZ;

            for (int i = 0; i < line2.size(); i++)
            {
                LinesRender2::Point &p = line2[i];
                p.mZ = z; 
            }
        }
        
        BL_FLOAT lineWidth = 4.0;

        vector<LinesRender2::Line> &partialLines = mTmpBuf0;
        PointsToLines(mPartialsPoints, &partialLines);
        
        mWaterfallRender->SetAdditionalPoints(DETECTION, partialLines, lineWidth,
                                              POINTS_OPTIM_SAME_COLOR);
    }
}

// Set add data to false if we already call DisplayDetection()
void
MorphoWaterfallView::DisplayDetectionBeta0(bool addData)
{
    if (!mShowTrackingLines)
    {
        mWaterfallRender->ShowAdditionalLines(DETECTION, false);
        
        return;
    }
    
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->ShowAdditionalLines(DETECTION, mShowDetectionPoints);

        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf7;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, mCurrentMagns,
                                         mSampleRate, mCurrentMagns.GetSize());

        if (addData)
            mWaterfallRender->AddData(DETECTION, data);
        
        mWaterfallRender->SetLineMode(DETECTION, LinesRender2::LINES_FREQ);

        // Add points corresponding to raw detected partials
        vector<Partial2> partials = mCurrentNormPartials; //mCurrentRawPartials;

        // Create line
        vector<vector<LinesRender2::Point> > segments;
        for (int i = 0; i < partials.size(); i++)
        {
            vector<LinesRender2::Point> lineAlpha;
            vector<LinesRender2::Point> lineBeta;
                
            const Partial2 &partial = partials[i];
            
            // First point (standard)
            LinesRender2::Point p0;
            
            BL_FLOAT partialX0 = partial.mFreq;
            partialX0 =
                mViewScale->ApplyScale(mViewXScale, partialX0,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                              
            p0.mX = partialX0 - 0.5;
            p0.mY = partial.mAmp;
            p0.mZ = 1.0;
            p0.mId = (int)partial.mId;

            lineAlpha.push_back(p0);
            lineBeta.push_back(p0);

            // Second point (extrapolated)
            LinesRender2::Point p1;

            // amp + alpha0, with the sum managin scales correctly
            //BL_FLOAT partialY1 = partial.mAmp;
            BL_FLOAT partialY1 = partial.mAmpAlpha0;
            
            p1.mX = partialX0 - 0.5;
            p1.mY = partialY1;
            p1.mZ = 1.0;
            p1.mId = (int)partial.mId;

            // Red
            p1.mR = 255;
            p1.mG = 0;
            p1.mB = 0;
            p1.mA = 255;
            
            lineAlpha.push_back(p1);
            segments.push_back(lineAlpha);

            // Third point (extrapolated)
            LinesRender2::Point p2;

            // Was 1000
            BL_FLOAT partialX1 = partial.mFreq + partial.mBeta0*VIEW_BETA0_COEFF;
            
            partialX1 =
                mViewScale->ApplyScale(mViewXScale, partialX1,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                              
            p2.mX = partialX1 - 0.5;
            p2.mY = partial.mAmp;
            p2.mZ = 1.0;
            p2.mId = (int)partial.mId;

            // Blue
            p2.mR = 0;
            p2.mG = 0;
            p2.mB = 255;
            p2.mA = 255;
            
            lineBeta.push_back(p2);
            segments.push_back(lineBeta);
        }

        //
        int numSlices = mWaterfallRender->GetNumSlices();
        
        mPartialsSegments.push_back(segments);
        
        while(mPartialsSegments.size() > numSlices)
            mPartialsSegments.pop_front();
        
        // Update Z
        int divisor = mWaterfallRender->GetNumSlices();
        if (divisor <= 0)
            divisor = 1;
        BL_FLOAT incrZ = 1.0/divisor;

        for (int j = 0; j < mPartialsSegments.size(); j++)
        {
            vector<vector<LinesRender2::Point> > &segments = mPartialsSegments[j];

            for (int i = 0; i < segments.size(); i++)
            {
                vector<LinesRender2::Point> &seg = segments[i];

                if (seg.empty())
                    continue;
                
                BL_FLOAT z = seg[0].mZ;
                z -= incrZ;
            
                for (int k = 0; k < seg.size(); k++)
                {   
                    LinesRender2::Point &p = seg[k];

                    p.mZ = z;
                }
            }
        }
        
        BL_FLOAT lineWidth = 2.0;
        
        vector<LinesRender2::Line> &partialLines = mTmpBuf8;
        SegmentsToLines(mPartialsSegments, &partialLines);

        mWaterfallRender->SetAdditionalLines(DETECTION, partialLines, lineWidth);
    }
}

void
MorphoWaterfallView::DisplayZombiePoints()
{
#define ZOMBIE_POINT_OFFSET_X 0.0025

    if (!mShowTrackingLines)
    {
        mWaterfallRender->ShowAdditionalPoints(DETECTION, false);
        
        return;
    }
    
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->ShowAdditionalPoints(DETECTION, mShowDetectionPoints);
        
        // Add points corresponding to raw detected partials
        vector<Partial2> partials = mCurrentNormPartials;

        // Create line
        vector<LinesRender2::Point> line;
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];

            if (partial.mState != Partial2::State::ZOMBIE)
                continue;
            
            LinesRender2::Point p;
            
            BL_FLOAT partialX = partial.mFreq;

            partialX =
                mViewScale->ApplyScale(mViewXScale, partialX,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                              
            p.mX = partialX - 0.5;
            p.mY = partial.mAmp;
            
            p.mZ = 1.0;
            
            p.mId = (int)partial.mId;

            // Make a small offset, so we can see both points
            p.mX += ZOMBIE_POINT_OFFSET_X;
            
            line.push_back(p);
        }
        
        //
        int numSlices = mWaterfallRender->GetNumSlices();

        // Keep track of the points we pop
        vector<LinesRender2::Point> prevPoints;
        // Initialize, just in case
        if (!mPartialsPointsZombie.empty())
            prevPoints = mPartialsPointsZombie[0];
        
        mPartialsPointsZombie.push_back(line);
        
        while(mPartialsPointsZombie.size() > numSlices)
        {
            prevPoints = mPartialsPointsZombie[0];
            mPartialsPointsZombie.pop_front();
        }
        
        // Magenta
        unsigned char color[4] = { 255, 0, 255, 255 };
        
        // Set color
        for (int j = 0; j < mPartialsPointsZombie.size(); j++)
        {
            vector<LinesRender2::Point> &line2 = mPartialsPointsZombie[j];

            for (int i = 0; i < line2.size(); i++)
            {
                LinesRender2::Point &p = line2[i];

                // Color
                p.mR = color[0];
                p.mG = color[1];
                p.mB = color[2];
                p.mA = color[3];
            }
        }

        // Update Z
        int divisor = mWaterfallRender->GetNumSlices();
        if (divisor <= 0)
            divisor = 1;
        BL_FLOAT incrZ = 1.0/divisor;
        for (int j = 0; j < mPartialsPointsZombie.size(); j++)
        {
            vector<LinesRender2::Point> &line2 = mPartialsPointsZombie[j];

            if (line2.empty())
                continue;
            
            BL_FLOAT z = line2[0].mZ;
            z -= incrZ;

            for (int i = 0; i < line2.size(); i++)
            {
                LinesRender2::Point &p = line2[i];
                p.mZ = z; 
            }
        }
        
        BL_FLOAT lineWidth = 4.0;

        vector<LinesRender2::Line> &partialLines = mTmpBuf1;
        //PointsToLines(mPartialsPointsZombie, &partialLines);
        
        // Add all the points at the same time
        PointsToLinesMix(mPartialsPoints, mPartialsPointsZombie, &partialLines);
        
        mWaterfallRender->SetAdditionalPoints(DETECTION, partialLines, lineWidth,
                                              POINTS_OPTIM_SAME_COLOR);
    }
}

void
MorphoWaterfallView::DisplayTracking()
{
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->ShowAdditionalLines(TRACKING, mShowTrackingLines);
        
        // Add the magnitudes
        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf3;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, mCurrentMagns,
                                         mSampleRate, mCurrentMagns.GetSize());
                                         
        mWaterfallRender->AddData(TRACKING, data);

        if (!mShowTrackingLines)
        {
            mWaterfallRender->ShowAdditionalLines(TRACKING, false);
            
            return;
        }
        
        mWaterfallRender->SetLineMode(TRACKING, LinesRender2::LINES_FREQ);
        
        // Add lines corresponding to the well tracked partials
        vector<Partial2> partials = mCurrentNormPartials;

#if !OPTIM_PARTIAL_TRACKING_MEMORY
        // Create blue lines from trackers
        vector<LinesRender2::Point> line;
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];
            
            LinesRender2::Point p;
            
            BL_FLOAT partialX = partial.mFreq;

            partialX =
                mViewScale->ApplyScale(mViewXScale, partialX,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                       
            p.mX = partialX - 0.5;
            p.mY = partial.mAmp;
            
            p.mZ = 0.0;
            
            p.mId = (int)partial.mId;
            
            line.push_back(p);
        }
#else // Optimized
        vector<LinesRender2::Point> &line = mTmpBuf9;
        line.resize(partials.size());
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];
            
            LinesRender2::Point &p = line[i];
            
            BL_FLOAT partialX = partial.mFreq;

            partialX =
                mViewScale->ApplyScale(mViewXScale, partialX,
                                       (BL_FLOAT)0.0, (BL_FLOAT)(mSampleRate*0.5));
                                       
            p.mX = partialX - 0.5;
            p.mY = partial.mAmp;
            
            p.mZ = 0.0;
            
            p.mId = (int)partial.mId;
        }
#endif
        
        //
        int numSlices = mWaterfallRender->GetNumSlices();
        
        // Keep track of the points we pop
        vector<LinesRender2::Point> prevPoints;
        // Initialize, just in case
        if (!mFilteredPartialsPoints.empty())
            prevPoints = mFilteredPartialsPoints[0];
        
        mFilteredPartialsPoints.push_back(line);
        
        while(mFilteredPartialsPoints.size() > numSlices)
        {
            prevPoints = mFilteredPartialsPoints[0];
            
            mFilteredPartialsPoints.pop_front();
        }

        // Optimized a lot
        CreateLines(prevPoints);
        
        // Set color
        for (int j = 0; j < mPartialLines.size(); j++)
        {
            LinesRender2::Line &line2 = mPartialLines[j];
            
            if (!line2.mPoints.empty())
            {
                // Default
                //line.mColor[0] = color[0];
                //line.mColor[1] = color[1];
                //line.mColor[2] = color[2];
                
                // Debug
                IdToColor(line2.mPoints[0].mId, line2.mColor);
                
                line2.mColor[3] = 255; // alpha
            }
        }
        
        //BL_FLOAT lineWidth = 1.5;

        BL_FLOAT lineWidth = 4.0;
        
        mWaterfallRender->SetAdditionalLines(TRACKING, mPartialLines, lineWidth);
    }
}

void
MorphoWaterfallView::DisplayInput()
{
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->ShowAdditionalPoints(AMP, false);

        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf11;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, mCurrentMagns,
                                         mSampleRate, mCurrentMagns.GetSize());
    
        mWaterfallRender->AddData(AMP, data);

        mWaterfallRender->SetLineMode(AMP, LinesRender2::LINES_FREQ);
    }
}

void
MorphoWaterfallView::DisplayHarmo()
{
    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
#if 1 // Beautiful (but decrease the peaks by the noise floor)
    WDL_TypedBuf<BL_FLOAT> noise;
    mMorphoFrame.GetNoiseEnvelope(&noise, applyFactor);
      
    WDL_TypedBuf<BL_FLOAT> harmo = mCurrentMagns;
    BLUtils::SubstractValues(&harmo, noise);
    
    BLUtils::ClipMin(&harmo, (BL_FLOAT)0.0);
#endif

#if 0 // Not beautiful (very drafty view, but correct data)
    WDL_TypedBuf<BL_FLOAT> harmo;
    mMorphoFrame.GetHarmoEnvelope(&harmo, applyFactor);
#endif
    
    if (mWaterfallRender != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf10;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, harmo,
                                         mSampleRate, harmo.GetSize());
        
        mWaterfallRender->AddData(HARMONIC, data);
        mWaterfallRender->SetLineMode(HARMONIC, LinesRender2::LINES_FREQ);
        
        mWaterfallRender->ShowAdditionalPoints(HARMONIC, false);
        mWaterfallRender->ShowAdditionalLines(HARMONIC, false);
    }
}

void
MorphoWaterfallView::DisplayNoise()
{
    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
    WDL_TypedBuf<BL_FLOAT> noise;
    mMorphoFrame.GetNoiseEnvelope(&noise, applyFactor);
        
    if (mWaterfallRender != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf4;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, noise,
                                         mSampleRate, noise.GetSize());
        
        mWaterfallRender->AddData(NOISE, data);
        mWaterfallRender->SetLineMode(NOISE, LinesRender2::LINES_FREQ);

        mWaterfallRender->ShowAdditionalPoints(NOISE, false);
        mWaterfallRender->ShowAdditionalLines(NOISE, false);
    }
}

void
MorphoWaterfallView::DisplayAmplitude()
{
#define AMP_Y_COEFF 10.0 //20.0 //1.0

    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
    BL_FLOAT amp = mMorphoFrame.GetAmplitude(applyFactor);

    WDL_TypedBuf<BL_FLOAT> amps;
    amps.Resize(BUFFER_SIZE/2);
    BLUtils::FillAllValue(&amps, amp);
    
    BLUtils::MultValues(&amps, (BL_FLOAT)AMP_Y_COEFF);
    
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->AddData(AMPLITUDE, amps);
        
        // WARNING: Won't benefit from straight lines optim
        mWaterfallRender->SetLineMode(AMPLITUDE,
                                      //LinesRender2::LINES_FREQ);
                                      LinesRender2::LINES_TIME);

        mWaterfallRender->ShowAdditionalPoints(AMPLITUDE, false);
        mWaterfallRender->ShowAdditionalLines(AMPLITUDE, false);
    }
}

void
MorphoWaterfallView::DisplayFrequency()
{
    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
#define FREQ_Y_COEFF 40.0 //10.0

    BL_FLOAT freq = mMorphoFrame.GetFrequency(applyFactor);
  
    freq /= mSampleRate*0.5;
    
    WDL_TypedBuf<BL_FLOAT> freqs;
    freqs.Resize(BUFFER_SIZE/2);
    BLUtils::FillAllValue(&freqs, freq);
    
    BLUtils::MultValues(&freqs, (BL_FLOAT)FREQ_Y_COEFF);
    
    if (mWaterfallRender != NULL)
    {
        mWaterfallRender->AddData(FREQUENCY, freqs);
        
        // WARNING: Won't benefit from straight lines optim
        mWaterfallRender->SetLineMode(FREQUENCY,
                                      //LinesRender2::LINES_FREQ);
                                      LinesRender2::LINES_TIME);

        mWaterfallRender->ShowAdditionalPoints(FREQUENCY, false);
        mWaterfallRender->ShowAdditionalLines(FREQUENCY, false);
    }
}

void
MorphoWaterfallView::DisplayColor()
{
    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
    WDL_TypedBuf<BL_FLOAT> color;
    mMorphoFrame.GetColorProcessed(&color, applyFactor);
    
    if (mWaterfallRender != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf5;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, color,
                                         mSampleRate, color.GetSize());
        
        mWaterfallRender->AddData(COLOR, data);
        mWaterfallRender->SetLineMode(COLOR, LinesRender2::LINES_FREQ);

        mWaterfallRender->ShowAdditionalPoints(COLOR, false);
        mWaterfallRender->ShowAdditionalLines(COLOR, false);
    }
}

void
MorphoWaterfallView::DisplayWarping()
{
    bool applyFactor = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
    
#define WARPING_COEFF 1.0

    WDL_TypedBuf<BL_FLOAT> warping;
    mMorphoFrame.GetWarpingProcessed(&warping, applyFactor); //
    
    BLUtils::AddValues(&warping, (BL_FLOAT)-1.0);
    
    BLUtils::MultValues(&warping, (BL_FLOAT)WARPING_COEFF);
    
    if (mWaterfallRender != NULL)
    {
        WDL_TypedBuf<BL_FLOAT> &data = mTmpBuf6;
        mViewScale->ApplyScaleFilterBank(mViewXScaleFB, &data, warping,
                                         mSampleRate, warping.GetSize());
        
        mWaterfallRender->AddData(WARPING, data);
        mWaterfallRender->SetLineMode(WARPING, LinesRender2::LINES_FREQ);

        mWaterfallRender->ShowAdditionalPoints(WARPING, false);
        mWaterfallRender->ShowAdditionalLines(WARPING, false);
    }
}

#if 0 // Not used anymore
// Optimized version
void
MorphoWaterfallView::CreateLinesPrev(const vector<LinesRender2::Point> &prevPoints)
{
    if (mWaterfallRender == NULL)
        return;
    
    if (mFilteredPartialsPoints.empty())
        return;
    
    // Update z for the current line points
    //
    int divisor = mWaterfallRender->GetNumSlices(); // - 1;
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
        mFilteredPartialsPoints[mFilteredPartialsPoints.size() - 1];
    for (int i = 0; i < newPoints.size(); i++)
    {
        LinesRender2::Point newPoint = newPoints[i];

        // Adjust
        newPoint.mZ = 1.0 - incrZ;
        
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
#endif

// More optimized version
void
MorphoWaterfallView::CreateLines(const vector<LinesRender2::Point> &prevPoints)
{
    if (mWaterfallRender == NULL)
        return;
    
    if (mFilteredPartialsPoints.empty())
        return;
    
    // Update z for the current line points
    //
    int divisor = mWaterfallRender->GetNumSlices(); // - 1;
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

    // Optimized (memory)
    for (int i = 0; i < mPartialLines.size(); i++)
    {
        LinesRender2::Line &line = mPartialLines[i];
        vector<LinesRender2::Point>::iterator it =
            remove_if(line.mPoints.begin(), line.mPoints.end(),
                      LinesRender2::Point::IsZLEqZero);
        line.mPoints.erase(it, line.mPoints.end());
        
    }
    vector<LinesRender2::Line>::iterator it2 =
        remove_if(mPartialLines.begin(), mPartialLines.end(),
                  LinesRender2::Line::IsPointsEmpty);
    mPartialLines.erase(it2, mPartialLines.end());
    
    // Create the new lines
    vector<LinesRender2::Point> &newPoints =
        mFilteredPartialsPoints[mFilteredPartialsPoints.size() - 1];

    for (int i = 0; i < mPartialLines.size(); i++)
        mPartialLines[i].ComputeIds();
    IdLinker<LinesRender2::Point, LinesRender2::Line>::LinkIds(&newPoints,
                                                               &mPartialLines, true);
    
    for (int i = 0; i < newPoints.size(); i++)
    {
        LinesRender2::Point newPoint = newPoints[i];

        // Adjust
        newPoint.mZ = 1.0 - incrZ;

        if (newPoint.mLinkedId >= 0)
            // Matching
        {
            // Add the point to prev line
            LinesRender2::Line &prevLine = mPartialLines[newPoint.mLinkedId];
            
            prevLine.mPoints.push_back(newPoint);
        }
        else
        {
            // Create a new line!
            LinesRender2::Line newLine;
            newLine.mPoints.push_back(newPoint);
            
            mPartialLines.push_back(newLine);
        }
    }
}

void
MorphoWaterfallView::PointsToLines(const deque<vector<LinesRender2::Point> > &points,
                                   vector<LinesRender2::Line> *lines)
{
    lines->resize(points.size());
    for (int i = 0; i < lines->size(); i++)
    {
        LinesRender2::Line &line = (*lines)[i];
        line.mPoints = points[i];
        
        // Dummy color
        line.mColor[0] = 0;
        line.mColor[1] = 0;
        line.mColor[2] = 0;
        line.mColor[3] = 0;
    }
}

void
MorphoWaterfallView::
PointsToLinesMix(const deque<vector<LinesRender2::Point> > &points0,
                 const deque<vector<LinesRender2::Point> > &points1,
                 vector<LinesRender2::Line> *lines)
{
    lines->resize(points0.size());
    for (int i = 0; i < lines->size(); i++)
    {
        LinesRender2::Line &line = (*lines)[i];
        line.mPoints = points0[i];

        for (int j = 0; j < points1[i].size(); j++)
        {
            const LinesRender2::Point &p = points1[i][j];
            line.mPoints.push_back(p);
        }
        
        // Dummy color
        line.mColor[0] = 0;
        line.mColor[1] = 0;
        line.mColor[2] = 0;
        line.mColor[3] = 0;
    }
}

void
MorphoWaterfallView::
SegmentsToLines(const deque<vector<vector<LinesRender2::Point> > > &segments,
                vector<LinesRender2::Line> *lines)
{    
    if (segments.empty())
    {
        lines->clear();
        
        return;
    }
    
    // Count the total number of lines
    int numLines = 0;
    for (int i = 0; i < segments.size(); i++)
    {
        const vector<vector<LinesRender2::Point> > &seg0 = segments[i];
        
        for (int j = 0; j < seg0.size(); j++)
        {
            const vector<LinesRender2::Point> &seg = seg0[j];

            if (seg.size() != 2)
                continue;

            numLines++;
        }
    }

    lines->resize(numLines);

    // Process
    int lineNum = 0;
    LinesRender2::Line line;
    line.mPoints.resize(2);
    for (int i = 0; i < segments.size(); i++)
    {
        const vector<vector<LinesRender2::Point> > &seg0 = segments[i];
        
        for (int j = 0; j < seg0.size(); j++)
        {
            const vector<LinesRender2::Point> &seg = seg0[j];

            if (seg.size() != 2)
                continue;

            line.mPoints[0] = seg[0];
            line.mPoints[1] = seg[1];
            
            // Take the color of the last point
            line.mColor[0] = seg[1].mR;
            line.mColor[1] = seg[1].mG;
            line.mColor[2] = seg[1].mB;
            line.mColor[3] = seg[1].mA;

            (*lines)[lineNum++] = line;
        }
    }
}

#endif // IGRAPHICS_NANOVG
