// SPDX-License-Identifier: GPL-3.0-or-later
// From Eglantine (snowstormxd, 894f2c119 "Use the full size of Mario Kart World lighting buffers").
#pragma once

#include <algorithm>
#include "common/common_types.h"
#include "common/literals.h"

namespace VideoCommon {

/// NVN graphics storage slots in constant buffer 0 (16 bytes each, 0x100 per stage from 0x110)
/// hold an address and a size, unlike bare native pointers and pointer tables.
[[nodiscard]] constexpr bool IsNvnGraphicsStorageDescriptor(size_t stage, u32 cbuf_index,
                                                            u32 cbuf_offset, u32 indirect_offset) {
    const u32 base = 0x110 + static_cast<u32>(stage) * 0x100;
    return stage < 5 && cbuf_index == 0 && indirect_offset == ~0U && cbuf_offset >= base &&
           cbuf_offset < base + 0x100 && (cbuf_offset - base) % 0x10 == 0;
}

} // namespace VideoCommon
