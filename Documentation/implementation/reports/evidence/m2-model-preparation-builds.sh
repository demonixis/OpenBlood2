set -eu
for cfg in debug release sanitize; do
 targets='model_view_tests services_tests character_nodes_tests attachment_tests character_animation_tests explosion_model_tests explosion_fragment_tests'
 if [ "$cfg" != sanitize ]; then targets="$targets blood2_app model_gpu_tests explosion_model_gpu_tests level_change_gpu_tests"; fi
 cmake --build build/$cfg --target $targets -j 3 > /tmp/b2-model-prep-$cfg-build.log 2>&1
 ctest --test-dir build/$cfg -R '^(model_view|services|character|character_nodes|attachments|explosion_models|explosion_fragments)\.' --output-on-failure > /tmp/b2-model-prep-$cfg-tests.log 2>&1
done
