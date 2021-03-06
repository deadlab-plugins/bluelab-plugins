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
//  GraphTimeAxis2.cpp
//  BL-InfrasonicViewer
//
//  Created by applematuer on 11/22/19.
//
//

#ifdef IGRAPHICS_NANOVG

#include <GraphControl11.h>

#include "GraphTimeAxis2.h"

#define MAX_NUM_LABELS 128

// FIX: Clear the time axis if we change the time window
#define FIX_RESET_TIME_AXIS 1

GraphTimeAxis2::GraphTimeAxis2()
{
    mGraph = NULL;
    
    mCurrentTime = 0.0;
}

GraphTimeAxis2::~GraphTimeAxis2() {}

void
GraphTimeAxis2::Init(GraphControl11 *graph, int bufferSize,
                     BL_FLOAT timeDuration, BL_FLOAT spacingSeconds,
                     int yOffset)
{
    mGraph = graph;
    mBufferSize = bufferSize;
    mTimeDuration = timeDuration;
    mSpacingSeconds = spacingSeconds;
    
    //
    
    // Horizontal axis: time
#define MIN_HAXIS_VALUE 0.0
#define MAX_HAXIS_VALUE 1.0
    
#define NUM_HAXIS_DATA 6
    static char *HAXIS_DATA [NUM_HAXIS_DATA][2] =
    {
        { "0.0", "" },
        { "0.2", "" },
        { "0.4", "" },
        { "0.6", "" },
        { "0.8", "" },
        { "1.0", "" },
    };
    
    int hAxisColor[4] = { 48, 48, 48, /*255*/0 }; // invisible vertical bars
    // Choose maximum brightness color for labels,
    // to see them well over clear spectrograms
    int hAxisLabelColor[4] = { 255, 255, 255, 255 };
    int hAxisOverlayColor[4] = { 48, 48, 48, 255 };
    
    mGraph->AddHAxis(HAXIS_DATA, NUM_HAXIS_DATA, false, hAxisColor, hAxisLabelColor,
                     yOffset,
                     hAxisOverlayColor);
}

void
GraphTimeAxis2::Reset(int bufferSize, BL_FLOAT timeDuration,
                      BL_FLOAT spacingSeconds)
{
    mBufferSize = bufferSize;
    mTimeDuration = timeDuration;
    
    mSpacingSeconds = spacingSeconds;
    
#if FIX_RESET_TIME_AXIS
    // Reset the labels
    Update(mCurrentTime);
#endif
}

void
GraphTimeAxis2::Update(BL_FLOAT currentTime)
{
    // Just in case
    if (mGraph == NULL)
        return;
    
    mCurrentTime = currentTime;
    
    // Make a cool axis, with values sliding as we zoom
    // and units changing when necessary
    // (the border values have decimals, the middle values are rounded and slide)
#define EPS 1e-15
    
    BL_FLOAT startTime = currentTime - mTimeDuration;
    BL_FLOAT endTime = currentTime;
        
    BL_FLOAT duration = endTime - startTime;
        
    // Convert to milliseconds
    startTime *= 1000.0;
    duration *= 1000.0;
        
    char *hAxisData[MAX_NUM_LABELS][2];
        
    // Allocate
#define LABEL_MAX_SIZE 64
    for (int i = 0; i < MAX_NUM_LABELS; i++)
    {
        hAxisData[i][0] = new char[LABEL_MAX_SIZE];
        memset(hAxisData[i][0], '\0', LABEL_MAX_SIZE);
            
        hAxisData[i][1] = new char[LABEL_MAX_SIZE];
        memset(hAxisData[i][1], '\0', LABEL_MAX_SIZE);
    }
    
    BL_FLOAT tm = startTime;
    
    // Align to 0 seconds
    tm = tm - fmod(tm, mSpacingSeconds*1000.0);
    
    for (int i = 0; i < MAX_NUM_LABELS; i++)
    {
#if 0 // Bad if we want to display e.g 0.5s
        // Check the "units"
        if ((i > 0) && (i < mNumLabels - 1))
            // Middle values
        {
            if (duration >= 1000.0)
            {
                tm = ((int)(tm/1000.0))*1000.0;
            }
            else if (duration >= 100.0)
            {
                tm = ((int)(tm/100.0))*100.0;
            }
            else if (duration >= 10.0)
            {
                tm = ((int)(tm/10.0))*10.0;
            }
        }
#endif
        
        // Parameter
        BL_FLOAT t = (tm - startTime)/duration;
        
        sprintf(hAxisData[i][0], "%g", t);
            
        if ((i > 0) && (i < MAX_NUM_LABELS - 1)) // Squeeze the borders
        {
            int seconds = (int)tm/1000;
            int millis = tm - seconds*1000;
            
            // Default
            sprintf(hAxisData[i][1], "0s");
            
#if 0 // Prev formatting
            if ((seconds != 0) && (millis != 0))
                sprintf(hAxisData[i][1], "%ds %dms", seconds, millis);
            else
                if ((seconds == 0) && (millis != 0))
                    sprintf(hAxisData[i][1], "%dms", millis);
                else
                    if ((seconds != 0) && (millis == 0))
                        sprintf(hAxisData[i][1], "%ds", seconds);
#endif
            
#if 1 // New formatting
            if ((seconds != 0) && (millis != 0))
                sprintf(hAxisData[i][1], "%d.%ds", seconds, millis/100);
            else
                if ((seconds == 0) && (millis != 0))
                    sprintf(hAxisData[i][1], "%dms", millis);
                else
                    if ((seconds != 0) && (millis == 0))
                        sprintf(hAxisData[i][1], "%ds", seconds);
#endif
        }
        
        tm += mSpacingSeconds*1000.0;
        
        if (tm > startTime + duration)
            break;
    }
        
    mGraph->ReplaceHAxis(hAxisData, MAX_NUM_LABELS);
        
    // Free
    for (int i = 0; i < MAX_NUM_LABELS; i++)
    {
        delete []hAxisData[i][0];
        delete []hAxisData[i][1];
    }
}

BL_FLOAT
GraphTimeAxis2::ComputeTimeDuration(int numBuffers, int bufferSize,
                                    int oversampling, BL_FLOAT sampleRate)
{
    int numSamples = (numBuffers*bufferSize)/oversampling;
    
    BL_FLOAT timeDuration = numSamples/sampleRate;
    
    return timeDuration;
}

#endif // IGRAPHICS_NANOVG
