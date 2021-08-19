//
//  SASViewerRender4.cpp
//  BL-Waves
//
//  Created by applematuer on 10/13/18.
//
//

#ifdef IGRAPHICS_NANOVG

#include <Axis3D.h>

#include <BLUtils.h>
#include <BLDebug.h>

// Include for flag for partials debug
#include <SASViewerProcess4.h>

#include "SASViewerRender4.h"

// Axis drawing
#define AXIS_OFFSET_Z 0.06

// Artificially modify the coeff, to increase the spread on the grid
#define MEL_COEFF 4.0

// Do noty skpi any slices
#define DBG_DISPLAY_ALL_SLICES 0 //1

SASViewerRender4::SASViewerRender4(SASViewerPluginInterface *plug,
                                   GraphControl12 *graphControl,
                                   BL_FLOAT sampleRate, int bufferSize)
{
    mPlug = plug;
    
    mSampleRate = sampleRate;
    mBufferSize = bufferSize;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
    {
        mLinesRenders[i] = new LinesRender2();
        mLinesRenders[i]->SetMode(LinesRender2::LINES_FREQ);

        mLinesRenders[i]->SetUseLegacyLock(true);

#if DBG_DISPLAY_ALL_SLICES
        mLinesRenders[i]->DBG_SetDisplayAllSlices();
#endif

        // Fix, to have the detection points synchronized to the lines
        mLinesRenders[i]->DBG_FixDensityNumSlices();
    }
    
    mCurrentMode = SASViewerProcess4::TRACKING;
    
    SetGraph(graphControl);
    
    mMouseIsDown = false;
    mPrevDrag[0] = 0;
    mPrevDrag[1] = 0;
    
    mPrevMouseDrag = false;
    
    // Rotation of the camera
    mCamAngle0 = 0.0;
    mCamAngle1 = MIN_CAM_ANGLE_1;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
    {
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    }
    
    mAddNum = 0;
}

SASViewerRender4::~SASViewerRender4()
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        delete mLinesRenders[i];
}

void
SASViewerRender4::SetGraph(GraphControl12 *graphControl)
{
    mGraph = graphControl;
    
    if (mGraph != NULL)
    {
        mGraph->AddCustomControl(this);
        mGraph->AddCustomDrawer(mLinesRenders[(int)mCurrentMode]);
    }
}

