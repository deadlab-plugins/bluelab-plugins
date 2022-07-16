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

//#include <SpectrogramDisplay2.h>
#include <SpectrogramDisplay3.h>
#include <MiniView2.h>
#include <GhostTrack2.h>

#include "GhostCustomControl2.h"

#define WHEEL_ZOOM_STEP 0.025 //0.1

// 4 pixels
#define SELECTION_BORDER_SIZE 8

GhostCustomControl2::GhostCustomControl2(GhostTrack2 *track)
{
    mTrack = track;
    
    mPrevMouseDrag = false;
    
    mStartDrag[0] = 0;
    mStartDrag[1] = 0;
    
    mPrevMouseY = 0.0;
    
    mPrevMouseInsideSelect = false;
    
    mSelectionActive = false;
    
    for (int i = 0; i < 4; i++)
        mSelection[i] = 0.0;
    
    for (int i = 0; i < 4; i++)
        mBorderSelected[i] = false;
    
    mSpectroDisplay = NULL;
    
    mPrevMouseDownInsideSpectro = false;
    mPrevMouseDownInsideMiniView = false;
    
    mSelectionType = GhostTrack2::RECTANGLE;
    
    mPrevMouseDown = false;
    
    mMiniView = NULL;

    mMouseIsOver = false;

    mCursorListener = NULL;
}

void
GhostCustomControl2::SetCursorListener(CursorListener *listener)
{
    mCursorListener = listener;
}

void
GhostCustomControl2::Resize(int prevWidth, int prevHeight,
                            int newWidth, int newHeight)
{
    mSelection[0] = (((BL_FLOAT)mSelection[0])/prevWidth)*newWidth;
    mSelection[1] = (((BL_FLOAT)mSelection[1])/prevHeight)*newHeight;
    mSelection[2] = (((BL_FLOAT)mSelection[2])/prevWidth)*newWidth;
    mSelection[3] = (((BL_FLOAT)mSelection[3])/prevHeight)*newHeight;
}

void
GhostCustomControl2::SetSpectrogramDisplay(SpectrogramDisplay3 *spectroDisplay)
{
    mSpectroDisplay = spectroDisplay;
}

void
GhostCustomControl2::SetMiniView(MiniView2 *miniView)
{
    mMiniView = miniView;
}

void
GhostCustomControl2::
SetSelectionType(int selectionType)
{
    if (mSelectionType == selectionType)
        return;
    
    mSelectionType = selectionType;

#if 0
    // Update the selection !
    // For example, if we were in RECTANGULAR and we go in HORIZONTAL,
    // the selection will grow !
    UpdateSelectionType();
    mTrack->UpdateSelection(mSelection[0], mSelection[1],
                            mSelection[2], mSelection[3],
                            false);
#endif
}

void
GhostCustomControl2::OnMouseDown(float x, float y, const IMouseMod &pMod)
{
    mMouseIsOver = true;
    
    if (mSpectroDisplay == NULL)
        return;
    
    if (mMiniView == NULL)
        return;
    
    if (mTrack->GetMode() != GhostTrack2::EDIT)
        return;
    
    mPrevMouseDown = true;
    mPrevMouseY = y;
    
    int width;
    int height;
    mTrack->GetGraphSize(&width, &height);
    bool insideSpectro = mSpectroDisplay->PointInsideSpectrogram(x, y, width, height);
    if (insideSpectro)
    {
        mPrevMouseDownInsideSpectro = true;
    }
    else
    {
        mPrevMouseDownInsideSpectro = false;
    }
    
    //bool insideMiniView = mSpectroDisplay->PointInsideMiniView(x, y, width, height);
    bool insideMiniView = mMiniView->IsPointInside(x, y, width, height);
    if (insideMiniView)
    {
        mPrevMouseDownInsideMiniView = true;
    }
    else
    {
        mPrevMouseDownInsideMiniView = false;
    }
    
    //if (pMod->Cmd)
    if (pMod.C || pMod.Cmd) // NEW
        // Command pressed (or Control on Windows)
    {
        // We are dragging the spectrogram,
        // do not change the bar position
        
        return;
    }
    
    mPrevMouseDrag = false;
    
    if (insideSpectro)
    {
        mStartDrag[0] = x;
        mStartDrag[1] = y;
        
        // Check that if is the beginning of a drag from inside the selection.
        // In this case, move the selection.
        mPrevMouseInsideSelect = InsideSelection(x, y);
        
        // Try to select borders
        SelectBorders(x, y);
    }
}

