// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

module clepsydre.protocol.frame;

import clepsydre.protocol.cobs;
import clepsydre.protocol.crc32;

namespace clepsydre::protocol {

namespace {

// Position des champs de l'en-tête ; entiers en petit-boutiste.
constexpr std::size_t versionOffset = 0;
constexpr std::size_t senderOffset = 1;
constexpr std::size_t bootOffset = 2;
constexpr std::size_t transportOffset = 6;
constexpr std::size_t typeOffset = 10;
constexpr std::size_t lengthOffset = 11;

static_assert(lengthOffset + 1 == headerSize);
static_assert(maxPayloadSize <= 0xFF, "la longueur tient sur un octet");

constexpr std::byte delimiter{0};

void store(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = std::byte{static_cast<std::uint8_t>(value >> (8U * index))};
    }
}

[[nodiscard]] std::uint32_t load(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        value |= std::to_integer<std::uint32_t>(bytes[offset + index]) << (8U * index);
    }
    return value;
}

[[nodiscard]] std::optional<Sender> toSender(std::byte byte) noexcept {
    switch (const auto value = std::to_integer<std::uint8_t>(byte); value) {
    case static_cast<std::uint8_t>(Sender::Control):
    case static_cast<std::uint8_t>(Sender::Safety):
        return static_cast<Sender>(value);
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<MessageType> toMessageType(std::byte byte) noexcept {
    const auto value = std::to_integer<std::uint8_t>(byte);
    if (value < static_cast<std::uint8_t>(MessageType::Heartbeat) ||
        value > static_cast<std::uint8_t>(MessageType::Test)) {
        return std::nullopt;
    }
    return static_cast<MessageType>(value);
}

} // namespace

std::optional<Payload> Payload::from(std::span<const std::byte> bytes) noexcept {
    if (bytes.size() > maxPayloadSize) {
        return std::nullopt;
    }
    return copyOf(bytes);
}

Payload Payload::copyOf(std::span<const std::byte> bytes) noexcept {
    Payload payload;
    std::ranges::copy(bytes, payload.bytes_.begin());
    payload.size_ = bytes.size();
    return payload;
}

std::span<const std::byte> Payload::bytes() const noexcept {
    return std::span{bytes_}.first(size_);
}

EncodedFrame::EncodedFrame(const Frame& frame) noexcept {
    const std::span<const std::byte> payload = frame.payload.bytes();
    std::array<std::byte, maxFrameSize> decoded{};
    const std::span<std::byte> view{decoded};
    view[versionOffset] = std::byte{protocolVersion};
    view[senderOffset] = std::byte{static_cast<std::uint8_t>(frame.header.sender)};
    store(view, bootOffset, frame.header.bootNumber);
    store(view, transportOffset, frame.header.transportNumber);
    view[typeOffset] = std::byte{static_cast<std::uint8_t>(frame.header.type)};
    view[lengthOffset] = std::byte{static_cast<std::uint8_t>(payload.size())};
    std::ranges::copy(payload, view.subspan(headerSize).begin());
    const std::size_t covered = headerSize + payload.size();
    store(view, covered, crc32c(view.first(covered)));
    // Les tailles sont bornées à la compilation : l'encodage tient toujours dans bytes_.
    static_assert(maxEncodedFrameSize - 1 >= cobsMaxEncodedSize(maxFrameSize));
    const std::span<std::byte> output{bytes_};
    size_ = cobsEncode(view.first(covered + crcSize), output.first(output.size() - 1));
    output[size_++] = delimiter; // place réservée au délimiteur ci-dessus
}

std::span<const std::byte> EncodedFrame::bytes() const noexcept {
    return std::span{bytes_}.first(size_);
}

std::expected<Frame, FrameError> parseFrame(std::span<const std::byte> decoded) noexcept {
    // Enveloppe, commune à toutes les versions.
    if (decoded.size() < envelopeMinSize) {
        return std::unexpected{FrameError::TooShort};
    }
    if (decoded.size() > envelopeMaxSize) {
        return std::unexpected{FrameError::TooLong};
    }
    const std::size_t covered = decoded.size() - crcSize;
    if (crc32c(decoded.first(covered)) != load(decoded, covered)) {
        return std::unexpected{FrameError::BadCrc};
    }
    // La version n'est lue qu'après le CRC : une altération ne passe pas pour une autre version.
    if (std::to_integer<std::uint8_t>(decoded[versionOffset]) != protocolVersion) {
        return std::unexpected{FrameError::VersionMismatch};
    }
    // Version 1.
    if (decoded.size() < headerSize + crcSize) {
        return std::unexpected{FrameError::TooShort};
    }
    if (decoded.size() > maxFrameSize) {
        return std::unexpected{FrameError::TooLong};
    }
    const std::span<const std::byte> payload = decoded.subspan(headerSize, covered - headerSize);
    if (std::to_integer<std::size_t>(decoded[lengthOffset]) != payload.size()) {
        return std::unexpected{FrameError::LengthMismatch};
    }
    const std::optional<Sender> sender = toSender(decoded[senderOffset]);
    if (!sender) {
        return std::unexpected{FrameError::UnknownSender};
    }
    const std::optional<MessageType> type = toMessageType(decoded[typeOffset]);
    if (!type) {
        return std::unexpected{FrameError::UnknownType};
    }
    return Frame{
        .header = {.sender = *sender,
                   .bootNumber = load(decoded, bootOffset),
                   .transportNumber = load(decoded, transportOffset),
                   .type = *type},
        // payload.size() <= maxFrameSize - headerSize - crcSize == maxPayloadSize.
        .payload = Payload::copyOf(payload),
    };
}

std::optional<std::expected<Frame, FrameError>> FrameReceiver::push(std::byte byte) noexcept {
    if (byte != delimiter) {
        if (size_ == buffer_.size()) {
            overflow_ = true;
        } else {
            std::span{buffer_}[size_++] = byte; // size_ < buffer_.size(), contrôlé ci-dessus
        }
        return std::nullopt;
    }
    const std::span<const std::byte> encoded = std::span{buffer_}.first(size_);
    const bool overflow = overflow_;
    size_ = 0;
    overflow_ = false;
    if (overflow) {
        return std::unexpected{FrameError::TooLong};
    }
    std::array<std::byte, envelopeMaxSize> decoded{};
    const std::optional<std::size_t> size = cobsDecode(encoded, decoded);
    if (!size) {
        return std::unexpected{FrameError::InvalidCobs};
    }
    return parseFrame(std::span{decoded}.first(*size));
}

} // namespace clepsydre::protocol
