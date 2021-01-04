//
//  GraphCurve.cpp
//  EQHack
//
//  Created by Apple m'a Tuer on 10/09/17.
//
//

#include <BLUtils.h>
#include <GraphSwapColor.h>
#include <Scale.h>

#include <GraphControl12.h>

#include "GraphAxis2.h"

GraphAxis2::GraphAxis2()
{
    mGraph = NULL;
    
    for (int i = 0; i < 4; i++)
        mColor[i] = 0;
    
    for (int i = 0; i < 4; i++)
        mLabelColor[i] = 0;
    
    //mOffset = 0.0;
    mOffsetX = 0.0;
    mOffsetY = 0.0;
    
    mOverlay = false;
    for (int i = 0; i < 4; i++)
        mLabelOverlayColor[i] = 0;
    
    mLinesOverlay = false;;
    for (int i = 0; i < 4; i++)
        mLinesOverlayColor[i] = 0;
    
    mFontSizeCoeff = 1.0;
    
    mScale = Scale::LINEAR;
    mMinVal = 0.0;
    mMaxVal = 1.0;
    
    mAlignTextRight = false;
    
    mAlignRight = false;
    
    mLineWidth = 1.0;
    
    // Bounds
    mBounds[0] = 0.0;
    mBounds[1] = 1.0;
}

GraphAxis2::~GraphAxis2() {}

void
GraphAxis2::InitHAxis(Scale::Type scale,
                      BL_GUI_FLOAT minX, BL_GUI_FLOAT maxX,
                      int axisColor[4], int axisLabelColor[4],
                      BL_GUI_FLOAT lineWidth,
                      BL_GUI_FLOAT offsetY,
                      int axisOverlayColor[4],
                      BL_GUI_FLOAT fontSizeCoeff,
                      int axisLinesOverlayColor[4])
{
    //mOffset = 0.0;
    
    // ????
    // Warning, offset Y is normalized value
    mOffsetX = 0.0; //
    mOffsetY = offsetY;
    mOverlay = false;
    mLinesOverlay = false;
    
    mFontSizeCoeff = fontSizeCoeff;
    
    mScale = scale;
    mMinVal = minX;
    mMaxVal = maxX;
    
    mAlignTextRight = false;
    mAlignRight = true;
    
    InitAxis(axisColor, axisLabelColor,
             axisOverlayColor, axisLinesOverlayColor, lineWidth);
    
    NotifyGraph();
}

void
GraphAxis2::InitVAxis(Scale::Type scale,
                      BL_GUI_FLOAT minY, BL_GUI_FLOAT maxY,
                      int axisColor[4], int axisLabelColor[4],
                      BL_GUI_FLOAT lineWidth,
                      BL_GUI_FLOAT offsetX, BL_GUI_FLOAT offsetY,
                      int axisOverlayColor[4],
                      BL_GUI_FLOAT fontSizeCoeff, bool alignTextRight,
                      int axisLinesOverlayColor[4],
                      bool alignRight)
{
    mOverlay = false;
    mLinesOverlay = false;
    //mOffset = offset;
    mOffsetX = offsetX;
    //mOffsetY = 0.0;
    mOffsetY = offsetY;
    
    mFontSizeCoeff = fontSizeCoeff;
    
    mScale = scale;
    mMinVal = minY;
    mMaxVal = maxY;
    
    mAlignTextRight = alignTextRight;
    mAlignRight = alignRight;
    
    InitAxis(axisColor, axisLabelColor,
             axisOverlayColor, axisLinesOverlayColor,
             lineWidth);
    
    NotifyGraph();
}

void
GraphAxis2::SetMinMaxValues(BL_GUI_FLOAT minVal, BL_GUI_FLOAT maxVal)
{
    mMinVal = minVal;
    mMaxVal = maxVal;
    
    NotifyGraph();
}

void
GraphAxis2::SetData(char *data[][2], int numData)
{
    mValues.clear();
    
    // Copy data
    for (int i = 0; i < numData; i++)
    {
        char *cData[2] = { data[i][0], data[i][1] };
        
        BL_GUI_FLOAT val = atof(cData[0]);
        BL_GUI_FLOAT t = (val - mMinVal)/(mMaxVal - mMinVal);
        
        t = Scale::ApplyScale(mScale, t, mMinVal, mMaxVal);
        
        string text(cData[1]);
        
        // Error here, if we add an Y axis, we must not use mMinXdB
        GraphAxisData aData;
        aData.mT = t;
        aData.mText = text;
        
        mValues.push_back(aData);
    }
    
    NotifyGraph();
}

void
GraphAxis2::SetBounds(BL_FLOAT bounds[2])
{
    mBounds[0] = bounds[0];
    mBounds[1] = bounds[1];
    
    NotifyGraph();
}

void
GraphAxis2::InitAxis(int axisColor[4], int axisLabelColor[4],
                     int axisLabelOverlayColor[4],
                     int axisLinesOverlayColor[4],
                     BL_GUI_FLOAT lineWidth)
{
    // Color
    int sAxisColor[4] = { axisColor[0], axisColor[1],
                          axisColor[2], axisColor[3] };
    SWAP_COLOR(sAxisColor);

    int sLabelColor[4] = { axisLabelColor[0], axisLabelColor[1],
                           axisLabelColor[2], axisLabelColor[3] };
    SWAP_COLOR(sLabelColor);
    
    int sOverColor[4] = { axisLabelOverlayColor[0], axisLabelOverlayColor[1],
                          axisLabelOverlayColor[2], axisLabelOverlayColor[3] };
    SWAP_COLOR(sOverColor);
    
    int sLineOverColor[4];
    if (axisLinesOverlayColor != NULL)
    {
        for (int i = 0; i < 4; i++)
        {
            sLineOverColor[i] = axisLinesOverlayColor[i];
        }
    }
    SWAP_COLOR(sLineOverColor);
    
    // Copy color
    for (int i = 0; i < 4; i++)
    {
        mColor[i] = sAxisColor[i];
        mLabelColor[i] = sLabelColor[i];
        
        mLabelOverlayColor[i] = 0;
        
        if (axisLabelOverlayColor != NULL)
        {
            mOverlay = true;
            mLabelOverlayColor[i] = sOverColor[i];
        }
        
        mLinesOverlayColor[i] = 0;
        
        if (axisLinesOverlayColor != NULL)
        {
            mLinesOverlay = true;
            
            mLinesOverlayColor[i] = sLineOverColor[i];
        }
    }
    
    mLineWidth = lineWidth;
    
    NotifyGraph();
}

void
GraphAxis2::SetGraph(GraphControl12 *graph)
{
    mGraph = graph;
}

void
GraphAxis2::NotifyGraph()
{
    if (mGraph != NULL)
        // Notify the graph
        mGraph->SetDataChanged();
}
