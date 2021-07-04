// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/timesrv/common.h>
#include <kernel/types/KProcess.h>

#include "IHardwareOpusDecoder.h"
#include "results.h"

namespace skyline::service::codec {
    size_t CalculateOutBufferSize(i32 sampleRate, i32 channelCount, i32 frameSize) {
        return util::AlignUp(frameSize * channelCount / (OpusFullbandSampleRate / sampleRate), 0x40);
    }

    IHardwareOpusDecoder::IHardwareOpusDecoder(const DeviceState &state, ServiceManager &manager, i32 sampleRate, i32 channelCount, u32 workBufferSize, KHandle kWorkBuffer)
    : BaseService(state, manager),
      sampleRate(sampleRate),
      channelCount(channelCount),
      workBuffer(state.process->GetHandle<kernel::type::KTransferMemory>(kWorkBuffer)),
      decoderOutputBufferSize(CalculateOutBufferSize(sampleRate, channelCount, MaxFrameSizeNormal)) {
        if (workBufferSize < decoderOutputBufferSize)
            throw OpusException("Bad memory allocation: not enought memory.");

        decoderState = reinterpret_cast<OpusDecoder*>(workBuffer->kernel.ptr);

        int result{opus_decoder_init(decoderState, sampleRate, channelCount)};
        if (result != OPUS_OK)
            throw OpusException(result);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return DecodeInterleavedImpl(request, response);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedWithPerfOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return DecodeInterleavedImpl(request, response, true);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedWithPerfAndResetOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return DecodeInterleaved(session, request, response);
    }

    Result IHardwareOpusDecoder::DecodeInterleaved(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto reset{request.Pop<bool>()};
        if (reset)
            ResetContext();

        return DecodeInterleavedImpl(request, response, true);
    }

    void IHardwareOpusDecoder::ResetContext() {
        opus_decoder_ctl(decoderState, OPUS_RESET_STATE);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedImpl(ipc::IpcRequest &request, ipc::IpcResponse &response, bool performanceInfo) {
        auto dataIn{request.inputBuf.at(0)};
        auto dataOut{request.outputBuf.at(0).cast<opus_int16>()};

        if (dataIn.size() <= sizeof(OpusDataHeader))
            throw OpusException("Incorrect Opus packet size.");

        u32 opusPacketSize{dataIn.as<OpusDataHeader>().GetPacketSize()};
        u32 requiredInSize{static_cast<u32>(opusPacketSize + sizeof(OpusDataHeader))};
        if (opusPacketSize > MaxInputBufferSize || dataIn.size() < requiredInSize)
            throw OpusException(OPUS_BUFFER_TOO_SMALL);

        // Subtract header from the input buffer to get the opus packet
        auto sampleDataIn = dataIn.subspan(sizeof(OpusDataHeader));

        auto perfTimer{timesrv::TimeSpanType::FromNanoseconds(util::GetTimeNs())};
        i32 decodedCount{opus_decode(decoderState, sampleDataIn.data(), opusPacketSize, dataOut.data(), decoderOutputBufferSize, false)};
        perfTimer = timesrv::TimeSpanType::FromNanoseconds(util::GetTimeNs()) - perfTimer;

        if (decodedCount < 0)
            throw OpusException(decodedCount);

        // Decoding is successful, decoded data size is opus packet size + header
        response.Push(requiredInSize);
        response.Push(decodedCount);
        if (performanceInfo)
            response.Push<u64>(perfTimer.Microseconds());

        return {};
    }
}
