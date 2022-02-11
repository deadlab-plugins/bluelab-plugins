//
//  SMVProcessColComputerMidSide.cpp
//  BL-SoundMetaViewer
//
//  Created by applematuer on 3/24/20.
//
//

#include <BLUtils.h>
#include <BLUtilsMath.h>

#include "SMVProcessColComputerMidSide.h"


SMVProcessColComputerMidSide::SMVProcessColComputerMidSide() {}

SMVProcessColComputerMidSide::~SMVProcessColComputerMidSide() {}

void
SMVProcessColComputerMidSide::ComputeCol(const WDL_TypedBuf<BL_FLOAT> magns[2],
                                         const WDL_TypedBuf<BL_FLOAT> phases[2],
                                         const WDL_TypedBuf<BL_FLOAT> phasesUnwrap[2],
                                         WDL_TypedBuf<BL_FLOAT> *resultCol)
{
    resultCol->Resize(magns[0].GetSize());
    
    WDL_TypedBuf<BL_FLOAT> ms;
    ComputeMidSide(magns, &ms);
    
    // Adapt the scale
    //BLUtils::ApplyParamShape(&ms, 0.5);
    BLUtils::ApplyParamShape(&ms, (BL_FLOAT)0.25);
    
    // Compute
    for (int i = 0; i < ms.GetSize(); i++)
    {
        BL_FLOAT m = ms.Get()[i];
        
        resultCol->Get()[i] = m;
    }
}

void
SMVProcessColComputerMidSide::ComputeMidSide(const WDL_TypedBuf<BL_FLOAT> magns[2],
                                             WDL_TypedBuf<BL_FLOAT> *ms)
{
    ms->Resize(magns[0].GetSize());
    
    for (int i = 0; i < magns[0].GetSize(); i++)
    {
        BL_FLOAT l = magns[0].Get()[i];
        BL_FLOAT r = magns[1].Get()[i];
        
        BL_FLOAT angle = std::atan2(r, l);
        
        // Adjust
        angle = -angle;
        angle -= M_PI/4.0;
        
        if (angle < 0.0)
            angle += M_PI;
        
        if (angle > M_PI)
            angle -= M_PI;
        
        // Center 0
        angle -= M_PI*0.5;
        
        // Take abs, for mid-side
        angle = std::fabs(angle);
        
        // Normalize angle
        angle = angle*(2.0/M_PI);
        
        // Mid will be 1, extreme side will be 0
        angle = 1.0 - angle;
        
        ms->Get()[i] = angle;
    }
}
