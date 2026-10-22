# Application identity, version codes, icons and platform bundles.
# See Documentation/packaging.md. project(VERSION) in CMakeLists.txt is the only
# version number; everything below derives from it.

set(BLOOD2_BUILD_NUMBER 0 CACHE STRING "Build number (0-99) appended to platform version codes")
if(NOT BLOOD2_BUILD_NUMBER MATCHES "^[0-9]$|^[1-9][0-9]$")
    message(FATAL_ERROR "BLOOD2_BUILD_NUMBER must be an integer from 0 to 99")
endif()
foreach(part IN ITEMS MINOR PATCH)
    if(PROJECT_VERSION_${part} GREATER 99)
        message(FATAL_ERROR "Version codes allow at most 99 for the ${part} component")
    endif()
endforeach()
# Monotonic integer for Android versionCode and Apple CFBundleVersion.
math(EXPR BLOOD2_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 1000000 + ${PROJECT_VERSION_MINOR} * 10000 + ${PROJECT_VERSION_PATCH} * 100 + ${BLOOD2_BUILD_NUMBER}")
set(BLOOD2_GAME_IDENTIFIER net.demonixis.openblood2)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(BLOOD2_IOS TRUE)
endif()
if(BLOOD2_IOS OR CMAKE_GENERATOR STREQUAL "Xcode")
    set(bundle_default ON)
else()
    set(bundle_default OFF)
endif()
option(BLOOD2_APPLE_BUNDLE "Build Apple .app bundles with icons and Info.plist" ${bundle_default})
set(BLOOD2_APPLE_TEAM_ID "" CACHE STRING "Apple development team; empty disables code signing")
if(BLOOD2_IOS AND NOT BLOOD2_APPLE_BUNDLE)
    message(FATAL_ERROR "iOS builds require BLOOD2_APPLE_BUNDLE")
endif()

set(blood2_platform_dir "${PROJECT_SOURCE_DIR}/platform")

# SDLActivity loads the Android game as libmain.so; other platforms build executables.
function(blood2_add_application target)
    if(ANDROID)
        add_library(${target} SHARED ${ARGN})
    else()
        add_executable(${target} ${ARGN})
    endif()
endfunction()

function(blood2_package_application target)
    set(icon blood2)
    set(BLOOD2_PLIST_NAME "OpenBlood 2")
    set(BLOOD2_PLIST_EXECUTABLE OpenBlood2)
    set(BLOOD2_PLIST_IDENTIFIER ${BLOOD2_GAME_IDENTIFIER})
    set(description "OpenBlood 2")
    get_target_property(file_name ${target} OUTPUT_NAME)
    set(generated "${PROJECT_BINARY_DIR}/generated/${target}")

    if(WIN32)
        set(BLOOD2_RC_ICON "${blood2_platform_dir}/windows/${icon}.ico")
        set(BLOOD2_RC_DESCRIPTION "${description}")
        set(BLOOD2_RC_FILE_NAME "${file_name}.exe")
        configure_file("${blood2_platform_dir}/windows/application.rc.in" "${generated}/application.rc" @ONLY)
        target_sources(${target} PRIVATE "${generated}/application.rc")
    endif()

    if(APPLE AND BLOOD2_APPLE_BUNDLE)
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME ${BLOOD2_PLIST_EXECUTABLE}
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BLOOD2_PLIST_IDENTIFIER}
            XCODE_ATTRIBUTE_MARKETING_VERSION ${PROJECT_VERSION}
            XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION ${BLOOD2_VERSION_CODE})
        if(BLOOD2_APPLE_TEAM_ID)
            set_target_properties(${target} PROPERTIES
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${BLOOD2_APPLE_TEAM_ID}
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
        else()
            set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
        endif()
        if(BLOOD2_IOS)
            set(plist "${blood2_platform_dir}/apple/ios/Info.plist.in")
            set(catalog "${blood2_platform_dir}/apple/ios/Assets.xcassets")
            target_sources(${target} PRIVATE "${catalog}")
            set_source_files_properties("${catalog}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
            set_target_properties(${target} PROPERTIES
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
                XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
        else()
            set(plist "${blood2_platform_dir}/apple/macos/Info.plist.in")
            set(BLOOD2_PLIST_ICON ${icon}.icns)
            set(icns "${blood2_platform_dir}/apple/macos/${icon}.icns")
            target_sources(${target} PRIVATE "${icns}")
            set_source_files_properties("${icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        endif()
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            set(BLOOD2_PLIST_MINIMUM_SYSTEM ${CMAKE_OSX_DEPLOYMENT_TARGET})
        else()
            set(BLOOD2_PLIST_MINIMUM_SYSTEM 26.0)
        endif()
        configure_file("${plist}" "${generated}/Info.plist" @ONLY)
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${generated}/Info.plist")
    endif()

    if(ANDROID)
        set_target_properties(${target} PROPERTIES OUTPUT_NAME main)
        blood2_android_package(${target})
    endif()
endfunction()

# Android: stage libmain.so per ABI and drive the Gradle project in platform/android.
function(blood2_android_package target)
    set(BLOOD2_ANDROID_FLAVOR phone CACHE STRING "Android APK flavor: phone or quest")
    set_property(CACHE BLOOD2_ANDROID_FLAVOR PROPERTY STRINGS phone quest)
    if(NOT BLOOD2_ANDROID_FLAVOR MATCHES "^(phone|quest)$")
        message(FATAL_ERROR "BLOOD2_ANDROID_FLAVOR must be phone or quest")
    endif()
    set(jni "${PROJECT_BINARY_DIR}/android/jniLibs")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${jni}/${ANDROID_ABI}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${target}>" "${jni}/${ANDROID_ABI}/libmain.so"
        VERBATIM)

    # The checked-in wrapper pins Gradle (and its checksum) for the pinned plugin.
    if(CMAKE_HOST_WIN32)
        set(gradle "${blood2_platform_dir}/android/gradlew.bat")
    else()
        set(gradle "${blood2_platform_dir}/android/gradlew")
    endif()
    # The NDK lives in <sdk>/ndk/<version>; Gradle needs the SDK root.
    if(DEFINED ENV{ANDROID_HOME})
        set(sdk "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT})
        set(sdk "$ENV{ANDROID_SDK_ROOT}")
    else()
        get_filename_component(sdk "${CMAKE_ANDROID_NDK}/../.." ABSOLUTE)
    endif()
    string(SUBSTRING ${BLOOD2_ANDROID_FLAVOR} 0 1 first)
    string(TOUPPER ${first} first)
    string(SUBSTRING ${BLOOD2_ANDROID_FLAVOR} 1 -1 rest)
    # Only Release builds the store variant (unsigned until signing is configured);
    # other configurations produce debug-signed, installable APKs.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(variant Release)
    else()
        set(variant Debug)
    endif()
    add_custom_target(blood2_apk
        COMMAND "${CMAKE_COMMAND}" -E env "ANDROID_HOME=${sdk}"
            "${gradle}" --no-daemon --console=plain
            -p "${blood2_platform_dir}/android"
            --project-cache-dir "${PROJECT_BINARY_DIR}/android/gradle-cache"
            "assemble${first}${rest}${variant}"
            "-Pblood2.buildDir=${PROJECT_BINARY_DIR}/android/gradle"
            "-Pblood2.jniLibs=${jni}"
            "-Pblood2.sdlJava=${sdl3_SOURCE_DIR}/android-project/app/src/main/java"
            "-Pblood2.versionName=${PROJECT_VERSION}"
            "-Pblood2.versionCode=${BLOOD2_VERSION_CODE}"
        DEPENDS ${target}
        USES_TERMINAL VERBATIM
        COMMENT "Assembling the ${BLOOD2_ANDROID_FLAVOR} ${variant} APK")
endfunction()
