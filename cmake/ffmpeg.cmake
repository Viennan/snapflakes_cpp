# ffmpeg.cmake
# Modern CMake configuration for FFmpeg components

function(find_ffmpeg)
    set(options)
    set(oneValueArgs VERSION)
    set(multiValueArgs COMPONENTS)

    cmake_parse_arguments(FFMPEG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT FFMPEG_COMPONENTS)
        message(STATUS "No ffmpeg components specified, using default (all): avformat, avcodec, avutil, avdevice, avfilter, swscale, swresample, postproc")
        set(FFMPEG_COMPONENTS
            avformat
            avcodec
            avutil
            avdevice
            avfilter
            swscale
            swresample
            postproc
        )
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)

        foreach(comp IN LISTS FFMPEG_COMPONENTS)
            # pkg-config search name
            set(pc_name "lib${comp}")

            # target name, eg：ffmpeg::avformat
            set(target_name "ffmpeg::${comp}")

            if (TARGET ${target_name})
                continue()
            endif()

            # construct version requirement
            if (FFMPEG_VERSION)
                set(version_expr "${pc_name}>=${FFMPEG_VERSION}")
            else()
                set(version_expr "${pc_name}")
            endif()

            pkg_check_modules(PC_${comp} REQUIRED IMPORTED_TARGET ${version_expr})

            # create imported target with unified name prefix
            add_library(${target_name} INTERFACE IMPORTED)

            # include dirs
            target_include_directories(${target_name}
                INTERFACE ${PC_${comp}_INCLUDE_DIRS}
            )

            # compile flags
            target_compile_options(${target_name}
                INTERFACE ${PC_${comp}_CFLAGS_OTHER}
            )

            # link libs
            target_link_libraries(${target_name}
                INTERFACE PkgConfig::PC_${comp}
            )
        endforeach()

    else()
        message(FATAL_ERROR "find_ffmpeg() is not supported on ${CMAKE_SYSTEM_NAME}")
    endif()

    # create a combined interface target that includes all found components
    if (NOT TARGET ffmpeg::ffmpeg)
        add_library(ffmpeg::ffmpeg INTERFACE IMPORTED)
        foreach(comp IN LISTS FFMPEG_COMPONENTS)
            target_link_libraries(ffmpeg::ffmpeg
                INTERFACE ffmpeg::${comp}
            )
        endforeach()
    endif()

    # export found components
    set(FFMPEG_FOUND_COMPONENTS ${FFMPEG_COMPONENTS} PARENT_SCOPE)

endfunction()