#include "Geode/platform/cplatform.h"
#include "Geode/utils/JsonValidation.hpp"
#include <Geode/Geode.hpp>
#include <Geode/DefaultInclude.hpp>
#include <matjson.hpp>
#include <fmt/chrono.h>
#include <date/date.h>

using namespace geode::prelude;

class ModMetadataLinks::Impl final {
public:
    std::optional<std::string> m_homepage;
    std::optional<std::string> m_source;
    std::optional<std::string> m_community;
};

// fix build errors lol
template <>
struct matjson::Serialize<geode::ModMetadata::Dependency::Importance> {
    static geode::Result<geode::ModMetadata::Dependency::Importance, std::string> fromJson(Value const& value)
    {
        auto str = GEODE_UNWRAP(value.asString());
        if (str == "required") return geode::Ok(geode::ModMetadata::Dependency::Importance::Required);
        if (str == "recommended") return geode::Ok(geode::ModMetadata::Dependency::Importance::Recommended);
        if (str == "suggested") return geode::Ok(geode::ModMetadata::Dependency::Importance::Suggested);
        return geode::Err("Invalid importance");
    }

    static Value toJson(geode::ModMetadata::Dependency::Importance const& value)
    {
        switch (value) {
            case geode::ModMetadata::Dependency::Importance::Required: return "required";
            case geode::ModMetadata::Dependency::Importance::Recommended: return "recommended";
            case geode::ModMetadata::Dependency::Importance::Suggested: return "suggested";
        }
        return "unknown";
    }
};

template <>
struct matjson::Serialize<geode::ModMetadata::Incompatibility::Importance> {
    static geode::Result<geode::ModMetadata::Incompatibility::Importance, std::string> fromJson(Value const& value)
    {
        auto str = GEODE_UNWRAP(value.asString());
        if (str == "breaking") return geode::Ok(geode::ModMetadata::Incompatibility::Importance::Breaking);
        if (str == "conflicting") return geode::Ok(geode::ModMetadata::Incompatibility::Importance::Conflicting);
        if (str == "superseded") return geode::Ok(geode::ModMetadata::Incompatibility::Importance::Superseded);
        return geode::Err("Invalid importance");
    }

    static Value toJson(geode::ModMetadata::Incompatibility::Importance const& value)
    {
        switch (value) {
            case geode::ModMetadata::Incompatibility::Importance::Breaking: return "breaking";
            case geode::ModMetadata::Incompatibility::Importance::Conflicting: return "conflicting";
            case geode::ModMetadata::Incompatibility::Importance::Superseded: return "superseded";
        }
        return "unknown";
    }
};

struct ServerDateTime final {
    using Clock = std::chrono::system_clock;
    using Value = std::chrono::time_point<Clock>;

    Value value;

    std::string toAgoString() const {
        auto const fmtPlural = [](auto count, auto unit) {
            if (count == 1) {
                return fmt::format("{} {} ago", count, unit);
            }
            return fmt::format("{} {}s ago", count, unit);
        };
        auto now = Clock::now();
        auto len = std::chrono::duration_cast<std::chrono::minutes>(now - value).count();
        if (len < 60) {
            return fmtPlural(len, "minute");
        }
        len = std::chrono::duration_cast<std::chrono::hours>(now - value).count();
        if (len < 24) {
            return fmtPlural(len, "hour");
        }
        len = std::chrono::duration_cast<std::chrono::days>(now - value).count();
        if (len < 31) {
            return fmtPlural(len, "day");
        }
        return fmt::format("{:%b %d %Y}", value);
    }

    static Result<ServerDateTime> parse(std::string const& str) {
        std::stringstream ss(str);
        date::sys_seconds seconds;
        if (ss >> date::parse("%Y-%m-%dT%H:%M:%S%Z", seconds)) {
            return Ok(ServerDateTime {
                .value = seconds
            });
        }
        return Err("Invalid date time format '{}'", str);
    }
};

struct ServerTag final {
    size_t id;
    std::string name;
    std::string displayName;

    static Result<ServerTag> parse(matjson::Value const& json) {
        auto root = checkJson(json, "ServerTag");
        auto res = ServerTag();

        root.needs("id").into(res.id);
        root.needs("name").into(res.name);
        root.needs("display_name").into(res.displayName);

        return root.ok(res);
    }

    static Result<std::vector<ServerTag>> parseList(matjson::Value const& json) {
        auto payload = checkJson(json, "ServerTagsList");
        std::vector<ServerTag> list {};
        for (auto& item : payload.items()) {
            auto mod = ServerTag::parse(item.json());
            if (mod) {
                list.push_back(mod.unwrap());
            } else {
                geode::log::error("Unable to parse tag from the server: {}", mod.unwrapErr());
            }
        }
        return payload.ok(list);
    }
};

struct ServerDeveloper final {
    std::string username;
    std::string displayName;
    bool isOwner;
};

