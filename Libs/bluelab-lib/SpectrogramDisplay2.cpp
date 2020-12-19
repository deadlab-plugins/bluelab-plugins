//
//  SpectrogramDisplay2.cpp
//  BL-Ghost
//
//  Created by Pan on 14/06/18.
//
//

#ifdef IGRAPHICS_NANOVG

#include <BLSpectrogram4.h>
//#include <MiniView.h>

#include "SpectrogramDisplay2.h"

#define USE_ZOOM_NVG_TRANSFORM 1

#define USE_SPECTRO_NEAREST 0

SpectrogramDisplay2::SpectrogramDisplay2(SpectrogramDisplayState *state)
{
    mState = state;
    if (mState == NULL)
    {
        mState = new SpectrogramDisplayState();
        
        mState->mCenterPos = 0.5;
        mState->mAbsTranslation = 0.0;
        
        mState->mMinX = 0.0;
        mState->mMaxX = 1.0;
        
        mState->mAbsMinX = 0.0;
        mState->mAbsMaxX = 1.0;
    }
    
    mVg = NULL;
    
    // Spectrogram
    mSpectrogram = NULL;
    mNvgSpectroImage = 0;
    mNeedUpdateSpectrogram = false;
    mNeedUpdateSpectrogramData = false;
    mNvgColormapImage = 0;
    
    mNeedUpdateSpectrogramFullData = false;
    mNvgSpectroFullImage = 0;
    
    mShowSpectrogram = false;
    
    //mSpectrogramGain = 1.0;
    
    mSpectrogramAlpha = 1.0;
    
    //mMiniView = NULL;
    
    mDrawBGSpectrogram = true;
    
    mNeedRedraw = true;
}

SpectrogramDisplay2::~SpectrogramDisplay2()
{
    if (mVg == NULL)
      return;
  
    if (mNvgSpectroImage != 0)
        nvgDeleteImage(mVg, mNvgSpectroImage);
    
    if (mNvgColormapImage != 0)
        nvgDeleteImage(mVg, mNvgColormapImage);
    
    if (mNvgSpectroFullImage != 0)
        nvgDeleteImage(mVg, mNvgSpectroFullImage);
}

SpectrogramDisplay2::SpectrogramDisplayState *
SpectrogramDisplay2::GetState()
{
    return mState;
}

void
SpectrogramDisplay2::Reset()
{
    mSpectroImageData.Resize(0);
    
    mNeedUpdateSpectrogram = true;   
    mNeedUpdateSpectrogramData = true;
    
    // FIX: with this line uncommented, there is a bug:
    // the background is updated at each edit but with the current zoom
#if 0
    mNeedUpdateSpectrogramFullData = true;
#endif
    
    mNeedRedraw = true;
}

bool
SpectrogramDisplay2::NeedUpdateSpectrogram()
{
    return mNeedUpdateSpectrogram;
}

