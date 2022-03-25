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

#ifndef PANDA_ASSEMBLER_META_H_
#define PANDA_ASSEMBLER_META_H_

#include <memory>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "annotation.h"
#include "modifiers.h"

#include "assembly-type.h"

namespace panda::pandasm {

class Metadata {
public:
    class Error {
    public:
        enum class Type {
            INVALID_VALUE,
            MISSING_ATTRIBUTE,
            MISSING_VALUE,
            MULTIPLE_ATTRIBUTE,
            UNEXPECTED_ATTRIBUTE,
            UNEXPECTED_VALUE,
            UNKNOWN_ATTRIBUTE
        };

        Error(std::string msg, Type type) : msg_(std::move(msg)), type_(type) {}
        ~Error() = default;
        DEFAULT_MOVE_SEMANTIC(Error);
        DEFAULT_COPY_SEMANTIC(Error);

        std::string GetMessage() const
        {
            return msg_;
        }

        Type GetType() const
        {
            return type_;
        }

    private:
        std::string msg_;
        Type type_;
    };

    Metadata() = default;

    virtual ~Metadata() = default;

    std::optional<Error> SetAttribute(std::string_view attribute)
    {
        auto err = Validate(attribute);
        if (err) {
            return err;
        }

        SetFlags(attribute);

        return Store(attribute);
    }

    void RemoveAttribute(const std::string &attribute)
    {
        RemoveFlags(attribute);

        set_attributes_.erase(attribute);
    }

    bool GetAttribute(const std::string &attribute) const
    {
        return set_attributes_.find(attribute) != set_attributes_.cend();
    }

    std::optional<Error> SetAttributeValue(std::string_view attribute, std::string_view value)
    {
        auto err = Validate(attribute, value);
        if (err) {
            return err;
        }

        SetFlags(attribute, value);

        return StoreValue(attribute, value);
    }

    std::vector<std::string> GetAttributeValues(const std::string &attribute) const
    {
        auto it = attributes_.find(attribute);
        if (it == attributes_.cend()) {
            return {};
        }

        return it->second;
    }

    std::optional<std::string> GetAttributeValue(const std::string &attribute) const
    {
        auto values = GetAttributeValues(attribute);
        if (!values.empty()) {
            return values.front();
        }

        return {};
    }

    const std::unordered_set<std::string> &GetBoolAttributes() const
    {
        return set_attributes_;
    }

    const std::unordered_map<std::string, std::vector<std::string>> &GetAttributes() const
    {
        return attributes_;
    }

    virtual std::optional<Error> ValidateData()
    {
        return {};
    }

    DEFAULT_COPY_SEMANTIC(Metadata);

    DEFAULT_MOVE_SEMANTIC(Metadata);

protected:
    virtual std::optional<Error> Validate(std::string_view attribute) const = 0;

    virtual std::optional<Error> Validate(std::string_view attribute, std::string_view value) const = 0;

    virtual std::optional<Error> StoreValue(std::string_view attribute, std::string_view value)
    {
        std::string key(attribute);
        auto it = attributes_.find(key);
        if (it == attributes_.cend()) {
            std::tie(it, std::ignore) = attributes_.try_emplace(key);
        }

        it->second.emplace_back(value);

        return {};
    }

    virtual std::optional<Error> Store(std::string_view attribute)
    {
        set_attributes_.emplace(attribute);

        return {};
    }

    virtual void SetFlags(std::string_view attribute) = 0;

    virtual void SetFlags(std::string_view attribute, std::string_view value) = 0;

    virtual void RemoveFlags(std::string_view attribute) = 0;

    virtual void RemoveFlags(std::string_view attribute, std::string_view value) = 0;

    bool HasAttribute(std::string_view attribute) const
    {
        std::string key(attribute);
        return GetAttribute(key) || GetAttributeValue(key);
    }

    std::optional<Error> ValidateSize(std::string_view value) const;

private:
    std::unordered_set<std::string> set_attributes_;
    std::unordered_map<std::string, std::vector<std::string>> attributes_;
};

class AnnotationMetadata : public Metadata {
public:
    const std::vector<AnnotationData> &GetAnnotations() const
    {
        return annotations_;
    }

    void SetAnnotations(std::vector<AnnotationData> &&annotations)
    {
        annotations_ = std::forward<std::vector<AnnotationData>>(annotations);
    }

    void AddAnnotations(const std::vector<AnnotationData> &annotations)
    {
        annotations_.insert(annotations_.end(), annotations.begin(), annotations.end());
    }

