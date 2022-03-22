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

#ifndef PANDA_LIBPANDABASE_UTILS_JSON_PARSER_H_
#define PANDA_LIBPANDABASE_UTILS_JSON_PARSER_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <algorithm>

#include "macros.h"

namespace panda {

class JsonObject {
public:
    class Value;
    using StringT = std::string;
    using NumT = double;
    using BoolT = bool;
    using ArrayT = std::vector<Value>;
    using Key = StringT;
    using JsonObjPointer = std::unique_ptr<JsonObject>;

    class Value {
    public:
        Value() = default;
        ~Value() = default;
        NO_COPY_SEMANTIC(Value);

        Value(Value &&rhs) noexcept
        {
            value_ = std::move(rhs.value_);
            rhs.value_ = std::monostate {};
        }

        Value &operator=(Value &&rhs) noexcept
        {
            value_ = std::move(rhs.value_);
            rhs.value_ = std::monostate {};
            return *this;
        }

        template <typename T>
        void SetValue(T &&rhs) noexcept
        {
            value_ = std::forward<T>(rhs);
        }

        template <typename T>
        T *Get()
        {
            return std::get_if<T>(&value_);
        }

        template <typename T>
        const T *Get() const
        {
            return std::get_if<T>(&value_);
        }

    private:
        std::variant<std::monostate, StringT, NumT, BoolT, ArrayT, JsonObjPointer> value_;
    };

    // Recursive descent parser:
    class Parser {
    public:
        explicit Parser(JsonObject *target) : current_obj_(target) {}
        bool Parse(const std::string &text);
        bool Parse(std::streambuf *stream_buf);

        virtual ~Parser() = default;

        NO_COPY_SEMANTIC(Parser);
        NO_MOVE_SEMANTIC(Parser);

    private:
        bool Parse();

        bool GetJsonObject(JsonObject *empty_obj);
        bool GetValue();
        bool GetString(char delim);
        bool GetJsonString();
        bool GetNum();
        bool GetBool();
        bool GetArray();
        bool InsertKeyValuePairIn(JsonObject *empty_obj);

        char GetSymbol();
        char PeekSymbol();
        bool TryGetSymbol(int symbol);

        static bool IsWhitespace(int symbol);

    private:
        std::istream istream_ {nullptr};
        JsonObject *current_obj_ {nullptr};
        Value parsed_temp_;
        StringT string_temp_;

        size_t log_recursion_level_ {0};
    };

public:
    JsonObject() = default;
    ~JsonObject() = default;
    NO_COPY_SEMANTIC(JsonObject);
    NO_MOVE_SEMANTIC(JsonObject);

    explicit JsonObject(const std::string &text)
    {
        Parser(this).Parse(text);
    }

    explicit JsonObject(std::streambuf *stream_buf)
    {
        Parser(this).Parse(stream_buf);
    }

    size_t GetSize() const
    {
        ASSERT(values_map_.size() == keys_.size());
        ASSERT(values_map_.size() == string_map_.size());
        return values_map_.size();
    }

    size_t GetIndexByKey(const Key &key) const
    {
        auto it = std::find(keys_.begin(), keys_.end(), key);
        if (it != keys_.end()) {
            return it - keys_.begin();
        }
        return static_cast<size_t>(-1);
    }

    const auto &GetKeyByIndex(size_t idx) const
    {
        ASSERT(idx < GetSize());
        return keys_[idx];
    }

    template <typename T>
    const T *GetValue(const Key &key) const
    {
        auto iter = values_map_.find(key);
        return (iter == values_map_.end()) ? nullptr : iter->second.Get<T>();
    }

    const StringT *GetValueSourceString(const Key &key) const
    {
        auto iter = string_map_.find(key);
        return (iter == string_map_.end()) ? nullptr : &iter->second;
    }

    template <typename T>
    const T *GetValue(size_t idx) const
    {
        auto iter = values_map_.find(GetKeyByIndex(idx));
        return (iter == values_map_.end()) ? nullptr : iter->second.Get<T>();
    }

    const auto &GetUnorderedMap() const
    {
        return values_map_;
    }

    bool IsValid() const
    {
        return is_valid_;
    }

private:
    bool is_valid_ {false};
    std::unordered_map<Key, Value> values_map_;
    // String representation is stored additionally as a "source" of scalar values:
    std::unordered_map<Key, StringT> string_map_;

    // Stores the order in which keys were added (allows to access elements by index):
    std::vector<Key> keys_;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_JSON_PARSER_H_
