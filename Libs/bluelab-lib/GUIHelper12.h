//
//  GUIHelper12.hpp
//  UST-macOS
//
//  Created by applematuer on 9/25/20.
//
//

#ifndef GUIHelper12_h
#define GUIHelper12_h

#include <IControls.h>

#include <BLVumeterControl.h>
#include <BLVumeterNeedleControl.h>
#include <BLVumeter2SidesControl.h>
#include <ResizeGUIPluginInterface.h>

#include "IPlug_include_in_plug_hdr.h"


using namespace iplug;
using namespace iplug::igraphics;

class GraphControl12;
class IRadioButtonsControl;
class IGUIResizeButtonControl;

// TODO
#define VumeterControl IBKnobControl

// GUIHelper12: from GUIHelper11, for GraphControl12
//
class GUIHelper12
{
public:
    enum Style
    {
        STYLE_BLUELAB,
        STYLE_UST,
        STYLE_BLUELAB_V3
    };
    
    enum Position
    {
        TOP,
        BOTTOM,
        LOWER_LEFT,
        LOWER_RIGHT
    };
    
    enum Size
    {
        SIZE_DEFAULT,
        SIZE_SMALL,
        SIZE_BIG
    };
    
    GUIHelper12(Style style);
    
    virtual ~GUIHelper12();
    
    IBKnobControl *CreateKnob(IGraphics *graphics,
                              float x, float y,
                              const char *bitmapFname, int nStates,
                              int paramIdx,
                              const char *tfBitmapFname,
                              const char *title = NULL,
                              Size titleSize = SIZE_DEFAULT,
                              ICaptionControl **caption = NULL,
                              bool createValue = true);
    
#ifdef IGRAPHICS_NANOVG
    GraphControl12 *CreateGraph(Plugin *plug, IGraphics *graphics,
                                float x, float y,
                                const char *bitmapFname, int paramIdx,
                                const char *overlayFname = NULL);
#endif // IGRAPHICS_NANOVG
    
    // 2 states or more
    IBSwitchControl *CreateSwitchButton(IGraphics *graphics,
                                        float x, float y,
                                        const char *bitmapFname, int nStates,
                                        int paramIdx,
                                        const char *title = NULL,
                                        Size titleSize = SIZE_DEFAULT);
    
    // 2 states
    IBSwitchControl *CreateToggleButton(IGraphics *graphics,
                                        float x, float y,
                                        const char *bitmapFname,
                                        int paramIdx,
                                        const char *title = NULL,
                                        Size titleSize = SIZE_DEFAULT);
    
    VumeterControl *CreateVumeter(IGraphics *graphics,
                                  float x, float y,
                                  const char *bitmapFname, int nStates,
                                  int paramIdx, const char *title = NULL);
    
    BLVumeterControl *CreateVumeterV(IGraphics *graphics,
                                     float x, float y,
                                     const char *bitmapFname,
                                     int paramIdx, const char *title = NULL);
    
    // Margins are in pixels
    BLVumeter2SidesControl *CreateVumeter2SidesV(IGraphics *graphics,
                                                 float x, float y,
                                                 const char *bitmapFname,
                                                 int paramIdx,
                                                 const char *title = NULL,
                                                 float marginMin = 0.0,
                                                 float marginMax = 0.0);
    
    BLVumeterNeedleControl *CreateVumeterNeedleV(IGraphics *graphics,
                                                 float x, float y,
                                                 const char *bitmapFname,
                                                 int paramIdx,
                                                 const char *title = NULL);
    
    ITextControl *CreateText(IGraphics *graphics,
                             float x, float y,
                             const char *textStr, float size,
                             const char *font,
                             const IColor &color, EAlign align,
                             float offsetX = 0.0, float offsetY = 0.0);

    
    IBitmapControl *CreateBitmap(IGraphics *graphics,
                                 float x, float y,
                                 const char *bitmapFname,
                                 //EAlign align = EAlign::Near,
                                 float offsetX = 0.0, float offsetY = 0.0,
                                 float *width = NULL, float *height = NULL);

    
    void CreateVersion(Plugin *plug, IGraphics *graphics,
                       const char *versionStr);
    
