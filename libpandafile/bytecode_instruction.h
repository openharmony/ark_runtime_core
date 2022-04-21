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

#ifndef PANDA_LIBPANDAFILE_BYTECODE_INSTRUCTION_H_
#define PANDA_LIBPANDAFILE_BYTECODE_INSTRUCTION_H_

#include "file.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "utils/bit_helpers.h"

namespace panda {

enum class BytecodeInstMode { FAST, SAFE };

template <const BytecodeInstMode>
class BytecodeInstBase;

class BytecodeId {
public:
    constexpr explicit BytecodeId(uint32_t id) : id_(id) {}

    constexpr BytecodeId() = default;

    ~BytecodeId() = default;

    DEFAULT_COPY_SEMANTIC(BytecodeId);
    NO_MOVE_SEMANTIC(BytecodeId);

    panda_file::File::Index AsIndex() const
    {
        ASSERT(id_ < std::numeric_limits<uint16_t>::max());
        return id_;
    }

    panda_file::File::EntityId AsFileId() const
    {
        return panda_file::File::EntityId(id_);
    }

    bool IsValid() const
    {
        return id_ != INVALID;
    }

    bool operator==(BytecodeId id) const noexcept
    {
        return id_ == id.id_;
    }

    friend std::ostream &operator<<(std::ostream &stream, BytecodeId id)
    {
        return stream << id.id_;
    }

private:
    static constexpr size_t INVALID = std::numeric_limits<uint32_t>::max();

    uint32_t id_ {INVALID};
};

template <>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class BytecodeInstBase<BytecodeInstMode::FAST> {
public:
    BytecodeInstBase() = default;
    explicit BytecodeInstBase(const uint8_t *pc) : pc_ {pc} {}
    ~BytecodeInstBase() = default;

protected:
    const uint8_t *GetPointer(int32_t offset) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return pc_ + offset;
    }

    const uint8_t *GetAddress() const
    {
        return pc_;
    }

    const uint8_t *GetAddress() volatile const
    {
        return pc_;
    }

    template <class T>
    T Read(size_t offset) const
    {
        using unaligned_type __attribute__((aligned(1))) = const T;
        return *reinterpret_cast<unaligned_type *>(GetPointer(offset));
    }

    uint8_t ReadByte(size_t offset) const
    {
        return Read<uint8_t>(offset);
    }

private:
    const uint8_t *pc_ {nullptr};
};

template <>
class BytecodeInstBase<BytecodeInstMode::SAFE> {
public:
    BytecodeInstBase() = default;
    explicit BytecodeInstBase(const uint8_t *pc, const uint8_t *from, const uint8_t *to)
        : pc_ {pc}, from_ {from}, to_ {to}, valid_ {true}
    {
        ASSERT(from_ <= to_);
        ASSERT(pc_ >= from_);
        ASSERT(pc_ <= to_);
    }

protected:
    const uint8_t *GetPointer(int32_t offset) const
    {
        return GetPointer(offset, 1);
    }

    bool IsLast(size_t size) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const uint8_t *ptr_next = pc_ + size;
        return ptr_next > to_;
    }

    const uint8_t *GetPointer(int32_t offset, size_t size) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const uint8_t *ptr_from = pc_ + offset;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const uint8_t *ptr_to = ptr_from + size - 1;
        if (from_ == nullptr || ptr_from < from_ || ptr_to > to_) {
            valid_ = false;
            return from_;
        }
        return ptr_from;
    }

    const uint8_t *GetAddress() const
    {
        return pc_;
    }

    const uint8_t *GetFrom() const
    {
        return from_;
    }

    const uint8_t *GetTo() const
    {
        return to_;
    }

    uint32_t GetOffset() const
    {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pc_) - reinterpret_cast<uintptr_t>(from_));
    }

    const uint8_t *GetAddress() volatile const
    {
        return pc_;
    }

    template <class T>
    T Read(size_t offset) const
    {
        using unaligned_type __attribute__((aligned(1))) = const T;
        auto ptr = reinterpret_cast<unaligned_type *>(GetPointer(offset, sizeof(T)));
        if (IsValid()) {
            return *ptr;
        }
        return {};
    }

    bool IsValid() const
    {
        return valid_;
    }

private:
    const uint8_t *pc_ {nullptr};
    const uint8_t *from_ {nullptr};
    const uint8_t *to_ {nullptr};
    mutable bool valid_ {false};
};

