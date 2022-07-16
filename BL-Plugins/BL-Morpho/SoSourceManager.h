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
 
#ifndef SO_SOURCE_MANAGER_H
#define SO_SOURCE_MANAGER_H

#include <vector>
using namespace std;

#include <BLTypes.h>

class SoSource;
class SoSourceManager
{
 public:
    SoSourceManager();
    virtual ~SoSourceManager();

    void NewSource();

    int GetNumSources() const;
    SoSource *GetSource(int index);
    void RemoveSource(int index);

    int GetCurrentSourceIdx() const;
    void SetCurrentSourceIdx(int index);
    SoSource *GetCurrentSource();

    void Reset(BL_FLOAT sampleRate);

 protected:    
    //
    vector<SoSource *> mSources;

    int mCurrentSourceIdx;

    BL_FLOAT mSampleRate;
};

#endif
