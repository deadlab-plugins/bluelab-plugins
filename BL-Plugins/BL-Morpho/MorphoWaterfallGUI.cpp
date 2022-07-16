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
 
#include <MorphoWaterfallView.h>
#include <MorphoWaterfallRender.h>

#include <CurveViewAmp.h>
#include <CurveViewPitch.h>
#include <CurveViewPartials.h>

#include <ViewTitle.h>

#include <GraphControl12.h>
#include <GUIHelper12.h>

#include <ChromagramObj.h> // Maybe TMP

#include "MorphoWaterfallGUI.h"

// Remove control correctly
#define FIX_REMOVE_CONTROL_MORPHO 1

// Avoid a crash
#define USE_LEGACY_LOCK 1

// Max speed, no limitation
// => too fast and takes 10% more resources
//#define SPEED_MOD 1

// Slowed down speed
// More readable and takes 10% less resources
#define SPEED_MOD 4


MorphoWaterfallGUI::MorphoWaterfallGUI(BL_FLOAT sampleRate,
                                       MorphoPlugMode plugMode)
{
    mPlugMode = plugMode;
    
    mWaterfallView = new MorphoWaterfallView(sampleRate, mPlugMode);
    mWaterfallView->SetSpeedMod(SPEED_MOD);
    
    mGraph = NULL;
    mGraphics = NULL;

    mView3DListener = NULL;

    mWaterfallRender = NULL;

    mCurveViewAmp = new CurveViewAmp();
    mCurveViewAmp->SetBounds(22.0, 22.0, 86.0, 56.0);
    mCurveViewAmp->SetSpeedMod(SPEED_MOD);
    
    mCurveViewPitch = new CurveViewPitch();
    mCurveViewPitch->SetBounds(205.0, 22.0, 86.0, 56.0);
    mCurveViewPitch->SetSpeedMod(SPEED_MOD);
    
    mCurveViewPartials = new CurveViewPartials();
    mCurveViewPartials->SetBounds(388.0, 22.0, 86.0, 56.0);
    mCurveViewPartials->SetSpeedMod(SPEED_MOD);

    mViewTitle = new ViewTitle();
    mViewTitle->SetPos(22.0/*14.0*/, 404.0 - 14.0 - 2.0);
    
    mViewTitle->SetTitle("tracking"); // Default mode for MorphoWaterfallView
}

MorphoWaterfallGUI::~MorphoWaterfallGUI()
{
    ClearControls(mGraphics);

    delete mWaterfallView;

    if (mWaterfallRender != NULL)
        delete mWaterfallRender;

    delete mCurveViewAmp;
    delete mCurveViewPitch;
    delete mCurveViewPartials;

    delete mViewTitle;
}

void
MorphoWaterfallGUI::Reset(BL_FLOAT sampleRate)
{
    mWaterfallView->Reset(sampleRate);
}

void
MorphoWaterfallGUI::AddMorphoFrames(const vector<MorphoFrame7> &frames)
{
    FeedCurveViews(frames);

    if (!frames.empty())
        mWaterfallView->AddMorphoFrame(frames[frames.size() - 1]);
}

void
MorphoWaterfallGUI::Lock()
{
    if (mGraph == NULL)
        return;
    
    mGraph->Lock();
}

void
MorphoWaterfallGUI::Unlock()
{
    if (mGraph == NULL)
        return;
    
    mGraph->Unlock();
}

void
MorphoWaterfallGUI::PushAllData()
{
    if (mGraph == NULL)
        return;
  
    mGraph->PushAllData();
}

void
MorphoWaterfallGUI::OnUIOpen() {}

void
MorphoWaterfallGUI::OnUIClose()
{    
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetGraph(NULL);

    // Hack to avoid a crash
    if (mGraph != NULL)
        mGraph->ClearLinkedObjects();

    ClearControls(mGraphics);

    mGraph = NULL;
}

void
MorphoWaterfallGUI::SetView3DListener(View3DPluginInterface *view3DListener)
{
    mView3DListener = view3DListener;

    if (mWaterfallView != NULL)
        mWaterfallView->SetView3DListener(mView3DListener);
}

bool
MorphoWaterfallGUI::IsControlsCreated() const
{
    if (mGraph == NULL)
        return false;

    return true;
}

GraphControl12 *
MorphoWaterfallGUI::CreateControls(GUIHelper12 *guiHelper,
                                   Plugin *plug,
                                   IGraphics *pGraphics,
                                   int graphX, int graphY,
                                   const char *graphBitmapFn,
                                   int offsetX, int offsetY,
                                   int graphParamIdx)
{
    mGUIHelper = guiHelper;

    mGraphics = pGraphics;
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(plug, pGraphics,
                                     graphX, graphY,
                                     graphBitmapFn, graphParamIdx);

#if USE_LEGACY_LOCK
    mGraph->SetUseLegacyLock(true);
#endif
    
    int graphWidthSmall;
    int graphHeightSmall;
    mGraph->GetSize(&graphWidthSmall, &graphHeightSmall);

    // GUIResize
    int newGraphWidth = graphWidthSmall + offsetX;
    int newGraphHeight = graphHeightSmall + offsetY;
    
    mGraph->Resize(newGraphWidth, newGraphHeight);
    
    mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
    mGraph->SetClearColor(0, 0, 0, 255);

    if (mWaterfallRender != NULL)
        mWaterfallRender->SetGraph(mGraph);
    else
        mWaterfallRender = mWaterfallView->CreateWaterfallRender(mGraph);
    //mGraph->AddCustomDrawer(mWaterfallRender); // No need (and wrong)

    //
    mGraph->AddCustomDrawer(mCurveViewAmp);
    mGraph->AddCustomDrawer(mCurveViewPitch);
    mGraph->AddCustomDrawer(mCurveViewPartials);

    mGraph->AddCustomDrawer(mViewTitle);
    
    return mGraph;
}

