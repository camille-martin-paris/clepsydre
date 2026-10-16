// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

module clepsydre.sim.audit_journal;

import mddlog.adapter.auditcanonical;

namespace clepsydre::sim {

namespace {

namespace fs = std::filesystem;
using namespace mddlog::adapter;

// Segments de 16 Kio : un registre de N + S + 3 enregistrements de 813 octets au plus y tient à 19
// par segment. 256 segments, soit 4 Mio, gardent 15 segments pour le registre et quelque dix
// mille événements du simulateur.
constexpr std::size_t segmentSize = std::size_t{16} * 1024;
constexpr std::size_t segmentCount = 256;
constexpr std::size_t producerStreams = 2; // commande et sécurité

constexpr std::string_view segmentPrefix = "segment-";
constexpr std::string_view segmentSuffix = ".bin";
constexpr std::string_view bootFile = "boot";

// Force l'écriture d'un fichier ou d'un répertoire sur le support de l'hôte. Les descripteurs
// POSIX et FILE* sont fermés dans la même fonction, qui vérifie le résultat de la fermeture.
// NOLINTBEGIN(cppcoreguidelines-owning-memory): ressources C fermées sur place, résultat vérifié
bool syncPath(const fs::path& path, bool directory) {
    if (directory) {
        DIR* handle = ::opendir(path.c_str());
        if (handle == nullptr) {
            return false;
        }
        const bool synced = ::fsync(::dirfd(handle)) == 0;
        return (::closedir(handle) == 0) && synced;
    }
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    const bool synced = ::fsync(::fileno(file)) == 0;
    return (std::fclose(file) == 0) && synced;
}

// Ajoute bytes à la fin du fichier, créé s'il le faut.
bool appendTo(const fs::path& path, std::span<const std::uint8_t> bytes, bool create) {
    std::FILE* file = std::fopen(path.c_str(), create ? "wbx" : "ab");
    if (file == nullptr) {
        return false;
    }
    const std::size_t written =
        bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), file);
    return (std::fclose(file) == 0) && written == bytes.size();
}

// NOLINTEND(cppcoreguidelines-owning-memory)

std::optional<SegmentRef> segmentRefOf(std::string_view name) {
    if (!name.starts_with(segmentPrefix) || !name.ends_with(segmentSuffix)) {
        return std::nullopt;
    }
    name.remove_prefix(segmentPrefix.size());
    name.remove_suffix(segmentSuffix.size());
    SegmentRef ref = 0;
    const char* const first = std::to_address(name.begin());
    const char* const last = std::to_address(name.end());
    const auto [end, error] = std::from_chars(first, last, ref);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return ref;
}

bool isLedger(std::string_view stream) {
    return stream.find(":ledger:") != std::string_view::npos;
}

} // namespace

FileMedium::FileMedium(fs::path directory, std::size_t capacity)
    : directory_{std::move(directory)}, capacity_{capacity} {
    if (const auto listing = segments()) {
        for (const SegmentInfo& info : *listing) {
            next_ = std::max(next_, info.segment + 1);
        }
    }
}

fs::path FileMedium::pathOf(SegmentRef segment) const {
    return directory_ /
           (std::string{segmentPrefix} + std::to_string(segment) + std::string{segmentSuffix});
}

OpenAnswer FileMedium::open(const SegmentOpening& opening) {
    const auto listing = segments();
    if (!listing) {
        return {.status = OpenStatus::Failed, .segment = 0, .end = 0};
    }
    if (listing->size() >= capacity_) {
        return {.status = OpenStatus::NoSpace, .segment = 0, .end = 0};
    }
    const SegmentRef segment = next_++;
    if (!appendTo(pathOf(segment), opening.bytes, true)) {
        return {.status = OpenStatus::Failed, .segment = 0, .end = 0};
    }
    return {.status = OpenStatus::Opened, .segment = segment, .end = opening.bytes.size()};
}

AppendAnswer FileMedium::append(SegmentRef segment, std::span<const std::uint8_t> bytes) {
    const fs::path path = pathOf(segment);
    std::error_code error;
    if (!fs::is_regular_file(path, error) || !appendTo(path, bytes, false)) {
        return {.status = AppendStatus::Failed, .end = 0};
    }
    const std::uintmax_t size = fs::file_size(path, error);
    if (error) {
        return {.status = AppendStatus::Failed, .end = 0};
    }
    return {.status = AppendStatus::Written, .end = size};
}

SyncAnswer FileMedium::sync(SegmentRef segment, std::uint64_t offset) {
    const fs::path path = pathOf(segment);
    std::error_code error;
    const std::uintmax_t size = fs::file_size(path, error);
    if (error || size < offset) {
        return SyncAnswer::Failed;
    }
    // Le fichier, puis le répertoire qui le nomme : l'existence du segment est confirmée aussi.
    if (!syncPath(path, false) || !syncPath(directory_, true)) {
        return SyncAnswer::Failed;
    }
    return SyncAnswer::Durable;
}

