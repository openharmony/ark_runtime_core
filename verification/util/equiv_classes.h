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

#ifndef PANDA_VERIFICATION_UTIL_EQUIV_CLASSES_H_
#define PANDA_VERIFICATION_UTIL_EQUIV_CLASSES_H_

#include "lazy.h"
#include "abstract_index.h"

#include "runtime/include/mem/panda_containers.h"

#include <limits>

namespace panda::verifier {
/*
Future improvements:
optimization: tree-organization
each classentry is either: fixed index, or pointer to other classindex.
each classentry has refcounter
when object class is determined, after finding actual class, object class set
to this classindex and refcounts of classentry of prev class and new one recalculated.
when refcount == 0 class is disposed

introduce flatten operation
*/

template <typename T>
class EqClass;

template <>
class EqClass<size_t> {
public:
    using Idx = AbstractIndex<size_t, EqClass>;

    class ClassIndex : public Idx {
    public:
        ClassIndex &operator=(size_t val)
        {
            Idx::operator=(val);
            return *this;
        }
    };

    class ObjIndex : public Idx {
    public:
        ObjIndex &operator=(size_t val)
        {
            Idx::operator=(val);
            return *this;
        }
    };

    struct ClassEntry {
        size_t Size = 0;
        ObjIndex Head;
        ObjIndex Tail;
    };

    struct ObjectEntry {
        ClassIndex Class;
        ObjIndex Next;
        ObjIndex Prev;
    };

    ClassEntry &ClsEntry(ClassIndex cls)
    {
        return EqClasses_[cls];
    }

    const ClassEntry &ClsEntry(ClassIndex cls) const
    {
        return EqClasses_[cls];
    }

    ClassIndex NewClassIndex()
    {
        ClassIndex cls;
        if (!FreeClassIndices_.empty()) {
            cls = FreeClassIndices_.back();
            FreeClassIndices_.pop_back();
        } else {
            cls = EqClasses_.size();
            EqClasses_.push_back({});
        }
        return cls;
    }

    void DisposeClassIndex(ClassIndex idx)
    {
        auto &entry = ClsEntry(idx);
        entry.Head.Invalidate();
        entry.Tail.Invalidate();
        entry.Size = 0;
        FreeClassIndices_.push_back(idx);
    }

    ObjectEntry &ObjEntry(ObjIndex idx)
    {
        return Objects_[idx];
    }

    const ObjectEntry &ObjEntry(ObjIndex idx) const
    {
        return Objects_[idx];
    }

    ClassIndex ObjClass(ObjIndex idx) const
    {
        return Objects_[idx].Class;
    }

    ClassIndex JoinClasses(ClassIndex lhs_class, ClassIndex rhs_class)
    {
        if (lhs_class == rhs_class) {
            return lhs_class;
        }
        auto [lhs, rhs] =
            (lhs_class < rhs_class ? std::tuple {lhs_class, rhs_class} : std::tuple {rhs_class, lhs_class});
        auto &lhs_cls_entry = ClsEntry(lhs);
        auto &rhs_cls_entry = ClsEntry(rhs);
        auto &lhs_tail_obj_entry = ObjEntry(lhs_cls_entry.Tail);
        auto &rhs_head_obj_entry = ObjEntry(rhs_cls_entry.Head);
        lhs_tail_obj_entry.Next = rhs_cls_entry.Head;
        rhs_head_obj_entry.Prev = lhs_cls_entry.Tail;
        lhs_cls_entry.Tail = rhs_cls_entry.Tail;
        lhs_cls_entry.Size += rhs_cls_entry.Size;
        auto obj = lhs_tail_obj_entry.Next;
        while (obj.IsValid()) {
            auto &obj_entry = ObjEntry(obj);
            obj_entry.Class = lhs;
            obj = obj_entry.Next;
        }
        DisposeClassIndex(rhs);
        return lhs;
    }

    ObjIndex NewObjIndex()
    {
        ObjIndex obj;
        if (!FreeObjIndices_.empty()) {
            obj = FreeObjIndices_.back();
            FreeObjIndices_.pop_back();
        } else {
            obj = Objects_.size();
            Objects_.push_back({});
        }
        ClassIndex cls = NewClassIndex();
        auto &cls_entry = ClsEntry(cls);
        cls_entry.Head = cls_entry.Tail = obj;
        ++cls_entry.Size;
        ObjEntry(obj).Class = cls;
        return obj;
    }

    void DisposeObjIndex(ObjIndex obj)
    {
        if (!obj.IsValid()) {
            return;
        }
        auto cls = ObjClass(obj);
        auto &cls_entry = ClsEntry(cls);
        auto &obj_entry = ObjEntry(obj);
        auto &prev = obj_entry.Prev;
        auto &next = obj_entry.Next;
        if (prev.IsValid()) {
            ObjEntry(prev).Next = next;
        }
        if (next.IsValid()) {
            ObjEntry(next).Prev = prev;
        }
        if (cls_entry.Head == obj) {
            cls_entry.Head = next;
        }
        if (cls_entry.Tail == obj) {
            cls_entry.Tail = prev;
        }
        --cls_entry.Size;
        if (cls_entry.Size == 0) {
            DisposeClassIndex(cls);
        }
        obj_entry.Next.Invalidate();
        obj_entry.Prev.Invalidate();
        obj_entry.Class.Invalidate();
        FreeObjIndices_.push_back(obj);
    }

    template <typename F>
    void EquateLazy(F fetcher)
    {
        if (auto obj = fetcher()) {
            ClassIndex cls = ObjClass(*obj);
            ForEach(fetcher, [this, &cls](ObjIndex object) { cls = JoinClasses(cls, ObjClass(object)); });
        }
    }

