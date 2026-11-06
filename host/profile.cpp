#include "profile.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>

namespace portablekit {

const char *env(const char *name) {
    // The prefix is fixed for the life of the process, so the name is built
    // once per call and never kept: getenv's result points into the
    // environment, not into this string.
    std::string full = game().env_prefix;
    full += '_';
    full += name;
    return std::getenv(full.c_str());
}

bool env_set(const char *name) { return env(name) != nullptr; }

std::string env_name(const char *name) { return std::string(game().env_prefix) + '_' + name; }

std::uint32_t configured_system_language() {
    const char *override = env("SYSTEM_LANGUAGE");
    if (override == nullptr || *override == '\0') return game().system_language;

    std::string_view text(override);
    std::uint32_t number = 0u;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && number <= 11u) return number;

    // PSP_SYSTEMPARAM_LANGUAGE_* values from the PSPSDK's sysparam header.
    // Locale strings are only a convenience for the host; the PSP still sees
    // the integer returned by sceUtilityGetSystemParamInt/GetLanguageMode.
    constexpr std::array<std::string_view, 12> kLocaleCodes{
        "ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "zh-tw", "zh-cn"};
    for (std::uint32_t i = 0u; i < kLocaleCodes.size(); ++i)
        if (text == kLocaleCodes[i]) return i;
    return game().system_language;
}

namespace {
const ProfileVariant *g_active_variant = nullptr;
}

const ProfileVariant *find_variant(std::string_view executable_sha256) {
    for (const ProfileVariant &variant : game().variants)
        if (variant.executable_sha256 != nullptr && executable_sha256 == variant.executable_sha256) return &variant;
    return nullptr;
}

const ProfileVariant *find_variant_by_encrypted(std::string_view eboot_sha256) {
    for (const ProfileVariant &variant : game().variants)
        if (variant.encrypted_executable_sha256 != nullptr && eboot_sha256 == variant.encrypted_executable_sha256)
            return &variant;
    return nullptr;
}

bool is_known_executable(std::string_view executable_sha256) {
    return (game().executable_sha256 != nullptr && executable_sha256 == game().executable_sha256) ||
           find_variant(executable_sha256) != nullptr;
}

void set_active_variant(const ProfileVariant *variant) { g_active_variant = variant; }
const ProfileVariant *active_variant() { return g_active_variant; }

ExtraHleHook active_register_extra_hle() {
    if (g_active_variant != nullptr && g_active_variant->register_extra_hle != nullptr)
        return g_active_variant->register_extra_hle;
    return game().register_extra_hle;
}

PatchImageHook active_patch_loaded_image() {
    if (g_active_variant != nullptr && g_active_variant->patch_loaded_image != nullptr)
        return g_active_variant->patch_loaded_image;
    return game().patch_loaded_image;
}

std::span<const std::uint32_t> active_overlay_slots() {
    if (g_active_variant != nullptr && !g_active_variant->overlay_slots.empty()) return g_active_variant->overlay_slots;
    return game().overlay_slots;
}

} // namespace portablekit
