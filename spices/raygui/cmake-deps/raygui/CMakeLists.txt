# spices/raygui/cmake-deps/raygui -- a real CMake target for raygui.
#
# raygui ships as a single header (src/raygui.h) with no CMakeLists of its
# own, which `tur fetch` cannot make anything of:
#
#   * with no :targets, the manifest's include dir comes from the
#     "${SOURCE_DIR}/include else ${SOURCE_DIR}" heuristic -- and raygui puts
#     its header in src/, so <raygui.h> was never on the include path.  The
#     compiler wraps hoisted includes in `#if __has_include(...)`, so the
#     miss was silent: every Gui* call fell through to an implicit
#     declaration returning int, which is where "incompatible types when
#     assigning to type 'Font' from type 'int'" and "invalid initializer"
#     (GuiGrid returns a Vector2) came from.
#   * the manifest also emitted `-lraygui` for a library that does not
#     exist, so the link failed with "cannot find -lraygui".
#   * and nothing anywhere defined RAYGUI_IMPLEMENTATION, so even a correct
#     -l would have found no Gui* symbols to link against.
#
# This shim is declared as a :path cmake-dep with :targets ["raygui"], which
# makes tur source the include dirs from the target's
# INTERFACE_INCLUDE_DIRECTORIES and the link dir from $<TARGET_FILE_DIR:...>
# instead of the heuristics -- the same escape hatch yyjson uses.
cmake_minimum_required(VERSION 3.20)
project(raygui_shim C)

include(FetchContent)

FetchContent_Declare(raygui_upstream
  GIT_REPOSITORY https://github.com/raysan5/raygui
  GIT_TAG        4.0
)
FetchContent_MakeAvailable(raygui_upstream)

# One translation unit carries RAYGUI_IMPLEMENTATION; every other includer
# gets declarations only.
add_library(raygui STATIC "${CMAKE_CURRENT_LIST_DIR}/raygui_impl.c")

target_include_directories(raygui PUBLIC "${raygui_upstream_SOURCE_DIR}/src")

# raygui.h needs raylib's types. `raylib` is declared by the raylib spice in
# the same generated CMakeLists; CMake resolves a plain target name at
# generate time, so it does not matter that raylib's block is emitted after
# this add_subdirectory().
target_link_libraries(raygui PUBLIC raylib)
