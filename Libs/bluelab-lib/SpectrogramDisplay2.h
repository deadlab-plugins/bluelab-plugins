//
//  SpectrogramDisplay2.h
//  BL-Ghost
//
//  Created by Pan on 14/06/18.
//
//

#ifndef __BL_Ghost__SpectrogramDisplay2__
#define __BL_Ghost__SpectrogramDisplay2__

#ifdef IGRAPHICS_NANOVG

#include "IPlug_include_in_plug_hdr.h"

#include <BLTypes.h>
#include <GraphControl12.h>

class BLSpectrogram4;
//class MiniView;
class NVGcontext;
class SpectrogramDisplay2 : public GraphCustomDrawer
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
        
        WDL_TypedBuf<unsigned char> mSpectroImageFullData;
    };
    
    SpectrogramDisplay2(SpectrogramDisplayState *spectroTransform);
    virtual ~SpectrogramDisplay2();
    
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
    
    void GetSpectroNormCoordinate(int x, int y, int width, int height,
                                  BL_FLOAT *nx, BL_FLOAT *ny);
    
    // Spectrogram
    void SetBounds(BL_FLOAT left, BL_FLOAT top,
                   BL_FLOAT right, BL_FLOAT bottom);
    void SetSpectrogram(BLSpectrogram4 *spectro);
    
    void ShowSpectrogram(bool flag);
    void UpdateSpectrogram(bool updateData = true, bool updateFullData = false);

    void UpdateColormap(bool flag);
    
    //void SetMiniView(MiniView *view);
    
    // Reset all
    void ResetSpectrogramTransform();
    
    // Rest internal translation only
    void ResetSpectrogramTranslation();
    
    //
    void SetSpectrogramZoom(BL_FLOAT zoomX);
    
    void SetSpectrogramAbsZoom(BL_FLOAT zoomX);
    
    void SetSpectrogramCenterPos(BL_FLOAT centerPos);
    
    // Return true if we are in bounds
    bool SetSpectrogramTranslation(BL_FLOAT tX);
    
    // Clamps
    void GetSpectrogramVisibleNormBounds(BL_FLOAT *minX, BL_FLOAT *maxX);
    
    // Allows result ouside of [0, 1]
    void GetSpectrogramVisibleNormBounds2(BL_FLOAT *minX, BL_FLOAT *maxX);
    
    void SetSpectrogramVisibleNormBounds2(BL_FLOAT minX, BL_FLOAT maxX);
    
    // Called after local data recomputation
    void ResetSpectrogramZoomAndTrans();

    //MiniView *GetMiniView();
    
    // For optimization
    // To be able to disable background spectrogram
    // will avoid to draw the spectrogram twice with Chroma and GhostViewer
    void SetDrawBGSpectrogram(bool flag);
    
    void SetAlpha(BL_FLOAT alpha);
    
protected:
    // NanoVG
    NVGcontext *mVg;
    
    // Spectrogram
    BLSpectrogram4 *mSpectrogram;
    BL_FLOAT mSpectrogramBounds[4];
    
    int mNvgSpectroImage;
    WDL_TypedBuf<unsigned char> mSpectroImageData;
    
    bool mNeedUpdateSpectrogram;
    bool mNeedUpdateSpectrogramData;
    
    int mNvgSpectroFullImage;
    bool mNeedUpdateSpectrogramFullData;
    
    SpectrogramDisplayState *mState;
    
    BL_FLOAT mSpectrogramAlpha;

    bool mNeedUpdateColormapData;
    
    //
    bool mShowSpectrogram;
    
    //BL_FLOAT mSpectrogramGain;
    
    // Colormap
    int mNvgColormapImage;
    WDL_TypedBuf<unsigned int> mColormapImageData;
    
    // Mini view
    //MiniView *mMiniView;
    
    // For optimization (Chroma)
    bool mDrawBGSpectrogram;
    
    bool mNeedRedraw;
};

#endif

#endif /* defined(__BL_Ghost__SpectrogramDisplay2__) */
