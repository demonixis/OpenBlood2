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
    namespace = "net.demonixis.openblood2"
    compileSdk = 36

    defaultConfig {
        applicationId = "net.demonixis.openblood2"
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
        // Immersive Horizon OS OpenXR app; Meta requires min 32 / target 34.
        create("quest") {
            dimension = "device"
            minSdk = 32
            targetSdk = 34
            // The game is arm64 only; keep the loader AAR's other ABIs out of the APK.
            ndk {
                abiFilters += "arm64-v8a"
            }
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

dependencies {
    // Khronos OpenXR loader, dlopen'ed by SDL (libopenxr_loader.so). The version matches
    // the OpenXR headers vendored in the pinned SDL; AAR SHA-256 in Documentation/packaging.md.
    "questImplementation"("org.khronos.openxr:openxr_loader_for_android:1.1.63")
}