    std::optional<Error> ValidateData() override;

protected:
    std::optional<Error> Store(std::string_view attribute) override;

    std::optional<Error> StoreValue(std::string_view attribute, std::string_view value) override;

    virtual bool IsAnnotationRecordAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

    virtual bool IsAnnotationIdAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

    virtual bool IsAnnotationElementTypeAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

    virtual bool IsAnnotationElementArrayComponentTypeAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

    virtual bool IsAnnotationElementNameAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

    virtual bool IsAnnotationElementValueAttribute([[maybe_unused]] std::string_view attribute) const
    {
        return false;
    }

private:
    class AnnotationElementBuilder {
    public:
        void Initialize(std::string_view name)
        {
            name_ = name;
            is_initialized_ = true;
        }

        void Reset()
        {
            name_.clear();
            values_.clear();
            type_ = {};
            component_type_ = {};
            is_initialized_ = false;
        }

        void SetType(Value::Type type)
        {
            type_ = type;
        }

        void SetComponentType(Value::Type type)
        {
            ASSERT(type != Value::Type::ARRAY);
            component_type_ = type;
        }

        std::optional<Error> AddValue(
            std::string_view value,
            const std::unordered_map<std::string, std::unique_ptr<AnnotationData>> &annotation_id_map);

        AnnotationElement CreateAnnotationElement()
        {
            if (IsArray()) {
                return AnnotationElement(name_,
                                         std::make_unique<ArrayValue>(component_type_.value(), std::move(values_)));
            }

            return AnnotationElement(name_, std::make_unique<ScalarValue>(values_.front()));
        }

        bool IsArray() const
        {
            return type_.value() == Value::Type::ARRAY;
        }

        bool IsTypeSet() const
        {
            return type_.has_value();
        }

        bool IsComponentTypeSet() const
        {
            return component_type_.has_value();
        }

        bool IsInitialized() const
        {
            return is_initialized_;
        }

        bool IsCompleted() const
        {
            if (!IsTypeSet()) {
                return false;
            }

            if (IsArray() && !IsComponentTypeSet()) {
                return false;
            }

            if (!IsArray() && values_.empty()) {
                return false;
            }

            return true;
        }

    private:
        bool is_initialized_ {false};
        std::string name_;
        std::optional<Value::Type> type_;
        std::optional<Value::Type> component_type_;
        std::vector<ScalarValue> values_;
    };

    class AnnotationBuilder {
    public:
        void Initialize(std::string_view name)
        {
            name_ = name;
            is_initialized_ = true;
        }

        void Reset()
        {
            name_.clear();
            elements_.clear();
            id_ = {};
            is_initialized_ = false;
        }

        void SetId(std::string_view id)
        {
            id_ = id;
        }

        std::string GetId() const
        {
            ASSERT(HasId());
            return id_.value();
        }

        void AddElement(AnnotationElement &&element)
        {
            elements_.push_back(std::forward<AnnotationElement>(element));
        }

        std::unique_ptr<AnnotationData> CreateAnnotationData()
        {
            return std::make_unique<AnnotationData>(name_, std::move(elements_));
        };

        void AddAnnnotationDataToVector(std::vector<AnnotationData> *annotations)
        {
            annotations->emplace_back(name_, std::move(elements_));
        }

        bool HasId() const
        {
            return id_.has_value();
        }

        bool IsInitialized() const
        {
            return is_initialized_;
        }

    private:
        std::string name_;
        std::optional<std::string> id_;
        std::vector<AnnotationElement> elements_;
        bool is_initialized_ {false};
    };

    std::optional<Metadata::Error> MeetExpRecordAttribute(std::string_view attribute, std::string_view value);
    std::optional<Metadata::Error> MeetExpIdAttribute(std::string_view attribute, std::string_view value);
    std::optional<Metadata::Error> MeetExpElementNameAttribute(std::string_view attribute, std::string_view value);
    std::optional<Metadata::Error> MeetExpElementTypeAttribute(std::string_view attribute, std::string_view value);
    std::optional<Metadata::Error> MeetExpElementArrayComponentTypeAttribute(std::string_view attribute,
                                                                             std::string_view value);
    std::optional<Metadata::Error> MeetExpElementValueAttribute(std::string_view attribute, std::string_view value);

    void InitializeAnnotationBuilder(std::string_view name)
    {
        if (IsParseAnnotation()) {
            ResetAnnotationBuilder();
        }

        annotation_builder_.Initialize(name);
    }

