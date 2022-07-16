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
 
#ifndef SO_FILE_SOURCE_H
#define SO_FILE_SOURCE_H

#include <BLUtilsFile.h>

#include <SoSourceImpl.h>

#include <Morpho_defs.h>

class GhostTrack2;
class SoFileSource : public SoSourceImpl
{
 public:
    SoFileSource(GhostTrack2 *ghostTrack);
    virtual ~SoFileSource();

    void SetFileName(const char *fileName);
    void GetFileName(char fileName[FILENAME_SIZE]);

    void GetName(char name[FILENAME_SIZE]) override;
    
    // Parameters
    void SetSpectroSelectionType(SelectionType type);
    SelectionType GetSpectroSelectionType() const;

    // Get selection on x only
    // Square selection for later
    void GetNormSelection(BL_FLOAT *x0, BL_FLOAT *x1);
    
protected:
    char mFileName[FILENAME_SIZE];

    // Parameters
    SelectionType mSelectionType;

    //
    GhostTrack2 *mGhostTrack;
};

#endif
