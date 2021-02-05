#ifndef BL_QUEUE_H
#define BL_QUEUE_H

#include <vector>
using namespace std;

// Implementation of queue with fixed size, and circular buffer
// to get optimal memory (avoid memory allocation/deallocation)
template<class T> class bl_queue
{
public:
    bl_queue(int size) { mFixedSize = true; mData.resize(size); mCursor = 0; }
    bl_queue() { mFixedSize = true; mCursor = 0; }
    ~bl_queue() { mData.clear(); mNonFixedSizeData.clear(); }

    bool empty() const
    {
        if (mFixedSize)
            return mData.empty();
        else
            return mNonFixedSizeData.size();
    }
    
    void clear(T value)
    {
        if (mFixedSize)
        {
            for (int i = 0; i < mData.size(); i++)
                mData[i] = value;
            mCursor = 0;
        }
        else
        {
            mNonFixedSizeData.clear();
        }
    }

    void clear()
    {
        if (!mFixedSize)
            mNonFixedSizeData.clear();
    }
    
    long size() const
    {
        if (mFixedSize)
            return mData.size();
        else
            return mNonFixedSizeData.size();
    };
    
    // After this, the cursor is not well set
    // Need to call set_all() maybe, and restart
    // from the beginning in the caller.
    void resize(int size)
    {
        mNonFixedSizeData.clear();

        mFixedSize = true;
        
        mData.resize(size);
        mCursor = 0;
    }

    void set_fixed_size(bool fixed)
    {
        if (mFixedSize && !fixed)
        {
            mNonFixedSizeData.clear();
            for (int i = 0; i < mData.size(); i++)
            {
                mNonFixedSizeData.push_back(mData[(mCursor + i) % mData.size()]);
            }
            
            mData.clear();
            mCursor = 0;
        }
        else if (!mFixedSize && fixed)
        {
            mData.resize(mNonFixedSizeData.size());
            mCursor = 0;
            
            for (int i = 0; i < mNonFixedSizeData.size(); i++)
            {
                mData[i] = mNonFixedSizeData[i];
            }

            mNonFixedSizeData.clear();
        }

        mFixedSize = fixed;
    }

    // For fixed size
    //
    
    // Push a new value and pop at the same time
    // so the queue size stays the same, and
    // no memory is allocated or deallocated
    void push_pop(T value)
    {
        mData[mCursor] = value;
        mCursor = (mCursor + 1) % mData.size();
    }

    // For non fixed size
    //
    
    void push_back(T value)
    {
        set_fixed_size(false);
        mNonFixedSizeData.push_back(value);
    }

    void pop_front()
    {
        set_fixed_size(false);
        mNonFixedSizeData.pop_front();
    }
    
    // Get the i-th element
    T &operator[](int index)
    {
        if (mFixedSize)
            return mData[(mCursor + index) % mData.size()];
        else
            return mNonFixedSizeData[index];
    }

    const T &operator[](int index) const
    {
        if (mFixedSize)
            return mData[(mCursor + index) % mData.size()];
        else
            return mNonFixedSizeData[index];
    }
    
 protected:
    vector<T> mData;

    int mCursor;

    bool mFixedSize;
    deque<T> mNonFixedSizeData;
};
#endif
