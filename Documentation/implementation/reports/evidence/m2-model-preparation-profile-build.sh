set -eu
inc='-I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include'
for f in model model_view; do c++ -O0 -g -std=c++20 $inc -c /tmp/b2-model-profile-$f.cpp -o /tmp/b2-model-profile-$f.o; done
c++ -O2 -std=c++20 $inc /tmp/b2-model-profile-frame.cpp src/app/model_scene.cpp src/app/sprite_scene.cpp /tmp/b2-model-profile-model.o /tmp/b2-model-profile-model_view.o -o /tmp/b2-model-profile build/debug/libblood2_game.a build/debug/libengine.a build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a -pthread -lm
/tmp/b2-model-profile '/home/yann/.steam/steam/steamapps/common/Blood II/' 01_SubwayA > /tmp/b2-model-profile.log
