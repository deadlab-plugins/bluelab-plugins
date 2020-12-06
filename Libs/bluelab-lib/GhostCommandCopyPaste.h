#ifndef GHOST_COMMAND_COPY_PASTE_H
#define GHOST_COMMAND_COPY_PASTE_H

#include <GhostCommand.h>

#include "IPlug_include_in_plug_hdr.h"

class GhostCommandCopyPaste : public Command
{
public:
    GhostCommandCopyPaste();
   
    // To manage on copy then multiple pastes
    GhostCommandCopyPaste(const GhostCommandCopyPaste &other);
    
    virtual ~GhostCommandCopyPaste();
    
    void Copy(const vector<WDL_TypedBuf<BL_FLOAT> > &magns,
              const vector<WDL_TypedBuf<BL_FLOAT> > &phases,
              int offsetXLines);
    
    // Apply is paste
    void Apply(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
               vector<WDL_TypedBuf<BL_FLOAT> > *phases);
    
    void Undo(vector<WDL_TypedBuf<BL_FLOAT> > *magns,
              vector<WDL_TypedBuf<BL_FLOAT> > *phases);
    
    bool IsPasteDone();
    
    void ComputePastedSelection();
    
    void GetPastedSelection(BL_FLOAT pastedSelection[4], bool yLogScale);
    
    int GetOffsetXLines();
    
protected:
    WDL_TypedBuf<BL_FLOAT> mCopiedMagns;
    WDL_TypedBuf<BL_FLOAT> mCopiedPhases;
    
    int mOffsetXLines;
    
    BL_FLOAT mCopiedSelection[4];
    BL_FLOAT mPastedSelection[4];
    
    bool mIsPasteDone;
};

#endif
