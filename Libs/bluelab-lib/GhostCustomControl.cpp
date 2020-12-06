#include "GhostCustomControl.h"

GhostCustomControl::GhostCustomControl(GhostPluginInterface *plug)
{
    mPlug = plug;
    
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
    
    mSelectionType = Ghost::RECTANGLE;
    
    mPrevMouseDown = false;
}

void
GhostCustomControl::Resize(int prevWidth, int prevHeight,
                           int newWidth, int newHeight)
{
    mSelection[0] = (((BL_FLOAT)mSelection[0])/prevWidth)*newWidth;
    mSelection[1] = (((BL_FLOAT)mSelection[1])/prevHeight)*newHeight;
    mSelection[2] = (((BL_FLOAT)mSelection[2])/prevWidth)*newWidth;
    mSelection[3] = (((BL_FLOAT)mSelection[3])/prevHeight)*newHeight;
}

void
GhostCustomControl::SetSpectrogramDisplay(SpectrogramDisplay *spectroDisplay)
{
    mSpectroDisplay = spectroDisplay;
}

void
GhostCustomControl::SetSelectionType(Ghost::SelectionType selectionType)
{
    mSelectionType = selectionType;
    
    // Update the selection !
    // For example, if we were in RECTANGULAR and we go in HORIZONTAL,
    // the selection will grow !
    UpdateSelectionType();
    mPlug->UpdateSelection(mSelection[0], mSelection[1],
                           mSelection[2], mSelection[3],
                           false);

}

void
GhostCustomControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
    if (mPlug->GetMode() != Ghost::EDIT)
        return;
    
    mPrevMouseDown = true;
    mPrevMouseY = y;
    
    int width;
    int height;
    mPlug->GetGraphSize(&width, &height);
    bool insideSpectro = mSpectroDisplay->PointInsideSpectrogram(x, y, width, height);
    if (insideSpectro)
    {
        mPrevMouseDownInsideSpectro = true;
    }
    else
    {
        mPrevMouseDownInsideSpectro = false;
    }
    
    bool insideMiniView = mSpectroDisplay->PointInsideMiniView(x, y, width, height);
    if (insideMiniView)
    {
        mPrevMouseDownInsideMiniView = true;
    }
    else
    {
        mPrevMouseDownInsideMiniView = false;
    }
    
    if (pMod->Cmd)
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
GhostCustomControl::OnMouseUp(int x, int y, IMouseMod* pMod)
{
    if (mPlug->GetMode() != Ghost::EDIT)
        return;
    
    // FIX: click on the resize button, to a bigger size, then the
    // mouse up is detected inside the graph, without previous mouse down
    // (would put the bar in incorrect position)
    if (!mPrevMouseDown)
        return;
    mPrevMouseDown = false;
    
    if (pMod->Cmd)
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
    mPlug->GetGraphSize(&width, &height);
    
    bool insideSpectro = mSpectroDisplay->PointInsideSpectrogram(x, y, width, height);
    if (insideSpectro)
    {
        if (!mPrevMouseDrag)
            // Pure mouse up
        {
            mPlug->SetBarActive(true);
            mPlug->SetBarPos(x);
    
            // Call this to "refresh"
            // Avoid jumps of the background when starting translation
            mPlug->UpdateZoom(1.0);
            
            // Set the play bar to origin
            mPlug->ResetPlayBar();
        
            mSelectionActive = false;
        }
    
        for (int i = 0; i < 4; i++)
            mBorderSelected[i] = false;
    }
}

