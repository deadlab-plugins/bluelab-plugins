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
 
#include <GUIHelper12.h>
#include <IGUIResizeButtonControl.h>
#include <GraphControl11.h>

#include "ResizeGUIPluginInterface.h"

// Ableton, Windows
// - play
// - change to medium GUI
// - change to small GUI
// => after a little while, the medium GUI button is selected again automatically,
// and the GUI starts to resize, then it freezes
//
// FIX: use SetValueFromUserInput() to avoid a call by the host to VSTSetParameter()
// NOTE: may still freeze some rare times
#define FIX_ABLETON_RESIZE_GUI_FREEZE 1

ResizeGUIPluginInterface::ResizeGUIPluginInterface(Plugin *plug)
{
    mPlug = plug;
    mIsResizingGUI = false;
}

ResizeGUIPluginInterface::~ResizeGUIPluginInterface() {}

void
ResizeGUIPluginInterface::ApplyGUIResize(int guiSizeIdx)
{
    if (mIsResizingGUI)
        return;
    
    mIsResizingGUI = true;
    
    int newGUIWidth;
    int newGUIHeight;
    PreResizeGUI(guiSizeIdx, &newGUIWidth, &newGUIHeight);
    
    if (mPlug->GetUI() != NULL)
        // If GUI is currently opened
        mPlug->GetUI()->Resize(newGUIWidth, newGUIHeight, 1.0f, true);
    else
        // If GUI is currently closed
        // (changing parameter from Host native UI)
        mPlug->ResetLastEditorSize();
    
    mIsResizingGUI = false;
}

#if 0 // Prev
// BUG: with VST3, prev gui resize parameter is not reset
void
ResizeGUIPluginInterface::GUIResizeParamChange(int paramNum,
                                               int params[],
                                               IGUIResizeButtonControl *buttons[],
                                               int numParams)
{
    // For fix Ableton Windows resize GUI
    bool winPlatform = false;
#ifdef WIN32
    winPlatform = true;
#endif
    
    bool fixAbletonWin = false;
#if FIX_ABLETON_RESIZE_GUI_FREEZE
    fixAbletonWin = true;
#endif
    
    int val = mPlug->GetParam(params[paramNum])->Int();
    if (val == 1)
    {
        // Reset the two other buttons
        
        // For the moment, keep the only case of Ableton Windows
        // (because we already have tested all plugs on Mac,
        // and half of the hosts on Windows)
        if (!winPlatform || !fixAbletonWin ||
            (mPlug->GetHost() != kHostAbletonLive) ||
            (mPlug->GetUI() == NULL)) // host UI ?
        {
            for (int i = 0; i < numParams; i++)
            {
                if (i != paramNum)
                    GUIHelper12::ResetParameter(mPlug, params[i]);
            }
        }
        else
            // Ableton Windows + fix enabled
        {
            for (int i = 0; i < numParams; i++)
            {
                if (i != paramNum)
                {
                    if (buttons[i] != NULL)
                        buttons[i]->SetValueFromUserInput(0.0);
                }
            }
        }
    }
}
#endif

// New: made some clean
void
ResizeGUIPluginInterface::GUIResizeParamChange(int paramNum,
                                               int params[],
                                               IGUIResizeButtonControl *buttons[],
                                               int numParams)
{
    int val = mPlug->GetParam(params[paramNum])->Int();
    if (val == 1)
    {
        // Reset the two other buttons
        if (mPlug->GetUI() == NULL) // from host UI ?
        {
            for (int i = 0; i < numParams; i++)
            {
                if (i != paramNum)
                    GUIHelper12::ResetParameter(mPlug, params[i]);
            }
        }
        else // Reset directly the button
        {
            for (int i = 0; i < numParams; i++)
            {
                if (i != paramNum)
                {
                    if (buttons[i] != NULL)
                        buttons[i]->SetValueFromUserInput(0.0);
                }
            }
        }
    }
}

void
ResizeGUIPluginInterface::GUIResizeComputeOffsets(int defaultGUIWidth,
                                                  int defaultGUIHeight,
                                                  int newGUIWidth,
                                                  int newGUIHeight,
                                                  int *offsetX,
                                                  int *offsetY)
{
    *offsetX = newGUIWidth - defaultGUIWidth;
    *offsetY = newGUIHeight - defaultGUIHeight;
}

