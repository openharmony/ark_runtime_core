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

#include "runtime/include/class_linker.h"

#include "os/filesystem.h"
#include "runtime/bridge/bridge.h"
#include "runtime/class_initializer.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/field.h"
#include "runtime/include/itable_builder.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "libpandabase/macros.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/span.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/field_data_accessor-inl.h"
#include "libpandafile/method_data_accessor-inl.h"
#include "libpandafile/modifiers.h"
#include "libpandafile/panda_cache.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "runtime/include/tooling/debug_inf.h"
#include "trace/trace.h"

namespace panda {

using Type = panda_file::Type;
using SourceLang = panda_file::SourceLang;

void ClassLinker::AddPandaFile(std::unique_ptr<const panda_file::File> &&pf, ClassLinkerContext *context)
{
    ASSERT(pf != nullptr);

    const panda_file::File *file = pf.get();

    SCOPED_TRACE_STREAM << __FUNCTION__ << " " << file->GetFilename();

    {
        os::memory::LockHolder lock {panda_files_lock_};
        panda_files_.push_back({context, std::forward<std::unique_ptr<const panda_file::File>>(pf)});
    }

    if (context == nullptr || context->IsBootContext()) {
        boot_panda_files_.push_back(file);
    }

    if (Runtime::GetCurrent()->IsInitialized()) {
        // LoadModule for initial boot files is called in runtime
        Runtime::GetCurrent()->GetNotificationManager()->LoadModuleEvent(file->GetFilename());
    }

    tooling::DebugInf::AddCodeMetaInfo(file);
}

void ClassLinker::FreeClassData(Class *class_ptr)
{
    Span<Field> fields = class_ptr->GetFields();
    if (fields.Size() > 0) {
        allocator_->Free(fields.begin());
    }
    Span<Method> methods = class_ptr->GetMethods();
    size_t n = methods.Size() + class_ptr->GetNumCopiedMethods();
    if (n > 0) {
        mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
        for (auto &method : methods) {
            // We create Profiling data in method class via InternalAllocator.
            // Therefore, we should delete it via InternalAllocator too.
            allocator->Free(method.GetProfilingData());
        }
        allocator_->Free(methods.begin());
    }
    bool has_own_itable = !class_ptr->IsArrayClass();
    auto itable = class_ptr->GetITable().Get();
    if (has_own_itable && !itable.Empty()) {
        for (size_t i = 0; i < itable.Size(); i++) {
            Span<Method *> imethods = itable[i].GetMethods();
            if (!imethods.Empty()) {
                allocator_->Free(imethods.begin());
            }
        }
        allocator_->Free(itable.begin());
    }
    Span<Class *> interfaces = class_ptr->GetInterfaces();
    if (!interfaces.Empty()) {
        allocator_->Free(interfaces.begin());
    }
}

void ClassLinker::FreeClass(Class *class_ptr)
{
    FreeClassData(class_ptr);
    GetExtension(class_ptr->GetSourceLang())->FreeClass(class_ptr);
}

ClassLinker::~ClassLinker()
{
    for (auto &copied_name : copied_names_) {
        allocator_->Free(reinterpret_cast<void *>(const_cast<uint8_t *>(copied_name)));
    }
}

ClassLinker::ClassLinker(mem::InternalAllocatorPtr allocator,
                         std::vector<std::unique_ptr<ClassLinkerExtension>> &&extensions)
    : allocator_(allocator), copied_names_(allocator->Adapter())
{
    for (auto &ext : extensions) {
        extensions_[ToExtensionIndex(ext->GetLanguage())] = std::move(ext);
    }
}

template <class T, class... Args>
static T *InitializeMemory(T *mem, Args... args)
{
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return new (mem) T(std::forward<Args>(args)...);
}

bool ClassLinker::Initialize(bool compressed_string_enabled)
{
    if (is_initialized_) {
        return true;
    }

    for (auto &ext : extensions_) {
        if (ext == nullptr) {
            continue;
        }

        if (!ext->Initialize(this, compressed_string_enabled)) {
            return false;
        }
    }

    is_initialized_ = true;

    return true;
}

bool ClassLinker::InitializeRoots(ManagedThread *thread)
{
    for (auto &ext : extensions_) {
        if (ext == nullptr) {
            continue;
        }

        if (!ext->InitializeRoots(thread)) {
            return false;
        }
    }

    return true;
}

using ClassEntry = std::pair<panda_file::File::EntityId, const panda_file::File *>;
using PandaFiles = PandaVector<const panda_file::File *>;

static ClassEntry FindClassInPandaFiles(const uint8_t *descriptor, const PandaFiles &panda_files)
{
    for (auto *pf : panda_files) {
        auto class_id = pf->GetClassId(descriptor);
        if (class_id.IsValid() && !pf->IsExternal(class_id)) {
            return {class_id, pf};
        }
    }

    return {};
}

Class *ClassLinker::FindLoadedClass(const uint8_t *descriptor, ClassLinkerContext *context)
{
    ASSERT(context != nullptr);
    return context->FindClass(descriptor);
}

template <class ClassDataAccessorT>
static size_t GetClassSize(ClassDataAccessorT data_accessor, size_t vtable_size, size_t imt_size,
                           size_t *out_num_sfields)
{
    size_t num_8bit_sfields = 0;
    size_t num_16bit_sfields = 0;
    size_t num_32bit_sfields = 0;
    size_t num_64bit_sfields = 0;
    size_t num_ref_sfields = 0;
    size_t num_tagged_sfields = 0;
    size_t num_sfields = 0;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    data_accessor.template EnumerateStaticFieldTypes([&num_8bit_sfields, &num_16bit_sfields, &num_32bit_sfields,
                                                      &num_64bit_sfields, &num_ref_sfields, &num_tagged_sfields,
                                                      &num_sfields](Type field_type) {
        ++num_sfields;

        switch (field_type.GetId()) {
            case Type::TypeId::U1:
            case Type::TypeId::I8:
            case Type::TypeId::U8:
                ++num_8bit_sfields;
                break;
            case Type::TypeId::I16:
            case Type::TypeId::U16:
                ++num_16bit_sfields;
                break;
            case Type::TypeId::I32:
            case Type::TypeId::U32:
            case Type::TypeId::F32:
                ++num_32bit_sfields;
                break;
            case Type::TypeId::I64:
            case Type::TypeId::U64:
            case Type::TypeId::F64:
                ++num_64bit_sfields;
                break;
            case Type::TypeId::REFERENCE:
                ++num_ref_sfields;
                break;
            case Type::TypeId::TAGGED:
                ++num_tagged_sfields;
                break;
            default:
                UNREACHABLE();
                break;
        }
    });

    *out_num_sfields = num_sfields;

    return ClassHelper::ComputeClassSize(vtable_size, imt_size, num_8bit_sfields, num_16bit_sfields, num_32bit_sfields,
                                         num_64bit_sfields, num_ref_sfields, num_tagged_sfields);
}

class ClassDataAccessorWrapper {
public:
    explicit ClassDataAccessorWrapper(panda_file::ClassDataAccessor *data_accessor = nullptr)
        : data_accessor_(data_accessor)
    {
    }

