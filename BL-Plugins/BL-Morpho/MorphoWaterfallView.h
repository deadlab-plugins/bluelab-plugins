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
 
#ifndef MORPHO_WATERFALL_VIEW_H
#define MORPHO_WATERFALL_VIEW_H

#ifdef IGRAPHICS_NANOVG

#include <deque>
using namespace std;

#include <MorphoFrame7.h>

#include <LinesRender2.h>

#include <Morpho_defs.h>

class MorphoWaterfallRender;
class View3DPluginInterface;
class MorphoWaterfallView 
{
 public:
    // Display mode
    enum DisplayMode
    {
        AMP = 0,
        HARMONIC,
        NOISE,
        DETECTION,
        TRACKING,
        COLOR,
        WARPING,
        AMPLITUDE,
        FREQUENCY,
        
        NUM_MODES
    };

    MorphoWaterfallView(BL_FLOAT sampleRate, MorphoPlugMode plugMode);
    virtual ~MorphoWaterfallView();

    void Reset(BL_FLOAT sampleRate);
    
    MorphoWaterfallRender *CreateWaterfallRender(GraphControl12 *graphControl);

    void SetView3DListener(View3DPluginInterface *view3DListener);
    
    void SetSpeedMod(int speedMod);
    
    // Frame
    void AddMorphoFrame(const MorphoFrame7 &frame);

    void SetDisplayMode(DisplayMode mode);
        
 protected:
    // Display
    void SetShowTrackingLines(bool flag);
    void SetShowDetectionPoints(bool flag);

    //
    void Display(); // was Display()
    
    void IdToColor(int idx, unsigned char color[3]);

#if 0
    // Optimized version
    void CreateLinesPrev(const vector<LinesRender2::Point> &prevPoints);
#endif
    // More optimized
    void CreateLines(const vector<LinesRender2::Point> &prevPoints);

    // Display
    void DisplayDetection();
    void DisplayDetectionBeta0(bool addData); // Variation
    void DisplayZombiePoints();
    
    void DisplayTracking();

    void DisplayInput();
    void DisplayHarmo();
    void DisplayNoise();
    void DisplayAmplitude();
    void DisplayFrequency();
    void DisplayColor();
    void DisplayWarping();

    void PointsToLines(const deque<vector<LinesRender2::Point> > &points,
                       vector<LinesRender2::Line> *lines);
    void SegmentsToLines(const deque<vector<vector<LinesRender2::Point> > >&segments,
                         vector<LinesRender2::Line> *lines);

    void PointsToLinesMix(const deque<vector<LinesRender2::Point> > &points0,
                          const deque<vector<LinesRender2::Point> > &points1,
                          vector<LinesRender2::Line> *lines);

    //
    View3DPluginInterface *mView3DListener;
    
    //
    BL_FLOAT mSampleRate;
    
    //
    WDL_TypedBuf<BL_FLOAT> mCurrentMagns;
    // Not filtered
    vector<Partial2> mCurrentRawPartials;
    // Filtered
    vector<Partial2> mCurrentNormPartials;

    MorphoFrame7 mMorphoFrame;
    
    //
    // For tracking detection
    deque<vector<LinesRender2::Point> > mPartialsPoints;

    // For displaying beta0
    deque<vector<vector<LinesRender2::Point> > > mPartialsSegments;

    // For zombie points
    deque<vector<LinesRender2::Point> > mPartialsPointsZombie;
    
    // For tracking display
    deque<vector<LinesRender2::Point> > mFilteredPartialsPoints;
    
    // Keep an history, to avoid recomputing the whole lines each time
    // With this, we compute only the new extremity of the line
    vector<LinesRender2::Line> mPartialLines;
    
    //
    bool mShowTrackingLines;
    bool mShowDetectionPoints;

    // Data scale for viewing
    Scale *mViewScale;
    Scale::Type mViewXScale;
    Scale::FilterBankType mViewXScaleFB;

    //
    long int mAddNum;
    bool mSkipAdd;
    int mSpeedMod;
        
    MorphoWaterfallRender *mWaterfallRender;

    DisplayMode mViewMode;

    MorphoPlugMode mPlugMode;

    BL_FLOAT mIdColorMask[3];
    
 private:
    vector<LinesRender2::Line> mTmpBuf0;
    vector<LinesRender2::Line> mTmpBuf1;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7;
    vector<LinesRender2::Line> mTmpBuf8;
    vector<LinesRender2::Point> mTmpBuf9;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf10;
    WDL_TypedBuf<BL_FLOAT> mTmpBuf11;
};

#endif // IGRAPHICS_NANOVG

#endif
