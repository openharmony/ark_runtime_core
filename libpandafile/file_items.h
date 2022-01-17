/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef PANDA_LIBPANDAFILE_FILE_ITEMS_H_
#define PANDA_LIBPANDAFILE_FILE_ITEMS_H_

#include "file.h"
#include "file_format_version.h"
#include "file_writer.h"
#include "macros.h"
#include "modifiers.h"
#include "type.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <list>

namespace panda::panda_file {

enum class ClassTag : uint8_t {
    NOTHING = 0x00,
    INTERFACES = 0x01,
    SOURCE_LANG = 0x02,
    RUNTIME_ANNOTATION = 0x03,
    ANNOTATION = 0x04,
    SOURCE_FILE = 0x05,
    RUNTIME_TYPE_ANNOTATION = 0x06,
    TYPE_ANNOTATION = 0x07
};

enum class MethodTag : uint8_t {
    NOTHING = 0x00,
    CODE = 0x01,
    SOURCE_LANG = 0x02,
    RUNTIME_ANNOTATION = 0x03,
    RUNTIME_PARAM_ANNOTATION = 0x04,
    DEBUG_INFO = 0x05,
    ANNOTATION = 0x06,
    PARAM_ANNOTATION = 0x07,
    TYPE_ANNOTATION = 0x08,
    RUNTIME_TYPE_ANNOTATION = 0x09
};

enum class FieldTag : uint8_t {
    NOTHING = 0x00,
    INT_VALUE = 0x01,
    VALUE = 0x02,
    RUNTIME_ANNOTATION = 0x03,
    ANNOTATION = 0x04,
    RUNTIME_TYPE_ANNOTATION = 0x05,
    TYPE_ANNOTATION = 0x06
};

enum class SourceLang : uint8_t { ECMASCRIPT, PANDA_ASSEMBLY, LAST = PANDA_ASSEMBLY };

static constexpr size_t ID_SIZE = File::EntityId::GetSize();
static constexpr size_t IDX_SIZE = sizeof(uint16_t);
static constexpr size_t TAG_SIZE = 1;
static constexpr uint32_t INVALID_OFFSET = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t MAX_INDEX_16 = std::numeric_limits<uint16_t>::max();
static constexpr uint32_t MAX_INDEX_32 = std::numeric_limits<uint32_t>::max();

enum class IndexType {
    // 16-bit indexes
    CLASS = 0x0,
    METHOD = 0x1,
    FIELD = 0x2,
    PROTO = 0x3,
    LAST_16 = PROTO,

    // 32-bit indexes
    LINE_NUMBER_PROG = 0x04,
    LAST_32 = LINE_NUMBER_PROG,

    NONE
};

static constexpr size_t INDEX_COUNT_16 = static_cast<size_t>(IndexType::LAST_16) + 1;

class IndexedItem;

class BaseItem {
public:
    using VisitorCallBack = std::function<bool(BaseItem *)>;

    BaseItem() = default;
    virtual ~BaseItem() = default;

    DEFAULT_COPY_SEMANTIC(BaseItem);
    DEFAULT_MOVE_SEMANTIC(BaseItem);

    size_t GetSize() const
    {
        return CalculateSize();
    }

    virtual size_t CalculateSize() const = 0;

    virtual void ComputeLayout() {};

    virtual size_t Alignment()
    {
        return 1;
    }

    virtual bool IsForeign() const
    {
        return false;
    }

    uint32_t GetOffset() const
    {
        return offset_;
    }

    panda_file::File::EntityId GetFileId() const
    {
        return panda_file::File::EntityId(offset_);
    }

    void SetOffset(uint32_t offset)
    {
        offset_ = offset;
    }

    bool NeedsEmit() const
    {
        return needs_emit_;
    }

    void SetNeedsEmit(bool needs_emit)
    {
        needs_emit_ = needs_emit;
    }

    const std::list<IndexedItem *> &GetIndexDependencies() const
    {
        return index_deps_;
    }

    void AddIndexDependency(IndexedItem *item)
    {
        ASSERT(item != nullptr);
        index_deps_.push_back(item);
    }

    void SetOrderIndex(uint32_t order)
    {
        order_ = order;
    }

    uint32_t GetOrderIndex() const
    {
        return order_;
    }

    bool HasOrderIndex() const
    {
        return order_ != INVALID_INDEX;
    }

    virtual bool Write(Writer *writer) = 0;

    virtual std::string GetName() const = 0;

    virtual void Dump([[maybe_unused]] std::ostream &os) const {}

    virtual void Visit([[maybe_unused]] const VisitorCallBack &cb) {}

private:
    bool needs_emit_ {true};
    uint32_t offset_ {0};
    uint32_t order_ {INVALID_INDEX};
    std::list<IndexedItem *> index_deps_;
};

class IndexedItem : public BaseItem {
public:
    uint32_t GetIndex(const BaseItem *item) const
    {
        auto *idx = FindIndex(item);
        ASSERT(idx != nullptr);
        return idx->index;
    }

