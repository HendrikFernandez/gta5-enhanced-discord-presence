#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <string>
#include <utility>

#include "discord_rpc.h"

#ifdef __MINGW32__
// Script Hook V's import library uses MSVC-decorated C++ exports. MinGW uses
// different C++ symbol names, so bind these six exports by their SDK names.
extern "C" {
__declspec(dllimport) void shv_script_wait(DWORD) __asm__("?scriptWait@@YAXK@Z");
__declspec(dllimport) void shv_script_register(HMODULE, void (*)())
    __asm__("?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
__declspec(dllimport) void shv_script_unregister(HMODULE)
    __asm__("?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
__declspec(dllimport) void shv_native_init(UINT64) __asm__("?nativeInit@@YAX_K@Z");
__declspec(dllimport) void shv_native_push64(UINT64) __asm__("?nativePush64@@YAX_K@Z");
__declspec(dllimport) PUINT64 shv_native_call() __asm__("?nativeCall@@YAPEA_KXZ");
}
#define scriptWait shv_script_wait
#define scriptRegister shv_script_register
#define scriptUnregister shv_script_unregister
#define nativeInit shv_native_init
#define nativePush64 shv_native_push64
#define nativeCall shv_native_call
#else
#include "main.h" // Script Hook V SDK
#endif

namespace {

constexpr DWORD kSampleIntervalMs = 5000;
constexpr ULONGLONG kResendIntervalMs = 60000;
constexpr size_t kDiscordFieldMax = 127;
HMODULE g_module = nullptr;

// Native hashes from alloc8or/gta5-nativedb-data. Native calls stay on the
// Script Hook V script thread; they must never run from Discord's worker thread.
namespace native_hash {
constexpr UINT64 player_id = 0x4F8644AF03D0E0D6;
constexpr UINT64 player_ped_id = 0xD80958FC74E988A6;
constexpr UINT64 is_player_playing = 0x5E9564D8246B909A;
constexpr UINT64 is_player_dead = 0x424D4687FA1E5652;
constexpr UINT64 is_player_being_arrested = 0x388A47C51ABDAC8E;
constexpr UINT64 is_pause_menu_active = 0xB0034A223497FFCB;
constexpr UINT64 network_is_in_session = 0xCA97246103B63917;
constexpr UINT64 get_mission_flag = 0xA33CDCCDA663159E;
constexpr UINT64 get_entity_model = 0x9F47B058362C84B5;
constexpr UINT64 get_entity_coords = 0x3FEF770D40960D5A;
constexpr UINT64 get_name_of_zone = 0xCD90657D4C30E1CA;
constexpr UINT64 get_hash_key = 0xD24D37CC275948CC;
constexpr UINT64 stat_get_int = 0x767FBC2AC802EF3D;
constexpr UINT64 stat_get_float = 0xD7AE6C9C9C6AC54C;
constexpr UINT64 thread_iterator_reset = 0xDADFADA5A20143A8;
constexpr UINT64 thread_iterator_next = 0x30B4FA1C82DD4B9F;
constexpr UINT64 get_script_name = 0x05A42BA9FC8DA96B;
} // namespace native_hash

UINT64 call(UINT64 hash, std::initializer_list<UINT64> arguments = {}) {
    nativeInit(hash);
    for (UINT64 argument : arguments) nativePush64(argument);
    const PUINT64 result = nativeCall();
    return result ? *result : 0;
}

UINT64 float_argument(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct Vec3 { float x, y, z; };

Vec3 entity_coords(int entity) {
    nativeInit(native_hash::get_entity_coords);
    nativePush64(static_cast<UINT64>(entity));
    nativePush64(1); // alive
    const PUINT64 result = nativeCall();
    Vec3 position{};
    if (result) {
        // Script Hook V uses 8-byte slots for the three returned floats.
        std::memcpy(&position.x, &result[0], sizeof(float));
        std::memcpy(&position.y, &result[1], sizeof(float));
        std::memcpy(&position.z, &result[2], sizeof(float));
    }
    return position;
}

std::string call_string(UINT64 hash, std::initializer_list<UINT64> arguments) {
    const auto* value = reinterpret_cast<const char*>(call(hash, arguments));
    return value ? std::string(value) : std::string();
}

std::string module_directory(HMODULE module) {
    char path[MAX_PATH]{};
    if (!GetModuleFileNameA(module, path, MAX_PATH)) return ".\\";
    std::string result(path);
    const auto slash = result.find_last_of("\\/");
    return slash == std::string::npos ? ".\\" : result.substr(0, slash + 1);
}

void diagnostic_log(const std::string& directory, const char* message) {
    const std::string path = directory + "GtaVEnhancedPresence.log";
    if (std::FILE* file = std::fopen(path.c_str(), "a")) {
        std::time_t now = std::time(nullptr);
        std::fprintf(file, "%lld %s\n", static_cast<long long>(now), message);
        std::fclose(file);
    }
}

std::string config_value(const std::string& path, const char* section,
                         const char* key) {
    char value[512]{};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), path.c_str());
    return value;
}

bool valid_app_id(const std::string& value) {
    return value.size() >= 17 && value.size() <= 20 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        });
}

