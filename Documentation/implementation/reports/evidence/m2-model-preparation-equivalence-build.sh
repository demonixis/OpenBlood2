set -eu
base=/tmp/b2-model-prep-baseline
for unit in model model_view; do
 c++ -O0 -g -std=c++20 -I "$base" -I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include -c "$base/$unit.cpp" -o "$base/$unit.o"
done
for version in before after; do
 extra=
 override=
 scene=src/app/model_scene.cpp
 if [ "$version" = before ]; then extra="$base/model.o $base/model_view.o"; override="-I $base"; scene="$base/model_scene.cpp"; fi
 c++ -O2 -std=c++20 $override -I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include /tmp/b2-model-prep-equivalence.cpp "$scene" $extra -o /tmp/b2-model-prep-equivalence-$version build/debug/libblood2_game.a build/debug/libengine.a build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a -pthread -lm
done
