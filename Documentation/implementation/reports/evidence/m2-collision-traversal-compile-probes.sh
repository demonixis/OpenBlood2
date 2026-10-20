set -eu
for cfg in debug release; do
  spd=spdlog
  extra=
  if [ "$cfg" = debug ]; then spd=spdlogd; extra=/tmp/b2-session-profile.cpp; fi
  c++ -O2 -std=c++20 -I src -I src/game/legacy -I src/game/original -I build/$cfg/generated -I build/$cfg/_deps/sdl3-src/include /tmp/b2-frame-probe.cpp $extra src/app/model_scene.cpp src/app/sprite_scene.cpp -o /tmp/b2-frame-probe-$cfg-after build/$cfg/libblood2_game.a build/$cfg/libengine.a build/$cfg/_deps/sdl3-build/libSDL3.a build/$cfg/_deps/spdlog-build/lib$spd.a -pthread -lm
done
