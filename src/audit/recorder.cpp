// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

module clepsydre.audit.recorder;

namespace clepsydre::audit {

namespace {

constexpr std::string_view pumpPrefix = "pump-";

bool knownProducer(Producer producer) noexcept {
    return producer == Producer::Command || producer == Producer::Safety;
}

// `pump-<série>:<producteur>:boot-<n>` ; le texte déborde si le numéro de série est trop long.
Text streamOf(const Origin& origin) noexcept {
    Text text{mddlog::core::auditStreamCapacity};
    text.append(pumpPrefix);
    text.append(origin.serial);
    text.append(":");
    text.append(producerName(origin.producer));
    text.append(":boot-");
    text.append(origin.boot);
    return text;
}

// Suffixe de la cible après `pump-<série>`.
std::optional<std::string_view> targetSuffix(Target target) noexcept {
    switch (target) {
    case Target::Pump:
        return std::string_view{};
    case Target::Channel:
        return "/channel-1";
    case Target::Library:
        return "/library";
    case Target::SyringeList:
        return "/syringe-list";
    case Target::Software:
        return "/software";
    case Target::Clock:
        return "/clock";
    case Target::Journal:
        return "/journal";
    case Target::Link:
        return "/link";
    case Target::Battery:
        return "/battery";
    case Target::Keypad:
        return "/keypad";
    }
    return std::nullopt;
}

} // namespace

void Text::append(std::string_view part) noexcept {
    if (overflowed_ || part.size() > limit_ - size_) {
        overflowed_ = true;
        return;
    }
    const std::span<char> data{data_};
    for (const char c : part) {
        data[size_] = c;
        ++size_;
    }
}

void Text::append(std::uint64_t value) noexcept {
    std::array<char, 20> digits{}; // 2⁶⁴ − 1 compte 20 chiffres
    std::size_t count = 0;
    for (bool first = true; first || value != 0; first = false) {
        std::span{digits}[digits.size() - 1 - count] = static_cast<char>('0' + (value % 10));
        value /= 10;
        ++count;
    }
    append(std::string_view{digits.data() + (digits.size() - count), count});
}

void Detail::key(std::string_view name) noexcept {
    if (!text_.view().empty()) {
        text_.append(" ");
    }
    text_.append(name);
    text_.append("=");
}

Detail& Detail::add(std::string_view name, std::string_view value) noexcept {
    key(name);
    text_.append(value);
    return *this;
}

Detail& Detail::add(std::string_view name, std::uint64_t value) noexcept {
    key(name);
    text_.append(value);
    return *this;
}

bool validSerial(std::string_view serial) noexcept {
    if (serial.empty() || serial.size() > maxSerialLength) {
        return false;
    }
    return std::ranges::all_of(serial, [](char c) {
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        return letter || digit || c == '-';
    });
}

Recorder::Recorder(const Origin& origin) noexcept
    : stream_{streamOf(origin)}, producer_{origin.producer}, boot_{origin.boot},
      valid_{validSerial(origin.serial) && knownProducer(origin.producer)},
      ring_{valid_ ? stream_.view() : std::string_view{}} {
    serial_.append(valid_ ? origin.serial : std::string_view{});
}

Correlation Recorder::correlation(std::string_view kind, std::uint64_t number) const noexcept {
    Correlation out;
    out.text_.append(pumpPrefix);
    out.text_.append(serial_.view());
    out.text_.append(":");
    out.prefix_ = out.text_.view().size();
    out.text_.append(producerName(producer_));
    out.text_.append(":boot-");
    out.text_.append(boot_);
    if (!kind.empty()) {
        out.text_.append(kind);
        out.text_.append(number);
    }
    return out;
}

Correlation Recorder::input() noexcept {
    ++inputs_;
    return correlation(":input-", inputs_);
}

Correlation Recorder::condition() noexcept {
    ++conditions_;
    return correlation(":cond-", conditions_);
}

Correlation Recorder::boot() const noexcept {
    return correlation({}, 0);
}

Result Recorder::refuse(Outcome outcome) noexcept {
    ++refused_;
    return {.outcome = outcome, .sequence = 0, .refusal = std::nullopt};
}

Result Recorder::record(Action action, const Entry& entry) noexcept {
    return admit(spec(action), entry);
}

Result Recorder::record(AlarmCondition condition, AlarmStep step, const Entry& entry) noexcept {
    return admit(spec(condition, step), entry);
}

Result Recorder::admit(const std::optional<ActionSpec>& spec, const Entry& entry) noexcept {
    if (!spec) {
        return refuse(Outcome::UnknownAction);
    }
    if (!contains(spec->phases, entry.phase)) {
        return refuse(Outcome::PhaseNotAdmitted);
    }
    if (!contains(spec->producers, producer_)) {
        return refuse(Outcome::ProducerNotAdmitted);
    }
    const std::optional<std::string_view> suffix = targetSuffix(entry.target);
    if (!suffix) {
        return refuse(Outcome::TargetInvalid);
    }
    Text target{mddlog::core::auditTargetCapacity};
    target.append(pumpPrefix);
    target.append(serial_.view());
    target.append(*suffix);

    // Temps monotone en tête, heure douteuse ensuite, puis les clés de l'action.
    Text detail{mddlog::core::auditDetailCapacity};
    detail.append("mono=");
    detail.append(static_cast<std::uint64_t>(entry.monotonic.count()));
    if (entry.clockDoubtful) {
        detail.append(" clock=doubtful");
    }
    if (entry.detail != nullptr) {
        if (entry.detail->overflowed()) {
            return refuse(Outcome::DetailTooLong);
        }
        if (!entry.detail->view().empty()) {
            detail.append(" ");
            detail.append(entry.detail->view());
        }
    }
    if (detail.overflowed()) {
        return refuse(Outcome::DetailTooLong);
    }

    const mddlog::core::AuditWriteResult result = ring_.tryRecord({
        .category = spec->category,
        .phase = entry.phase,
        .time = entry.time,
        .action = spec->name,
        .actor = entry.actor,
        .target = target.view(),
        .requirementRef = spec->requirement,
        .riskRef = spec->risk,
        .correlationId = entry.correlation,
        .sourceSequence = std::nullopt,
        .detail = detail.view(),
    });
    if (!result.wasAdmitted()) {
        Result refused = refuse(Outcome::Refused);
        refused.refusal = result.refusal();
        return refused;
    }
    return {.outcome = Outcome::Admitted, .sequence = result.sequence(), .refusal = std::nullopt};
}

} // namespace clepsydre::audit
