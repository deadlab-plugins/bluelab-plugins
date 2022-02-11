//
//  Graph.cpp
//  Transient
//
//  Created by Apple m'a Tuer on 03/09/17.
//
//

#include <math.h>

#include <stdio.h>

#include <GL/glew.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#endif

#include "nanovg.h"

// Warning: Niko hack in NanoVg to support FBO even on GL2
#define NANOVG_GL2_IMPLEMENTATION

#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"

#include "../../WDL/lice/lice_glbitmap.h"

#include "../../WDL/IPlug/IPlugBase.h"

// Plugin resource file
#include "resource.h"

#include "SpectrogramDisplay.h"
#include "Utils.h"
#include "GLContext.h"
#include "GraphControl10.h"
#include "Debug.h"

#define FILL_CURVE_HACK 1

// To fix line holes between filled polygons
#define FILL_CURVE_HACK2 1

#define CURVE_DEBUG 0

// Good, but misses some maxima
#define CURVE_OPTIM      0
#define CURVE_OPTIM_THRS 2048

// -1 pixel
#define OVERLAY_OFFSET -1.0

GraphControl10::GraphControl10(IPlugBase *pPlug, IRECT pR, int paramIdx,
                             int numCurves, int numCurveValues,
                             const char *fontPath)
	: IControl(pPlug, pR, paramIdx),
	mFontInitialized(false),
	mAutoAdjustParamSmoother(1.0, 0.9),
	mNumCurves(numCurves),
	mNumCurveValues(numCurveValues),
	mHAxis(NULL),
	mVAxis(NULL)
{
	for (int i = 0; i < mNumCurves; i++)
	{
		GraphCurve4 *curve = new GraphCurve4(numCurveValues);

		mCurves.push_back(curve);
	}

	mAutoAdjustFlag = false;
	mAutoAdjustFactor = 1.0;

	mYScaleFactor = 1.0;

    // Bottom separator
    mSeparatorY0 = false;
    mSepY0LineWidth = 1.0;
    
    mSepY0Color[0] = 0;
    mSepY0Color[1] = 0;
    mSepY0Color[2] = 0;
    mSepY0Color[3] = 0;
    
    // dB Scale
	mXdBScale = false;
	//mMinX = 0.1;
    mMinX = 0.0;
	mMaxX = 1.0;
    
    SetClearColor(0, 0, 0, 255);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
    
    // No need since this is static
    mGLInitialized = false;

    mFontPath.Set(fontPath);

#if 0 // TEST: Ableton 10 Win 10: do not make call to glewInit() here 
	  //because we are not sure it it the correct thread
	  // InitGL() will be done later, in OnIdle()

   // Seems like GL must be initialized in this thread, otherwise
   // it will make Ableton 10 on windows to consume all the resources 
   InitGL();
   
   InitNanoVg();
#endif

    mLiceFb = NULL;
    
    mSpectrogramDisplay = NULL;;
    
    mBounds[0] = 0.0;
    mBounds[1] = 0.0;
    mBounds[2] = 1.0;
    mBounds[3] = 1.0;
    
    mBgImage = NULL;
    mNvgBackgroundImage = 0;
    
#if PROFILE_GRAPH
    mDebugCount = 0;
#endif
}

GraphControl10::~GraphControl10()
{
    for (int i = 0; i < mNumCurves; i++)
        delete mCurves[i];
    
    if (mHAxis != NULL)
        delete mHAxis;
    
    if (mVAxis != NULL)
        delete mVAxis;

    if (mSpectrogramDisplay != NULL)
        delete mSpectrogramDisplay;
    
    // Will be delete in the plugin !
    //if (mBgImage != NULL)
    //    delete mBgImage;
    
    if (mNvgBackgroundImage != 0)
	{
		// FIX: Ableton 10 win 10
		// mVg could be null if GL has not succeed to initialize before
		if (mVg != NULL)
			nvgDeleteImage(mVg, mNvgBackgroundImage);
	}

	// FIX: Ableton 10, windows 10
	// fix crash when scanning plugin (real crash)
	if (mGLInitialized)
	{
		// FIX Mixcraft crash: VST3, Windows: load AutoGain project, play, inser Denoiser, play, remove Denoiser, play !> this crashed
		// This fix works.
#if 1
		// Mixcraft FIX: do not unbind, bind instead
		// Otherwise ExitNanoVg() crases with Mixcraft if no context is bound
		GLContext *context = GLContext::Get();
		if (context != NULL)
			context->Bind();
#endif

		// QUESTION: is this fix for Windows or Mac ?
#if 1 // AbletonLive need these two linesco
		// Otherwise when destroying the plugin in AbletonLive,
		// there are many graphical bugs (huge white rectangles)
		// and the software becomes unusable
		//GLContext *context = GLContext::Get();
		//if (context != NULL)
		//context->Unbind();
#endif
    
		ExitNanoVg();
	}

    if (mLiceFb != NULL)
        delete mLiceFb; 

	// FIX: Ableton 10, windows 10
	// fix crash when scanning plugin (real crash)
	if (mGLInitialized)
	{
	   // No need anymore since this is static
	 ExitGL();
	}
}

void
GraphControl10::GetSize(int *width, int *height)
{
    *width = this->mRECT.W();
    *height = this->mRECT.H();
}

