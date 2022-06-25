#include <GraphSwapColor.h>

#include <BLDebug.h>

#include "CurveViewPitch.h"

// Pitch is in fact a pitch in term of chroma feature
// Avoid tracing the curve when there are big jumps
// (in this case, the curve is right but should continue outside the graph)
#define AVOID_TRACING_PITCH_JUMPS 1

CurveViewPitch::CurveViewPitch(int maxNumData)
: CurveView("pitch", 0.0, maxNumData) {}

CurveViewPitch::~CurveViewPitch() {}

void
CurveViewPitch::DrawCurve(NVGcontext *vg, int width, int height)
{
    if (mData.empty())
        return;
    
    nvgSave(vg);
    nvgReset(vg);
    
    // Color
    int sColor[4] = { mCurveColor.R, mCurveColor.G,
                      mCurveColor.B, mCurveColor.A };
    SWAP_COLOR(sColor);
    nvgStrokeColor(vg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));

    // Width
    nvgStrokeWidth(vg, mCurveLineWidth);

    // Will avoid that the curves goes outside the borders
    // if curve line width is 2.0 for example.
    BL_FLOAT xOffset = 0.0;
    BL_FLOAT adjustedWidth = mWidth;
    if (mCurveLineWidth > 1.0)
    {
        xOffset = mCurveLineWidth*0.5;
        adjustedWidth = mWidth - 2.0*xOffset;
    }
        
    // Bevel
    nvgLineJoin(vg, NVG_BEVEL);

    // Draw the curve
    nvgBeginPath(vg);
    
    BL_FLOAT ty0 = 1.0 - mData[0];
    if (ty0 > 1.0)
        ty0 = 1.0;
    if (ty0 < 0.0)
        ty0 = 0.0;
    BL_FLOAT y0 = ty0*(mHeight - CURVE_Y_OFFSET*2);
    
    nvgMoveTo(vg, mX + xOffset, mY + y0 + CURVE_Y_OFFSET);

    BL_FLOAT prevTy = ty0;
    for (int i = 1; i < mData.size(); i++)
    {
        BL_FLOAT tx = ((BL_FLOAT)i)/(mData.size() - 1);
        BL_FLOAT ty = 1.0 - mData[i];

        // Double-check bounds!
        if (ty < 0.0)
            ty = 0.0;
        if (ty > 1.0)
            ty = 1.0;
        
        BL_FLOAT y = ty*(mHeight - CURVE_Y_OFFSET*2);

        bool shouldSkipTrace = false;
#if AVOID_TRACING_PITCH_JUMPS
        if (fabs(ty - prevTy) > 0.8/*0.5*/)
            shouldSkipTrace = true;
#endif
        
        if (!shouldSkipTrace)
        {
            nvgLineTo(vg, mX + tx*adjustedWidth + xOffset,
                      mY + y + CURVE_Y_OFFSET);
        }
        else // Make a jump without tracing (pitch is "cyclic")
        {
            nvgMoveTo(vg, mX + tx*adjustedWidth + xOffset,
                      mY + y + CURVE_Y_OFFSET);
        }

        prevTy = ty;
    }

    nvgStroke(vg);
    
    nvgRestore(vg);
}