void
MorphoWaterfallGUI::SetGraphEnabled(bool flag)
{
    if (mGraph != NULL)
    {
        mGraph->SetEnabled(flag);
        mGraph->SetInteractionDisabled(!flag);

        // To refresh well when changed track 
        if (flag)
            mGraph->SetDataChanged();
    }
}

void
MorphoWaterfallGUI::SetWaterfallViewMode(WaterfallViewMode mode)
{
    MorphoWaterfallView::DisplayMode displayMode;

    switch (mode)
    {
        case MORPHO_WATERFALL_VIEW_AMP:
        {
            displayMode = MorphoWaterfallView::AMP;
            mViewTitle->SetTitle("amplitude");
        }
        break;

        case MORPHO_WATERFALL_VIEW_HARMONIC:
        {
            displayMode = MorphoWaterfallView::HARMONIC;
            mViewTitle->SetTitle("harmonic");
        }
        break;

        case MORPHO_WATERFALL_VIEW_NOISE:
        {
            displayMode = MorphoWaterfallView::NOISE;
            mViewTitle->SetTitle("noise");
        }
        break;
            
        case MORPHO_WATERFALL_VIEW_DETECTION:
        {
            displayMode = MorphoWaterfallView::DETECTION;
            mViewTitle->SetTitle("detection");
        }
        break;
            
        case MORPHO_WATERFALL_VIEW_TRACKING:
        {
            displayMode = MorphoWaterfallView::TRACKING;
            mViewTitle->SetTitle("tracking");
        }
        break;
            
        case MORPHO_WATERFALL_VIEW_COLOR:
        {
            displayMode = MorphoWaterfallView::COLOR;
            mViewTitle->SetTitle("color");
        }
        break;
            
        case MORPHO_WATERFALL_VIEW_WARPING:
        {
            displayMode = MorphoWaterfallView::WARPING;
            mViewTitle->SetTitle("warping");
        }
        break;

        default:
            break;
    }
            
    mWaterfallView->SetDisplayMode(displayMode);
}

void
MorphoWaterfallGUI::SetWaterfallCameraAngle0(BL_FLOAT angle)
{
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetCamAngle0(angle);
}

void
MorphoWaterfallGUI::SetWaterfallCameraAngle1(BL_FLOAT angle)
{
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetCamAngle1(angle);
}

void
MorphoWaterfallGUI::SetWaterfallCameraFov(BL_FLOAT angle)
{
    if (mWaterfallRender != NULL)
        mWaterfallRender->SetCamFov(angle);
}

void
MorphoWaterfallGUI::ClearControls(IGraphics *pGraphics)
{
    // Hack to avoid a crash
    if (mGraph != NULL)
        mGraph->ClearLinkedObjects();
    
#if FIX_REMOVE_CONTROL_MORPHO || (defined APP_API)
    if ((mGraph != NULL) && (mGraphics != NULL))
        pGraphics->RemoveControl(mGraph);
#endif

    mGraph = NULL;
}

void
MorphoWaterfallGUI::FeedCurveViews(const vector<MorphoFrame7> &frames)
{
    if (frames.empty())
        return;

    bool applyFactors = (mPlugMode == MORPHO_PLUG_MODE_SYNTH);
                         
    // Push
    for (int i = 0; i < frames.size(); i++)
    {
        const MorphoFrame7 &frame = frames[i];

        BL_FLOAT amp = frame.GetAmplitude(applyFactors);
        mCurveViewAmp->PushData(amp);

        BL_FLOAT freq = frame.GetFrequency(applyFactors);

        BL_FLOAT pitch = ChromagramObj::FreqToChroma(freq, 440.0);
        mCurveViewPitch->PushData(pitch);
    }

    // Set
    const MorphoFrame7 &frame0 = frames[frames.size() - 1];

    vector<Partial2> partials;
    frame0.GetNormPartials(&partials, applyFactors);

    if (mCurveViewPartials->SpeedModCanAdd())
    {
        mCurveViewPartials->ResetData();
        for (int i = 0; i < partials.size(); i++)
        {
            const Partial2 &partial = partials[i];
            
            BL_FLOAT amp = partial.mAmp;
            BL_FLOAT freq = partial.mFreq;
            
            float t = freq;
            
            // Spread more
            t = pow(t, 0.5);
            
            mCurveViewPartials->AddData(amp, t);
        }
    }
}
