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
//  AvgHistogram.cpp
//  EQHack
//
//  Created by Apple m'a Tuer on 10/09/17.
//
//

#include <BLUtils.h>
#include "MinMaxAvgHistogramDB.h"


MinMaxAvgHistogramDB::MinMaxAvgHistogramDB(int size, BL_FLOAT smoothCoeff, BL_FLOAT defaultValue,
                                           BL_FLOAT mindB, BL_FLOAT maxdB)
{
    mMinData.Resize(size);
    mMaxData.Resize(size);
    
    mSmoothCoeff = smoothCoeff;
    
    mDefaultValue = defaultValue;
    
    mMindB = mindB;
    mMaxdB = maxdB;
    
    Reset();
}

MinMaxAvgHistogramDB::~MinMaxAvgHistogramDB() {}

void
MinMaxAvgHistogramDB::AddValue(int index, BL_FLOAT val)
{
    val = BLUtils::NormalizedYTodB(val, mMindB, mMaxdB);
    
    // min
    BL_FLOAT minVal = mMinData.Get()[index];
    if (val < minVal)
    {
        BL_FLOAT newVal = (1.0 - mSmoothCoeff) * val + mSmoothCoeff*mMinData.Get()[index];
        mMinData.Get()[index] = newVal;
    }
    
    // max
    BL_FLOAT maxVal = mMaxData.Get()[index];
    if (val > maxVal)
    {
        BL_FLOAT newVal = (1.0 - mSmoothCoeff) * val + mSmoothCoeff*mMaxData.Get()[index];
        mMaxData.Get()[index] = newVal;
    }
}

void
MinMaxAvgHistogramDB::AddValues(WDL_TypedBuf<BL_FLOAT> *values)
{
    if (values->GetSize() > mMinData.GetSize())
        return;
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        BL_FLOAT val = values->Get()[i];
        
        AddValue(i, val);
    }
}

void
MinMaxAvgHistogramDB::GetMinValues(WDL_TypedBuf<BL_FLOAT> *values)
{
    values->Resize(mMinData.GetSize());
    
    for (int i = 0; i < mMinData.GetSize(); i++)
    {
        BL_FLOAT val = mMinData.Get()[i];
        
        BLUtils::NormalizedYTodBInv(val, mMindB, mMaxdB);
        
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogramDB::GetMaxValues(WDL_TypedBuf<BL_FLOAT> *values)
{
    values->Resize(mMaxData.GetSize());
    
    for (int i = 0; i < mMaxData.GetSize(); i++)
    {
        BL_FLOAT val = mMaxData.Get()[i];
        
        BLUtils::NormalizedYTodBInv(val, mMindB, mMaxdB);
        
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogramDB::GetAvgValues(WDL_TypedBuf<BL_FLOAT> *values)
{
    values->Resize(mMaxData.GetSize());
    
    for (int i = 0; i < mMaxData.GetSize(); i++)
    {
        BL_FLOAT minVal = mMinData.Get()[i];
        BL_FLOAT maxVal = mMaxData.Get()[i];
        
        BL_FLOAT val = (minVal + maxVal)/2.0;
        
        //BL_FLOAT val = BLUtils::AverageYDB(minVal, maxVal, mMindB, mMaxdB);
        
        BLUtils::NormalizedYTodBInv(val, mMindB, mMaxdB);
        
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogramDB::Reset()
{
    for (int i = 0; i < mMinData.GetSize(); i++)
    {
        mMinData.Get()[i] = mDefaultValue;
        mMaxData.Get()[i] = mDefaultValue;
    }
}