    void ResetAnnotationBuilder()
    {
        ASSERT(IsParseAnnotation());

        if (IsParseAnnotationElement() && annotation_element_builder_.IsCompleted()) {
            ResetAnnotationElementBuilder();
        }

        if (annotation_builder_.HasId()) {
            id_map_.insert({annotation_builder_.GetId(), annotation_builder_.CreateAnnotationData()});
        } else {
            annotation_builder_.AddAnnnotationDataToVector(&annotations_);
        }

        annotation_builder_.Reset();
    }

    bool IsParseAnnotation() const
    {
        return annotation_builder_.IsInitialized();
    }

    void InitializeAnnotationElementBuilder(std::string_view name)
    {
        if (IsParseAnnotationElement() && annotation_element_builder_.IsCompleted()) {
            ResetAnnotationElementBuilder();
        }

        annotation_element_builder_.Initialize(name);
    }

    void ResetAnnotationElementBuilder()
    {
        ASSERT(IsParseAnnotationElement());
        ASSERT(annotation_element_builder_.IsCompleted());

        annotation_builder_.AddElement(annotation_element_builder_.CreateAnnotationElement());

        annotation_element_builder_.Reset();
    }

    bool IsParseAnnotationElement() const
    {
        return annotation_element_builder_.IsInitialized();
    }

    AnnotationBuilder annotation_builder_;
    AnnotationElementBuilder annotation_element_builder_;
    std::vector<AnnotationData> annotations_;
    std::unordered_map<std::string, std::unique_ptr<AnnotationData>> id_map_;
};

class ItemMetadata : public AnnotationMetadata {
public:
    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    void SetAccessFlags(uint32_t access_flags)
    {
        access_flags_ = access_flags;
    }

    bool IsForeign() const;

private:
    uint32_t access_flags_ {0};
};

class RecordMetadata : public ItemMetadata {
public:
    virtual std::string GetBase() const;

    virtual std::vector<std::string> GetInterfaces() const;

    virtual bool IsAnnotation() const;

    virtual bool IsRuntimeAnnotation() const;

    virtual bool IsTypeAnnotation() const;

    virtual bool IsRuntimeTypeAnnotation() const;

protected:
    std::optional<Error> Validate(std::string_view attribute) const override;

    std::optional<Error> Validate(std::string_view attribute, std::string_view value) const override;

    void SetFlags(std::string_view attribute) override;

    void SetFlags(std::string_view attribute, std::string_view value) override;

    void RemoveFlags(std::string_view attribute) override;

    void RemoveFlags(std::string_view attribute, std::string_view value) override;
};

class FieldMetadata : public ItemMetadata {
public:
    void SetFieldType(const Type &type)
    {
        field_type_ = type;
    }

    Type GetFieldType() const
    {
        return field_type_;
    }

    void SetValue(const ScalarValue &value)
    {
        value_ = value;
    }

    std::optional<ScalarValue> GetValue() const
    {
        return value_;
    }

protected:
    std::optional<Error> StoreValue(std::string_view attribute, std::string_view value) override;

    std::optional<Error> Validate(std::string_view attribute) const override;

    std::optional<Error> Validate(std::string_view attribute, std::string_view value) const override;

    void SetFlags(std::string_view attribute) override;

    void SetFlags(std::string_view attribute, std::string_view value) override;

    void RemoveFlags(std::string_view attribute) override;

    void RemoveFlags(std::string_view attribute, std::string_view value) override;

    virtual bool IsValueAttribute(std::string_view attribute)
    {
        return attribute == "value";
    }

private:
    Type field_type_;
    std::optional<ScalarValue> value_;
};

class FunctionMetadata : public ItemMetadata {
public:
    virtual bool HasImplementation() const;

    virtual bool IsCtor() const;

    virtual bool IsCctor() const;

protected:
    std::optional<Error> Validate(std::string_view attribute) const override;

    std::optional<Error> Validate(std::string_view attribute, std::string_view value) const override;

    void SetFlags(std::string_view attribute) override;

    void SetFlags(std::string_view attribute, std::string_view value) override;

    void RemoveFlags(std::string_view attribute) override;

    void RemoveFlags(std::string_view attribute, std::string_view value) override;
};

class ParamMetadata : public AnnotationMetadata {
protected:
    std::optional<Error> Validate(std::string_view attribute) const override;

    std::optional<Error> Validate(std::string_view attribute, std::string_view value) const override;

    void SetFlags(std::string_view attribute) override;

    void SetFlags(std::string_view attribute, std::string_view value) override;

    void RemoveFlags(std::string_view attribute) override;

    void RemoveFlags(std::string_view attribute, std::string_view value) override;
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_META_H_
