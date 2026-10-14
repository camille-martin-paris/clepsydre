// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

module clepsydre.protocol.link;

import clepsydre.protocol.frame;

namespace clepsydre::protocol {

namespace {

// Signal de vie : état, indicateur de réception (0 ou 1), dernier numéro reçu en petit-boutiste
// (0 sans réception).
constexpr std::size_t stateOffset = 0;
constexpr std::size_t flagOffset = 1;
constexpr std::size_t numberOffset = 2;

// Moitié de l'espace des numéros de transport : un écart inférieur est une avance, un écart
// supérieur un retour en arrière, modulo 2^32.
constexpr std::uint32_t halfRange = 0x8000'0000U;

} // namespace

Payload encodeHeartbeat(const Heartbeat& heartbeat) noexcept {
    std::array<std::byte, heartbeatSize> bytes{};
    const std::span<std::byte> view{bytes};
    view[stateOffset] = std::byte{heartbeat.state};
    view[flagOffset] = std::byte{heartbeat.lastReceived ? std::uint8_t{1} : std::uint8_t{0}};
    const std::uint32_t number = heartbeat.lastReceived.value_or(0);
    for (std::size_t index = 0; index < 4; ++index) {
        view[numberOffset + index] = std::byte{static_cast<std::uint8_t>(number >> (8U * index))};
    }
    // heartbeatSize <= maxPayloadSize : la charge utile est toujours admise.
    static_assert(heartbeatSize <= maxPayloadSize);
    return Payload::from(view).value_or(Payload{});
}

std::optional<Heartbeat> decodeHeartbeat(std::span<const std::byte> bytes) noexcept {
    if (bytes.size() != heartbeatSize) {
        return std::nullopt;
    }
    std::uint32_t number = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        number |= std::to_integer<std::uint32_t>(bytes[numberOffset + index]) << (8U * index);
    }
    const auto flag = std::to_integer<std::uint8_t>(bytes[flagOffset]);
    Heartbeat heartbeat{.state = std::to_integer<std::uint8_t>(bytes[stateOffset]),
                        .lastReceived = std::nullopt};
    if (flag == 1) {
        heartbeat.lastReceived = number;
    } else if (flag != 0 || number != 0) {
        return std::nullopt; // indicateur inconnu, ou numéro sans réception
    }
    return heartbeat;
}

LinkTransmitter::LinkTransmitter(Sender self, std::uint32_t bootNumber) noexcept
    : self_{self}, bootNumber_{bootNumber} {}

EncodedFrame LinkTransmitter::encode(MessageType type, const Payload& payload) noexcept {
    const Frame frame{.header = {.sender = self_,
                                 .bootNumber = bootNumber_,
                                 .transportNumber = next_++, // modulo 2^32
                                 .type = type},
                      .payload = payload};
    return EncodedFrame{frame};
}

bool LinkTransmitter::heartbeatDue(MonotonicTime now) const noexcept {
    return !lastHeartbeat_ || now - *lastHeartbeat_ >= heartbeatPeriod;
}

EncodedFrame LinkTransmitter::heartbeat(MonotonicTime now, const Heartbeat& heartbeat) noexcept {
    lastHeartbeat_ = now;
    return encode(MessageType::Heartbeat, encodeHeartbeat(heartbeat));
}

LinkMonitor::LinkMonitor(Sender peer, MonotonicTime start) noexcept
    : peer_{peer}, lastValid_{start} {}

void LinkMonitor::poll(MonotonicTime now) noexcept {
    const bool watched = state_ == LinkState::Waiting || state_ == LinkState::Up;
    if (watched && now - lastValid_ >= linkTimeout) {
        lose(LinkFault::Timeout);
    }
}

void LinkMonitor::onReceived(MonotonicTime now,
                             const std::expected<Frame, FrameError>& received) noexcept {
    poll(now);
    if (state_ == LinkState::OutOfService) {
        return;
    }
    if (!received) {
        if (received.error() == FrameError::VersionMismatch) {
            lose(LinkFault::VersionMismatch);
            state_ = LinkState::OutOfService;
        } else if (state_ == LinkState::Up) {
            lose(LinkFault::Rejected, received.error());
        }
        return;
    }
    const Header& header = received->header;
    if (state_ != LinkState::Up) {
        if (header.sender == peer_) {
            establish(now, header);
        }
        return;
    }
    if (const LinkFault fault = check(header); fault != LinkFault::None) {
        lose(fault);
        return;
    }
    lastValid_ = now;
    lastTransport_ = header.transportNumber;
}

LinkFault LinkMonitor::check(const Header& header) const noexcept {
    if (header.sender != peer_) {
        return LinkFault::WrongSender;
    }
    if (header.bootNumber != peerBoot_) {
        return LinkFault::BootChanged;
    }
    const std::uint32_t step = header.transportNumber - lastTransport_; // modulo 2^32
    if (step == 0) {
        return LinkFault::TransportRepeated;
    }
    if (step >= halfRange) {
        return LinkFault::TransportBackward;
    }
    return step == 1 ? LinkFault::None : LinkFault::TransportGap;
}

void LinkMonitor::lose(LinkFault fault, std::optional<FrameError> rejection) noexcept {
    state_ = LinkState::Lost;
    fault_ = fault;
    rejection_ = rejection;
    ++losses_;
}

void LinkMonitor::establish(MonotonicTime now, const Header& header) noexcept {
    state_ = LinkState::Up;
    lastValid_ = now;
    peerBoot_ = header.bootNumber;
    lastTransport_ = header.transportNumber;
}

} // namespace clepsydre::protocol
