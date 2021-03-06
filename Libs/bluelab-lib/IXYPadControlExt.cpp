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

#include "IXYPadControlExt.h"

#define HANDLE_NUM_FRAMES 3

#define AUTO_HIGHLIGHT_HANDLES 0

IXYPadControlExt::IXYPadControlExt(Plugin *plug,
                                   const IRECT& bounds,
                                   const std::initializer_list<int>& params,
                                   const IBitmap& trackBitmap,
                                   float borderSize, bool reverseY)
: IControl(bounds, params)
{
    mPlug = plug;
    
    mTrackBitmap = trackBitmap;

    mBorderSize = borderSize;

    mReverseY = reverseY;
    
    mMouseDown = false;

    mListener = NULL;
}

IXYPadControlExt::~IXYPadControlExt() {}

void
IXYPadControlExt::SetListener(IXYPadControlExtListener *listener)
{
    mListener = listener;
}

void
IXYPadControlExt::AddHandle(IGraphics *pGraphics, const char *handleBitmapFname,
                            const std::initializer_list<int>& params)
{
    if (params.size() != 2)
        return;

    vector<int> paramsVec;
    for (auto& paramIdx : params) {
        paramsVec.push_back(paramIdx);
    }
    
    IBitmap handleBitmap =
        pGraphics->LoadBitmap(handleBitmapFname, HANDLE_NUM_FRAMES);
    
    Handle handle;
    handle.mBitmap = handleBitmap;
    handle.mParamIdx[0] = paramsVec[0];
    handle.mParamIdx[1] = paramsVec[1];

    handle.mOffsetX = 0.0;
    handle.mOffsetY = 0.0;

    handle.mPrevX = 0.0;
    handle.mPrevY = 0.0;

    handle.mIsGrabbed = false;

    handle.mIsEnabled = true;

    handle.mState = HANDLE_NORMAL;
    
    mHandles.push_back(handle);
}

int
IXYPadControlExt::GetNumHandles()
{
    return mHandles.size();
}

void
IXYPadControlExt::SetHandleEnabled(int handleNum, bool flag)
{
    if (handleNum >= mHandles.size())
        return;

    mHandles[handleNum].mIsEnabled = flag;

    //SetDirty(false);
    mDirty = true;
}

bool
IXYPadControlExt::IsHandleEnabled(int handleNum)
{
    if (handleNum >= mHandles.size())
        return false;

    return mHandles[handleNum].mIsEnabled;
}

void
IXYPadControlExt::SetHandleState(int handleNum, HandleState state)
{
    if (handleNum >= mHandles.size())
        return;

    mHandles[handleNum].mState = state;
}

void
IXYPadControlExt::Draw(IGraphics& g)
{
    DrawTrack(g);
    DrawHandles(g);
}

void
IXYPadControlExt::OnMouseDown(float x, float y, const IMouseMod& mod)
{
#ifndef __linux__
  if (mod.A)
  {
      //SetValueToDefault(GetValIdxForPos(x, y));
      if (!mHandles.empty())
      {
          if (mHandles[0].mIsEnabled)
          {
              // Set values to defaults
              IParam *param0 = mPlug->GetParam(mHandles[0].mParamIdx[0]);
              param0->Set(param0->GetDefault(true));
              
              IParam *param1 = mPlug->GetParam(mHandles[0].mParamIdx[1]);
              param1->Set(param1->GetDefault(true));
              
              // Force refresh, in case of handle param is also used e.g on knobs
              GUIHelper12::RefreshParameter(mPlug, mHandles[0].mParamIdx[0]);
              GUIHelper12::RefreshParameter(mPlug, mHandles[0].mParamIdx[1]);

              // For a good refresh
              mPlug->SendParameterValueFromUI(handle.mParamIdx[0],
                                              param0->GetNormalized());
              mPlug->SendParameterValueFromUI(handle.mParamIdx[1],
                                              param1->GetNormalized());
        
              if (mListener != NULL)
                  mListener->OnHandleChanged(0);
          }
      }
      
      return;
  }
#else
  // On Linux, Alt+click does nothing (at least on my xubuntu)
  // So use Ctrl-click instead to reset parameters
  if (mod.C)
  {
      //SetValueToDefault(GetValIdxForPos(x, y));
      if (!mHandles.empty())
      {
          if (mHandles[0].mIsEnabled)
          {
              // Set values to defaults
              IParam *param0 = mPlug->GetParam(mHandles[0].mParamIdx[0]);
              param0->Set(param0->GetDefault(true));
              
              IParam *param1 = mPlug->GetParam(mHandles[0].mParamIdx[1]);
              param1->Set(param1->GetDefault(true));
              
              // Force refresh, in case of handle param is also used e.g on knobs
              GUIHelper12::RefreshParameter(mPlug, mHandles[0].mParamIdx[0]);
              GUIHelper12::RefreshParameter(mPlug, mHandles[0].mParamIdx[1]);

              // For a good refresh
              mPlug->SendParameterValueFromUI(mHandles[0].mParamIdx[0],
                                              param0->GetNormalized());
              mPlug->SendParameterValueFromUI(mHandles[0].mParamIdx[1],
                                              param1->GetNormalized());
              
              if (mListener != NULL)
                  mListener->OnHandleChanged(0);
          }
      }
      
      return;
  }
#endif
        
    mMouseDown = true;

    for (int i = 0; i < mHandles.size(); i++)
    {
        Handle &handle = mHandles[i];
        if (!handle.mIsEnabled)
            continue;
        
        handle.mPrevX = x;
        handle.mPrevY = y;
    }
    
    // Check if we clicked exactly on and handle
    // In this case, do not make the handle jump
    float offsetX;
    float offsetY;
    int handleNum = MouseOnHandle(x, y, &offsetX, &offsetY);
    if (handleNum != -1)
    {
        mHandles[handleNum].mOffsetX = offsetX;
        mHandles[handleNum].mOffsetY = offsetY;

        mHandles[handleNum].mIsGrabbed = true;

#if AUTO_HIGHLIGHT_HANDLES
        // Hilight current handle
        if (mHandles[handleNum].mState != HANDLE_GRAYED_OUT)
            mHandles[handleNum].mState = HANDLE_HIGHLIGHTED;

        // Un-highlight other handles
        for (int i = 1; i < mHandles.size(); i++)
        {
            if (i != handleNum)
            {
                if (mHandles[i].mState != HANDLE_GRAYED_OUT)
                    mHandles[i].mState = HANDLE_NORMAL;
            }
        }
#endif
    }
    
    // Direct jump
    OnMouseDrag(x, y, 0., 0., mod);
}