bool
SpectrogramDisplay2::DoUpdateSpectrogram()
{
    if (mVg == NULL)
      return true;
  
    // Update first, before displaying
    if (!mNeedUpdateSpectrogram)
      return false;
    
    //int w = mSpectrogram->GetMaxNumCols();
    int w = mSpectrogram->GetNumCols(); // ??
    int h = mSpectrogram->GetHeight();
    
#if 0 //1 // Avoid white image when there is no data
    if ((w == 0) ||
        (mNvgSpectroImage == 0) || (mNvgSpectroFullImage == 0))
    {
        w = 1;
        int imageSize = w*h*4;
        
        mSpectroImageData.Resize(imageSize);
        memset(mSpectroImageData.Get(), 0, imageSize);
        
        // TEST
        //mSpectrogram->GetImageDataFloat(mSpectroImageData.Get());
        
        // Spectrogram image
        if (mNvgSpectroImage != 0)
            nvgDeleteImage(mVg, mNvgSpectroImage);
        
        mNvgSpectroImage = nvgCreateImageRGBA(mVg,
                                              w, h,
#if USE_SPECTRO_NEAREST
                                              NVG_IMAGE_NEAREST |
#endif
                                              NVG_IMAGE_ONE_FLOAT_FORMAT,
                                              mSpectroImageData.Get());
        // Spectrogram full image
        if (mNvgSpectroFullImage != 0)
            nvgDeleteImage(mVg, mNvgSpectroFullImage);
        
        mNvgSpectroFullImage = nvgCreateImageRGBA(mVg,
                                                  w, h,
#if USE_SPECTRO_NEAREST
                                                  NVG_IMAGE_NEAREST |
#endif
                                                  NVG_IMAGE_ONE_FLOAT_FORMAT,
                                                  mSpectroImageData.Get());
        
        mNeedUpdateSpectrogram = false;
        mNeedUpdateSpectrogramData = false;
        mNeedUpdateSpectrogramFullData = false;
        
        return true;
    }
#endif
    
    // Avoid white image when there is no data
    if (mSpectroImageData.GetSize() == 0)
    {
        mSpectroImageData.Resize(4);
        memset(mSpectroImageData.Get(), 0, 4);
        
        mNeedUpdateSpectrogramData = true;
        mNeedUpdateSpectrogramFullData = true;
    }
    
    int imageSize = w*h*4;
    
    if (mNeedUpdateSpectrogramData ||
        (mNvgSpectroImage == 0) || (mNvgSpectroFullImage == 0))
    {
        bool imageCreated = false;
        if ((mSpectroImageData.GetSize() != imageSize) ||
            (mNvgSpectroImage == 0))
        {
            mSpectroImageData.Resize(imageSize);
            
            memset(mSpectroImageData.Get(), 0, imageSize);
            mSpectrogram->GetImageDataFloat(mSpectroImageData.Get());
            
            // Spectrogram image
            if (mNvgSpectroImage != 0)
                nvgDeleteImage(mVg, mNvgSpectroImage);
            
            if (mState->mSpectroImageFullData.GetSize() == 0)
            {
                mState->mSpectroImageFullData = mSpectroImageData;
            }
            
            mNvgSpectroImage = nvgCreateImageRGBA(mVg,
                                                  w, h,
#if USE_SPECTRO_NEAREST
                                                  NVG_IMAGE_NEAREST |
#endif
                                                  NVG_IMAGE_ONE_FLOAT_FORMAT,
                                                  mSpectroImageData.Get());
            
            imageCreated = true;
        }
        
        bool fullImageCreated = false;
        if (mNeedUpdateSpectrogramFullData || (mNvgSpectroFullImage == 0))
        {
            // Spectrogram full image
            if (mNvgSpectroFullImage != 0)
                nvgDeleteImage(mVg, mNvgSpectroFullImage);
                
            mNvgSpectroFullImage = nvgCreateImageRGBA(mVg,
                                                      w, h,
#if USE_SPECTRO_NEAREST
                                                      NVG_IMAGE_NEAREST |
#endif
                                                      NVG_IMAGE_ONE_FLOAT_FORMAT,
                                                      //mSpectroImageData.Get());
                                                      mState->mSpectroImageFullData.Get());
            
            
            fullImageCreated = true;

        }
        
        if (!imageCreated || !fullImageCreated)
        {
            memset(mSpectroImageData.Get(), 0, imageSize);
            bool updated = mSpectrogram->GetImageDataFloat(mSpectroImageData.Get());

            if (updated)
            {
                if (!imageCreated)
                {
                    // Spectrogram image
                    nvgUpdateImage(mVg, mNvgSpectroImage, mSpectroImageData.Get());
                }
                
                if (!fullImageCreated)
                {
                    // Spectrogram full image
                    if (mNeedUpdateSpectrogramFullData)
                        nvgUpdateImage(mVg,
                                       mNvgSpectroFullImage,
                                       mState->mSpectroImageFullData.Get());
                }
            }
        }
    }

    if (mNeedUpdateColormapData || (mNvgColormapImage == 0))
    {
      // Colormap
      //WDL_TypedBuf<unsigned int> colorMapData;
      bool updated = mSpectrogram->GetColormapImageDataRGBA(&mColormapImageData);
    
      //if ((colorMapData.GetSize() != mColormapImageData.GetSize()) ||
      //    (mNvgColormapImage == 0))
      if (mNvgColormapImage == 0)
      {
          //mColormapImageData = colorMapData;
        
          if (mNvgColormapImage != 0)
              nvgDeleteImage(mVg, mNvgColormapImage);
        
          mNvgColormapImage =
                        nvgCreateImageRGBA(mVg,
                                           mColormapImageData.GetSize(), 1,
                                           NVG_IMAGE_NEAREST,
                                           (unsigned char *)mColormapImageData.Get());
      }
      else
      {
        //mColormapImageData = colorMapData;
        
          if (updated)
              nvgUpdateImage(mVg, mNvgColormapImage,
                             (unsigned char *)mColormapImageData.Get());
      }
    }
    
    mNeedUpdateSpectrogram = false;
    mNeedUpdateSpectrogramData = false;
    mNeedUpdateSpectrogramFullData = false;
    
    return true;
}

