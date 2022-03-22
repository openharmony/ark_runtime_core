/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PANDA_LIBPANDABASE_SERIALIZER_SERIALIZER_H_
#define PANDA_LIBPANDABASE_SERIALIZER_SERIALIZER_H_

#include <securec.h>

#include "utils/expected.h"
#include "for_each_tuple.h"
#include "tuple_to_struct.h"
#include "struct_to_tuple.h"
#include "concepts.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>

namespace panda::serializer {

inline uintptr_t ToUintPtr(const uint8_t *p)
{
    return reinterpret_cast<uintptr_t>(p);
}

inline const uint8_t *ToUint8tPtr(uintptr_t v)
{
    return reinterpret_cast<const uint8_t *>(v);
}

template <typename T>
// NOLINTNEXTLINE(google-runtime-references)
inline auto TypeToBuffer(const T &value, /* out */ std::vector<uint8_t> &buffer)
    -> std::enable_if_t<std::is_pod_v<T>, Expected<size_t, const char *>>
{
    const auto *ptr = reinterpret_cast<const uint8_t *>(&value);
    std::copy(ptr, ToUint8tPtr(ToUintPtr(ptr) + sizeof(value)), std::back_inserter(buffer));
    return sizeof(value);
}

template <class VecT>
// NOLINTNEXTLINE(google-runtime-references)
inline auto TypeToBuffer(const VecT &vec, /* out */ std::vector<uint8_t> &buffer)
    -> std::enable_if_t<is_vectorable_v<VecT> && std::is_pod_v<typename VecT::value_type>,
                        Expected<size_t, const char *>>
{
    using type = typename VecT::value_type;
    // pack size
    uint32_t size = vec.size() * sizeof(type);
    auto ret = TypeToBuffer(size, buffer);
    if (!ret) {
        return ret;
    }

    // pack data
    const auto *ptr = reinterpret_cast<const uint8_t *>(vec.data());
    const uint8_t *ptr_end = ToUint8tPtr(ToUintPtr(ptr) + size);
    std::copy(ptr, ptr_end, std::back_inserter(buffer));
    return ret.Value() + size;
}

template <class UnMap>
// NOLINTNEXTLINE(google-runtime-references)
inline auto TypeToBuffer(const UnMap &map, /* out */ std::vector<uint8_t> &buffer)
    -> std::enable_if_t<is_hash_mappable_v<UnMap>, Expected<size_t, const char *>>
{
    // pack size
    auto ret = TypeToBuffer(static_cast<uint32_t>(map.size()), buffer);
    if (!ret) {
        return ret;
    }

    // At the moment, we can't use: [key, value]
    // because clang-format-8 can't correctly detect the source code language.
    // https://bugs.llvm.org/show_bug.cgi?id=37433
    //
    for (const auto &it : map) {
        // pack key
        auto k = TypeToBuffer(it.first, buffer);
        if (!k) {
            return k;
        }
        ret.Value() += k.Value();

        // pack value
        auto v = TypeToBuffer(it.second, buffer);
        if (!v) {
            return v;
        }
        ret.Value() += v.Value();
    }

    return ret;
}

template <typename T>
Expected<size_t, const char *> BufferToType(const uint8_t *data, size_t size, /* out */ T &value)
{
    static_assert(std::is_pod<T>::value, "Type is not supported");

    if (sizeof(value) > size) {
        return Unexpected("Cannot deserialize POD type, the buffer is too small.");
    }

    auto *ptr = reinterpret_cast<uint8_t *>(&value);
    (void)memcpy_s(ptr, sizeof(value), data, sizeof(value));
    return sizeof(value);
}

// NOLINTNEXTLINE(google-runtime-references)
inline Expected<size_t, const char *> BufferToType(const uint8_t *data, size_t size, /* out */ std::string &str)
{
    // unpack size
    uint32_t str_size = 0;
    auto r = BufferToType(data, size, str_size);
    if (!r || str_size == 0) {
        return r;
    }
    ASSERT(r.Value() <= size);
    data = ToUint8tPtr(ToUintPtr(data) + r.Value());
    size -= r.Value();

    // unpack string
    if (size < str_size) {
        return Unexpected("Cannot deserialize string, the buffer is too small.");
    }

    str.resize(str_size);
    (void)memcpy_s(str.data(), str_size, data, str_size);
    return r.Value() + str_size;
}

template <typename T>
Expected<size_t, const char *> BufferToType(const uint8_t *data, size_t size, /* out */ std::vector<T> &vector)
{
    static_assert(std::is_pod<T>::value, "Type is not supported");

    // unpack size
    uint32_t vector_size = 0;
    auto r = BufferToType(data, size, vector_size);
    if (!r || vector_size == 0) {
        return r;
    }
    ASSERT(r.Value() <= size);
    data = ToUint8tPtr(ToUintPtr(data) + r.Value());
    size -= r.Value();

    // unpack data
    if (size < vector_size || (vector_size % sizeof(T))) {
        return Unexpected("Cannot deserialize vector, the buffer is too small.");
    }

    vector.resize(vector_size / sizeof(T));
    (void)memcpy_s(vector.data(), vector_size, data, vector_size);

    return r.Value() + vector_size;
}

template <typename K, typename V>
Expected<size_t, const char *> BufferToType(const uint8_t *data, size_t size, /* out */ std::unordered_map<K, V> &map)
{
    size_t backup_size = size;
    uint32_t count = 0;
    auto r = BufferToType(data, size, count);
    if (!r) {
        return r;
    }
    ASSERT(r.Value() <= size);
    data = ToUint8tPtr(ToUintPtr(data) + r.Value());
    size -= r.Value();

    for (uint32_t i = 0; i < count; ++i) {
        K key {};
        auto v = serializer::BufferToType(data, size, key);
        if (!v) {
            return v;
        }
        ASSERT(v.Value() <= size);
        data = ToUint8tPtr(ToUintPtr(data) + v.Value());
        size -= v.Value();

        V value {};
        v = serializer::BufferToType(data, size, value);
        if (!v) {
            return v;
        }
        ASSERT(v.Value() <= size);
        data = ToUint8tPtr(ToUintPtr(data) + v.Value());
        size -= v.Value();

        auto ret = map.emplace(std::make_pair(std::move(key), std::move(value)));
        if (!ret.second) {
            return Unexpected("Cannot emplace KeyValue to map.");
        }
    }
    return backup_size - size;
}

namespace internal {

class Serializer {
public:
    // NOLINTNEXTLINE(google-runtime-references)
    explicit Serializer(std::vector<uint8_t> &buffer) : buffer_(buffer) {}

