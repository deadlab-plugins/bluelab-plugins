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
//  MorphoWaterfallRender.cpp
//  BL-Waves
//
//  Created by applematuer on 10/13/18.
//
//

#ifdef IGRAPHICS_NANOVG

#include <Axis3D.h>

#include <BLUtils.h>
#include <BLDebug.h>

// Include for flag for partials debug
#include <MorphoWaterfallView.h>

#include <View3DPluginInterface.h>

#include "MorphoWaterfallRender.h"

// Axis drawing
#define AXIS_OFFSET_Z 0.06

// Artificially modify the coeff, to increase the spread on the grid
#define MEL_COEFF 4.0

// Do noty skpi any slices
#define DBG_DISPLAY_ALL_SLICES 0

MorphoWaterfallRender::MorphoWaterfallRender(GraphControl12 *graphControl)
{
    mView3DListener = NULL;
        
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
    {
        mLinesRenders[i] = new LinesRender2();
        mLinesRenders[i]->SetMode(LinesRender2::LINES_FREQ);

        mLinesRenders[i]->SetUseLegacyLock(true);

#if DBG_DISPLAY_ALL_SLICES
        mLinesRenders[i]->DBG_SetDisplayAllSlices();
#endif

        // Fix, to have the detection points synchronized to the lines
        mLinesRenders[i]->DBG_FixDensityNumSlices();

        // Max density
        mLinesRenders[i]->SetDensity(1.0);
    }
    
    mCurrentMode = MorphoWaterfallView::TRACKING;
    
    SetGraph(graphControl);
    
    mMouseIsDown = false;
    mPrevDrag[0] = 0;
    mPrevDrag[1] = 0;
    
    mPrevMouseDrag = false;
    
    // Rotation of the camera
    mCamAngle0 = 0.0;
    mCamAngle1 = MIN_CAM_ANGLE_1;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);

    const BL_FLOAT magnsScale = 0.3;
    mLinesRenders[MorphoWaterfallView::AMP]->SetScale(magnsScale);
    mLinesRenders[MorphoWaterfallView::HARMONIC]->SetScale(magnsScale);
    mLinesRenders[MorphoWaterfallView::NOISE]->SetScale(magnsScale);
    mLinesRenders[MorphoWaterfallView::DETECTION]->SetScale(magnsScale);
    mLinesRenders[MorphoWaterfallView::TRACKING]->SetScale(magnsScale);
    mLinesRenders[MorphoWaterfallView::COLOR]->SetScale(magnsScale);
    
    mAddNum = 0;
}

MorphoWaterfallRender::~MorphoWaterfallRender()
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        delete mLinesRenders[i];
}

void
MorphoWaterfallRender::SetGraph(GraphControl12 *graphControl)
{
    mGraph = graphControl;
    
    if (mGraph != NULL)
    {
        mGraph->AddCustomControl(this);
        mGraph->AddCustomDrawer(mLinesRenders[(int)mCurrentMode]);
    }
}

void
MorphoWaterfallRender::SetView3DListener(View3DPluginInterface *view3DListener)
{
    mView3DListener = view3DListener;
}

