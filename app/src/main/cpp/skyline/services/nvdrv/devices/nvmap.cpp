// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvmap.h"

namespace skyline::service::nvdrv::device {
    NvMap::NvMapObject::NvMapObject(u32 id, u32 size) : id(id), size(size) {}

    NvMap::NvMap(const DeviceState &state) : NvDevice(state) {}

    NvStatus NvMap::Create(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 size;   // In
            u32 handle; // Out
        } &data = buffer.as<Data>();

        std::unique_lock lock(mapMutex);
        maps.push_back(std::make_shared<NvMapObject>(idIndex++, data.size));
        data.handle = maps.size();

        state.logger->Debug("Size: 0x{:X} -> Handle: 0x{:X}", data.size, data.handle);
        return NvStatus::Success;
    }

    NvStatus NvMap::FromId(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 id;     // In
            u32 handle; // Out
        } &data = buffer.as<Data>();

        std::shared_lock lock(mapMutex);
        for (auto it{maps.begin()}; it < maps.end(); it++) {
            if ((*it)->id == data.id) {
                data.handle = (it - maps.begin()) + 1;
                state.logger->Debug("ID: 0x{:X} -> Handle: 0x{:X}", data.id, data.handle);
                return NvStatus::Success;
            }
        }

        state.logger->Warn("Handle not found for ID: 0x{:X}", data.id);
        return NvStatus::BadValue;
    }

    NvStatus NvMap::Alloc(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 handle;   // In
            u32 heapMask; // In
            u32 flags;    // In
            u32 align;    // In
            u8 kind;      // In
            u8 _pad0_[7];
            u8 *ptr;      // InOut
        } &data = buffer.as<Data>();

        try {
            auto object{GetObject(data.handle)};
            object->heapMask = data.heapMask;
            object->flags = data.flags;
            object->align = data.align;
            object->kind = data.kind;
            object->ptr = data.ptr;
            object->status = NvMapObject::Status::Allocated;

            state.logger->Debug("Handle: 0x{:X}, HeapMask: 0x{:X}, Flags: {}, Align: 0x{:X}, Kind: {}, Pointer: 0x{:X}", data.handle, data.heapMask, data.flags, data.align, data.kind, data.ptr);
            return NvStatus::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("Invalid NvMap handle: 0x{:X}", data.handle);
            return NvStatus::BadParameter;
        }
    }

    NvStatus NvMap::Free(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 handle;   // In
            u32 _pad0_;
            u8 *ptr;      // Out
            u32 size;     // Out
            u32 flags;    // Out
        } &data = buffer.as<Data>();

        std::unique_lock lock(mapMutex);
        try {
            auto &object{maps.at(data.handle - 1)};
            if (object.use_count() > 1) {
                data.ptr = object->ptr;
                data.flags = 0x0;
            } else {
                data.ptr = nullptr;
                data.flags = 0x1; // Not free yet
            }

            data.size = object->size;
            object = nullptr;

            state.logger->Debug("Handle: 0x{:X} -> Pointer: 0x{:X}, Size: 0x{:X}, Flags: 0x{:X}", data.handle, data.ptr, data.size, data.flags);
            return NvStatus::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("Invalid NvMap handle: 0x{:X}", data.handle);
            return NvStatus::BadParameter;
        }
    }

    NvStatus NvMap::Param(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        // https://android.googlesource.com/kernel/tegra/+/refs/heads/android-tegra-flounder-3.10-marshmallow/include/linux/nvmap.h#102
        enum class Parameter : u32 {
            Size = 1,
            Alignment = 2,
            Base = 3,
            HeapMask = 4,
            Kind = 5,
            Compr = 6,
        };

        struct Data {
            u32 handle;          // In
            Parameter parameter; // In
            u32 result;          // Out
        } &data = buffer.as<Data>();

        try {
            auto object{GetObject(data.handle)};

            switch (data.parameter) {
                case Parameter::Size:
                    data.result = object->size;
                    break;

                case Parameter::Alignment:
                    data.result = object->align;
                    break;

                case Parameter::HeapMask:
                    data.result = object->heapMask;
                    break;

                case Parameter::Kind:
                    data.result = object->kind;
                    break;

                case Parameter::Compr:
                    data.result = 0;
                    break;

                default:
                    state.logger->Warn("Parameter not implemented: 0x{:X}", data.parameter);
                    return NvStatus::NotImplemented;
            }

            state.logger->Debug("Handle: 0x{:X}, Parameter: {} -> Result: 0x{:X}", data.handle, data.parameter, data.result);
            return NvStatus::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("Invalid NvMap handle: 0x{:X}", data.handle);
            return NvStatus::BadParameter;
        }
    }

    NvStatus NvMap::GetId(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 id;     // Out
            u32 handle; // In
        } &data = buffer.as<Data>();

        try {
            data.id = GetObject(data.handle)->id;
            state.logger->Debug("Handle: 0x{:X} -> ID: 0x{:X}", data.handle, data.id);
            return NvStatus::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("Invalid NvMap handle: 0x{:X}", data.handle);
            return NvStatus::BadParameter;
        }
    }
}
