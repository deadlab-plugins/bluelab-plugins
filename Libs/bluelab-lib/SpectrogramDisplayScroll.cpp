//
//  SpectrogramDisplayScroll.cpp
//  BL-Chroma
//
//  Created by Pan on 14/06/18.
//
//

#ifdef IGRAPHICS_NANOVG

#include <BLSpectrogram4.h>
#include <UpTime.h>
#include <BLDebug.h>

#include "SpectrogramDisplayScroll.h"

// Spectrogram
#define GLSL_COLORMAP 1


#define USE_SPECTRO_NEAREST 0

// Number of columns we hide on the left
#define MARGIN_COEFF 8

// Avoids black column of 1 pixel on the right
// (increase of 2 pixels on the right)
#define RIGHT_OFFSET 0.0025

// FIX: If the plugin was hidden, and the host playing,
// mSpectroMagns and mSpectroPhases continued to grow, without being ever flushed
// (big memory leak)
#define FIX_HIDE_PLUG_MEM_LEAK 1

// USELESS
// Added for InfraSonicViewer
#define FIX_SPECTROGRAM_JITTER 0

SpectrogramDisplayScroll::SpectrogramDisplayScroll(Plugin *plug, NVGcontext *vg)
{
    mPlug = plug;
    
    mVg = vg;
    
    // Spectrogram
    mSpectrogram = NULL;
    mNvgSpectroImage = 0;
    mNeedUpdateSpectrogram = false;
    mNeedUpdateSpectrogramData = false;
    
    mNeedUpdateColormapData = false;
    mNvgColormapImage = 0;
    
    mShowSpectrogram = false;
    
    mSpectrogramGain = 1.0;
    
    mBufferSize = 2048;
    mSampleRate = 44100.0;
    mOverlapping = 0;
    
    mPrevSpectroLineNum = 0;
    
    mPrevTimeMillis = UpTime::GetUpTime();
    
    mLinesOffset = 0.0;
    
    mAddLineRemainder = 0.0;
    
    // Avoid jump when restarting playback
    mPrevIsPlaying = false;
    mPrevPixelOffset = 0.0;
    
    mIsPlaying = false;
    
#if USE_VARIABLE_SPEED
    mSpeedMod = 1;
#endif
}

SpectrogramDisplayScroll::~SpectrogramDisplayScroll()
{
    if (mNvgSpectroImage != 0)
        nvgDeleteImage(mVg, mNvgSpectroImage);
    
    if (mNvgColormapImage != 0)
        nvgDeleteImage(mVg, mNvgColormapImage);
}

void
SpectrogramDisplayScroll::SetNvgContext(NVGcontext *vg)
{
    mVg = vg;
}

void
SpectrogramDisplayScroll::ResetGfx()
{
    if (mNvgSpectroImage != 0)
        nvgDeleteImage(mVg, mNvgSpectroImage);
    mNvgSpectroImage = 0;
    
    if (mNvgColormapImage != 0)
        nvgDeleteImage(mVg, mNvgColormapImage);
    mNvgColormapImage = 0;
    
    mVg = NULL;
    
    // Force re-creating the nvg images
    mSpectroImageData.Resize(0);
    mColormapImageData.Resize(0);
    
    mNeedUpdateSpectrogramData = true;
    
    // FIX(2/2): fixes Logic, close plug window, re-open => the graph control view was blank
#if 1
    mNeedUpdateSpectrogram = true;
#endif
    
    // FIX: on Reaper, with Panogram, when clicking on "windows float selected FX",
    // the colormap looks red (until we choose another colormap)
    // (after ResetGL(), the colormap was not renewed)
    //
    // (No problem with Ghost-X)
#if 1
    mNeedUpdateColormapData = true;
#endif
}

void
SpectrogramDisplayScroll::RefreshGfx()
{
    if (mSpectrogram != NULL)
    {
        mSpectrogram->TouchColorMap();
    }
}

void
SpectrogramDisplayScroll::Reset()
{
    mSpectroImageData.Resize(0);
    
    mNeedUpdateSpectrogram = true;
    
    mNeedUpdateSpectrogramData = true;
    
    mNeedUpdateColormapData = true;
    
    mPrevSpectroLineNum = 0;
    
    mPrevTimeMillis = UpTime::GetUpTime();
    
    mLinesOffset = 0.0;
    
    mAddLineRemainder = 0.0;
}

void
SpectrogramDisplayScroll::ResetScroll()
{
    mLinesOffset = 0;
    
    mSpectroMagns.clear();
    mSpectroPhases.clear();
    
    // Set to 0: no jump (but lag)
    // Set to mOverlapping: avoid very big lag
    mAddLineRemainder = mOverlapping;
}

bool
SpectrogramDisplayScroll::NeedUpdateSpectrogram()
{
    return mNeedUpdateSpectrogram;
}

