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

#ifndef PANDA_RUNTIME_ARCH_HELPERS_H_
#define PANDA_RUNTIME_ARCH_HELPERS_H_

#include "libpandabase/utils/arch.h"
#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/span.h"

namespace panda::arch {

template <Arch A>
struct ExtArchTraits;

#if !defined(PANDA_TARGET_ARM32_ABI_HARD)
template <>
struct ExtArchTraits<Arch::AARCH32> {
    using signed_word_type = int32_t;
    using unsigned_word_type = uint32_t;

    static constexpr size_t NUM_GP_ARG_REGS = 4;
    static constexpr size_t GP_ARG_NUM_BYTES = NUM_GP_ARG_REGS * ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t NUM_FP_ARG_REGS = 0;
    static constexpr size_t FP_ARG_NUM_BYTES = NUM_FP_ARG_REGS * ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t GPR_SIZE = ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t FPR_SIZE = 0;
    static constexpr bool HARDFP = false;
};
#else   // !defined(PANDA_TARGET_ARM32_ABI_HARD)
template <>
struct ExtArchTraits<Arch::AARCH32> {
    using signed_word_type = int32_t;
    using unsigned_word_type = uint32_t;

    static constexpr size_t NUM_GP_ARG_REGS = 4;
    static constexpr size_t GP_ARG_NUM_BYTES = NUM_GP_ARG_REGS * ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t NUM_FP_ARG_REGS = 16; /* s0 - s15 */
    static constexpr size_t FP_ARG_NUM_BYTES = NUM_FP_ARG_REGS * ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t GPR_SIZE = ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr size_t FPR_SIZE = ArchTraits<Arch::AARCH32>::POINTER_SIZE;
    static constexpr bool HARDFP = true;
};
#endif  // !defined(PANDA_TARGET_ARM32_ABI_HARD)

template <>
struct ExtArchTraits<Arch::AARCH64> {
    using signed_word_type = int64_t;
    using unsigned_word_type = uint64_t;

    static constexpr size_t NUM_GP_ARG_REGS = 8;
    static constexpr size_t GP_ARG_NUM_BYTES = NUM_GP_ARG_REGS * ArchTraits<Arch::AARCH64>::POINTER_SIZE;
    static constexpr size_t NUM_FP_ARG_REGS = 8;
    static constexpr size_t FP_ARG_NUM_BYTES = NUM_FP_ARG_REGS * ArchTraits<Arch::AARCH64>::POINTER_SIZE;
    static constexpr size_t GPR_SIZE = ArchTraits<Arch::AARCH64>::POINTER_SIZE;
    static constexpr size_t FPR_SIZE = ArchTraits<Arch::AARCH64>::POINTER_SIZE;
    static constexpr bool HARDFP = true;
};

template <>
struct ExtArchTraits<Arch::X86_64> {
    using signed_word_type = int64_t;
    using unsigned_word_type = uint64_t;

    static constexpr size_t NUM_GP_ARG_REGS = 6;
    static constexpr size_t GP_ARG_NUM_BYTES = NUM_GP_ARG_REGS * ArchTraits<Arch::X86_64>::POINTER_SIZE;
    static constexpr size_t NUM_FP_ARG_REGS = 8;
    static constexpr size_t FP_ARG_NUM_BYTES = NUM_FP_ARG_REGS * ArchTraits<Arch::X86_64>::POINTER_SIZE;
    static constexpr size_t GPR_SIZE = ArchTraits<Arch::X86_64>::POINTER_SIZE;
    static constexpr size_t FPR_SIZE = ArchTraits<Arch::X86_64>::POINTER_SIZE;
    static constexpr bool HARDFP = true;
};

template <class T>
inline uint8_t *AlignPtr(uint8_t *ptr)
{
    return reinterpret_cast<uint8_t *>(RoundUp(reinterpret_cast<uintptr_t>(ptr), sizeof(T)));
}

template <class T>
inline const uint8_t *AlignPtr(const uint8_t *ptr)
{
    return reinterpret_cast<const uint8_t *>(RoundUp(reinterpret_cast<uintptr_t>(ptr), sizeof(T)));
}

template <Arch A>
class ArgCounter {
public:
    template <class T>
    void Count()
    {
        constexpr size_t PTR_SIZE = ArchTraits<A>::POINTER_SIZE;
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (ExtArchTraits<A>::HARDFP) {
                size_t num_bytes = std::max(sizeof(T), ExtArchTraits<A>::FPR_SIZE);
                fpr_arg_size_ = RoundUp(fpr_arg_size_, num_bytes);
                if (fpr_arg_size_ < ExtArchTraits<A>::FP_ARG_NUM_BYTES) {
                    fpr_arg_size_ += num_bytes;
                } else {
                    stack_size_ = RoundUp(stack_size_, num_bytes);
                    stack_size_ += num_bytes;
                }
                return;
            }
        }

