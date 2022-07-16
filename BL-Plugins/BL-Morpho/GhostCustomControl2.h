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
 
#ifndef GHOST_CUSTOM_CONTROL2_H
#define GHOST_CUSTOM_CONTROL2_H

#ifdef IGRAPHICS_NANOVG

#include <BLTypes.h>

#include <GraphControl12.h>

class CursorListener
{
public:
    virtual void CursorMoved(BL_FLOAT x, BL_FLOAT y) = 0;

    virtual void CursorOut() = 0;
};

class SpectrogramDisplay3;
class MiniView2;
class GhostTrack2;
class GhostCustomControl2 : public GraphCustomControl
{
public:
    GhostCustomControl2(GhostTrack2 *track);
    
    virtual ~GhostCustomControl2() {}

    void SetCursorListener(CursorListener *listener);
    
    void Resize(int prevWidth, int prevHeight,
                int newWidth, int newHeight);
    
    //void SetSpectrogramDisplay(SpectrogramDisplay2 *spectroDisplay);
    void SetSpectrogramDisplay(SpectrogramDisplay3 *spectroDisplay);
    void SetMiniView(MiniView2 *miniView);
    
    void SetSelectionType(int selectionType);
    
    virtual void OnMouseDown(float x, float y, const IMouseMod &pMod) override;
    virtual void OnMouseUp(float x, float y, const IMouseMod &pMod) override;
    virtual void OnMouseDrag(float x, float y, float dX, float dY,
                             const IMouseMod &pMod) override;
    virtual /*bool*/ void OnMouseDblClick(float x, float y,
                                          const IMouseMod &pMod) override;
    virtual void OnMouseWheel(float x, float y,
                              const IMouseMod &pMod, float d) override;
    virtual bool OnKeyDown(float x, float y,
                           const IKeyPress &key) override { return false; }
    
    virtual void OnMouseOver(float x, float y, const IMouseMod &pMod) override;
    virtual void OnMouseOut() override;
    
    //void UpdateZoomSelection(BL_FLOAT zoomChange);
    void UpdateZoom(BL_FLOAT zoomChange);
    
    void GetSelection(BL_FLOAT selection[4]);
    void SetSelection(BL_FLOAT x0, BL_FLOAT y0,
                      BL_FLOAT x1, BL_FLOAT y1);
    
    void SetSelectionActive(bool flag);
    // NOTE: does not totally work, must click a second time for it to works 
    bool GetSelectionActive();
    
    // Refresh the selection (for example after zoom)
    void UpdateSelection(bool updateCenterPos);

    bool IsMouseOver() const;
    
protected:
    bool InsideSelection(int x, int y);
    
    void SelectBorders(int x, int y);
    bool BorderSelected();
    
    // Update the selection depending on the selection type
    void UpdateSelectionType();
    
    GhostTrack2 *mTrack;
    
    // Used to detect pure mouse up, without drag
    bool mPrevMouseDrag;
    
    int mStartDrag[2];
  
    BL_FLOAT mPrevMouseY;
    
    bool mSelectionActive;
    BL_FLOAT mSelection[4];
    
    bool mPrevMouseInsideSelect;
    
    bool mBorderSelected[4];
    
    // Inside or outside spectro ?
    bool mPrevMouseDownInsideSpectro;
    bool mPrevMouseDownInsideMiniView;
    
    //SpectrogramDisplay2 *mSpectroDisplay;
    SpectrogramDisplay3 *mSpectroDisplay;
    
    int mSelectionType;
    
    // Detect if we actually made the mouse down insde the spectrogram
    // (FIXES: mouse up on resize button to a bigger size, and then the mouse up
    // is inside the graph at the end (without previous mouse down inside)
    bool mPrevMouseDown;
    
    MiniView2 *mMiniView;

    bool mMouseIsOver;

    CursorListener *mCursorListener;
};

#endif

#endif
