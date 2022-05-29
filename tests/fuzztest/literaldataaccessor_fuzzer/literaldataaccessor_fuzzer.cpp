/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "literaldataaccessor_fuzzer.h"
#include "libpandafile/file.h"
#include "libpandafile/literal_data_accessor.h"

namespace OHOS {
void LiteralDataAccessorFuzzTest(const uint8_t *data, size_t size)
{
    auto pf = panda::panda_file::OpenPandaFileFromMemory(data, size);
    if (pf == nullptr) {
        return;
    }
    panda::panda_file::File::EntityId literal_arrays_id = pf->GetLiteralArraysId();
    panda::panda_file::LiteralDataAccessor(*pf, literal_arrays_id);
}
}  // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::LiteralDataAccessorFuzzTest(data, size);
    return 0;
}
