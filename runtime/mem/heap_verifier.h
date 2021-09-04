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

#ifndef PANDA_RUNTIME_MEM_HEAP_VERIFIER_H_
#define PANDA_RUNTIME_MEM_HEAP_VERIFIER_H_

#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/logger.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/rendezvous.h"

namespace panda::mem {

class HeapManager;
class GCRoot;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_HEAP_VERIFIER(level) LOG(level, RUNTIME) << "HEAP_VERIFIER: "

/**
 * HeapReferenceVerifier checks reference checks if the referent is with the heap and it is live.
 */
class HeapReferenceVerifier {
public:
    explicit HeapReferenceVerifier(HeapManager *heap, size_t *count) : HEAP(heap), FAIL_COUNT(count) {}
    ~HeapReferenceVerifier() = default;
    DEFAULT_MOVE_CTOR(HeapReferenceVerifier)
    DEFAULT_COPY_CTOR(HeapReferenceVerifier)
    NO_MOVE_OPERATOR(HeapReferenceVerifier);
    NO_COPY_OPERATOR(HeapReferenceVerifier);

    void operator()(ObjectHeader *object_header, ObjectHeader *referent);

    void operator()(const GCRoot &root);

private:
    HeapManager *const HEAP {nullptr};
    size_t *const FAIL_COUNT {nullptr};
};

/**
 * HeapObjectVerifier iterates over HeapManager's allocated objects. If an object contains reference, it checks if the
 * referent is with the heap and it is live.
 */
template <LangTypeT LangType = LANG_TYPE_STATIC>
class HeapObjectVerifier {
public:
    HeapObjectVerifier() = delete;

    HeapObjectVerifier(HeapManager *heap, size_t *count) : HEAP(heap), FAIL_COUNT(count) {}

    void operator()(ObjectHeader *obj);

    size_t GetFailCount() const
    {
        return *FAIL_COUNT;
    }

private:
    HeapManager *const HEAP {nullptr};
    size_t *const FAIL_COUNT {nullptr};
};

/**
 * A class to query address validity.
 */
template <class LanguageConfig>
class HeapVerifier {
public:
    explicit HeapVerifier(HeapManager *heap) : heap_(heap) {}
    ~HeapVerifier() = default;
    DEFAULT_MOVE_SEMANTIC(HeapVerifier);
    DEFAULT_COPY_SEMANTIC(HeapVerifier);

    bool IsValidObjectAddress(void *addr) const;

    bool IsHeapAddress(void *addr) const;

    size_t VerifyRoot() const;

    size_t VerifyHeap() const;

    size_t VerifyAll() const
    {
        return VerifyRoot() + VerifyHeap();
    }

    size_t VerifyAllPaused() const;

private:
    HeapManager *heap_;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_HEAP_VERIFIER_H_
