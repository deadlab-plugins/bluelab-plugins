//
//  SASViewerRender2.h
//  BL-Waves
//
//  Created by applematuer on 10/13/18.
//
//

#ifndef __BL_SASViewer__SASViewerRender2__
#define __BL_SASViewer__SASViewerRender2__

#ifdef IGRAPHICS_NANOVG

#include <vector>
#include <deque>
using namespace std;

#include <GraphControl12.h>
#include <LinesRender2.h>
#include <SASViewerPluginInterface.h>
#include <SASViewerProcess2.h>

// Sides
//#define MAX_ANGLE_0 70.0

// Set to 90 for debugging
#define MAX_CAM_ANGLE_0 90.0

// Above
#define MAX_CAM_ANGLE_1 90.0 //70.0

// Below

// Almost horizontal (a little above)
#define MIN_CAM_ANGLE_1 15.0

class Axis3D;
class SASViewerRender2 : public GraphCustomControl
{
public:
    SASViewerRender2(SASViewerPluginInterface *plug,
                    GraphControl12 *graphControl,
                    BL_FLOAT sampleRate, int bufferSize);
    
    virtual ~SASViewerRender2();
    
    void SetGraph(GraphControl12 *graphControl);
    
    void Clear();
    
    virtual void AddMagns(SASViewerProcess2::Mode mode,
                          const WDL_TypedBuf<BL_FLOAT> &magns);
    virtual void AddPoints(SASViewerProcess2::Mode mode,
                           const vector<LinesRender2::Point> &points);

    virtual void SetLineMode(SASViewerProcess2::Mode mode,
                             LinesRender2::Mode lineMode);
    
    // Control
    virtual void OnMouseDown(float x, float y, const IMouseMod &mod) override;
    virtual void OnMouseUp(float x, float y, const IMouseMod &mod) override;
    virtual void OnMouseDrag(float x, float y, float dX, float dY,
                             const IMouseMod &mod) override;
    virtual void OnMouseDblClick(float x, float y, const IMouseMod &mod) override;
    virtual void OnMouseWheel(float x, float y,
                              const IMouseMod &mod, BL_FLOAT d) override;
    
    // Parameters
    virtual void SetSpeed(BL_FLOAT speed);
    virtual void SetDensity(BL_FLOAT density);
    virtual void SetScale(BL_FLOAT scale);
    
    // For parameter sent from plug (save state and automation)
    void SetCamAngle0(BL_FLOAT angle);
    void SetCamAngle1(BL_FLOAT angle);
    void SetCamFov(BL_FLOAT angle);
    
    //
    void SetMode(SASViewerProcess2::Mode mode);
    
    // Used for rendering tracked partials
    int GetNumSlices();
    int GetSpeed();
    
    void SetAdditionalLines(SASViewerProcess2::Mode mode,
                            const vector<LinesRender2::Line> &lines,
                            BL_FLOAT lineWidth);
    
    void ClearAdditionalLines();
    
    void ShowTrackingLines(SASViewerProcess2::Mode mode, bool flag);
    
    // For debugging
    void DBG_SetNumSlices(int numSlices);
    
protected:
    void MagnsToPoints(vector<LinesRender2::Point> *points,
                       const WDL_TypedBuf<BL_FLOAT> &magns);
    
    //
    SASViewerPluginInterface *mPlug;
    
    GraphControl12 *mGraph;
    
    LinesRender2 *mLinesRenders[SASViewerProcess2::NUM_MODES];
    
    Axis3D *mFreqsAxis;
    
    // Selection
    bool mMouseIsDown;
    float mPrevDrag[2];
    
    // Used to detect pure mouse up, without drag
    bool mPrevMouseDrag;
    
    // Rotation
    BL_FLOAT mCamAngle0;
    BL_FLOAT mCamAngle1;
    
    BL_FLOAT mScale;
    
    BL_FLOAT mSampleRate;
    int mBufferSize;
    
    //
    unsigned long long int mAddNum;
    
    SASViewerProcess2::Mode mCurrentMode;
};

#endif // IGRAPHICS_NANOVG

#endif /* defined(__BL_SASViewer__SASViewerRender2__) */