void
SASViewerRender4::Clear()
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->ClearSlices();

    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::AddData(SASViewerProcess4::Mode mode,
                          const WDL_TypedBuf<BL_FLOAT> &data)
{
    if (data.GetSize() == 0)
        return;
    
    vector<LinesRender2::Point> points;
    DataToPoints(&points, data);
    
    mLinesRenders[(int)mode]->AddSlice(points);
    
    mAddNum++;
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::AddPoints(SASViewerProcess4::Mode mode,
                            const vector<LinesRender2::Point> &points)
{
    mLinesRenders[(int)mode]->AddSlice(points);
    
    mAddNum++;
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetLineMode(SASViewerProcess4::Mode mode,
                              LinesRender2::Mode lineMode)
{
   mLinesRenders[(int)mode]->SetMode(lineMode);
}

void
SASViewerRender4::OnMouseDown(float x, float y, const IMouseMod &mod)
{
    mMouseIsDown = true;
    
    mPrevDrag[0] = x;
    mPrevDrag[1] = y;
    
    mPrevMouseDrag = false;
}

void
SASViewerRender4::OnMouseUp(float x, float y, const IMouseMod &mod)
{
    if (!mMouseIsDown)
        return;
    
    mMouseIsDown = false;
}

void
SASViewerRender4::OnMouseDrag(float x, float y, float dX, float dY,
                              const IMouseMod &mod)
{
    if (mod.A)
    // Alt-drag => zoom
    {
        if (!mPrevMouseDrag)
        {
            mPrevMouseDrag = true;
            
            mPrevDrag[0] = y;
            
            return;
        }
        
#define DRAG_WHEEL_COEFF 0.2
        
        BL_FLOAT dY2 = y - mPrevDrag[0];
        mPrevDrag[0] = y;
        
        dY2 *= -1.0;
        dY2 *= DRAG_WHEEL_COEFF;
        
        OnMouseWheel(mPrevDrag[0], mPrevDrag[1], mod, dY2);
        
        return;
    }
    
    mPrevMouseDrag = true;
    
    // Move camera
    float dragX = x - mPrevDrag[0];
    float dragY = y - mPrevDrag[1];
    
    mPrevDrag[0] = x;
    mPrevDrag[1] = y;
    
    mCamAngle0 += dragX;
    mCamAngle1 += dragY;
    
    // Bounds
    if (mCamAngle0 < -MAX_CAM_ANGLE_0)
        mCamAngle0 = -MAX_CAM_ANGLE_0;
    if (mCamAngle0 > MAX_CAM_ANGLE_0)
        mCamAngle0 = MAX_CAM_ANGLE_0;
    
    if (mCamAngle1 < MIN_CAM_ANGLE_1)
        mCamAngle1 = MIN_CAM_ANGLE_1;
    if (mCamAngle1 > MAX_CAM_ANGLE_1)
        mCamAngle1 = MAX_CAM_ANGLE_1;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    // Camera changed
    //
    // Without that, the camera point of view is not modified if
    // the sound is not playing
    //mGraph->SetDirty(true);
    if (mGraph != NULL)
        mGraph->SetDataChanged();
    
    mPlug->SetCameraAngles(mCamAngle0, mCamAngle1);
}

void
SASViewerRender4::OnMouseDblClick(float x, float y, const IMouseMod &mod)
{
    // Reset the view
    mCamAngle0 = 0.0;
    mCamAngle1 = MIN_CAM_ANGLE_1;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
            mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    // Reset the fov
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->ResetZoom();

    if (mGraph != NULL)
        mGraph->SetDataChanged();
    
    mPlug->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    BL_FLOAT fov = mLinesRenders[0]->GetCameraFov();
    mPlug->SetCameraFov(fov);
}

void
SASViewerRender4::OnMouseWheel(float x, float y,
                              const IMouseMod &mod, float d)
{
#define WHEEL_ZOOM_STEP 0.025
    
    BL_FLOAT zoomChange = 1.0 + d*WHEEL_ZOOM_STEP;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->ZoomChanged(zoomChange);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
    
    BL_FLOAT angle = mLinesRenders[0]->GetCameraFov();
    mPlug->SetCameraFov(angle);
}

void
SASViewerRender4::SetSpeed(BL_FLOAT speed)
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetSpeed(speed);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetDensity(BL_FLOAT density)
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetDensity(density);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetScale(BL_FLOAT scale)
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetScale(scale);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetCamAngle0(BL_FLOAT angle)
{
    mCamAngle0 = angle;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetCamAngle1(BL_FLOAT angle)
{
    mCamAngle1 = angle;
    
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraAngles(mCamAngle0, mCamAngle1);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
SASViewerRender4::SetCamFov(BL_FLOAT angle)
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->SetCameraFov(angle);
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

int
SASViewerRender4::GetNumSlices()
{
    int numSlices = (int)mLinesRenders[0]->GetNumSlices();
    
    return numSlices;
}

void
SASViewerRender4::SetMode(SASViewerProcess4::Mode mode)
{
    if (mGraph != NULL)
        mGraph->RemoveCustomDrawer(mLinesRenders[mCurrentMode]);
    
    mCurrentMode = mode;
    
    if (mGraph != NULL)
        mGraph->AddCustomDrawer(mLinesRenders[mCurrentMode]);
}

int
SASViewerRender4::GetSpeed()
{
    int speed = mLinesRenders[0]->GetSpeed();
    
    return speed;
}

void
SASViewerRender4::SetAdditionalLines(SASViewerProcess4::Mode mode,
                                     const vector<LinesRender2::Line> &lines,
                                     BL_FLOAT lineWidth)
{
    mLinesRenders[(int)mode]->SetAdditionalLines(lines, lineWidth);
}

void
SASViewerRender4::ClearAdditionalLines()
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->ClearAdditionalLines();
}

void
SASViewerRender4::ShowAdditionalLines(SASViewerProcess4::Mode mode, bool flag)
{
    mLinesRenders[(int)mode]->ShowAdditionalLines(flag);
}

void
SASViewerRender4::SetAdditionalPoints(SASViewerProcess4::Mode mode,
                                      const vector<LinesRender2::Line> &lines,
                                      BL_FLOAT lineWidth)
{
    mLinesRenders[(int)mode]->SetAdditionalPoints(lines, lineWidth);
}

void
SASViewerRender4::ClearAdditionalPoints()
{
    for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
        mLinesRenders[i]->ClearAdditionalPoints();
}

void
SASViewerRender4::ShowAdditionalPoints(SASViewerProcess4::Mode mode, bool flag)
{
    mLinesRenders[(int)mode]->ShowAdditionalPoints(flag);
}

void
SASViewerRender4::DBG_SetNumSlices(int numSlices)
{
     for (int i = 0; i < (int)SASViewerProcess4::NUM_MODES; i++)
     {
         mLinesRenders[i]->SetNumSlices(numSlices);
         mLinesRenders[i]->DBG_ForceDensityNumSlices();
     }
}

void
SASViewerRender4::DataToPoints(vector<LinesRender2::Point> *points,
                               const WDL_TypedBuf<BL_FLOAT> &data)
{
    if (data.GetSize() == 0)
        return;
    
    // Convert to points
    points->resize(data.GetSize());
    
    // Optim
    BL_FLOAT xCoeff = 0.0;
    if (data.GetSize() > 0)
    {
        if (data.GetSize() <= 1)
            xCoeff = 1.0/data.GetSize();
        else
            xCoeff = 1.0/(data.GetSize() - 1);
    }
    
    // Loop
    for (int i = 0; i < data.GetSize(); i++)
    {
        BL_FLOAT magn = data.Get()[i];
        
        LinesRender2::Point &p = (*points)[i];
        
        p.mX = xCoeff*i - 0.5;
        p.mY = magn;
        
        // Fill with dummy Z (to avoid undefined value)
        p.mZ = 0.0;
        
        // Fill with dummy color (to avoid undefined value)
        p.mR = 0;
        p.mG = 0;
        p.mB = 0;
        p.mA = 0;
    }
}

#endif // IGRAPHICS_NANOVG
