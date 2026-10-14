// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <cstddef>
#include <cstdint>
#include <span>

export module clepsydre.protocol.crc32;

// Code de détection d'erreur des trames de la liaison entre les processeurs (ADR 0006,
// SYS-REQ-053) : CRC-32C (Castagnoli), polynôme 0x1EDC6F41 en représentation réfléchie
// 0x82F63B78, valeur initiale et masque final 0xFFFFFFFF. Valeur de contrôle : 0xE3069283 pour
// les neuf octets ASCII « 123456789 ». Choix justifié dans docs/design/link-protocol.md.

export namespace clepsydre::protocol {

[[nodiscard]] std::uint32_t crc32c(std::span<const std::byte> bytes) noexcept;

} // namespace clepsydre::protocol
