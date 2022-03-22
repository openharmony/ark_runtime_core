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
// Our main goal is to have similar interface for two different platforms - high-end and low-end.
//
// 64 bits object header for high-end devices: (64 bits pointer)
// |--------------------------------------------------------------------------------------|--------------------|
// |                                   Object Header (128 bits)                           |        State       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |                 Mark Word (64 bits)                 |      Class Word (64 bits)      |                    |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         nothing:60         | RB:1 | GC:1 | state:00 |     OOP to metadata object     |       Unlock       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |   tId:29   |   Lcount:31   | RB:1 | GC:1 | state:00 |     OOP to metadata object     |  Lightweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         Monitor:60         | RB:1 | GC:1 | state:01 |     OOP to metadata object     |  Heavyweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |           Hash:60          | RB:1 | GC:1 | state:10 |     OOP to metadata object     |       Hashed       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |           Forwarding address:62          | state:11 |     OOP to metadata object     |         GC         |
// |-----------------------------------------------------|--------------------------------|--------------------|
//
// 64 bits object header for high-end devices: (32 bits pointer)
// |--------------------------------------------------------------------------------------|--------------------|
// |                                   Object Header (64 bits)                            |        State       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |                 Mark Word (32 bits)                 |      Class Word (32 bits)      |                    |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         nothing:28         | RB:1 | GC:1 | state:00 |     OOP to metadata object     |       Unlock       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |   tId:13   |   Lcount:15   | RB:1 | GC:1 | state:00 |     OOP to metadata object     |  Lightweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         Monitor:28         | RB:1 | GC:1 | state:01 |     OOP to metadata object     |  Heavyweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |           Hash:28          | RB:1 | GC:1 | state:10 |     OOP to metadata object     |       Hashed       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |           Forwarding address:30          | state:11 |     OOP to metadata object     |         GC         |
// |-----------------------------------------------------|--------------------------------|--------------------|
//
// However, we can also support such version of the object header: (Hash is stored just after the object in mem)
// |--------------------------------------------------------------------------------------|--------------------|
// |                                   Object Header (64 bits)                            |        State       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |                 Mark Word (32 bits)                 |      Class Word (32 bits)      |                    |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |     nothing:27    | Hash:1 | RB:1 | GC:1 | state:00 |     OOP to metadata object     |       Unlock       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// | tId:13 |LCount:14 | Hash:1 | RB:1 | GC:1 | state:00 |     OOP to metadata object     |  Lightweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |     Monitor:27    | Hash:1 | RB:1 | GC:1 | state:01 |     OOP to metadata object     |  Heavyweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |      Forwarding address:29      | Hash:1 | state:11 |     OOP to metadata object     |         GC         |
// |-----------------------------------------------------|--------------------------------|--------------------|
//
// 32 bits object header for low-end devices:
// |--------------------------------------------------------------------------------------|--------------------|
// |                                   Object Header (32 bits)                            |        State       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |                 Mark Word (16 bits)                 |      Class Word (16 bits)      |                    |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         nothing:12         | RB:1 | GC:1 | state:00 |     OOP to metadata object     |       Unlock       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |   tId:7    |   Lcount:4    | RB:1 | GC:1 | state:00 |     OOP to metadata object     |  Lightweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         Monitor:12         | RB:1 | GC:1 | state:01 |     OOP to metadata object     |  Heavyweight Lock  |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |           Hash:12          | RB:1 | GC:1 | state:10 |     OOP to metadata object     |       Hashed       |
// |-----------------------------------------------------|--------------------------------|--------------------|
// |         Forwarding address:14            | state:11 |     OOP to metadata object     |         GC         |
// |-----------------------------------------------------|--------------------------------|--------------------|
#ifndef PANDA_RUNTIME_MARK_WORD_H_
#define PANDA_RUNTIME_MARK_WORD_H_

#include <cstdint>

#include "libpandabase/os/thread.h"
#include "libpandabase/utils/logger.h"
#include "runtime/monitor.h"
#include "runtime/object_header_config.h"

namespace panda {

// Small helper
template <class Config>
class MarkWordConfig {
public:
    using markWordSize = typename Config::Size;
    static constexpr markWordSize CONFIG_MARK_WORD_BIT_SIZE = Config::BITS;
    static constexpr markWordSize CONFIG_LOCK_THREADID_SIZE = Config::LOCK_THREADID_SIZE;
    static constexpr bool CONFIG_IS_HASH_IN_OBJ_HEADER = Config::IS_HASH_IN_OBJ_HEADER;
    static constexpr markWordSize CONFIG_HASH_STATUS_SIZE = Config::IS_HASH_IN_OBJ_HEADER ? 0 : 1;
};

// One of our main purpose is to create an common interface for both IoT and High-level Mark Word.
// That's why we should always operate with uint32_t and convert it into markWordSize if necessary.

class MarkWord : private MarkWordConfig<MemoryModelConfig> {
public:
    using markWordSize = typename MarkWordConfig::markWordSize;  // To be visible outside

