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

#ifndef PANDA_VERIFICATION_UTIL_TESTS_VERIFIER_TEST_H_
#define PANDA_VERIFICATION_UTIL_TESTS_VERIFIER_TEST_H_

#include "include/runtime.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {
class VerifierTest : public testing::Test {
public:
    VerifierTest()
    {
        RuntimeOptions options;
        Logger::InitializeDummyLogging();
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetHeapSizeLimit(64_MB);
        options.SetVerificationEnabled(true);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~VerifierTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_;
};
}  // namespace panda::verifier::test

#endif  // PANDA_VERIFICATION_UTIL_TESTS_VERIFIER_TEST_H_
