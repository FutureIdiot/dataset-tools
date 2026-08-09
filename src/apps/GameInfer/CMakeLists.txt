project(GameInfer VERSION 0.0.0.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp)
list(FILTER _src EXCLUDE REGEX "/workers/")
add_executable(${PROJECT_NAME} ${_src})

find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Concurrent
        game-infer::game-infer
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(${PROJECT_NAME} PRIVATE .)

qt_add_translations(${PROJECT_NAME}
        TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/GameInfer_zh_CN.ts
        RESOURCE_PREFIX /i18n
)

set(SEPARATOR_WORKER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/workers/separator_worker.py
        ${CMAKE_CURRENT_SOURCE_DIR}/workers/pyproject.toml
        ${CMAKE_CURRENT_SOURCE_DIR}/workers/uv.lock
        ${CMAKE_CURRENT_SOURCE_DIR}/workers/.python-version
)
target_sources(${PROJECT_NAME} PRIVATE ${SEPARATOR_WORKER_FILES})
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${SEPARATOR_WORKER_FILES})
set(SEPARATOR_WORKER_OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/separator-worker")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SEPARATOR_WORKER_OUTPUT_DIR}
)
foreach (_worker_file IN LISTS SEPARATOR_WORKER_FILES)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_worker_file}"
            "${SEPARATOR_WORKER_OUTPUT_DIR}"
    )
endforeach ()

option(GAMEINFER_REQUIRE_SEPARATOR_UV "Fail configuration when the separator runtime launcher is unavailable" OFF)
find_program(GAMEINFER_SEPARATOR_UV_EXECUTABLE NAMES uv
        DOC "uv executable bundled with GameInfer for the managed separator runtime")
if (GAMEINFER_SEPARATOR_UV_EXECUTABLE)
    get_filename_component(GAMEINFER_SEPARATOR_UV_REALPATH
            "${GAMEINFER_SEPARATOR_UV_EXECUTABLE}" REALPATH)
    set(SEPARATOR_RUNTIME_OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/separator-runtime")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SEPARATOR_RUNTIME_OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GAMEINFER_SEPARATOR_UV_REALPATH}"
            "${SEPARATOR_RUNTIME_OUTPUT_DIR}/uv${CMAKE_EXECUTABLE_SUFFIX}"
    )
else ()
    if (GAMEINFER_REQUIRE_SEPARATOR_UV)
        message(FATAL_ERROR "GameInfer separator runtime: uv is required but was not found")
    endif ()
    message(WARNING "GameInfer separator runtime: uv was not found and will not be bundled")
endif ()

set(GAMEINFER_SEPARATOR_DEFAULT_MODEL "" CACHE FILEPATH
        "Optional separator model to bundle as GameInfer's small default")
option(GAMEINFER_REQUIRE_SEPARATOR_DEFAULT_MODEL
        "Fail configuration when the bundled default separator model is unavailable" OFF)
if (GAMEINFER_SEPARATOR_DEFAULT_MODEL AND EXISTS "${GAMEINFER_SEPARATOR_DEFAULT_MODEL}")
    get_filename_component(GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH
            "${GAMEINFER_SEPARATOR_DEFAULT_MODEL}" REALPATH)
    get_filename_component(GAMEINFER_SEPARATOR_DEFAULT_MODEL_NAME
            "${GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH}" NAME)
    set(SEPARATOR_MODEL_OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/model/separator")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
            "${GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SEPARATOR_MODEL_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH}"
            "${SEPARATOR_MODEL_OUTPUT_DIR}/${GAMEINFER_SEPARATOR_DEFAULT_MODEL_NAME}"
    )
elseif (GAMEINFER_REQUIRE_SEPARATOR_DEFAULT_MODEL)
    message(FATAL_ERROR "GameInfer separator model: bundled default model is required but unavailable")
endif ()

if (APPLE)
    install(FILES ${SEPARATOR_WORKER_FILES} DESTINATION GameInfer.app/Contents/MacOS/separator-worker)
    if (GAMEINFER_SEPARATOR_UV_EXECUTABLE)
        install(PROGRAMS "${GAMEINFER_SEPARATOR_UV_REALPATH}"
                DESTINATION GameInfer.app/Contents/MacOS/separator-runtime
                RENAME uv)
    endif ()
    if (GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH)
        install(FILES "${GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH}"
                DESTINATION GameInfer.app/Contents/MacOS/model/separator)
    endif ()
else ()
    install(FILES ${SEPARATOR_WORKER_FILES} DESTINATION separator-worker)
    if (GAMEINFER_SEPARATOR_UV_EXECUTABLE)
        install(PROGRAMS "${GAMEINFER_SEPARATOR_UV_REALPATH}"
                DESTINATION separator-runtime
                RENAME uv${CMAKE_EXECUTABLE_SUFFIX})
    endif ()
    if (GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH)
        install(FILES "${GAMEINFER_SEPARATOR_DEFAULT_MODEL_REALPATH}" DESTINATION model/separator)
    endif ()
endif ()


if (WIN32)
    include(${PROJECT_CMAKE_MODULES_DIR}/winrc.cmake)

    if (NOT ${CMAKE_BUILD_TYPE} MATCHES "Deb")
        set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif ()

    if ("$ENV{WindowsSDKVersion}" MATCHES [[^([0-9.]*)(\\|/)?$]])
        set(WINDOWS_SDK_VERSION "${CMAKE_MATCH_1}")
    else ()
        message(WARNING "Failed to parse Windows SDK version from ENV{WindowsSDKVersion}.")
    endif ()

    set(WINDOWS_SDK_DIR "$ENV{WindowsSdkDir}")

    if (WINDOWS_SDK_DIR AND WINDOWS_SDK_VERSION)
        include_directories("${WINDOWS_SDK_DIR}Include/${WINDOWS_SDK_VERSION}/um")
        link_directories("${WINDOWS_SDK_DIR}Lib/${WINDOWS_SDK_VERSION}/um/x64")

        target_link_libraries(${PROJECT_NAME} PRIVATE dxgi)
    else ()
        message(WARNING "Windows SDK not found. Please ensure Windows SDK is installed.")
    endif ()
endif ()

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif ()

set_property(TARGET DeployedTargets APPEND PROPERTY TARGETS ${PROJECT_NAME})
