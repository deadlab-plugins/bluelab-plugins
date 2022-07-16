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
 
#ifndef WATERFALL_SOURCE_H
#define WATERFALL_SOURCE_H

#include "IPlug_include_in_plug_hdr.h"
using namespace iplug;
using namespace igraphics;

#include <BLTypes.h>

#include <Morpho_defs.h>

// Parent class of SoSource and SySource
class GUIHelper12;
class View3DPluginInterface;
class MorphoWaterfallGUI;
class WaterfallSource
{
public:
    WaterfallSource(BL_FLOAT sampleRate, MorphoPlugMode plugMode);
    virtual ~WaterfallSource();

    virtual void Reset(BL_FLOAT sampleRate);

    virtual void OnUIOpen();
    virtual void OnUIClose();

    virtual void SetViewEnabled(bool flag);

    virtual void CreateWaterfallControls(GUIHelper12 *guiHelper,
                                         Plugin *plug,
                                         View3DPluginInterface *view3DListener,
                                         int grapX, int graphY,
                                         const char *graphBitmapFN);

    virtual void SetWaterfallViewMode(WaterfallViewMode mode);
    virtual WaterfallViewMode GetWaterfallViewMode() const;

    virtual void SetWaterfallCameraAngle0(BL_FLOAT angle);
    virtual void SetWaterfallCameraAngle1(BL_FLOAT angle);
    virtual void SetWaterfallCameraFov(BL_FLOAT angle);

protected:
    WaterfallViewMode mWaterfallViewMode;

    BL_FLOAT mWaterfallAngle0;
    BL_FLOAT mWaterfallAngle1;
    BL_FLOAT mWaterfallCameraFov;

    MorphoWaterfallGUI *mWaterfallGUI;
};

#endif
