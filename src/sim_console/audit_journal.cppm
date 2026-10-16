// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module clepsydre.sim.audit_journal;

export import clepsydre.audit.recorder;
export import mddlog.adapter.auditdrain;
export import mddlog.adapter.auditlogverifier;
export import mddlog.adapter.auditmedium;
export import mddlog.adapter.auditstore;

// Journal d'audit persistant du simulateur (#136), sur l'hôte seulement : les anneaux des
// enregistreurs sont vidés par l'adaptateur de mddlog (AuditSinkAdapter) vers son sink persistant
// (PersistingAuditSink), qui écrit des segments en ajout seul, les confirme durablement et tient
// un registre (ledger) par démarrage. Le support est un répertoire de fichiers, ou la mémoire.
//
// Hors périmètre : l'ancrage par le témoin (#47) ; sans fournisseur d'ancrage, le lecteur de
// mddlog rend au mieux « Unanchored » (chaîne cohérente, non ancrée). La rotation et la rétention
// (#45) ne sont pas activées.

export namespace clepsydre::sim {

// Support de stockage sur un répertoire de l'hôte : un fichier par segment, `segment-<n>.bin`.
// sync() force l'écriture du fichier et du répertoire (fsync) avant de répondre Durable : c'est le
// meilleur engagement que l'hôte puisse prendre, pas celui d'une mémoire qualifiée (ADR-004 9.2).
class FileMedium final : public mddlog::adapter::StorageMedium {
public:
    FileMedium(std::filesystem::path directory, std::size_t capacity);

    mddlog::adapter::OpenAnswer open(const mddlog::adapter::SegmentOpening& opening) override;
    mddlog::adapter::AppendAnswer append(mddlog::adapter::SegmentRef segment,
                                         std::span<const std::uint8_t> bytes) override;
    mddlog::adapter::SyncAnswer sync(mddlog::adapter::SegmentRef segment,
                                     std::uint64_t offset) override;
    std::optional<std::vector<std::uint8_t>>
    read(mddlog::adapter::SegmentRef segment, std::uint64_t offset, std::uint64_t length) override;
    std::optional<std::vector<mddlog::adapter::SegmentInfo>> segments() override;
    bool reclaim(mddlog::adapter::SegmentRef segment) override;

private:
    [[nodiscard]] std::filesystem::path pathOf(mddlog::adapter::SegmentRef segment) const;

    std::filesystem::path directory_;
    std::size_t capacity_;
    mddlog::adapter::SegmentRef next_ = 1;
};

// Événement relu sur le support, avec son flux et sa séquence.
struct StoredEvent {
    std::string stream;
    std::uint64_t sequence = 0;
    mddlog::core::AuditCategory category = mddlog::core::AuditCategory::Lifecycle;
    mddlog::core::AuditPhase phase = mddlog::core::AuditPhase::Executed;
    std::string action;
    std::string actor;
    std::string target;
    std::string correlation;
    std::string requirement;
    std::string risk;
    std::string detail;
    bool inVocabulary = false; // l'action figure au vocabulaire (docs/event-vocabulary.md)
};

// Compteurs visibles du journal : rien n'est perdu en silence.
struct AuditStatus {
    std::uint64_t handedOff = 0;       // événements remis au sink
    std::uint64_t durable = 0;         // événements confirmés durablement, tous flux
    std::uint64_t pending = 0;         // encore dans les anneaux
    std::uint64_t losses = 0;          // pertes après admission (sink, support)
    std::uint64_t storageFailures = 0; // échecs d'ouverture, d'écriture ou de synchronisation
};

// Une note du lecteur de mddlog qui révèle un écart avec ce que les registres ont consigné :
// citation non reproduite, enregistrements manquants, registres en fourche, enregistrement mal
// formé... Une fin sans clôture (coupure) ou une citation reproduite n'en sont pas.
[[nodiscard]] bool anomaly(mddlog::adapter::BoundaryKind kind) noexcept;

// Journal cohérent pour le lecteur : support lisible, chaque flux et chaque registre cohérent
// (« Unanchored » sans témoin, « Anchored » avec), et aucune note d'anomalie. Sans ancrage, une
// troncature n'est visible que par les citations des registres suivants (#47).
[[nodiscard]] bool coherent(const mddlog::adapter::LogReport& report) noexcept;

class AuditJournal {
public:
    // Journal dans directory, créé au besoin, ou en mémoire sans répertoire. serial est le
    // numéro de série de la pompe simulée. Renvoie nullptr et explique l'erreur dans error si le
    // répertoire ou le compteur de démarrage est inutilisable.
    static std::unique_ptr<AuditJournal> open(const std::optional<std::filesystem::path>& directory,
                                              std::string_view serial, std::string& error);

    AuditJournal(const AuditJournal&) = delete;
    AuditJournal(AuditJournal&&) = delete;
    AuditJournal& operator=(const AuditJournal&) = delete;
    AuditJournal& operator=(AuditJournal&&) = delete;
    ~AuditJournal();

    // Démarrage du processeur de commande : incrémente durablement le compteur de démarrage avant
    // tout événement (SW-REQ-020), ouvre un nouveau registre, et rend le numéro. Un démarrage
    // précédent s'arrête sans clôture, comme après une coupure. Rien si le compteur est
    // illisible ou ne peut être écrit.
    [[nodiscard]] std::optional<std::uint64_t> start();

    // Confie l'anneau d'un enregistreur au consommateur ; il doit survivre au journal.
    [[nodiscard]] bool attach(audit::Recorder::Ring& ring);

    // Vide les anneaux vers le support ; à appeler depuis le seul fil consommateur.
    void drain();

    // Clôture : confirme les derniers événements et écrit les clôtures du registre.
    void close();

    [[nodiscard]] AuditStatus status() const;

    // Verdict du lecteur de mddlog pour chaque flux, sans ancrage (#47).
    [[nodiscard]] mddlog::adapter::LogReport verify();

    // Événements relus sur le support, flux par flux, dans l'ordre des séquences ; les
    // enregistrements du registre de mddlog sont omis.
    [[nodiscard]] std::vector<StoredEvent> events();

    [[nodiscard]] std::string_view serial() const noexcept { return serial_; }

    [[nodiscard]] const std::optional<std::filesystem::path>& directory() const noexcept {
        return directory_;
    }

private:
    AuditJournal(std::optional<std::filesystem::path> directory, std::string serial,
                 std::unique_ptr<mddlog::adapter::StorageMedium> medium);

    [[nodiscard]] std::optional<std::uint64_t> incrementBoot();

    std::optional<std::filesystem::path> directory_;
    std::string serial_;
    std::unique_ptr<mddlog::adapter::StorageMedium> medium_;
    std::uint64_t memoryBoot_ = 0; // compteur du journal en mémoire
    std::shared_ptr<mddlog::adapter::PersistingAuditSink> sink_;
    std::unique_ptr<mddlog::adapter::AuditSinkAdapter> adapter_;
    // Compteurs des démarrages précédents, que chaque démarrage remet à zéro dans mddlog.
    std::uint64_t handedBefore_ = 0;
    std::uint64_t lossesBefore_ = 0;
    std::uint64_t failuresBefore_ = 0;
};

} // namespace clepsydre::sim
