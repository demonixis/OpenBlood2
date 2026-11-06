# Baked GI files (.ob2gi) carried by local builds (gi_contracts.md lot G6, decision D3).
# Builds copy files and nothing else: never a bake. The files are commercial-derived, so they are
# never committed (.gitignore, the gi_policy test) and public CI configures BLOOD2_PACKAGE_GI OFF.
#
# The copies land where gi_search_roots() looks for the packaged copy, SDL_GetBasePath()/gi:
#   - beside the executable (single- and multi-config generators);
#   - Resources/gi in Apple bundles (the bundle root on iOS);
#   - assets/gi in the Android APK, through -Pblood2.assets.
# Each file is its own copy command found by a CONFIGURE_DEPENDS glob, so an empty or absent folder
# adds no build step, and a new bake is copied by the next build without relinking.

set(BLOOD2_GI_DATA_DIR "${PROJECT_SOURCE_DIR}/data/gi/baked" CACHE PATH "Baked GI files to package (lot G6)")
option(BLOOD2_PACKAGE_GI "Copy the baked GI files of BLOOD2_GI_DATA_DIR into local builds" ON)

function(blood2_package_gi target)
    if(NOT BLOOD2_PACKAGE_GI)
        return()
    endif()
    file(GLOB_RECURSE gi_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${BLOOD2_GI_DATA_DIR}/*.ob2gi")
    list(SORT gi_files)
    list(LENGTH gi_files count)
    message(STATUS "Packaged GI files: ${count} from ${BLOOD2_GI_DATA_DIR}")
    if(NOT gi_files)
        return()
    endif()
    if(APPLE AND BLOOD2_APPLE_BUNDLE)
        # The bundle copies them like any resource, keeping the worlds/ subfolders.
        foreach(file IN LISTS gi_files)
            file(RELATIVE_PATH relative "${BLOOD2_GI_DATA_DIR}" "${file}")
            get_filename_component(folder "${relative}" DIRECTORY)
            target_sources(${target} PRIVATE "${file}")
            set_source_files_properties("${file}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/gi/${folder}"
                                                             HEADER_FILE_ONLY TRUE)
        endforeach()
        return()
    endif()
    if(ANDROID)
        set(destination "${PROJECT_BINARY_DIR}/android/assets/gi")
    else()
        get_property(multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(multi)
            set(destination "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/gi")
        else()
            set(destination "${CMAKE_CURRENT_BINARY_DIR}/gi")
        endif()
    endif()
    set(outputs "")
    foreach(file IN LISTS gi_files)
        file(RELATIVE_PATH relative "${BLOOD2_GI_DATA_DIR}" "${file}")
        set(output "${destination}/${relative}")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${file}" "${output}"
            DEPENDS "${file}"
            COMMENT "Packaging GI file ${relative}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()
    add_custom_target(blood2_gi_data ALL DEPENDS ${outputs})
    add_dependencies(${target} blood2_gi_data)
endfunction()
