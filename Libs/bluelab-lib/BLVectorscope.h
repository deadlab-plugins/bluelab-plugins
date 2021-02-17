//
//  BLVectorscope.h
//  UST
//
//  Created by applematuer on 7/27/19.
//
//

#ifndef __UST__BLVectorscope__
#define __UST__BLVectorscope__

#ifdef IGRAPHICS_NANOVG

#include <vector>
using namespace std;

#include "../../WDL/fastqueue.h"

#include <GraphControl12.h>

#include "IPlug_include_in_plug_hdr.h"

// Modes
#define NUM_MODES 5

// Fireworks has 2 curves.
// And Upmix has no curve
#define POLAR_SAMPLE_MODE_ID    0
#define LISSAJOUS_MODE_ID       1
#define FIREWORKS_MODE_ID       2
#define UPMIX_MODE_ID           3
#define SOURCE_MODE_ID          4

#define POLAR_SAMPLE_CURVE_ID   0
#define LISSAJOUS_CURVE_ID      1
#define FIREWORKS_CURVE0_ID     2
#define FIREWORKS_CURVE1_ID     3
#define SOURCE_CURVE_ID         4


class BLCircleGraphDrawer;
class BLLissajousGraphDrawer;
class BLFireworks;
class BLUpmixGraphDrawer;
class StereoWidthGraphDrawer3;
class SourceComputer;
class GraphCurve5;

class BLVectorscopePlug
{
public:
    virtual void VectorscopeUpdatePanCB(BL_FLOAT newPan) = 0;
    virtual void VectorscopeUpdateDPanCB(BL_FLOAT dpan) = 0;
    virtual void VectorscopeUpdateDWidthCB(BL_FLOAT dwidth) = 0;
    virtual void VectorscopeUpdateDepthCB(BL_FLOAT newDepth) = 0;
    virtual void VectorscopeUpdateDDepthCB(BL_FLOAT ddepth) = 0;
};

// USTVectorscope2: several modes
// USTVectorscope3: polar level object
// USTVectorscope4: use fireworks view instead of polar levels
// BLVectorscope: from USTVectorscope4
//
class GUIHelper12;
class BLVectorscope : public GraphCustomControl
{
public:
    enum Mode
    {
      POLAR_SAMPLE = 0,
      LISSAJOUS,
      FIREWORKS,
      UPMIX,
      SOURCE
    };
    
    BLVectorscope(BLVectorscopePlug *plug,
                  BL_FLOAT sampleRate);
    
    virtual ~BLVectorscope();

    void Reset(BL_FLOAT sampleRate);
    void Reset();
    
    void SetMode(Mode mode);
    
    void OnMouseDown(float x, float y, const IMouseMod &mod);
    void OnMouseUp(float x, float y, const IMouseMod &mod);
    void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod &mod);

    void SetGraphs(GraphControl12 *graph0,
                   GraphControl12 *graph1,
                   GraphControl12 *graph2,
                   GraphControl12 *graph3,
                   GraphControl12 *graph4,
                   GUIHelper12 *guiHelper = NULL);

    void AddSamples(const vector<WDL_TypedBuf<BL_FLOAT> > &samples);

    //
    BLUpmixGraphDrawer *GetUpmixGraphDrawer();
    
protected:
    void SetCurveStyle(GraphControl12 *graph,
                       int curveNum,
                       BL_FLOAT minX, BL_FLOAT maxX,
                       BL_FLOAT minY, BL_FLOAT maxY,
                       bool pointFlag,
                       BL_FLOAT strokeSize,
                       bool bevelFlag,
                       int r, int g, int b,
                       bool curveFill, BL_FLOAT curveFillAlpha,
                       bool pointsAsLines,
                       bool pointOverlay,
                       bool linesPolarFlag);

    //
    Mode mMode;
    
    BLVectorscopePlug *mPlug;
    
    GraphControl12 *mGraphs[NUM_MODES];
    
    // Fireworks has 2 curves.
    // And Upmix has no curve
    GraphCurve5 *mCurves[NUM_MODES];
    
    BLCircleGraphDrawer *mCircleDrawerPolarSamples;
    BLCircleGraphDrawer *mCircleDrawerFireworks;
    BLLissajousGraphDrawer *mLissajousDrawer;
    BLUpmixGraphDrawer *mUpmixDrawer;
    StereoWidthGraphDrawer3 *mCircleDrawerSource;
    
    BLFireworks *mFireworks;
    SourceComputer *mSourceComputer;
    
    //WDL_TypedBuf<BL_FLOAT> mSamples[2];
    WDL_TypedFastQueue<BL_FLOAT> mSamples[2];

private:
    // Tmp buffers
    WDL_TypedBuf<BL_FLOAT> mTmpBuf0[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf1[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf2[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf3[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf4[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf5[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf6[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf7[2];
    WDL_TypedBuf<BL_FLOAT> mTmpBuf8[2];
};

#endif // IGRAPHICS_NANOVG

#endif /* defined(__UST__BLVectorscope__) */
