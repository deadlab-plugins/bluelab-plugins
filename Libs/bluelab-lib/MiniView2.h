//
//  MiniView2.h
//  BL-Ghost
//
//  Created by Pan on 15/06/18.
//
//

#ifndef __BL_Ghost__MiniView2__
#define __BL_Ghost__MiniView2__

#ifdef IGRAPHICS_NANOVG

#include <BLTypes.h>
#include <GraphControl12.h>

#include "IPlug_include_in_plug_hdr.h"

class NVGcontext;

class MiniView2 : public GraphCustomDrawer
{
public:
    MiniView2(int maxNumPoints,
             BL_FLOAT x0, BL_FLOAT y0, BL_FLOAT x1, BL_FLOAT y1);
    
    virtual ~MiniView2();

    void PreDraw(NVGcontext *vg, int width, int height) override;
    
    bool IsPointInside(int x, int y, int width, int height);
    
    void SetData(const WDL_TypedBuf<BL_FLOAT> &data);
    
    void GetWaveForm(WDL_TypedBuf<BL_FLOAT> *waveform);
    
    void SetBounds(BL_FLOAT minNormX, BL_FLOAT maxNormX);
    
    BL_FLOAT GetDrag(int dragX, int width);
    
protected:
    BL_FLOAT mBounds[4];
    
    int mMaxNumPoints;
    
    WDL_TypedBuf<BL_FLOAT> mWaveForm;
    
    BL_FLOAT mMinNormX;
    BL_FLOAT mMaxNormX;
};

#endif

#endif /* defined(__BL_Ghost__MiniView2__) */
