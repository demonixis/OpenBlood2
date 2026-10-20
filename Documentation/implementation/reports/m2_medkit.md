# M2: Original medkit acquisition, use and preservation

Original MedKitPU and CInvMedkit now connect to player collection, direct item
commands, actual local activation audio and inventory restoration. Read the
[contracts](../../contracts/medkit_contracts.md) and
[source ledger](../sources/m2_medkit_sources.json). M2 remains incomplete.

After collecting a medkit, **H** uses it; release Shift first because the original
direct-item command excludes RUN. Focus/text/camera/death restrictions still apply.
Both first acquisition and recharge grant 100 charges. The source aims at 100 HP,
spends the integer requested gain and retains its fractional and over-100 return
quirks. The inspector distinguishes requested and actual healing, charge changes,
queued actions, actual playback and failures. Original medikit.wav is decoded
through the typed local sound service. Pickup inventory1.wav remains a request;
original PCX inventory HUD and localized console item text remain unavailable.

The optional owned medkit is saved with existing inventory values and staged
against the new owner during restoration. Zero charges differ from absence.
KeepAlive preserves this item while discarding keys, matching original Load.
Malformed values reject before mutation. No raw owner handle enters a snapshot.
The restoration test changes player/client identities and uses the restored item,
rather than merely comparing a copied counter.

CityHub now has **435 created / 31 transferred sounds / 541 editor-only /
18 unsupported / one failed**. Structural inspection checks 467 entries, with
18 unknown and no invalid schemas. The existing fallengrills missing named world
model remains a failure. Only MedKitPU leaves the unsupported set; no aliases or
successful placeholders are introduced. SubwayA coverage is unchanged.

The installed catalog contains 22 authored MedKitPU entries across 57 parseable
base worlds: 20 create with real models, while two retain explicit unsupported
TypeAction-schema failures. The complete supported pickup catalog now creates
1,290 entries, rejects 178 old schemas and has no missing model on successful
creation. No rejected schema was weakened to improve these counts.

Validation on Linux with the pinned stack:

| Configuration | Result | Includes |
| --- | --- | --- |
| Debug, inspector enabled | 730/730 headless | 118 private-data tests |
| Release, inspector absent | 730/730 headless | 118 private-data tests |
| Clang ASan/UBSan, leak detection | 62/62 targeted | 9 private tests; no findings |
| Vulkan GPU, Debug and Release | 4/4 each | 3 private scenarios each |

Commands: `cmake --build build/debug -j4`, `cmake --build build/release -j3`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`.
The sanitizer build/run targets medkit, object lifetime, pickups, inventory,
player restoration/keep-alive/controls, entry and cinematic audio. GPU checks
cover pickups, objectives/destination rehearsal, player controls and lifecycle.
The pickup harness advances the authored intro before testing input restrictions;
normal gameplay does not skip it. H is injected through the common SDL adapter,
not through OS keyboard automation. The harness asserts 25 remaining charges,
100 HP and an actual activation voice while the inspector is hidden, then repeats
after unload/reload. Captures were visually checked; the Release capture has no UI.

The authored CityHub pickup test repeats load/contact/use/unload twice. Its actual
medikit.wav contains 45,312 decoded frames with nonzero samples; local voices
survive player removal and automatically expire. Restoration changes player/client
identities and verifies healing against the new owner. Existing inventory failure
and keep-alive tests also preserve nondefault medkit charges. Full-queue, missing
sound, dead/pending owner, fractional gain, lower maximum HP and invalid snapshot
cases retain explicit outcomes.

All 43 changed C++ files pass clang-format 23.1.3 checks. The reference integrity
check covers 10,923 inventoried files with no changes and six clean reference
repositories. English documentation links are checked. Detailed commands, test
logs, source hashes and the committed project patch are retained in the
[validation record](evidence/m2-medkit.json).

The private fixture exercises the authored CityHub MedKitPU using a deliberately
injected contact and health deficit, not a claimed complete level playthrough.
The GPU pickup scenario also uses a labelled diagnostic item and H adapter input
with inspection hidden and after reload. Other inventory items, string-spawn
bounce, multiplayer, full inventory HUD, required world services and automatic
NEW_LEVEL remain unavailable. Dependencies and reference repositories are unchanged.
