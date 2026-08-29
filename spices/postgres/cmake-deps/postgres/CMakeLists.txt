# spices/postgres/cmake-deps/postgres -- a `pq` target for the system libpq.
#
# libpq is taken from the system (libpq-dev / brew libpq), never built from
# source: there is no standalone libpq repo to FetchContent.
#
# Declaring it as a :prefer-system cmake-dep with
# :targets ["PostgreSQL::PostgreSQL"] got the include dirs and the link
# directory right and then asked the linker for the wrong library.  tur
# derives the -l name from the CMake target's basename, so
# PostgreSQL::PostgreSQL became `-lPostgreSQL` -- but the file is libpq.so,
# and every test died on "cannot find -lPostgreSQL".  The target name and the
# library's base name simply differ here, and nothing in the :cmake-deps
# surface lets them differ.
#
# So: find the package, then re-export what it found under a target whose
# name IS the library's base name.  With :targets ["pq"] tur reads
#   include_dirs <- $<TARGET_PROPERTY:pq,INTERFACE_INCLUDE_DIRECTORIES>
#   link_dirs    <- $<TARGET_FILE_DIR:pq>
#   link_libs    <- "pq"
# and all three are right by construction.
cmake_minimum_required(VERSION 3.20)
project(postgres_deps C)

# Homebrew keeps libpq keg-only, so FindPostgreSQL needs pointing at it.
if(APPLE AND NOT PostgreSQL_ROOT)
  find_program(_brew brew)
  if(_brew)
    execute_process(COMMAND "${_brew}" --prefix libpq
                    OUTPUT_VARIABLE _libpq_prefix
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(_libpq_prefix)
      set(PostgreSQL_ROOT "${_libpq_prefix}")
    endif()
  endif()
endif()

find_package(PostgreSQL)

if(NOT PostgreSQL_FOUND)
  message(FATAL_ERROR
    "libpq not found. Install it first:\n"
    "  Debian/Ubuntu: apt-get install libpq-dev\n"
    "  macOS:         brew install libpq")
endif()

# GLOBAL so the $<TARGET_FILE_DIR:pq> genexpr in the generated manifest, which
# is evaluated in the parent scope, can see the target.
add_library(pq UNKNOWN IMPORTED GLOBAL)
set_target_properties(pq PROPERTIES
  IMPORTED_LOCATION             "${PostgreSQL_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${PostgreSQL_INCLUDE_DIRS}")
