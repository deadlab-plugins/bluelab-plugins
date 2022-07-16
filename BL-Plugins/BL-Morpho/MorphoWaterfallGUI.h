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
 
#ifndef MORPHO_WATERFALL_GUI_H
#define MORPHO_WATERFALL_GUI_H

#include <vector>
using namespace std;

#include <BLTypes.h>

#include <MorphoFrameSynth2.h> // For mode

#include <MorphoFrame7.h>

#include <Morpho_defs.h>

#include "IPlug_include_in_plug_hdr.h"
using namespace iplug;
using namespace iplug::igraphics;

class GraphControl12;
class GUIHelper12;
class MorphoWaterfallView;
class MorphoWaterfallRender;
class CurveViewAmp;
class CurveViewPitch;
class CurveViewPartials;
class ViewTitle;
class View3DPluginInterface;
class MorphoWaterfallGUI
{
public:
    MorphoWaterfallGUI(BL_FLOAT sampleRate,
                       MorphoPlugMode plugMode);
    virtual ~MorphoWaterfallGUI();

    void Reset(BL_FLOAT sampleRate);

    void AddMorphoFrames(const vector<MorphoFrame7> &frames);
    
    // Legacy mechanism
    void Lock();
    void Unlock();
    
    // Lock Free
    void PushAllData();
    
    void OnUIOpen();
    void OnUIClose();

    void SetView3DListener(View3DPluginInterface *view3DListener);
    
    bool IsControlsCreated() const;
    
    GraphControl12 *CreateControls(GUIHelper12 *guiHelper,
                                   Plugin *plug,
                                   IGraphics *pGraphics,
                                   int graphX, int graphY,
                                   const char *graphBitmapFn,
                                   int offsetX, int offsetY,
                                   int graphParamIdx = kNoParameter);

    void SetGraphEnabled(bool flag);
    
    // Parameters
    void SetWaterfallViewMode(WaterfallViewMode mode);
    void SetSynthMode(MorphoFrameSynth2::SynthMode mode);
    void SetTimeSmoothCoeff(BL_FLOAT timeSmooth);
    void SetDetectThreshold(BL_FLOAT detectThrs);
    void SetFreqThreshold(BL_FLOAT freqThrs);

    void SetWaterfallCameraAngle0(BL_FLOAT angle);
    void SetWaterfallCameraAngle1(BL_FLOAT angle);
    void SetWaterfallCameraFov(BL_FLOAT angle);
    
protected:
    void ClearControls(IGraphics *pGraphics);
    void FeedCurveViews(const vector<MorphoFrame7> &frames);

    MorphoPlugMode mPlugMode;
    
    //
    MorphoWaterfallView *mWaterfallView;

    //
    View3DPluginInterface *mView3DListener;
        
    //
    GUIHelper12 *mGUIHelper;
    IGraphics *mGraphics;
    GraphControl12 *mGraph;
    MorphoWaterfallRender *mWaterfallRender;

    //
    CurveViewAmp *mCurveViewAmp;
    CurveViewPitch *mCurveViewPitch;
    CurveViewPartials *mCurveViewPartials;

    ViewTitle *mViewTitle;
};

#endif