    bool HasIndex(const BaseItem *item) const
    {
        return FindIndex(item) != nullptr;
    }

    void SetIndex(const BaseItem *start, const BaseItem *end, uint32_t index)
    {
        ASSERT(FindIndex(start, end) == nullptr);
        indexes_.push_back({start, end, index});
    }

    void ClearIndexes()
    {
        indexes_.clear();
    }

    void IncRefCount()
    {
        ++ref_count_;
    }

    void DecRefCount()
    {
        ASSERT(ref_count_ != 0);
        --ref_count_;
    }

    size_t GetRefCount() const
    {
        return ref_count_;
    }

    virtual IndexType GetIndexType() const
    {
        return IndexType::NONE;
    }

private:
    struct Index {
        const BaseItem *start;
        const BaseItem *end;
        uint32_t index;
    };

    const Index *FindIndex(const BaseItem *start, const BaseItem *end) const
    {
        auto it = std::find_if(indexes_.cbegin(), indexes_.cend(),
                               [start, end](const Index &idx) { return idx.start == start && idx.end == end; });

        return it != indexes_.cend() ? &*it : nullptr;
    }

    const Index *FindIndex(const BaseItem *item) const
    {
        ASSERT(item->HasOrderIndex());
        auto order_idx = item->GetOrderIndex();

        auto it = std::find_if(indexes_.cbegin(), indexes_.cend(), [order_idx](const Index &idx) {
            if (idx.start == nullptr && idx.end == nullptr) {
                return true;
            }

            if (idx.start == nullptr || idx.end == nullptr) {
                return false;
            }

            ASSERT(idx.start->HasOrderIndex());
            ASSERT(idx.end->HasOrderIndex());
            return idx.start->GetOrderIndex() <= order_idx && order_idx < idx.end->GetOrderIndex();
        });

        return it != indexes_.cend() ? &*it : nullptr;
    }

    std::vector<Index> indexes_;
    size_t ref_count_ {1};
};

class TypeItem : public IndexedItem {
public:
    explicit TypeItem(Type type) : type_(type) {}

    explicit TypeItem(Type::TypeId type_id) : type_(type_id) {}

    ~TypeItem() override = default;

    Type GetType() const
    {
        return type_;
    }

    IndexType GetIndexType() const override
    {
        return IndexType::CLASS;
    }

    DEFAULT_MOVE_SEMANTIC(TypeItem);
    DEFAULT_COPY_SEMANTIC(TypeItem);

private:
    Type type_;
};

class PrimitiveTypeItem : public TypeItem {
public:
    explicit PrimitiveTypeItem(Type type) : PrimitiveTypeItem(type.GetId()) {}

    explicit PrimitiveTypeItem(Type::TypeId type_id) : TypeItem(type_id)
    {
        ASSERT(GetType().IsPrimitive());
        SetNeedsEmit(false);
        SetOffset(GetType().GetFieldEncoding());
    }

    ~PrimitiveTypeItem() override = default;

    size_t CalculateSize() const override
    {
        return 0;
    }

    bool Write([[maybe_unused]] Writer *writer) override
    {
        return true;
    }

    std::string GetName() const override
    {
        return "primitive_type_item";
    }

    DEFAULT_MOVE_SEMANTIC(PrimitiveTypeItem);
    DEFAULT_COPY_SEMANTIC(PrimitiveTypeItem);
};

class StringItem : public BaseItem {
public:
    explicit StringItem(std::string str);

    ~StringItem() override = default;

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "string_item";
    }

    const std::string &GetData() const
    {
        return str_;
    }

    DEFAULT_MOVE_SEMANTIC(StringItem);
    DEFAULT_COPY_SEMANTIC(StringItem);

private:
    std::string str_;
    size_t utf16_length_ {0};
    size_t is_ascii_ {0};
};

class AnnotationItem;
class BaseClassItem;
class ClassItem;
class ForeignClassItem;
class ValueItem;

class BaseFieldItem : public IndexedItem {
public:
    IndexType GetIndexType() const override
    {
        return IndexType::FIELD;
    }

    ~BaseFieldItem() override = default;

    DEFAULT_MOVE_SEMANTIC(BaseFieldItem);
    DEFAULT_COPY_SEMANTIC(BaseFieldItem);

protected:
    BaseFieldItem(BaseClassItem *cls, StringItem *name, TypeItem *type);

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

private:
    BaseClassItem *class_ {nullptr};
    StringItem *name_ {nullptr};
    TypeItem *type_ {nullptr};
};

class FieldItem : public BaseFieldItem {
public:
    FieldItem(ClassItem *cls, StringItem *name, TypeItem *type, uint32_t access_flags);

    ~FieldItem() override = default;

    void SetValue(ValueItem *value);

    void AddRuntimeAnnotation(AnnotationItem *runtime_annotation)
    {
        runtime_annotations_.push_back(runtime_annotation);
    }

    void AddAnnotation(AnnotationItem *annotation)
    {
        annotations_.push_back(annotation);
    }

