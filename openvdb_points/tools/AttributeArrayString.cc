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
/// @file AttributeArrayString.cc
///
/// @authors Dan Bailey


#include <openvdb_points/tools/AttributeArrayString.h>

#include <openvdb/Metadata.h>
#include <openvdb/metadata/MetaMap.h>

#include <boost/algorithm/string/predicate.hpp> // startswith

#include <sstream>

namespace openvdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {
namespace tools {


namespace {

    bool isStringMeta(const Name& key, const Metadata::Ptr& meta)
    {
        // ensure the metadata is StringMetadata
        if (meta->typeName() != "string")           return false;
        // string attribute metadata must have a key that starts "string:"
        if (!boost::starts_with(key, "string:"))    return false;

        return true;
    }

    Name getStringKey(const StringIndexType index)
    {
        std::stringstream ss;
        ss << "string:" << (index - 1);
        return ss.str();
    }

    StringIndexType getStringIndex(const Name& key)
    {
        Name indexStr = key.substr(7, key.size() - 7);

        // extract the index as an unsigned integer
        std::istringstream indexSS(indexStr);
        Index index;
        indexSS >> index;

        return (index + 1);
    }

} // namespace


////////////////////////////////////////


// StringMetaInserter implementation


StringMetaInserter::StringMetaInserter(MetaMap& metadata)
    : mMetadata(metadata)
{
    // populate the cache
    resetCache();
}


void StringMetaInserter::insert(const Name& name)
{
    // name already exists, so do nothing

    if (std::binary_search(mValues.begin(), mValues.end(), name))  return;

    // find first unused index in the cache

    Index index = 1;
    for (std::vector<Index>::iterator   it = mIndices.begin(),
                                        itEnd = mIndices.end(); it != itEnd; ++it) {
        if (*it != index)    break;
        ++index;
    }

    // now insert into metadata

    const Name key = getStringKey(index);
    mMetadata.insertMeta(key, StringMetadata(name));

    // finally update the caches (insertion sort)

    mIndices.insert(std::upper_bound(mIndices.begin(), mIndices.end(), index), index);
    mValues.insert(std::upper_bound(mValues.begin(), mValues.end(), name), name);
}


void StringMetaInserter::resetCache()
{
    mIndices.clear();
    mValues.clear();

    for (MetaMap::ConstMetaIterator it = mMetadata.beginMeta(),
                                    itEnd = mMetadata.endMeta(); it != itEnd; ++it) {
        const Name& key = it->first;
        const Metadata::Ptr meta = it->second;

        // ensure the metadata is StringMetadata and key starts "string:"
        if (!isStringMeta(key, meta))   continue;

        // extract index and add to cache
        Index index = getStringIndex(key);
        mIndices.push_back(index);

        // extract value from metadata and add to cache
        StringMetadata* stringMeta = static_cast<StringMetadata*>(meta.get());
        assert(stringMeta);
        mValues.push_back(stringMeta->value());
    }

    std::sort(mIndices.begin(), mIndices.end());
    std::sort(mValues.begin(), mValues.end());
}


////////////////////////////////////////

// StringAttributeHandle implementation


StringAttributeHandle::Ptr
StringAttributeHandle::create(const AttributeArray& array, const MetaMap& metadata, const bool preserveCompression)
{
    return StringAttributeHandle::Ptr(new StringAttributeHandle(array, metadata, preserveCompression));
}


StringAttributeHandle::StringAttributeHandle(const AttributeArray& array,
                                             const MetaMap& metadata,
                                             const bool preserveCompression)
        : mHandle(array, preserveCompression)
        , mMetadata(metadata)
{
    if (!isString(array)) {
        OPENVDB_THROW(TypeError, "Cannot create a StringAttributeHandle for an attribute array that is not a string.");
    }
}


Name StringAttributeHandle::get(Index n, Index m) const
{
    Name name;
    this->get(name, n, m);
    return name;
}


void StringAttributeHandle::get(Name& name, Index n, Index m) const
{
    StringIndexType index = mHandle.get(n, m);

    // index zero is reserved for an empty string

    if (index == 0) {
        name = "";
        return;
    }

    const Name key = getStringKey(index);

    // key is assumed to exist in metadata

    openvdb::StringMetadata::ConstPtr meta = mMetadata.getMetadata<StringMetadata>(key);

    if (!meta) {
        OPENVDB_THROW(LookupError, "String attribute cannot be found with index - \"" << index << "\".");
    }

    name = meta->value();
}


////////////////////////////////////////

// StringAttributeWriteHandle implementation

StringAttributeWriteHandle::Ptr
StringAttributeWriteHandle::create(AttributeArray& array, const MetaMap& metadata, const bool expand)
{
    return StringAttributeWriteHandle::Ptr(new StringAttributeWriteHandle(array, metadata, expand));
}


StringAttributeWriteHandle::StringAttributeWriteHandle(AttributeArray& array,
                                                       const MetaMap& metadata,
                                                       const bool expand)
    : StringAttributeHandle(array, metadata, /*preserveCompression=*/ false)
    , mWriteHandle(array, expand)
{
    // populate the cache
    resetCache();
}


void StringAttributeWriteHandle::expand(bool fill)
{
    mWriteHandle.expand(fill);
}


void StringAttributeWriteHandle::collapse()
{
    // zero is used for an empty string
    mWriteHandle.collapse(0);
}


void StringAttributeWriteHandle::collapse(const Name& name)
{
    Index index = getIndex(name);
    mWriteHandle.collapse(index);
}


bool StringAttributeWriteHandle::compact()
{
    return mWriteHandle.compact();
}


void StringAttributeWriteHandle::fill(const Name& name)
{
    Index index = getIndex(name);
    mWriteHandle.fill(index);
}


void StringAttributeWriteHandle::set(Index n, const Name& name)
{
    Index index = getIndex(name);
    mWriteHandle.set(n, /*stride*/0, index);
}


void StringAttributeWriteHandle::set(Index n, Index m, const Name& name)
{
    Index index = getIndex(name);
    mWriteHandle.set(n, m, index);
}


void StringAttributeWriteHandle::resetCache()
{
    mCache.clear();

    // re-populate the cache

    for (MetaMap::ConstMetaIterator it = mMetadata.beginMeta(),
                                    itEnd = mMetadata.endMeta(); it != itEnd; ++it) {
        const Name& key = it->first;
        const Metadata::Ptr meta = it->second;

        // ensure the metadata is StringMetadata and key starts "string:"
        if (!isStringMeta(key, meta))   continue;

        StringMetadata* stringMeta = dynamic_cast<StringMetadata*>(meta.get());
        assert(stringMeta);

        // remove "string:"
        Index index = getStringIndex(key);

        // add to the cache
        mCache[stringMeta->value()] = index;
    }
}


Index StringAttributeWriteHandle::getIndex(const Name& name)
{
    // zero used for an empty string
    if (name.empty())   return Index(0);

    ValueMap::const_iterator it = mCache.find(name);

    if (it == mCache.end()) {
        OPENVDB_THROW(LookupError, "String does not exist in Metadata, insert it and reset the cache - \"" << name << "\".");
    }

    return it->second;
}


////////////////////////////////////////


} // namespace tools
} // namespace OPENVDB_VERSION_NAME
} // namespace openvdb


// Copyright (c) 2015-2016 Double Negative Visual Effects
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
