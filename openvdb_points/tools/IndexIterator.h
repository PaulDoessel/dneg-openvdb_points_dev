///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015-2016 Double Negative Visual Effects
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of Double Negative Visual Effects nor the names
// of its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
//
/// @file IndexIterator.h
///
/// @authors Dan Bailey
///
/// @brief  Index Iterators.
///


#ifndef OPENVDB_TOOLS_INDEX_ITERATOR_HAS_BEEN_INCLUDED
#define OPENVDB_TOOLS_INDEX_ITERATOR_HAS_BEEN_INCLUDED

#include <openvdb/version.h>
#include <openvdb/Types.h>

namespace openvdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {
namespace tools {


/// @brief Count up the number of times the iterator can iterate
///
/// @param iter the iterator.
///
/// @note counting by iteration only performed where a dynamic filter is in use,
template <typename IterT>
inline Index64 iterCount(const IterT& iter);


////////////////////////////////////////


/// @brief A no-op filter that can be used when iterating over all indices
class NullFilter
{
public:
    NullFilter() { }
    static bool initialized() { return true; }
    template <typename LeafT> void reset(const LeafT&) { }
    template <typename IterT> static bool valid(const IterT&) { return true; }
}; // class NullFilter


/// @brief A forward iterator over array indices in a single voxel
class ValueVoxelCIter
{
public:
    struct Parent
    {
        Parent(): mOffset(0) { }
        explicit Parent(Index32 offset): mOffset(offset) { }
        Index32 getValue(unsigned /*offset*/) const { return mOffset; }
    private:
        Index32 mOffset;
    }; // struct Parent

    typedef Parent NodeType;

    ValueVoxelCIter()
        : mOffset(0), mParent(), mValid(true) {}
    ValueVoxelCIter(Index32 prevOffset, Index32 offset)
        : mOffset(offset), mParent(prevOffset), mValid(true) {}
    ValueVoxelCIter(const ValueVoxelCIter& other)
        : mOffset(other.mOffset), mParent(other.mParent), mValid(other.mValid) { }

    /// @brief  Returns the item to which this iterator is currently pointing.
    inline Index32 operator*() { return mOffset; }
    inline Index32 operator*() const { return mOffset; }

    /// @brief  Advance to the next (valid) item (prefix).
    ValueVoxelCIter& operator++() {
        mValid = false;
        return *this;
    }

    inline operator bool() const { return mValid; }
    inline bool test() const { return mValid; }
    inline Index32 end() const { return mOffset+1; }

    inline void reset(Index32 /*item*/, Index32 /*end*/) { }

    Parent& parent() { return mParent; }
    Index32 offset() { return mOffset; }
    inline bool next() { this->operator++(); return this->test(); }

    /// @brief For efficiency, Coord assumed to be readily available when iterating over indices of a single voxel
    inline Coord getCoord() const { OPENVDB_THROW(RuntimeError, "ValueVoxelCIter does not provide a valid Coord."); }
    inline void getCoord(Coord& /*coord*/) const { OPENVDB_THROW(RuntimeError, "ValueVoxelCIter does not provide a valid Coord."); }

    /// @brief Equality operators
    inline bool operator==(const ValueVoxelCIter& other) const { return mOffset == other.mOffset; }
    inline bool operator!=(const ValueVoxelCIter& other) const { return !this->operator==(other); }

private:
    Index32 mOffset;
    Parent mParent;
    mutable bool mValid;
}; // class ValueVoxelCIter


/// @brief A forward iterator over array indices with filtering
/// IteratorT can be either IndexIter or ValueIndexIter (or some custom index iterator)
/// FilterT should be a struct or class with a valid() method than can be evaluated per index
/// Here's a simple filter example that only accepts even indices:
///
/// struct EvenIndexFilter
/// {
///     bool valid(const Index32 offset) const {
///         return (offset % 2) == 0;
///     }
/// };
///
template <typename IteratorT, typename FilterT>
class IndexIter
{
public:
    /// @brief A forward iterator over array indices from a value iterator (such as ValueOnCIter)
    class ValueIndexIter
    {
    public:
        ValueIndexIter(const IteratorT& iter)
            : mEnd(0), mItem(0), mIter(iter), mParent(&mIter.parent())
        {
            if (mIter) {
                assert(mParent);
                Index32 start = mIter.offset() > 0 ? Index32(mParent->getValue(mIter.offset() - 1)) : Index32(0);
                this->reset(start, *mIter);
                if (mItem >= mEnd)   this->operator++();
            }
        }
        ValueIndexIter(const ValueIndexIter& other)
            : mEnd(other.mEnd), mItem(other.mItem), mIter(other.mIter), mParent(other.mParent) { assert(mParent); }

        inline Index32 end() const { return mEnd; }

        inline void reset(Index32 item, Index32 end) {
            mItem = item;
            mEnd = end;
        }