        size_t num_bytes = std::max(sizeof(T), PTR_SIZE);
        gpr_arg_size_ = RoundUp(gpr_arg_size_, num_bytes);
        if (gpr_arg_size_ < ExtArchTraits<A>::GP_ARG_NUM_BYTES) {
            gpr_arg_size_ += num_bytes;
        } else {
            stack_size_ = RoundUp(stack_size_, num_bytes);
            stack_size_ += num_bytes;
        }
    }

    size_t GetStackSize() const
    {
        return GetStackSpaceSize() / ArchTraits<A>::POINTER_SIZE;
    }

    size_t GetStackSpaceSize() const
    {
        return RoundUp(ExtArchTraits<A>::FP_ARG_NUM_BYTES + ExtArchTraits<A>::GP_ARG_NUM_BYTES + stack_size_,
                       2 * ArchTraits<A>::POINTER_SIZE);
    }

private:
    size_t gpr_arg_size_ = 0;
    size_t fpr_arg_size_ = 0;
    size_t stack_size_ = 0;
};

template <Arch A>
class ArgReader {
public:
    ArgReader(const Span<uint8_t> &gpr_args, const Span<uint8_t> &fpr_args, const uint8_t *stack_args)
        : gpr_args_(gpr_args), fpr_args_(fpr_args), stack_args_(stack_args)
    {
    }

    template <class T>
    T Read()
    {
        return *ReadPtr<T>();
    }

    template <class T>
    const T *ReadPtr()
    {
        constexpr size_t PTR_SIZE = ArchTraits<A>::POINTER_SIZE;
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_same<T, double>::value || std::is_same<T, float>::value) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (ExtArchTraits<A>::HARDFP) {
                const T *v;
                size_t read_bytes = std::max(sizeof(T), ExtArchTraits<A>::FPR_SIZE);
                fp_arg_bytes_read_ = RoundUp(fp_arg_bytes_read_, read_bytes);
                if (fp_arg_bytes_read_ < ExtArchTraits<A>::FP_ARG_NUM_BYTES) {
                    v = reinterpret_cast<const T *>(fpr_args_.data() + fp_arg_bytes_read_);
                    fp_arg_bytes_read_ += read_bytes;
                } else {
                    stack_args_ = AlignPtr<T>(stack_args_);
                    v = reinterpret_cast<const T *>(stack_args_);
                    stack_args_ += read_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
                return v;
            }
        }
        size_t read_bytes = std::max(sizeof(T), PTR_SIZE);
        gp_arg_bytes_read_ = RoundUp(gp_arg_bytes_read_, read_bytes);
        const T *v;
        if (gp_arg_bytes_read_ < ExtArchTraits<A>::GP_ARG_NUM_BYTES) {
            v = reinterpret_cast<const T *>(gpr_args_.data() + gp_arg_bytes_read_);
            gp_arg_bytes_read_ += read_bytes;
        } else {
            stack_args_ = AlignPtr<T>(stack_args_);
            v = reinterpret_cast<const T *>(stack_args_);
            stack_args_ += read_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        return v;
    }

private:
    const Span<uint8_t> &gpr_args_;
    const Span<uint8_t> &fpr_args_;
    const uint8_t *stack_args_;
    size_t gp_arg_bytes_read_ = 0;
    size_t fp_arg_bytes_read_ = 0;
};

template <Arch A>
class ArgWriter {
public:
    ArgWriter(Span<uint8_t> *gpr_args, Span<uint8_t> *fpr_args, uint8_t *stack_args)
        : gpr_args_(gpr_args), fpr_args_(fpr_args), stack_args_(stack_args)
    {
    }

    template <class T>
    void Write(T v)
    {
        constexpr size_t PTR_SIZE = ArchTraits<A>::POINTER_SIZE;
        size_t write_bytes = std::max(sizeof(T), PTR_SIZE);
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (ExtArchTraits<A>::HARDFP) {
                size_t num_bytes = std::max(sizeof(T), ExtArchTraits<A>::FPR_SIZE);
                if (fp_arg_bytes_written_ < ExtArchTraits<A>::FP_ARG_NUM_BYTES) {
                    *reinterpret_cast<T *>(fpr_args_->data() + fp_arg_bytes_written_) = v;
                    fp_arg_bytes_written_ += num_bytes;
                } else {
                    stack_args_ = AlignPtr<T>(stack_args_);
                    *reinterpret_cast<T *>(stack_args_) = v;
                    stack_args_ += write_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
                return;
            }
        }
        gp_arg_bytes_written_ = RoundUp(gp_arg_bytes_written_, write_bytes);
        if (gp_arg_bytes_written_ < ExtArchTraits<A>::GP_ARG_NUM_BYTES) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (std::is_integral<T>::value && sizeof(T) < PTR_SIZE) {
                // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
                if constexpr (std::is_signed<T>::value) {
                    *reinterpret_cast<typename ExtArchTraits<A>::signed_word_type *>(gpr_args_->data() +
                                                                                     gp_arg_bytes_written_) = v;
                } else {  // NOLINT(readability-misleading-indentation)
                    *reinterpret_cast<typename ExtArchTraits<A>::unsigned_word_type *>(gpr_args_->data() +
                                                                                       gp_arg_bytes_written_) = v;
                }
            } else {  // NOLINT(readability-misleading-indentation)
                *reinterpret_cast<T *>(gpr_args_->data() + gp_arg_bytes_written_) = v;
            }
            gp_arg_bytes_written_ += write_bytes;
        } else {
            stack_args_ = AlignPtr<T>(stack_args_);
            *reinterpret_cast<T *>(stack_args_) = v;
            stack_args_ += write_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

private:
    Span<uint8_t> *gpr_args_;
    Span<uint8_t> *fpr_args_;
    uint8_t *stack_args_;
    size_t gp_arg_bytes_written_ = 0;
    size_t fp_arg_bytes_written_ = 0;
};

// This class is required due to specific calling conventions in AARCH32
template <>
class ArgWriter<Arch::AARCH32> {
public:
    ArgWriter(Span<uint8_t> *gpr_args, Span<uint8_t> *fpr_args, uint8_t *stack_args)
        : gpr_args_(gpr_args), fpr_args_(fpr_args), stack_args_(stack_args)
    {
    }