template <const BytecodeInstMode Mode = BytecodeInstMode::FAST>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class BytecodeInst : public BytecodeInstBase<Mode> {
    using Base = BytecodeInstBase<Mode>;

public:
#include <bytecode_instruction_enum_gen.h>

    BytecodeInst() = default;

    ~BytecodeInst() = default;

    template <const BytecodeInstMode M = Mode, typename = std::enable_if_t<M == BytecodeInstMode::FAST>>
    explicit BytecodeInst(const uint8_t *pc) : Base {pc}
    {
    }

    template <const BytecodeInstMode M = Mode, typename = std::enable_if_t<M == BytecodeInstMode::SAFE>>
    explicit BytecodeInst(const uint8_t *pc, const uint8_t *from, const uint8_t *to) : Base {pc, from, to}
    {
    }

    template <Format format, size_t idx = 0>
    BytecodeId GetId() const;

    template <Format format, size_t idx = 0>
    uint16_t GetVReg() const;

    template <Format format, size_t idx = 0>
    auto GetImm() const;

    BytecodeId GetId(size_t idx = 0) const;

    uint16_t GetVReg(size_t idx = 0) const;

    // Read imm and return it as int64_t/uint64_t
    auto GetImm64(size_t idx = 0) const;

    BytecodeInst::Opcode GetOpcode() const;

    uint8_t GetPrimaryOpcode() const
    {
        return static_cast<unsigned>(GetOpcode()) & std::numeric_limits<uint8_t>::max();
    }

    bool IsPrimaryOpcodeValid() const;

    uint8_t GetSecondaryOpcode() const
    {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        return (static_cast<unsigned>(GetOpcode()) >> std::numeric_limits<uint8_t>::digits) &
               std::numeric_limits<uint8_t>::max();
    }

    template <const BytecodeInstMode M = Mode>
    auto JumpTo(int32_t offset) const -> std::enable_if_t<M == BytecodeInstMode::FAST, BytecodeInst>
    {
        return BytecodeInst(Base::GetPointer(offset));
    }

    template <const BytecodeInstMode M = Mode>
    auto JumpTo(int32_t offset) const -> std::enable_if_t<M == BytecodeInstMode::SAFE, BytecodeInst>
    {
        if (!IsValid()) {
            return {};
        }
        const uint8_t *ptr = Base::GetPointer(offset);
        if (!IsValid()) {
            return {};
        }
        return BytecodeInst(ptr, Base::GetFrom(), Base::GetTo());
    }

    template <const BytecodeInstMode M = Mode>
    auto IsLast() const -> std::enable_if_t<M == BytecodeInstMode::SAFE, bool>
    {
        return Base::IsLast(GetSize());
    }

    template <const BytecodeInstMode M = Mode>
    auto IsValid() const -> std::enable_if_t<M == BytecodeInstMode::SAFE, bool>
    {
        return Base::IsValid();
    }

    template <Format format>
    BytecodeInst GetNext() const
    {
        return JumpTo(Size(format));
    }

    BytecodeInst GetNext() const
    {
        return JumpTo(GetSize());
    }

    const uint8_t *GetAddress() const
    {
        return Base::GetAddress();
    }

    const uint8_t *GetAddress() volatile const
    {
        return Base::GetAddress();
    }

    template <const BytecodeInstMode M = Mode>
    auto GetFrom() const -> std::enable_if_t<M == BytecodeInstMode::SAFE, const uint8_t *>
    {
        return Base::GetFrom();
    }

    template <const BytecodeInstMode M = Mode>
    auto GetTo() const -> std::enable_if_t<M == BytecodeInstMode::SAFE, const uint8_t *>
    {
        return Base::GetTo();
    }

    template <const BytecodeInstMode M = Mode>
    auto GetOffset() const -> std::enable_if_t<M == BytecodeInstMode::SAFE, uint32_t>
    {
        return Base::GetOffset();
    }

    uint8_t ReadByte(size_t offset) const
    {
        return Base::template Read<uint8_t>(offset);
    }

    template <class R, class S>
    auto ReadHelper(size_t byteoffset, size_t bytecount, size_t offset, size_t width) const;

    template <size_t offset, size_t width, bool is_signed = false>
    auto Read() const;

    template <bool is_signed = false>
    auto Read64(size_t offset, size_t width) const;

    size_t GetSize() const;

    Format GetFormat() const;

    bool HasFlag(Flags flag) const;

    bool CanThrow() const;

    bool IsTerminator() const
    {
        return HasFlag(Flags::RETURN) || HasFlag(Flags::JUMP) || (GetOpcode() == Opcode::THROW_V8);
    }

    static constexpr bool HasId(Format format, size_t idx);

    static constexpr bool HasVReg(Format format, size_t idx);

    static constexpr bool HasImm(Format format, size_t idx);

    static constexpr size_t Size(Format format);
};

template <const BytecodeInstMode Mode>
std::ostream &operator<<(std::ostream &os, const BytecodeInst<Mode> &inst);

using BytecodeInstruction = BytecodeInst<BytecodeInstMode::FAST>;
using BytecodeInstructionSafe = BytecodeInst<BytecodeInstMode::SAFE>;

}  // namespace panda

#endif  // PANDA_LIBPANDAFILE_BYTECODE_INSTRUCTION_H_