void
GhostCustomControl2::OnMouseUp(float x, float y, const IMouseMod &pMod)
{
    mMouseIsOver = true;
    
    if (mTrack->GetMode() != GhostTrack2::EDIT)
        return;
    
    // FIX: click on the resize button, to a bigger size, then the
    // mouse up is detected inside the graph, without previous mouse down
    // (would put the bar in incorrect position)
    if (!mPrevMouseDown)
        return;
    mPrevMouseDown = false;
    
    //if (pMod->Cmd)
    if (pMod.C || pMod.Cmd) // NEW
        // Command pressed (or control on Windows)
    {
        // We are dragging the spectrogram,
        // do not change the bar position
        
        return;
    }

    mPrevMouseDownInsideSpectro = false;
    mPrevMouseDownInsideMiniView = false;
    
    int width;
    int height;
    mTrack->GetGraphSize(&width, &height);
    
    bool insideSpectro = mSpectroDisplay->PointInsideSpectrogram(x, y, width, height);
    if (insideSpectro)
    {
        if (!mPrevMouseDrag)
            // Pure mouse up
        {
            mTrack->SetBarActive(true);
            mTrack->SetBarPos(x);
    
            // Call this to "refresh"
            // Avoid jumps of the background when starting translation
            mTrack->UpdateZoom(1.0);
            
            // Set the play bar to origin
            mTrack->ResetPlayBar();
        
            mSelectionActive = false;
        }
    
        for (int i = 0; i < 4; i++)
            mBorderSelected[i] = false;
    }
}