    void AddRuntimeTypeAnnotation(AnnotationItem *runtime_type_annotation)
    {
        runtime_type_annotations_.push_back(runtime_type_annotation);
    }

    void AddTypeAnnotation(AnnotationItem *type_annotation)
    {
        type_annotations_.push_back(type_annotation);
    }

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "field_item";
    }

    std::vector<AnnotationItem *> *GetRuntimeAnnotations()
    {
        return &runtime_annotations_;
    }

    std::vector<AnnotationItem *> *GetAnnotations()
    {
        return &annotations_;
    }

    std::vector<AnnotationItem *> *GetTypeAnnotations()
    {
        return &type_annotations_;
    }

    std::vector<AnnotationItem *> *GetRuntimeTypeAnnotations()
    {
        return &runtime_type_annotations_;
    }

    DEFAULT_MOVE_SEMANTIC(FieldItem);
    DEFAULT_COPY_SEMANTIC(FieldItem);

private:
    bool WriteValue(Writer *writer);

    bool WriteAnnotations(Writer *writer);

    bool WriteTaggedData(Writer *writer);

    uint32_t access_flags_ {0};
    ValueItem *value_ {nullptr};
    std::vector<AnnotationItem *> runtime_annotations_;
    std::vector<AnnotationItem *> annotations_;
    std::vector<AnnotationItem *> type_annotations_;
    std::vector<AnnotationItem *> runtime_type_annotations_;
};

class ProtoItem;
class CodeItem;

class LineNumberProgramItem : public IndexedItem {
public:
    enum class Opcode : uint8_t {
        END_SEQUENCE = 0x00,
        ADVANCE_PC = 0x01,
        ADVANCE_LINE = 0x02,
        START_LOCAL = 0x03,
        START_LOCAL_EXTENDED = 0x04,
        END_LOCAL = 0x05,
        RESTART_LOCAL = 0x06,
        SET_PROLOGUE_END = 0x07,
        SET_EPILOGUE_BEGIN = 0x08,
        SET_FILE = 0x09,
        SET_SOURCE_CODE = 0x0a,
        SET_COLUMN = 0X0b, // The SET_COLUMN opcode takes a single unsigned LEB128 operand and
                           // stores it in the column register of the state machine.
        LAST
    };

    static constexpr uint8_t OPCODE_BASE = static_cast<uint8_t>(Opcode::LAST);
    static constexpr int32_t LINE_RANGE = 15;
    static constexpr int32_t LINE_BASE = -4;

    void EmitEnd();

    void EmitAdvancePc(std::vector<uint8_t> *constant_pool, uint32_t value);

    void EmitAdvanceLine(std::vector<uint8_t> *constant_pool, int32_t value);

    void EmitStartLocal(std::vector<uint8_t> *constant_pool, int32_t register_number, StringItem *name,
                        StringItem *type);

    void EmitStartLocalExtended(std::vector<uint8_t> *constant_pool, int32_t register_number, StringItem *name,
                                StringItem *type, StringItem *type_signature);

    void EmitEndLocal(int32_t register_number);

    void EmitRestartLocal(int32_t register_number);

    bool EmitSpecialOpcode(uint32_t pc_inc, int32_t line_inc);

    void EmitColumn(std::vector<uint8_t> *constant_pool, uint32_t pc_inc, int32_t column);

    void EmitPrologEnd();

    void EmitEpilogBegin();

    void EmitSetFile(std::vector<uint8_t> *constant_pool, StringItem *source_file);

    void EmitSetSourceCode(std::vector<uint8_t> *constant_pool, StringItem *source_code);

    bool Write(Writer *writer) override;

    size_t CalculateSize() const override;

    std::string GetName() const override
    {
        return "line_number_program_item";
    }

    const std::vector<uint8_t> &GetData() const
    {
        return data_;
    }

    IndexType GetIndexType() const override
    {
        return IndexType::LINE_NUMBER_PROG;
    }

private:
    void EmitOpcode(Opcode opcode);
    void EmitRegister(int32_t register_number);

    static void EmitUleb128(std::vector<uint8_t> *data, uint32_t value);

    static void EmitSleb128(std::vector<uint8_t> *data, int32_t value);

    std::vector<uint8_t> data_;
};

class DebugInfoItem : public BaseItem {
public:
    explicit DebugInfoItem(LineNumberProgramItem *item) : program_(item) {}

    ~DebugInfoItem() override = default;

    DEFAULT_MOVE_SEMANTIC(DebugInfoItem);
    DEFAULT_COPY_SEMANTIC(DebugInfoItem);

    size_t GetLineNumber() const
    {
        return line_num_;
    }

    void SetLineNumber(size_t line_num)
    {
        line_num_ = line_num;
    }

    LineNumberProgramItem *GetLineNumberProgram() const
    {
        return program_;
    }

    void SetLineNumberProgram(LineNumberProgramItem *program)
    {
        ASSERT(program->GetOffset() != 0);
        program_ = program;
    }

    void AddParameter(StringItem *name)
    {
        parameters_.push_back(name);
    }

