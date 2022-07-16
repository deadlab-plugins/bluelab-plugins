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
 
#include <SySource.h>

#include "SySourceManager.h"

SySourceManager::SySourceManager()
{
    mCurrentSourceIdx = -1;

    mSampleRate = 44100.0;
    
    // Create the "mix" source
    NewSource();
    SySource *source = GetCurrentSource();
    if (source != NULL)
        source->SetTypeMixSource();
}

SySourceManager::~SySourceManager()
{
    for (int i = 0; i < mSources.size(); i++)
    {
        SySource *source = mSources[i];
        delete source;
    }
}

void
SySourceManager::NewSource()
{
    SySource *source = new SySource(mSampleRate);
    mSources.push_back(source);

    mCurrentSourceIdx = mSources.size() - 1;
}

int
SySourceManager::GetNumSources() const
{
    return mSources.size();
}

SySource *
SySourceManager::GetSource(int index)
{
    if (index >= mSources.size())
        return NULL;

    return mSources[index];
}

void
SySourceManager::RemoveSource(int index)
{
    if (index >= mSources.size())
        return;

    SySource *source = mSources[index];

    // Erase dos not call the destructor...
    mSources.erase(mSources.begin() + index);

    if (source != NULL)
        delete source;
    
    if (mCurrentSourceIdx >= mSources.size())
        // We removed the last source
        // Update the index
        mCurrentSourceIdx = mSources.size() - 1;
}

int
SySourceManager::GetCurrentSourceIdx() const
{
    return mCurrentSourceIdx;
}

void
SySourceManager::SetCurrentSourceIdx(int index)
{
    mCurrentSourceIdx = index;
}

SySource *
SySourceManager::GetCurrentSource()
{
    if ((mCurrentSourceIdx != -1) &&
        (mCurrentSourceIdx < mSources.size()))
        return mSources[mCurrentSourceIdx];
    
    return NULL;
}

void
SySourceManager::Reset(BL_FLOAT sampleRate)
{
    for (int i = 0; i < mSources.size(); i++)
    {
        SySource *source = mSources[i];
        source->Reset(sampleRate);
    }
}
