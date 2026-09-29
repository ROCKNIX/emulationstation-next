# The unit tests for EmulationStation's pure code (#120 box 4).
#
# Standalone on purpose: this file is a project of its own when it is the
# top of the tree, so the tests build with the host compiler and no SDL,
# no freetype, no curl, and no cross toolchain --
#
#   cmake -S es-app/tests/unit -B build-tests && cmake --build build-tests
#   ./build-tests/es-unit-tests
#
# and it is also picked up by the top-level CMakeLists.txt under
# -DES_BUILD_TESTS=ON, which defaults OFF so the image build never sees it.

cmake_minimum_required(VERSION 3.10)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project("es-unit-tests" CXX)
endif()

get_filename_component(ES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)

# The sources below and no more (the list is the record; a count beside it
# went stale, #308 F-ES-29): the pure text, the offline
# proxy's answers, the string helpers they call (with the credential mask
# every logged command line goes through, fork #177), the token check's
# retry schedule, the index's rule for what a game is owed, the time of day
# a save's date carries (fork #195), the Wi-Fi rows' reading of wifictl
# (fork #191), and the offline proxy's address with the test for a URL on it
# (fork #199), the save state manager's job queue (fork #205 / #206: one job
# at a time, a file pending deletion hides its tile, a copy hides nothing) -- plus the
# log's header-only rules (es-core/src/LogPolicy.h,
# fork #178), and the header-only option swap the save state manager makes
# in a launch command (es-core/src/utils/CommandLineUtil.h, #308 F-CS-33), and
# where a tabbed text's columns start (es-core/src/resources/TabStops.h,
# header-only, #308 F-ES-11). Anything that needs a window, a font or a file does not belong
# in this binary -- see README.md.
add_executable(es-unit-tests
	${CMAKE_CURRENT_SOURCE_DIR}/CloudTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CheevosRetryTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OfflineAchievementsTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CheevosIndexTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/TimeTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/MaskSecretsTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LogPolicyTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/WifiTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OfflineProxyUrlTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/SaveStateJobQueueTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CaptureRotationTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DisplayAspectTextTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CommandLineTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/TabStopsTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LaunchCommandTests.cpp
	${ES_ROOT}/es-app/src/CloudText.cpp
	${ES_ROOT}/es-app/src/CheevosRetry.cpp
	${ES_ROOT}/es-app/src/OfflineAchievementsText.cpp
	${ES_ROOT}/es-app/src/CheevosIndex.cpp
	${ES_ROOT}/es-app/src/WifiText.cpp
	${ES_ROOT}/es-app/src/SaveStateJobQueue.cpp
	${ES_ROOT}/es-app/src/CaptureRotationText.cpp
	${ES_ROOT}/es-app/src/DisplayAspectText.cpp
	${ES_ROOT}/es-core/src/utils/StringUtil.cpp
	${ES_ROOT}/es-core/src/utils/TimeText.cpp
	${ES_ROOT}/es-core/src/utils/OfflineProxyUrl.cpp
)

# OfflineAchievementsText reads JSON with rapidjson, header-only, which the
# image build takes from the toolchain's sysroot. On a host without it, name
# a copy: -DRAPIDJSON_INCLUDE_DIR=<build root>/build/rapidjson-*/include. A
# missing library stops the configure here rather than building a binary
# that silently tests less (engineering-practices.md, guards fail closed).
find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h)
if(NOT RAPIDJSON_INCLUDE_DIR)
	message(FATAL_ERROR "rapidjson/document.h was not found; pass -DRAPIDJSON_INCLUDE_DIR=<dir holding rapidjson/>")
endif()

target_include_directories(es-unit-tests PRIVATE
	${ES_ROOT}/es-app/src
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
	${RAPIDJSON_INCLUDE_DIR}
)

set_target_properties(es-unit-tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# The settings files' writer, reader and lock (audit #307: PL-024, PL-041,
# PL-063, PL-064, PL-065; #308 F-ES-07, F-ES-08), against real files: a
# binary of its own, so es-unit-tests stays the one that touches nothing.
# es-file-tests makes a scratch directory under $TMPDIR (or /tmp), forks
# the processes that play the other writer or the other waiter, and removes
# the directory again. POSIX only, like AtomicFileUtil's own guarantees.
add_executable(es-file-tests
	${CMAKE_CURRENT_SOURCE_DIR}/AtomicFileTests.cpp
	${ES_ROOT}/es-core/src/utils/AtomicFileUtil.cpp
)
target_include_directories(es-file-tests PRIVATE
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
find_package(Threads REQUIRED)
target_link_libraries(es-file-tests PRIVATE Threads::Threads)
# The tree whose sources a case reads (the PL-069 guard on ThreadedCloudSync).
target_compile_definitions(es-file-tests PRIVATE ES_ROOT_DIR="${ES_ROOT}")
set_target_properties(es-file-tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# SystemConf itself, against real files (the audit of the fix round): the
# shipped SystemConf.cpp and AtomicFileUtil.cpp, with the log, Settings,
# Paths and SDL's clock as doubles -- the first three defined in
# SystemConfTests.cpp, SDL_timer.h under fakes/, which the angle-bracket
# include finds before any system copy. A binary of its own for the same
# reason es-file-tests is one: it touches files.
add_executable(es-conf-tests
	${CMAKE_CURRENT_SOURCE_DIR}/SystemConfTests.cpp
	${ES_ROOT}/es-core/src/SystemConf.cpp
	${ES_ROOT}/es-core/src/utils/AtomicFileUtil.cpp
)
target_include_directories(es-conf-tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/fakes
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
target_link_libraries(es-conf-tests PRIVATE Threads::Threads)
set_target_properties(es-conf-tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# AtomicFileUtil's _WIN32 branches, compiled on this host with _WIN32
# defined for that one object (audit of the fixes of #307, claude
# G-E1-06): the header's contract for them is checked where it can run.
# Only the branches that use standard C and C++ -- which is all of them --
# so the object builds here as it does on Windows.
add_library(atomicfile-win32-branch OBJECT ${ES_ROOT}/es-core/src/utils/AtomicFileUtil.cpp)
target_compile_definitions(atomicfile-win32-branch PRIVATE _WIN32)
target_include_directories(atomicfile-win32-branch PRIVATE ${ES_ROOT}/es-core/src)
set_target_properties(atomicfile-win32-branch PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_executable(es-file-tests-win32
	${CMAKE_CURRENT_SOURCE_DIR}/AtomicFileWin32Tests.cpp
	$<TARGET_OBJECTS:atomicfile-win32-branch>
)
target_include_directories(es-file-tests-win32 PRIVATE
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
set_target_properties(es-file-tests-win32 PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

