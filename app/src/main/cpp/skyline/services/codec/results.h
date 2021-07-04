// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::codec::result {
    constexpr Result OpusBadArgument(111, 2);
    constexpr Result OpusBufferTooSmall(111, 3);
    constexpr Result OpusInternalError(111, 4);
    constexpr Result OpusInvalidState(111, 5);
    constexpr Result OpusUnimplemented(111, 6);
    constexpr Result OpusAllocationFailed(111, 7);
    constexpr Result BadOpusDataHeader(111, 16);
    constexpr Result OpusInvalidPacket(111, 17);
}