    // Big enum with all useful masks and shifts
    enum MarkWordRepresentation : markWordSize {
        STATUS_SIZE = 2UL,
        GC_STATUS_SIZE = 1UL,
        RB_STATUS_SIZE = 1UL,
        HASH_STATUS_SIZE = CONFIG_HASH_STATUS_SIZE,  // This parameter is used only in special memory model config
        MONITOR_POINTER_SIZE =
            CONFIG_MARK_WORD_BIT_SIZE - STATUS_SIZE - GC_STATUS_SIZE - RB_STATUS_SIZE - HASH_STATUS_SIZE,
        // If we don't have Hash inside an object header, thisThread constant shouldn't be used
        HASH_SIZE = (CONFIG_HASH_STATUS_SIZE != 0UL)
                        ? 0UL
                        : (CONFIG_MARK_WORD_BIT_SIZE - STATUS_SIZE - GC_STATUS_SIZE - RB_STATUS_SIZE),
        FORWARDING_ADDRESS_SIZE = CONFIG_MARK_WORD_BIT_SIZE - STATUS_SIZE - HASH_STATUS_SIZE,

        // Unlocked state masks and shifts
        UNLOCKED_STATE_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - MONITOR_POINTER_SIZE,
        UNLOCKED_STATE_MASK = (1UL << MONITOR_POINTER_SIZE) - 1UL,
        UNLOCKED_STATE_MASK_IN_PLACE = UNLOCKED_STATE_MASK << UNLOCKED_STATE_SHIFT,

        // Lightweight Lock state masks and shifts
        LIGHT_LOCK_THREADID_SIZE = CONFIG_LOCK_THREADID_SIZE,
        LIGHT_LOCK_LOCK_COUNT_SIZE = MONITOR_POINTER_SIZE - LIGHT_LOCK_THREADID_SIZE,

        LIGHT_LOCK_LOCK_COUNT_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - MONITOR_POINTER_SIZE,
        LIGHT_LOCK_LOCK_COUNT_MASK = (1UL << LIGHT_LOCK_LOCK_COUNT_SIZE) - 1UL,
        LIGHT_LOCK_LOCK_COUNT_MASK_IN_PLACE = LIGHT_LOCK_LOCK_COUNT_MASK << LIGHT_LOCK_LOCK_COUNT_SHIFT,
        LIGHT_LOCK_LOCK_MAX_COUNT = LIGHT_LOCK_LOCK_COUNT_MASK,

        LIGHT_LOCK_THREADID_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - MONITOR_POINTER_SIZE + LIGHT_LOCK_LOCK_COUNT_SIZE,
        LIGHT_LOCK_THREADID_MASK = (1UL << LIGHT_LOCK_THREADID_SIZE) - 1UL,
        LIGHT_LOCK_THREADID_MASK_IN_PLACE = LIGHT_LOCK_THREADID_MASK << LIGHT_LOCK_THREADID_SHIFT,
        LIGHT_LOCK_THREADID_MAX_COUNT = LIGHT_LOCK_THREADID_MASK,

        // Heavyweight Lock state masks and shifts
        MONITOR_POINTER_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - MONITOR_POINTER_SIZE,
        MONITOR_POINTER_MASK = (1UL << MONITOR_POINTER_SIZE) - 1UL,
        MONITOR_POINTER_MASK_IN_PLACE = MONITOR_POINTER_MASK << MONITOR_POINTER_SHIFT,
        MONITOR_POINTER_MAX_COUNT = MONITOR_POINTER_MASK,

        // Hash state masks and shifts
        HASH_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - HASH_SIZE,
        HASH_MASK = (1UL << HASH_SIZE) - 1UL,
        HASH_MASK_IN_PLACE = HASH_MASK << HASH_SHIFT,

        // Forwarding state masks and shifts
        FORWARDING_ADDRESS_SHIFT = CONFIG_MARK_WORD_BIT_SIZE - FORWARDING_ADDRESS_SIZE,
        FORWARDING_ADDRESS_MASK = (1UL << FORWARDING_ADDRESS_SIZE) - 1UL,
        FORWARDING_ADDRESS_MASK_IN_PLACE = FORWARDING_ADDRESS_MASK << FORWARDING_ADDRESS_SHIFT,

        // Status bits masks and shifts
        STATUS_SHIFT = 0UL,
        STATUS_MASK = (1UL << STATUS_SIZE) - 1UL,
        STATUS_MASK_IN_PLACE = STATUS_MASK << STATUS_SHIFT,