    void CreateLogo(Plugin *plug, IGraphics *graphics,
                    const char *logoFname, Position pos);
    
    void CreateLogoAnim(Plugin *plug, IGraphics *graphics,
                        const char *logoFname,
                        int nStates, Position pos);
    
    void CreatePlugName(Plugin *plug, IGraphics *graphics,
                        const char *plugNameFname, Position pos);

    
    void CreateHelpButton(Plugin *plug, IGraphics *graphics,
                          const char *bmpFname,
                          const char *manualFileName,
                          Position pos);
    void ShowHelp(Plugin *plug, IGraphics *graphics,
                  const char *manualFileName);
    
    void CreateDemoMessage(IGraphics *graphics);
    
    void CreateWatermarkMessage(IGraphics *graphics, const char *message,
                                IColor *color = NULL);
    
    IRadioButtonsControl *
        CreateRadioButtons(IGraphics *graphics,
                           float x, float y,
                           const char *bitmapFname,
                           int numButtons, float size, int paramIdx,
                           bool horizontalFlag, const char *title,
                           EAlign align = EAlign::Center,
                           EAlign titleAlign = EAlign::Center,
                           const char **radioLabels = NULL);
    
    IControl *CreateGUIResizeButton(ResizeGUIPluginInterface *plug,
                                    IGraphics *graphics,
                                    float x, float y,
                                    const char *bitmapFname,
                                    int paramIdx,
                                    char *label,
                                    int guiSizeIdx);
    
    IControl *CreateRolloverButton(IGraphics *graphics,
                                   float x, float y,
                                   const char *bitmapFname,
                                   int paramIdx,
                                   char *label,
                                   bool toggleFlag = false);

    
    void GetValueTextColor(IColor *valueTextColor) const;
    
    // Graph
    void GetGraphAxisColor(int color[4]);
    void GetGraphAxisOverlayColor(int color[4]);
    void GetGraphAxisLabelColor(int color[4]);
    void GetGraphAxisLabelOverlayColor(int color[4]);
    float GetGraphAxisLineWidth();
    float GetGraphAxisLineWidthBold();
    
    void GetGraphCurveDescriptionColor(int color[4]);
    void GetGraphCurveColorBlue(int color[4]);
    void GetGraphCurveColorGreen(int color[4]);
    void GetGraphCurveColorLightBlue(int color[4]);
    float GetGraphCurveFillAlpha();
    float GetGraphCurveFillAlphaLight();
    
    void GetGraphCurveColorGray(int color[4]);
    void GetGraphCurveColorRed(int color[4]);

    // For overlay curves
    void GetGraphCurveColorBlack(int color[4]);

    
    static void ResetParameter(Plugin *plug, int paramIdx);
    
    // Refresh all the controls, from their values
    static void RefreshAllParameters(Plugin *plug, int numParams);

    static bool PromptForFile(Plugin *plug, EFileAction action, WDL_String *result,
                              char* dir = "", char* extensions = "");
    
    // NOTE: set it public, can be useful
    ITextControl *CreateTitle(IGraphics *graphics, float x, float y,
                              const char *title, Size size,
                              EAlign align = EAlign::Center);

    // public for Precedence
    ITextControl *CreateValueText(IGraphics *graphics,
                                  float x, float y,
                                  const char *textValue);

    // Circle graph drawer
    void GetCircleGDCircleLineWidth(float *circleLineWidth);
    void GetCircleGDLinesWidth(float *linesWidth);
    void GetCircleGDLinesColor(IColor *linesColor);
    void GetCircleGDTextColor(IColor *textColor);
    void GetCircleGDOffsetX(int *x);
    void GetCircleGDOffsetY(int *y);
    
protected:
    void GetManualFullPath(Plugin *plug, IGraphics *graphics,
                           const char *manualFileName,
                           char fullFileName[1024]);
        
