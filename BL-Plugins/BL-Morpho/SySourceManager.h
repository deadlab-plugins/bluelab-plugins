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
 
#ifndef SY_SOURCE_MANAGER_H
#define SY_SOURCE_MANAGER_H

#include <vector>
using namespace std;

class SySource;
class SySourceManager
{
 public:
    SySourceManager();
    virtual ~SySourceManager();

    void NewSource();

    int GetNumSources() const;
    SySource *GetSource(int index);
    void RemoveSource(int index);

    int GetCurrentSourceIdx() const;
    void SetCurrentSourceIdx(int index);
    SySource *GetCurrentSource();

    void Reset(BL_FLOAT sampleRate);
    
 protected:
    vector<SySource *> mSources;

    int mCurrentSourceIdx;

    BL_FLOAT mSampleRate;
};

#endif