    std::vector<uint8_t> *GetConstantPool()
    {
        return &constant_pool_;
    }

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "debug_info_item";
    }

    void Dump(std::ostream &os) const override;

private:
    size_t line_num_ {0};
    LineNumberProgramItem *program_;
    std::vector<uint8_t> constant_pool_;
    std::vector<StringItem *> parameters_;
};

class BaseMethodItem : public IndexedItem {
public:
    ProtoItem *GetProto() const
    {
        return proto_;
    }

    bool IsStatic() const
    {
        return (access_flags_ & ACC_STATIC) != 0;
    }

    IndexType GetIndexType() const override
    {
        return IndexType::METHOD;
    }

    StringItem *GetNameItem() const
    {
        return name_;
    }

    ~BaseMethodItem() override = default;

    DEFAULT_MOVE_SEMANTIC(BaseMethodItem);
    DEFAULT_COPY_SEMANTIC(BaseMethodItem);

protected:
    BaseMethodItem(BaseClassItem *cls, StringItem *name, ProtoItem *proto, uint32_t access_flags);

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

private:
    BaseClassItem *class_ {nullptr};
    StringItem *name_ {nullptr};
    ProtoItem *proto_ {nullptr};
    uint32_t access_flags_ {0};
};

class MethodParamItem {
public:
    explicit MethodParamItem(TypeItem *type) : type_(type) {}
    ~MethodParamItem() = default;
    DEFAULT_MOVE_SEMANTIC(MethodParamItem);
    DEFAULT_COPY_SEMANTIC(MethodParamItem);

    void AddRuntimeAnnotation(AnnotationItem *runtime_annotation)
    {
        runtime_annotations_.push_back(runtime_annotation);
    }

    void AddAnnotation(AnnotationItem *annotation)
    {
        annotations_.push_back(annotation);
    }

    void AddRuntimeTypeAnnotation(AnnotationItem *runtime_type_annotation)
    {
        runtime_type_annotations_.push_back(runtime_type_annotation);
    }

    void AddTypeAnnotation(AnnotationItem *type_annotation)
    {
        type_annotations_.push_back(type_annotation);
    }

    TypeItem *GetType() const
    {
        return type_;
    }

    const std::vector<AnnotationItem *> &GetRuntimeAnnotations() const
    {
        return runtime_annotations_;
    }

    const std::vector<AnnotationItem *> &GetAnnotations() const
    {
        return annotations_;
    }

    bool HasAnnotations() const
    {
        return !annotations_.empty();
    }

    bool HasRuntimeAnnotations() const
    {
        return !runtime_annotations_.empty();
    }

private:
    TypeItem *type_;
    std::vector<AnnotationItem *> runtime_annotations_;
    std::vector<AnnotationItem *> annotations_;
    std::vector<AnnotationItem *> type_annotations_;
    std::vector<AnnotationItem *> runtime_type_annotations_;
};

class ParamAnnotationsItem;

class MethodItem : public BaseMethodItem {
public:
    MethodItem(ClassItem *cls, StringItem *name, ProtoItem *proto, uint32_t access_flags,
               std::vector<MethodParamItem> params);

    ~MethodItem() override = default;

    DEFAULT_MOVE_SEMANTIC(MethodItem);
    DEFAULT_COPY_SEMANTIC(MethodItem);

    void SetSourceLang(SourceLang lang)
    {
        source_lang_ = lang;
    }

    void SetCode(CodeItem *code)
    {
        code_ = code;
    }

    void SetDebugInfo(DebugInfoItem *debug_info)
    {
        debug_info_ = debug_info;
    }

    void AddRuntimeAnnotation(AnnotationItem *runtime_annotation)
    {
        runtime_annotations_.push_back(runtime_annotation);
    }

    void AddAnnotation(AnnotationItem *annotation)
    {
        annotations_.push_back(annotation);
    }

    void AddRuntimeTypeAnnotation(AnnotationItem *runtime_type_annotation)
    {
        runtime_type_annotations_.push_back(runtime_type_annotation);
    }

    void AddTypeAnnotation(AnnotationItem *type_annotation)
    {
        type_annotations_.push_back(type_annotation);
    }

    void SetRuntimeParamAnnotationItem(ParamAnnotationsItem *annotations)
    {
        runtime_param_annotations_ = annotations;
    }

    void SetParamAnnotationItem(ParamAnnotationsItem *annotations)
    {
        param_annotations_ = annotations;
    }

    bool HasRuntimeParamAnnotations() const
    {
        return std::any_of(params_.cbegin(), params_.cend(),
                           [](const MethodParamItem &item) { return item.HasRuntimeAnnotations(); });
    }

    bool HasParamAnnotations() const
    {
        return std::any_of(params_.cbegin(), params_.cend(),
                           [](const MethodParamItem &item) { return item.HasAnnotations(); });
    }

    CodeItem *GetCode() const
    {
        return code_;
    }