        // An object status variants
        STATUS_UNLOCKED = 0UL,
        STATUS_LIGHTWEIGHT_LOCK = 0UL,
        STATUS_HEAVYWEIGHT_LOCK = 1UL,
        STATUS_HASHED = 2UL,
        STATUS_GC = 3UL,  // Or Forwarding state

        // Marked for GC bit masks and shifts
        GC_STATUS_SHIFT = STATUS_SIZE,
        GC_STATUS_MASK = (1UL << GC_STATUS_SIZE) - 1UL,
        GC_STATUS_MASK_IN_PLACE = GC_STATUS_MASK << GC_STATUS_SHIFT,

        // Read barrier bit masks and shifts
        RB_STATUS_SHIFT = STATUS_SIZE + GC_STATUS_SIZE,
        RB_STATUS_MASK = (1UL << RB_STATUS_SIZE) - 1UL,
        RB_STATUS_MASK_IN_PLACE = RB_STATUS_MASK << RB_STATUS_SHIFT,

        // Hashed bits masks and shifts
        HASH_STATUS_SHIFT = STATUS_SIZE + GC_STATUS_SIZE + RB_STATUS_SIZE,
        HASH_STATUS_MASK = (1UL << HASH_STATUS_SIZE) - 1UL,
        HASH_STATUS_MASK_IN_PLACE = HASH_STATUS_MASK << HASH_STATUS_SHIFT,
    };

    enum ObjectState {
        STATE_UNLOCKED,
        STATE_LIGHT_LOCKED,
        STATE_HEAVY_LOCKED,
        STATE_HASHED,
        STATE_GC,
    };

    /* Create MarkWord from different objects:
     * (We can't change GC bit)
     */
    MarkWord DecodeFromMonitor(Monitor::MonitorId monitor)
    {
        // Clear monitor and status bits
        markWordSize temp = Value() & (~(MONITOR_POINTER_MASK_IN_PLACE | STATUS_MASK_IN_PLACE));
        markWordSize monitor_in_place = (static_cast<markWordSize>(monitor) & MONITOR_POINTER_MASK)
                                        << MONITOR_POINTER_SHIFT;
        return MarkWord(temp | monitor_in_place | (STATUS_HEAVYWEIGHT_LOCK << STATUS_SHIFT));
    }

    MarkWord DecodeFromHash(uint32_t hash);

    MarkWord DecodeFromForwardingAddress(markWordSize forwarding_address)
    {
        static_assert(sizeof(markWordSize) == OBJECT_POINTER_SIZE,
                      "MarkWord has different size than OBJECT_POINTER_SIZE");
        ASSERT((forwarding_address & FORWARDING_ADDRESS_MASK_IN_PLACE) == forwarding_address);
        return DecodeFromForwardingAddressField(forwarding_address >> FORWARDING_ADDRESS_SHIFT);
    }

    MarkWord DecodeFromLightLock(os::thread::ThreadId thread_id, uint32_t count)
    {
        // Clear monitor and status bits
        markWordSize temp =
            Value() &
            (~(LIGHT_LOCK_THREADID_MASK_IN_PLACE | LIGHT_LOCK_LOCK_COUNT_MASK_IN_PLACE | STATUS_MASK_IN_PLACE));
        markWordSize lightlock_thread_in_place = (static_cast<markWordSize>(thread_id) & LIGHT_LOCK_THREADID_MASK)
                                                 << LIGHT_LOCK_THREADID_SHIFT;
        markWordSize lightlock_lock_count_in_place = (static_cast<markWordSize>(count) & LIGHT_LOCK_LOCK_COUNT_MASK)
                                                     << LIGHT_LOCK_LOCK_COUNT_SHIFT;
        return MarkWord(temp | lightlock_thread_in_place | lightlock_lock_count_in_place |
                        (STATUS_LIGHTWEIGHT_LOCK << STATUS_SHIFT));
    }

    MarkWord DecodeFromUnlocked()
    {
        // Clear monitor and status bits
        markWordSize unlocked = Value() & (~(UNLOCKED_STATE_MASK_IN_PLACE | STATUS_MASK_IN_PLACE));
        return MarkWord(unlocked | (STATUS_UNLOCKED << STATUS_SHIFT));
    }

    bool IsMarkedForGC() const
    {
        return (Value() & GC_STATUS_MASK_IN_PLACE) != 0U;
    }

    bool IsReadBarrierSet() const
    {
        return (Value() & RB_STATUS_MASK_IN_PLACE) != 0U;
    }

    bool IsHashed() const;

    MarkWord SetMarkedForGC()
    {
        return MarkWord((Value() & (~GC_STATUS_MASK_IN_PLACE)) | GC_STATUS_MASK_IN_PLACE);
    }

    MarkWord SetUnMarkedForGC()
    {
        return MarkWord(Value() & (~GC_STATUS_MASK_IN_PLACE));
    }

