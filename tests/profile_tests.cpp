// Editions of a game (ProfileVariant): finding one by its executable's hash,
// and the hooks and overlay slots the running one sets. No game data.

#include "profile.hpp"

#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

int g_failures = 0;

void check(bool condition, const char *what) {
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
    if (!condition) ++g_failures;
}

void set_system_language_env(const char *value) {
#if defined(_WIN32)
    _putenv_s("PORTABLEKIT_SYSTEM_LANGUAGE", value == nullptr ? "" : value);
#else
    if (value == nullptr) unsetenv("PORTABLEKIT_SYSTEM_LANGUAGE");
    else setenv("PORTABLEKIT_SYSTEM_LANGUAGE", value, 1);
#endif
}

} // namespace

int main() {
    using namespace portablekit;

    set_system_language_env(nullptr);
    check(configured_system_language() == game().system_language, "profile language is used by default");
    set_system_language_env("es");
    check(configured_system_language() == 3u, "Spanish locale code selects PSP Spanish");
    set_system_language_env("3");
    check(configured_system_language() == 3u, "PSP numeric language selects Spanish");
    set_system_language_env("zh-cn");
    check(configured_system_language() == 11u, "Chinese simplified locale code selects PSP language 11");
    set_system_language_env("0");
    check(configured_system_language() == 0u, "PSP language zero selects Japanese");
    for (const char *invalid : {"", "12", "-1", "3extra", "Spanish"}) {
        set_system_language_env(invalid);
        check(configured_system_language() == game().system_language, "invalid language falls back to profile");
    }
    set_system_language_env(nullptr);

    const std::string patched(64, '1');
    const std::string other_eboot(64, '2');
    const std::string other(64, '3');
    const std::string unknown(64, '9');

    check(find_variant(patched) != nullptr && std::string(find_variant(patched)->key) == "patched",
          "a patched executable is found by its hash");
    check(find_variant(unknown) == nullptr && !is_known_executable(unknown), "an unknown executable is not known");
    check(is_known_executable(other), "an edition's executable is known");
    check(find_variant_by_encrypted(other_eboot) != nullptr &&
              std::string(find_variant_by_encrypted(other_eboot)->key) == "other",
          "an encrypted edition is found by its EBOOT.BIN's hash");

    check(active_variant() == nullptr && active_overlay_slots().empty() && active_patch_loaded_image() == nullptr,
          "before an edition is chosen, the profile's own hooks apply");
    set_active_variant(find_variant(patched));
    check(active_overlay_slots().size() == 2u && active_patch_loaded_image() != nullptr,
          "the patched edition's overlay slots and patch hook apply");
    set_active_variant(find_variant(other));
    check(active_overlay_slots().empty() && active_patch_loaded_image() == nullptr,
          "an edition that changes nothing keeps the profile's own");
    set_active_variant(nullptr);

    if (g_failures != 0) std::printf("%d failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
