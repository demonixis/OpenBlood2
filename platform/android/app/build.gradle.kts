plugins {
    id("com.android.application")
}

// Supplied by cmake/Packaging.cmake: native libraries, the pinned SDL Java
// sources and the version derived from project(VERSION).
fun blood2(name: String): String =
    providers.gradleProperty("blood2.$name").orNull
        ?: error("Missing -Pblood2.$name; build through the CMake blood2_apk target")

layout.buildDirectory.set(file(blood2("buildDir") + "/app"))

android {
    namespace = "org.openblood2.game"
    compileSdk = 36

    defaultConfig {
        applicationId = "org.openblood2.game"
        minSdk = 29
        targetSdk = 36
        versionCode = blood2("versionCode").toInt()
        versionName = blood2("versionName")
    }

    flavorDimensions += "device"
    productFlavors {
        create("phone") {
            dimension = "device"
        }
        // Horizon OS 2D panel app; Meta recommends min 32 / target 34.
        create("quest") {
            dimension = "device"
            minSdk = 32
            targetSdk = 34
        }
    }

    sourceSets {
        getByName("main") {
            java.srcDir(blood2("sdlJava"))
            jniLibs.srcDir(blood2("jniLibs"))
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    packaging {
        jniLibs {
            // Retain RelWithDebInfo symbols for on-device crash diagnosis.
            keepDebugSymbols += "**/libmain.so"
        }
    }
}