void
GhostCustomControl2::OnMouseDrag(float x, float y, float dX, float dY,
                                 const IMouseMod &pMod)
{
    mMouseIsOver = true;

    if (mCursorListener != NULL)
        mCursorListener->CursorMoved(x, y);
    
    if (mTrack->GetMode() != GhostTrack2::EDIT)
        return;
    
    bool beginDrag = !mPrevMouseDrag;
    
    mPrevMouseDrag = true;
    
    int width;
    int height;
    mTrack->GetGraphSize(&width, &height);
    
    if (mPrevMouseDownInsideSpectro)
    {
        //if (pMod->Cmd)
        if (pMod.C || pMod.Cmd) // NEW
            // Command pressed (on Control on Windows)
        {
            // Drag the spectrogram
            mTrack->Translate(dX);
            mTrack->SetNeedRecomputeData(true);
            
            // For zoom with alt
            mStartDrag[0] = x;
            mPrevMouseY = y;
            
            return;
        }
    
        if (pMod.A)
            // Alt-drag => zoom
        {
#define DRAG_WHEEL_COEFF 0.2

            BL_FLOAT dY2 = y - mPrevMouseY;
            mPrevMouseY = y;
            
            dY2 *= -1.0;
            dY2 *= DRAG_WHEEL_COEFF;
            
            OnMouseWheel(mStartDrag[0], mStartDrag[1], pMod, dY2);
            
            return;
        }
        
        if (!mPrevMouseInsideSelect && !BorderSelected())
        {
            mSelection[0] = mStartDrag[0];
            mSelection[1] = mStartDrag[1];
            mSelection[2] = x;
            mSelection[3] = y;

            // Swap if necessary
            if (mSelection[0] > mSelection[2])
            {
                // Swap
                int tmp = mSelection[0];
                mSelection[0] = mSelection[2];
                mSelection[2] = tmp;
            }
        
            if (mSelection[1] > mSelection[3])
            {
                // Swap
                int tmp = mSelection[1];
                mSelection[1] = mSelection[3];
                mSelection[3] = tmp;
            }
            
            UpdateSelectionType();
            
            // Selection is active !
            // (could have been disactivated by putting the bar)
            mSelectionActive = true;

            mTrack->UpdateSelection(mSelection[0], mSelection[1],
                                   mSelection[2], mSelection[3],
                                   false, true);
        
            mTrack->ClearBar();
        
            // Set the play bar at the beginning of the new selection
            if (beginDrag)
                mTrack->ResetPlayBar();
            
            //mPlug->SelectionChanged();
        }
        else
        {
            //mPlug->BeforeSelTranslation();
            
            if (!BorderSelected())
            {
                // Move the selection
                mSelection[0] += dX;
                mSelection[1] += dY;
        
                mSelection[2] += dX;
                mSelection[3] += dY;
            }
            else
            // Modify the selection
            {
                if (mBorderSelected[0])
                    mSelection[0] += dX;
            
                if (mBorderSelected[1])
                    mSelection[1] += dY;
            
                if (mBorderSelected[2])
                    mSelection[2] += dX;
            
                if (mBorderSelected[3])
                    mSelection[3] += dY;
                
                //mPlug->SelectionChanged();
            }

#if 0
            UpdateSelectionType();
#endif
            
            mTrack->UpdateSelection(mSelection[0], mSelection[1],
                                    mSelection[2], mSelection[3],
                                    false, false/*true*/);
        
            //mPlug->AfterSelTranslation();
            
            // Set the play bar at the beginning of the new selection
            if (beginDrag)
                mTrack->ResetPlayBar();
        }
        
        // NEW: capture playbar inside selection
        if (mTrack->PlayBarOutsideSelection())
        {
            // Play bar is outside the selection,
            // rest it to the beginning of the selection
            //
            // This allows moving the selection, with play bar
            // "captured" inside the selection, on both sides
            mTrack->ResetPlayBar();
        }
    }
    
    if (mPrevMouseDownInsideMiniView)
    {
        //MiniView *miniView = mSpectroDisplay->GetMiniView();
        if (mMiniView != NULL)
        {
            BL_FLOAT drag = mMiniView->GetDrag(dX, width);
            
            drag *= width;
            
            // Drag the spectrogram
            mTrack->Translate(drag);
            mTrack->SetNeedRecomputeData(true);
        }
    }
}

//bool
void
GhostCustomControl2::OnMouseDblClick(float x, float y, const IMouseMod &pMod)
{
    mMouseIsOver = true;
    
    int width;
    int height;
    mTrack->GetGraphSize(&width, &height);
    
    //bool insideMiniView = mSpectroDisplay->PointInsideMiniView(x, y, width, height);
    bool insideMiniView = mMiniView->IsPointInside(x, y, width, height);
    
    if (insideMiniView)
        mTrack->RewindView();
    
    //return true;
}

void
GhostCustomControl2::OnMouseWheel(float x, float y, const IMouseMod &pMod, float d)
{
    mMouseIsOver = true;
    
    if (mTrack->GetMode() != GhostTrack2::EDIT)
        return;
    
    // Don't check insideSpectro here
    // We just need the focus anywhere...
    
#if ZOOM_ON_POINTER
    // Set the zoom center as the mouse position and not
    // at the bar position
    mTrack->SetZoomCenter(x);
#endif
    
    BL_FLOAT zoomChange = 1.0 + d*WHEEL_ZOOM_STEP;
    mTrack->UpdateZoom(zoomChange);

#if 0
    // For hozizontal selection, grow the selection to the
    // extremities if necessary when zooming
    UpdateSelectionType();
#endif
    
    bool isSelectionActive = mTrack->IsSelectionActive();
        
    mTrack->UpdateSelection(mSelection[0], mSelection[1],
                           mSelection[2], mSelection[3],
                           false, isSelectionActive);
    
    mTrack->SetNeedRecomputeData(true);
}

void
GhostCustomControl2::OnMouseOver(float x, float y, const IMouseMod &pMod)
{
    if (mCursorListener != NULL)
        mCursorListener->CursorMoved(x, y);

    mMouseIsOver = true;
}

