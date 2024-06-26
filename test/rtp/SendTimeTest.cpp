#include "rtp/RtpHeader.h"
#include "rtp/SendTimeDial.h"
#include "utils/Time.h"
#include <cstdint>
#include <gtest/gtest.h>

uint32_t createSendTime(uint64_t timestamp)
{
    const uint64_t GCD = 512; // to minimize shift out
    const uint64_t NOMINATOR = (1 << 18) / GCD;
    const uint64_t DENOMINATOR = utils::Time::sec / GCD;
    return ((timestamp * NOMINATOR) / DENOMINATOR) & 0xFFFFFFu;
}

TEST(RtpSendTimeTest, wrap)
{
    uint64_t start = 982348934;
    uint64_t sendTime;
    rtp::SendTimeDial converter;
    auto now = start + utils::Time::sec * 5;
    for (int i = 0; i < 25000; ++i)
    {
        now += utils::Time::ms * 10;
        sendTime = createSendTime(start + i * utils::Time::ms * 10);
        auto sendTimestamp = converter.toAbsoluteTime(sendTime, now);
        EXPECT_LE(now - sendTimestamp, utils::Time::sec * 15 + utils::Time::ms * 11);
    }
}

TEST(RtpSendTimeTest, reordering)
{
    uint64_t localTimer = 979886;
    uint64_t remoteTimer = 2342334;

    rtp::SendTimeDial converter;
    const auto localTick = utils::Time::sec / 512;
    const auto remoteTick = (1 << 18) / 512;

    auto sendTimestamp = converter.toAbsoluteTime(remoteTimer % (1 << 24), localTimer);
    int64_t offset = localTimer - sendTimestamp;

    for (int i = 0; i < 25000; ++i)
    {
        auto x = rand() % 25;
        localTimer += localTick * x;
        remoteTimer += remoteTick * x;
        auto delta = rand() % 2000;
        auto sendTime = remoteTimer + delta;
        auto sendTimestamp = converter.toAbsoluteTime(sendTime % (1 << 24), localTimer);

        EXPECT_NEAR(localTimer - offset + delta * 3815, sendTimestamp, 3500);
    }
}

TEST(RtpSendTimeTest, corner)
{
    rtp::SendTimeDial converter;
    auto sendTimestamp = converter.toAbsoluteTime(0, 0);
    EXPECT_EQ(sendTimestamp, uint64_t(0) - uint64_t(2 * 1953125));

    sendTimestamp = converter.toAbsoluteTime((1 << 24) - 5, utils::Time::ms * 50);
    EXPECT_EQ(sendTimestamp, uint64_t(0) - uint64_t(2 * 1953125) - 5 * 3815 + 2);

    sendTimestamp = converter.toAbsoluteTime(512 * 2, utils::Time::ms * 51);
    EXPECT_EQ(sendTimestamp, uint64_t(0));
}

namespace rtp
{
uint32_t nsToSecondsFp6_18(uint64_t timestampNs);
}

TEST(RtpSendTimeTest, multipleExtensions)
{
    const uint8_t absSendTimeId = 3;
    const uint8_t audioLevelId = 1;

    memory::Packet packet;
    auto rtpHeader = rtp::RtpHeader::create(packet);
    std::memset(rtpHeader->getPayload(), 0xCD, 200);

    rtp::RtpHeaderExtension header;
    auto cursor = header.extensions().begin();
    EXPECT_EQ(header.size(), 4);

    rtp::GeneralExtension1Byteheader audioLevel(audioLevelId, 1);
    audioLevel.data[0] = 122;
    header.addExtension(cursor, audioLevel);
    EXPECT_EQ(header.size(), 8);

    rtp::GeneralExtension1Byteheader absSendTime(absSendTimeId, 3);
    absSendTime.data[0] = 1;
    absSendTime.data[1] = 2;
    absSendTime.data[2] = 3;
    header.addExtension(cursor, absSendTime);
    EXPECT_EQ(header.size(), 12);

    rtpHeader->setExtensions(header);
    EXPECT_EQ(rtpHeader->headerLength(), rtp::MIN_RTP_HEADER_SIZE + header.size());
    EXPECT_NE(rtpHeader->getExtensionHeader(), nullptr);
    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
    packet.setLength(rtpHeader->headerLength());

    auto it = rtpHeader->getExtensionHeader()->extensions().begin();
    EXPECT_EQ(it->getId(), audioLevelId);
    EXPECT_EQ(it->getDataLength(), 1);

    ++it;
    EXPECT_EQ(it->getId(), absSendTimeId);
    EXPECT_EQ(it->getDataLength(), 3);
    EXPECT_EQ(it->data[0], 1);
    EXPECT_EQ(it->data[1], 2);
    EXPECT_EQ(it->data[2], 3);

    rtp::setTransmissionTimestamp(packet, absSendTimeId, 0x102030);
    it = rtpHeader->getExtensionHeader()->extensions().begin();
    EXPECT_EQ(it->getId(), audioLevelId);
    EXPECT_EQ(it->data[0], 122);
    EXPECT_EQ(it->getDataLength(), 1);

    ++it;
    EXPECT_EQ(it->getId(), absSendTimeId);
    EXPECT_EQ(it->getDataLength(), 3);
    const auto expectedData = rtp::nsToSecondsFp6_18(0x102030);
    EXPECT_EQ(it->data[0], expectedData >> 16);
    EXPECT_EQ(it->data[1], (expectedData >> 8) & 0xFF);
    EXPECT_EQ(it->data[2], expectedData & 0xFF);

    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
}

