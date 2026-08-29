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
#     and `-lglad` never existed.  The loader is now vendored pre-generated
#     under glad/ -- see the note above that target.
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

# glfw's OUTPUT_NAME is glfw3, so its archive is libglfw3.a while its target is
# `glfw`. That mismatch used to need a rename here -- the manifest derived `-l`
# from the target *name*, so `-lglfw` did not resolve. rjungemann/turmeric#791
# made tur link by $<TARGET_FILE:glfw>, which is the artifact CMake actually
# produces, so the rename is gone and glfw builds under its own name.

# ---------------------------------------------------------------------------
# glad -- vendored, pre-generated
# ---------------------------------------------------------------------------
# glad is a Python *generator*: it renders the loader from Jinja2 templates.
# Running it at build time made jinja2 a hard build dependency of this spice --
# `ModuleNotFoundError: No module named 'jinja2'` fails the whole configure, and
# the interpreter CMake picks is not necessarily the one on PATH, so a venv is
# not enough to fix it locally.
#
# So we vendor the generator's output instead of the generator, which is exactly
# what raylib does with its own copy (raylib/src/external/glad.h is checked in,
# and raylib's build needs no Python at all). Same API, no toolchain
# prerequisite, and one fewer thing to fetch.
#
# glad/ was produced by glad 2.0.6 with:
#
#     python3 -m glad --api gl:core=3.3 --reproducible c
#
# and the full provenance (generator version, spec, extension list, options) is
# in the header of glad/include/glad/gl.h. Regenerate only when the target GL
# version changes; `--reproducible` pins the bundled Khronos spec so the output
# is deterministic and the build never reaches khronos.org.
add_library(glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/gl.c")
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")
