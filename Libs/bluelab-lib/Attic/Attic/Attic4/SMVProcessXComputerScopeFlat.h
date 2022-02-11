//
//  SMVProcessXComputerScopeFlat.h
//  BL-SoundMetaViewer
//
//  Created by applematuer on 3/24/20.
//
//

#ifndef __BL_SoundMetaViewer__SMVProcessXComputerScopeFlat__
#define __BL_SoundMetaViewer__SMVProcessXComputerScopeFlat__

// Flat vectorscope
class Axis3DFactory2;
class SMVProcessXComputerScope;

class SMVProcessXComputerScopeFlat : public SMVProcessXComputer
{
public:
    SMVProcessXComputerScopeFlat(Axis3DFactory2 *axisFactory);
    
    virtual ~SMVProcessXComputerScopeFlat();
    
    virtual void Reset(BL_FLOAT sampleRate) {};
    
    void ComputeX(const WDL_TypedBuf<BL_FLOAT> samples[2],
                  const WDL_TypedBuf<BL_FLOAT> magns[2],
                  const WDL_TypedBuf<BL_FLOAT> phases[2],
                  WDL_TypedBuf<BL_FLOAT> *resultX,
                  WDL_TypedBuf<BL_FLOAT> *resultY = NULL,
                  bool *isPolar = NULL, BL_FLOAT polarCenter[2] = NULL,
                  bool *isScalable = NULL);
    
    Axis3D *CreateAxis();
    
protected:
    SMVProcessXComputerScope *mComputerScope;
    
    Axis3DFactory2 *mAxisFactory;
};

#endif /* defined(__BL_SoundMetaViewer__SMVProcessXComputerScopeFlat__) */
