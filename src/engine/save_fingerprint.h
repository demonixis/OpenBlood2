#pragma once
#include "engine/save_file.h"
#include <functional>
#include <map>

// Game-data fingerprint of a save (save_load_contracts.md, "Fingerprint"). Directory-block
// digests reuse the bounded REZ metadata reads; whole archives are never hashed, so a same-size
// in-place content change stays undetected, as the world contracts already exclude.
namespace OpenBlood2
{
    // Build classes and their payload versions; lot 4 derives it from the save-support declaration.
    using SaveClassSchema = std::map<std::string, std::uint32_t, std::less<>>;
    // SHA-256 of u32 count, then per entry in path order: u32 length, path bytes, u32 mount ordinal,
    // u32 offset, u32 size. Fails if an entry's archive is not one of the mounts.
    bool resource_index_digest(const Resources&, Sha256Digest&, std::string& error);
    // Edition, ordered mounts, index digest and the world DAT digest of the mounted resources.
    // `classes` must be the strictly ascending schema of the classes present in OBJS.
    bool build_fingerprint(const Resources&, std::string_view world_path, std::vector<SaveClass> classes,
                           SaveFingerprint&, SaveError&);
    // Compares a saved fingerprint with the current installation (`current`, built for the saved
    // world) and the build's class schema. Every mismatch is refused (accepted decisions 5 and 6);
    // the error lists all differences.
    bool check_fingerprint(const SaveFingerprint& saved, const SaveFingerprint& current,
                           const SaveClassSchema& registered, SaveError&);
} // namespace OpenBlood2
