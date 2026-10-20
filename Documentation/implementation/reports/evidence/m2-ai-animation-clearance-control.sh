set -eu
# Run from Blood2 after building Debug. Override only the prior AIState.cpp
# against current checked services and layout, to isolate the regression.
git show 444035f86168a586c91c4b0ff5c6a77448a8b457:src/game/original/AIState.cpp > /tmp/b2-ai-resize-original-state.cpp
c++ -O2 -std=c++20 -I src -I src/game/legacy -I src/game/original -I build/debug/generated -I build/debug/_deps/sdl3-src/include tests/ai_runtime_tests.cpp /tmp/b2-ai-resize-original-state.cpp -o /tmp/b2-ai-resize-before build/debug/libblood2_game.a build/debug/libengine.a build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a -pthread -lm
set +e
/tmp/b2-ai-resize-before clearance_private "$1" > /tmp/b2-ai-resize-before.log 2>&1
control_status=$?
set -e
# This control must fail at the suspended-actor assertion, not merely fail to run.
test "$control_status" -eq 1
rg -q 'AI_Update: AI animation resize blocked' /tmp/b2-ai-resize-before.log
rg -q 'ai->RuntimeReady\(\)' /tmp/b2-ai-resize-before.log