    template <typename It>
    void Equate(It begin, It end)
    {
        if (begin == end) {
            return;
        }
        It it = begin;
        ClassIndex cls = ObjClass(*it++);
        for (; it != end; ++it) {
            cls = JoinClasses(cls, ObjClass(*it));
        }
    }

    void Equate(std::initializer_list<ObjIndex> objects)
    {
        Equate(objects.begin(), objects.end());
    }

    auto AllEqualToLazy(ObjIndex idx)
    {
        ClassIndex cls = ObjClass(idx);
        ObjIndex obj = ClsEntry(cls).Head;
        return [this, obj]() mutable -> std::optional<ObjIndex> {
            auto o = obj;
            if (obj.IsValid()) {
                obj = ObjEntry(obj).Next;
                return o;
            }
            return {};
        };
    }

    template <typename F>
    bool IsAllEqualLazy(F fetcher)
    {
        if (auto obj = fetcher()) {
            ClassIndex cls = ObjClass(*obj);
            return FoldLeft(fetcher, true,
                            [this, cls](bool result, ObjIndex object) { return result && cls == ObjClass(object); });
        }
        return true;
    }

    template <typename It>
    bool IsAllEqual(It begin, It end)
    {
        if (begin == end) {
            return true;
        }
        It it = begin;
        ClassIndex cls = ObjClass(*it++);
        for (; it != end; ++it) {
            if (cls != ObjClass(*it)) {
                return false;
            }
        }
        return true;
    }

    bool IsAllEqual(std::initializer_list<ObjIndex> objects)
    {
        return IsAllEqual(objects.begin(), objects.end());
    }

    size_t ClassSizeOf(ObjIndex obj) const
    {
        return ClsEntry(ObjClass(obj)).Size;
    }

    size_t AmountOfUsedObjIndices() const
    {
        return Objects_.size() - FreeObjIndices_.size();
    }

    size_t AmountOfUsedClassIndices() const
    {
        return EqClasses_.size() - FreeClassIndices_.size();
    }

    void shrink_to_fit()
    {
        // 1. get tail indices from EqClasses_ & Object_
        // 2. remove them from FreeClassIndices_ & FreeObjIndices_
        // 3. optimize all vectors in capacity
    }

private:
    PandaVector<ClassIndex> FreeClassIndices_;
    PandaVector<ObjIndex> FreeObjIndices_;
    PandaVector<ClassEntry> EqClasses_;
    PandaVector<ObjectEntry> Objects_;
};
}  // namespace panda::verifier

namespace std {
template <>
struct hash<panda::verifier::EqClass<size_t>::ObjIndex> {
    size_t operator()(const panda::verifier::EqClass<size_t>::ObjIndex &idx) const
    {
        return std::hash<panda::verifier::AbstractIndex<size_t, panda::verifier::EqClass<size_t>>> {}(
            static_cast<panda::verifier::AbstractIndex<size_t, panda::verifier::EqClass<size_t>>>(idx));
    }
};
}  // namespace std

namespace panda::verifier {
template <typename Obj>
class EqClass : private EqClass<size_t> {
public:
    using Base = EqClass<size_t>;

    std::optional<ObjIndex> GetIndex(const Obj &obj) const
    {
        auto it = ObjToIndex_.find(obj);
        if (it != ObjToIndex_.end()) {
            return it->second;
        }
        return {};
    }

    ObjIndex GetIndexOrCreate(const Obj &obj)
    {
        if (auto i = GetIndex(obj)) {
            return *i;
        }
        auto idx = Base::NewObjIndex();
        ObjToIndex_[obj] = idx;
        return idx;
    }

    void DisposeObject(const Obj &obj)
    {
        ObjIndex idx = GetIndexOrCreate(obj);
        ObjToIndex_.erase(obj);
        IndexToObj_.erase(idx);
        Base::DisposeObjIndex(idx);
    }

    template <typename F>
    void EquateLazy(F fetcher)
    {
        Base::EquateLazy(Transform(fetcher, [this](const Obj &obj) { return GetIndexOrCreate(obj); }));
    }

    template <typename It>
    void Equate(It begin, It end)
    {
        Base::EquateLazy([this, &begin, end]() -> std::optional<ObjIndex> {
            if (begin != end) {
                return GetIndexOrCreate(*begin++);
            }
            return {};
        });
    }

    void Equate(std::initializer_list<Obj> objects)
    {
        Equate(objects.begin(), objects.end());
    }

    auto AllEqualToLazy(const Obj &obj)
    {
        return Transform(Base::AllEqualToLazy(GetIndexOrCreate(obj)),
                         [this](ObjIndex idx) { return IndexToObj_[idx]; });
    }

    template <typename F>
    bool IsAllEqualLazy(F fetcher)
    {
        return Base::IsAllEqualLazy(Transform(fetcher, [this](const Obj &obj) { return GetIndexOrCreate(obj); }));
    }

    template <typename It>
    bool IsAllEqual(It begin, It end)
    {
        return Base::IsAllEqualLazy([this, &begin, end]() -> std::optional<ObjIndex> {
            if (begin != end) {
                return GetIndexOrCreate(*begin++);
            }
            return {};
        });
    }

    bool IsAllEqual(std::initializer_list<Obj> objects)
    {
        return IsAllEqual(objects.begin(), objects.end());
    }

    size_t ClassSizeOf(const Obj &obj) const
    {
        if (auto idx = GetIndex(obj)) {
            return Base::ClassSizeOf(*idx);
        }
        return 0;
    }

private:
    PandaUnorderedMap<Obj, ObjIndex> ObjToIndex_;
    PandaUnorderedMap<ObjIndex, Obj> IndexToObj_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_EQUIV_CLASSES_H_
