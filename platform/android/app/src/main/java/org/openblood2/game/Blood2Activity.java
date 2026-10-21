package org.openblood2.game;

import org.libsdl.app.SDLActivity;

/** Starts the game; SDL is linked statically into libmain.so. */
public class Blood2Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }
}
