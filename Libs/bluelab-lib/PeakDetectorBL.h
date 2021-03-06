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
 
#ifndef PEAK_DETECTOR_BL_H
#define PEAK_DETECTOR_BL_H

#include <PeakDetector.h>

class PeakDetectorBL : public PeakDetector
{
 public:
    PeakDetectorBL();
    virtual ~PeakDetectorBL();

    void DetectPeaks(const WDL_TypedBuf<BL_FLOAT> &data, vector<Peak> *peaks,
                     int minIndex = -1, int maxIndex = -1) override;

 protected:
    bool DiscardInvalidPeaks(const WDL_TypedBuf<BL_FLOAT> &data,
                             int peakIndex, int leftIndex, int rightIndex);
};

#endif
