// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <cstddef>
#include <optional>
#include <span>

export module clepsydre.protocol.cobs;

// Délimitation des trames de la liaison entre les processeurs (ADR 0006) : encodage COBS
// (Consistent Overhead Byte Stuffing). Le résultat ne contient aucun octet nul ; l'émetteur
// ajoute un octet nul après chaque trame, qui en marque la fin. Le décodage refuse toute suite
// qui ne peut pas résulter d'un encodage : octet nul, bloc qui dépasse la fin.

export namespace clepsydre::protocol {

// Taille maximale de l'encodage de size octets : un octet de code par bloc de 254 octets, plus
// le code du dernier bloc.
[[nodiscard]] constexpr std::size_t cobsMaxEncodedSize(std::size_t size) noexcept {
    return size + (size / 254) + 1;
}

// Encode input dans output et renvoie la taille écrite, toujours non nulle ; renvoie 0, sans rien
// écrire, si output a moins de cobsMaxEncodedSize(input.size()) octets.
[[nodiscard]] std::size_t cobsEncode(std::span<const std::byte> input,
                                     std::span<std::byte> output) noexcept;

// Décode input (sans le délimiteur) dans output et renvoie la taille écrite, ou std::nullopt si
// input n'est pas un encodage COBS valide ou si output est trop petit.
[[nodiscard]] std::optional<std::size_t> cobsDecode(std::span<const std::byte> input,
                                                    std::span<std::byte> output) noexcept;

} // namespace clepsydre::protocol
