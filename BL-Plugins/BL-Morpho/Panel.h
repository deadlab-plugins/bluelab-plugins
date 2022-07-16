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
 
#ifndef PANEL_H
#define PANEL_H

#include <vector>
using namespace std;

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace iplug::igraphics;

class Panel
{
public:
    Panel(IGraphics *graphics) { mGraphics = graphics; }
    virtual ~Panel() { Clear(); }

    void SetGraphics(IGraphics *graphics) { mGraphics = graphics; }
    
    void Add(IControl *control) { mControls.push_back(control); }

    void Add(const vector<IControl *> &controls)
    { mControls.insert(mControls.end(), controls.begin(), controls.end()); }
    
    void Clear()
    {
        if (mGraphics == NULL)
            return;

        for (int i = 0; i < mControls.size(); i++)
        {
            IControl *control = mControls[i];
            mGraphics->RemoveControl(control);
        }

        mControls.clear();
    }

protected:
    IGraphics *mGraphics;
    
    vector<IControl *> mControls;
};

#endif