    template <class Callback>
    void EnumerateStaticFieldTypes(const Callback &cb) const
    {
        data_accessor_->EnumerateFields([cb](panda_file::FieldDataAccessor &fda) {
            if (!fda.IsStatic()) {
                return;
            }

            cb(Type::GetTypeFromFieldEncoding(fda.GetType()));
        });
    }

    ~ClassDataAccessorWrapper() = default;

    DEFAULT_COPY_SEMANTIC(ClassDataAccessorWrapper);
    DEFAULT_MOVE_SEMANTIC(ClassDataAccessorWrapper);

private:
    panda_file::ClassDataAccessor *data_accessor_;
};

ClassLinker::ClassInfo ClassLinker::GetClassInfo(panda_file::ClassDataAccessor *data_accessor, Class *base,
                                                 Span<Class *> interfaces, ClassLinkerContext *context)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(data_accessor);

    auto vtable_builder = ctx.CreateVTableBuilder();
    auto itable_builder = ctx.CreateITableBuilder();
    auto imtable_builder = ctx.CreateIMTableBuilder();

    itable_builder->Build(this, base, interfaces, data_accessor->IsInterface());
    vtable_builder->Build(data_accessor, base, itable_builder->GetITable(), context);
    imtable_builder->Build(data_accessor, itable_builder->GetITable());

    ClassDataAccessorWrapper data_accessor_wrapper(data_accessor);
    size_t num_sfields = 0;
    size_t size = GetClassSize(data_accessor_wrapper, vtable_builder->GetVTableSize(), imtable_builder->GetIMTSize(),
                               &num_sfields);

    return {size, num_sfields, std::move(vtable_builder), std::move(itable_builder), std::move(imtable_builder)};
}

class ClassDataAccessor {
public:
    explicit ClassDataAccessor(Span<Field> fields) : fields_(fields) {}

    template <class Callback>
    void EnumerateStaticFieldTypes(const Callback &cb) const
    {
        for (const auto &field : fields_) {
            if (!field.IsStatic()) {
                continue;
            }

            cb(field.GetType());
        }
    }

    ~ClassDataAccessor() = default;

    DEFAULT_COPY_SEMANTIC(ClassDataAccessor);
    DEFAULT_MOVE_SEMANTIC(ClassDataAccessor);

private:
    Span<Field> fields_;
};

ClassLinker::ClassInfo ClassLinker::GetClassInfo(Span<Method> methods, Span<Field> fields, Class *base,
                                                 Span<Class *> interfaces, bool is_interface)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*base);

    auto vtable_builder = ctx.CreateVTableBuilder();
    auto itable_builder = ctx.CreateITableBuilder();
    auto imtable_builder = ctx.CreateIMTableBuilder();

    itable_builder->Build(this, base, interfaces, is_interface);
    vtable_builder->Build(methods, base, itable_builder->GetITable(), is_interface);
    imtable_builder->Build(itable_builder->GetITable(), is_interface);

    ClassDataAccessor data_accessor(fields);
    size_t num_sfields = 0;
    size_t size =
        GetClassSize(data_accessor, vtable_builder->GetVTableSize(), imtable_builder->GetIMTSize(), &num_sfields);

    return {size, num_sfields, std::move(vtable_builder), std::move(itable_builder), std::move(imtable_builder)};
}

static void LoadMethod(Method *method, panda_file::MethodDataAccessor *method_data_accessor, Class *klass,
                       LanguageContext ctx, const ClassLinkerExtension *ext)
{
    const auto &pf = method_data_accessor->GetPandaFile();
    panda_file::ProtoDataAccessor pda(pf, method_data_accessor->GetProtoId());

    uint32_t access_flags = method_data_accessor->GetAccessFlags();

    auto *method_name = pf.GetStringData(method_data_accessor->GetNameId()).data;
    if (utf::IsEqual(method_name, ctx.GetCtorName()) || utf::IsEqual(method_name, ctx.GetCctorName())) {
        access_flags |= ACC_CONSTRUCTOR;
    }

    auto code_id = method_data_accessor->GetCodeId();
    size_t num_args = method_data_accessor->IsStatic() ? pda.GetNumArgs() : pda.GetNumArgs() + 1;

    if (!code_id.has_value()) {
        InitializeMemory(method, klass, &pf, method_data_accessor->GetMethodId(), panda_file::File::EntityId(0),
                         access_flags, num_args, reinterpret_cast<const uint16_t *>(pda.GetShorty().Data()));

        if (method_data_accessor->IsNative()) {
            method->SetCompiledEntryPoint(ext->GetNativeEntryPointFor(method));
        } else {
            method->SetInterpreterEntryPoint();
        }
    } else {
        InitializeMemory(method, klass, &pf, method_data_accessor->GetMethodId(), code_id.value(), access_flags,
                         num_args, reinterpret_cast<const uint16_t *>(pda.GetShorty().Data()));
        method->SetCompiledEntryPoint(GetCompiledCodeToInterpreterBridge(method));
    }
}