std::string discord_field(std::string value) {
    std::string result;
    result.reserve(value.size());
    bool formatting = false;
    for (unsigned char c : value) {
        if (c == '~') formatting = !formatting;
        else if (!formatting && c >= 32) result.push_back(static_cast<char>(c));
    }
    if (result.size() <= kDiscordFieldMax) return result;
    size_t end = kDiscordFieldMax - 3;
    while (end && (static_cast<unsigned char>(result[end]) & 0xC0) == 0x80) --end;
    result.resize(end);
    result += "...";
    return result;
}

std::string money_text(int value) {
    const bool negative = value < 0;
    const uint64_t amount = negative ? static_cast<uint64_t>(-static_cast<int64_t>(value))
                                     : static_cast<uint64_t>(value);
    std::string digits = std::to_string(amount);
    for (size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
    return std::string(negative ? "-$" : "$") + digits;
}

// The zone codes and labels correspond to GTA V's GET_NAME_OF_ZONE result.
// Unlisted codes fall back to the code itself so new Enhanced zones still show.
std::string zone_name(const std::string& code) {
    static const std::pair<const char*, const char*> zones[] = {
#include "zone_names.inc"
    };
    for (const auto& [key, name] : zones) {
        if (code == key) return name;
    }
    return code.empty() ? "San Andreas" : code;
}

struct NamedActivity {
    std::string name;
    bool hobby = false;
};

NamedActivity named_activity(const std::string& path) {
    call(native_hash::thread_iterator_reset);
    // An upper bound protects against an iterator that behaves unexpectedly on
    // a different game build. A nonpositive ID means it is exhausted.
    NamedActivity best;
    int best_priority = 3;
    for (int i = 0; i < 1024; ++i) {
        const int id = static_cast<int>(call(native_hash::thread_iterator_next));
        if (id <= 0) break;
        std::string script = call_string(native_hash::get_script_name,
                                         {static_cast<UINT64>(id)});
        if (script.empty()) continue;
        // The INI holds only known mission and activity scripts. Other active
        // game controllers must not be mistaken for the player's mission.
        int priority = 0;
        for (const char* section : {"StoryMissions", "SideMissions", "Activities"}) {
            const std::string name = config_value(path, section, script.c_str());
            if (!name.empty() && priority < best_priority) {
                best = {name, priority == 2};
                best_priority = priority;
            }
            ++priority;
        }
        if (best_priority == 0) break;
    }
    return best;
}

bool stat_int(const char* name, int& out) {
    const UINT64 hash = call(native_hash::get_hash_key,
                             {reinterpret_cast<UINT64>(name)});
    return call(native_hash::stat_get_int,
                {hash, reinterpret_cast<UINT64>(&out), UINT64(-1)}) != 0;
}

bool stat_float(const char* name, float& out) {
    const UINT64 hash = call(native_hash::get_hash_key,
                             {reinterpret_cast<UINT64>(name)});
    return call(native_hash::stat_get_float,
                {hash, reinterpret_cast<UINT64>(&out), UINT64(-1)}) != 0;
}

bool save_playtime_text(std::string& out) {
    // Story Mode tracks time separately for Michael, Franklin and Trevor.
    // Each stat is a millisecond count. Sum all three to describe the save,
    // regardless of which protagonist is currently active.
    uint64_t total_ms = 0;
    bool found = false;
    const char* names[] = {"SP0_TOTAL_PLAYING_TIME", "SP1_TOTAL_PLAYING_TIME",
                           "SP2_TOTAL_PLAYING_TIME"};
    uint32_t values[3]{};
    for (size_t i = 0; i < 3; ++i) {
        int value = 0;
        if (stat_int(names[i], value)) {
            values[i] = static_cast<uint32_t>(value);
            total_ms += values[i];
            found = true;
        }
    }
    if (!found) return false;
    static bool logged = false;
    if (!logged) {
        const std::string message = "Save playtime stats (ms): " +
            std::to_string(values[0]) + ", " + std::to_string(values[1]) +
            ", " + std::to_string(values[2]);
        diagnostic_log(module_directory(g_module), message.c_str());
        logged = true;
    }
    const uint64_t minutes = total_ms / 60000;
    out = "Play time: " + std::to_string(minutes / 60) + "h " +
          std::to_string(minutes % 60) + "m";
    return true;
}

struct Presence {
    std::string details;
    std::string state;
    bool online = false;
    std::string image_key;
    std::string image_text;
};

struct ImageAssets {
    std::string fallback;
    std::string michael;
    std::string franklin;
    std::string trevor;
};

Presence sample_game(const std::string& missions_path, const ImageAssets& images) {
    if (call(native_hash::network_is_in_session)) return {{}, {}, true};

    const int player = static_cast<int>(call(native_hash::player_id));
    const int ped = static_cast<int>(call(native_hash::player_ped_id));
    if (!ped || !call(native_hash::is_player_playing, {static_cast<UINT64>(player)})) {
        return {"🎮 Main menu", "Grand Theft Auto V Enhanced", false,
                images.fallback, "Grand Theft Auto V Enhanced"};
    }

    std::string status;
    const char* status_icon = "🎮";
    if (call(native_hash::is_player_dead, {static_cast<UINT64>(player)})) {
        status = "Wasted";
        status_icon = "💀";
    } else if (call(native_hash::is_player_being_arrested,
                    {static_cast<UINT64>(player), 0})) {
        status = "Busted";
        status_icon = "🚓";
    } else if (call(native_hash::is_pause_menu_active)) {
        status = "Paused";
        status_icon = "⏸️";
    } else {
        const NamedActivity activity = named_activity(missions_path);
        if (!activity.name.empty()) {
            status = std::string(activity.hobby ? "Activity: " : "Mission: ") + activity.name;
            status_icon = activity.hobby ? "🏁" : "🎯";
        }
        else if (call(native_hash::get_mission_flag)) {
            status = "In a mission";
            status_icon = "🎯";
        } else {
            status = "Free roam";
            status_icon = "🚗";
        }
    }

    const Vec3 position = entity_coords(ped);
    const std::string zone_code = call_string(native_hash::get_name_of_zone,
        {float_argument(position.x), float_argument(position.y), float_argument(position.z)});
    const std::string area = zone_name(zone_code);
    std::string details = std::string(status_icon) + " " + status;
    std::string state = "📍 " + area;

    // completionpercentage_controller stores the same total used by the
    // Story Mode 100% checklist in this stat. Omit it if unavailable.
    float completion = 0.0f;
    if (stat_float("total_progress_made", completion) &&
        completion >= 0.0f && completion <= 100.0f) {
        char progress[24]{};
        std::snprintf(progress, sizeof(progress), "%.1f%%", completion);
        state += "  ·  🏆 ";
        state += progress;
    }

    // GTA V keeps separate cash stats for its three protagonists. A custom
    // player model has no unambiguous Story Mode wallet, so omit cash for it.
    const uint32_t model = static_cast<uint32_t>(call(native_hash::get_entity_model,
                                                     {static_cast<UINT64>(ped)}));
    const char* cash_stat = nullptr;
    const std::string* character_image = nullptr;
    const char* image_text = "Grand Theft Auto V Enhanced";
    switch (model) {
        case 225514697u:
            cash_stat = "SP0_TOTAL_CASH";
            character_image = &images.michael;
            image_text = "Michael De Santa";
            break;
        case 2602752943u:
            cash_stat = "SP1_TOTAL_CASH";
            character_image = &images.franklin;
            image_text = "Franklin Clinton";
            break;
        case 2608926626u:
            cash_stat = "SP2_TOTAL_CASH";
            character_image = &images.trevor;
            image_text = "Trevor Philips";
            break;
        default: break;
    }
    int money = 0;
    if (cash_stat && stat_int(cash_stat, money))
        state += "  ·  💵 " + money_text(money);

    std::string save_time;
    if (save_playtime_text(save_time)) state += "  ·  ⏱ " + save_time;

    const bool has_character_image = character_image && !character_image->empty();
    return {discord_field(details), discord_field(state), false,
            has_character_image ? *character_image : images.fallback,
            has_character_image ? image_text : "Grand Theft Auto V Enhanced"};
}

void script_main();

void script_main() {
    const std::string directory = module_directory(g_module);
    diagnostic_log(directory, "Script thread entered");
    const std::string config_path = directory + "GtaVEnhancedPresence.ini";
    const std::string missions_path = directory + "GtaVEnhancedPresence.missions.ini";
    const int64_t start_timestamp = static_cast<int64_t>(std::time(nullptr));

    bool initialized = false;
    bool sent = false;
    bool warned = false;
    ULONGLONG last_sent = 0;
    Presence previous;
    ImageAssets images;
    bool first_sample = true;
    bool first_wait = true;

    while (true) {
        if (!initialized) {
            const std::string app_id = config_value(config_path, "Discord", "ApplicationId");
            if (!warned) diagnostic_log(directory,
                valid_app_id(app_id) ? "Application ID valid" : "Application ID missing or invalid");
            if (valid_app_id(app_id)) {
                images.fallback = config_value(config_path, "Discord", "LargeImageKey");
                images.michael = config_value(config_path, "Discord", "MichaelImageKey");
                images.franklin = config_value(config_path, "Discord", "FranklinImageKey");
                images.trevor = config_value(config_path, "Discord", "TrevorImageKey");
                diagnostic_log(directory, "Initializing Discord RPC");
                Discord_Initialize(app_id.c_str(), nullptr, 0, nullptr);
                diagnostic_log(directory, "Discord RPC initialized");
                initialized = true;
                warned = false;
            } else if (!warned) {
                OutputDebugStringA("GtaVEnhancedPresence: set ApplicationId in GtaVEnhancedPresence.ini\n");
                warned = true;
            }
        }

        if (initialized) {
            if (first_sample) diagnostic_log(directory, "Sampling game state");
            Presence next = sample_game(missions_path, images);
            if (first_sample) {
                diagnostic_log(directory, next.online ? "Online session detected" : "Game state sampled");
                first_sample = false;
            }
            if (next.online) {
                if (sent) Discord_ClearPresence();
                sent = false;
            } else {
                const ULONGLONG now = GetTickCount64();
                const bool image_changed = !sent || next.image_key != previous.image_key;
                if (!sent || next.details != previous.details ||
                    next.state != previous.state ||
                    next.image_key != previous.image_key ||
                    next.image_text != previous.image_text ||
                    now - last_sent >= kResendIntervalMs) {
                    previous = std::move(next);
                    DiscordRichPresence rpc{};
                    rpc.details = previous.details.c_str();
                    rpc.state = previous.state.c_str();
                    rpc.startTimestamp = start_timestamp;
                    rpc.largeImageKey = previous.image_key.empty() ? nullptr :
                                        previous.image_key.c_str();
                    rpc.largeImageText = previous.image_text.c_str();
                    if (!sent) diagnostic_log(directory, "Sending first Discord presence");
                    Discord_UpdatePresence(&rpc);
                    if (!sent) diagnostic_log(directory, "First Discord presence queued");
                    if (image_changed) {
                        const std::string artwork = "Presence artwork: " +
                            (previous.image_key.empty() ? std::string("application icon") :
                                                          previous.image_key);
                        diagnostic_log(directory, artwork.c_str());
                    }
                    last_sent = now;
                    sent = true;
                }
            }
        }
        scriptWait(kSampleIntervalMs);
        if (first_wait) {
            diagnostic_log(directory, "Script thread resumed after wait");
            first_wait = false;
        }
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        scriptRegister(module, script_main);
    } else if (reason == DLL_PROCESS_DETACH) {
        scriptUnregister(module);
    }
    return TRUE;
}