std::optional<std::vector<std::uint8_t>> FileMedium::read(SegmentRef segment, std::uint64_t offset,
                                                          std::uint64_t length) {
    std::ifstream file{pathOf(segment), std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    file.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::uint8_t> bytes;
    if (!file) {
        return bytes;
    }
    std::vector<char> buffer(static_cast<std::size_t>(length));
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = static_cast<std::size_t>(file.gcount());
    bytes.reserve(count);
    std::transform(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count),
                   std::back_inserter(bytes), [](char c) { return static_cast<std::uint8_t>(c); });
    return bytes;
}

std::optional<std::vector<SegmentInfo>> FileMedium::segments() {
    std::error_code error;
    const fs::directory_iterator entries{directory_, error};
    if (error) {
        return std::nullopt;
    }
    std::vector<SegmentInfo> listing;
    for (const fs::directory_entry& entry : entries) {
        const auto ref = segmentRefOf(entry.path().filename().string());
        if (!ref) {
            continue;
        }
        const std::uintmax_t size = entry.file_size(error);
        if (error) {
            return std::nullopt;
        }
        listing.push_back({.segment = *ref, .size = size});
    }
    std::ranges::sort(listing, {}, &SegmentInfo::segment);
    return listing;
}

bool FileMedium::reclaim(SegmentRef segment) {
    std::error_code error;
    return fs::remove(pathOf(segment), error) && syncPath(directory_, true);
}

bool anomaly(BoundaryKind kind) noexcept {
    switch (kind) {
    case BoundaryKind::PredecessorMatched:
    case BoundaryKind::RecoveredMatched:
    case BoundaryKind::ClosedAt:
    case BoundaryKind::EndedWithoutClose:
    case BoundaryKind::NoCloseRecorded:
    case BoundaryKind::LedgerNotClosed:
    case BoundaryKind::RemovedUnderRetention:
    case BoundaryKind::NoEarlierHistoryKnown:
        return false;
    default:
        return true;
    }
}

bool coherent(const LogReport& report) noexcept {
    const auto verified = [](const StreamBoundaryReport& stream) {
        return stream.report.verdict == Verdict::Anchored ||
               stream.report.verdict == Verdict::Unanchored;
    };
    return !report.mediumUnreadable && report.unlisted.empty() &&
           std::ranges::all_of(report.streams, verified) &&
           std::ranges::none_of(report.notes,
                                [](const BoundaryNote& note) { return anomaly(note.kind); });
}

AuditJournal::AuditJournal(std::optional<fs::path> directory, std::string serial,
                           std::unique_ptr<StorageMedium> medium)
    : directory_{std::move(directory)}, serial_{std::move(serial)}, medium_{std::move(medium)} {}

AuditJournal::~AuditJournal() = default;

std::unique_ptr<AuditJournal> AuditJournal::open(const std::optional<fs::path>& directory,
                                                 std::string_view serial, std::string& error) {
    if (!audit::validSerial(serial)) {
        error = "numéro de série invalide : " + std::string{serial};
        return nullptr;
    }
    std::unique_ptr<StorageMedium> medium;
    if (directory) {
        std::error_code failure;
        fs::create_directories(*directory, failure);
        if (failure || !fs::is_directory(*directory, failure)) {
            error = "répertoire du journal d'audit inutilisable : " + directory->string();
            return nullptr;
        }
        medium = std::make_unique<FileMedium>(*directory, segmentCount);
    } else {
        medium = std::make_unique<InMemoryStorageMedium>(segmentCount);
    }
    return std::unique_ptr<AuditJournal>{
        new AuditJournal{directory, std::string{serial}, std::move(medium)}};
}

std::optional<std::uint64_t> AuditJournal::incrementBoot() {
    if (!directory_) {
        return ++memoryBoot_;
    }
    // Compteur en texte décimal ; absent au premier démarrage. Illisible ou corrompu : aucun
    // démarrage, comme la pompe qui se met hors service (SW-REQ-020).
    const fs::path path = *directory_ / bootFile;
    std::uint64_t boot = 0;
    std::error_code failure;
    if (fs::exists(path, failure)) {
        std::ifstream file{path};
        std::string text;
        if (!std::getline(file, text)) {
            return std::nullopt;
        }
        const char* const last = std::to_address(text.cend());
        const auto [end, error] = std::from_chars(text.data(), last, boot);
        if (error != std::errc{} || end != last) {
            return std::nullopt;
        }
    }
    ++boot;
    // Écrit à côté, forcé sur le support, puis renommé : une coupure laisse l'ancien ou le nouveau.
    const fs::path next = *directory_ / (std::string{bootFile} + ".tmp");
    const std::string text = std::to_string(boot) + "\n";
    const std::vector<std::uint8_t> bytes{text.begin(), text.end()};
    fs::remove(next, failure);
    if (!appendTo(next, bytes, true) || !syncPath(next, false)) {
        return std::nullopt;
    }
    fs::rename(next, path, failure);
    if (failure || !syncPath(*directory_, true)) {
        return std::nullopt;
    }
    return boot;
}

