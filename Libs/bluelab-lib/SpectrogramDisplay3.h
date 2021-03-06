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
 
//
//  SpectrogramDisplay3.h
//  BL-Ghost
//
//  Created by Pan on 14/06/18.
//
//

#ifndef __BL_Ghost__SpectrogramDisplay3__
#define __BL_Ghost__SpectrogramDisplay3__

#ifdef IGRAPHICS_NANOVG

#include "IPlug_include_in_plug_hdr.h"

#include <BLTypes.h>
#include <GraphControl12.h>

class BLSpectrogram4;
class NVGcontext;
class SpectrogramDisplay3 : public GraphCustomDrawer
{
public:
    struct SpectrogramDisplayState
    {
        // For local spectrogram
        BL_FLOAT mMinX;
        BL_FLOAT mMaxX;
        
        // For global spectrogram
        BL_FLOAT mAbsMinX;
        BL_FLOAT mAbsMaxX;
        
        BL_FLOAT mAbsTranslation;
        
        BL_FLOAT mCenterPos;

        // For foreground spectrogram
        BL_FLOAT mZoomAdjustFactor;
        BL_FLOAT mZoomAdjustOffset;

        // For background spectrogram
        BL_FLOAT mZoomAdjustFactorBG;
        BL_FLOAT mZoomAdjustOffsetBG;
        
        // Background spectrogram
        int mSpectroImageWidth;
        int mSpectroImageHeight;

        int mBGSpectroImageWidth;
        int mBGSpectroImageHeight;
        
        WDL_TypedBuf<unsigned char> mBGSpectroImageData;

        // NEW since Morpho: colormap data is now in SpectrogramDisplayState
        WDL_TypedBuf<unsigned int> mColorMapImageData;
        
        int mSpeedMod;
    };
    
    SpectrogramDisplay3(SpectrogramDisplayState *spectroTransform);
    virtual ~SpectrogramDisplay3();
    
    SpectrogramDisplayState *GetState();
    
    void Reset();
    
    bool NeedUpdateSpectrogram();
    bool DoUpdateSpectrogram();
    
    void PreDraw(NVGcontext *vg, int width, int height) override;
    bool IsOwnedByGraph() override { return true; }
    bool NeedRedraw() override;
    
    //
    bool PointInsideSpectrogram(int x, int y,
                                int width, int height);
    
    bool PointInsideMiniView(int x, int y,
                             int width, int height);
    
    void GetNormCoordinate(int x, int y, int width, int height,
                           BL_FLOAT *nx, BL_FLOAT *ny);
    
    // Spectrogram
    void SetBounds(BL_FLOAT left, BL_FLOAT top,
                   BL_FLOAT right, BL_FLOAT bottom);
    void SetSpectrogram(BLSpectrogram4 *spectro);
    void SetSpectrogramBG(BLSpectrogram4 *spectro);
    
    void ShowSpectrogram(bool flag);
    void UpdateSpectrogram(bool updateData = true, bool updateBgData = false);

    void UpdateColorMap(bool flag);
    
    // Reset all
    void ResetTransform();
    
    // Rest internal translation only
    void ResetTranslation();
    
    //
    void SetZoom(BL_FLOAT zoomX);
    void SetAbsZoom(BL_FLOAT zoomX);

    // Zoom adjust, for the spectrogram exactly adjusted to th waveform
    void SetZoomAdjust(BL_FLOAT zoomAdjustZoom, BL_FLOAT zoomAdjustOffset);
    void SetZoomAdjustBG(BL_FLOAT zoomAdjustZoom, BL_FLOAT zoomAdjustOffset);
    
    void SetCenterPos(BL_FLOAT centerPos);
    
    // Return true if we are in bounds
    bool SetTranslation(BL_FLOAT tX);
    
    // Allows result ouside of [0, 1]
    void GetVisibleNormBounds(BL_FLOAT *minX, BL_FLOAT *maxX);
    
    void SetVisibleNormBounds(BL_FLOAT minX, BL_FLOAT maxX);
    
    // Called after local data recomputation
    void ResetZoomAndTrans();
    
    // For optimization
    // To be able to disable background spectrogram
    // will avoid to draw the spectrogram twice with Chroma and GhostViewer
    void SetDrawBGSpectrogram(bool flag);
    
    void SetAlpha(BL_FLOAT alpha);
    
    void ClearBGSpectrogram();

    // Only set to adapt scrolling to sample rate
    void SetSpeedMod(int speedMod);
    int GetSpeedMod();
    
protected:
    void ApplyZoomAdjustFactor(BL_FLOAT *zoom, BL_FLOAT *tx,
                               BL_FLOAT minX, BL_FLOAT maxX,
                               BL_FLOAT zoomAdjustFactor,
                               BL_FLOAT zoomAdjustOffset);
    
    void CleanBGSpectrogram();
    
    // NanoVG
    NVGcontext *mVg;
    
    // Spectrogram
    BLSpectrogram4 *mSpectrogram;
    BLSpectrogram4 *mSpectrogramBG;
    BL_FLOAT mSpectrogramBounds[4];
    
    int mNvgSpectroImage;
    WDL_TypedBuf<unsigned char> mSpectroImageData;
    
    bool mNeedUpdateSpectrogram;
    bool mNeedUpdateSpectrogramData;
    
    int mNvgBGSpectroImage;
    bool mNeedUpdateBGSpectrogramData;
    
    SpectrogramDisplayState *mState;
    
    BL_FLOAT mSpectrogramAlpha;

    bool mNeedUpdateColorMapData;
    
    //
    bool mShowSpectrogram;
    
    // Colormap
    int mNvgColorMapImage;
    
    // For optimization (Chroma)
    bool mDrawBGSpectrogram;
    
    bool mNeedRedraw;
};

#endif

#endif /* defined(__BL_Ghost__SpectrogramDisplay3__) */
