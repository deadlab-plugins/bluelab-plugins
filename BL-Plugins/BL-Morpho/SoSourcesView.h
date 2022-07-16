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
 
#ifndef SO_SOURCES_VIEW_H
#define SO_SOURCES_VIEW_H

#include <BLUtilsFile.h>

#include <Morpho_defs.h>

// For listener
#include <ITabsBarControl.h>

class SoSource;
class SoSourcesViewListener
{
public:
    virtual void SoSourceChanged(const SoSource *source) = 0;
    virtual void OnRemoveSource(int sourceNum) = 0;
};

//class ITabsBarControl;
class SoSourceManager;
class MorphoObj;
class GUIHelper12;
class View3DPluginInterface;
class SoSourcesView : public ITabsBarListener
{
 public:
    SoSourcesView(Plugin *plug,
                  View3DPluginInterface *view3DListener,
                  MorphoObj *morphoObj,
                  SoSourceManager *sourceManager);
    virtual ~SoSourcesView();

    void SetListener(SoSourcesViewListener *listener);
    
    void SetTabsBar(ITabsBarControl *tabsBar);

    void SetGUIHelper(GUIHelper12 *guiHelper);
    void SetSpectroGUIParams(int graphX, int graphY,
                             const char *graphBitmapFN);
    void SetWaterfallGUIParams(int graphX, int graphY,
                               const char *graphBitmapFN);
    
    // Create a new tab and a new corresponding source
    void CreateNewTab();

    // Tabs bar
    void NewTab();
    void SelectTab(int tabNum);
    
    void OnTabSelected(int tabNum) override;
    void OnTabClose(int tabNum) override;

    void ReCreateTabs(int numTabs);

    void ClearGUI();
    void Refresh();

    //
    void OnUIOpen();
    void OnUIClose();

    void OnIdle();

    // Re-create spectrograms when returning to So mode
    void RecreateControls();
    
 protected:
    void EnableCurrentView();
    
    //
    Plugin *mPlug;
    View3DPluginInterface *mView3DListener;
    MorphoObj *mMorphoObj;

    ITabsBarControl *mTabsBar;

    SoSourcesViewListener *mListener;
    
    SoSourceManager *mSourceManager;

    //
    GUIHelper12 *mGUIHelper;
    
    // Spectro GUI
    int mSpectroGraphX;
    int mSpectroGraphY;
    char mSpectroGraphBitmapFN[FILENAME_SIZE];

    // Waterfall GUI
    int mWaterfallGraphX;
    int mWaterfallGraphY;
    char mWaterfallGraphBitmapFN[FILENAME_SIZE];
};

#endif
