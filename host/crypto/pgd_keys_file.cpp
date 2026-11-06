#include "crypto/pgd_keys_file.hpp"

#include "extension_keys.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace portablekit {
namespace {

// The desktop app checks these same public SHA-256 fingerprints. A port never
// embeds the corresponding 16-byte keys in an executable or repository.
const std::array<DeclaredKey, 8> kPgdKeys = {{
    {"kirk.aes.38", "b5e2a84dbaaf99015874182a8609494d3c8e38f1837755ee29858c78b986ad17", 16u, "PGD"},
    {"kirk.aes.39", "be3798d2e2a46c0018a4f7e41f5557e6cc2aeb64b0b15ae6b7063a8fa41c8559", 16u, "PGD"},
    {"kirk.aes.63", "16df5079b5e31b07045b9d117c9db8bf9c45e606dbe4a9c71bb763f12d61be83", 16u, "PGD"},
    {"amctrl.1cd4", "dbe5dd77c4a5f3b930b30f520ae71064a4b48161ba43d79252351effc0ecb403", 16u, "PGD"},
    {"amctrl.1ce4", "4aae0892755232421ac3077d844110b0c0334eeadc9cef6093e55a3fb8e56ee4", 16u, "PGD"},
    {"amctrl.1cf4", "e0942366cf4a2142ac1369df3d404198ac8d9bf9df73ccde6e540f2554dbcf4c", 16u, "PGD"},
    {"amctrl.dnas.1a90", "3f238f8d5e24ce9402f00be5bb869155d83bcf05457948faa36822a20af04057", 16u, "PGD"},
    {"amctrl.dnas.1aa0", "47d6a491c1dc0424e79d60b28fd7f9fc9fb6f7be0552cf4e77dfdcbea3260869", 16u, "PGD"},
}};

const std::array<std::pair<std::string_view, int>, 5> kAmctrlNames = {{
    {"amctrl.1cd4", 1}, {"amctrl.1ce4", 2}, {"amctrl.1cf4", 3},
    {"amctrl.dnas.1a90", 4}, {"amctrl.dnas.1aa0", 5},
}};

std::size_t required_count(const CryptoKeys &keys) {
    std::size_t count = 0;
    for (const std::uint8_t slot : {0x38u, 0x39u, 0x63u})
        if (keys.kirk(slot) != nullptr) ++count;
    for (int index = 1; index <= 3; ++index)
        if (keys.amctrl.contains(index)) ++count;
    return count;
}

} // namespace

PgdKeysFileReport load_pgd_keys_file(const std::filesystem::path &path, CryptoKeys &keys) {
    const DeclaredKeysFile file = read_declared_keys_file(path, kPgdKeys);
    PgdKeysFileReport report;
    report.found = file.found;
    report.problems = file.problems;
    if (!file.found) return report;

    // Built-in values take precedence. The supported PGD keys have no built-in
    // values today; keeping this rule prevents a local file replacing any.
    for (const auto &[slot, value] : file.keys.kirk_aes) keys.kirk_aes.try_emplace(slot, value);
    for (const auto &[name, value] : file.keys.named) keys.named.try_emplace(name, value);
    for (const auto &[name, index] : kAmctrlNames) {
        if (const auto value = key_value(&file.keys, name); value && value->size() == 16u) {
            Key16 key{};
            std::copy(value->begin(), value->end(), key.begin());
            keys.amctrl.try_emplace(index, key);
        }
    }
    report.required_present = required_count(keys);
    return report;
}

} // namespace portablekit
