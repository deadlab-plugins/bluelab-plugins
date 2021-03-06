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
 
#ifndef ITABS_BAR_CONTROL_H
#define ITABS_BAR_CONTROL_H

#include <vector>
using namespace std;

#include <IControl.h>

#include <BLUtilsFile.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace iplug::igraphics;

class ITabsBarListener
{
public:
    virtual void OnTabSelected(int tabNum) = 0;
    virtual void OnTabClose(int tabNum) = 0;
};

class ITabsBarControl : public IControl
{
 public:
    ITabsBarControl(const IRECT &bounds, int paramIdx = kNoParameter);
    virtual ~ITabsBarControl();

    void SetListener(ITabsBarListener *listener);

    void Draw(IGraphics &g) override;
    void OnMouseDown(float x, float y, const IMouseMod &mod) override;
    void OnMouseOver(float x, float y, const IMouseMod &mod) override;
    void OnMouseOut() override;
    
    void NewTab(const char *name);
    void SelectTab(int tabNum);
    int GetSelectedTab() const;
    void CloseTab(int tabNum);
    int GetNumTabs() const;
    void SetTabName(int tabNum, const char *name);
    
    // Colors
    //
    
    // Bar background color 
    void SetBackgroundColor(const IColor &color);
    
    // For the tab that is enabled
    void SetTabEnabledColor(const IColor &color);
    
    // For the tab that is not enabled
    void SetTabDisabledColor(const IColor &color);
    
    // Lines around tabs
    void SetTabLinesColor(const IColor &color);

    void SetTabsRolloverColor(const IColor &color);
    
    void SetCrossColor(const IColor &color);
    
    void SetCrossRolloverColor(const IColor &color);

    void SetNameColor(const IColor &color);

    // Style
    void SetCrossLineWidth(float width);

    // For Morpho
    void SetFontSize(float fontSize);
    void SetFont(const char *font);
    
protected:
    void DrawBackground(IGraphics &g);
    void DrawTabs(IGraphics &g);
    void DrawCrosses(IGraphics &g);
    void DrawTabNames(IGraphics &g);
    
    void DisableAllRollover();
    
    int MouseOverCrossIdx(float x, float y);
    int MouseOverTabIdx(float x, float y);
    
    //
    // Bar background color 
    IColor mBGColor;
    // For the tab that is enabled
    IColor mTabEnabledColor;
    // For the tab that is not enabled
    IColor mTabDisabledColor;
    // Lines around tabs
    IColor mTabLinesColor;

    float mTabsLinesWidth;

    IColor mTabRolloverColor;
    
    IColor mCrossColor;
    float mCrossRatio;
    float mCrossLineWidth;

    IColor mCrossRolloverColor;

    IColor mNameColor;
    float mFontSize;
    char mFont[255];
    
    //
    ITabsBarListener *mListener;

    class Tab
    {
    public:
        Tab(const char *name);
        Tab(const Tab &other);
        
        virtual ~Tab();

        const char *GetName() const;
        const char *GetShortName() const;
        void SetName(const char *name);
                     
        void SetEnabled(bool flag);
        bool IsEnabled() const;

        // Rollover
        void SetTabRollover(bool flag);
        bool IsTabRollover() const;

        void SetCrossRollover(bool flag);
        bool IsCrossRollover() const;
        
    protected:        
        //
        char mName[FILENAME_SIZE];
        char mShortName[FILENAME_SIZE];

        bool mIsEnabled;

        bool mIsTabRollover;
        bool mIsCrossRollover;
    };

    vector<Tab> mTabs;
};

#endif /* ITABS_BAR_CONTROL_H */
