set -eu
cfg=$1
if [ "$cfg" = sanitize ]; then
 export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
 export UBSAN_OPTIONS=halt_on_error=1
fi
ctest --test-dir build/$cfg -R '^(objects|containers|compound_shell|movement|locomotion|doors|interaction|keyframes|worldpaths|weapons|sceneexplosion|player_controls)\.|^world\.level_change|^pushables\.containers$|^ai_runtime\.(activation|visibility|visibility_filter|visibility_atomic|subway)$' -V -j 2
