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

#include "verification/cache/results_cache.h"
#include "verification/util/synchronized.h"

#include "runtime/include/runtime.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/panda_containers.h"

#include "libpandabase/os/file.h"
#include "utils/logger.h"

namespace panda::verifier {

struct VerificationResultCache::Impl {
    std::string filename;
    Synchronized<PandaUnorderedSet<uint64_t>> verified_ok;
    Synchronized<PandaUnorderedSet<uint64_t>> verified_fail;

    template <typename It>
    Impl(std::string file_name, It data_start, It data_end)
        : filename {std::move(file_name)}, verified_ok {data_start, data_end}
    {
    }
};

VerificationResultCache::Impl *VerificationResultCache::impl {nullptr};

bool VerificationResultCache::Enabled()
{
    return impl != nullptr;
}

void VerificationResultCache::Initialize(const std::string &filename)
{
    if (Enabled()) {
        return;
    }
    using panda::os::file::Mode;
    using panda::os::file::Open;
    using Data = PandaVector<uint64_t>;

    auto file = Open(filename, Mode::READONLY);
    if (!file.IsValid()) {
        file = Open(filename, Mode::READWRITECREATE);
    }
    if (!file.IsValid()) {
        LOG(INFO, VERIFIER) << "Cannot open verification cache file '" << filename << "'";
        return;
    }

    auto size = file.GetFileSize();
    if (!size.HasValue()) {
        LOG(INFO, VERIFIER) << "Cannot get verification cache file size";
        file.Close();
        return;
    }

    Data data;

    auto elements = *size / sizeof(Data::value_type);
    if (elements > 0) {
        data.resize(elements, 0);
        if (!file.ReadAll(data.data(), *size)) {
            LOG(INFO, VERIFIER) << "Cannot read verification cache data";
            file.Close();
            return;
        }
    }

    file.Close();

    impl = new (mem::AllocatorAdapter<Impl>().allocate(1)) Impl {filename, data.cbegin(), data.cend()};
    ASSERT(Enabled());
}

void VerificationResultCache::Destroy(bool update_file)
{
    if (!Enabled()) {
        return;
    }
    if (update_file) {
        PandaVector<uint64_t> data;
        impl->verified_ok([&data](auto set) {
            data.reserve(set->size());
            data.insert(data.begin(), set->cbegin(), set->cend());
        });
        using panda::os::file::Mode;
        using panda::os::file::Open;
        do {
            auto file = Open(impl->filename, Mode::READWRITECREATE);
            if (!file.IsValid()) {
                LOG(INFO, VERIFIER) << "Cannot open verification cache file '" << impl->filename << "'";
                break;
            }
            if (!file.ClearData()) {
                LOG(INFO, VERIFIER) << "Cannot clear verification cache file '" << impl->filename << "'";
                file.Close();
                break;
            }
            if (!file.WriteAll(data.data(), data.size() * sizeof(uint64_t))) {
                LOG(INFO, VERIFIER) << "Cannot write to verification cache file '" << impl->filename << "'";
                file.ClearData();
                file.Close();
                break;
            }
            file.Close();
        } while (false);
    }
    impl->~Impl();
    mem::AllocatorAdapter<Impl>().deallocate(impl, 1);
    impl = nullptr;
}

void VerificationResultCache::CacheResult(uint64_t method_id, bool result)
{
    if (!Enabled()) {
        return;
    }
    if (result) {
        impl->verified_ok->insert(method_id);
    } else {
        impl->verified_fail->insert(method_id);
    }
}

VerificationResultCache::Status VerificationResultCache::Check(uint64_t method_id)
{
    if (Enabled()) {
        if (impl->verified_ok->count(method_id) > 0) {
            return Status::OK;
        }
        if (impl->verified_fail->count(method_id) > 0) {
            return Status::FAILED;
        }
    }
    return Status::UNKNOWN;
}

}  // namespace panda::verifier
