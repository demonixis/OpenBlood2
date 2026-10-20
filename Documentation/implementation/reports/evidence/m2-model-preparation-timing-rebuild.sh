set -eu
# Run the retained equivalence-setup.py first to extract baseline sampler/view/scene files.
base=/tmp/b2-model-prep-baseline
for cfg in debug release; do
 spd=spdlog
 opt='-O3 -DNDEBUG'
 extra=
 if [ "$cfg" = debug ]; then spd=spdlogd; opt='-O0 -g'; extra=/tmp/b2-session-profile.cpp; fi
 for unit in model model_view; do
  c++ $opt -std=c++20 -I "$base" -I src -I src/game/legacy -I src/game/original -I build/$cfg/generated -I build/$cfg/_deps/sdl3-src/include -c "$base/$unit.cpp" -o "$base/$cfg-$unit.o"
 done
 c++ -O2 -std=c++20 -I "$base" -I src -I src/game/legacy -I src/game/original -I build/$cfg/generated -I build/$cfg/_deps/sdl3-src/include /tmp/b2-frame-probe.cpp $extra "$base/model_scene.cpp" src/app/sprite_scene.cpp "$base/$cfg-model.o" "$base/$cfg-model_view.o" -o /tmp/b2-model-prep-$cfg-before build/$cfg/libblood2_game.a build/$cfg/libengine.a build/$cfg/_deps/sdl3-build/libSDL3.a build/$cfg/_deps/spdlog-build/lib$spd.a -pthread -lm
done
sh /tmp/b2-model-prep-compile.sh
