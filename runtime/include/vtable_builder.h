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

#ifndef PANDA_RUNTIME_INCLUDE_VTABLE_BUILDER_H_
#define PANDA_RUNTIME_INCLUDE_VTABLE_BUILDER_H_

#include "libpandabase/macros.h"
#include "libpandabase/utils/hash.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/file-inl.h"
#include "libpandafile/file_items.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/method.h"

namespace panda {

class ClassLinker;
class ClassLinkerContext;

class MethodInfo {
public:
    class Proto {
    public:
        Proto(const panda_file::File &pf, panda_file::File::EntityId proto_id) : pda_(pf, proto_id) {}
        ~Proto() = default;
        DEFAULT_COPY_CTOR(Proto)
        DEFAULT_MOVE_CTOR(Proto)
        NO_COPY_OPERATOR(Proto);
        NO_MOVE_OPERATOR(Proto);

        bool IsEqualBySignatureAndReturnType(const Proto &other) const;

        panda_file::ProtoDataAccessor &GetProtoDataAccessor()
        {
            return pda_;
        }

    private:
        bool AreTypesEqual(const Proto &other, panda_file::Type t1, panda_file::Type t2, size_t ref_idx) const;

        mutable panda_file::ProtoDataAccessor pda_;
    };

    MethodInfo(const panda_file::File &pf, panda_file::File::EntityId method_id, size_t index, ClassLinkerContext *ctx)
        : mda_(pf, method_id), proto_(pf, mda_.GetProtoId()), ctx_(ctx), index_(index)
    {
    }

    explicit MethodInfo(Method *method, size_t index = 0, bool is_base = false, bool needs_copy = false)
        : mda_(*method->GetPandaFile(), method->GetFileId()),
          proto_(*method->GetPandaFile(), mda_.GetProtoId()),
          method_(method),
          ctx_(method->GetClass()->GetLoadContext()),
          index_(index),
          needs_copy_(needs_copy),
          is_base_(is_base)
    {
    }
    ~MethodInfo() = default;
    DEFAULT_COPY_CTOR(MethodInfo)
    DEFAULT_MOVE_CTOR(MethodInfo)
    NO_COPY_OPERATOR(MethodInfo);
    NO_MOVE_OPERATOR(MethodInfo);

    bool IsEqualByNameAndSignature(const MethodInfo &other) const
    {
        return GetName() == other.GetName() && proto_.IsEqualBySignatureAndReturnType(other.proto_);
    }

    panda_file::File::StringData GetName() const
    {
        return mda_.GetPandaFile().GetStringData(mda_.GetNameId());
    }

    panda_file::File::StringData GetClassName() const
    {
        return mda_.GetPandaFile().GetStringData(mda_.GetClassId());
    }

    panda_file::MethodDataAccessor &GetMethodDataAccessor()
    {
        return mda_;
    }

    Proto &GetProto()
    {
        return proto_;
    }

    Method *GetMethod() const
    {
        return method_;
    }

    size_t GetIndex() const
    {
        return index_;
    }

    bool IsAbstract() const
    {
        return (mda_.GetAccessFlags() & ACC_ABSTRACT) != 0;
    }

    bool IsPublic() const
    {
        return (mda_.GetAccessFlags() & ACC_PUBLIC) != 0;
    }

    bool IsProtected() const
    {
        return (mda_.GetAccessFlags() & ACC_PROTECTED) != 0;
    }

    bool IsPrivate() const
    {
        return (mda_.GetAccessFlags() & ACC_PRIVATE) != 0;
    }

    bool IsInterfaceMethod() const
    {
        if (method_ != nullptr) {
            return method_->GetClass()->IsInterface();
        }

        panda_file::ClassDataAccessor cda(mda_.GetPandaFile(), mda_.GetClassId());
        return cda.IsInterface();
    }

    bool NeedsCopy() const
    {
        return needs_copy_;
    }

    bool IsBase() const
    {
        return is_base_;
    }

    ClassLinkerContext *GetLoadContext() const
    {
        return ctx_;
    }

