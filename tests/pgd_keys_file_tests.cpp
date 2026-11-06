#include "crypto/pgd_keys_file.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void check(bool success, const char *description) {
    std::printf("%s %s\n", success ? "ok  " : "FAIL", description);
    if (!success) ++failures;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace portablekit;

    const fs::path missing = fs::temp_directory_path() / "portablekit-pgd-does-not-exist.keys";
    CryptoKeys missing_keys;
    const PgdKeysFileReport absent = load_pgd_keys_file(missing, missing_keys);
    check(!absent.found && absent.required_present == 0u && absent.problems.empty(),
          "a missing optional keys file is harmless");
    check(missing_keys.kirk_aes.empty() && missing_keys.amctrl.empty(),
          "a missing file adds no PGD keys");

    const std::string nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path path = fs::temp_directory_path() / ("portablekit-pgd-test-" + nonce + ".keys");
    {
        std::ofstream out(path);
        out << "# Dummy data: no PSP key material in these tests\n"
            << "kirk.aes.38 = 000102030405060708090A0B0C0D0E0F\n"
            << "amctrl.1CD4 = 00\n"
            << "unknown.key = 00000000000000000000000000000000\n";
    }

    CryptoKeys keys;
    Key16 existing{};
    existing.fill(0xABu);
    keys.kirk_aes.emplace(0x63u, existing);
    const PgdKeysFileReport report = load_pgd_keys_file(path, keys);
    fs::remove(path);

    check(report.found && report.problems.size() == 2u,
          "wrong fingerprints and invalid sizes are both rejected");
    check(keys.kirk(0x38u) == nullptr && !keys.amctrl.contains(1),
          "rejected PGD keys cannot enter the runtime key store");
    check(keys.kirk(0x63u) != nullptr && *keys.kirk(0x63u) == existing,
          "the loader does not replace a pre-existing runtime key");
    check(keys.named.empty() && report.required_present == 1u,
          "unknown names are ignored and required-key accounting is accurate");

    if (failures != 0) std::printf("%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
