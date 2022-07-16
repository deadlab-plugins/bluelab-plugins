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
 
#ifndef MORPHO_DEFS_H
#define MORPHO_DEFS_H

// TODO: move this somewhere else
#define WATERFALL_MAX_CAM_ANGLE_0 90.0
#define WATERFALL_MAX_CAM_ANGLE_1 90.0
#define WATERFALL_MIN_CAM_ANGLE_1 15.0
#define WATERFALL_MIN_FOV 15.0
#define WATERFALL_MAX_FOV 52.0

// View well aligned
/*#define WATERFALL_DEFAULT_CAM_ANGLE0 0.0
#define WATERFALL_DEFAULT_CAM_ANGLE1 WATERFALL_MIN_CAM_ANGLE_1
#define WATERFALL_DEFAULT_CAM_FOV    WATERFALL_MAX_FOV*/

// Cool intermediate orientation
#define WATERFALL_DEFAULT_CAM_ANGLE0 -13.0
#define WATERFALL_DEFAULT_CAM_ANGLE1 52.0
#define WATERFALL_DEFAULT_CAM_FOV    WATERFALL_MAX_FOV

#define MAX_NUM_SOURCES 5

#define BUFFER_SIZE 2048

// For analysis
#define MIN_AMP_DB -120.0

enum SelectionType
{
    RECTANGLE = 0,
    HORIZONTAL,
    VERTICAL
};

enum MorphoPlugMode
{
    MORPHO_PLUG_MODE_SYNTH = 0,
    MORPHO_PLUG_MODE_SOURCES
};

enum WaterfallViewMode
{
    MORPHO_WATERFALL_VIEW_AMP = 0,
    MORPHO_WATERFALL_VIEW_HARMONIC,
    MORPHO_WATERFALL_VIEW_NOISE,
    MORPHO_WATERFALL_VIEW_DETECTION,
    MORPHO_WATERFALL_VIEW_TRACKING,
    MORPHO_WATERFALL_VIEW_COLOR,
    MORPHO_WATERFALL_VIEW_WARPING
};

enum SoSourceType
{
    MORPHO_SOURCE_TYPE_BYPASS = 0,
    MORPHO_SOURCE_TYPE_MONOPHONIC,
    MORPHO_SOURCE_TYPE_COMPLEX
};

enum SySourceSynthType
{
    MORPHO_SOURCE_ALL_PARTIALS = 0,
    MORPHO_SOURCE_EVEN_PARTIALS,
    MORPHO_SOURCE_ODD_PARTIALS
};

#define MAX_NOISE_COEFF 2.0

#define MAX_TIME_STRETCH 5.0

#define PARTIAL_ID_FRAME_OFFSET 1024

#endif