    DebugInfoItem *GetDebugInfo() const
    {
        return debug_info_;
    }

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "method_item";
    }

    std::vector<MethodParamItem> &GetParams()
    {
        return params_;
    }

    std::vector<AnnotationItem *> *GetRuntimeAnnotations()
    {
        return &runtime_annotations_;
    }

    std::vector<AnnotationItem *> *GetAnnotations()
    {
        return &annotations_;
    }

    std::vector<AnnotationItem *> *GetTypeAnnotations()
    {
        return &type_annotations_;
    }

    std::vector<AnnotationItem *> *GetRuntimeTypeAnnotations()
    {
        return &runtime_type_annotations_;
    }

private:
    bool WriteRuntimeAnnotations(Writer *writer);

    bool WriteTypeAnnotations(Writer *writer);

    bool WriteTaggedData(Writer *writer);

    std::vector<MethodParamItem> params_;

    SourceLang source_lang_;
    CodeItem *code_;
    DebugInfoItem *debug_info_;
    std::vector<AnnotationItem *> runtime_annotations_;
    std::vector<AnnotationItem *> annotations_;
    std::vector<AnnotationItem *> type_annotations_;
    std::vector<AnnotationItem *> runtime_type_annotations_;
    ParamAnnotationsItem *runtime_param_annotations_ {nullptr};
    ParamAnnotationsItem *param_annotations_ {nullptr};
};

class BaseClassItem : public TypeItem {
public:
    StringItem *GetNameItem()
    {
        return &name_;
    }

protected:
    explicit BaseClassItem(const std::string &name) : TypeItem(Type::TypeId::REFERENCE), name_(name) {}

    ~BaseClassItem() override = default;

    size_t CalculateSize() const override;

    void ComputeLayout() override;

    bool Write(Writer *writer) override;

    DEFAULT_MOVE_SEMANTIC(BaseClassItem);
    DEFAULT_COPY_SEMANTIC(BaseClassItem);

private:
    StringItem name_;
};

class ClassItem : public BaseClassItem {
public:
    explicit ClassItem(const std::string &name)
        : BaseClassItem(name),
          super_class_(nullptr),
          access_flags_(0),
          source_lang_(SourceLang::PANDA_ASSEMBLY),
          source_file_(nullptr)
    {
    }

    ~ClassItem() override = default;

    void SetAccessFlags(uint32_t access_flags)
    {
        access_flags_ = access_flags;
    }

    void SetSourceLang(SourceLang lang)
    {
        source_lang_ = lang;
    }

    void SetSuperClass(BaseClassItem *super_class)
    {
        super_class_ = super_class;
    }

    void AddInterface(BaseClassItem *iface)
    {
        AddIndexDependency(iface);
        ifaces_.push_back(iface);
    }

    void AddRuntimeAnnotation(AnnotationItem *runtime_annotation)
    {
        runtime_annotations_.push_back(runtime_annotation);
    }

    void AddAnnotation(AnnotationItem *annotation)
    {
        annotations_.push_back(annotation);
    }

    void AddRuntimeTypeAnnotation(AnnotationItem *runtime_type_annotation)
    {
        runtime_type_annotations_.push_back(runtime_type_annotation);
    }

    void AddTypeAnnotation(AnnotationItem *type_annotation)
    {
        type_annotations_.push_back(type_annotation);
    }

    template <class... Args>
    FieldItem *AddField(Args... args)
    {
        fields_.emplace_back(std::make_unique<FieldItem>(this, std::forward<Args>(args)...));
        return fields_.back().get();
    }

    template <class... Args>
    MethodItem *AddMethod(Args... args)
    {
        methods_.emplace_back(std::make_unique<MethodItem>(this, std::forward<Args>(args)...));
        return methods_.back().get();
    }

    void SetSourceFile(StringItem *item)
    {
        source_file_ = item;
    }

    size_t CalculateSizeWithoutFieldsAndMethods() const;

    size_t CalculateSize() const override;

    void ComputeLayout() override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "class_item";
    }

    void VisitFields(const VisitorCallBack &cb)
    {
        for (auto &field : fields_) {
            cb(field.get());
        }
    }

    void VisitMethods(const VisitorCallBack &cb)
    {
        for (auto &method : methods_) {
            cb(method.get());
        }
    }

    void Visit(const VisitorCallBack &cb) override
    {
        VisitFields(cb);
        VisitMethods(cb);
    }

    std::vector<AnnotationItem *> *GetRuntimeAnnotations()
    {
        return &runtime_annotations_;
    }

    std::vector<AnnotationItem *> *GetAnnotations()
    {
        return &annotations_;
    }

    std::vector<AnnotationItem *> *GetTypeAnnotations()
    {
        return &type_annotations_;
    }

    std::vector<AnnotationItem *> *GetRuntimeTypeAnnotations()
    {
        return &runtime_type_annotations_;
    }

    DEFAULT_MOVE_SEMANTIC(ClassItem);
    DEFAULT_COPY_SEMANTIC(ClassItem);