void
MorphoWaterfallRender::Clear()
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->ClearSlices();

    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::AddData(MorphoWaterfallView::DisplayMode mode,
                               const WDL_TypedBuf<BL_FLOAT> &data)
{
    if (data.GetSize() == 0)
        return;
    
    vector<LinesRender2::Point> &points = mTmpBuf0;
    DataToPoints(&points, data);
    
    mLinesRenders[(int)mode]->AddSlice(points);
    
    mAddNum++;
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::AddPoints(MorphoWaterfallView::DisplayMode mode,
                                 const vector<LinesRender2::Point> &points)
{
    mLinesRenders[(int)mode]->AddSlice(points);
    
    mAddNum++;
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetLineMode(MorphoWaterfallView::DisplayMode mode,
                                   LinesRender2::Mode lineMode)
{
   mLinesRenders[(int)mode]->SetMode(lineMode);
}

void
MorphoWaterfallRender::OnMouseDown(float x, float y, const IMouseMod &mod)
{
    mMouseIsDown = true;
    
    mPrevDrag[0] = x;
    mPrevDrag[1] = y;
    
    mPrevMouseDrag = false;
}

void
MorphoWaterfallRender::OnMouseUp(float x, float y, const IMouseMod &mod)
{
    if (!mMouseIsDown)
        return;
    
    mMouseIsDown = false;
}

void
MorphoWaterfallRender::OnMouseDrag(float x, float y, float dX, float dY,
                                   const IMouseMod &mod)
{
    if (mod.A)
    // Alt-drag => zoom
    {
        if (!mPrevMouseDrag)
        {
            mPrevMouseDrag = true;
            
            mPrevDrag[0] = y;
            
            return;
        }
        
#define DRAG_WHEEL_COEFF 0.2
        
        BL_FLOAT dY2 = y - mPrevDrag[0];
        mPrevDrag[0] = y;
        
        dY2 *= -1.0;
        dY2 *= DRAG_WHEEL_COEFF;
        
        OnMouseWheel(mPrevDrag[0], mPrevDrag[1], mod, dY2);
        
        return;
    }
    
    mPrevMouseDrag = true;
    
    // Move camera
    float dragX = x - mPrevDrag[0];
    float dragY = y - mPrevDrag[1];
    
    mPrevDrag[0] = x;
    mPrevDrag[1] = y;
    
    mCamAngle0 += dragX;
    mCamAngle1 += dragY;
    
    // Bounds
    if (mCamAngle0 < -MAX_CAM_ANGLE_0)
        mCamAngle0 = -MAX_CAM_ANGLE_0;
    if (mCamAngle0 > MAX_CAM_ANGLE_0)
        mCamAngle0 = MAX_CAM_ANGLE_0;
    
    if (mCamAngle1 < MIN_CAM_ANGLE_1)
        mCamAngle1 = MIN_CAM_ANGLE_1;
    if (mCamAngle1 > MAX_CAM_ANGLE_1)
        mCamAngle1 = MAX_CAM_ANGLE_1;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    // Camera changed
    //
    // Without that, the camera point of view is not modified if
    // the sound is not playing
    if (mGraph != NULL)
        mGraph->SetDataChanged();

    if (mView3DListener != NULL)
        mView3DListener->SetCameraAngles(mCamAngle0, mCamAngle1);
}

void
MorphoWaterfallRender::OnMouseDblClick(float x, float y, const IMouseMod &mod)
{
    // Reset the view
    mCamAngle0 = WATERFALL_DEFAULT_CAM_ANGLE0;
    mCamAngle1 = WATERFALL_DEFAULT_CAM_ANGLE1;
    BL_FLOAT camFov = WATERFALL_DEFAULT_CAM_FOV;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    // Reset the fov
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        //mLinesRenders[i]->ResetZoom();
        mLinesRenders[i]->SetCameraFov(camFov);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();

    if (mView3DListener != NULL)
        mView3DListener->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    BL_FLOAT fov = mLinesRenders[0]->GetCameraFov();
    if (mView3DListener != NULL)
        mView3DListener->SetCameraFov(fov);
}

void
MorphoWaterfallRender::OnMouseWheel(float x, float y,
                                    const IMouseMod &mod, float d)
{
#define WHEEL_ZOOM_STEP 0.025
    
    BL_FLOAT zoomChange = 1.0 + d*WHEEL_ZOOM_STEP;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->ZoomChanged(zoomChange);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
    
    BL_FLOAT angle = mLinesRenders[0]->GetCameraFov();
    mView3DListener->SetCameraFov(angle);
}

void
MorphoWaterfallRender::SetSpeed(BL_FLOAT speed)
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetSpeed(speed);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetSpeedMod(int speedMod)
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetSpeedMod(speedMod);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetDensity(BL_FLOAT density)
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetDensity(density);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetScale(BL_FLOAT scale)
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetScale(scale);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetCamAngle0(BL_FLOAT angle)
{
    mCamAngle0 = angle;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetCamAngle1(BL_FLOAT angle)
{
    mCamAngle1 = angle;
    
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
MorphoWaterfallRender::SetCamFov(BL_FLOAT angle)
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraFov(angle);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

int
MorphoWaterfallRender::GetNumSlices()
{
    int numSlices = (int)mLinesRenders[0]->GetNumSlices();
    
    return numSlices;
}

void
MorphoWaterfallRender::SetMode(MorphoWaterfallView::DisplayMode mode)
{
    if (mGraph != NULL)
        mGraph->RemoveCustomDrawer(mLinesRenders[mCurrentMode]);
    
    mCurrentMode = mode;
    
    if (mGraph != NULL)
        mGraph->AddCustomDrawer(mLinesRenders[mCurrentMode]);
}

int
MorphoWaterfallRender::GetSpeed()
{
    int speed = mLinesRenders[0]->GetSpeed();
    
    return speed;
}

void
MorphoWaterfallRender::SetAdditionalLines(MorphoWaterfallView::DisplayMode mode,
                                          const vector<LinesRender2::Line> &lines,
                                          BL_FLOAT lineWidth)
{
    mLinesRenders[(int)mode]->SetAdditionalLines(lines, lineWidth);
}

void
MorphoWaterfallRender::ClearAdditionalLines()
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->ClearAdditionalLines();
}

void
MorphoWaterfallRender::ShowAdditionalLines(MorphoWaterfallView::DisplayMode mode,
                                           bool flag)
{
    mLinesRenders[(int)mode]->ShowAdditionalLines(flag);
}

void
MorphoWaterfallRender::SetAdditionalPoints(MorphoWaterfallView::DisplayMode mode,
                                           const vector<LinesRender2::Line> &lines,
                                           BL_FLOAT lineWidth, bool optimSameColor)
{
    mLinesRenders[(int)mode]->SetAdditionalPointsOptimSameColor(optimSameColor);
    
    mLinesRenders[(int)mode]->SetAdditionalPoints(lines, lineWidth);
}

void
MorphoWaterfallRender::ClearAdditionalPoints()
{
    for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
        mLinesRenders[i]->ClearAdditionalPoints();
}

void
MorphoWaterfallRender::ShowAdditionalPoints(MorphoWaterfallView::DisplayMode mode,
                                            bool flag)
{
    mLinesRenders[(int)mode]->ShowAdditionalPoints(flag);
}

void
MorphoWaterfallRender::DBG_SetNumSlices(int numSlices)
{
     for (int i = 0; i < (int)MorphoWaterfallView::NUM_MODES; i++)
     {
         mLinesRenders[i]->SetNumSlices(numSlices);
         mLinesRenders[i]->DBG_ForceDensityNumSlices();
     }
}

void
MorphoWaterfallRender::DataToPoints(vector<LinesRender2::Point> *points,
                                    const WDL_TypedBuf<BL_FLOAT> &data)
{
    if (data.GetSize() == 0)
        return;
    
    // Convert to points
    points->resize(data.GetSize());
    
    // Optim
    BL_FLOAT xCoeff = 0.0;
    if (data.GetSize() > 0)
    {
        if (data.GetSize() <= 1)
            xCoeff = 1.0/data.GetSize();
        else
            xCoeff = 1.0/(data.GetSize() - 1);
    }
    
    // Loop
    for (int i = 0; i < data.GetSize(); i++)
    {
        BL_FLOAT magn = data.Get()[i];
        
        LinesRender2::Point &p = (*points)[i];
        
        p.mX = xCoeff*i - 0.5;
        p.mY = magn;
        
        // Fill with dummy Z (to avoid undefined value)
        p.mZ = 0.0;
        
        // Fill with dummy color (to avoid undefined value)
        p.mR = 0;
        p.mG = 0;
        p.mB = 0;
        p.mA = 0;
    }
}

#endif // IGRAPHICS_NANOVG
