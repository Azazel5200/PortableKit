#pragma once

// Optional PGD keys from the player's local keys file. No key values are
// distributed with a port, and only fingerprint-checked names are accepted.

#include "crypto_keys.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace portablekit {

struct PgdKeysFileReport {
    bool found{};
    std::size_t required_present{};
    std::vector<std::string> problems;
};

// Merge only the PGD key slots and amctrl constants; retain built-in keys for
// the executable and save data. A missing file leaves `keys` unchanged.
[[nodiscard]] PgdKeysFileReport load_pgd_keys_file(const std::filesystem::path &path, CryptoKeys &keys);

} // namespace portablekit
