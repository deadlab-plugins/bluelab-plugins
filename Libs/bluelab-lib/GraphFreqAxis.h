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
 
//
//  GraphFreqAxis.h
//  BL-InfrasonicViewer
//
//  Created by applematuer on 11/22/19.
//
//

#ifndef __BL_InfrasonicViewer__GraphFreqAxis__
#define __BL_InfrasonicViewer__GraphFreqAxis__

#ifdef IGRAPHICS_NANOVG

class GraphControl11;
class GraphFreqAxis
{
public:
    GraphFreqAxis();
    
    virtual ~GraphFreqAxis();
    
    void Init(GraphControl11 *graph,
              int bufferSize, BL_FLOAT sampleRate);
    
    void Reset(int bufferSize, BL_FLOAT sampleRate);
    
protected:
    void Update();
    
    //
    GraphControl11 *mGraph;
    
    int mBufferSize;
    BL_FLOAT mSampleRate;
};

#endif // IGRAPHICS_NANOVG

#endif /* defined(__BL_InfrasonicViewer__GraphFreqAxis__) */
