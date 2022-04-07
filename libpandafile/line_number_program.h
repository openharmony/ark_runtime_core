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

#ifndef LIBPANDAFILE_LINE_NUMBER_PROGRAM_H
#define LIBPANDAFILE_LINE_NUMBER_PROGRAM_H

#include "file-inl.h"
#include "file_items.h"

namespace panda::panda_file {

class LineProgramState {
public:
    LineProgramState(const File &pf, File::EntityId file, size_t line, Span<const uint8_t> constant_pool)
        : pf_(pf), file_(file), line_(line), constant_pool_(constant_pool)
    {
    }

    void AdvanceLine(int32_t v)
    {
        line_ += v;
    }

    void AdvancePc(uint32_t v)
    {
        address_ += v;
    }

    void SetFile(uint32_t offset)
    {
        file_ = File::EntityId(offset);
    }

    const uint8_t *GetFile() const
    {
        return pf_.GetStringData(file_).data;
    }

    bool HasFile() const
    {
        return file_.IsValid();
    }

    void SetSourceCode(uint32_t offset)
    {
        source_code_ = File::EntityId(offset);
    }

    const uint8_t *GetSourceCode() const
    {
        return pf_.GetStringData(source_code_).data;
    }

    bool HasSourceCode() const
    {
        return source_code_.IsValid();
    }

    size_t GetLine() const
    {
        return line_;
    }

    void SetColumn(int32_t c)
    {
        column_ = c;
    }

    size_t GetColumn() const
    {
        return column_;
    }

    uint32_t GetAddress() const
    {
        return address_;
    }

    uint32_t ReadULeb128()
    {
        return panda_file::helpers::ReadULeb128(&constant_pool_);
    }

    int32_t ReadSLeb128()
    {
        return panda_file::helpers::ReadLeb128(&constant_pool_);
    }

    const File &GetPandaFile() const
    {
        return pf_;
    }

private:
    const File &pf_;

    File::EntityId file_;
    File::EntityId source_code_;
    size_t line_;
    size_t column_ {0};
    Span<const uint8_t> constant_pool_;

    uint32_t address_ {0};
};

template <class Handler>
class LineNumberProgramProcessor {
public:
    LineNumberProgramProcessor(const uint8_t *program, Handler *handler)
        : state_(handler->GetState()), program_(program), handler_(handler)
    {
    }

    ~LineNumberProgramProcessor() = default;

    NO_COPY_SEMANTIC(LineNumberProgramProcessor);
    NO_MOVE_SEMANTIC(LineNumberProgramProcessor);

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    void Process()
    {
        handler_->ProcessBegin();

        auto opcode = ReadOpcode();
        bool res = false;
        while (opcode != Opcode::END_SEQUENCE) {
            switch (opcode) {
                case Opcode::ADVANCE_LINE: {
                    res = HandleAdvanceLine();
                    break;
                }
                case Opcode::ADVANCE_PC: {
                    res = HandleAdvancePc();
                    break;
                }
                case Opcode::SET_FILE: {
                    res = HandleSetFile();
                    break;
                }
                case Opcode::SET_SOURCE_CODE: {
                    res = HandleSetSourceCode();
                    break;
                }
                case Opcode::SET_PROLOGUE_END:
                case Opcode::SET_EPILOGUE_BEGIN:
                    break;
                case Opcode::START_LOCAL: {
                    res = HandleStartLocal();
                    break;
                }
                case Opcode::START_LOCAL_EXTENDED: {
                    res = HandleStartLocalExtended();
                    break;
                }
                case Opcode::RESTART_LOCAL: {
                    LOG(FATAL, PANDAFILE) << "Opcode RESTART_LOCAL is not supported";
                    break;
                }
                case Opcode::END_LOCAL: {
                    res = HandleEndLocal();
                    break;
                }
                case Opcode::SET_COLUMN: {
                    HandleSetColumn();
                    break;
                }
                default: {
                    res = HandleSpecialOpcode(opcode);
                    break;
                }
            }

            if (!res) {
                break;
            }

            opcode = ReadOpcode();
        }

        handler_->ProcessEnd();
    }

private:
    using Opcode = LineNumberProgramItem::Opcode;

    Opcode ReadOpcode()
    {
        auto opcode = static_cast<Opcode>(*program_);
        ++program_;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return opcode;
    }

    int32_t ReadRegisterNumber()
    {
        auto [regiser_number, n, is_full] = leb128::DecodeSigned<int32_t>(program_);
        LOG_IF(!is_full, FATAL, COMMON) << "Cannot read a register number";
        program_ += n;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return regiser_number;
    }

    bool HandleAdvanceLine() const
    {
        auto line_diff = state_->ReadSLeb128();
        return handler_->HandleAdvanceLine(line_diff);
    }

    bool HandleAdvancePc() const
    {
        auto pc_diff = state_->ReadULeb128();
        return handler_->HandleAdvancePc(pc_diff);
    }

    bool HandleSetFile() const
    {
        return handler_->HandleSetFile(state_->ReadULeb128());
    }

    bool HandleSetSourceCode() const
    {
        return handler_->HandleSetSourceCode(state_->ReadULeb128());
    }

    bool HandleSetPrologueEnd() const
    {
        return handler_->HandleSetPrologueEnd();
    }

    bool HandleSetEpilogueBegin() const
    {
        return handler_->HandleSetEpilogueBegin();
    }

    bool HandleStartLocal()
    {
        auto reg_number = ReadRegisterNumber();
        auto name_index = state_->ReadULeb128();
        auto type_index = state_->ReadULeb128();
        return handler_->HandleStartLocal(reg_number, name_index, type_index);
    }

    bool HandleStartLocalExtended()
    {
        auto reg_number = ReadRegisterNumber();
        auto name_index = state_->ReadULeb128();
        auto type_index = state_->ReadULeb128();
        auto type_signature_index = state_->ReadULeb128();
        return handler_->HandleStartLocalExtended(reg_number, name_index, type_index, type_signature_index);
    }

    bool HandleEndLocal()
    {
        auto reg_number = ReadRegisterNumber();
        return handler_->HandleEndLocal(reg_number);
    }

    bool HandleSetColumn()
    {
        auto cn = state_->ReadULeb128();
        return handler_->HandleSetColumn(cn);
    }

    bool HandleSpecialOpcode(LineNumberProgramItem::Opcode opcode)
    {
        ASSERT(static_cast<uint8_t>(opcode) >= LineNumberProgramItem::OPCODE_BASE);

        auto adjust_opcode = static_cast<uint8_t>(opcode) - LineNumberProgramItem::OPCODE_BASE;
        uint32_t pc_offset = adjust_opcode / LineNumberProgramItem::LINE_RANGE;
        int32_t line_offset = adjust_opcode % LineNumberProgramItem::LINE_RANGE + LineNumberProgramItem::LINE_BASE;
        return handler_->HandleSpecialOpcode(pc_offset, line_offset);
    }

    LineProgramState *state_;
    const uint8_t *program_;
    Handler *handler_;
};

}  // namespace panda::panda_file

#endif  // LIBPANDAFILE_LINE_NUMBER_PROGRAM_H
