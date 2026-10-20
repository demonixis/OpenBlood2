set -eu
# Run from Blood2. The before timing executables were preserved at baseline d1ddceb;
# build the frame probe with that revision using the same compiler/options below.
# The equivalence control adapts only shape access to the current private record layout.
python3 - <<'PYBASE'
from pathlib import Path
import subprocess
s=subprocess.check_output(['git','show','d1ddceb972fa0052b93abe27eaed2f786c1531df:src/engine/objects.cpp'],text=True)
s=s.replace('object->container =','object->container_ =').replace('slot.object->container)','slot.object->container())')
Path('/tmp/b2-container-index-old-objects.cpp').write_text(s)
PYBASE
for cfg in debug release; do
 spd=spdlog
 extra=
 if [ "$cfg" = debug ]; then spd=spdlogd; extra=/tmp/b2-session-profile.cpp; fi
 c++ -O2 -std=c++20 -I src -I src/game/legacy -I src/game/original -I build/$cfg/generated -I build/$cfg/_deps/sdl3-src/include /tmp/b2-frame-probe.cpp $extra src/app/model_scene.cpp src/app/sprite_scene.cpp -o /tmp/b2-container-index-$cfg-after build/$cfg/libblood2_game.a build/$cfg/libengine.a build/$cfg/_deps/sdl3-build/libSDL3.a build/$cfg/_deps/spdlog-build/lib$spd.a -pthread -lm
done
c++ -O0 -g -std=c++20 -I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include -c /tmp/b2-container-index-old-objects.cpp -o /tmp/b2-container-index-old-objects.o
for version in before after; do
 extra=
 if [ "$version" = before ]; then extra=/tmp/b2-container-index-old-objects.o; fi
 c++ -O2 -std=c++20 -I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include /tmp/b2-container-index-equivalence.cpp src/app/model_scene.cpp src/app/sprite_scene.cpp $extra -o /tmp/b2-container-index-equivalence-$version build/debug/libblood2_game.a build/debug/libengine.a build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a -pthread -lm
done
