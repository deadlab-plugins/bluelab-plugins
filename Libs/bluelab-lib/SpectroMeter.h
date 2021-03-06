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
 
#ifndef SPECTRO_METER_H
#define SPECTRO_METER_H

#include <BLTypes.h>

#include <ITextButtonControl.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace iplug::igraphics;

class GUIHelper12;
class SpectroMeter
{
public:
    enum TimeMode
    {
        SPECTRO_METER_TIME_SAMPLES = 0,
        SPECTRO_METER_TIME_HMS
    };

    enum FreqMode
    {
        SPECTRO_METER_FREQ_HZ = 0,
        SPECTRO_METER_FREQ_BIN
    };

    enum DisplayType
    {
        SPECTRO_METER_DISPLAY_POS = 0,
        SPECTRO_METER_DISPLAY_SELECTION
    };
    
    //
    
    SpectroMeter(BL_FLOAT x, BL_FLOAT y,
                 BL_FLOAT textWidth,
                 int timeParamIdx, int freqParamIdx,
                 int buffersize, BL_FLOAT sampleRate,
                 DisplayType type = SPECTRO_METER_DISPLAY_SELECTION);
    virtual ~SpectroMeter();

    void Reset(int bufferSize, BL_FLOAT sampleRate);
    
    void GenerateUI(GUIHelper12 *guiHelper, IGraphics *graphics,
                    int offsetX, int offsetY = 0);
    void ClearUI();
    
    void SetCursorPosition(BL_FLOAT timeX, BL_FLOAT freqY);
    void ResetCursorPosition();
    
    void SetSelectionValues(BL_FLOAT timeX, BL_FLOAT freqY,
                            BL_FLOAT timeW, BL_FLOAT freqH);
    void ResetSelectionValues();

    void SetTimeMode(TimeMode mode);
    void SetFreqMode(FreqMode mode);

    // Style
    void SetTextFieldHSpacing(int spacing);
    void SetTextFieldVSpacing(int spacing);
    // Bigger spacing, between curso meter and selection meters
    void SetTextFieldVSpacing1(int spacing);

    void SetBackgroundColor(const IColor &color);
    void SetBorderColor(const IColor &color);
    void SetBorderWidth(float borderWidth);
    
protected:    
    void UpdateTextBGColor();

    void ConvertToHMS(BL_FLOAT timeSec,
                      int *h, int *m, int *s, int *ms);

    void TimeToStr(BL_FLOAT timeSec, char buf[256]);
    void HMSStr(BL_FLOAT timeSec, char buf[256]);
    void SamplesStr(BL_FLOAT timeSec, char buf[256]);

    void FreqToStr(BL_FLOAT freqHz, char buf[256]);
    
    BL_FLOAT AdjustFreq(BL_FLOAT freq);

    void RefreshValues();
    
    //
    TimeMode mTimeMode;
    FreqMode mFreqMode;
    
    int mTimeParamIdx;
    int mFreqParamIdx;

    int mBufferSize;
    BL_FLOAT mSampleRate;
    
    BL_FLOAT mX;
    BL_FLOAT mY;
    
    ITextButtonControl *mCursorPosTexts[2];
    ITextButtonControl *mSelPosTexts[2];
    ITextButtonControl *mSelSizeTexts[2];

    bool mSelectionActive;
    
    // Saved prev values
    BL_FLOAT mPrevCursorTimeX;
    BL_FLOAT mPrevCursorFreqY;

    BL_FLOAT mPrevSelTimeX;
    BL_FLOAT mPrevSelFreqY;
    
    BL_FLOAT mPrevSelTimeW;
    BL_FLOAT mPrevSelFreqH;

    DisplayType mDisplayType;

    // Style
    int mTextFieldHSpacing;
    int mTextFieldVSpacing;
    int mTextFieldVSpacing1;

    IColor mBGColor;

    IColor mBorderColor;
    float mBorderWidth;

    BL_FLOAT mTextWidth;
};

#endif