        /// @brief  Returns the item to which this iterator is currently pointing.
        inline Index32 operator*() { assert(mIter); return mItem; }
        inline Index32 operator*() const { assert(mIter); return mItem; }

        /// @brief  Return @c true if this iterator is not yet exhausted.
        inline operator bool() const { return mIter; }
        inline bool test() const { return mIter; }

        /// @brief  Advance to the next (valid) item (prefix).
        inline ValueIndexIter& operator++() {
            ++mItem;
            while (mItem >= mEnd && mIter.next()) {
                assert(mParent);
                this->reset(mParent->getValue(mIter.offset() - 1), *mIter);
            }
            return *this;
        }

        /// @brief  Advance to the next (valid) item.
        inline bool next() { this->operator++(); return this->test(); }
        inline bool increment() { this->next(); return this->test(); }

        /// Return the coordinates of the item to which the value iterator is pointing.
        inline Coord getCoord() const { assert(mIter); return mIter.getCoord(); }
        /// Return in @a xyz the coordinates of the item to which the value iterator is pointing.
        inline void getCoord(Coord& xyz) const { assert(mIter); xyz = mIter.getCoord(); }

        /// Return the const value iterator
        inline const IteratorT& valueIter() const { return mIter; }

        /// @brief Equality operators
        bool operator==(const ValueIndexIter& other) const { return mItem == other.mItem; }
        bool operator!=(const ValueIndexIter& other) const { return !this->operator==(other); }

    private:
        Index32 mEnd, mItem;
        IteratorT mIter;
        const typename IteratorT::NodeType* mParent;
    }; // ValueIndexIter

    IndexIter(const IteratorT& iterator, const FilterT& filter)
        : mIterator(iterator)
        , mFilter(filter)
    {
        if (!mFilter.initialized()) {
            OPENVDB_THROW(RuntimeError, "Filter needs to be initialized before constructing the iterator.");
        }
        if (mIterator) {
            this->reset(*mIterator, mIterator.end());
        }
    }
    IndexIter(const IndexIter& other)
        : mIterator(other.mIterator)
        , mFilter(other.mFilter)
    {
        if (!mFilter.initialized()) {
            OPENVDB_THROW(RuntimeError, "Filter needs to be initialized before constructing the iterator.");
        }
    }

    Index32 end() const { return mIterator.end(); }

    /// @brief Reset the begining and end of the iterator.
    void reset(Index32 begin, Index32 end) {
        mIterator.reset(begin, end);
        while (mIterator.test() && !mFilter.template valid<ValueIndexIter>(mIterator)) {
            ++mIterator;
        }
    }

    /// @brief  Returns the item to which this iterator is currently pointing.
    Index32 operator*() { assert(mIterator); return *mIterator; }
    Index32 operator*() const { assert(mIterator); return *mIterator; }

    /// @brief  Return @c true if this iterator is not yet exhausted.
    operator bool() const { return mIterator.test(); }
    bool test() const { return mIterator.test(); }

    /// @brief  Advance to the next (valid) item (prefix).
    IndexIter& operator++() {
        while (true) {
            ++mIterator;
            if (!mIterator.test() || mFilter.template valid<ValueIndexIter>(mIterator)) {
                break;
            }
        }
        return *this;
    }

    /// @brief  Advance to the next (valid) item (postfix).
    IndexIter operator++(int /*dummy*/) {
        IndexIter newIterator(*this);
        this->operator++();
        return newIterator;
    }

    /// @brief  Advance to the next (valid) item.
    bool next() { this->operator++(); return this->test(); }
    bool increment() { this->next(); return this->test(); }

    /// Return the const filter
    inline const FilterT& filter() const { return mFilter; }

    /// Return the coordinates of the item to which the value iterator is pointing.
    inline Coord getCoord() const { assert(mIterator); return mIterator.getCoord(); }
    /// Return in @a xyz the coordinates of the item to which the value iterator is pointing.
    inline void getCoord(Coord& xyz) const { assert(mIterator); xyz = mIterator.getCoord(); }

    /// @brief Equality operators
    bool operator==(const IndexIter& other) const { return mIterator == other.mIterator; }
    bool operator!=(const IndexIter& other) const { return !this->operator==(other); }

private:
    ValueIndexIter mIterator;
    FilterT mFilter;
}; // class IndexIter


////////////////////////////////////////


template <typename IterT>
inline Index64 iterCount(const IterT& iter)
{
    Index64 size = 0;
    for (IterT newIter(iter); newIter; ++newIter, ++size) { }
    return size;
}


////////////////////////////////////////


} // namespace tools
} // namespace OPENVDB_VERSION_NAME
} // namespace openvdb


#endif // OPENVDB_TOOLS_INDEX_ITERATOR_HAS_BEEN_INCLUDED


// Copyright (c) 2015-2016 Double Negative Visual Effects
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