    ITextControl *CreateText(IGraphics *graphics, float x, float y,
                             const char *textStr, const IText &text,
                             float offsetX, float offsetY,
                             EAlign align = EAlign::Center);
    
    ICaptionControl *CreateValue(IGraphics *graphics,
                                 float x, float y,
                                 const char *bitmapFname,
                                 int paramIdx);

    float GetTextWidth(IGraphics *graphics, const IText &text, const char *textStr);
    
    ITextControl *CreateRadioLabelText(IGraphics *graphics,
                                       float x, float y,
                                       const char *textStr,
                                       const IBitmap &bitmap,
                                       const IText &text,
                                       EAlign align = EAlign::Near);
    
    
    //
    Style mStyle;
    
    bool mCreateTitles;

    bool mCreatePlugName;
    bool mCreateLogo;
    bool mCreateHelpButton;
    
    float mTitleTextSize;
    float mTitleTextOffsetX;
    float mTitleTextOffsetY;
    IColor mTitleTextColor;
    IColor mHilightTextColor;
    
    Size mDefaultTitleSize;
    char *mTitleFont;
    
    float mTitleTextSizeBig;
    float mTitleTextOffsetXBig;
    float mTitleTextOffsetYBig;
    
    float mValueCaptionOffset;
    float mValueTextSize;
    float mValueTextOffsetX;
    float mValueTextOffsetY;
    IColor mValueTextColor;
    IColor mValueTextFGColor;
    IColor mValueTextBGColor;
    char *mValueTextFont;

    Position mVersionPosition;
    
    float mVersionTextSize;
    float mVersionTextOffsetX;
    float mVersionTextOffsetY;
    IColor mVersionTextColor;
    char *mVersionTextFont;
    
    IColor mVumeterColor;
    IColor mVumeterNeedleColor;
    float mVumeterNeedleDepth;
    
    float mLogoOffsetX;
    float mLogoOffsetY;
    float mAnimLogoSpeed;
    
    float mPlugNameOffsetX;
    float mPlugNameOffsetY;
    
    float mTrialOffsetX;
    float mTrialOffsetY;
    
    float mHelpButtonOffsetX;
    float mHelpButtonOffsetY;
    
    float mDemoTextSize;
    float mDemoTextOffsetX;
    float mDemoTextOffsetY;
    IColor mDemoTextColor;
    char *mDemoFont;
    
    float mWatermarkTextSize;
    float mWatermarkTextOffsetX;
    float mWatermarkTextOffsetY;
    IColor mWatermarkTextColor;
    char *mWatermarkFont;
    
    float mRadioLabelTextSize;
    float mRadioLabelTextOffsetX;
    IColor mRadioLabelTextColor;
    char *mLabelTextFont;
    
    float mButtonLabelTextOffsetX;
    float mButtonLabelTextOffsetY;
    
    //
    IColor mGraphAxisColor;
    IColor mGraphAxisOverlayColor;
    IColor mGraphAxisLabelColor;
    IColor mGraphAxisLabelOverlayColor;
    float mGraphAxisLineWidth;
    float mGraphAxisLineWidthBold;
    
    IColor mGraphCurveDescriptionColor;
    IColor mGraphCurveColorBlue;
    IColor mGraphCurveColorGreen;
    IColor mGraphCurveColorLightBlue;
    float mGraphCurveFillAlpha;
    float mGraphCurveFillAlphaLight;

    IColor mGraphCurveColorGray;
    IColor mGraphCurveColorRed;

    IColor mGraphCurveColorBlack;
    
    // Circle graph drawer
    float mCircleGDCircleLineWidth;
    float mCircleGDLinesWidth;
    IColor mCircleGDLinesColor;
    IColor mCircleGDTextColor;
    int mCircleGDOffsetX;
    int mCircleGDOffsetY;
};

#endif /* GUIHelper12_hpp */
