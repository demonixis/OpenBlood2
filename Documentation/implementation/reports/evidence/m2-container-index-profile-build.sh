set -eu
inc='-I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include'
for file in server_containers server_physics container objects; do
 c++ -O0 -g -std=c++20 $inc -c /tmp/b2-cost-$file.cpp -o /tmp/b2-cost-$file.o
done
c++ -O2 -std=c++20 $inc /tmp/b2-cost-frame.cpp src/app/model_scene.cpp src/app/sprite_scene.cpp /tmp/b2-cost-server_containers.o /tmp/b2-cost-server_physics.o /tmp/b2-cost-container.o /tmp/b2-cost-objects.o -o /tmp/b2-cost-frame build/debug/libblood2_game.a build/debug/libengine.a build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a -pthread -lm
/tmp/b2-cost-frame '/home/yann/.steam/steam/steamapps/common/Blood II/' 01_SubwayA > /tmp/b2-cost-frame.log