void
IXYPadControlExt::OnMouseUp(float x, float y, const IMouseMod& mod)
{
    mMouseDown = false;

    for (int i = 0; i < mHandles.size(); i++)
    {
        Handle &handle = mHandles[i];

        if (!handle.mIsEnabled)
            continue;
        
        handle.mOffsetX = 0.0;
        handle.mOffsetY = 0.0;

        handle.mIsGrabbed = false;
    }
    
    SetDirty(true);
}

void
IXYPadControlExt::OnMouseDrag(float x, float y, float dX, float dY,
                              const IMouseMod& mod)
{
    for (int i = 0; i < mHandles.size(); i++)
    {
        Handle &handle = mHandles[i];

        if (!handle.mIsEnabled)
            continue;
        
        if (!handle.mIsGrabbed)
            continue;
        
        // For sensitivity
        if (mod.S) // Shift pressed => move slower
        {
#define PRECISION 0.25
            
            float newX = handle.mPrevX + PRECISION*dX;
            float newY = handle.mPrevY + PRECISION*dY;

            handle.mPrevX = newX;
            handle.mPrevY = newY;
            
            x = newX;
            y = newY;
        }

        // Manage well when hitting/releasing shift witing the same drag action
        handle.mPrevX = x;
        handle.mPrevY = y;
        
        // For dragging from click on the handle
        x -= handle.mOffsetX;
        y -= handle.mOffsetY;
    
        // Original code
        mRECT.Constrain(x, y);
    
        float xn = x;
        float yn = y;
        PixelsToParams(i, &xn, &yn);

        mPlug->GetParam(handle.mParamIdx[0])->SetNormalized(xn);
        mPlug->GetParam(handle.mParamIdx[1])->SetNormalized(yn);
        
        // Force refresh, in case of handle param is also used e.g on knobs
        GUIHelper12::RefreshParameter(mPlug, handle.mParamIdx[0]);
        GUIHelper12::RefreshParameter(mPlug, handle.mParamIdx[1]);

        // For a good refresh
        mPlug->SendParameterValueFromUI(handle.mParamIdx[0], xn);
        mPlug->SendParameterValueFromUI(handle.mParamIdx[1], yn);
        
        if (mListener != NULL)
            mListener->OnHandleChanged(i);
    }
    
    SetDirty(true);
}

bool
IXYPadControlExt::GetHandleNormPos(int handleNum,
                                   float *tx, float *ty,
                                   bool normRectify) const
{
    if (handleNum >= mHandles.size())
        return false;

    const Handle &handle = mHandles[handleNum];
    *tx = mPlug->GetParam(handle.mParamIdx[1])->Value();
    *ty = mPlug->GetParam(handle.mParamIdx[1])->Value();

    if (normRectify)
    {
        // Rectify, so if the pad is not square, we could even compute
        // good distance between handles later
        float w = GetRECT().W();
        float h = GetRECT().H();

        if (w < h)
            *tx *= w/h;
        else if (w > h)
            *ty *= h/w;
    }
    
    return true;
}

void
IXYPadControlExt::DrawTrack(IGraphics& g)
{
    IBlend blend = GetBlend();
    g.DrawBitmap(mTrackBitmap,
                 GetRECT().GetCentredInside(IRECT(0, 0, mTrackBitmap)),
                 0, &blend);
}

