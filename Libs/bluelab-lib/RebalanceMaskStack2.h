//
//  RebalanceMaskStack2.h
//  BL-Rebalance
//
//  Created by applematuer on 6/14/20.
//
//

#ifndef __BL_Rebalance__RebalanceMaskStack2__
#define __BL_Rebalance__RebalanceMaskStack2__

//#include <deque>
//using namespace std;
#include <bl_queue.h>

#include "IPlug_include_in_plug_hdr.h"

// RebalanceMaskStack2: from RebalanceMaskStack
// - changed input and output types
//
class RebalanceMaskStack2
{
public:
    RebalanceMaskStack2(int width, int stackDepth);
    
    virtual ~RebalanceMaskStack2();
    
    void Reset();
    
    void AddMask(const WDL_TypedBuf<BL_FLOAT> &mask);
    
    // Reference
    void GetMaskAvg(WDL_TypedBuf<BL_FLOAT> *mask);
    // May not work..
    void GetMaskVariance(WDL_TypedBuf<BL_FLOAT> *mask);
    // Good
    // If index is not -1, compute only the result for index
    void GetMaskWeightedAvg(WDL_TypedBuf<BL_FLOAT> *mask, int index = -1);
    // Not so bad, but worse
    void GetMaskVariance2(WDL_TypedBuf<BL_FLOAT> *mask);
    
    void GetLineAvg(WDL_TypedBuf<BL_FLOAT> *line, int lineNum);
                 
protected:
    //void AddMask(const deque<WDL_TypedBuf<BL_FLOAT> > &mask);
    void AddMask(const bl_queue<WDL_TypedBuf<BL_FLOAT> > &mask);
    
    void GetMaskAvg(bl_queue<WDL_TypedBuf<BL_FLOAT> > *mask);
    void GetMaskVariance(bl_queue<WDL_TypedBuf<BL_FLOAT> > *mask);
    void GetMaskWeightedAvg(bl_queue<WDL_TypedBuf<BL_FLOAT> > *mask, int index = -1);
    void GetMaskVariance2(bl_queue<WDL_TypedBuf<BL_FLOAT> > *mask);
    
    void BufferToQue(bl_queue<WDL_TypedBuf<BL_FLOAT> > *que,
                     const WDL_TypedBuf<BL_FLOAT> &buffer,
                     int width);
    
    void QueToBuffer(WDL_TypedBuf<BL_FLOAT> *buf,
                     const bl_queue<WDL_TypedBuf<BL_FLOAT> > &cols);
    
    // For variance2
    BL_FLOAT ComputeVariance(const bl_queue<WDL_TypedBuf<BL_FLOAT> > &history,
                             int index);

    
    //
    int mWidth;
    
    int mStackDepth;
    
    //deque<deque<WDL_TypedBuf<BL_FLOAT> > > mStack;
    bl_queue<bl_queue<WDL_TypedBuf<BL_FLOAT> > > mStack;
    
    // For variance2
    bl_queue<WDL_TypedBuf<BL_FLOAT> > mLastHistory;
    bl_queue<WDL_TypedBuf<BL_FLOAT> > mAvgHistory;
};

#endif /* defined(__BL_Rebalance__RebalanceMaskStack2__) */
