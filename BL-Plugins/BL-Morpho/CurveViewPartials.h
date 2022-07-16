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
 
#ifndef CURVE_VIEW_PARTIALS_H
#define CURVE_VIEW_PARTIALS_H

#include <CurveView.h>

class CurveViewPartials : public CurveView
{
public:
    CurveViewPartials(int maxNumData = CURVE_VIEW_DEFAULT_NUM_DATA);
    virtual ~CurveViewPartials();

protected:
    void DrawCurve(NVGcontext *vg, int width, int height) override;

    void DrawPartials(NVGcontext *vg, int width, int height);
    void DrawPartialsCurve(NVGcontext *vg, int width, int height);

    void DrawBottomLine(NVGcontext *vg, int width, int height);
        
    //
    IColor mPartialsCurveColor;

    float mPartialsCurveLineWidth;
};

#endif
