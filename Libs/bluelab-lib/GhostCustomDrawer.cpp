#ifdef IGRAPHICS_NANOVG

#include <GraphSwapColor.h>

#include "GhostCustomDrawer.h"

GhostCustomDrawer::GhostCustomDrawer(GhostPluginInterface *plug,
                                     BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1,
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
    
    mPlug = plug;
}

GhostCustomDrawer::State *
GhostCustomDrawer::GetState()
{
    return mState;
}

void
GhostCustomDrawer::PostDraw(NVGcontext *vg, int width, int height)
{
    DrawBar(vg, width, height);
    
    DrawSelection(vg, width, height);
    
    DrawPlayBar(vg, width, height);
}

void
GhostCustomDrawer::ClearBar()
{
    mState->mBarActive = false;
}

void
GhostCustomDrawer::ClearSelection()
{
    mState->mSelectionActive = false;
}

void
GhostCustomDrawer::SetBarPos(BL_FLOAT pos)
{
    mState->mBarPos = pos;
    
    // Set play bar to the same position, to start
    mState->mPlayBarPos = pos;
    
    mState->mBarActive = true;
    mState->mSelectionActive = false;
}

void
GhostCustomDrawer::SetSelectionActive(bool flag)
{
    mState->mSelectionActive = flag;
}

BL_FLOAT
GhostCustomDrawer::GetBarPos()
{
    return mState->mBarPos;
}

void
GhostCustomDrawer::SetBarActive(bool flag)
{
    mState->mBarActive = flag;
}

bool
GhostCustomDrawer::IsBarActive()
{
    return mState->mBarActive;
}

void
GhostCustomDrawer::SetSelection(BL_FLOAT x0, BL_FLOAT y0,
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
}

void
GhostCustomDrawer::GetSelection(BL_FLOAT *x0, BL_FLOAT *y0,
                                BL_FLOAT *x1, BL_FLOAT *y1)
{
    *x0 = mState->mSelection[0];
    *y0 = mState->mSelection[1];
    
    *x1 = mState->mSelection[2];
    *y1 = mState->mSelection[3];
}

void
GhostCustomDrawer::UpdateZoomSelection(BL_FLOAT zoomChange)
{
    //mPlug->UpdateZoomSelection(mState->mSelection, zoomChange);
    mPlug->UpdateZoom(zoomChange);
}

bool
GhostCustomDrawer::IsSelectionActive()
{
    return mState->mSelectionActive;
}

BL_FLOAT
GhostCustomDrawer::GetPlayBarPos()
{
    return mState->mPlayBarPos;
}

void
GhostCustomDrawer::SetPlayBarPos(BL_FLOAT pos, bool activate)
{
    mState->mPlayBarPos = pos;
    
    if (activate)
        mState->mPlayBarActive = true;
}

bool
GhostCustomDrawer::IsPlayBarActive()
{
    return mState->mPlayBarActive;
}

void
GhostCustomDrawer::SetPlayBarActive(bool flag)
{
    mState->mPlayBarActive = flag;
}

void
GhostCustomDrawer::SetSelPlayBarPos(BL_FLOAT pos)
{
    mState->mPlayBarPos = mState->mSelection[0] +
                        pos*(mState->mSelection[2] - mState->mSelection[0]);
    
    mState->mPlayBarActive = true;
}

void
GhostCustomDrawer::DrawBar(NVGcontext *vg, int width, int height)
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
        nvgStrokeColor(vg, nvgRGBA(colors[i][0], colors[i][1], colors[i][2], colors[i][3]));
    
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
GhostCustomDrawer::DrawSelection(NVGcontext *vg, int width, int height)
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
        nvgStrokeColor(vg, nvgRGBA(colors[i][0], colors[i][1], colors[i][2], colors[i][3]));
    
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
GhostCustomDrawer::DrawPlayBar(NVGcontext *vg, int width, int height)
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