private:
    bool WriteIfaces(Writer *writer);

    bool WriteAnnotations(Writer *writer);

    bool WriteTaggedData(Writer *writer);

    BaseClassItem *super_class_;
    uint32_t access_flags_;
    SourceLang source_lang_;
    std::vector<BaseClassItem *> ifaces_;
    std::vector<AnnotationItem *> runtime_annotations_;
    std::vector<AnnotationItem *> annotations_;
    std::vector<AnnotationItem *> type_annotations_;
    std::vector<AnnotationItem *> runtime_type_annotations_;
    StringItem *source_file_;
    std::vector<std::unique_ptr<FieldItem>> fields_;
    std::vector<std::unique_ptr<MethodItem>> methods_;
};

class ForeignClassItem : public BaseClassItem {
public:
    explicit ForeignClassItem(const std::string &name) : BaseClassItem(name) {}

    ~ForeignClassItem() override = default;

    bool IsForeign() const override
    {
        return true;
    }

    std::string GetName() const override
    {
        return "foreign_class_item";
    }

    DEFAULT_MOVE_SEMANTIC(ForeignClassItem);
    DEFAULT_COPY_SEMANTIC(ForeignClassItem);
};

class ForeignFieldItem : public BaseFieldItem {
public:
    ForeignFieldItem(BaseClassItem *cls, StringItem *name, TypeItem *type) : BaseFieldItem(cls, name, type) {}

    ~ForeignFieldItem() override = default;

    bool IsForeign() const override
    {
        return true;
    }

    std::string GetName() const override
    {
        return "foreign_field_item";
    }

    DEFAULT_MOVE_SEMANTIC(ForeignFieldItem);
    DEFAULT_COPY_SEMANTIC(ForeignFieldItem);
};

class ForeignMethodItem : public BaseMethodItem {
public:
    ForeignMethodItem(BaseClassItem *cls, StringItem *name, ProtoItem *proto, uint32_t access_flags)
        : BaseMethodItem(cls, name, proto, access_flags)
    {
    }

    ~ForeignMethodItem() override = default;

    bool IsForeign() const override
    {
        return true;
    }

    std::string GetName() const override
    {
        return "foreign_method_item";
    }

    DEFAULT_MOVE_SEMANTIC(ForeignMethodItem);
    DEFAULT_COPY_SEMANTIC(ForeignMethodItem);
};

class ProtoItem;

class ParamAnnotationsItem : public BaseItem {
public:
    ParamAnnotationsItem(MethodItem *method, bool is_runtime_annotations);

    ~ParamAnnotationsItem() override = default;

    std::string GetName() const override
    {
        return "param_annotations_item";
    }

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    DEFAULT_MOVE_SEMANTIC(ParamAnnotationsItem);
    DEFAULT_COPY_SEMANTIC(ParamAnnotationsItem);

private:
    std::vector<std::vector<AnnotationItem *>> annotations_;
};

class ProtoItem : public IndexedItem {
public:
    ProtoItem(TypeItem *ret_type, const std::vector<MethodParamItem> &params);

    ~ProtoItem() override = default;

    DEFAULT_MOVE_SEMANTIC(ProtoItem);
    DEFAULT_COPY_SEMANTIC(ProtoItem);

    size_t CalculateSize() const override
    {
        size_t size = shorty_.size() * sizeof(uint16_t);
        size += reference_types_.size() * IDX_SIZE;
        return size;
    }

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "proto_item";
    }

    IndexType GetIndexType() const override
    {
        return IndexType::PROTO;
    }

    size_t Alignment() override
    {
        return sizeof(uint16_t);
    }

private:
    static constexpr size_t SHORTY_ELEM_SIZE = 4;

    void AddType(TypeItem *type, size_t *n);

    std::vector<uint16_t> shorty_;
    std::vector<TypeItem *> reference_types_;
};

class CodeItem : public BaseItem {
public:
    class CatchBlock : public BaseItem {
    public:
        CatchBlock(MethodItem *method, BaseClassItem *type, size_t handler_pc, size_t code_size = 0)
            : method_(method), type_(type), handler_pc_(handler_pc), code_size_(code_size)
        {
        }

        ~CatchBlock() override = default;

        DEFAULT_MOVE_SEMANTIC(CatchBlock);
        DEFAULT_COPY_SEMANTIC(CatchBlock);

        size_t CalculateSize() const override;

        bool Write(Writer *writer) override;

        std::string GetName() const override
        {
            return "catch_block_item";
        }

    private:
        MethodItem *method_;
        BaseClassItem *type_;
        size_t handler_pc_;
        size_t code_size_;
    };

    class TryBlock : public BaseItem {
    public:
        TryBlock(size_t start_pc, size_t length, std::vector<CatchBlock> catch_blocks)
            : start_pc_(start_pc), length_(length), catch_blocks_(std::move(catch_blocks))
        {
        }

        ~TryBlock() override = default;

        DEFAULT_MOVE_SEMANTIC(TryBlock);
        DEFAULT_COPY_SEMANTIC(TryBlock);

        size_t CalculateSizeWithoutCatchBlocks() const;

        void ComputeLayout() override;

        size_t CalculateSize() const override;

        bool Write(Writer *writer) override;

