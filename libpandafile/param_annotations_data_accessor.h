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

#ifndef PANDA_LIBPANDAFILE_PARAM_ANNOTATIONS_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_PARAM_ANNOTATIONS_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"
#include "helpers.h"

namespace panda::panda_file {
class ParamAnnotationsDataAccessor {
public:
    class AnnotationArray {
    public:
        AnnotationArray(uint32_t count, Span<const uint8_t> offsets) : count_(count), offsets_(offsets) {}

        ~AnnotationArray() = default;

        NO_COPY_SEMANTIC(AnnotationArray);
        NO_MOVE_SEMANTIC(AnnotationArray);
        uint32_t GetCount() const
        {
            return count_;
        }

        uint32_t GetSize() const
        {
            return sizeof(uint32_t) + count_ * ID_SIZE;
        }

        template <class Callback>
        void EnumerateAnnotations(const Callback &cb)
        {
            auto sp = offsets_;
            for (size_t i = 0; i < count_; i++) {
                File::EntityId id(helpers::Read<ID_SIZE>(&sp));
                cb(id);
            }
        }

    private:
        uint32_t count_;
        Span<const uint8_t> offsets_;
    };

    ParamAnnotationsDataAccessor(const File &panda_file, File::EntityId id) : panda_file_(panda_file), id_(id)
    {
        auto sp = panda_file_.GetSpanFromId(id);
        count_ = helpers::Read<COUNT_SIZE>(&sp);
        annotations_array_ = sp;
    }

    ~ParamAnnotationsDataAccessor() = default;

    NO_COPY_SEMANTIC(ParamAnnotationsDataAccessor);
    NO_MOVE_SEMANTIC(ParamAnnotationsDataAccessor);

    template <class Callback>
    void EnumerateAnnotationArrays(const Callback &cb)
    {
        auto sp = annotations_array_;
        size_t size = COUNT_SIZE;

        for (size_t i = 0; i < count_; i++) {
            auto count = helpers::Read<COUNT_SIZE>(&sp);
            AnnotationArray array(count, sp);
            sp = sp.SubSpan(count * ID_SIZE);
            cb(array);
            size += array.GetSize();
        }

        size_ = size;
    }

    AnnotationArray GetAnnotationArray(uint32_t index)
    {
        ASSERT(index < count_);

        auto sp = annotations_array_;
        for (uint32_t i = 0; i < count_; i++) {
            auto count = helpers::Read<COUNT_SIZE>(&sp);
            if (i == index) {
                return AnnotationArray {count, sp};
            }
            sp = sp.SubSpan(count * ID_SIZE);
        }
        UNREACHABLE();
    }

    uint32_t GetCount() const
    {
        return count_;
    }

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipAnnotationArrays();
        }

        return size_;
    }

    File::EntityId GetParamAnnotationsId() const
    {
        return id_;
    }

private:
    static constexpr size_t COUNT_SIZE = sizeof(uint32_t);

    void SkipAnnotationArrays()
    {
        EnumerateAnnotationArrays([](const AnnotationArray & /* unused */) {});
    }

    const File &panda_file_;
    File::EntityId id_;

    uint32_t count_;
    Span<const uint8_t> annotations_array_ {nullptr, nullptr};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_PARAM_ANNOTATIONS_DATA_ACCESSOR_H_
