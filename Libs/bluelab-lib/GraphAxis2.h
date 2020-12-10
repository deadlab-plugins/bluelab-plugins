//
//  GraphCurve.h
//  EQHack
//
//  Created by Apple m'a Tuer on 10/09/17.
//
//

#ifndef GraphAxis2_h
#define GraphAxis2_h

#include <string>
using namespace std;

#include <BLTypes.h>
#include <GraphCurve5.h>
#include <Scale.h>

#include "IPlug_include_in_plug_hdr.h"

class GraphAxis2
{
public:
    GraphAxis2();
    
    virtual ~GraphAxis2();
    
    void InitHAxis(Scale::Type scale,
                   BL_GUI_FLOAT minX, BL_GUI_FLOAT maxX,
                   int axisColor[4], int axisLabelColor[4],
                   BL_GUI_FLOAT lineWidth,
                   BL_GUI_FLOAT offsetY = 0.0,
                   int axisOverlayColor[4] = NULL,
                   BL_GUI_FLOAT fontSizeCoeff = 1.0,
                   int axisLinesOverlayColor[4] = NULL);
    
    void InitVAxis(Scale::Type scale,
                   BL_GUI_FLOAT minY, BL_GUI_FLOAT maxY,
                   int axisColor[4], int axisLabelColor[4],
                   BL_GUI_FLOAT lineWidth,
                   BL_GUI_FLOAT offset = 0.0, BL_GUI_FLOAT offsetX = 0.0,
                   int axisOverlayColor[4] = NULL,
                   BL_GUI_FLOAT fontSizeCoeff = 1.0,
                   bool alignTextRight = false,
                   int axisLinesOverlayColor[4] = NULL,
                   bool alignRight = true);
    
    void SetMinMaxValues(BL_GUI_FLOAT minVal, BL_GUI_FLOAT maxVal);
    
    void SetData(char *data[][2], int numData);
    
    // Screen bounds [0, 1] by default
    void SetBounds(BL_FLOAT bounds[2]);

    
protected:
    void InitAxis(int axisColor[4],
                  int axisLabelColor[4],
                  int axisLabelOverlayColor[4],
                  int axisLinesOverlayColor[4],
                  BL_GUI_FLOAT lineWidth);
    
    typedef struct
    {
        BL_GUI_FLOAT mT;
        string mText;
    } GraphAxisData;

    friend class GraphControl12;
    
    //
    vector<GraphAxisData> mValues;
    
    Scale::Type mScale;
    BL_FLOAT mMinVal;
    BL_FLOAT mMaxVal;
    
    int mColor[4];
    int mLabelColor[4];
    
    // Hack
    BL_GUI_FLOAT mOffset;
    
    // To be able to display the axis on the right
    BL_GUI_FLOAT mOffsetX;
    BL_GUI_FLOAT mOffsetY;
    
    // Overlay axis labels ?
    bool mOverlay;
    int mLabelOverlayColor[4];
    
    // Overlay axis lines ?
    bool mLinesOverlay;
    int mLinesOverlayColor[4];
    
    BL_GUI_FLOAT mFontSizeCoeff;
    
    bool mAlignTextRight;
    bool mAlignRight;
    
    BL_GUI_FLOAT mLineWidth;
    
    BL_GUI_FLOAT mBounds[2];
};

#endif
