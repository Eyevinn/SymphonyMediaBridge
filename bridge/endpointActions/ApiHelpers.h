#pragma once
#include "api/EndpointDescription.h"
#include "bridge/RtpMap.h"
#include "bridge/endpointActions/ApiActions.h"
#include "bridge/engine/SimulcastStream.h"
#include "bridge/engine/SsrcWhitelist.h"
#include <mutex>
#include <vector>

namespace ice
{
class IceCandidate;
}

namespace bridge
{

std::pair<std::vector<ice::IceCandidate>, std::pair<std::string, std::string>> getIceCandidatesAndCredentials(
    const api::EndpointDescription::Transport&);
std::pair<std::vector<ice::IceCandidate>, std::pair<std::string, std::string>> getIceCandidatesAndCredentials(
    const api::EndpointDescription::Ice& ice);
std::unique_lock<std::mutex> getConferenceMixer(ActionContext*,
    const std::string& conferenceId,
    bridge::Mixer*& outMixer);
api::EndpointDescription::Candidate iceCandidateToApi(const ice::IceCandidate&);
void addDefaultAudioProperties(api::EndpointDescription::Audio& audioChannel);
void addVp8VideoProperties(api::EndpointDescription::Video& videoChannel);
void addH264VideoProperties(api::EndpointDescription::Video& videoChannel,
    const std::string& profileLevelId,
    const uint32_t packetizationMode);
void addDefaultVideoProperties(api::EndpointDescription::Video& videoChannel);

bridge::RtpMap makeRtpMap(const api::EndpointDescription::Audio& audio);
bridge::RtpMap makeRtpMap(const api::EndpointDescription::Video& video,
    const api::EndpointDescription::PayloadType& payloadType);

utils::Optional<uint8_t> findAbsSendTimeExtensionId(
    const std::vector<std::pair<uint32_t, std::string>>& rtpHeaderExtensions);
utils::Optional<uint8_t> findC9InfoExtensionId(
    const std::vector<std::pair<uint32_t, std::string>>& rtpHeaderExtensions);
utils::Optional<uint8_t> findAudioLevelExtensionId(
    const std::vector<std::pair<uint32_t, std::string>>& rtpHeaderExtensions);
std::vector<bridge::SimulcastStream> makeSimulcastStreams(const api::EndpointDescription::Video&,
    const std::string& endpointId);
bridge::SsrcWhitelist makeWhitelistedSsrcsArray(const api::EndpointDescription::Video&);
} // namespace bridge