        std::string GetName() const override
        {
            return "try_block_item";
        }

    private:
        size_t start_pc_;
        size_t length_;
        std::vector<CatchBlock> catch_blocks_;
    };

    CodeItem(size_t num_vregs, size_t num_args, std::vector<uint8_t> instructions)
        : num_vregs_(num_vregs), num_args_(num_args), instructions_(std::move(instructions))
    {
    }

    CodeItem() = default;

    ~CodeItem() override = default;

    void SetNumVregs(size_t num_vregs)
    {
        num_vregs_ = num_vregs;
    }

    void SetNumArgs(size_t num_args)
    {
        num_args_ = num_args;
    }

    std::vector<uint8_t> *GetInstructions()
    {
        return &instructions_;
    }

    void SetNumInstructions(size_t num_ins)
    {
        num_ins_ = num_ins;
    }

    size_t GetNumInstructions() const
    {
        return num_ins_;
    }

    void AddTryBlock(const TryBlock &try_block)
    {
        try_blocks_.push_back(try_block);
    }

    size_t CalculateSizeWithoutTryBlocks() const;

    void ComputeLayout() override;

    size_t CalculateSize() const override;

    size_t GetCodeSize() const;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "code_item";
    }

    DEFAULT_MOVE_SEMANTIC(CodeItem);
    DEFAULT_COPY_SEMANTIC(CodeItem);

private:
    size_t num_vregs_ {0};
    size_t num_args_ {0};
    size_t num_ins_ {0};
    std::vector<uint8_t> instructions_;
    std::vector<TryBlock> try_blocks_;
};

class ScalarValueItem;
class ArrayValueItem;

class ValueItem : public BaseItem {
public:
    enum class Type { INTEGER, LONG, FLOAT, DOUBLE, ID, ARRAY };

    explicit ValueItem(Type type) : type_(type) {}

    ~ValueItem() override = default;

    DEFAULT_MOVE_SEMANTIC(ValueItem);
    DEFAULT_COPY_SEMANTIC(ValueItem);

    Type GetType() const
    {
        return type_;
    }

    bool IsArray() const
    {
        return type_ == Type::ARRAY;
    }

    bool Is32bit() const
    {
        return type_ == Type::INTEGER || type_ == Type::FLOAT || type_ == Type::ID;
    }

    std::string GetName() const override
    {
        return "value_item";
    }

    const ScalarValueItem *GetAsScalar() const;

    const ArrayValueItem *GetAsArray() const;

private:
    Type type_;
};

class ScalarValueItem : public ValueItem {
public:
    explicit ScalarValueItem(uint32_t v) : ValueItem(Type::INTEGER), value_(v) {}

    explicit ScalarValueItem(uint64_t v) : ValueItem(Type::LONG), value_(v) {}

    explicit ScalarValueItem(float v) : ValueItem(Type::FLOAT), value_(v) {}

    explicit ScalarValueItem(double v) : ValueItem(Type::DOUBLE), value_(v) {}

    explicit ScalarValueItem(BaseItem *v) : ValueItem(Type::ID), value_(v) {}

    ~ScalarValueItem() override = default;

    DEFAULT_MOVE_SEMANTIC(ScalarValueItem);
    DEFAULT_COPY_SEMANTIC(ScalarValueItem);

    template <class T>
    T GetValue() const
    {
        return std::get<T>(value_);
    }

    File::EntityId GetId() const
    {
        return File::EntityId(GetValue<BaseItem *>()->GetOffset());
    }

    size_t GetULeb128EncodedSize();

    size_t GetSLeb128EncodedSize() const;

    size_t CalculateSize() const override;

    size_t Alignment() override;

    bool Write(Writer *writer) override;

    bool WriteAsUleb128(Writer *writer);

private:
    std::variant<uint32_t, uint64_t, float, double, BaseItem *> value_;
};

class ArrayValueItem : public ValueItem {
public:
    ArrayValueItem(panda_file::Type component_type, std::vector<ScalarValueItem> items)
        : ValueItem(Type::ARRAY), component_type_(component_type), items_(std::move(items))
    {
    }

    ~ArrayValueItem() override = default;

    DEFAULT_MOVE_SEMANTIC(ArrayValueItem);
    DEFAULT_COPY_SEMANTIC(ArrayValueItem);

    size_t CalculateSize() const override;

    void ComputeLayout() override;

    bool Write(Writer *writer) override;

private:
    size_t GetComponentSize() const;

    panda_file::Type component_type_;
    std::vector<ScalarValueItem> items_;
};

class LiteralItem;
class LiteralArrayItem;

class LiteralItem : public BaseItem {
public:
    enum class Type { B1, B2, B4, B8, STRING, METHOD };

    explicit LiteralItem(uint8_t v) : type_(Type::B1), value_(v) {}

    explicit LiteralItem(uint16_t v) : type_(Type::B2), value_(v) {}

    explicit LiteralItem(uint32_t v) : type_(Type::B4), value_(v) {}

    explicit LiteralItem(uint64_t v) : type_(Type::B8), value_(v) {}

