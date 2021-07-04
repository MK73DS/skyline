// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <opus.h>

#include <common.h>
#include <services/base_service.h>
#include <kernel/types/KTransferMemory.h>

namespace skyline::service::codec {
    /**
     * @brief This structure is located at the beginning of OpusDataIn for DecodeInterleaved*, with the actual opus packet following this
     * @note These fields are big-endian
     * @url https://github.com/switchbrew/libnx/blob/c5a9a909a91657a9818a3b7e18c9b91ff0cbb6e3/nx/include/switch/services/hwopus.h#L19
     */
    struct OpusDataHeader {
        u32 sizeBe; //!< Size of the packet following this header, encoded in big-endian
        u32 finalRangeBe; //!< Indicates the final range of the codec encoder's entropy coder, this can be left at zero, encoded in big-endian

        u32 GetPacketSize() {
            return util::SwapEndianness(sizeBe);
        }
    };
    static_assert(sizeof(OpusDataHeader) == 0x8);

    /**
     * @return The required output buffer size for decoding an Opus stream with the given parameters
     */
    size_t CalculateOutBufferSize(i32 sampleRate, i32 channelCount, i32 frameSize);

    static constexpr u32 OpusFullbandSampleRate{48000};
    static constexpr u32 MaxFrameSizeNormal = OpusFullbandSampleRate * 0.040; //!< 40ms frame size limit for normal decoders
    static constexpr u32 MaxFrameSizeMultiEx = OpusFullbandSampleRate * 0.120; //!< 120ms frame size limit for ex decoders added in 12.0.0
    static constexpr u32 MaxInputBufferSize{0x600}; //!< Maximum allocated size of the input buffer

    /**
     * @brief This service decodes an Opus audio stream. Always created with OpenHardwareOpusDecoder by IHardwareOpusDecoderManager
     * @url https://switchbrew.org/wiki/Audio_services#IHardwareOpusDecoder
     */
    class IHardwareOpusDecoder : public BaseService {
      private:
        std::shared_ptr<kernel::type::KTransferMemory> workBuffer;
        OpusDecoder *decoderState{};
        i32 sampleRate;
        i32 channelCount;
        i32 decoderOutputBufferSize;

        /**
         * @brief Resets the codec state to be equivalent to a freshly initialized state
         */
        void ResetContext();

        /**
         * @brief Called by all DecodeInterleaved* functions
         * @param performanceInfo Whether to return decode time taken or not
         */
        Result DecodeInterleavedImpl(ipc::IpcRequest &request, ipc::IpcResponse &response, bool performanceInfo = false);

      public:
        /**
         * @param sampleRate The sample rate of the Opus audio data
         * @param channelCount The channel count of the Opus audio data
         */
        IHardwareOpusDecoder(const DeviceState &state, ServiceManager &manager, i32 sampleRate, i32 channelCount, u32 workBufferSize, KHandle kWorkBuffer);

        /**
         * @brief Takes an OpusDataIn input buffer and a PcmDataOut output buffer.
         * Decodes the Opus source data to the output buffer and returns decoded data size and decoded sample count
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleavedOld
         */
        Result DecodeInterleavedOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Takes an OpusDataIn input buffer and a PcmDataOut output buffer.
         * Decodes the Opus source data to the output buffer and returns decoded data size, decoded sample count and decode time taken in microseconds
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleavedWithPerfOld
         */
        Result DecodeInterleavedWithPerfOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Same as DecodeInterleaved
         * @see DecodeInterleaved
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleaved
         */
        Result DecodeInterleavedWithPerfAndResetOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Takes an input boolean flag, an OpusDataIn input buffer and a PcmDataOut output buffer.
         * Decodes the Opus source data to the output buffer and returns decoded data size, decoded sample count and decode time taken in microseconds
         * @note The bool flag indicates whether or not a reset of the decoder context is being requested
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleaved
         */
        Result DecodeInterleaved(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x0, IHardwareOpusDecoder, DecodeInterleavedOld),
          SFUNC(0x4, IHardwareOpusDecoder, DecodeInterleavedWithPerfOld),
          SFUNC(0x6, IHardwareOpusDecoder, DecodeInterleavedWithPerfAndResetOld),
          SFUNC(0x8, IHardwareOpusDecoder, DecodeInterleaved),
      )
    };

  class OpusException : public skyline::exception {
    public:
      OpusException(const std::string& message) : skyline::exception(message) {}
      OpusException(int errorCode) : skyline::exception("Opus failed with error code {}: {}", errorCode, opus_strerror(errorCode)) {}
  };
}
