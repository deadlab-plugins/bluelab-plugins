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
 
#include <GraphSwapColor.h>

#include "ViewTitle.h"

#define TITLE_FONT_SIZE 15.0
#define TITLE_FONT "OpenSans-Bold"

#define OVERLAY_OFFSET -1

ViewTitle::ViewTitle()
{
    memset(mTitle, '\0', TITLE_SIZE);

    // Bounds
    mX = 0.0;
    mY = 0.0;

    // Style
    mTitleColor = IColor(255, 255, 255, 255);
    mTitleColorDark = IColor(255, 48, 48, 48);
}

ViewTitle::~ViewTitle() {}

void
ViewTitle::SetPos(float x, float y)
{
    mX = x;
    mY = y;
}

void
ViewTitle::SetTitle(const char *title)
{
    strcpy(mTitle, title);
}

void
ViewTitle::PostDraw(NVGcontext *vg, int width, int height)
{
    nvgSave(vg);
    nvgReset(vg);

    // Text params
    nvgFontSize(vg, TITLE_FONT_SIZE);
	nvgFontFace(vg, TITLE_FONT);
    nvgFontBlur(vg, 0);
	nvgTextAlign(vg, NVG_ALIGN_LEFT |NVG_ALIGN_MIDDLE);

    // Dark color
    int sColorDark[4] = { mTitleColorDark.R, mTitleColorDark.G,
                          mTitleColorDark.B, mTitleColorDark.A };
    SWAP_COLOR(sColorDark);
    
    nvgFillColor(vg, nvgRGBA(sColorDark[0], sColorDark[1],
                             sColorDark[2], sColorDark[3]));

    // Draw dark text
    nvgText(vg, mX, mY, mTitle, NULL);
    
    // Dark color
    int sColor[4] = { mTitleColor.R, mTitleColor.G,
                      mTitleColor.B, mTitleColor.A };
    SWAP_COLOR(sColor);
    
    nvgFillColor(vg, nvgRGBA(sColor[0], sColor[1], sColor[2], sColor[3]));

    // Draw light text
    nvgText(vg, mX + OVERLAY_OFFSET, mY + OVERLAY_OFFSET, mTitle, NULL);
    
    nvgRestore(vg);
}
