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

#include <GhostTrack2.h>
#include <GraphSwapColor.h>

#include "GhostCustomDrawer2.h"

GhostCustomDrawer2::GhostCustomDrawer2(GhostTrack2 *track,
                                       BL_FLOAT x0, BL_FLOAT y0,
                                       BL_FLOAT x1, BL_FLOAT y1,
                                       State *state)
{
    mState = state;
    if (mState == NULL)
    {
        mState = new State();;
    
        mState->mBarActive = false;
        mState->mBarPos = 0.0;
    
        mState->mPlayBarActive = false;
        mState->mPlayBarPos = 0.0;
    
        mState->mSelectionActive = false;
    
        for (int i = 0; i < 4; i++)
            mState->mSelection[i] = 0.0;
    }
    
    // Warning: y is reversed ??
    mBounds[0] = x0;
    mBounds[1] = y0;
    mBounds[2] = x1;
    mBounds[3] = y1;
    
    mTrack = track;

    mNeedRedraw = true;
}

GhostCustomDrawer2::State *
GhostCustomDrawer2::GetState()
{
    return mState;
}

void
GhostCustomDrawer2::PostDraw(NVGcontext *vg, int width, int height)
{
    DrawBar(vg, width, height);
    
    DrawSelection(vg, width, height);
    
    DrawPlayBar(vg, width, height);

    mNeedRedraw = false;
}

void
GhostCustomDrawer2::ClearBar()
{
    mState->mBarActive = false;

    mNeedRedraw = true;
}

void
GhostCustomDrawer2::ClearSelection()
{
    mState->mSelectionActive = false;

    mNeedRedraw = true;
}

void
GhostCustomDrawer2::SetBarPos(BL_FLOAT pos)
{
    mState->mBarPos = pos;
    
    // Set play bar to the same position, to start
    mState->mPlayBarPos = pos;
    
    mState->mBarActive = true;
    mState->mSelectionActive = false;

    mNeedRedraw = true;
}

void
GhostCustomDrawer2::SetSelectionActive(bool flag)
{
    mState->mSelectionActive = flag;

    mNeedRedraw = true;
}

bool
GhostCustomDrawer2::GetSelectionActive()
{
    return mState->mSelectionActive;
}

BL_FLOAT
GhostCustomDrawer2::GetBarPos()
{
    return mState->mBarPos;
}

void
GhostCustomDrawer2::SetBarActive(bool flag)
{
    mState->mBarActive = flag;

    mNeedRedraw = true;
}

bool
GhostCustomDrawer2::IsBarActive()
{
    return mState->mBarActive;
}

void
GhostCustomDrawer2::SetSelection(BL_FLOAT x0, BL_FLOAT y0,
                                 BL_FLOAT x1, BL_FLOAT y1)
{
    // Bound only y
    // For x, we may want to select sound outside the view
    // and play it
    
    if (y0 < mBounds[1])
        y0 = mBounds[1];
    if (y0 > mBounds[3]) // Selection can be reversed
        y0 = mBounds[3];
    if (y1 < mBounds[1]) // Selection can be reversed
        y1 = mBounds[1];
    if (y1 > mBounds[3])
        y1 = mBounds[3];
        
    mState->mSelection[0] = x0;
    mState->mSelection[1] = y0;
    mState->mSelection[2] = x1;
    mState->mSelection[3] = y1;

    mNeedRedraw = true;
}

void
GhostCustomDrawer2::GetSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                 BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = mState->mSelection[0];
    *y0 = mState->mSelection[1];
    
    *x1 = mState->mSelection[2];
    *y1 = mState->mSelection[3];
}

void
//GhostCustomDrawer2::UpdateZoomSelection(BL_FLOAT zoomChange)
GhostCustomDrawer2::UpdateZoom(BL_FLOAT zoomChange)
{
    //mPlug->UpdateZoomSelection(mState->mSelection, zoomChange);
    mTrack->UpdateZoom(zoomChange);

    mNeedRedraw = true;
}

bool
GhostCustomDrawer2::IsSelectionActive()
{
    return mState->mSelectionActive;
}

BL_FLOAT
GhostCustomDrawer2::GetPlayBarPos()
{
    return mState->mPlayBarPos;
}

void
GhostCustomDrawer2::SetPlayBarPos(BL_FLOAT pos, bool activate)
{
    mState->mPlayBarPos = pos;
    
    if (activate)
        mState->mPlayBarActive = true;

    mNeedRedraw = true;
}

bool
GhostCustomDrawer2::IsPlayBarActive()
{
    return mState->mPlayBarActive;
}

void
GhostCustomDrawer2::SetPlayBarActive(bool flag)
{
    mState->mPlayBarActive = flag;

    mNeedRedraw = true;
}