void
GhostCustomControl::OnMouseDrag(int x, int y, int dX, int dY, IMouseMod* pMod)
{
    if (mPlug->GetMode() != Ghost::EDIT)
        return;
    
    bool beginDrag = !mPrevMouseDrag;
    
    mPrevMouseDrag = true;
    
    int width;
    int height;
    mPlug->GetGraphSize(&width, &height);
    
    if (mPrevMouseDownInsideSpectro)
    {
        if (pMod->Cmd)
            // Command pressed (on Control on Windows)
        {
            // Drag the spectrogram
            mPlug->Translate(dX);
            mPlug->SetNeedRecomputeData(true);
            
            // For zoom with alt
            mStartDrag[0] = x;
            mPrevMouseY = y;
            
            return;
        }
    
        if (pMod->A)
            // Alt-drag => zoom
        {
#define DRAG_WHEEL_COEFF 0.2

            BL_FLOAT dY = y - mPrevMouseY;
            mPrevMouseY = y;
            
            dY *= -1.0;
            dY *= DRAG_WHEEL_COEFF;
            
            OnMouseWheel(mStartDrag[0], mStartDrag[1], pMod, dY);
            
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

            mPlug->UpdateSelection(mSelection[0], mSelection[1],
                                   mSelection[2], mSelection[3],
                                   false, true);
        
            mPlug->ClearBar();
        
            // Set the play bar at the beginning of the new selection
            if (beginDrag)
                mPlug->ResetPlayBar();
            
            mPlug->SelectionChanged();
        }
        else
        {
            mPlug->BeforeSelTranslation();
            
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
                
                mPlug->SelectionChanged();
            }
        
            UpdateSelectionType();
            
            mPlug->UpdateSelection(mSelection[0], mSelection[1],
                                   mSelection[2], mSelection[3],
                                   false, false/*true*/);
        
            mPlug->AfterSelTranslation();
            
            // Set the play bar at the beginning of the new selection
            if (beginDrag)
                mPlug->ResetPlayBar();
        }
        
        // NEW: capture playbar inside selection
        if (mPlug->PlayBarOutsideSelection())
        {
            // Play bar is outside the selection,
            // rest it to the beginning of the selection
            //
            // This allows moving the selection, with play bar
            // "captured" inside the selection, on both sides
            mPlug->ResetPlayBar();
        }
    }
    
    if (mPrevMouseDownInsideMiniView)
    {
        MiniView *miniView = mSpectroDisplay->GetMiniView();
        if (miniView != NULL)
        {
            BL_FLOAT drag = miniView->GetDrag(dX, width);
            
            drag *= width;
            
            // Drag the spectrogram
            mPlug->Translate(drag);
            mPlug->SetNeedRecomputeData(true);
        }
    }
}

bool
GhostCustomControl::OnMouseDblClick(int x, int y, IMouseMod* pMod)
{
    int width;
    int height;
    mPlug->GetGraphSize(&width, &height);
    
    bool insideMiniView = mSpectroDisplay->PointInsideMiniView(x, y, width, height);
    
    if (insideMiniView)
        mPlug->RewindView();
    
    return true;
}

void
GhostCustomControl::OnMouseWheel(int x, int y, IMouseMod* pMod, float d)
{
    if (mPlug->GetMode() != Ghost::EDIT)
        return;
    
    // Don't check insideSpectro here
    // We just need the focus anywhere...
    
#if ZOOM_ON_POINTER
    // Set the zoom center as the mouse position and not
    // at the bar position
    mPlug->SetZoomCenter(x);
#endif
    
    BL_FLOAT zoomChange = 1.0 + d*WHEEL_ZOOM_STEP;
    mPlug->UpdateZoom(zoomChange);
    
    // For hozizontal selection, grow the selection to the
    // extremities if necessary when zooming
    UpdateSelectionType();
    
    bool isSelectionActive = mPlug->IsSelectionActive();
    
    mPlug->UpdateSelection(mSelection[0], mSelection[1],
                           mSelection[2], mSelection[3],
                           false, isSelectionActive);
    
    mPlug->SetNeedRecomputeData(true);
}

bool
GhostCustomControl::InsideSelection(int x, int y)
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
GhostCustomControl::SelectBorders(int x, int y)
{
    // FIX: avoid selecting selection rectangle border when it is not visible !
    // Do not try to select a border if only the play bar is visible
    bool barActive = mPlug->IsBarActive();
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
GhostCustomControl::BorderSelected()
{
    for (int i = 0; i < 4; i++)
    {
        if (mBorderSelected[i])
            return true;
    }
    
    return false;
}

void
GhostCustomControl::UpdateZoomSelection(BL_FLOAT zoomChange)
{
    Ghost::UpdateZoomSelection(mSelection, zoomChange);
}

void
GhostCustomControl::GetSelection(BL_FLOAT selection[4])
{
    for (int i = 0; i < 4; i++)
        selection[i] = mSelection[i];
}

void
GhostCustomControl::SetSelection(BL_FLOAT x0, BL_FLOAT y0,
                                 BL_FLOAT x1, BL_FLOAT y1)
{
    mSelection[0] = x0;
    mSelection[1] = y0;
    mSelection[2] = x1;
    mSelection[3] = y1;
}

void
GhostCustomControl::SetSelectionActive(bool flag)
{
    mSelectionActive = flag;
}

void
GhostCustomControl::UpdateSelection(bool updateCenterPos)
{
    mPlug->UpdateSelection(mSelection[0], mSelection[1],
                           mSelection[2], mSelection[3],
                           updateCenterPos);
}

void
GhostCustomControl::UpdateSelectionType()
{
    if (mSelectionType == Ghost::RECTANGLE)
        return;
    
    int width;
    int height;
    mPlug->GetGraphSize(&width, &height);
    
    if (mSelectionType == Ghost::HORIZONTAL)
    {
        mSelection[0] = 0;
        mSelection[2] = width;
        
        return;
    }
    
    if (mSelectionType == Ghost::VERTICAL)
    {
        mSelection[1] = 0;
        mSelection[3] = height;
        
        return;
    }
}
