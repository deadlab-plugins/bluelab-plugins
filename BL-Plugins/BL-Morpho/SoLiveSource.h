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
 
#ifndef SO_LIVE_SOURCE_H
#define SO_LIVE_SOURCE_H

#include <SoSourceImpl.h>

class GhostTrack2;
class SoLiveSource : public SoSourceImpl
{
 public:
    SoLiveSource(GhostTrack2 *ghostTrack);
    virtual ~SoLiveSource();

    void GetName(char name[FILENAME_SIZE]) override;

protected:
    GhostTrack2 *mGhostTrack;
};

#endif
