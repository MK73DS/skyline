// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/timesrv/common.h>

#include "IHardwareOpusDecoder.h"
#include "results.h"

namespace skyline::service::codec {
    size_t CalculateOutBufferSize(i32 sampleRate, i32 channelCount, i32 frameSize) {
        return util::AlignUp(frameSize * channelCount / (OpusFullbandSampleRate / sampleRate), 0x40);
    }

    IHardwareOpusDecoder::IHardwareOpusDecoder(const DeviceState &state, ServiceManager &manager, i32 sampleRate, i32 channelCount) : BaseService(state, manager), sampleRate(sampleRate), channelCount(channelCount), decoderOutputBufferSize(CalculateOutBufferSize(sampleRate, channelCount, MaxFrameSizeNormal)) {
        int result;
        decoderState = opus_decoder_create(sampleRate, channelCount, &result);
        if (result != OPUS_OK)
            throw OpusException(result);
    }

    IHardwareOpusDecoder::~IHardwareOpusDecoder() {
        opus_decoder_destroy(decoderState);
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