bool ClassLinker::LoadMethods(Class *klass, ClassInfo *class_info, panda_file::ClassDataAccessor *data_accessor,
                              [[maybe_unused]] ClassLinkerErrorHandler *error_handler)
{
    uint32_t num_methods = data_accessor->GetMethodsNumber();

    uint32_t num_vmethods = klass->GetNumVirtualMethods();
    uint32_t num_smethods = num_methods - num_vmethods;

    auto &copied_methods = class_info->vtable_builder->GetCopiedMethods();
    uint32_t n = num_methods + copied_methods.size();
    if (n == 0) {
        return true;
    }

    Span<Method> methods {allocator_->AllocArray<Method>(n), n};

    size_t smethod_idx = num_vmethods;
    size_t vmethod_idx = 0;

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    auto *ext = GetExtension(ctx);
    ASSERT(ext != nullptr);

    size_t method_index = 0;
    data_accessor->EnumerateMethods([klass, &smethod_idx, &vmethod_idx, &methods, ctx, ext,
                                     &method_index](panda_file::MethodDataAccessor &method_data_accessor) {
        Method *method = method_data_accessor.IsStatic() ? &methods[smethod_idx++] : &methods[vmethod_idx++];
        LoadMethod(method, &method_data_accessor, klass, ctx, ext);

        method_index++;
    });

    for (size_t i = 0; i < copied_methods.size(); i++) {
        size_t idx = num_methods + i;
        InitializeMemory(&methods[idx], copied_methods[i]);
        methods[idx].SetIsDefaultInterfaceMethod();
    }

    klass->SetMethods(methods, num_vmethods, num_smethods);

    return true;
}

bool ClassLinker::LoadFields(Class *klass, panda_file::ClassDataAccessor *data_accessor,
                             [[maybe_unused]] ClassLinkerErrorHandler *error_handler)
{
    uint32_t num_fields = data_accessor->GetFieldsNumber();
    if (num_fields == 0) {
        return true;
    }

    uint32_t num_sfields = klass->GetNumStaticFields();

    Span<Field> fields {allocator_->AllocArray<Field>(num_fields), num_fields};

    size_t sfields_idx = 0;
    size_t ifields_idx = num_sfields;
    data_accessor->EnumerateFields(
        [klass, &sfields_idx, &ifields_idx, &fields](panda_file::FieldDataAccessor &field_data_accessor) {
            Field *field = field_data_accessor.IsStatic() ? &fields[sfields_idx++] : &fields[ifields_idx++];
            InitializeMemory(field, klass, &field_data_accessor.GetPandaFile(), field_data_accessor.GetFieldId(),
                             field_data_accessor.GetAccessFlags(),
                             panda_file::Type::GetTypeFromFieldEncoding(field_data_accessor.GetType()));
        });

    klass->SetFields(fields, num_sfields);

    return true;
}

static void LayoutFieldsWithoutAlignment(size_t size, size_t *offset, size_t *space, PandaList<Field *> *fields)
{
    while ((space == nullptr || *space >= size) && !fields->empty()) {
        Field *field = fields->front();
        field->SetOffset(*offset);
        *offset += size;
        if (space != nullptr) {
            *space -= size;
        }
        fields->pop_front();
    }
}

static uint32_t LayoutReferenceFields(size_t size, size_t *offset, const PandaList<Field *> &fields)
{
    uint32_t volatile_fields_num = 0;
    // layout volatile fields firstly
    for (auto *field : fields) {
        if (field->IsVolatile()) {
            volatile_fields_num++;
            field->SetOffset(*offset);
            *offset += size;
        }
    }
    for (auto *field : fields) {
        if (!field->IsVolatile()) {
            field->SetOffset(*offset);
            *offset += size;
        }
    }
    return volatile_fields_num;
}

static size_t LayoutFields(Class *klass, PandaList<Field *> *tagged_fields, PandaList<Field *> *fields64,
                           PandaList<Field *> *fields32, PandaList<Field *> *fields16, PandaList<Field *> *fields8,
                           PandaList<Field *> *ref_fields, bool is_static)
{
    constexpr size_t SIZE_64 = sizeof(uint64_t);
    constexpr size_t SIZE_32 = sizeof(uint32_t);
    constexpr size_t SIZE_16 = sizeof(uint16_t);
    constexpr size_t SIZE_8 = sizeof(uint8_t);

    size_t offset;

    if (is_static) {
        offset = klass->GetStaticFieldsOffset();
    } else {
        offset = (klass->GetBase() != nullptr) ? klass->GetBase()->GetObjectSize()
                                               : static_cast<size_t>(ObjectHeader::ObjectHeaderSize());
    }

    if (!ref_fields->empty()) {
        offset = AlignUp(offset, ClassHelper::OBJECT_POINTER_SIZE);
        klass->SetRefFieldsNum(ref_fields->size(), is_static);
        klass->SetRefFieldsOffset(offset, is_static);
        auto volatile_num = LayoutReferenceFields(ClassHelper::OBJECT_POINTER_SIZE, &offset, *ref_fields);
        klass->SetVolatileRefFieldsNum(volatile_num, is_static);
    }

    static_assert(coretypes::TaggedValue::TaggedTypeSize() == SIZE_64,
                  "Please fix alignment of the fields of type \"TaggedValue\"");
    if (!IsAligned<SIZE_64>(offset) && (!fields64->empty() || !tagged_fields->empty())) {
        size_t padding = AlignUp(offset, SIZE_64) - offset;

        LayoutFieldsWithoutAlignment(SIZE_32, &offset, &padding, fields32);
        LayoutFieldsWithoutAlignment(SIZE_16, &offset, &padding, fields16);
        LayoutFieldsWithoutAlignment(SIZE_8, &offset, &padding, fields8);

        offset += padding;
    }

    LayoutFieldsWithoutAlignment(coretypes::TaggedValue::TaggedTypeSize(), &offset, nullptr, tagged_fields);
    LayoutFieldsWithoutAlignment(SIZE_64, &offset, nullptr, fields64);

    if (!IsAligned<SIZE_32>(offset) && !fields32->empty()) {
        size_t padding = AlignUp(offset, SIZE_32) - offset;

        LayoutFieldsWithoutAlignment(SIZE_16, &offset, &padding, fields16);
        LayoutFieldsWithoutAlignment(SIZE_8, &offset, &padding, fields8);

        offset += padding;
    }

    LayoutFieldsWithoutAlignment(SIZE_32, &offset, nullptr, fields32);

    if (!IsAligned<SIZE_16>(offset) && !fields16->empty()) {
        size_t padding = AlignUp(offset, SIZE_16) - offset;

        LayoutFieldsWithoutAlignment(SIZE_8, &offset, &padding, fields8);

        offset += padding;
    }

    LayoutFieldsWithoutAlignment(SIZE_16, &offset, nullptr, fields16);

    LayoutFieldsWithoutAlignment(SIZE_8, &offset, nullptr, fields8);

    return offset;
}