struct ServerModVersion final {
    ModMetadata metadata;
    std::string downloadURL;
    std::string hash;
    size_t downloadCount;

    bool operator==(ServerModVersion const&) const = default;

    static Result<ServerModVersion> parse(matjson::Value const& json) {
        auto root = checkJson(json, "ServerModVersion");

        auto res = ServerModVersion();

        res.metadata.setGeodeVersion(root.needs("geode").get<VersionInfo>());

        // Verify target GD version
        auto gd_obj = root.needs("gd");
        std::string gd = "0.000";
        if (gd_obj.hasNullable(GEODE_PLATFORM_SHORT_IDENTIFIER)) {
            gd = gd_obj.hasNullable(GEODE_PLATFORM_SHORT_IDENTIFIER). get<std::string>();
        }

        if (gd != "*") {
            res.metadata.setGameVersion(gd);
        }

        // Get server info
        root.needs("download_link").into(res.downloadURL);
        root.needs("download_count").into(res.downloadCount);
        root.needs("hash").into(res.hash);

        // Get mod metadata info
        res.metadata.setID(root.needs("mod_id").get<std::string>());
        res.metadata.setName(root.needs("name").get<std::string>());
        res.metadata.setDescription(root.needs("description").get<std::string>());
        res.metadata.setVersion(root.needs("version").get<VersionInfo>());
        res.metadata.setIsAPI(root.needs("api").get<bool>());

        std::vector<ModMetadata::Dependency> dependencies {};
        for (auto& obj : root.hasNullable("dependencies").items()) {
            // todo: this should probably be generalized to use the same function as mod.json

            bool onThisPlatform = !obj.hasNullable("platforms");
            for (auto& plat : obj.hasNullable("platforms").items()) {
                if (PlatformID::coveredBy(plat.get<std::string>(), GEODE_PLATFORM_TARGET)) {
                    onThisPlatform = true;
                }
            }
            if (!onThisPlatform) {
                continue;
            }

            ModMetadata::Dependency dependency;
            obj.needs("mod_id").mustBe<std::string>("a valid id", &ModMetadata::validateID).into(dependency.id);
            obj.needs("version").into(dependency.version);
            obj.hasNullable("importance").into(dependency.importance);

            // Check if this dependency is installed, and if so assign the `mod` member to mark that
            auto mod = Loader::get()->getInstalledMod(dependency.id);
            if (mod && dependency.version.compare(mod->getVersion())) {
                dependency.mod = mod;
            }

            dependencies.push_back(dependency);
        }
        res.metadata.setDependencies(dependencies);

        std::vector<ModMetadata::Incompatibility> incompatibilities {};
        for (auto& obj : root.hasNullable("incompatibilities").items()) {
            ModMetadata::Incompatibility incompatibility;
            obj.hasNullable("importance").into(incompatibility.importance);

            auto modIdValue = obj.needs("mod_id");

            // Do not validate if we have a supersede, maybe the old ID is invalid
            if (incompatibility.importance == ModMetadata::Incompatibility::Importance::Superseded) {
                modIdValue.into(incompatibility.id);
            } else {
                modIdValue.mustBe<std::string>("a valid id", &ModMetadata::validateID).into(incompatibility.id);
            }

            obj.needs("version").into(incompatibility.version);

            // Check if this incompatability is installed, and if so assign the `mod` member to mark that
            auto mod = Loader::get()->getInstalledMod(incompatibility.id);
            if (mod && incompatibility.version.compare(mod->getVersion())) {
                incompatibility.mod = mod;
            }

            incompatibilities.push_back(incompatibility);
        }
        res.metadata.setIncompatibilities(incompatibilities);

        return root.ok(res);
    }
};

struct ServerModReplacement final {
    std::string id;
    VersionInfo version;
    std::string download_link;

    static Result<ServerModReplacement> parse(matjson::Value const& json) {
        auto root = checkJson(json, "ServerModReplacement");
        auto res = ServerModReplacement();

        root.needs("id").into(res.id);
        root.needs("version").into(res.version);

        return root.ok(res);
    }
};

struct ServerModUpdate final {
    std::string id;
    VersionInfo version;
    std::optional<ServerModReplacement> replacement;

    static Result<ServerModUpdate> parse(matjson::Value const& json) {
        auto root = checkJson(json, "ServerModUpdate");

        auto res = ServerModUpdate();

        root.needs("id").into(res.id);
        root.needs("version").into(res.version);
        if (root.hasNullable("replacement")) {
            GEODE_UNWRAP_INTO(res.replacement, ServerModReplacement::parse(root.hasNullable("replacement").json()));
        }

        return root.ok(res);
    }

    static Result<std::vector<ServerModUpdate>> parseList(matjson::Value const& json) {
        auto payload = checkJson(json, "ServerModUpdatesList");

        std::vector<ServerModUpdate> list {};
        for (auto& item : payload.items()) {
            auto mod = ServerModUpdate::parse(item.json());
            if (mod) {
                list.push_back(mod.unwrap());
            } else {
                geode::log::error("Unable to parse mod update from the server: {}", mod.unwrapErr());
            }
        }

        return payload.ok(list);
    }