TEST(RtpTest, extHeader)
{
    const char* rawRtp = "\x90\x6f\x73\x53\x76\xd2\x24\x44\x05\xd2\x09\x1b\xbe\xde\x00\x02"
                         "\x32\xb2\x78\x66\x10\xaa\x00\x00\x4a\xb2\x78\xb9\x73\x42\xb1\xda"
                         "\x90\x4b\x5c\x18\x6b\x7f\x89\x61\xc6\xd7\x34\x2d\xf9\xbf\x41\xa9"
                         "\x8d\x46\x90\x55\xc0\x27\xd1\x61\x06\x21\x06\x1e\xd8\x37\xb7\x46"
                         "\xcc\xee\x15\x61\xe2\xf0\x25\xe1\x34\x92\xa2\xc1\x92\x3e\x56\x2c"
                         "\x63\xed\x85\x67\x95\x69\x68\x20\x19\x12\xda\x16\xe8\xca\x69\x7f"
                         "\x9d\x30\x1e\x1b\x59\x20\x3f\xad\xca\x0e\xef\x17\x08\x6d\x50\xa8"
                         "\xdf\xf1\xee\x81\x78\xef\x25";
    memory::Packet packet;
    std::memcpy(packet.get(), rawRtp, 119);
    packet.setLength(119);
    const auto rtpHeader = rtp::RtpHeader::fromPacket(packet);

    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
    int count = 0;
    for (const auto& extension : rtpHeader->getExtensionHeader()->extensions())
    {
        ++count;
    }
    EXPECT_EQ(count, 2);

    EXPECT_EQ(rtpHeader->headerLength(), rtp::MIN_RTP_HEADER_SIZE + 12);
}

TEST(RtpTest, extHeader15)
{
    const char* rawRtp = "\x90\x6f\x73\x53\x76\xd2\x24\x44\x05\xd2\x09\x1b\xbe\xde\x00\x02"
                         "\x32\xb2\x78\x66\x10\xaa\xF0\x00\x4a\xb2\x78\xb9\x73\x42\xb1\xda"
                         "\x90\x4b\x5c\x18\x6b\x7f\x89\x61\xc6\xd7\x34\x2d\xf9\xbf\x41\xa9"
                         "\x8d\x46\x90\x55\xc0\x27\xd1\x61\x06\x21\x06\x1e\xd8\x37\xb7\x46"
                         "\xcc\xee\x15\x61\xe2\xf0\x25\xe1\x34\x92\xa2\xc1\x92\x3e\x56\x2c"
                         "\x63\xed\x85\x67\x95\x69\x68\x20\x19\x12\xda\x16\xe8\xca\x69\x7f"
                         "\x9d\x30\x1e\x1b\x59\x20\x3f\xad\xca\x0e\xef\x17\x08\x6d\x50\xa8"
                         "\xdf\xf1\xee\x81\x78\xef\x25";
    memory::Packet packet;
    std::memcpy(packet.get(), rawRtp, 119);
    packet.setLength(119);
    const auto rtpHeader = rtp::RtpHeader::fromPacket(packet);

    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
    int count = 0;
    for (const auto& extension : rtpHeader->getExtensionHeader()->extensions())
    {
        ++count;
        EXPECT_NE(extension.getId(), 0);
    }
    EXPECT_EQ(count, 2);
    EXPECT_EQ(rtpHeader->headerLength(), rtp::MIN_RTP_HEADER_SIZE + 12);
}

TEST(RtpTest, extHeader2ByteHeaders)
{
    const char* rawRtp = "\x90\x6f\x73\x53\x76\xd2\x24\x44\x05\xd2\x09\x1b\x01\x00\x00\x02"
                         "\x32\xb2\x78\x66\x10\xaa\xF0\x00\x4a\xb2\x78\xb9\x73\x42\xb1\xda"
                         "\x90\x4b\x5c\x18\x6b\x7f\x89\x61\xc6\xd7\x34\x2d\xf9\xbf\x41\xa9"
                         "\x8d\x46\x90\x55\xc0\x27\xd1\x61\x06\x21\x06\x1e\xd8\x37\xb7\x46"
                         "\xcc\xee\x15\x61\xe2\xf0\x25\xe1\x34\x92\xa2\xc1\x92\x3e\x56\x2c"
                         "\x63\xed\x85\x67\x95\x69\x68\x20\x19\x12\xda\x16\xe8\xca\x69\x7f"
                         "\x9d\x30\x1e\x1b\x59\x20\x3f\xad\xca\x0e\xef\x17\x08\x6d\x50\xa8"
                         "\xdf\xf1\xee\x81\x78\xef\x25";
    memory::Packet packet;
    std::memcpy(packet.get(), rawRtp, 119);
    packet.setLength(119);
    const auto rtpHeader = rtp::RtpHeader::fromPacket(packet);

    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
    const auto extensionCollection = rtpHeader->getExtensionHeader()->extensions();

    EXPECT_EQ(extensionCollection.begin(), extensionCollection.end());
    EXPECT_EQ(rtpHeader->headerLength(), rtp::MIN_RTP_HEADER_SIZE + 12);
}

