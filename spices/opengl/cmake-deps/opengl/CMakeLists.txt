# spices/opengl/cmake-deps/opengl -- real CMake targets for glfw and glad.
#
# Fetching these two directly left the manifest asking for libraries that do
# not exist under those names:
#
#   * glfw's target is `glfw` but its OUTPUT_NAME is `glfw3`, so the archive
#     is libglfw3.a and it lands in <BINARY_DIR>/src, not <BINARY_DIR>.  With
#     no :targets the manifest guessed `-lglfw` in <BINARY_DIR>: wrong name,
#     wrong directory.
#   * glad v2.0.6 is a Python *generator*, not a library.  Its checkout has no
#     root CMakeLists and builds nothing, so <glad/gl.h> was never produced
#     and `-lglad` never existed.  The loader has to be generated.
#
# Declaring this directory as a :path cmake-dep with
# :targets ["glfw" "glad"] makes tur read include dirs from each target's
# INTERFACE_INCLUDE_DIRECTORIES and link dirs from $<TARGET_FILE_DIR:...>,
# which is correct by construction.
cmake_minimum_required(VERSION 3.20)
project(opengl_deps C)

include(FetchContent)

# ---------------------------------------------------------------------------
# glfw
# ---------------------------------------------------------------------------
# glfw 3.4 builds the X11 and Wayland backends on Linux by default, and the
# Wayland one needs wayland-scanner + wayland-protocols at configure time.  A
# headless CI runner has neither, so the configure aborted with "Failed to
# find wayland-scanner" and no dep got built at all.  X11 alone is enough to
# compile and link these bindings; raylib's vendored glfw is configured the
# same way.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# Make the archive's name match the target's, so the -l the manifest derives
# from the target name resolves.
set_target_properties(glfw PROPERTIES OUTPUT_NAME glfw)

# ---------------------------------------------------------------------------
# glad
# ---------------------------------------------------------------------------
FetchContent_Declare(glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad
  GIT_TAG        v2.0.6
)
FetchContent_MakeAvailable(glad)

set(GLAD_CMAKE_DIR   "${glad_SOURCE_DIR}/cmake" CACHE STRING "" FORCE)
set(GLAD_SOURCES_DIR "${glad_SOURCE_DIR}"       CACHE STRING "" FORCE)
include("${glad_SOURCE_DIR}/cmake/GladConfig.cmake")

# REPRODUCIBLE pins the bundled Khronos specification instead of fetching the
# latest one at build time -- the build should not change under us, and a CI
# runner should not need to reach khronos.org.
glad_add_library(glad STATIC REPRODUCIBLE API gl:core=3.3)