    explicit LiteralItem(StringItem *v) : type_(Type::STRING), value_(v) {}

    explicit LiteralItem(MethodItem *v) : type_(Type::METHOD), value_(v) {}

    ~LiteralItem() override = default;

    DEFAULT_MOVE_SEMANTIC(LiteralItem);
    DEFAULT_COPY_SEMANTIC(LiteralItem);

    Type GetType() const
    {
        return type_;
    }

    std::string GetName() const override
    {
        return "literal_item";
    }

    template <class T>
    T GetValue() const
    {
        return std::get<T>(value_);
    }

    size_t CalculateSize() const override;

    size_t Alignment() override;

    File::EntityId GetId() const
    {
        return File::EntityId(GetValue<StringItem *>()->GetOffset());
    }

    File::EntityId GetMethodId() const
    {
        return File::EntityId(GetValue<MethodItem *>()->GetFileId());
    }

    bool Write(Writer *writer) override;

private:
    Type type_;
    std::variant<uint8_t, uint16_t, uint32_t, uint64_t, StringItem *, MethodItem *> value_;
};

class LiteralArrayItem : public ValueItem {
public:
    explicit LiteralArrayItem() : ValueItem(Type::ARRAY) {}

    ~LiteralArrayItem() override = default;

    DEFAULT_MOVE_SEMANTIC(LiteralArrayItem);
    DEFAULT_COPY_SEMANTIC(LiteralArrayItem);

    void AddItems(const std::vector<LiteralItem> &item);

    size_t CalculateSize() const override;

    void ComputeLayout() override;

    bool Write(Writer *writer) override;

private:
    std::vector<LiteralItem> items_;
};

class AnnotationItem : public BaseItem {
public:
    class Elem {
    public:
        Elem(StringItem *name, ValueItem *value) : name_(name), value_(value)
        {
            value_->SetNeedsEmit(!value_->Is32bit());
        }

        ~Elem() = default;

        DEFAULT_MOVE_SEMANTIC(Elem);
        DEFAULT_COPY_SEMANTIC(Elem);

        StringItem *GetName()
        {
            return name_;
        }

        ValueItem *GetValue()
        {
            return value_;
        }

        void SetValue(ValueItem *item)
        {
            value_ = item;
        }

    private:
        StringItem *name_;
        ValueItem *value_;
    };

    class Tag {
    public:
        explicit Tag(char item) : item_(item) {}

        ~Tag() = default;

        DEFAULT_MOVE_SEMANTIC(Tag);
        DEFAULT_COPY_SEMANTIC(Tag);

        uint8_t GetItem() const
        {
            return item_;
        }

    private:
        uint8_t item_;
    };

    AnnotationItem(BaseClassItem *cls, std::vector<Elem> elements, std::vector<Tag> tags)
        : class_(cls), elements_(std::move(elements)), tags_(std::move(tags))
    {
        AddIndexDependency(cls);
    }

    ~AnnotationItem() override = default;

    DEFAULT_MOVE_SEMANTIC(AnnotationItem);
    DEFAULT_COPY_SEMANTIC(AnnotationItem);

    size_t CalculateSize() const override;

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "annotation_item";
    }

    std::vector<Elem> *GetElements()
    {
        return &elements_;
    }

    const std::vector<Tag> &GetTags() const
    {
        return tags_;
    }

private:
    BaseClassItem *class_;
    std::vector<Elem> elements_;
    std::vector<Tag> tags_;
};

enum class MethodHandleType : uint8_t {
    PUT_STATIC = 0x00,
    GET_STATIC = 0x01,
    PUT_INSTANCE = 0x02,
    GET_INSTANCE = 0x03,
    INVOKE_STATIC = 0x04,
    INVOKE_INSTANCE = 0x05,
    INVOKE_CONSTRUCTOR = 0x06,
    INVOKE_DIRECT = 0x07,
    INVOKE_INTERFACE = 0x08
};

class MethodHandleItem : public BaseItem {
public:
    MethodHandleItem(MethodHandleType type, BaseItem *entity) : type_(type), entity_(entity) {}

    ~MethodHandleItem() override = default;

    DEFAULT_MOVE_SEMANTIC(MethodHandleItem);
    DEFAULT_COPY_SEMANTIC(MethodHandleItem);

    size_t CalculateSize() const override
    {
        return sizeof(uint8_t) + leb128::UnsignedEncodingSize(entity_->GetOffset());
    }

    bool Write(Writer *writer) override;

    std::string GetName() const override
    {
        return "method_handle_item";
    }

    MethodHandleType GetType() const
    {
        return type_;
    }

private:
    MethodHandleType type_;
    BaseItem *entity_;
};

enum class ArgumentType : uint8_t {
    INTEGER = 0x00,
    LONG = 0x01,
    FLOAT = 0x02,
    DOUBLE = 0x03,
    STRING = 0x04,
    CLASS = 0x05,
    METHOD_HANDLE = 0x06,
    METHOD_TYPE = 0x07
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_FILE_ITEMS_H_