void
GhostCustomControl2::OnMouseOut()
{
    if (mCursorListener != NULL)
        mCursorListener->CursorOut();

    mMouseIsOver = false;
}

bool
GhostCustomControl2::InsideSelection(int x, int y)
{
    if (!mSelectionActive)
        return false;
    
    if (x < mSelection[0])
        return false;
    
    if (y < mSelection[1])
        return false;
    
    if (x > mSelection[2])
        return false;
    
    if (y > mSelection[3])
        return false;
    
    return true;
}

void
GhostCustomControl2::SelectBorders(int x, int y)
{
    // FIX: avoid selecting selection rectangle border when it is not visible !
    // Do not try to select a border if only the play bar is visible
    bool barActive = mTrack->IsBarActive();
    if (barActive)
        return;
    
    for (int i = 0; i < 4; i++)
        mBorderSelected[i] = false;
    
    int dist0 = abs(x - (int)mSelection[0]);
    if ((dist0 < SELECTION_BORDER_SIZE) && // near x
        (y > mSelection[1]) && (y < mSelection[3])) // on the interval of y
        mBorderSelected[0] = true;
    
    int dist1 = abs(y - (int)mSelection[1]);
    if ((dist1 < SELECTION_BORDER_SIZE) && // near y
        (x > mSelection[0]) && (x < mSelection[2])) // on the interval of x
        mBorderSelected[1] = true;
    
    int dist2 = abs(x - (int)mSelection[2]);
    if ((dist2 < SELECTION_BORDER_SIZE) && // near x
        (y > mSelection[1]) && (y < mSelection[3])) // on the interval of y
        mBorderSelected[2] = true;
    
    int dist3 = abs(y - (int)mSelection[3]);
    if ((dist3 < SELECTION_BORDER_SIZE) && // near y
        (x > mSelection[0]) && (x < mSelection[2])) // on the interval of x
        mBorderSelected[3] = true;
}

bool
GhostCustomControl2::BorderSelected()
{
    for (int i = 0; i < 4; i++)
    {
        if (mBorderSelected[i])
            return true;
    }
    
    return false;
}

/*void
GhostCustomControl2::UpdateZoomSelection(BL_FLOAT zoomChange)
{
    mPlug->UpdateZoomSelection(mSelection, zoomChange);
}*/

void
GhostCustomControl2::UpdateZoom(BL_FLOAT zoomChange)
{
    mTrack->UpdateZoom(zoomChange);
}

void
GhostCustomControl2::GetSelection(BL_FLOAT selection[4])
{
    for (int i = 0; i < 4; i++)
        selection[i] = mSelection[i];
}

void
GhostCustomControl2::SetSelection(BL_FLOAT x0, BL_FLOAT y0,
                                  BL_FLOAT x1, BL_FLOAT y1)
{
    mSelection[0] = x0;
    mSelection[1] = y0;
    mSelection[2] = x1;
    mSelection[3] = y1;
}

void
GhostCustomControl2::SetSelectionActive(bool flag)
{
    mSelectionActive = flag;
}

bool
GhostCustomControl2::GetSelectionActive()
{
    return mSelectionActive;
}

void
GhostCustomControl2::UpdateSelection(bool updateCenterPos)
{
    mTrack->UpdateSelection(mSelection[0], mSelection[1],
                            mSelection[2], mSelection[3],
                            updateCenterPos);
}

bool
GhostCustomControl2::IsMouseOver() const
{
    return mMouseIsOver;
}
    
void
GhostCustomControl2::UpdateSelectionType()
{
    if (mSelectionType == GhostTrack2::RECTANGLE)
        return;
    
    int width;
    int height;
    mTrack->GetGraphSize(&width, &height);
    
    if (mSelectionType == GhostTrack2::HORIZONTAL)
    {
        mSelection[0] = 0;
        mSelection[2] = width;
        
        return;
    }
    
    if (mSelectionType == GhostTrack2::VERTICAL)
    {
        mSelection[1] = 0;
        mSelection[3] = height;
        
        return;
    }
}

#endif