/* static */
bool ClassLinker::LayoutFields(Class *klass, Span<Field> fields, bool is_static,
                               [[maybe_unused]] ClassLinkerErrorHandler *error_handler)
{
    PandaList<Field *> tagged_fields;
    PandaList<Field *> fields64;
    PandaList<Field *> fields32;
    PandaList<Field *> fields16;
    PandaList<Field *> fields8;
    PandaList<Field *> ref_fields;

    for (auto &field : fields) {
        auto type = field.GetType();

        if (!type.IsPrimitive()) {
            ref_fields.push_back(&field);
            continue;
        }

        switch (type.GetId()) {
            case Type::TypeId::U1:
            case Type::TypeId::I8:
            case Type::TypeId::U8:
                fields8.push_back(&field);
                break;
            case Type::TypeId::I16:
            case Type::TypeId::U16:
                fields16.push_back(&field);
                break;
            case Type::TypeId::I32:
            case Type::TypeId::U32:
            case Type::TypeId::F32:
                fields32.push_back(&field);
                break;
            case Type::TypeId::I64:
            case Type::TypeId::U64:
            case Type::TypeId::F64:
                fields64.push_back(&field);
                break;
            case Type::TypeId::TAGGED:
                tagged_fields.push_back(&field);
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    size_t size =
        panda::LayoutFields(klass, &tagged_fields, &fields64, &fields32, &fields16, &fields8, &ref_fields, is_static);

    if (!is_static && !klass->IsVariableSize()) {
        klass->SetObjectSize(size);
    }

    return true;
}

bool ClassLinker::LinkMethods(Class *klass, ClassInfo *class_info,
                              [[maybe_unused]] ClassLinkerErrorHandler *error_handler)
{
    class_info->vtable_builder->UpdateClass(klass);
    class_info->itable_builder->Resolve(klass);
    class_info->itable_builder->UpdateClass(klass);
    class_info->imtable_builder->UpdateClass(klass);

    return true;
}

bool ClassLinker::LinkFields(Class *klass, ClassLinkerErrorHandler *error_handler)
{
    if (!LayoutFields(klass, klass->GetStaticFields(), true, error_handler)) {
        LOG(ERROR, CLASS_LINKER) << "Cannot layout static fields of class '" << klass->GetName() << "'";
        return false;
    }

    if (!LayoutFields(klass, klass->GetInstanceFields(), false, error_handler)) {
        LOG(ERROR, CLASS_LINKER) << "Cannot layout instance fields of class '" << klass->GetName() << "'";
        return false;
    }

    return true;
}

Class *ClassLinker::LoadBaseClass(panda_file::ClassDataAccessor *cda, LanguageContext ctx, ClassLinkerContext *context,
                                  ClassLinkerErrorHandler *error_handler)
{
    auto base_class_id = cda->GetSuperClassId();
    auto *ext = GetExtension(ctx);
    ASSERT(ext != nullptr);
    if (base_class_id.GetOffset() == 0) {
        return ext->GetClassRoot(ClassRoot::OBJECT);
    }

    auto &pf = cda->GetPandaFile();
    auto *base_class = ext->GetClass(pf, base_class_id, context, error_handler);
    if (base_class == nullptr) {
        LOG(INFO, CLASS_LINKER) << "Cannot find base class '"
                                << utf::Mutf8AsCString(pf.GetStringData(base_class_id).data) << "' of class '"
                                << utf::Mutf8AsCString(pf.GetStringData(cda->GetClassId()).data) << "' in ctx "
                                << context;
        return nullptr;
    }

    return base_class;
}

std::optional<Span<Class *>> ClassLinker::LoadInterfaces(panda_file::ClassDataAccessor *cda,
                                                         ClassLinkerContext *context,
                                                         ClassLinkerErrorHandler *error_handler)
{
    ASSERT(context != nullptr);
    size_t ifaces_num = cda->GetIfacesNumber();

    if (ifaces_num == 0) {
        return Span<Class *>(nullptr, ifaces_num);
    }

    Span<Class *> ifaces {allocator_->AllocArray<Class *>(ifaces_num), ifaces_num};

    for (size_t i = 0; i < ifaces_num; i++) {
        auto id = cda->GetInterfaceId(i);
        auto &pf = cda->GetPandaFile();
        auto *iface = GetClass(pf, id, context, error_handler);
        if (iface == nullptr) {
            LOG(INFO, CLASS_LINKER) << "Cannot find interface '" << utf::Mutf8AsCString(pf.GetStringData(id).data)
                                    << "' of class '" << utf::Mutf8AsCString(pf.GetStringData(cda->GetClassId()).data)
                                    << "' in ctx " << context;
            ASSERT(!ifaces.Empty());
            allocator_->Free(ifaces.begin());
            return {};
        }

        ifaces[i] = iface;
    }

    return ifaces;
}

// This class is required to clear static unordered_set on return
class ClassScopeStaticSetAutoCleaner {
public:
    ClassScopeStaticSetAutoCleaner() = default;
    explicit ClassScopeStaticSetAutoCleaner(std::unordered_set<uint64_t> *set_ptr)
    {
        set_ptr_ = set_ptr;
    }
    ~ClassScopeStaticSetAutoCleaner()
    {
        set_ptr_->clear();
    }

    NO_COPY_SEMANTIC(ClassScopeStaticSetAutoCleaner);
    NO_MOVE_SEMANTIC(ClassScopeStaticSetAutoCleaner);

private:
    std::unordered_set<uint64_t> *set_ptr_;
};

static uint64_t GetClassUniqueHash(uint32_t panda_file_hash, uint32_t class_id)
{
    const uint8_t BITS_TO_SHUFFLE = 32;
    return (static_cast<uint64_t>(panda_file_hash) << BITS_TO_SHUFFLE) | static_cast<uint64_t>(class_id);
}

Class *ClassLinker::LoadClass(panda_file::ClassDataAccessor *class_data_accessor, const uint8_t *descriptor,
                              Class *base_class, Span<Class *> interfaces, ClassLinkerContext *context,
                              ClassLinkerExtension *ext, ClassLinkerErrorHandler *error_handler)
{
    ASSERT(context != nullptr);
    ClassInfo class_info = GetClassInfo(class_data_accessor, base_class, interfaces, context);

    auto *klass = ext->CreateClass(descriptor, class_info.vtable_builder->GetVTableSize(),
                                   class_info.imtable_builder->GetIMTSize(), class_info.size);

    klass->SetLoadContext(context);
    klass->SetBase(base_class);
    klass->SetInterfaces(interfaces);
    klass->SetFileId(class_data_accessor->GetClassId());
    klass->SetPandaFile(&class_data_accessor->GetPandaFile());
    klass->SetAccessFlags(class_data_accessor->GetAccessFlags());

    auto &pf = class_data_accessor->GetPandaFile();
    auto class_id = class_data_accessor->GetClassId();
    klass->SetClassIndex(pf.GetClassIndex(class_id));
    klass->SetMethodIndex(pf.GetMethodIndex(class_id));
    klass->SetFieldIndex(pf.GetFieldIndex(class_id));

    klass->SetNumVirtualMethods(class_info.vtable_builder->GetNumVirtualMethods());
    klass->SetNumCopiedMethods(class_info.vtable_builder->GetCopiedMethods().size());
    klass->SetNumStaticFields(class_info.num_sfields);

    if (!LoadMethods(klass, &class_info, class_data_accessor, error_handler)) {
        FreeClass(klass);
        LOG(ERROR, CLASS_LINKER) << "Cannot load methods of class '" << descriptor << "'";
        return nullptr;
    }

    if (!LoadFields(klass, class_data_accessor, error_handler)) {
        FreeClass(klass);
        LOG(ERROR, CLASS_LINKER) << "Cannot load fields of class '" << descriptor << "'";
        return nullptr;
    }

    if (!LinkMethods(klass, &class_info, error_handler)) {
        FreeClass(klass);
        LOG(ERROR, CLASS_LINKER) << "Cannot link methods of class '" << descriptor << "'";
        return nullptr;
    }

    if (!LinkFields(klass, error_handler)) {
        FreeClass(klass);
        LOG(ERROR, CLASS_LINKER) << "Cannot link fields of class '" << descriptor << "'";
        return nullptr;
    }

    return klass;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
Class *ClassLinker::LoadClass(const panda_file::File *pf, panda_file::File::EntityId class_id,
                              const uint8_t *descriptor, ClassLinkerContext *context,
                              ClassLinkerErrorHandler *error_handler)
{
    ASSERT(!pf->IsExternal(class_id));
    ASSERT(context != nullptr);
    panda_file::ClassDataAccessor class_data_accessor(*pf, class_id);
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(&class_data_accessor);

    // This set is used to find out if the class is its own superclass
    static thread_local std::unordered_set<uint64_t> anti_circulation_id_set;
    ClassScopeStaticSetAutoCleaner class_set_auto_cleaner_on_return(&anti_circulation_id_set);

    auto *ext = GetExtension(ctx);
    if (ext == nullptr) {
        PandaStringStream ss;
        ss << "Cannot load class '" << descriptor << "' as class linker hasn't " << ctx << " language extension";
        LOG(ERROR, CLASS_LINKER) << ss.str();
        OnError(error_handler, Error::CLASS_NOT_FOUND, ss.str());
        return nullptr;
    }

    Class *base_class = nullptr;
    bool need_load_base = IsInitialized() || !utf::IsEqual(ctx.GetObjectClassDescriptor(), descriptor);

    if (need_load_base) {
        uint32_t class_id_int = class_id.GetOffset();
        uint32_t panda_file_hash = pf->GetFilenameHash();
        if (anti_circulation_id_set.find(GetClassUniqueHash(panda_file_hash, class_id_int)) ==
            anti_circulation_id_set.end()) {
            anti_circulation_id_set.insert(GetClassUniqueHash(panda_file_hash, class_id_int));
        } else {
            ThrowClassCircularityError(utf::Mutf8AsCString(pf->GetStringData(class_data_accessor.GetClassId()).data),
                                       ctx);
            return nullptr;
        }

        base_class = LoadBaseClass(&class_data_accessor, ctx, context, error_handler);
        if (base_class == nullptr) {
            LOG(INFO, CLASS_LINKER) << "Cannot load base class of class '" << descriptor << "'";
            return nullptr;
        }
    }

    auto res = LoadInterfaces(&class_data_accessor, context, error_handler);
    if (!res) {
        LOG(INFO, CLASS_LINKER) << "Cannot load interfaces of class '" << descriptor << "'";
        return nullptr;
    }

    auto *klass = LoadClass(&class_data_accessor, descriptor, base_class, res.value(), context, ext, error_handler);
    if (klass == nullptr) {
        return nullptr;
    }

    if (LIKELY(ext->CanInitializeClasses())) {
        ext->InitializeClass(klass);
        klass->SetState(Class::State::LOADED);
    }

    Runtime::GetCurrent()->GetNotificationManager()->ClassLoadEvent(klass);

    auto *other_klass = context->InsertClass(klass);
    if (other_klass != nullptr) {
        // Someone has created the class in the other thread (increase the critical section?)
        FreeClass(klass);
        return other_klass;
    }

    RemoveCreatedClassInExtension(klass);
    Runtime::GetCurrent()->GetNotificationManager()->ClassPrepareEvent(klass);

    return klass;
}

static const uint8_t *CopyMutf8String(mem::InternalAllocatorPtr allocator, const uint8_t *descriptor)
{
    size_t size = utf::Mutf8Size(descriptor) + 1;  // + 1 - null terminate
    auto *ptr = allocator->AllocArray<uint8_t>(size);
    (void)memcpy_s(ptr, size, descriptor, size);
    return ptr;
}

Class *ClassLinker::BuildClass(const uint8_t *descriptor, bool need_copy_descriptor, uint32_t access_flags,
                               Span<Method> methods, Span<Field> fields, Class *base_class, Span<Class *> interfaces,
                               ClassLinkerContext *context, bool is_interface)
{
    ASSERT(context != nullptr);
    if (need_copy_descriptor) {
        descriptor = CopyMutf8String(allocator_, descriptor);
        os::memory::LockHolder lock(copied_names_lock_);
        copied_names_.push_front(descriptor);
    }

    auto *ext = GetExtension(base_class->GetSourceLang());
    ASSERT(ext != nullptr);

    ClassInfo class_info = GetClassInfo(methods, fields, base_class, interfaces, is_interface);

    // Need to protect ArenaAllocator and loaded_classes_
    auto *klass = ext->CreateClass(descriptor, class_info.vtable_builder->GetVTableSize(),
                                   class_info.imtable_builder->GetIMTSize(), class_info.size);
    klass->SetLoadContext(context);
    klass->SetBase(base_class);
    klass->SetInterfaces(interfaces);
    klass->SetAccessFlags(access_flags);

    klass->SetNumVirtualMethods(class_info.vtable_builder->GetNumVirtualMethods());
    klass->SetNumCopiedMethods(class_info.vtable_builder->GetCopiedMethods().size());
    klass->SetNumStaticFields(class_info.num_sfields);

    ASSERT(klass->GetNumCopiedMethods() == 0);

    size_t num_smethods = methods.size() - klass->GetNumVirtualMethods();
    klass->SetMethods(methods, klass->GetNumVirtualMethods(), num_smethods);
    klass->SetFields(fields, klass->GetNumStaticFields());

    for (auto &method : methods) {
        method.SetClass(klass);
    }

    for (auto &field : fields) {
        field.SetClass(klass);
    }

    if (!LinkMethods(klass, &class_info, ext->GetErrorHandler())) {
        LOG(ERROR, CLASS_LINKER) << "Cannot link class methods '" << descriptor << "'";
        return nullptr;
    }

    if (!LinkFields(klass, ext->GetErrorHandler())) {
        LOG(ERROR, CLASS_LINKER) << "Cannot link class fields '" << descriptor << "'";
        return nullptr;
    }

    ext->InitializeClass(klass);
    klass->SetState(Class::State::LOADED);

    Runtime::GetCurrent()->GetNotificationManager()->ClassLoadEvent(klass);

    auto *other_klass = context->InsertClass(klass);
    if (other_klass != nullptr) {
        // Someone has created the class in the other thread (increase the critical section?)
        FreeClass(klass);
        return other_klass;
    }

    RemoveCreatedClassInExtension(klass);
    Runtime::GetCurrent()->GetNotificationManager()->ClassPrepareEvent(klass);

    return klass;
}

Class *ClassLinker::CreateArrayClass(ClassLinkerExtension *ext, const uint8_t *descriptor, bool need_copy_descriptor,
                                     Class *component_class)
{
    if (need_copy_descriptor) {
        descriptor = CopyMutf8String(allocator_, descriptor);
        os::memory::LockHolder lock(copied_names_lock_);
        copied_names_.push_front(descriptor);
    }

    auto *array_class = ext->CreateClass(descriptor, ext->GetArrayClassVTableSize(), ext->GetArrayClassIMTSize(),
                                         ext->GetArrayClassSize());
    array_class->SetLoadContext(component_class->GetLoadContext());

    ext->InitializeArrayClass(array_class, component_class);

    return array_class;
}

Class *ClassLinker::LoadArrayClass(const uint8_t *descriptor, bool need_copy_descriptor, ClassLinkerContext *context,
                                   ClassLinkerErrorHandler *error_handler)
{
    Span<const uint8_t> sp(descriptor, 1);

    Class *component_class = GetClass(sp.cend(), need_copy_descriptor, context, error_handler);

    if (component_class == nullptr) {
        return nullptr;
    }

    if (UNLIKELY(component_class->GetType().GetId() == panda_file::Type::TypeId::VOID)) {
        OnError(error_handler, Error::NO_CLASS_DEF, "Try to create array with void component type");
        return nullptr;
    }

    auto *ext = GetExtension(component_class->GetSourceLang());
    ASSERT(ext != nullptr);

    auto *component_class_context = component_class->GetLoadContext();
    ASSERT(component_class_context != nullptr);
    if (component_class_context != context) {
        auto *loaded_class = FindLoadedClass(descriptor, component_class_context);
        if (loaded_class != nullptr) {
            return loaded_class;
        }
    }

    auto *array_class = CreateArrayClass(ext, descriptor, need_copy_descriptor, component_class);

    Runtime::GetCurrent()->GetNotificationManager()->ClassLoadEvent(array_class);

    auto *other_klass = component_class_context->InsertClass(array_class);
    if (other_klass != nullptr) {
        FreeClass(array_class);
        return other_klass;
    }

    RemoveCreatedClassInExtension(array_class);
    Runtime::GetCurrent()->GetNotificationManager()->ClassPrepareEvent(array_class);

    return array_class;
}

static PandaString PandaFilesToString(const PandaVector<const panda_file::File *> &panda_files)
{
    PandaStringStream ss;
    ss << "[";

    size_t n = panda_files.size();
    for (size_t i = 0; i < n; i++) {
        ss << panda_files[i]->GetFilename();

        if (i < n - 1) {
            ss << ", ";
        }
    }

    ss << "]";
    return ss.str();
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
Class *ClassLinker::GetClass(const uint8_t *descriptor, bool need_copy_descriptor, ClassLinkerContext *context,
                             ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    ASSERT(context != nullptr);
    Class *cls = FindLoadedClass(descriptor, context);
    if (cls != nullptr) {
        return cls;
    }

    if (ClassHelper::IsArrayDescriptor(descriptor)) {
        return LoadArrayClass(descriptor, need_copy_descriptor, context, error_handler);
    }

    if (context->IsBootContext()) {
        auto [class_id, panda_file] = FindClassInPandaFiles(descriptor, boot_panda_files_);

        if (!class_id.IsValid()) {
            PandaStringStream ss;
            ss << "Cannot find class " << descriptor
               << " in boot panda files: " << PandaFilesToString(boot_panda_files_);
            OnError(error_handler, Error::CLASS_NOT_FOUND, ss.str());
            return nullptr;
        }

        return LoadClass(panda_file, class_id, panda_file->GetStringData(class_id).data, context, error_handler);
    }

    return context->LoadClass(descriptor, need_copy_descriptor, error_handler);
}

Class *ClassLinker::GetClass(const panda_file::File &pf, panda_file::File::EntityId id, ClassLinkerContext *context,
                             ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    ASSERT(context != nullptr);
    Class *cls = pf.GetPandaCache()->GetClassFromCache(id);
    if (cls != nullptr) {
        return cls;
    }
    const uint8_t *descriptor = pf.GetStringData(id).data;

    cls = FindLoadedClass(descriptor, context);
    if (cls != nullptr) {
        pf.GetPandaCache()->SetClassCache(id, cls);
        return cls;
    }

    if (ClassHelper::IsArrayDescriptor(descriptor)) {
        cls = LoadArrayClass(descriptor, false, context, error_handler);
        if (LIKELY(cls != nullptr)) {
            pf.GetPandaCache()->SetClassCache(id, cls);
        }
        return cls;
    }

    if (context->IsBootContext()) {
        const panda_file::File *pf_ptr = nullptr;
        panda_file::File::EntityId ext_id;

        std::tie(ext_id, pf_ptr) = FindClassInPandaFiles(descriptor, boot_panda_files_);

        if (!ext_id.IsValid()) {
            PandaStringStream ss;
            ss << "Cannot find class " << descriptor
               << " in boot panda files: " << PandaFilesToString(boot_panda_files_);
            OnError(error_handler, Error::CLASS_NOT_FOUND, ss.str());
            return nullptr;
        }

        cls = LoadClass(pf_ptr, ext_id, descriptor, context, error_handler);
        if (LIKELY(cls != nullptr)) {
            pf.GetPandaCache()->SetClassCache(id, cls);
        }
        return cls;
    }

    return context->LoadClass(descriptor, false, error_handler);
}

Method *ClassLinker::GetMethod(const panda_file::File &pf, panda_file::File::EntityId id,
                               // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
                               ClassLinkerContext *context /* = nullptr */,
                               ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    Method *method = pf.GetPandaCache()->GetMethodFromCache(id);
    if (method != nullptr) {
        return method;
    }
    panda_file::MethodDataAccessor method_data_accessor(pf, id);

    auto class_id = method_data_accessor.GetClassId();
    if (context == nullptr) {
        panda_file::ClassDataAccessor class_data_accessor(pf, class_id);
        auto lang = class_data_accessor.GetSourceLang();
        if (!lang) {
            LOG(INFO, CLASS_LINKER) << "Cannot resolve language context for class_id " << class_id << " in file "
                                    << pf.GetFilename();
            return nullptr;
        }
        auto *extension = GetExtension(lang.value());
        context = extension->GetBootContext();
    }

    Class *klass = GetClass(pf, class_id, context, error_handler);

    if (klass == nullptr) {
        auto class_name = pf.GetStringData(class_id).data;
        LOG(INFO, CLASS_LINKER) << "Cannot find class '" << class_name << "' in ctx " << context;
        return nullptr;
    }
    method = GetMethod(klass, method_data_accessor, error_handler);
    if (LIKELY(method != nullptr)) {
        pf.GetPandaCache()->SetMethodCache(id, method);
    }
    return method;
}

Method *ClassLinker::GetMethod(const Method &caller, panda_file::File::EntityId id,
                               ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    auto *pf = caller.GetPandaFile();
    Method *method = pf->GetPandaCache()->GetMethodFromCache(id);
    if (method != nullptr) {
        return method;
    }

    panda_file::MethodDataAccessor method_data_accessor(*pf, id);
    auto class_id = method_data_accessor.GetClassId();

    auto *context = caller.GetClass()->GetLoadContext();
    auto *ext = GetExtension(caller.GetClass()->GetSourceLang());
    Class *klass = ext->GetClass(*pf, class_id, context, error_handler);

    if (klass == nullptr) {
        auto class_name = pf->GetStringData(class_id).data;
        LOG(INFO, CLASS_LINKER) << "Cannot find class '" << class_name << "' in ctx " << context;
        return nullptr;
    }

    method = GetMethod(klass, method_data_accessor, error_handler == nullptr ? ext->GetErrorHandler() : error_handler);
    if (LIKELY(method != nullptr)) {
        pf->GetPandaCache()->SetMethodCache(id, method);
    }
    return method;
}

Method *ClassLinker::GetMethod(const Class *klass, const panda_file::MethodDataAccessor &method_data_accessor,
                               ClassLinkerErrorHandler *error_handler)
{
    Method *method;
    auto id = method_data_accessor.GetMethodId();
    const auto &pf = method_data_accessor.GetPandaFile();

    if (!method_data_accessor.IsExternal() && klass->GetPandaFile() == &pf) {
        bool is_static = method_data_accessor.IsStatic();

        auto pred = [id](const Method &m) { return m.GetFileId() == id; };

        if (klass->IsInterface()) {
            method = is_static ? klass->FindStaticInterfaceMethod(pred) : klass->FindVirtualInterfaceMethod(pred);
        } else {
            method = is_static ? klass->FindStaticClassMethod(pred) : klass->FindVirtualClassMethod(pred);
        }

        if (method == nullptr) {
            PandaStringStream ss;
            ss << "Cannot find method '" << pf.GetStringData(method_data_accessor.GetNameId()).data << "' in class '"
               << klass->GetName() << "'";
            OnError(error_handler, Error::METHOD_NOT_FOUND, ss.str());
            return nullptr;
        }

        return method;
    }

    auto name = pf.GetStringData(method_data_accessor.GetNameId());
    auto proto = Method::Proto(pf, method_data_accessor.GetProtoId());

    auto pred = [name, &proto](const Method &m) { return m.GetName() == name && m.GetProto() == proto; };

    if (klass->IsInterface()) {
        method = klass->FindInterfaceMethod(pred);
    } else {
        method = klass->FindClassMethod(pred);
        if (method == nullptr && klass->IsAbstract()) {
            method = klass->FindInterfaceMethod(pred);
        }
    }

    if (method == nullptr) {
        PandaStringStream ss;
        ss << "Cannot find method '" << pf.GetStringData(method_data_accessor.GetNameId()).data << "' in class '"
           << klass->GetName() << "'";
        OnError(error_handler, Error::METHOD_NOT_FOUND, ss.str());
        return nullptr;
    }

    LOG_IF(method->IsStatic() != method_data_accessor.IsStatic(), FATAL, CLASS_LINKER)
        << "Expected ACC_STATIC for method " << pf.GetStringData(method_data_accessor.GetNameId()).data << " in class "
        << klass->GetName() << " does not match loaded value";

    return method;
}

Field *ClassLinker::GetFieldById(Class *klass, const panda_file::FieldDataAccessor &field_data_accessor,
                                 ClassLinkerErrorHandler *error_handler)
{
    bool is_static = field_data_accessor.IsStatic();
    auto &pf = field_data_accessor.GetPandaFile();
    auto id = field_data_accessor.GetFieldId();

    auto pred = [id](const Field &field) { return field.GetFileId() == id; };

    Field *field = is_static ? klass->FindStaticField(pred) : klass->FindInstanceField(pred);

    if (field == nullptr) {
        PandaStringStream ss;
        ss << "Cannot find field '" << pf.GetStringData(field_data_accessor.GetNameId()).data << "' in class '"
           << klass->GetName() << "'";
        OnError(error_handler, Error::FIELD_NOT_FOUND, ss.str());
        return nullptr;
    }

    pf.GetPandaCache()->SetFieldCache(id, field);
    return field;
}

Field *ClassLinker::GetFieldBySignature(Class *klass, const panda_file::FieldDataAccessor &field_data_accessor,
                                        ClassLinkerErrorHandler *error_handler)
{
    auto &pf = field_data_accessor.GetPandaFile();
    auto id = field_data_accessor.GetFieldId();
    auto field_name = pf.GetStringData(field_data_accessor.GetNameId());
    auto field_type = panda_file::Type::GetTypeFromFieldEncoding(field_data_accessor.GetType());
    Field *field = klass->FindField([&](const Field &fld) {
        if (field_type == fld.GetType() && field_name == fld.GetName()) {
            if (!field_type.IsReference()) {
                return true;
            }

            // compare field class type
            if (&pf == fld.GetPandaFile() && id == fld.GetFileId()) {
                return true;
            }
            panda_file::FieldDataAccessor fda(*fld.GetPandaFile(), fld.GetFileId());
            if (pf.GetStringData(panda_file::File::EntityId(field_data_accessor.GetType())) ==
                fld.GetPandaFile()->GetStringData(panda_file::File::EntityId(fda.GetType()))) {
                return true;
            }
        }
        return false;
    });

    if (field == nullptr) {
        PandaStringStream ss;
        ss << "Cannot find field '" << field_name.data << "' in class '" << klass->GetName() << "'";
        OnError(error_handler, Error::FIELD_NOT_FOUND, ss.str());
        return nullptr;
    }

    pf.GetPandaCache()->SetFieldCache(id, field);
    return field;
}

Field *ClassLinker::GetField(const panda_file::File &pf, panda_file::File::EntityId id,
                             // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
                             ClassLinkerContext *context /* = nullptr */,
                             ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    Field *field = pf.GetPandaCache()->GetFieldFromCache(id);
    if (field != nullptr) {
        return field;
    }
    panda_file::FieldDataAccessor field_data_accessor(pf, id);

    Class *klass = GetClass(pf, field_data_accessor.GetClassId(), context, error_handler);

    if (klass == nullptr) {
        auto class_name = pf.GetStringData(field_data_accessor.GetClassId()).data;
        LOG(INFO, CLASS_LINKER) << "Cannot find class '" << class_name << "' in ctx " << context;
        return nullptr;
    }

    if (!field_data_accessor.IsExternal() && klass->GetPandaFile() == &pf) {
        field = GetFieldById(klass, field_data_accessor, error_handler);
    } else {
        field = GetFieldBySignature(klass, field_data_accessor, error_handler);
    }
    return field;
}

Method *ClassLinker::GetMethod(std::string_view panda_file, panda_file::File::EntityId id)
{
    os::memory::LockHolder lock(panda_files_lock_);
    for (auto &data : panda_files_) {
        if (data.pf->GetFilename() == panda_file) {
            return GetMethod(*data.pf, id, data.context);
        }
    }

    return nullptr;
}

bool ClassLinker::InitializeClass(ManagedThread *thread, Class *klass)
{
    ASSERT(klass != nullptr);
    if (klass->IsInitialized()) {
        return true;
    }

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    return ctx.InitializeClass(this, thread, klass);
}

size_t ClassLinker::NumLoadedClasses()
{
    size_t sum = 0;

    for (auto &ext : extensions_) {
        if (ext == nullptr) {
            continue;
        }

        sum += ext->NumLoadedClasses();
    }

    return sum;
}

void ClassLinker::VisitLoadedClasses(size_t flag)
{
    for (auto &ext : extensions_) {
        if (ext == nullptr) {
            continue;
        }
        ext->VisitLoadedClasses(flag);
    }
}

void ClassLinker::OnError(ClassLinkerErrorHandler *error_handler, ClassLinker::Error error, const PandaString &msg)
{
    if (error_handler != nullptr) {
        error_handler->OnError(error, msg);
    }
}

Field *ClassLinker::GetField(const Method &caller, panda_file::File::EntityId id,
                             ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    Field *field = caller.GetPandaFile()->GetPandaCache()->GetFieldFromCache(id);
    if (field != nullptr) {
        return field;
    }
    auto *ext = GetExtension(caller.GetClass()->GetSourceLang());
    field = GetField(*caller.GetPandaFile(), id, caller.GetClass()->GetLoadContext(),
                     error_handler == nullptr ? ext->GetErrorHandler() : error_handler);
    if (LIKELY(field != nullptr)) {
        caller.GetPandaFile()->GetPandaCache()->SetFieldCache(id, field);
    }
    return field;
}

void ClassLinker::RemoveCreatedClassInExtension(Class *klass)
{
    if (klass == nullptr) {
        return;
    }
    auto ext = GetExtension(klass->GetSourceLang());
    if (ext != nullptr) {
        ext->OnClassPrepared(klass);
    }
}

}  // namespace panda