void
GhostCustomDrawer2::SetSelPlayBarPos(BL_FLOAT pos)
{
    mState->mPlayBarPos = mState->mSelection[0] +
                        pos*(mState->mSelection[2] - mState->mSelection[0]);
    
    mState->mPlayBarActive = true;

    mNeedRedraw = true;
}

bool
GhostCustomDrawer2::NeedRedraw()
{
    return mNeedRedraw;
}

void
GhostCustomDrawer2::DrawBar(NVGcontext *vg, int width, int height)
{
    if (!mState->mBarActive)
        return;
    
    BL_FLOAT strokeWidths[2] = { 3.0, 2.0 };
    
    // Two colors, for drawing two times, for overlay
    int colors[2][4] = { { 64, 64, 64, 255 }, { 255, 255, 255, 255 } };
    
    for (int i = 0; i < 2; i++)
    {
        nvgStrokeWidth(vg, strokeWidths[i]);
        
        SWAP_COLOR(colors[i]);
        nvgStrokeColor(vg, nvgRGBA(colors[i][0], colors[i][1],
                                   colors[i][2], colors[i][3]));
    
        // Draw the circle
        nvgBeginPath(vg);
    
        // Draw the line
        BL_FLOAT x = mState->mBarPos*width;
    
        //nvgMoveTo(vg, x, (1.0 - mBounds[1])*height);
        //nvgLineTo(vg, x, (1.0 - mBounds[3])*height);
        nvgMoveTo(vg, x, mBounds[1]*height);
        nvgLineTo(vg, x, mBounds[3]*height);
        
        nvgStroke(vg);
    }
}

void
GhostCustomDrawer2::DrawSelection(NVGcontext *vg, int width, int height)
{    
    if (!mState->mSelectionActive)
        return;
    
    BL_FLOAT strokeWidths[2] = { 3.0, 2.0 };
    
    // Two colors, for drawing two times, for overlay
    int colors[2][4] = { { 64, 64, 64, 255 }, { 255, 255, 255, 255 } };
    
    for (int i = 0; i < 2; i++)
    {
        nvgStrokeWidth(vg, strokeWidths[i]);
    
        SWAP_COLOR(colors[i]);
        nvgStrokeColor(vg, nvgRGBA(colors[i][0], colors[i][1],
                                   colors[i][2], colors[i][3]));
    
        // Draw the circle
        nvgBeginPath(vg);
    
        // Draw the line
        /*nvgMoveTo(vg, mSelection[0]*width, (1.0 - mSelection[1])*height);
    
        nvgLineTo(vg, mSelection[2]*width, (1.0 - mSelection[1])*height);
        nvgLineTo(vg, mSelection[2]*width, (1.0 - mSelection[3])*height);
        nvgLineTo(vg, mSelection[0]*width, (1.0 - mSelection[3])*height);
        nvgLineTo(vg, mSelection[0]*width, (1.0 - mSelection[1])*height); */
        
        nvgMoveTo(vg, mState->mSelection[0]*width, mState->mSelection[1]*height);
        
        nvgLineTo(vg, mState->mSelection[2]*width, mState->mSelection[1]*height);
        nvgLineTo(vg, mState->mSelection[2]*width, mState->mSelection[3]*height);
        nvgLineTo(vg, mState->mSelection[0]*width, mState->mSelection[3]*height);
        nvgLineTo(vg, mState->mSelection[0]*width, mState->mSelection[1]*height);
    
        nvgStroke(vg);
    }
}

void
GhostCustomDrawer2::DrawPlayBar(NVGcontext *vg, int width, int height)
{
    if (!mState->mPlayBarActive)
        return;
    
    BL_FLOAT strokeWidths[2] = { 2.0, 1.0 };
   
    
    // Two colors, for drawing two times, for overlay
    int colors[2][4] = { { 64, 64, 64, 255 }, { 255, 255, 255, 255 } };
   
    for (int i = 0; i < 2; i++)
    {
        nvgStrokeWidth(vg, strokeWidths[i]);
    
        SWAP_COLOR(colors[i]);
        nvgStrokeColor(vg, nvgRGBA(colors[i][0], colors[i][1],
                                   colors[i][2], colors[i][3]));
    
        // Draw the bar
        nvgBeginPath(vg);
    
        // Draw the line
        BL_FLOAT x = mState->mPlayBarPos*width;
    
        //nvgMoveTo(vg, x, (1.0 - mBounds[1])*height);
        //nvgLineTo(vg, x, (1.0 - mBounds[3])*height);
        
        nvgMoveTo(vg, x, mBounds[1]*height);
        nvgLineTo(vg, x, mBounds[3]*height);
        
        nvgStroke(vg);
    }
}

#endif
