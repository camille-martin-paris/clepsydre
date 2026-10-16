// SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
// SPDX-License-Identifier: EUPL-1.2

module;

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

export module clepsydre.audit.recorder;

export import clepsydre.audit.vocabulary;
export import mddlog.core.auditring;

// Enregistreur des événements d'audit d'un producteur (#136) : il compose les identifiants du
// vocabulaire (flux, corrélation, cible, docs/event-vocabulary.md), vérifie que l'action, la phase
// et le producteur sont admis, place le temps monotone en tête du détail, puis admet l'événement
// dans l'anneau borné de mddlog (AuditRing). Une admission est une copie en mémoire : la
// persistance relève du consommateur de l'anneau.
//
// Rien n'est tronqué : un identifiant ou un détail trop long est refusé. Chaque refus est compté,
// pour que la perte soit consignée (journal.loss) et jamais silencieuse. Ni allocation dynamique
// ni exception. Un seul fil d'exécution écrit dans un enregistreur. Conception détaillée :
// docs/design/audit-recorder.md.

export namespace clepsydre::audit {

// Texte borné possédé : ce qui ne tient pas sous la limite fixée à la construction est refusé, et
// le texte est alors marqué en débordement.
class Text {
public:
    static constexpr std::size_t capacity = mddlog::core::auditDetailCapacity;

    constexpr explicit Text(std::size_t limit) noexcept
        : limit_{limit < capacity ? limit : capacity} {}

    void append(std::string_view part) noexcept;
    void append(std::uint64_t value) noexcept;

    [[nodiscard]] std::string_view view() const noexcept { return {data_.data(), size_}; }

    [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }

private:
    std::array<char, capacity> data_{};
    std::size_t size_ = 0;
    std::size_t limit_;
    bool overflowed_ = false;
};

// Détail d'un événement : paires « clé=valeur » séparées par une espace, dans l'ordre d'ajout.
// Le temps monotone et l'heure douteuse sont ajoutés en tête par l'enregistreur.
class Detail {
public:
    Detail& add(std::string_view name, std::string_view value) noexcept;
    Detail& add(std::string_view name, std::uint64_t value) noexcept;

    [[nodiscard]] std::string_view view() const noexcept { return text_.view(); }

    [[nodiscard]] bool overflowed() const noexcept { return text_.overflowed(); }

private:
    void key(std::string_view name) noexcept;

    Text text_{Text::capacity};
};

// Identifiant de corrélation : `pump-<série>:<producteur>:boot-<n>[:input-<m>|:cond-<m>]`.
class Correlation {
public:
    [[nodiscard]] std::string_view view() const noexcept { return text_.view(); }

    // Sans le préfixe de la pompe, pour la clé `cond` du détail.
    [[nodiscard]] std::string_view local() const noexcept { return view().substr(prefix_); }

private:
    friend class Recorder;

    Text text_{mddlog::core::auditCorrelationCapacity};
    std::size_t prefix_ = 0;
};

// Origine d'un flux : la pompe, le processeur et son numéro de démarrage persistant (SW-REQ-020).
struct Origin {
    std::string_view serial;
    Producer producer = Producer::Command;
    std::uint64_t boot = 0;
};

// Numéro de série : lettres ASCII, chiffres et trait d'union, de 1 à 32 caractères.
inline constexpr std::size_t maxSerialLength = 32;
[[nodiscard]] bool validSerial(std::string_view serial) noexcept;

// Cible d'un événement (docs/event-vocabulary.md, « Cibles »).
enum class Target : std::uint8_t {
    Pump,
    Channel,
    Library,
    SyringeList,
    Software,
    Clock,
    Journal,
    Link,
    Battery,
    Keypad,
};

// Faits d'un événement, hors action.
struct Entry {
    AuditPhase phase = AuditPhase::Executed;
    std::string_view actor;
    Target target = Target::Pump;
    std::string_view correlation;
    mddlog::core::RawTime time = mddlog::core::RawTime::unavailable();
    std::chrono::milliseconds monotonic{0}; // temps monotone du producteur depuis son démarrage
    bool clockDoubtful = false;             // SYS-REQ-054 : entre un saut et la mise à l'heure
    const Detail* detail = nullptr;         // aucune clé d'action si nul
};

enum class Outcome : std::uint8_t {
    Admitted,
    UnknownAction,       // valeur hors du vocabulaire
    PhaseNotAdmitted,    // phase absente des phases admises de l'action
    ProducerNotAdmitted, // l'action n'est pas enregistrée par ce producteur
    TargetInvalid,       // cible hors de l'énumération
    DetailTooLong,       // le détail ne tient pas dans la capacité de mddlog
    Refused,             // refusé par mddlog : identifiant invalide, anneau plein...
};

struct Result {
    Outcome outcome = Outcome::Refused;
    std::uint64_t sequence = 0;                        // séquence attribuée, si admis
    std::optional<mddlog::core::AuditRefusal> refusal; // motif de mddlog, pour Refused
};

[[nodiscard]] constexpr bool admitted(const Result& result) noexcept {
    return result.outcome == Outcome::Admitted;
}

class Recorder {
public:
    static constexpr std::size_t ringCapacity = 64;
    using Ring = mddlog::core::AuditRing<ringCapacity>;

    // Flux `pump-<série>:<producteur>:boot-<n>`. Un numéro de série invalide rend l'enregistreur
    // invalide : il refuse tout événement.
    explicit Recorder(const Origin& origin) noexcept;

    Recorder(const Recorder&) = delete;
    Recorder(Recorder&&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    Recorder& operator=(Recorder&&) = delete;
    ~Recorder() = default;

    [[nodiscard]] bool valid() const noexcept { return valid_; }

    [[nodiscard]] std::string_view stream() const noexcept { return stream_.view(); }

    [[nodiscard]] Producer producer() const noexcept { return producer_; }

    // Anneau à confier au consommateur, qui le vide et persiste ses événements ; l'enregistreur
    // doit survivre au consommateur.
    [[nodiscard]] Ring& ring() noexcept { return ring_; }

    // Nouvelle entrée d'une personne, nouvelle condition détectée, ou le démarrage lui-même.
    [[nodiscard]] Correlation input() noexcept;
    [[nodiscard]] Correlation condition() noexcept;
    [[nodiscard]] Correlation boot() const noexcept;

    Result record(Action action, const Entry& entry) noexcept;
    Result record(AlarmCondition condition, AlarmStep step, const Entry& entry) noexcept;

    // Événements non admis depuis la création, quelle qu'en soit la cause.
    [[nodiscard]] std::uint64_t refused() const noexcept { return refused_; }

private:
    [[nodiscard]] Correlation correlation(std::string_view kind,
                                          std::uint64_t number) const noexcept;
    Result admit(const std::optional<ActionSpec>& spec, const Entry& entry) noexcept;
    Result refuse(Outcome outcome) noexcept;

    Text serial_{maxSerialLength};
    Text stream_{mddlog::core::auditStreamCapacity};
    Producer producer_;
    std::uint64_t boot_;
    bool valid_;
    Ring ring_;
    std::uint64_t inputs_ = 0;
    std::uint64_t conditions_ = 0;
    std::uint64_t refused_ = 0;
};

} // namespace clepsydre::audit
