plugins {
    id("com.android.application") version "9.4.1" apply false
}

// Keep Gradle outputs in the CMake build tree, never in the source tree.
layout.buildDirectory.set(file(providers.gradleProperty("blood2.buildDir").get() + "/root"))