    template <typename T>
    void operator()(const T &value)
    {
        TypeToBuffer(value, buffer_);
    }

    virtual ~Serializer() = default;

    NO_COPY_SEMANTIC(Serializer);
    NO_MOVE_SEMANTIC(Serializer);

private:
    std::vector<uint8_t> &buffer_;
};

class Deserializer {
public:
    Deserializer(const uint8_t *data, size_t size) : data_(data), size_(size) {}

    bool IsError() const
    {
        return error_ != nullptr;
    }

    const char *GetError() const
    {
        return error_;
    }

    size_t GetEndPosition() const
    {
        return pos_;
    }

    template <typename T>
    void operator()(T &value)
    {
        if (error_ != nullptr) {
            return;
        }

        ASSERT(pos_ < size_);
        const uint8_t *ptr = ToUint8tPtr(ToUintPtr(data_) + pos_);
        auto ret = BufferToType(ptr, size_ - pos_, value);
        if (!ret) {
            error_ = ret.Error();
            return;
        }
        pos_ += ret.Value();
    }

    virtual ~Deserializer() = default;

    NO_COPY_SEMANTIC(Deserializer);
    NO_MOVE_SEMANTIC(Deserializer);

private:
    const uint8_t *data_;
    size_t size_;
    size_t pos_ = 0;
    const char *error_ = nullptr;
};

}  // namespace internal

template <size_t N, typename Struct>
bool StructToBuffer(Struct &&str, /* out */ std::vector<uint8_t> &buffer)  // NOLINT(google-runtime-references)
{
    internal::ForEachTuple(internal::StructToTuple<N>(std::forward<Struct>(str)), internal::Serializer(buffer));
    return true;
}

template <size_t N, typename Struct>
Expected<size_t, const char *> RawBufferToStruct(const uint8_t *data, size_t size, /* out */ Struct &str)
{
    using S = std::remove_reference_t<Struct>;
    using TupleType = decltype(internal::StructToTuple<N, S>({}));

    TupleType tuple;
    internal::Deserializer deserializer(data, size);
    internal::ForEachTuple(tuple, deserializer);
    if (deserializer.IsError()) {
        return Unexpected(deserializer.GetError());
    }

    str = std::move(internal::TupleToStruct<S>(tuple));
    return deserializer.GetEndPosition();
}

template <size_t N, typename Struct>
bool BufferToStruct(const uint8_t *data, size_t size, /* out */ Struct &str)
{
    auto r = RawBufferToStruct<N>(data, size, str);
    if (!r) {
        return false;
    }
    return r.Value() == size;
}

template <size_t N, typename Struct>
bool BufferToStruct(const std::vector<uint8_t> &buffer, /* out */ Struct &str)
{
    return BufferToStruct<N>(buffer.data(), buffer.size(), str);
}

}  // namespace panda::serializer

#endif  // PANDA_LIBPANDABASE_SERIALIZER_SERIALIZER_H_
