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
 
#include "BLProfiler.h"

#if BL_PROFILE

void __ProfilerAppendMessage__(const char *filename, const char *message)
{
    char fullFilename[512];
    sprintf(fullFilename, BASE_FILE"%s", filename);
    FILE *file = fopen(fullFilename, "a+");
	if (file != NULL)
		fprintf(file, "%s\n", message);
    fclose(file);
}

#endif