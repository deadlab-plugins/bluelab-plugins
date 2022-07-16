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
 
#ifndef SY_SOURCES_VIEW_H
#define SY_SOURCES_VIEW_H

#include <BLUtilsFile.h>

#include <Morpho_defs.h>

// For listener
#include <IXYPadControlExt.h>

class SySource;
class SySourcesViewListener
{
public:
    virtual void SySourceChanged(const SySource *source) = 0;
};

//class IXYPadControlExt;
class SySourceManager;
class MorphoObj;
class IIconLabelControl;
class GUIHelper12;
class View3DPluginInterface;
class SySourcesView : public IXYPadControlExtListener
{
public:
    SySourcesView(Plugin *plug,
                  View3DPluginInterface *view3DListener,
                  MorphoObj *morphoObj,
                  SySourceManager *sourceManager);
    virtual ~SySourcesView();
    
    void SetListener(SySourcesViewListener *listener);
    
    void SetXYPad(IXYPadControlExt *xyPad);
    void SetIconLabel(IIconLabelControl *xyPad);

    void SetGUIHelper(GUIHelper12 *guiHelper);
    void SetWaterfallGUIParams(int graphX, int graphY,
                               const char *graphBitmapFN);
    
    // IXYPadControlListener
    void OnHandleChanged(int handleNum) override;
    
    void ClearGUI();
    void Refresh();

    void OnUIOpen();
    void OnUIClose();

    void OnIdle();

    void RecreateControls();
    
protected:
    void EnableCurrentView();
    
    //
    Plugin *mPlug;
    View3DPluginInterface *mView3DListener;
    MorphoObj *mMorphoObj;
    
    IXYPadControlExt *mXYPad;
    IIconLabelControl *mIconLabel;

    SySourcesViewListener *mListener;

    SySourceManager *mSourceManager;

    //
    GUIHelper12 *mGUIHelper;
    
    // Waterfall GUI
    int mWaterfallGraphX;
    int mWaterfallGraphY;
    char mWaterfallGraphBitmapFN[FILENAME_SIZE];
};

#endif
