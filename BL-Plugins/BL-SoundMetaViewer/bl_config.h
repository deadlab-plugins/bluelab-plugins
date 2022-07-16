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
 
// Necessary to move this include from IPlug_include_in_plug_hdr.h to here
// This is necessary for Windows compilation, because there is another config.h file
// for externals, and Visual Studio includes the wrong one
#include "config.h"

// BlueLab
#define BL_GUI_TYPE_FLOAT 0 //1
#define BL_TYPE_FLOAT 0 //1
#define BL_FIX_FLT_DENORMAL_OBJ 0 //1