    template <class T>
    void Write(T v)  // CODECHECK-NOLINT(C_RULE_ID_FUNCTION_SIZE)
    {
        constexpr size_t PTR_SIZE = ArchTraits<Arch::AARCH32>::POINTER_SIZE;
        size_t write_bytes = std::max(sizeof(T), PTR_SIZE);
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (ExtArchTraits<Arch::AARCH32>::HARDFP) {
                size_t num_bytes = std::max(sizeof(T), ExtArchTraits<Arch::AARCH32>::FPR_SIZE);

                if (fp_arg_bytes_written_ < ExtArchTraits<Arch::AARCH32>::FP_ARG_NUM_BYTES &&
                    (std::is_same<T, float>::value ||
                     (fp_arg_bytes_written_ < ExtArchTraits<Arch::AARCH32>::FP_ARG_NUM_BYTES - sizeof(float))) &&
                    !is_float_arm_stack_has_been_written_) {
                    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                    if (std::is_same<T, float>::value) {
                        if (half_empty_register_offset_ == 0) {
                            half_empty_register_offset_ = fp_arg_bytes_written_ + sizeof(float);
                            *reinterpret_cast<T *>(fpr_args_->data() + fp_arg_bytes_written_) = v;
                            fp_arg_bytes_written_ += num_bytes;
                        } else {
                            *reinterpret_cast<T *>(fpr_args_->data() + half_empty_register_offset_) = v;
                            if (half_empty_register_offset_ == fp_arg_bytes_written_) {
                                fp_arg_bytes_written_ += num_bytes;
                            }
                            half_empty_register_offset_ = 0;
                        }
                    } else {
                        fp_arg_bytes_written_ = RoundUp(fp_arg_bytes_written_, sizeof(T));
                        *reinterpret_cast<T *>(fpr_args_->data() + fp_arg_bytes_written_) = v;
                        fp_arg_bytes_written_ += num_bytes;
                    }
                } else {
                    is_float_arm_stack_has_been_written_ = true;
                    stack_args_ = AlignPtr<T>(stack_args_);
                    *reinterpret_cast<T *>(stack_args_) = v;
                    stack_args_ += write_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
                return;
            }
        }
        gp_arg_bytes_written_ = RoundUp(gp_arg_bytes_written_, write_bytes);
        if (gp_arg_bytes_written_ < ExtArchTraits<Arch::AARCH32>::GP_ARG_NUM_BYTES) {
            // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
            if constexpr (std::is_integral<T>::value && sizeof(T) < PTR_SIZE) {
                // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
                if constexpr (std::is_signed<T>::value) {
                    *reinterpret_cast<typename ExtArchTraits<Arch::AARCH32>::signed_word_type *>(
                        gpr_args_->data() + gp_arg_bytes_written_) = v;
                } else {  // NOLINT(readability-misleading-indentation)
                    *reinterpret_cast<typename ExtArchTraits<Arch::AARCH32>::unsigned_word_type *>(
                        gpr_args_->data() + gp_arg_bytes_written_) = v;
                }
            } else {  // NOLINT(readability-misleading-indentation)
                *reinterpret_cast<T *>(gpr_args_->data() + gp_arg_bytes_written_) = v;
            }
            gp_arg_bytes_written_ += write_bytes;
        } else {
            stack_args_ = AlignPtr<T>(stack_args_);
            *reinterpret_cast<T *>(stack_args_) = v;
            stack_args_ += write_bytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

private:
    Span<uint8_t> *gpr_args_;
    Span<uint8_t> *fpr_args_;
    uint8_t *stack_args_;
    size_t gp_arg_bytes_written_ = 0;
    size_t fp_arg_bytes_written_ = 0;
    size_t half_empty_register_offset_ = 0;
    bool is_float_arm_stack_has_been_written_ = false;
};

}  // namespace panda::arch

#endif  // PANDA_RUNTIME_ARCH_HELPERS_H_