    bool IsCopied() const
    {
        if (method_ == nullptr) {
            return false;
        }

        return method_->IsDefaultInterfaceMethod();
    }

private:
    mutable panda_file::MethodDataAccessor mda_;
    Proto proto_;
    Method *method_ {nullptr};
    ClassLinkerContext *ctx_ {nullptr};
    size_t index_ {0};
    bool needs_copy_ {false};
    bool is_base_ {false};
};

template <class SearchPred, class OverridePred>
class VTable {
public:
    void AddBaseMethod(const MethodInfo &info)
    {
        methods_.insert({info, methods_.size()});
    }

    bool AddMethod(const MethodInfo &info)
    {
        auto [beg_it, end_it] = methods_.equal_range(info);
        if (beg_it == methods_.cend()) {
            methods_.insert({info, methods_.size()});
            return true;
        }

        for (auto it = beg_it; it != end_it; ++it) {
            if (OverridePred()(it->first, info)) {
                size_t idx = it->second;
                methods_.erase(it);
                methods_.insert({info, idx});
                return true;
            }
        }

        return false;
    }

    void UpdateClass(Class *klass) const
    {
        auto vtable = klass->GetVTable();

        for (const auto [method_info, idx] : methods_) {
            Method *method = method_info.GetMethod();
            if (method == nullptr) {
                method = &klass->GetVirtualMethods()[method_info.GetIndex()];
                method->SetVTableIndex(idx);
            } else if (method_info.NeedsCopy()) {
                method = &klass->GetCopiedMethods()[method_info.GetIndex()];
                method->SetVTableIndex(idx);
            } else if (!method_info.IsBase()) {
                method->SetVTableIndex(idx);
            }

            vtable[idx] = method;
        }

        DumpVTable(klass);
    }

    static void DumpVTable([[maybe_unused]] Class *klass)
    {
#ifndef NDEBUG
        LOG(DEBUG, CLASS_LINKER) << "vtable of class " << klass->GetName() << ":";
        auto vtable = klass->GetVTable();
        size_t idx = 0;
        for (auto *method : vtable) {
            LOG(DEBUG, CLASS_LINKER) << "[" << idx++ << "] " << method->GetFullName();
        }
#endif  // NDEBUG
    }

    size_t Size() const
    {
        return methods_.size();
    }

private:
    struct HashByName {
        uint32_t operator()(const MethodInfo &method_info) const
        {
            return GetHash32String(method_info.GetName().data);
        }
    };

    PandaUnorderedMultiMap<MethodInfo, size_t, HashByName, SearchPred> methods_;
};

class VTableBuilder {
public:
    VTableBuilder() = default;

    virtual void Build(panda_file::ClassDataAccessor *cda, Class *base_class, ITable itable,
                       ClassLinkerContext *ctx) = 0;

    virtual void Build(Span<Method> methods, Class *base_class, ITable itable, bool is_interface) = 0;

    virtual void UpdateClass(Class *klass) const = 0;

    virtual size_t GetNumVirtualMethods() const = 0;

    virtual size_t GetVTableSize() const = 0;

    virtual const PandaVector<Method *> &GetCopiedMethods() const = 0;

    virtual ~VTableBuilder() = default;

    NO_COPY_SEMANTIC(VTableBuilder);
    NO_MOVE_SEMANTIC(VTableBuilder);
};

template <class SearchBySignature, class OverridePred>
class VTableBuilderImpl : public VTableBuilder {
    void Build(panda_file::ClassDataAccessor *cda, Class *base_class, ITable itable, ClassLinkerContext *ctx) override;

    void Build(Span<Method> methods, Class *base_class, ITable itable, bool is_interface) override;

    void UpdateClass(Class *klass) const override;

    size_t GetNumVirtualMethods() const override
    {
        return num_vmethods_;
    }

    size_t GetVTableSize() const override
    {
        return vtable_.Size();
    }

    const PandaVector<Method *> &GetCopiedMethods() const override
    {
        return copied_methods_;
    }

private:
    void BuildForInterface(panda_file::ClassDataAccessor *cda);

    void BuildForInterface(Span<Method> methods);

    void AddBaseMethods(Class *base_class);

    void AddClassMethods(panda_file::ClassDataAccessor *cda, ClassLinkerContext *ctx);

    void AddClassMethods(Span<Method> methods);

    void AddDefaultInterfaceMethods(ITable itable);

    VTable<SearchBySignature, OverridePred> vtable_;
    size_t num_vmethods_ {0};
    bool has_default_methods_ {false};
    PandaVector<Method *> copied_methods_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_VTABLE_BUILDER_H_