    MarkWord SetReadBarrier()
    {
        return MarkWord((Value() & (~RB_STATUS_MASK_IN_PLACE)) | RB_STATUS_MASK_IN_PLACE);
    }

    MarkWord ClearReadBarrier()
    {
        return MarkWord(Value() & (~RB_STATUS_MASK_IN_PLACE));
    }

    MarkWord SetHashed();

    ObjectState GetState() const
    {
        switch ((Value() >> STATUS_SHIFT) & STATUS_MASK) {
            case STATUS_HEAVYWEIGHT_LOCK:
                return STATE_HEAVY_LOCKED;
            case STATUS_HASHED:
                return STATE_HASHED;
            case STATUS_GC:
                return STATE_GC;
            case STATUS_UNLOCKED:
                // We should distinguish between Unlocked and Lightweight Lock states:
                return ((Value() & UNLOCKED_STATE_MASK_IN_PLACE) == 0U) ? STATE_UNLOCKED : STATE_LIGHT_LOCKED;
            default:
                LOG(DEBUG, RUNTIME) << "Undefined object state";
                return STATE_GC;
        }
    }

    os::thread::ThreadId GetThreadId() const
    {
        LOG_IF(GetState() != STATE_LIGHT_LOCKED, DEBUG, RUNTIME) << "Wrong State";
        return static_cast<os::thread::ThreadId>((Value() >> LIGHT_LOCK_THREADID_SHIFT) & LIGHT_LOCK_THREADID_MASK);
    }

    uint32_t GetLockCount() const
    {
        LOG_IF(GetState() != STATE_LIGHT_LOCKED, DEBUG, RUNTIME) << "Wrong State";
        return static_cast<uint32_t>((Value() >> LIGHT_LOCK_LOCK_COUNT_SHIFT) & LIGHT_LOCK_LOCK_COUNT_MASK);
    }

    uint32_t GetHash() const;

    markWordSize GetForwardingAddress() const
    {
        LOG_IF(GetState() != STATE_GC, DEBUG, RUNTIME) << "Wrong State";
        return GetForwardingAddressField() << FORWARDING_ADDRESS_SHIFT;
    }

    Monitor::MonitorId GetMonitorId() const
    {
        LOG_IF(GetState() != STATE_HEAVY_LOCKED, DEBUG, RUNTIME) << "Wrong State";
        return static_cast<Monitor::MonitorId>((Value() >> MONITOR_POINTER_SHIFT) & MONITOR_POINTER_MASK);
    }

    markWordSize GetValue() const
    {
        return value_;
    }

    ~MarkWord() = default;

    DEFAULT_COPY_SEMANTIC(MarkWord);
    DEFAULT_MOVE_SEMANTIC(MarkWord);

    friend class MarkWordTest;
    friend class ObjectHeader;

private:
    // The only field in MarkWord class.
    markWordSize value_ {0};

    markWordSize Value() volatile const
    {
        return value_;
    }

    // Functions depend on memory model config
    template <bool HashPolicy>
    uint32_t GetHashConfigured() const;

    template <bool HashPolicy>
    MarkWord DecodeFromHashConfigured(uint32_t hash);

    template <bool HashPolicy>
    bool IsHashedConfigured() const;

    template <bool HashPolicy>
    MarkWord SetHashedConfigured();

    explicit MarkWord(markWordSize value) noexcept : value_(value) {}

    MarkWord() noexcept : value_(0) {}

    bool operator==(const MarkWord &other) const
    {
        return Value() == other.Value();
    }

    /**
     * @param forwarding_address - address shifted by FORWARDING_ADDRESS_SHIFT
     * @return MarkWord with encoded forwarding_address and GC state
     */
    MarkWord DecodeFromForwardingAddressField(markWordSize forwarding_address)
    {
        ASSERT(forwarding_address <= (std::numeric_limits<markWordSize>::max() >> FORWARDING_ADDRESS_SHIFT));
        // Forwardind address consumes all bits except status. We don't need to save GC state
        markWordSize forwarding_address_in_place = (forwarding_address & FORWARDING_ADDRESS_MASK)
                                                   << FORWARDING_ADDRESS_SHIFT;
        return MarkWord(forwarding_address_in_place | (STATUS_GC << STATUS_SHIFT));
    }

    /**
     * @return pointer shifted by FORWARDING_ADDRESS_SHIFT
     */
    markWordSize GetForwardingAddressField() const
    {
        LOG_IF(GetState() != STATE_GC, DEBUG, RUNTIME) << "Wrong State";
        return (Value() >> FORWARDING_ADDRESS_SHIFT) & FORWARDING_ADDRESS_MASK;
    }
};

}  // namespace panda

#endif  // PANDA_RUNTIME_MARK_WORD_H_