std::optional<std::uint64_t> AuditJournal::start() {
    if (adapter_) {
        drain();
    }
    const std::optional<std::uint64_t> boot = incrementBoot();
    if (!boot) {
        return std::nullopt;
    }
    // Le démarrage précédent s'arrête sans clôture : le lecteur le dira « ended without close ».
    if (adapter_) {
        const AuditHealthSnapshot health = adapter_->healthSnapshot();
        lossesBefore_ += health.reportedLosses;
        handedBefore_ += health.handedOff;
    }
    if (sink_) {
        const StorageCounters counters = sink_->health().counters;
        failuresBefore_ += counters.openFailures + counters.appendFailures + counters.syncFailures;
    }
    adapter_.reset();
    sink_.reset();

    auto adapter = std::make_unique<AuditSinkAdapter>();
    StorageConfig config;
    config.segmentSize = segmentSize;
    config.segmentCount = segmentCount;
    config.maxProducerStreams = producerStreams;
    config.sync = SyncPolicy{.recordBound = 1, .ageBound = std::nullopt};
    AuditSinkAdapter* lossTarget = adapter.get();
    config.reportLoss = [lossTarget](std::uint64_t count) { lossTarget->reportLoss(count); };
    config.ledger = LedgerConfig{
        .streamId = "pump-" + serial_ + ":ledger:boot-" + std::to_string(*boot), .time = {}};
    auto sink = PersistingAuditSink::create(*medium_, std::move(config));
    if (!sink) {
        return std::nullopt;
    }
    sink_ = std::move(*sink);
    adapter->setSink(sink_);
    adapter_ = std::move(adapter);
    return boot;
}

bool AuditJournal::attach(audit::Recorder::Ring& ring) {
    return adapter_ && adapter_->addRing(ring) == AuditRingRegistration::Registered;
}

void AuditJournal::drain() {
    if (!adapter_) {
        return;
    }
    // Un passage remet au plus ce que les anneaux contiennent ; il s'arrête sur un refus du sink.
    while (true) {
        const AuditDrainResult result = adapter_->drainOnce();
        if (result.handedOff == 0 || result.status != AuditDrainStatus::Completed) {
            return;
        }
    }
}

void AuditJournal::close() {
    drain();
    if (sink_) {
        sink_->close();
    }
}

AuditStatus AuditJournal::status() const {
    AuditStatus out{.handedOff = handedBefore_,
                    .durable = 0,
                    .pending = 0,
                    .losses = lossesBefore_,
                    .storageFailures = failuresBefore_};
    if (adapter_) {
        const AuditHealthSnapshot health = adapter_->healthSnapshot();
        out.handedOff += health.handedOff;
        out.pending = health.pendingInRings;
        out.losses += health.reportedLosses;
    }
    if (sink_) {
        const StorageHealthSnapshot health = sink_->health();
        for (const StreamStorageHealth& stream : health.streams) {
            if (!isLedger(stream.streamId)) {
                out.durable += stream.durablePosition;
            }
        }
        out.storageFailures += health.counters.openFailures + health.counters.appendFailures +
                               health.counters.syncFailures;
    }
    return out;
}

LogReport AuditJournal::verify() {
    InMemoryAnchorProvider provider{"sans-temoin"};
    RetainedPosition retained;
    LogVerifier verifier{*medium_, provider, retained};
    return verifier.verify();
}

std::vector<StoredEvent> AuditJournal::events() {
    const LogImage image = LogImage::read(*medium_);
    std::vector<StoredEvent> out;
    for (const auto& [id, stream] : image.streams()) {
        if (isLedger(id)) {
            continue;
        }
        for (const StoredRecord& record : stream.records) {
            const CanonicalReadResult decoded = decodeCanonical(record.bytes);
            if (decoded.status != CanonicalReadStatus::Ok) {
                continue;
            }
            const DecodedAuditRecord& r = decoded.record;
            out.push_back({.stream = std::string{r.streamId},
                           .sequence = r.sequence,
                           .category = r.category,
                           .phase = r.phase,
                           .action = std::string{r.action},
                           .actor = std::string{r.actor},
                           .target = std::string{r.target},
                           .correlation = std::string{r.correlationId},
                           .requirement = std::string{r.requirementRef},
                           .risk = std::string{r.riskRef},
                           .detail = std::string{r.detail},
                           .inVocabulary = audit::find(r.action).has_value()});
        }
    }
    return out;
}

} // namespace clepsydre::sim