void
SpectrogramDisplay2::PreDraw(NVGcontext *vg, int width, int height)
{
    mVg = vg;

    bool updated = DoUpdateSpectrogram();
    
    if (!mShowSpectrogram)
    {
        /*if (mMiniView != NULL)
        {
            mMiniView->Display(mVg, width, height);
        }*/
    
        mNeedRedraw = false;
        
        return;
    }
    
    // Draw spectrogram first
    nvgSave(mVg);
    
    // New: set colormap only in the spectrogram state
    nvgSetColormap(mVg, mNvgColormapImage);
    
#define DEBUG_DISPLAY_FG 0 //1
#define DEBUG_DISPLAY_BG 1
    
#if DEBUG_DISPLAY_BG
    if (mDrawBGSpectrogram)
    {
        //
        // Spectrogram full image
        //
        // Display the full image first, so it will be in background
        NVGpaint fullImgPaint = nvgImagePattern(mVg,
                                                mSpectrogramBounds[0]*width,
                                                mSpectrogramBounds[1]*height,
                                                (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width,
                                                (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height,
                                                0.0, mNvgSpectroFullImage, mSpectrogramAlpha);
#if USE_ZOOM_NVG_TRANSFORM
        nvgResetTransform(mVg);
    
        BL_FLOAT absZoom = mState->mAbsMaxX - mState->mAbsMinX;
    
        nvgTranslate(mVg, mState->mAbsMinX*width, 0.0);
        nvgScale(mVg, absZoom, 1.0);
#endif
    
        BL_GUI_FLOAT b1f = mSpectrogramBounds[1]*height;
        BL_GUI_FLOAT b3f = (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height;
        
        nvgBeginPath(mVg);
    
        // Corner (x, y) => bottom-left
        nvgRect(mVg,
                mSpectrogramBounds[0]*width, b1f,
                (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width, b3f);
    
        nvgFillPaint(mVg, fullImgPaint);
        nvgFill(mVg);
    
#if USE_ZOOM_NVG_TRANSFORM
        nvgResetTransform(mVg);
#endif
    }
#endif
    
#if DEBUG_DISPLAY_FG
    //
    // Spectrogram image
    //
    
    // Display the rightmost par in case of zoom
    NVGpaint imgPaint = nvgImagePattern(mVg,
                                        mSpectrogramBounds[0]*width,
                                        mSpectrogramBounds[1]*height,
                                        (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width,
                                        (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height,
                                        0.0, mNvgSpectroImage, mSpectrogramAlpha);
    
    
#if USE_ZOOM_NVG_TRANSFORM
    nvgResetTransform(mVg);
    
    BL_FLOAT zoom = mState->mMaxX - mState->mMinX;
    
    nvgTranslate(mVg, mState->mMinX*width, 0.0);
    nvgScale(mVg, zoom, 1.0);
#endif
    
    BL_GUI_FLOAT b1f = mSpectrogramBounds[1]*height;
    BL_GUI_FLOAT b3f = (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height;
    
    //
    nvgBeginPath(mVg);
    nvgRect(mVg,
            mSpectrogramBounds[0]*width, b1f,
            (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width, b3f);
    
    
    nvgFillPaint(mVg, imgPaint);
    nvgFill(mVg);
    
#if USE_ZOOM_NVG_TRANSFORM
    nvgResetTransform(mVg);
#endif
    
#endif
    
    nvgRestore(mVg);

    /*if (mMiniView != NULL)
    {
        mMiniView->Display(mVg, width, height);
    }*/
    
    mNeedRedraw = updated;
}

bool
SpectrogramDisplay2::NeedRedraw()
{
    return mNeedRedraw;
}

bool
SpectrogramDisplay2::PointInsideSpectrogram(int x, int y, int width, int height)
{
    // Warning: y is reversed !
    BL_FLOAT nx = ((BL_FLOAT)x)/width;
    //BL_FLOAT ny = 1.0 - ((BL_FLOAT)y)/height;
    BL_FLOAT ny = ((BL_FLOAT)y)/height;
    
    if (nx < mSpectrogramBounds[0])
        return false;
    
    if (ny < mSpectrogramBounds[1])
        return false;
    
    if (nx > mSpectrogramBounds[2])
        return false;
    
    if (ny > mSpectrogramBounds[3])
        return false;
    
    return true;
}

#if 0
bool
SpectrogramDisplay2::PointInsideMiniView(int x, int y, int width, int height)
{
    if (mMiniView == NULL)
        return false;
    
    bool inside = mMiniView->IsPointInside(x, y, width, height);
    
    return inside;
}
#endif

void
SpectrogramDisplay2::GetSpectroNormCoordinate(int x, int y, int width, int height,
                                             BL_FLOAT *nx, BL_FLOAT *ny)
{
    BL_FLOAT nx0 = ((BL_FLOAT)x)/width;
    BL_FLOAT ny0 = ((BL_FLOAT)y)/height;
    
    *nx = (nx0 - mSpectrogramBounds[0])/
            (mSpectrogramBounds[2] - mSpectrogramBounds[0]);
    
    *ny = (ny0 - mSpectrogramBounds[1])/
    (mSpectrogramBounds[3] - mSpectrogramBounds[1]);
}

void
SpectrogramDisplay2::SetBounds(BL_FLOAT left, BL_FLOAT top,
                               BL_FLOAT right, BL_FLOAT bottom)
{
    mSpectrogramBounds[0] = left;
    mSpectrogramBounds[1] = top;
    mSpectrogramBounds[2] = right;
    mSpectrogramBounds[3] = bottom;
}

void
SpectrogramDisplay2::SetSpectrogram(BLSpectrogram4 *spectro)
{
    mSpectrogram = spectro;
    
    mShowSpectrogram = true;
    
    // Be sure to create the texture image in the right thread
    UpdateSpectrogram(true, true);
    
    mNeedRedraw = true;
}

/*
void
SpectrogramDisplay2::SetMiniView(MiniView *view)
{
  mMiniView = view;
}
*/

void
SpectrogramDisplay2::ShowSpectrogram(bool flag)
{
    mShowSpectrogram = flag;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::UpdateSpectrogram(bool updateData, bool updateFullData)
{
    mNeedUpdateSpectrogram = true;
    
    if (!mNeedUpdateSpectrogramData)
        mNeedUpdateSpectrogramData = updateData;
    
    if (!mNeedUpdateSpectrogramFullData)
    {
        if (updateFullData) // For debugging
            mNeedUpdateSpectrogramFullData = updateFullData;
    }
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::UpdateColormap(bool flag)
{
    mNeedUpdateSpectrogram = true;
    
    if (!mNeedUpdateColormapData)
    {
        mNeedUpdateColormapData = flag;
    }
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::ResetSpectrogramTransform()
{
    mState->mMinX = 0.0;
    mState->mMaxX = 1.0;
    
    mState->mAbsMinX = 0.0;
    mState->mAbsMaxX = 1.0;
    
    mState->mCenterPos = 0.5;
    mState->mAbsTranslation = 0.0;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::ResetSpectrogramTranslation()
{
    mState->mAbsTranslation = 0.0;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::SetSpectrogramZoom(BL_FLOAT zoomX)
{
    BL_FLOAT norm = (mState->mCenterPos - mState->mMinX)/
                        (mState->mMaxX - mState->mMinX);
    
    mState->mMinX = mState->mCenterPos - norm*zoomX;
    mState->mMaxX = mState->mCenterPos + (1.0 - norm)*zoomX;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::SetSpectrogramAbsZoom(BL_FLOAT zoomX)
{
    BL_FLOAT norm = (mState->mCenterPos - mState->mAbsMinX)/
                        (mState->mAbsMaxX - mState->mAbsMinX);
    
    mState->mAbsMinX = mState->mCenterPos - norm*zoomX;
    mState->mAbsMaxX = mState->mCenterPos + (1.0 - norm)*zoomX;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::SetSpectrogramCenterPos(BL_FLOAT centerPos)
{
    mState->mCenterPos = centerPos;
    
    mNeedRedraw = true;
}

bool
SpectrogramDisplay2::SetSpectrogramTranslation(BL_FLOAT tX)
{
    BL_FLOAT dX = tX - mState->mAbsTranslation;
    
    if (((mState->mAbsMinX > 1.0) && (dX > 0.0)) ||
        ((mState->mAbsMaxX < 0.0) && (dX < 0.0)))
        // Do not translate if the spectrogram gets out of view
        return false;
    
    mState->mAbsTranslation = tX;
    
    mState->mMinX += dX;
    mState->mMaxX += dX;
    
    mState->mAbsMinX += dX;
    mState->mAbsMaxX += dX;
    
    mNeedRedraw = true;
    
    return true;
}

void
SpectrogramDisplay2::GetSpectrogramVisibleNormBounds(BL_FLOAT *minX, BL_FLOAT *maxX)
{
    if (mState->mAbsMinX < 0.0)
        *minX = -mState->mAbsMinX/
                    (mState->mAbsMaxX - mState->mAbsMinX);
    else
        *minX = 0.0;
    
    if (mState->mAbsMaxX < 1.0)
        *maxX = 1.0;
    else
        *maxX = 1.0 - (mState->mAbsMaxX - 1.0)/
                        (mState->mAbsMaxX - mState->mAbsMinX);
}

void
SpectrogramDisplay2::GetSpectrogramVisibleNormBounds2(BL_FLOAT *minX, BL_FLOAT *maxX)
{
    *minX = -mState->mAbsMinX/
                (mState->mAbsMaxX - mState->mAbsMinX);
    *maxX = 1.0 - (mState->mAbsMaxX - 1.0)/
                (mState->mAbsMaxX - mState->mAbsMinX);
}

void
SpectrogramDisplay2::SetSpectrogramVisibleNormBounds2(BL_FLOAT minX, BL_FLOAT maxX)
{
    minX *= mState->mAbsMaxX - mState->mAbsMinX;
    BL_FLOAT spectroAbsMinX = -minX;
    
    maxX *= mState->mAbsMaxX - mState->mAbsMinX;
    
    BL_FLOAT spectroAbsMaxX = -maxX;
    
    BL_FLOAT trans = spectroAbsMinX - mState->mAbsMinX;
    
    mState->mAbsMinX = spectroAbsMinX;
    mState->mAbsMaxX = spectroAbsMaxX;
    
    //
    mState->mMinX += trans;
    mState->mMaxX += trans;
    
    mState->mCenterPos += trans;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::ResetSpectrogramZoomAndTrans()
{
    mState->mMinX = 0.0;
    mState->mMaxX = 1.0;
    
    mNeedRedraw = true;
}

#if 0
MiniView *
SpectrogramDisplay2::GetMiniView()
{
    return mMiniView;
}
#endif

void
SpectrogramDisplay2::SetDrawBGSpectrogram(bool flag)
{
    mDrawBGSpectrogram = flag;
    
    mNeedRedraw = true;
}

void
SpectrogramDisplay2::SetAlpha(BL_FLOAT alpha)
{
    mSpectrogramAlpha = alpha;
}

#endif // IGRAPHICS_NANOVG
