#pragma once

#include "concurrency/MpmcPublish.h"
#include "rtp/RtcpHeader.h"
#include "transport/PacketCounters.h"
#include <chrono>

namespace transport
{
struct ReportSummary
{
    bool empty() const { return packets == 0 && sequenceNumberSent == 0; }

    uint64_t getRtt() const { return (static_cast<uint64_t>(rttNtp) * utils::Time::sec) >> 16; }
    uint64_t packets = 0;
    uint64_t lostPackets = 0;
    double lossFraction = 0;
    uint32_t extendedSeqNoReceived = 0;
    uint32_t sequenceNumberSent = 0;
    uint32_t rttNtp = 0;
};

class RtpSenderState
{
public:
    explicit RtpSenderState(uint32_t rtpFrequency);

    void onRtpSent(uint64_t timestamp, memory::Packet& packet);
    void onReceiverBlockReceived(uint64_t timestamp, uint32_t wallClockNtp32, const rtp::ReportBlock& report);

    ReportSummary getSummary() const;
    PacketCounters getCounters();

    uint32_t getRttNtp() const;
    uint64_t getLastSendTime() const { return _sendTime; }

    void fillInReport(rtp::RtcpSenderReport& report, uint64_t timestamp, uint64_t wallClockNtp) const;

    void setRtpFrequency(uint32_t rtpFrequency);

    std::atomic_uint8_t payloadType;

    struct SendCounters
    {
        uint32_t packets = 0;
        uint32_t sequenceNumber = 0;
        uint64_t octets = 0;
        uint64_t timestamp = 0;
    };

private:
    uint32_t getRtpTimestamp(uint64_t timestamp) const;
    struct RemoteCounters
    {
        RemoteCounters()
            : cumulativeLossCount(0),
              rttNtp(0),
              lossFraction(0),
              extendedSeqNoReceived(0),
              timestampNtp32(0)
        {
        }

        uint32_t cumulativeLossCount;
        uint32_t rttNtp;
        double lossFraction;
        uint32_t extendedSeqNoReceived;
        uint32_t timestampNtp32;
        uint64_t timestamp;
    };

    SendCounters _sendCounters;
    SendCounters _sendCounterSnapshot;

    struct
    {
        uint32_t rtp = 0;
        uint64_t local = 0;
    } _rtpTimestampCorrelation;

    uint64_t _sendTime;
    RemoteCounters _remoteReport;

    uint32_t _rtpFrequency;
    concurrency::MpmcPublish<PacketCounters, 4> _counters;
    concurrency::MpmcPublish<ReportSummary, 4> _summary;
    concurrency::MpmcPublish<SendCounters, 4> _sendReport;
};

} // namespace transport