//
//  AvgHistogram.cpp
//  EQHack
//
//  Created by Apple m'a Tuer on 10/09/17.
//
//

#include "MinMAxAvgHistogram.h"


MinMaxAvgHistogram::MinMaxAvgHistogram(int size, double smoothCoeff,
				       double defaultValue)
{
    mMinData.Resize(size);
    mMaxData.Resize(size);
    
    mSmoothCoeff = smoothCoeff;

    mDefaultValue = defaultValue;
    
    Reset();
}

MinMaxAvgHistogram::~MinMaxAvgHistogram() {}

void
MinMaxAvgHistogram::AddValue(int index, double val)
{
    // min
    double minVal = mMinData.Get()[index];
    if (val < minVal)
    {
        double newVal = (1.0 - mSmoothCoeff) * val + mSmoothCoeff*mMinData.Get()[index];
        mMinData.Get()[index] = newVal;
    }
    
    // max
    double maxVal = mMaxData.Get()[index];
    if (val > maxVal)
    {
        double newVal = (1.0 - mSmoothCoeff) * val + mSmoothCoeff*mMaxData.Get()[index];
        mMaxData.Get()[index] = newVal;
    }
}

void
MinMaxAvgHistogram::AddValues(WDL_TypedBuf<double> *values)
{
    if (values->GetSize() > mMinData.GetSize())
        return;
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        double val = values->Get()[i];
        
        AddValue(i, val);
    }
}

void
MinMaxAvgHistogram::GetMinValues(WDL_TypedBuf<double> *values)
{
    values->Resize(mMinData.GetSize());
    
    for (int i = 0; i < mMinData.GetSize(); i++)
    {
        double val = mMinData.Get()[i];
        
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogram::GetMaxValues(WDL_TypedBuf<double> *values)
{
    values->Resize(mMaxData.GetSize());
    
    for (int i = 0; i < mMaxData.GetSize(); i++)
    {
        double val = mMaxData.Get()[i];
        
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogram::GetAvgValues(WDL_TypedBuf<double> *values)
{
    values->Resize(mMaxData.GetSize());
    
    for (int i = 0; i < mMaxData.GetSize(); i++)
    {
        double minVal = mMinData.Get()[i];
        double maxVal = mMaxData.Get()[i];
        
        double val = (minVal + maxVal) / 2.0;
        values->Get()[i] = val;
    }
}

void
MinMaxAvgHistogram::Reset()
{
    for (int i = 0; i < mMinData.GetSize(); i++)
    {
        mMinData.Get()[i] = mDefaultValue;
        mMaxData.Get()[i] = mDefaultValue;
    }
}