    bool hasUpdateForInstalledMod() const {
        if (auto mod = Loader::get()->getInstalledMod(this->id)) {
            return mod->getVersion() < this->version || this->replacement.has_value();
        }
        return false;
    }
};

struct ServerModLinks final {
    std::optional<std::string> community;
    std::optional<std::string> homepage;
    std::optional<std::string> source;

    static Result<ServerModLinks> parse(matjson::Value const& json) {
        auto payload = checkJson(json, "ServerModLinks");
        auto res = ServerModLinks();

        payload.hasNullable("community").into(res.community);
        payload.hasNullable("homepage").into(res.homepage);
        payload.hasNullable("source").into(res.source);

        return payload.ok(res);
    }
};

// Code WILL break if ServerModMetadata (or the other structs) is ever updated in geode (hopefully not anytime soon)
struct ServerModMetadata final {
    std::string id;
    bool featured;
    size_t downloadCount;
    std::vector<ServerDeveloper> developers;
    std::vector<ServerModVersion> versions;
    std::unordered_set<std::string> tags;
    std::optional<std::string> about;
    std::optional<std::string> changelog;
    std::optional<std::string> repository;
    std::optional<ServerDateTime> createdAt;
    std::optional<ServerDateTime> updatedAt;

    static Result<ServerModMetadata> parse(matjson::Value const& json) {
        auto root = checkJson(json, "ServerModMetadata");

        auto res = ServerModMetadata();
        root.needs("id").into(res.id);
        root.needs("featured").into(res.featured);
        root.needs("download_count").into(res.downloadCount);
        root.hasNullable("about").into(res.about);
        root.hasNullable("changelog").into(res.changelog);
        root.hasNullable("repository").into(res.repository);
        if (root.has("created_at")) {
            GEODE_UNWRAP_INTO(res.createdAt, ServerDateTime::parse(root.has("created_at").get<std::string>()));
        }
        if (root.has("updated_at")) {
            GEODE_UNWRAP_INTO(res.updatedAt, ServerDateTime::parse(root.has("updated_at").get<std::string>()));
        }

        std::vector<std::string> developerNames;
        for (auto& obj : root.needs("developers").items()) {
            auto dev = ServerDeveloper();
            obj.needs("username").into(dev.username);
            obj.needs("dispaly_name").into(dev.displayName);
            obj.needs("is_owner").into(dev.isOwner);
            res.developers.push_back(dev);
            developerNames.push_back(dev.displayName);
        }
        for (auto& item : root.needs("versions").items()) {
            auto versionRes = ServerModVersion::parse(item.json());
            if (versionRes) {
                auto version = versionRes.unwrap();
                version.metadata.setDetails(res.about);
                version.metadata.setChangelog(res.changelog);
                version.metadata.setDevelopers(developerNames);
                version.metadata.setRepository(res.repository);
                if (root.hasNullable("links")) {
                    auto linkRes = ServerModLinks::parse(root.hasNullable("links").json());
                    if (linkRes) {
                        auto links = linkRes.unwrap();
                        version.metadata.getLinksMut().getImpl()->m_community = links.community;
                        version.metadata.getLinksMut().getImpl()->m_homepage = links.homepage;
                        if (links.source.has_value()) version.metadata.setRepository(links.source);
                    }
                }
                res.versions.push_back(version);
            } else {
                geode::log::error("Unable to parse mod '{}' version from the server: {}", res.id, versionRes.unwrapErr());
            }
        }

        if (res.versions.empty()) {
            return Err("Mod '{}' has no (valid) versions", res.id);
        }

        for (auto& item : root.hasNullable("tags").items()) {
            res.tags.insert(item.get<std::string>());
        }

        root.needs("download_count").into(res.downloadCount);

        return root.ok(res);
    }

    ModMetadata latestVersion() const {
        return this->versions.front().metadata;
    }

    std::string formatDevelopersToString() const {
        std::optional<ServerDeveloper> owner = ranges::find(developers, [](auto item) {
           return item.isOwner;
        });
        switch (developers.size()) {
            case 0: return "Unknown"; break;
            case 1: return developers.front().displayName; break;
            case 2: return developers.front().displayName + " & " + developers.back().displayName; break;
            default: {
                if (owner) {
                    return fmt::format("{} + {} More", owner->displayName, developers.size() - 1);
                } else {
                    return fmt::format("{} + {} More", developers.front().displayName, developers.size() - 1);
                }
            } break;
        }
    }

    bool hasUpdateForInstalledMod() const {
        if (auto mod = Loader::get()->getInstalledMod(this->id)) {
            return mod->getVersion() < this->latestVersion().getVersion();
        }
        return false;
    }
};

// More can go down here idk i dont wanna bloat this