void
GraphControl10::Resize(int width, int height)
{
    // We must have width and height multiple of 4
    width = (width/4)*4;
    height = (height/4)*4;
    
    //
    int dWidth = this->mRECT.W() - width;
    
    this->mRECT.R = this->mRECT.L + width;
    this->mRECT.B = this->mRECT.T + height;
    
    // For mouse
    this->mTargetRECT = this->mRECT;
    
    mVAxis->mOffsetX -= dWidth;
    
    // Plan to re-create the fbo, to have a good definition when zooming
    if (mLiceFb != NULL)
        delete mLiceFb;
    mLiceFb = NULL;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetBounds(double x0, double y0, double x1, double y1)
{
    mBounds[0] = x0;
    mBounds[1] = y0;
    mBounds[2] = x1;
    mBounds[3] = y1;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::Resize(int numCurveValues)
{
    mNumCurveValues = numCurveValues;
    
    for (int i = 0; i < mNumCurves; i++)
	{
		GraphCurve4 *curve = mCurves[i];
        
		curve->ResetNumValues(mNumCurveValues);
	}
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

int
GraphControl10::GetNumCurveValues()
{
    return mNumCurveValues;
}

#if 0 // Commented for StereoWidthProcess::DebugDrawer
bool
GraphControl10::IsDirty()
{
    return mDirty;
}

void
GraphControl10::SetDirty(bool flag)
{
    mDirty = flag;
}
#endif

void
GraphControl10::SetMyDirty(bool flag)
{
#if DIRTY_OPTIM
    mMyDirty = flag;
#endif
}


const LICE_IBitmap *
GraphControl10::DrawGL()
{
#if PROFILE_GRAPH
    mDebugTimer.Start();
#endif
    
    IPlugBase::IMutexLock lock(mPlug);
    
	// FIX: Ableton 10 win 10: if gl is not yet initialized, try to initialize it here
	// (may solve, since we are here in the draw thread, whereas the InitGL()
	// in the constructor is certainly in the other thread.
	if (!mGLInitialized)
	{
		// Commented because if we try to initialize GL in this thread,
		// Ableton 10 on windows will consume all the resources

		//InitGL();

		//InitNanoVg();

		//if (!mGLInitialized)
			// Still not initialized
			// Do not try to draw otherwise it will crash
		//return NULL;
		return NULL;
	}

    //
    GLContext *context = GLContext::Get();

	// FIX: Ableton 10 win 10
	// Security test: if not yet initialized, do not draw
	if (context != NULL)
		context->Bind();
	else
		return NULL;

    // Added this test to avoid redraw everything each time
    // NOTE: added for StereoWidth
#if DIRTY_OPTIM
    if (mMyDirty)
#endif
        DrawGraph();
    
    // Must be commented, otherwise it make a refresh bug on Protools
    // FIX: on Protools, after zooming, the data is not refreshed until
    // we make a tiny zoom or we modify slightly a parameter.
    //mDirty = false;
    
#if DIRTY_OPTIM
    mMyDirty = false;
#endif
        
#if PROFILE_GRAPH
    mDebugTimer.Stop();
    
    if (mDebugCount++ % 100 == 0)
    {
        long t = mDebugTimer.Get();
        mDebugTimer.Reset();
        
        fprintf(stderr, "GraphControl10 - profile: %ld\n", t);
        
        char debugMessage[512];
        sprintf(debugMessage, "GraphControl10 - profile: %ld", t);
        Debug::AppendMessage("graph-profile.txt", debugMessage);
    }
#endif
    
    return mLiceFb;
}

//void
//GraphControl10::OnIdle()
//{
	// FIX: Albeton 10 windows 10
	//
	// Try to initialize OpenGL if not already
	// Called from the audio processing thread as it should be

//	if (!mGLInitialized)
//	{
//		InitGL();

//		InitNanoVg();
//	}
//}

void
GraphControl10::SetSeparatorY0(double lineWidth, int color[4])
{
    mSeparatorY0 = true;
    mSepY0LineWidth = lineWidth;
    
    for (int i = 0; i < 4; i++)
        mSepY0Color[i] = color[i];
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::AddHAxis(char *data[][2], int numData, int axisColor[4], int axisLabelColor[4],
                         double offsetY,
                         int axisOverlayColor[4])
{
    mHAxis = new GraphAxis();
    mHAxis->mOffset = 0.0;
    
    // Warning, offset Y is normalized value
    mHAxis->mOffsetY = offsetY;
    mHAxis->mOverlay = false;
    
    AddAxis(mHAxis, data, numData, axisColor, axisLabelColor, mMinX, mMaxX, axisOverlayColor);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::ReplaceHAxis(char *data[][2], int numData)
{
    int axisColor[4];
    int axisLabelColor[4];
    double offsetY = 0.0;
    
    int overlayColor[4];
    
    if (mHAxis != NULL)
    {
        for (int i = 0; i < 4; i++)
            axisColor[i] = mHAxis->mColor[i];
        
        for (int i = 0; i < 4; i++)
            axisLabelColor[i] = mHAxis->mLabelColor[i];
        
        offsetY = mHAxis->mOffsetY;
        
        for (int i = 0; i < 4; i++)
            overlayColor[i] = mHAxis->mOverlayColor[i];
        
        delete mHAxis;
    }
    
    AddHAxis(data, numData, axisColor, axisLabelColor, offsetY, overlayColor);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::AddVAxis(char *data[][2], int numData, int axisColor[4],
                         int axisLabelColor[4], double offset, double offsetX,
                         int axisOverlayColor[4])
{
    mVAxis = new GraphAxis();
    mVAxis->mOverlay = false;
    mVAxis->mOffset = offset;
    mVAxis->mOffsetX = offsetX;
    mVAxis->mOffsetY = 0.0;
    
    // Retreive the Y db scale
    double minY = -40.0;
    double maxY = 40.0;
    
    if (!mCurves.empty())
        // Get from the first curve
    {
        minY = mCurves[0]->mMinY;
        maxY = mCurves[0]->mMaxY;
    }
    
    AddAxis(mVAxis, data, numData, axisColor, axisLabelColor, minY, maxY, axisOverlayColor);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::AddVAxis(char *data[][2], int numData, int axisColor[4], int axisLabelColor[4],
                        bool dbFlag, double minY, double maxY,
                        double offset, int axisOverlayColor[4])
{
    mVAxis = new GraphAxis();
    mVAxis->mOverlay = false;
    mVAxis->mOffset = offset;
    mVAxis->mOffsetY = 0.0;
    
    AddAxis(mVAxis, data, numData, axisColor, axisLabelColor, minY, maxY, axisOverlayColor);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetXScale(bool dbFlag, double minX, double maxX)
{
    mXdBScale = dbFlag;
    
    mMinX = minX;
    mMaxX = maxX;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetAutoAdjust(bool flag, double smoothCoeff)
{
    mAutoAdjustFlag = flag;
    
    mAutoAdjustParamSmoother.SetSmoothCoeff(smoothCoeff);
    
    mDirty = true;
    
    // Not sur we must still use mDirty
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetYScaleFactor(double factor)
{
    mYScaleFactor = factor;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetClearColor(int r, int g, int b, int a)
{
    SET_COLOR_FROM_INT(mClearColor, r, g, b, a);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveColor(int curveNum, int r, int g, int b)
{
    if (curveNum >= mNumCurves)
        return;
    
    SET_COLOR_FROM_INT(mCurves[curveNum]->mColor, r, g, b, 255);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveAlpha(int curveNum, double alpha)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mAlpha = alpha;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveLineWidth(int curveNum, double lineWidth)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mLineWidth = lineWidth;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveSmooth(int curveNum, bool flag)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mDoSmooth = flag;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveFill(int curveNum, bool flag)
{
    if (curveNum >= mNumCurves)
        return;

    mCurves[curveNum]->mCurveFill = flag;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveFillAlpha(int curveNum, double alpha)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mFillAlpha = alpha;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveFillAlphaUp(int curveNum, double alpha)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mFillAlphaUp = alpha;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}


void
GraphControl10::SetCurvePointSize(int curveNum, double pointSize)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mPointSize = pointSize;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveXScale(int curveNum, bool dbFlag, double minX, double maxX)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->SetXScale(dbFlag, minX, maxX);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurvePointStyle(int curveNum, bool flag, bool pointsAsLines)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mPointStyle = flag;
    mCurves[curveNum]->mPointsAsLines = pointsAsLines;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValuesPoint(int curveNum,
                                   const WDL_TypedBuf<double> &xValues,
                                   const WDL_TypedBuf<double> &yValues)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug); // ??
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    curve->mPointStyle = true;
    
    curve->ClearValues();
    
    //mCurves[curveNum]->SetPointValues(xValues, yValues);
    
    if (curveNum >= mNumCurves)
        return;
    
#if 0 // For points, we don't care about num values
    if (xValues.GetSize() < mNumCurveValues)
        // Something went wrong
        return;
#endif
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
#if 0
    for (int i = 0; i < mNumCurveValues; i++)
#endif
    for (int i = 0; i < xValues.GetSize(); i++)
    {
        double x = xValues.Get()[i];
        
        if (i >= yValues.GetSize())
            // Avoids a crash
            continue;
        
        double y = yValues.Get()[i];
        
        x = ConvertX(curve, x, width);
        y = ConvertY(curve, y, height);
        
        curve->mXValues.Get()[i] = x;
        curve->mYValues.Get()[i] = y;
        //SetCurveValuePoint(curveNum, x, y);
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValuesPointWeight(int curveNum,
                                         const WDL_TypedBuf<double> &xValues,
                                         const WDL_TypedBuf<double> &yValues,
                                         const WDL_TypedBuf<double> &weights)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug); // ??
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    curve->mPointStyle = true;
    
    curve->ClearValues();
    
    //mCurves[curveNum]->SetPointValues(xValues, yValues);
    
    if (curveNum >= mNumCurves)
        return;
    
#if 0 // For points, we don't care about num values
    if (xValues.GetSize() < mNumCurveValues)
        // Something went wrong
        return;
#endif
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
#if 0
    for (int i = 0; i < mNumCurveValues; i++)
#endif
        for (int i = 0; i < xValues.GetSize(); i++)
        {
            double x = xValues.Get()[i];
            double y = yValues.Get()[i];
            
            x = ConvertX(curve, x, width);
            y = ConvertY(curve, y, height);
            
            curve->mXValues.Get()[i] = x;
            curve->mYValues.Get()[i] = y;
            //SetCurveValuePoint(curveNum, x, y);
        }
    
    curve->mWeights = weights;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveColorWeight(int curveNum,
                                   const WDL_TypedBuf<double> &colorWeights)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug); // ??
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    
    curve->mWeights = colorWeights;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveSingleValueH(int curveNum, bool flag)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mSingleValueH = flag;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveSingleValueV(int curveNum, bool flag)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->mSingleValueV = flag;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DrawText(NVGcontext *vg, double x, double y, double fontSize,
                        const char *text, int color[4],
                        int halign, int valign)
{
    nvgSave(vg);
    
    nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "font");
    nvgFontBlur(vg, 0);
	nvgTextAlign(vg, halign | valign);
    
    int sColor[4] = { color[0], color[1], color[2], color[3] };
    SWAP_COLOR(sColor);
    
    nvgFillColor(vg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
	nvgText(vg, x, y, text, NULL);
    
    nvgRestore(vg);
}

void
GraphControl10::SetSpectrogram(BLSpectrogram3 *spectro,
                               double left, double top, double right, double bottom)
{
    if (mSpectrogramDisplay != NULL)
        delete mSpectrogramDisplay;
    
    mSpectrogramDisplay = new SpectrogramDisplay(mVg);
    mSpectrogramDisplay->SetSpectrogram(spectro,
                                        left, top, right, bottom);
}

SpectrogramDisplay *
GraphControl10::GetSpectrogramDisplay()
{
    return mSpectrogramDisplay;
}

void
GraphControl10::UpdateSpectrogram(bool updateData, bool updateFullData)
{
    mSpectrogramDisplay->UpdateSpectrogram(updateData, updateFullData);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::AddCustomDrawer(GraphCustomDrawer *customDrawer)
{
    mCustomDrawers.push_back(customDrawer);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::CustomDrawersPreDraw()
{
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    for (int i = 0; i < mCustomDrawers.size(); i++)
    {
        GraphCustomDrawer *drawer = mCustomDrawers[i];
        drawer->PreDraw(mVg, width, height);
    }
}

void
GraphControl10::CustomDrawersPostDraw()
{
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    for (int i = 0; i < mCustomDrawers.size(); i++)
    {
        GraphCustomDrawer *drawer = mCustomDrawers[i];
        drawer->PostDraw(mVg, width, height);
    }
}

void
GraphControl10::DrawSeparatorY0()
{
    if (!mSeparatorY0)
        return;
    
    nvgSave(mVg);
    nvgStrokeWidth(mVg, mSepY0LineWidth);
    
    int sepColor[4] = { mSepY0Color[0],  mSepY0Color[1],
                        mSepY0Color[2], mSepY0Color[3] };
    SWAP_COLOR(sepColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sepColor[0], sepColor[1], sepColor[2], sepColor[3]));
    
    int width = this->mRECT.W();
   
    // Draw a vertical line ath the bottom
    nvgBeginPath(mVg);
    
    double x0 = 0;
    double x1 = width;
    
    double y = mSepY0LineWidth/2.0;
                    
    nvgMoveTo(mVg, x0, y);
    nvgLineTo(mVg, x1, y);
                    
    nvgStroke(mVg);
    
    nvgRestore(mVg);
}

void
GraphControl10::AddCustomControl(GraphCustomControl *customControl)
{
    mCustomControls.push_back(customControl);
}

void
GraphControl10::OnMouseDown(int x, int y, IMouseMod* pMod)
{
    IControl::OnMouseDown(x, y, pMod);
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseDown(x, y, pMod);
    }
}

void
GraphControl10::OnMouseUp(int x, int y, IMouseMod* pMod)
{
    IControl::OnMouseUp(x, y, pMod);
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseUp(x, y, pMod);
    }
}

void
GraphControl10::OnMouseDrag(int x, int y, int dX, int dY, IMouseMod* pMod)
{
    IControl::OnMouseDrag(x, y, dX, dY, pMod);
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseDrag(x, y, dX, dY, pMod);
    }
}

bool
GraphControl10::OnMouseDblClick(int x, int y, IMouseMod* pMod)
{
    bool dblClickDone = IControl::OnMouseDblClick(x, y, pMod);
    if (!dblClickDone)
        return false;
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseDblClick(x, y, pMod);
    }
    
    return true;
}

void
GraphControl10::OnMouseWheel(int x, int y, IMouseMod* pMod, double d)
{
    IControl::OnMouseWheel(x, y, pMod, d);
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseWheel(x, y, pMod, d);
    }
}

bool
GraphControl10::OnKeyDown(int x, int y, int key, IMouseMod* pMod)
{
    IControl::OnKeyDown(x, y, key, pMod);
    
    bool res = false;
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        res = control->OnKeyDown(x, y, key, pMod);
    }
    
    return res;
}

void
GraphControl10::OnMouseOver(int x, int y, IMouseMod* pMod)
{
    IControl::OnMouseOver(x, y, pMod);
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseOver(x, y, pMod);
    }
}

void
GraphControl10::OnMouseOut()
{
    IControl::OnMouseOut();
    
    for (int i = 0; i < mCustomControls.size(); i++)
    {
        GraphCustomControl *control = mCustomControls[i];
        control->OnMouseOut();
    }
}

void
GraphControl10::InitGL()
{
    if (mGLInitialized)
        return;
	
    GLContext::Ref();
    GLContext *context = GLContext::Get();

	// FIX: Ableton 10 on Windows 10
	// The plugin didn't load because it crashed when scanned in the host
	//
	if (context == NULL)
		return;

    context->Bind();

    mGLInitialized = true;
}

void
GraphControl10::ExitGL()
{
    GLContext *context = GLContext::Get();
    
	// FIX: crash Ableton 10 Windows 10 (plugins not recognized)con 
	if (context != NULL)
		context->Unbind();

    GLContext::Unref();

    mGLInitialized = false;
}

void
GraphControl10::DBG_PrintCoords(int x, int y)
{
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    double tX = ((double)x)/width;
    double tY = ((double)y)/height;
    
    tX = (tX - mBounds[0])/(mBounds[2] - mBounds[0]);
    tY = (tY - mBounds[1])/(mBounds[3] - mBounds[1]);
    
    fprintf(stderr, "(%g, %g)\n", tX, 1.0 - tY);
}

void
GraphControl10::SetBackgroundImage(LICE_IBitmap *bmp)
{
    mBgImage = bmp;
    
#if 0 // CRASHES on Protools
    if (mNvgBackgroundImage != 0)
        nvgDeleteImage(mVg, mNvgBackgroundImage);
#endif
    
    mNvgBackgroundImage = 0;
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DisplayCurveDescriptions()
{
#define DESCR_X 40.0
#define DESCR_Y0 10.0
    
#define DESCR_WIDTH 20
#define DESCR_Y_STEP 12
#define DESCR_SPACE 5
    
#define TEXT_Y_OFFSET 2
    
    int descrNum = 0;
    for (int i = 0; i < mCurves.size(); i++)
    {
        GraphCurve4 *curve = mCurves[i];
        char *descr = curve->mDescription;
        if (descr == NULL)
            continue;
        
        double y = this->mRECT.H() - (DESCR_Y0 + descrNum*DESCR_Y_STEP);
        
        nvgSave(mVg);
        
        // Must force alpha to 1, because sometimes,
        // the plugins hide the curves, but we still
        // want to display the description
        double prevAlpha = curve->mAlpha;
        curve->mAlpha = 1.0;
        
        SetCurveDrawStyle(curve);
        
        curve->mAlpha = prevAlpha;
        
        nvgBeginPath(mVg);
        
        nvgMoveTo(mVg, DESCR_X, y);
        nvgLineTo(mVg, DESCR_X + DESCR_WIDTH, y);
        
        nvgStroke(mVg);
        
        DrawText(DESCR_X + DESCR_WIDTH + DESCR_SPACE, y + TEXT_Y_OFFSET,
                 FONT_SIZE, descr,
                 curve->mDescrColor,
                 NVG_ALIGN_LEFT, NVG_ALIGN_MIDDLE);
        
        nvgRestore(mVg);
        
        descrNum++;
    }
}

void
GraphControl10::AddAxis(GraphAxis *axis, char *data[][2], int numData, int axisColor[4],
                       int axisLabelColor[4],
                       double minVal, double maxVal, int axisOverlayColor[4])
{
    // Copy color
    for (int i = 0; i < 4; i++)
    {
        int sAxisColor[4] = { axisColor[0], axisColor[1], axisColor[2], axisColor[3] };
        SWAP_COLOR(sAxisColor);
        
        axis->mColor[i] = sAxisColor[i];
        
        int sLabelColor[4] = { axisLabelColor[0], axisLabelColor[1],
                               axisLabelColor[2], axisLabelColor[3] };
        SWAP_COLOR(sLabelColor);
        
        axis->mLabelColor[i] = sLabelColor[i];
        
        if (axisOverlayColor != NULL)
        {
            axis->mOverlay = true;
            
            int sOverColor[4] = { axisOverlayColor[0], axisOverlayColor[1],
                axisOverlayColor[2], axisOverlayColor[3] };
            SWAP_COLOR(sOverColor);
            
            axis->mOverlayColor[i] = sOverColor[i];
        }
    }
    
    // Copy data
    for (int i = 0; i < numData; i++)
    {
        char *cData[2] = { data[i][0], data[i][1] };
        
        double t = atof(cData[0]);
        
        string text(cData[1]);
        
        // Error here, if we add an Y axis, we must not use mMinXdB
        GraphAxisData aData;
        aData.mT = (t - minVal)/(maxVal - minVal);
        aData.mText = text;
        
        axis->mValues.push_back(aData);
    }
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveDescription(int curveNum, const char *description, int descrColor[4])
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->SetDescription(description, descrColor);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveLimitToBounds(int curveNum, bool flag)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->SetLimitToBounds(flag);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::ResetCurve(int curveNum, double val)
{
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->FillAllXValues(mMinX, mMaxX);
    mCurves[curveNum]->FillAllYValues(val);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveYScale(int curveNum, bool dbFlag, double minY, double maxY)
{
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->SetYScale(dbFlag, minY, maxY);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::CurveFillAllValues(int curveNum, double val)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->FillAllXValues(mMinX, mMaxX);
    mCurves[curveNum]->FillAllYValues(val);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValues(int curveNum, const WDL_TypedBuf<double> *values)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    mCurves[curveNum]->SetYValues(values, mMinX, mMaxX);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValues2(int curveNum, const WDL_TypedBuf<double> *values)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    if (values->GetSize() < mNumCurveValues)
        // Something went wrong
        return;
    
    for (int i = 0; i < mNumCurveValues; i++)
    {
        double t = ((double)i)/(values->GetSize() - 1);
        double y = values->Get()[i];
        
        SetCurveValue(curveNum, t, y);
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValues3(int curveNum, const WDL_TypedBuf<double> *values)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    curve->ClearValues();
    
    if (values->GetSize() == 0)
        return;
    
    for (int i = 0; i < mNumCurveValues; i++)
    {
        double t = ((double)i)/(values->GetSize() - 1);
        double y = values->Get()[i];
        
        SetCurveValue(curveNum, t, y);
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValuesDecimateSimple(int curveNum,
                                             const WDL_TypedBuf<double> *values)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    int width = this->mRECT.W();
    
    double prevX = -1.0;
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        double t = ((double)i)/(values->GetSize() - 1);
        
        double x = t*width;
        double y = values->Get()[i];
        
        if (x - prevX < 1.0)
            continue;
        
        prevX = x;
        
        SetCurveValue(curveNum, t, y);
    }
    
    // Avoid last value at 0 !
    // (would make a traversing line in the display)
    double lastValue = values->Get()[values->GetSize() - 1];
    SetCurveValue(curveNum, 1.0, lastValue);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}


void
GraphControl10::SetCurveValuesDecimate(int curveNum,
                                      const WDL_TypedBuf<double> *values,
                                      bool isWaveSignal)
{    
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    
    int width = this->mRECT.W();
    
    double prevX = -1.0;
    double maxY = -1.0;
    
    if (isWaveSignal)
        maxY = (curve->mMinY + curve->mMaxY)/2.0;
        
    double thrs = 1.0/GRAPHCONTROL_PIXEL_DENSITY;
    for (int i = 0; i < values->GetSize(); i++)
    {
        double t = ((double)i)/(values->GetSize() - 1);
        
        double x = t*width;
        double y = values->Get()[i];
        
        // Keep the maximum
        // (we prefer keeping the maxima, and not discard them)
        if (!isWaveSignal)
        {
            if (fabs(y) > maxY)
                maxY = y;
        }
        else
        {
            if (fabs(y) > fabs(maxY))
                maxY = y;
        }
        
        if (x - prevX < thrs)
            continue;
        
        prevX = x;
        
        SetCurveValue(curveNum, t, maxY);
        
        maxY = -1.0;
        
        if (isWaveSignal)
            maxY = (curve->mMinY + curve->mMaxY)/2.0;
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValuesDecimate2(int curveNum,
                                      const WDL_TypedBuf<double> *values,
                                      bool isWaveSignal)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    curve->ClearValues();
    
    if (values->GetSize() == 0)
        return;
    
    // Decimate
    
    //GRAPHCONTROL_PIXEL_DENSITY ?
    double decFactor = ((double)mNumCurveValues)/values->GetSize();
    
    WDL_TypedBuf<double> decimValues;
    if (isWaveSignal)
        Utils::DecimateSamples(&decimValues, *values, decFactor);
    else
        Utils::DecimateValues(&decimValues, *values, decFactor);
    
    for (int i = 0; i < decimValues.GetSize(); i++)
    {
        double t = ((double)i)/(decimValues.GetSize() - 1);
        
        double y = decimValues.Get()[i];
        
        SetCurveValue(curveNum, t, y);
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValuesDecimate3(int curveNum,
                                        const WDL_TypedBuf<double> *values,
                                        bool isWaveSignal)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    curve->ClearValues();
    
    if (values->GetSize() == 0)
        return;
    
    // Decimate
    
    //GRAPHCONTROL_PIXEL_DENSITY ?
    double decFactor = ((double)mNumCurveValues)/values->GetSize();
    
    WDL_TypedBuf<double> decimValues;
    if (isWaveSignal)
        Utils::DecimateSamples2(&decimValues, *values, decFactor);
    else
        Utils::DecimateValues(&decimValues, *values, decFactor);
    
    for (int i = 0; i < decimValues.GetSize(); i++)
    {
        double t = ((double)i)/(decimValues.GetSize() - 1);
        
        double y = decimValues.Get()[i];
        
        SetCurveValue(curveNum, t, y);
    }
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveValue(int curveNum, double t, double val)
{
    // Must lock otherwise we may have curve will jerk (??)
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    
    // Normalize, then adapt to the graph
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    double x = t;
    
    if (x != GRAPH_VALUE_UNDEFINED)
    {
        if (mXdBScale)
            x = Utils::NormalizedXTodB(x, mMinX, mMaxX);
        
        // X should be already normalize in input
        //else
        //    x = (x - mMinX)/(mMaxX - mMinX);
    
        // Scale for the interface
        x = x * width;
    }
    
    double y = ConvertY(curve, val, height);
    
    // For Ghost and mini view
    if (curve->mLimitToBounds)
    {
        if (y < (1.0 - mBounds[3])*height)
            y = (1.0 - mBounds[3])*height;
        
        if (y > (1.0 - mBounds[1])*height)
            y = (1.0 - mBounds[1])*height;
    }
    
    curve->SetValue(t, x, y);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

#if 0
void
GraphControl10::SetCurveValuePoint(int curveNum, double x, double y)
{
    if (curveNum >= mNumCurves)
        return;
    
    if (curveNum >= mNumCurves)
        return;
    
    GraphCurve4 *curve = mCurves[curveNum];
    
    double t = (x - curve->mMinX)/(curve->mMaxX - curve->mMinX);
    
    SetCurveValue(curveNum, t, y);
}
#endif

void
GraphControl10::SetCurveSingleValueH(int curveNum, double val)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    //mCurves[curveNum]->SetValue(0.0, 0.0, val);
    SetCurveValue(curveNum, 0.0, val);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveSingleValueV(int curveNum, double val)
{
    // Must lock otherwise with DATA_STEP, curve will jerk
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    //mCurves[curveNum]->SetValue(0.0, 0.0, val);
    SetCurveValue(curveNum, 0.0, val);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::PushCurveValue(int curveNum, double val)
{
    IPlugBase::IMutexLock lock(mPlug);
    
    if (curveNum >= mNumCurves)
        return;
    
    int height = this->mRECT.H();
    
    GraphCurve4 *curve = mCurves[curveNum];
    val = ConvertY(curve, val, height);
    
    double dummyX = 1.0;
    curve->PushValue(dummyX, val);
    
    double maxXValue = this->mRECT.W();
    curve->NormalizeXValues(maxXValue);
    
    mDirty = true;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DrawAxis(bool lineLabelFlag)
{
    if (mHAxis != NULL)
        DrawAxis(mHAxis, true, lineLabelFlag);
    
    if (mVAxis != NULL)
        DrawAxis(mVAxis, false, lineLabelFlag);
}

void
GraphControl10::DrawAxis(GraphAxis *axis, bool horizontal, bool lineLabelFlag)
{
    nvgSave(mVg);
    nvgStrokeWidth(mVg, 1.0);
    
    int axisColor[4] = { axis->mColor[0], axis->mColor[1], axis->mColor[2], axis->mColor[3] };
    SWAP_COLOR(axisColor);
    
    nvgStrokeColor(mVg, nvgRGBA(axisColor[0], axisColor[1], axisColor[2], axisColor[3]));
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    for (int i = 0; i < axis->mValues.size(); i++)
    {
        const GraphAxisData &data = axis->mValues[i];
        
        double t = data.mT;
        const char *text = data.mText.c_str();

        if (horizontal)
        {
            double textOffset = FONT_SIZE*0.2;
            
            if (mXdBScale)
                t = Utils::NormalizedXTodB(t, mMinX, mMaxX);
            //else
            //    t = (t - mMinX)/(mMaxX - mMinX);
            
            double x = t*width;
        
            // For Impulse
            ///bool validateFirst = true;
            
            //bool validateFirst = lineLabelFlag ? (t > 0.0) : (i > 0);
            if ((i > 0) && (i < axis->mValues.size() - 1))
            //if (validateFirst && (i < axis->mValues.size() - 1))
                // First and last: don't draw axis line
            {
                if (lineLabelFlag)
                {
                    // Draw a vertical line
                    nvgBeginPath(mVg);

                    double y0 = 0.0;
                    double y1 = height;
        
                    nvgMoveTo(mVg, x, y0);
                    nvgLineTo(mVg, x, y1);
    
                    nvgStroke(mVg);
                }
                else
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(x + OVERLAY_OFFSET,
                                 textOffset + axis->mOffsetY*height + OVERLAY_OFFSET,
                                 FONT_SIZE, text,
                                 axis->mOverlayColor,
                                 NVG_ALIGN_CENTER, NVG_ALIGN_BOTTOM);
                    }
                    
                    DrawText(x, textOffset + axis->mOffsetY*height, FONT_SIZE, text, axis->mLabelColor,
                             NVG_ALIGN_CENTER, NVG_ALIGN_BOTTOM);
                }
            }
     
            if (!lineLabelFlag)
            {
                if (i == 0)
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(x + textOffset + OVERLAY_OFFSET,
                                 textOffset + axis->mOffsetY*height + OVERLAY_OFFSET,
                                 FONT_SIZE, text,
                                 axis->mOverlayColor, NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                    }
                    
                    // First text: aligne left
                    DrawText(x + textOffset, textOffset + axis->mOffsetY*height, FONT_SIZE,
                             text, axis->mLabelColor, NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                }
        
                if (i == axis->mValues.size() - 1)
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(x - textOffset + OVERLAY_OFFSET,
                                 textOffset + axis->mOffsetY*height + OVERLAY_OFFSET,
                                 FONT_SIZE, text,
                                 axis->mOverlayColor, NVG_ALIGN_RIGHT, NVG_ALIGN_BOTTOM);
                    }
                    
                    // Last text: aligne right
                    DrawText(x - textOffset, textOffset + axis->mOffsetY*height,
                             FONT_SIZE, text, axis->mLabelColor, NVG_ALIGN_RIGHT, NVG_ALIGN_BOTTOM);
                }
            }
        }
        else
            // Vertical
        {
            double textOffset = FONT_SIZE*0.2;
            
            // Re-added dB normalization for Ghost
            if (!mCurves.empty())
            {
                if (mCurves[0]->mYdBScale)
                    t = Utils::NormalizedXTodB(t, mCurves[0]->mMinY, mCurves[0]->mMaxY);
            }
            
            t = ConvertToBoundsY(t);
            
            // Do not need to nomalize on the Y axis, because we want to display
            // the dB linearly (this is the curve values that are displayed exponentially).
            //t = Utils::NormalizedYTodB3(t, mMinXdB, mMaxXdB);
            
            // For Impulse
            //t = (t - mCurves[0]->mMinY)/(mCurves[0]->mMaxY - mCurves[0]->mMinY);
            
            double y = t*height;
            
            // Hack
            // See Transient, with it 2 vertical axis
            y += axis->mOffset;
            
            if ((i > 0) && (i < axis->mValues.size() - 1))
                // First and last: don't draw axis line
            {
                if (lineLabelFlag)
                {
                    // Draw a horizontal line
                    nvgBeginPath(mVg);
                
                    double x0 = 0.0;
                    double x1 = width;
                
                    nvgMoveTo(mVg, x0, y);
                    nvgLineTo(mVg, x1, y);
                
                    nvgStroke(mVg);

                }
                else
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(textOffset + axis->mOffsetX + OVERLAY_OFFSET,
                                 y + OVERLAY_OFFSET,
                                 FONT_SIZE, text, axis->mOverlayColor,
                                 NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, NVG_ALIGN_BOTTOM);
                    }
                    
                    DrawText(textOffset + axis->mOffsetX, y, FONT_SIZE, text, axis->mLabelColor, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, NVG_ALIGN_BOTTOM);
                }
            }
            
            if (!lineLabelFlag)
            {
                if (i == 0)
                    // First text: align "top"
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(textOffset + axis->mOffsetX + OVERLAY_OFFSET,
                                 y + FONT_SIZE*0.75 + OVERLAY_OFFSET,
                                 FONT_SIZE, text, axis->mOverlayColor,
                                 NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                    }
                    
                    DrawText(textOffset + axis->mOffsetX, y + FONT_SIZE*0.75, FONT_SIZE, text, axis->mLabelColor, NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                }
                
                if (i == axis->mValues.size() - 1)
                    // Last text: align "bottom"
                {
                    if (axis->mOverlay)
                    {
                        // Draw background text (for overlay)
                        DrawText(textOffset + axis->mOffsetX + OVERLAY_OFFSET,
                                 y - FONT_SIZE*1.5 + OVERLAY_OFFSET,
                                 FONT_SIZE, text, axis->mOverlayColor,
                                 NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                    }
                    
                    DrawText(textOffset + axis->mOffsetX, y - FONT_SIZE*1.5, FONT_SIZE, text, axis->mLabelColor, NVG_ALIGN_LEFT, NVG_ALIGN_BOTTOM);
                }
            }
        }
    }
    
    nvgRestore(mVg);
}

void
GraphControl10::DrawCurves()
{
    for (int i = 0; i < mNumCurves; i++)
    {
        if (!mCurves[i]->mSingleValueH && !mCurves[i]->mSingleValueV)
        {
            if (mCurves[i]->mPointStyle)
            {
                if (!mCurves[i]->mPointsAsLines)
                    DrawPointCurve(mCurves[i]);
                else
                    DrawPointCurveLines(mCurves[i]);
            }
            else if (mCurves[i]->mCurveFill)
            {
                DrawFillCurve(mCurves[i]);
                
#if FILL_CURVE_HACK
                DrawLineCurve(mCurves[i]);
#endif
            }
            else
            {
                DrawLineCurve(mCurves[i]);
            }
        }
        else
        {
            if (mCurves[i]->mSingleValueH)
            {
                if (!mCurves[i]->mYValues.GetSize() == 0)
                {
                    if (mCurves[i]->mCurveFill)
                    {
                        DrawFillCurveSVH(mCurves[i]);
                    
#if FILL_CURVE_HACK
                        DrawLineCurveSVH(mCurves[i]);
#endif
                    }
            
                    DrawLineCurveSVH(mCurves[i]);
                }
            }
            else
                if (mCurves[i]->mSingleValueV)
                {
                    if (!mCurves[i]->mXValues.GetSize() == 0)
                    {
                        if (mCurves[i]->mCurveFill)
                        {
                            DrawFillCurveSVV(mCurves[i]);
                            
#if FILL_CURVE_HACK
                            DrawLineCurveSVV(mCurves[i]);
#endif
                        }
                        
                        DrawLineCurveSVV(mCurves[i]);
                    }
                }
        }
    }
}

void
GraphControl10::DrawLineCurve(GraphCurve4 *curve)
{
#if CURVE_DEBUG
    int numPointsDrawn = 0;
#endif
    
    nvgSave(mVg);
    
    SetCurveDrawStyle(curve);
    
    nvgBeginPath(mVg);
    
#if CURVE_OPTIM
    double prevX = -1.0;
#endif
    
    bool firstPoint = true;
    for (int i = 0; i < curve->mXValues.GetSize(); i ++)
    {
        double x = curve->mXValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
#if CURVE_OPTIM
        if (mNumCurveValues >= CURVE_OPTIM_THRS)
        {
            if (x - prevX < 1.0)
                // Less than 1 pixel. Do not display.
                continue;
        }
        
        prevX = x;
#endif
        
#if CURVE_DEBUG
        numPointsDrawn++;
#endif
        
        double y = curve->mYValues.Get()[i];
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
        
        if (firstPoint)
        {
            nvgMoveTo(mVg, x, y);
            
            firstPoint = false;
        }
        
        nvgLineTo(mVg, x, y);
    }
    
    nvgStroke(mVg);
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
    
#if CURVE_DEBUG
    fprintf(stderr, "GraphControl10::DrawLineCurve - num points: %d\n", numPointsDrawn);
#endif
}

#if !FILL_CURVE_HACK
        // Bug with direct rendering
        // It seems we have no stencil, and we can only render convex polygons
void
GraphControl10::DrawFillCurve(GraphCurve4 *curve)
{
    // TEST for compilation
    double lineWidth = 1.0;
    
    // Offset used to draw the closing of the curve outside the viewport
    // Because we draw both stroke and fill at the same time
#define OFFSET lineWidth
    
    nvgSave(mVg);
    
    int sFillColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
                      (int)(curve->mColor[2]*255), (int)(curve->mFillAlpha*255) };
    SWAP_COLOR(sFillColor);
    
    int sStrokeColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mAlpha*255) };
    SWAP_COLOR(sStrokeColor);
    
    nvgBeginPath(mVg);
    
    double x0 = 0.0;
    for (int i = 0; i < curve->mXValues.GetSize(); i ++)
    {        
        double x = curve->mXValues.Get()[i];
        double y = curve->mYValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
        
#if CURVE_DEBUG
        numPointsDrawn++;
#endif
        
        if (i == 0)
        {
            x0 = x;
            
            nvgMoveTo(mVg, x0 - OFFSET, 0 - OFFSET);
            nvgLineTo(mVg, x - OFFSET, y);
        }
        
        nvgLineTo(mVg, x, y);
        
        if (i >= curve->mXValues.GetSize() - 1)
            // Close
        {
            nvgLineTo(mVg, x + OFFSET, y);
            nvgLineTo(mVg, x + OFFSET, 0 - OFFSET);
            
            nvgClosePath(mVg);
        }
    }
    
    nvgFillColor(mVg, nvgRGBA(sFillColor[0], sFillColor[1], sFillColor[2], sFillColor[3]));
	nvgFill(mVg);
    
    nvgStrokeColor(mVg, nvgRGBA(sStrokeColor[0], sStrokeColor[1], sStrokeColor[2], sStrokeColor[3]));
    nvgStrokeWidth(mVg, lineWidth);
    nvgStroke(mVg);

    nvgRestore(mVg);
    
    mDirty = true;

    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif

}
#endif

#if FILL_CURVE_HACK
// Due to a bug, we render only convex polygons when filling curves
// So we separate in rectangles
void
GraphControl10::DrawFillCurve(GraphCurve4 *curve)
{
#if CURVE_DEBUG
    int numPointsDrawn = 0;
#endif
    
    // Offset used to draw the closing of the curve outside the viewport
    // Because we draw both stroke and fill at the same time
#define OFFSET lineWidth
    
    nvgSave(mVg);
    
    int sFillColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mFillAlpha*255) };
    SWAP_COLOR(sFillColor);
    
    int sStrokeColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mAlpha*255) };
    SWAP_COLOR(sStrokeColor);
    
    double prevX = -1.0;
    double prevY = 0.0;
    for (int i = 0; i < curve->mXValues.GetSize() - 1; i ++)
    {
        double x = curve->mXValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
        double y = curve->mYValues.Get()[i];
        
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
        
        if (prevX < 0.0)
            // Init
        {
            prevX = x;
            prevY = y;
            
            continue;
        }
        
        // Avoid any overlap
        // The problem can bee seen using alpha
        if (x - prevX < 2.0) // More than 1.0
            continue;
        
#if FILL_CURVE_HACK2
        // Go back from 1 pixel
        // (otherwise there will be line holes between filles polygons...)
        prevX -= 1.0;
#endif
        nvgBeginPath(mVg);
        
        nvgMoveTo(mVg, prevX, prevY);
        nvgLineTo(mVg, x, y);
        
        nvgLineTo(mVg, x, 0);
        nvgLineTo(mVg, prevX, 0);
        nvgLineTo(mVg, prevX, prevY);
        
        nvgClosePath(mVg);
        
        nvgFillColor(mVg, nvgRGBA(sFillColor[0], sFillColor[1], sFillColor[2], sFillColor[3]));
        nvgFill(mVg);
                  
        prevX = x;
        prevY = y;
    }
    
    // Fill upside if necessary
    if (curve->mFillAlphaUp)
        // Fill the upper area with specific alpha
    {
        int sFillColorUp[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
            (int)(curve->mColor[2]*255), (int)(curve->mFillAlphaUp*255) };
        SWAP_COLOR(sFillColorUp);
        
        int height = this->mRECT.H();
        
        double prevX = -1.0;
        double prevY = 0.0;
        for (int i = 0; i < curve->mXValues.GetSize() - 1; i ++)
        {
            double x = curve->mXValues.Get()[i];
            
            if (x == GRAPH_VALUE_UNDEFINED)
                continue;
            
            double y = curve->mYValues.Get()[i];
            
            if (y == GRAPH_VALUE_UNDEFINED)
                continue;
            
            if (prevX < 0.0)
                // Init
            {
                prevX = x;
                prevY = y;
                
                continue;
            }
            
            // Avoid any overlap
            // The problem can bee seen using alpha
            if (x - prevX < 2.0) // More than 1.0
                continue;
            
            nvgBeginPath(mVg);
            
            nvgMoveTo(mVg, prevX, prevY);
            nvgLineTo(mVg, x, y);
            
            nvgLineTo(mVg, x, height);
            nvgLineTo(mVg, prevX, height);
            nvgLineTo(mVg, prevX, prevY);
            
            nvgClosePath(mVg);
            
            nvgFillColor(mVg, nvgRGBA(sFillColorUp[0], sFillColorUp[1], sFillColorUp[2], sFillColorUp[3]));
            nvgFill(mVg);
            
            prevX = x;
            prevY = y;
        }
    }
    
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
    
#if CURVE_DEBUG
    fprintf(stderr, "GraphControl10::DrawFillCurve - num points: %d\n", numPointsDrawn);
#endif
}
#endif

void
GraphControl10::DrawLineCurveSVH(GraphCurve4 *curve)
{
    if (curve->mYValues.GetSize() == 0)
        return;
    
    double val = curve->mYValues.Get()[0];
    
    if (val == GRAPH_VALUE_UNDEFINED)
        return;
    
    int width = this->mRECT.W();
    
    nvgSave(mVg);
    nvgStrokeWidth(mVg, curve->mLineWidth);
    
    int sColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
                      (int)(curve->mColor[2]*255), (int)(curve->mAlpha*255) };
    SWAP_COLOR(sColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    nvgBeginPath(mVg);
    
    double x0 = 0;
    double x1 = width;
    
    nvgMoveTo(mVg, x0, val);
    nvgLineTo(mVg, x1, val);
    
    nvgStroke(mVg);
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DrawFillCurveSVH(GraphCurve4 *curve)
{
    if (curve->mYValues.GetSize() == 0)
        return;
    
    double val = curve->mYValues.Get()[0];
    
    if (val == GRAPH_VALUE_UNDEFINED)
        return;
    
    int width = this->mRECT.W();
    
    nvgSave(mVg);
    
    int sColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
                      (int)(curve->mColor[2]*255), (int)(curve->mFillAlpha*255) };
    SWAP_COLOR(sColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    nvgBeginPath(mVg);
        
    double x0 = 0;
    double x1 = width;
    double y0 = 0;
    double y1 = val;
    
    nvgMoveTo(mVg, x0, y0);
    nvgLineTo(mVg, x0, y1);
    nvgLineTo(mVg, x1, y1);
    nvgLineTo(mVg, x1, y0);
    
    nvgClosePath(mVg);
    
    nvgFillColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
	nvgFill(mVg);
    
    nvgStroke(mVg);
    
    // Fill upside if necessary
    if (curve->mFillAlphaUp)
        // Fill the upper area with specific alpha
    {
        int height = this->mRECT.H();
        
        int sFillColorUp[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
                                (int)(curve->mColor[2]*255), (int)(curve->mFillAlphaUp*255) };
        SWAP_COLOR(sFillColorUp);
        
        nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
        
        nvgBeginPath(mVg);
        
        double x0 = 0;
        double x1 = width;
        double y0 = height;
        double y1 = val;
        
        nvgMoveTo(mVg, x0, y0);
        nvgLineTo(mVg, x0, y1);
        nvgLineTo(mVg, x1, y1);
        nvgLineTo(mVg, x1, y0);
        
        nvgClosePath(mVg);
        
        nvgFillColor(mVg, nvgRGBA(sFillColorUp[0], sFillColorUp[1],
                                  sFillColorUp[2], sFillColorUp[3]));
        nvgFill(mVg);
    }
    
    
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DrawLineCurveSVV(GraphCurve4 *curve)
{
    // Finally, take the Y value
    // We will have to care about the curve Y scale !
    if (curve->mYValues.GetSize() == 0)
        return;
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    double val = curve->mYValues.Get()[0];
    
    if (val == GRAPH_VALUE_UNDEFINED)
        return;
    
    // Hack...
    val /= height;
    val *= width;
    
    nvgSave(mVg);
    nvgStrokeWidth(mVg, curve->mLineWidth);
    
    int sColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mAlpha*255) };
    SWAP_COLOR(sColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    nvgBeginPath(mVg);
    
    double y0 = 0;
    double y1 = height;
    
    double x = val;
    
    nvgMoveTo(mVg, x, y0);
    nvgLineTo(mVg, x, y1);
    
    nvgStroke(mVg);
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

// Fill right
// (only, for the moment)
void
GraphControl10::DrawFillCurveSVV(GraphCurve4 *curve)
{
    // Finally, take the Y value
    // We will have to care about the curve Y scale !
    if (curve->mYValues.GetSize() == 0)
        return;
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    double val = curve->mYValues.Get()[0];
    
    if (val == GRAPH_VALUE_UNDEFINED)
        return;
    
    // Hack...
    val /= height;
    val *= width;
    
    nvgSave(mVg);
    
    int sColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mFillAlpha*255) };
    SWAP_COLOR(sColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    nvgBeginPath(mVg);
    
    double y0 = 0;
    double y1 = height;
    double x0 = val;
    double x1 = width;
    
    nvgMoveTo(mVg, x0, y0);
    nvgLineTo(mVg, x0, y1);
    nvgLineTo(mVg, x1, y1);
    nvgLineTo(mVg, x1, y0);
    
    nvgClosePath(mVg);
    
    nvgFillColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
	nvgFill(mVg);
    
    nvgStroke(mVg);
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

#if 0 // OLD ? => TODO: check and remove
// TEST: draw lines instead of points
void
GraphControl10::DrawPointCurveLines(GraphCurve4 *curve)
{
    nvgSave(mVg);
    
    SetCurveDrawStyle(curve);
    
    int width = this->mRECT.W();
    double pointSize = curve->mPointSize; ///width;
    
    nvgBeginPath(mVg);
    
    bool firstPoint = true;
    for (int i = 0; i < curve->mXValues.GetSize(); i ++)
    {
        double x = curve->mXValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
        double y = curve->mYValues.Get()[i];
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
        
        if (firstPoint)
        {
            nvgMoveTo(mVg, x, y);
        
            firstPoint = false;
        }
        
        nvgLineTo(mVg, x, y);
    }
    
    nvgStroke(mVg);
    
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}
#endif

// Optimized !
void
GraphControl10::DrawPointCurve(GraphCurve4 *curve)
{
#define FILL_RECTS 1
    
    nvgSave(mVg);
    
    SetCurveDrawStyle(curve);
    
    double pointSize = curve->mPointSize;
    
#if !FILL_RECTS
    nvgBeginPath(mVg);
#endif
    
    for (int i = 0; i < curve->mXValues.GetSize(); i ++)
    {
        double x = curve->mXValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
        double y = curve->mYValues.Get()[i];
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
    
        if (curve->mWeights.GetSize() == curve->mXValues.GetSize())
        {
            double weight = curve->mWeights.Get()[i];
            SetCurveDrawStyleWeight(curve, weight);
        }
      
        // FIX: when points are very big, they are not centered
        x -= pointSize/2.0;
        y -= pointSize/2.0;
        
#if FILL_RECTS
        nvgBeginPath(mVg);
#endif
        nvgRect(mVg, x, y, pointSize, pointSize);
        
#if FILL_RECTS
        nvgFill(mVg);
#endif
    }
    
#if !FILL_RECTS
    nvgStroke(mVg);
#endif
    
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

// TEST (to display lines instead of points)
void
GraphControl10::DrawPointCurveLines(GraphCurve4 *curve)
{    
    nvgSave(mVg);
    
    SetCurveDrawStyle(curve);
    
    double pointSize = curve->mPointSize;
    nvgStrokeWidth(mVg, pointSize);
    
    int width = this->mRECT.W();
    
#if !FILL_RECTS
    nvgBeginPath(mVg);
#endif
    
    for (int i = 0; i < curve->mXValues.GetSize(); i ++)
    {
        double x = curve->mXValues.Get()[i];
        
        if (x == GRAPH_VALUE_UNDEFINED)
            continue;
        
        double y = curve->mYValues.Get()[i];
        if (y == GRAPH_VALUE_UNDEFINED)
            continue;
        
        nvgBeginPath(mVg);
        nvgMoveTo(mVg, width/2.0, 0.0);
        nvgLineTo(mVg, x, y);
        nvgStroke(mVg);
    }
    
    nvgRestore(mVg);
    
    mDirty = true;
    
    // ??
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::AutoAdjust()
{
    // First, compute the maximum value of all the curves
    double max = -1e16;
    for (int i = 0; i < mNumCurves; i++)
    {
        GraphCurve4 *curve = mCurves[i];
        
        for (int j = curve->mYValues.GetSize(); j >= 0; j--)
        {
            double val = curve->mYValues.Get()[j];
            
            if (val > max)
                max = val;
        }
    }
    
    // Compute the scale factor
    
    // keep a margin, do not scale to the maximum
    double margin = 0.25;
    double factor = 1.0;
    
    if (max > 0.0)
         factor = (1.0 - margin)/max;
    
    // Then smooth the factor
    mAutoAdjustParamSmoother.SetNewValue(factor);
    mAutoAdjustParamSmoother.Update();
    factor = mAutoAdjustParamSmoother.GetCurrentValue();
    
    mAutoAdjustFactor = factor;
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

double
GraphControl10::MillisToPoints(long long int elapsed, int sampleRate, int numSamplesPoint)
{
    double numSamples = (((double)elapsed)/1000.0)*sampleRate;
    
    double numPoints = numSamples/numSamplesPoint;
    
    return numPoints;
}

void
GraphControl10::InitFont(const char *fontPath)
{
#ifndef WIN32
    nvgCreateFont(mVg, "font", fontPath);

	mFontInitialized = true;
#else //  On windows, resources are not external files 
	
	// Load the resource in memory, then create the nvg font

	IGraphicsWin *graphics = (IGraphicsWin *)GetGUI();
	if (graphics == NULL)
		return;

	HINSTANCE instance = graphics->GetHInstance();
	if (instance == NULL)
		return;

	HRSRC fontResource = ::FindResource(instance, MAKEINTRESOURCE(FONT_ID), RT_RCDATA);

	HMODULE module = instance;

	unsigned int fontResourceSize = ::SizeofResource(module, fontResource);
	HGLOBAL fontResourceData = ::LoadResource(module, fontResource);
	void* pBinaryData = ::LockResource(fontResourceData);

	if (pBinaryData == NULL)
		return;

	unsigned char* data = (unsigned char*)malloc(fontResourceSize);
	int ndata = fontResourceSize;
	memcpy(data, fontResourceData, ndata);

	nvgCreateFontMem(mVg, "font", data, ndata, 1);

	mFontInitialized = true;
#endif
}

void
GraphControl10::DrawText(double x, double y, double fontSize,
                        const char *text, int color[4],
                        int halign, int valign)
{
    DrawText(mVg, x, y, fontSize, text, color, halign, valign);
}

void
GraphControl10::DrawGraph()
{
    IPlugBase::IMutexLock lock(mPlug);
    
    // Update first, before displaying
    if (mSpectrogramDisplay != NULL)
    {
        bool updated = mSpectrogramDisplay->DoUpdateSpectrogram();
        if (updated)
        {
            mDirty = true;
            
#if DIRTY_OPTIM
            mMyDirty = true;
#endif
        }
    }
    
    if ((mBgImage != NULL) && (mNvgBackgroundImage == 0))
        UpdateBackgroundImage();
    
	// On Windows, we need lazy evaluation, because we need HInstance and IGraphics
	if (!mFontInitialized)
		InitFont(NULL);
    
    if (mLiceFb == NULL)
    {
        // Take care of the following macro in Lice ! : DISABLE_LICE_EXTENSIONS
        
        mLiceFb = new LICE_GL_SysBitmap(0, 0);
        
        IRECT *r = GetRECT();
        mLiceFb->resize(r->W(), r->H());
    }
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();
    
    // Clear with Lice, because BitBlt needs it.
    unsigned char clearColor[4] = { (unsigned char)(mClearColor[0]*255), (unsigned char)(mClearColor[1]*255),
                                    (unsigned char)(mClearColor[2]*255), (unsigned char)(mClearColor[3]*255) };

    // Valgrind: error here ?
    
    // Clear the GL Bitmap and bind the FBO at the same time !
    LICE_Clear(mLiceFb, *((LICE_pixel *)&clearColor));
    
    // Set pixel ratio to 1.0
    // Otherwise, fonts will be blurry,
    // and lines too I guess...
    nvgBeginFrame(mVg, width, height, 1.0);
    
    DrawBackgroundImage();
    
    if (mAutoAdjustFlag)
    {
        AutoAdjust();
    }
    
    CustomDrawersPreDraw();
    
    // Draw spectrogram first
    //nvgSave(mVg);
    
    // New: set colormap only in the spectrogram state
//#if GLSL_COLORMAP
//    nvgSetColormap(mVg, mNvgColormapImage);
//#endif
    
    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->DrawSpectrogram(width, height);
    
    //nvgRestore(mVg);
    
    DrawAxis(true);
    
    nvgSave(mVg);
    
    DrawCurves();
    
    nvgRestore(mVg);
    
    if (mSpectrogramDisplay != NULL)
    {
        // Display MiniView after curves
        // So we have the mini waveform, then the selection over
        mSpectrogramDisplay->DrawMiniView(width, height);
    }
    
    DrawAxis(false);
    
    DisplayCurveDescriptions();
    
    //DrawSpectrogram();
    
    CustomDrawersPostDraw();
    
    DrawSeparatorY0();
    
    nvgEndFrame(mVg);
}

double
GraphControl10::ConvertX(GraphCurve4 *curve, double val, double width)
{
    double x = val;
    if (x != GRAPH_VALUE_UNDEFINED)
    {
        if (curve->mXdBScale)
        {
            if (val > 0.0)
                // Avoid -INF values
                x = Utils::NormalizedYTodB(x, curve->mMinX, curve->mMaxX);
        }
        else
            x = (x - curve->mMinX)/(curve->mMaxX - curve->mMinX);
        
        //x = x * mAutoAdjustFactor * mXScaleFactor * width;
        x = x * width;
    }
    
    return x;
}

double
GraphControl10::ConvertY(GraphCurve4 *curve, double val, double height)
{
    double y = val;
    if (y != GRAPH_VALUE_UNDEFINED)
    {
        if (curve->mYdBScale)
        {
            if (val > 0.0)
            // Avoid -INF values
                y = Utils::NormalizedYTodB(y, curve->mMinY, curve->mMaxY);
        }
        else
            y = (y - curve->mMinY)/(curve->mMaxY - curve->mMinY);
        
        y = y * mAutoAdjustFactor * mYScaleFactor * height;
    }
    
    return y;
}

void
GraphControl10::SetCurveDrawStyle(GraphCurve4 *curve)
{
    nvgStrokeWidth(mVg, curve->mLineWidth);
    
    int sColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mAlpha*255) };
    SWAP_COLOR(sColor);

    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    int sFillColor[4] = { (int)(curve->mColor[0]*255), (int)(curve->mColor[1]*255),
        (int)(curve->mColor[2]*255), (int)(curve->mFillAlpha*255) };
    SWAP_COLOR(sFillColor);
    
    nvgFillColor(mVg, nvgRGBA(sFillColor[0], sFillColor[1], sFillColor[2], sFillColor[3]));
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::SetCurveDrawStyleWeight(GraphCurve4 *curve, double weight)
{
    //nvgStrokeWidth(mVg, curve->mLineWidth);
    
    int sColor[4] = { (int)(curve->mColor[0]*255*weight), (int)(curve->mColor[1]*255*weight),
                      (int)(curve->mColor[2]*255*weight), (int)(curve->mAlpha*255*weight) };
    for (int i = 0; i < 4; i++)
    {
        if (sColor[i] < 0)
            sColor[i] = 0;
        
        if (sColor[i] > 255)
            sColor[i] = 255;
    }
    SWAP_COLOR(sColor);
    
    nvgStrokeColor(mVg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));
    
    int sFillColor[4] = { (int)(curve->mColor[0]*255*weight), (int)(curve->mColor[1]*255*weight),
                          (int)(curve->mColor[2]*255*weight), (int)(curve->mFillAlpha*255*weight) };
    for (int i = 0; i < 4; i++)
    {
        if (sFillColor[i] < 0)
            sFillColor[i] = 0;
        
        if (sFillColor[i] > 255)
            sFillColor[i] = 255;
    }
    SWAP_COLOR(sFillColor);
    
    nvgFillColor(mVg, nvgRGBA(sFillColor[0], sFillColor[1], sFillColor[2], sFillColor[3]));
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

double
GraphControl10::ConvertToBoundsX(double t)
{
    // Rescale
    t *= (mBounds[2] - mBounds[0]);
    t += mBounds[0];
    
    // Clip
    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;
    
    return t;
}

double
GraphControl10::ConvertToBoundsY(double t)
{    
    // Rescale
    t *= (mBounds[3] - mBounds[1]);
    t += (1.0 - mBounds[3]);
    
    // Clip
    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;
    
    return t;
}

void
GraphControl10::UpdateBackgroundImage()
{
    if (mBgImage == NULL)
        return;
    
    int w = mBgImage->getWidth();
    int h = mBgImage->getHeight();
    
    LICE_pixel *bits = mBgImage->getBits();
    
    mNvgBackgroundImage = nvgCreateImageRGBA(mVg,
                                             w, h,
                                             NVG_IMAGE_NEAREST, // 0
                                             (unsigned char *)bits);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::DrawBackgroundImage()
{
    if (mNvgBackgroundImage == 0)
        return;
    
    int width = this->mRECT.W();
    int height = this->mRECT.H();

    nvgSave(mVg);
    
    // Flip upside down !
    double bounds[4] = { 0.0, 1.0, 1.0, 0.0 };
    
    double alpha = 1.0;
    NVGpaint fullImgPaint = nvgImagePattern(mVg,
                                            bounds[0]*width, bounds[1]*height,
                                            (bounds[2] - bounds[0])*width,
                                            (bounds[3] - bounds[1])*height,
                                            0.0, mNvgBackgroundImage,
                                            alpha);
    nvgBeginPath(mVg);
    
    // Corner (x, y) => bottom-left
    nvgRect(mVg,
            bounds[0]*width, bounds[1]*height,
            (bounds[2] - bounds[0])*width,
            (bounds[3] - bounds[1])*height);
    
    nvgFillPaint(mVg, fullImgPaint);
    nvgFill(mVg);
    
    nvgRestore(mVg);
    
#if DIRTY_OPTIM
    mMyDirty = true;
#endif
}

void
GraphControl10::InitNanoVg()
{
	if (!mGLInitialized)
		return;
	
    // NVG_NIKO_ANTIALIAS_SKIP_FRINGES :
    // For Spectrogram, colormap, and display one image over the other
    // => The borders of the image were black
    mVg = nvgCreateGL2(NVG_ANTIALIAS
                       | NVG_NIKO_ANTIALIAS_SKIP_FRINGES
                       | NVG_STENCIL_STROKES
                       //| NVG_DEBUG
                       );
	if (mVg == NULL)
		return;
    
    InitFont(mFontPath.Get());
}

void
GraphControl10::ExitNanoVg()
{
	if (mVg != NULL)
		nvgDeleteGL2(mVg);

	// Just in case
	// NOTE: added for Mixcraft crash fix
	mVg = NULL;
}

