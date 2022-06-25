#ifndef CURVE_VIEW_H
#define CURVE_VIEW_H

#ifdef IGRAPHICS_NANOVG

#include <bl_queue.h>

#include <BLTypes.h>
#include <GraphControl12.h>

#define TITLE_SIZE 256

#define CURVE_Y_OFFSET 10

#define CURVE_VIEW_DEFAULT_NUM_DATA 60 //40 //10

class CurveView : public GraphCustomDrawer
{
 public:
    CurveView(const char *title,
              BL_FLOAT defaultVal = 0.0,
              int maxNumData = CURVE_VIEW_DEFAULT_NUM_DATA);
    virtual ~CurveView();

    void SetBounds(float x, float y, float width, float height);

    void PushData(BL_FLOAT data);

    // Check if speed mod is ok for adding
    // Only for the case ResetData()/AddData()
    bool SpeedModCanAdd();
    void ResetData();
    void AddData(BL_FLOAT data, BL_FLOAT t);

    void SetSpeedMod(int speedMod);
    
    void PostDraw(NVGcontext *vg, int width, int height) override;

protected:
    virtual void DrawCurve(NVGcontext *vg, int width, int height) = 0;

    //
    void DrawBackground(NVGcontext *vg, int width, int height);
    void DrawTitle(NVGcontext *vg, int width, int height);
    void DrawBorder(NVGcontext *vg, int width, int height);
        
    //
    char mTitle[TITLE_SIZE];
    
    float mX;
    float mY;
    float mWidth;
    float mHeight;

    // Style
    BL_FLOAT mBorderWidth;
    IColor mBorderColor;

    IColor mBackgroundColor;
    
    IColor mTitleColor;

    IColor mCurveColor;
    float mCurveLineWidth;
    
    BL_FLOAT mDefaultVal;
    int mMaxNumData;
    
    bl_queue<BL_FLOAT> mData;

    // Speed
    int mSpeedMod;
    long int mAddNum;
};

#endif // IGRAPHICS_NANOVG

#endif