bool
SpectrogramDisplayScroll::DoUpdateSpectrogram()
{
    // Update first, before displaying
    if (!mNeedUpdateSpectrogram)
        //return false;
        // Must return true, because the spectrogram scrolls over time
        // (for smooth scrolling), even if the data is not changed
        return true;
    
#if !FIX_SPECTROGRAM_JITTER
    int w = mSpectrogram->GetNumCols();
#else
    int w = mSpectrogram->GetMaxNumCols();
#endif
    
    int h = mSpectrogram->GetHeight();
    
#if 1 // Avoid white image when there is no data
    if ((w == 0) || (mNvgSpectroImage == 0))
    {
        w = 1;
        int imageSize = w*h*4;
        
        mSpectroImageData.Resize(imageSize);
        memset(mSpectroImageData.Get(), 0, imageSize);
        
        // Spectrogram image
        if (mNvgSpectroImage != 0)
            nvgDeleteImage(mVg, mNvgSpectroImage);
        
        mNvgSpectroImage = nvgCreateImageRGBA(mVg,
                                              w, h,
#if USE_SPECTRO_NEAREST
                                              NVG_IMAGE_NEAREST |
#endif
#if GLSL_COLORMAP
                                              NVG_IMAGE_ONE_FLOAT_FORMAT,
#else
                                              0,
#endif
                                              mSpectroImageData.Get());
        
        mNeedUpdateSpectrogram = false;
        mNeedUpdateSpectrogramData = false;
        
        return true;
    }
#endif
    
    int imageSize = w*h*4;
    
    if (mNeedUpdateSpectrogramData || (mNvgSpectroImage == 0))
    {
        if ((mSpectroImageData.GetSize() != imageSize) || (mNvgSpectroImage == 0))
        {
            mSpectroImageData.Resize(imageSize);
            
            memset(mSpectroImageData.Get(), 0, imageSize);
            bool updated = mSpectrogram->GetImageDataFloat(w, h, mSpectroImageData.Get());
            
            if (updated)
            {
                // Spectrogram image
                if (mNvgSpectroImage != 0)
                    nvgDeleteImage(mVg, mNvgSpectroImage);
            
                mNvgSpectroImage = nvgCreateImageRGBA(mVg,
                                                      w, h,
#if USE_SPECTRO_NEAREST
                                                      NVG_IMAGE_NEAREST |
#endif
#if GLSL_COLORMAP
                                                      NVG_IMAGE_ONE_FLOAT_FORMAT,
#else
                                                      0,
#endif
                                                      mSpectroImageData.Get());
            }
			 // No need since it has been better fixed in nanovg
#ifdef WIN32 // Hack: after having created the image, update it again
			 // FIX: spectrogram blinking between random pixels and correct pixels  
			 //nvgUpdateImage(mVg, mNvgSpectroImage, mSpectroImageData.Get());
#endif
        }
        else
        {
                memset(mSpectroImageData.Get(), 0, imageSize);
                bool updated =
                        mSpectrogram->GetImageDataFloat(w, h, mSpectroImageData.Get());
                if (updated)
                {
                    // Spectrogram image
                    nvgUpdateImage(mVg, mNvgSpectroImage, mSpectroImageData.Get());
                }
        }
    }
    
    if (mNeedUpdateColormapData || (mNvgColormapImage == 0))
    {
        // Colormap
        WDL_TypedBuf<unsigned int> colorMapData;
        bool updated = mSpectrogram->GetColormapImageDataRGBA(&colorMapData);
        if (updated || (mNvgColormapImage == 0))
        {
            if ((colorMapData.GetSize() != mColormapImageData.GetSize()) ||
                (mNvgColormapImage == 0))
            {
                mColormapImageData = colorMapData;
        
                if (mNvgColormapImage != 0)
                    nvgDeleteImage(mVg, mNvgColormapImage);
        
                    mNvgColormapImage = nvgCreateImageRGBA(mVg,
                                                           mColormapImageData.GetSize(),
                                                           1, NVG_IMAGE_NEAREST /*0*/,
                                                           (unsigned char *)mColormapImageData.Get());
            }
            else
            {
                mColormapImageData = colorMapData;
        
                nvgUpdateImage(mVg, mNvgColormapImage,
                               (unsigned char *)mColormapImageData.Get());
            }
        }
    }
    
    mNeedUpdateSpectrogram = false;
    mNeedUpdateSpectrogramData = false;
    mNeedUpdateColormapData = false;
    
    return true;
}