void
IXYPadControlExt::DrawHandles(IGraphics& g)
{
    // Draw them in reverse order, so we grab what we see
    for (int i = 0; i < mHandles.size(); i++)
    //for (int i = mHandles.size() - 1; i >= 0; i--)
    {
        const Handle &handle = mHandles[i];

        if (!handle.mIsEnabled)
            continue;
        
        float val0 = mPlug->GetParam(handle.mParamIdx[0])->Value();
        float val1 = mPlug->GetParam(handle.mParamIdx[1])->Value();
        
        float xn = mPlug->GetParam(handle.mParamIdx[0])->ToNormalized(val0);
        float yn = mPlug->GetParam(handle.mParamIdx[1])->ToNormalized(val1);

        if (!mReverseY)
            yn = 1.0 - yn;
    
        float x = xn;
        float y = yn;
        ParamsToPixels(i, &x, &y);

        int w = handle.mBitmap.W();
        int h = handle.mBitmap.H()/handle.mBitmap.N();

        IRECT handleRect(x, y, x + w, y + h);
        
        IBlend blend = GetBlend();
        g.DrawBitmap(handle.mBitmap, handleRect, (int)handle.mState, &blend);
    }
}

void
IXYPadControlExt::PixelsToParams(int handleNum, float *x, float *y)
{
    if (handleNum >= mHandles.size())
        return;

    if (!mHandles[handleNum].mIsEnabled)
        return;
    
    float w = mHandles[handleNum].mBitmap.W();
    float h = mHandles[handleNum].mBitmap.H()/mHandles[handleNum].mBitmap.N();
    
    *x = (*x - (mRECT.L + w/2 + mBorderSize)) /
        (mRECT.W() - w - mBorderSize*2.0);
    *y = 1.f - ((*y - (mRECT.T + h/2 + mBorderSize)) /
                (mRECT.H() - h - mBorderSize*2.0));

    if (mReverseY)
        *y = 1.0 - *y;
    
    // Bounds
    if (*x < 0.0)
        *x = 0.0;
    if (*x > 1.0)
        *x = 1.0;

    if (*y < 0.0)
        *y = 0.0;
    if (*y > 1.0)
        *y = 1.0;
}

void
IXYPadControlExt::ParamsToPixels(int handleNum, float *x, float *y)
{
    if (handleNum >= mHandles.size())
        return;

    if (!mHandles[handleNum].mIsEnabled)
        return;
    
    float w = mHandles[handleNum].mBitmap.W();
    float h = mHandles[handleNum].mBitmap.H()/mHandles[handleNum].mBitmap.N();
    
    *x = mRECT.L + mBorderSize + (*x)*(mRECT.W() - w - mBorderSize*2.0);
    *y = mRECT.T + mBorderSize + (*y)*(mRECT.H() - h - mBorderSize*2.0);
}

// Used to avoid handle jumps when clicking directly on it
// (In this case, we want to drag smoothly from the current position
// to a nearby position, without any jump)
int
IXYPadControlExt::MouseOnHandle(float mx, float my,
                                float *offsetX, float *offsetY)
{
    //for (int i = 0; i < mHandles.size(); i++)
    for (int i = mHandles.size() - 1; i >= 0; i--)
    {
        Handle &handle = mHandles[i];

        if (!handle.mIsEnabled)
            continue;
        
        float val0 = mPlug->GetParam(handle.mParamIdx[0])->Value();
        float val1 = mPlug->GetParam(handle.mParamIdx[1])->Value();
        
        float xn = mPlug->GetParam(handle.mParamIdx[0])->ToNormalized(val0);
        float yn = mPlug->GetParam(handle.mParamIdx[1])->ToNormalized(val1);
            
        if (!mReverseY)
            yn = 1.0 - yn;
    
        float x = xn;
        float y = yn;
        ParamsToPixels(i, &x, &y);

        int w = handle.mBitmap.W();
        int h = handle.mBitmap.H()/handle.mBitmap.N();

        IRECT handleRect(x, y, x + w, y + h);
        
        bool onHandle = handleRect.Contains(mx, my);

        *offsetX = 0.0;
        *offsetY = 0.0;
        if (onHandle)
        {
            *offsetX = mx - (x + w/2);
            *offsetY = my - (y + h/2);
        }

        if (onHandle)
            return i;
    }

    return -1;
}

void
IXYPadControlExt::RefreshAllHandlesParams()
{
    for (int i = mHandles.size() - 1; i >= 0; i--)
    {
        const Handle &handle = mHandles[i];

        if (!handle.mIsEnabled)
            continue;

        // Force refresh, in case of handle param is also used e.g on knobs
        GUIHelper12::RefreshParameter(mPlug, handle.mParamIdx[0]);
        GUIHelper12::RefreshParameter(mPlug, handle.mParamIdx[1]);

        IParam *param0 = mPlug->GetParam(handle.mParamIdx[0]);
        IParam *param1 = mPlug->GetParam(handle.mParamIdx[1]);
        
        // For a good refresh
        mPlug->SendParameterValueFromUI(handle.mParamIdx[0], param0->GetNormalized());
        mPlug->SendParameterValueFromUI(handle.mParamIdx[1], param1->GetNormalized());
    }
}