TEST(RtpTest, addToExistingExtHeader)
{
    uint8_t originalPayload[100];
    const auto payloadSize = sizeof(originalPayload);
    int i = 0;
    for (uint8_t* dst = originalPayload; i < 100; ++i, ++dst)
    {
        *dst = i + 1;
    }

    memory::Packet packet;
    auto rtpHeader = rtp::RtpHeader::create(packet);
    std::memcpy(rtpHeader->getPayload(), originalPayload, payloadSize);
    packet.setLength(payloadSize + rtpHeader->headerLength());
    EXPECT_EQ(std::memcmp(rtpHeader->getPayload(), originalPayload, payloadSize), 0);

    rtp::RtpHeaderExtension originalHeader;
    auto cursor = originalHeader.extensions().begin();
    rtp::GeneralExtension1Byteheader audioLevel(1, 1);
    audioLevel.data[0] = 56;
    originalHeader.addExtension(cursor, audioLevel);
    rtpHeader->setExtensions(originalHeader, payloadSize);
    packet.setLength(payloadSize + rtpHeader->headerLength());
    EXPECT_EQ(std::memcmp(rtpHeader->getPayload(), originalPayload, payloadSize), 0);

    rtp::RtpHeaderExtension newHeader;
    std::memcpy(&newHeader, rtpHeader->getExtensionHeader(), rtpHeader->getExtensionHeader()->size());
    cursor = newHeader.extensions().end();

    rtp::GeneralExtension1Byteheader absSendTime(2, 3);
    absSendTime.data[0] = 1;
    absSendTime.data[1] = 2;
    absSendTime.data[2] = 3;
    newHeader.addExtension(cursor, absSendTime);
    EXPECT_EQ(newHeader.size(), 12);

    rtpHeader->setExtensions(newHeader, payloadSize);
    packet.setLength(payloadSize + rtpHeader->headerLength());

    EXPECT_EQ(std::memcmp(rtpHeader->getPayload(), originalPayload, payloadSize), 0);
}

TEST(RtpTest, removeFromExistingExtHeader)
{
    alignas(alignof(rtp::RtpHeader)) const char rawRtp[120] = "\x90\x6f\x73\x53\x76\xd2\x24\x44\x05\xd2\x09\x1b\xbe\xde\x00\x02"
                         "\x32\xb2\x78\x66\x10\xaa\x00\x00\x4a\xb2\x78\xb9\x73\x42\xb1\xda"
                         "\x90\x4b\x5c\x18\x6b\x7f\x89\x61\xc6\xd7\x34\x2d\xf9\xbf\x41\xa9"
                         "\x8d\x46\x90\x55\xc0\x27\xd1\x61\x06\x21\x06\x1e\xd8\x37\xb7\x46"
                         "\xcc\xee\x15\x61\xe2\xf0\x25\xe1\x34\x92\xa2\xc1\x92\x3e\x56\x2c"
                         "\x63\xed\x85\x67\x95\x69\x68\x20\x19\x12\xda\x16\xe8\xca\x69\x7f"
                         "\x9d\x30\x1e\x1b\x59\x20\x3f\xad\xca\x0e\xef\x17\x08\x6d\x50\xa8"
                         "\xdf\xf1\xee\x81\x78\xef\x25";
    memory::Packet packet;
    std::memcpy(packet.get(), rawRtp, 119);
    packet.setLength(119);
    const auto rtpHeader = rtp::RtpHeader::fromPacket(packet);
    const auto payloadLength = packet.getLength() - rtpHeader->headerLength();

    EXPECT_TRUE(rtpHeader->getExtensionHeader()->isValid());
    rtp::RtpHeaderExtension newHeader;
    auto cursor = newHeader.extensions().begin();
    rtp::GeneralExtension1Byteheader audioLevel(1, 1);
    audioLevel.data[0] = 56;
    newHeader.addExtension(cursor, audioLevel);

    auto oldHeaderLength = rtpHeader->headerLength();
    rtpHeader->setExtensions(newHeader, payloadLength);
    EXPECT_EQ(rtpHeader->headerLength(), oldHeaderLength - 4);

    auto original = rtp::RtpHeader::fromPtr(rawRtp, 119);
    auto src = original->getPayload();
    auto newPayload = rtpHeader->getPayload();
    EXPECT_EQ(std::memcmp(src, newPayload, payloadLength), 0);
}
