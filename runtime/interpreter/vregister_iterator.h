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

#ifndef PANDA_RUNTIME_INTERPRETER_VREGISTER_ITERATOR_H_
#define PANDA_RUNTIME_INTERPRETER_VREGISTER_ITERATOR_H_

namespace panda::interpreter {

template <BytecodeInstruction::Format format>
class VRegisterIterator {
public:
    explicit VRegisterIterator(BytecodeInstruction insn, Frame *frame) : instn_(std::move(insn)), frame_(frame) {}

    template <class T>
    ALWAYS_INLINE inline T GetAs(size_t param_idx) const
    {
        size_t vreg_idx;

        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
        if constexpr (format == BytecodeInstruction::Format::V4_V4_ID16 /* short */) {
            switch (param_idx) {
                case 0: {
                    vreg_idx = instn_.GetVReg<format, 0>();
                    break;
                }
                case 1: {
                    vreg_idx = instn_.GetVReg<format, 1>();
                    break;
                }
                default:
                    UNREACHABLE();
            }
        } else if constexpr (format == BytecodeInstruction::Format::V4_V4_V4_V4_ID16) {
            switch (param_idx) {
                case 0: {
                    vreg_idx = instn_.GetVReg<format, 0>();
                    break;
                }
                case 1: {
                    vreg_idx = instn_.GetVReg<format, 1>();
                    break;
                }
                case 2: {
                    vreg_idx = instn_.GetVReg<format, 2>();
                    break;
                }
                case 3: {
                    vreg_idx = instn_.GetVReg<format, 3>();
                    break;
                }
                default:
                    UNREACHABLE();
            }
            // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
        } else if constexpr (format == BytecodeInstruction::Format::V8_ID16 /* range */) {
            vreg_idx = instn_.GetVReg<format, 0>() + param_idx;
        } else {
            UNREACHABLE();
        }

        auto vreg = frame_->GetVReg(vreg_idx);
        return vreg.template GetAs<T>();
    }

private:
    BytecodeInstruction instn_;
    Frame *frame_;
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_VREGISTER_ITERATOR_H_
