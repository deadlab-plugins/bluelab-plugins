#ifndef IXY_PAD_CONTROL_H
#define IXY_PAD_CONTROL_H

#include <IControl.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace iplug::igraphics;

class IXYPadControl : public IControl
{
 public:
    IXYPadControl(const IRECT& bounds,
                  const std::initializer_list<int>& params,
                  const IBitmap& trackBitmap,
                  const IBitmap& handleBitmap);

    virtual ~IXYPadControl();
    
    void Draw(IGraphics& g) override;

    void OnMouseDown(float x, float y, const IMouseMod& mod) override;
    void OnMouseUp(float x, float y, const IMouseMod& mod) override;
    void OnMouseDrag(float x, float y, float dX, float dY,
                     const IMouseMod& mod) override;
  
 protected:
    void DrawTrack(IGraphics& g);
    void DrawHandle(IGraphics& g);

    // Ensure that the handle doesn't go out of the track at all 
    void PixelsToParams(float *x, float *y);
    void ParamsToPixels(float *x, float *y);
 
    //
    IBitmap mTrackBitmap;
    IBitmap mHandleBitmap;
        
    bool mMouseDown;
};

#endif