void
SpectrogramDisplayScroll::DrawSpectrogram(int width, int height)
{
    if (!mShowSpectrogram)
        return;
    
    // Draw spectrogram first
    nvgSave(mVg);
    
    // New: set colormap only in the spectrogram state
#if GLSL_COLORMAP
    nvgSetColormap(mVg, mNvgColormapImage);
#endif

    BL_FLOAT scrollOffsetPixels = ComputeScrollOffsetPixels(width);
    
    //
    // Spectrogram image
    //
    
    // Display the rightmost par in case of zoom
    BL_FLOAT alpha = 1.0;
    NVGpaint imgPaint = nvgImagePattern(mVg,
                                        mSpectrogramBounds[0]*width + scrollOffsetPixels,
                                        mSpectrogramBounds[1]*height,
                                        (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width,
                                        (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height,
                                        0.0, mNvgSpectroImage, alpha);
    
    BL_GUI_FLOAT b1f = mSpectrogramBounds[1]*height;
    BL_GUI_FLOAT b3f = (mSpectrogramBounds[3] - mSpectrogramBounds[1])*height;
#if GRAPH_CONTROL_FLIP_Y
    b1f = height - b1f;
    b3f = height - b3f;
#endif
    
    nvgBeginPath(mVg);
    nvgRect(mVg,
            mSpectrogramBounds[0]*width + scrollOffsetPixels,
            b1f,
            (mSpectrogramBounds[2] - mSpectrogramBounds[0])*width, b3f);
    
    
    nvgFillPaint(mVg, imgPaint);
    nvgFill(mVg);
    
    nvgRestore(mVg);
}

void
SpectrogramDisplayScroll::SetSpectrogram(BLSpectrogram4 *spectro,
                                         BL_FLOAT left, BL_FLOAT top,
                                         BL_FLOAT right, BL_FLOAT bottom)
{
    mSpectrogram = spectro;
    
    // Must "shift" the left of the spectrogram,
    // So we won't see the black column on the left
#if !FIX_SPECTROGRAM_JITTER
    int numCols = mSpectrogram->GetNumCols();
#else
    int numCols = mSpectrogram->GetMaxNumCols();
#endif
    
    BL_FLOAT normLineSize = 0.0;
    
    if (numCols > 0)
        normLineSize = 1.0/((BL_FLOAT)numCols);
    
    mSpectrogramBounds[0] = left - MARGIN_COEFF*normLineSize;
    mSpectrogramBounds[1] = top;
    mSpectrogramBounds[2] = right + RIGHT_OFFSET; // Avoids black column of 1 pixel on the right
    mSpectrogramBounds[3] = bottom;
    
    mShowSpectrogram = true;
    
    // Be sure to create the texture image in the right thread
    UpdateSpectrogram(true);
    
    // Avoid scrolling over time at launch
    // (until we get initial position)
    mPrevSpectroLineNum = mSpectrogram->GetTotalLineNum();
    // Must set a value at the beginning
    // (otherwise scrolling will be very slow with overlapping 4)
    mAddLineRemainder = mOverlapping;
}

void
SpectrogramDisplayScroll::SetFftParams(int bufferSize,
                                       int overlapping,
                                       BL_FLOAT sampleRate)
{
    bool overlappingChanged = (overlapping != mOverlapping);
    
    mBufferSize = bufferSize; 
    mOverlapping = overlapping;
    mSampleRate = sampleRate;
    
    if (overlappingChanged)
    {
        // Must set a value at the beginning
        // (otherwise scrolling will be very slow with overlapping 4)
        mAddLineRemainder = mOverlapping;
    }
}

void
SpectrogramDisplayScroll::AddSpectrogramLine(const WDL_TypedBuf<BL_FLOAT> &magns,
                                             const WDL_TypedBuf<BL_FLOAT> &phases)
{
    mSpectroMagns.push_back(magns);
    mSpectroPhases.push_back(phases);
    
#if FIX_HIDE_PLUG_MEM_LEAK
    int maxCols = mSpectrogram->GetMaxNumCols();
    int bufferLimit = maxCols*2;
    
    while (mSpectroMagns.size() > bufferLimit)
    {
        mSpectroMagns.pop_front();
    }
    
    while (mSpectroPhases.size() > bufferLimit)
    {
        mSpectroPhases.pop_front();
    }
#endif
}

void
SpectrogramDisplayScroll::ShowSpectrogram(bool flag)
{
    mShowSpectrogram = flag;
}

void
SpectrogramDisplayScroll::UpdateSpectrogram(bool flag)
{
    mNeedUpdateSpectrogram = true;
    
    if (!mNeedUpdateSpectrogramData)
        mNeedUpdateSpectrogramData = flag;
}

void
SpectrogramDisplayScroll::UpdateColormap(bool flag)
{
    mNeedUpdateSpectrogram = true;
    
    if (!mNeedUpdateColormapData)
    {
        mNeedUpdateColormapData = flag;
    }
}

void
SpectrogramDisplayScroll::SetIsPlaying(bool flag)
{
    mIsPlaying = flag;
}

#if USE_VARIABLE_SPEED
void
SpectrogramDisplayScroll::SetSpeedMod(int speedMod)
{
    mSpeedMod = speedMod;
}

int
SpectrogramDisplayScroll::GetSpeedMod()
{
    return mSpeedMod;
}
#endif

BL_FLOAT
SpectrogramDisplayScroll::GetScaleRatio()
{
    int maxCols = mSpectrogram->GetMaxNumCols();
    BL_FLOAT ratio = ((BL_FLOAT)(maxCols - MARGIN_COEFF))/maxCols;
    
    return ratio;
}

void
SpectrogramDisplayScroll::AddSpectrogramLines(BL_FLOAT numLines)
{
    // Keep the remainder, to add back later
    int numLines0 = numLines + mAddLineRemainder;
    
    int numLinesAdded = 0;
    while (mSpectroMagns.size() > 0)
    {
        const WDL_TypedBuf<BL_FLOAT> &magns = mSpectroMagns[0];
        const WDL_TypedBuf<BL_FLOAT> &phases = mSpectroPhases[0];
        
        mSpectrogram->AddLine(magns, phases);
        
        mSpectroMagns.pop_front();
        mSpectroPhases.pop_front();
        
        numLinesAdded++;
        
        if (numLinesAdded >= numLines0)
            break;
    }
    
    mAddLineRemainder += numLines - (int)numLines;
}

BL_FLOAT
SpectrogramDisplayScroll::ComputeScrollOffsetPixels(int width)
{
    // Elapsed time since last time
    unsigned long long currentTimeMillis = UpTime::GetUpTime();
    long long elapsedMillis = currentTimeMillis - mPrevTimeMillis;
    mPrevTimeMillis = currentTimeMillis;
    
    // Special case: playback stopped
    //
    // If not playing, stop do not make the scrolling process
    
    // PROBLEM with Logic (Good with Reaper)
    // IsPlaying() should be called from the audio thread, which is not the case here
    //bool isPlaying = mPlug->IsPlaying();
    
    bool isPlaying = mIsPlaying;
    if (!isPlaying)
    {
        mPrevIsPlaying = false;
        
        return mPrevPixelOffset;
    }
    
    // Special case: playback just restarted
    //
    // Reset some things to avoid jumps in scrolling when restarting playback
    //
    if (!mPrevIsPlaying && isPlaying)
    {
        mLinesOffset = 0;
        
        // GOOD !
        // Flush the previous data if we stopped the playback and just restarted it
        // Avoids a big jump when restarting
        mSpectroMagns.clear();
        mSpectroPhases.clear();
        
        // Set to 0: no jump (but lag)
        // Set to mOverlapping: avoid very big lag
        mAddLineRemainder = mOverlapping;
    }
    // Update
    mPrevIsPlaying = isPlaying;
    
    // Do compute the offset
    //
    
    // How many lines by second ?
    BL_FLOAT lineSpeed = mOverlapping*mSampleRate/mBufferSize;
    
#if USE_VARIABLE_SPEED
    lineSpeed /= mSpeedMod;
#endif
    
    // Compute the offset in units "line"
    BL_FLOAT offsetLine = lineSpeed*(((BL_FLOAT)elapsedMillis)/1000.0);
    mLinesOffset -= offsetLine;
    
    // How many lines added
    unsigned long long currentLineNum = mSpectrogram->GetTotalLineNum();
    long long diffLineNum = currentLineNum - mPrevSpectroLineNum;
    mPrevSpectroLineNum = currentLineNum;
    
    mLinesOffset += diffLineNum;
    if (mLinesOffset < 0.0)
        mLinesOffset = 0.0;
    
    // Compute the offset in units "pixels"
#if !FIX_SPECTROGRAM_JITTER
    int numCols = mSpectrogram->GetNumCols();
#else
    int numCols = mSpectrogram->GetMaxNumCols();
#endif
    
    BL_FLOAT lineNumPixels = 0.0;
    if (numCols > 0)
        lineNumPixels = ((BL_FLOAT)width)/numCols;
    
    BL_FLOAT offsetPixels = mLinesOffset*lineNumPixels;
    
    // Add new data if we are shifted enpough
    AddSpectrogramLines(mLinesOffset + 1);
    
    // Special case: scrolled too much
    //
    // Avoid scrolling too much on the right
    // (for example when keeping the spacebar pressed)
    // 1 col (as the spectrogram is scalled on the left to hide the black colums)
    //
    // Avoids time scrolling at startup with samplerate 88200Hz too.
    if (offsetPixels > MARGIN_COEFF*lineNumPixels)
    {
        // Reset
        offsetPixels = 0;
        mLinesOffset = 0;
        mAddLineRemainder = mOverlapping;
    }
    
    mPrevPixelOffset = offsetPixels;
    
    return offsetPixels;
}

#endif // IGRAPHICS_NANOVG
