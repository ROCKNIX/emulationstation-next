# Tests for EmulationStation application code outside es-app/tests/unit
# (audit #307 stream E2), built with the host compiler and AddressSanitizer,
# no SDL, no cross toolchain:
#
#   cmake -S tests/app-unit -B build-app-unit && cmake --build build-app-unit
#   ./build-app-unit/app-unit-tests && ./build-app-unit/proxycards-tests
#   ./build-app-unit/bookkeeper-tests && ./build-app-unit/jobs-tests
#
# app-unit-tests: header-only pure code the application calls (the journey
# tiers a settings-first restore carries across its restart; the folder
# merge a rescan uses to keep every holder's FileData alive; who holds the
# offline achievements' run lock, with real files and processes; the long-job
# pages' one-line fitting; a worker's post to a window main() is letting go).
# What a finished launch command says it launched is pure string code and
# lives with the rest in es-app/tests/unit (LaunchCommandTests.cpp).
#
# proxycards-tests: the shipped es-app/src/ProxyCards.cpp itself, compiled
# against the small doubles under fakes/ -- the window, the card, the cloud
# sync, the settings and the proxy's answers -- so its threads, waits and
# outcomes run as they do on a device. The source is copied into the build
# tree at configure time so its quoted includes find the doubles and not
# the real headers beside it; a change to the source re-runs the copy.

cmake_minimum_required(VERSION 3.10)
project("es-app-unit-e2" CXX)

get_filename_component(ES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-g -fno-omit-frame-pointer -fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)
find_package(Threads REQUIRED)

add_executable(app-unit-tests
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/JourneyTiersTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/FolderMergeTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RunLockTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/TextFitTests.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/AppWindowTests.cpp
)
target_link_libraries(app-unit-tests Threads::Threads)
target_include_directories(app-unit-tests PRIVATE
	${ES_ROOT}/es-app/src
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)

configure_file(${ES_ROOT}/es-app/src/ProxyCards.cpp ${CMAKE_BINARY_DIR}/under-test/ProxyCards.cpp COPYONLY)
add_executable(proxycards-tests
	${CMAKE_CURRENT_SOURCE_DIR}/ProxyCardsTests.cpp
	${CMAKE_BINARY_DIR}/under-test/ProxyCards.cpp
	${ES_ROOT}/es-app/src/CloudText.cpp
	${ES_ROOT}/es-core/src/utils/StringUtil.cpp
)
target_include_directories(proxycards-tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
target_include_directories(proxycards-tests PRIVATE
	${ES_ROOT}/es-app/src
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
target_link_libraries(proxycards-tests Threads::Threads)

# jobs-tests: the shipped es-app/src/CloudTransferJob.cpp and
# OfflineScanJob.cpp -- the transfer and scan pages' runners -- against the
# doubles, running real commands in real process groups, so a stop, a
# cancel and a post are the ones a device makes.
configure_file(${ES_ROOT}/es-app/src/CloudTransferJob.cpp ${CMAKE_BINARY_DIR}/under-test/CloudTransferJob.cpp COPYONLY)
configure_file(${ES_ROOT}/es-app/src/OfflineScanJob.cpp ${CMAKE_BINARY_DIR}/under-test/OfflineScanJob.cpp COPYONLY)
add_executable(jobs-tests
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/JobsTests.cpp
	${CMAKE_BINARY_DIR}/under-test/CloudTransferJob.cpp
	${CMAKE_BINARY_DIR}/under-test/OfflineScanJob.cpp
	${ES_ROOT}/es-app/src/CloudText.cpp
	${ES_ROOT}/es-core/src/utils/StringUtil.cpp
)
target_include_directories(jobs-tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
target_include_directories(jobs-tests PRIVATE
	${ES_ROOT}/es-app/src
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
target_link_libraries(jobs-tests Threads::Threads)

# bookkeeper-tests: the shipped es-app/src/SaveStateBookkeeper.cpp with its
# pure queue, against doubles for the script runner and the log, and the
# cloud transfer lock at a path of the build's own (CLOUD_SYNC_LOCK_PATH),
# so a real process can hold it (audit #307 PL-068).
configure_file(${ES_ROOT}/es-app/src/SaveStateBookkeeper.cpp ${CMAKE_BINARY_DIR}/under-test/SaveStateBookkeeper.cpp COPYONLY)
add_executable(bookkeeper-tests
	${CMAKE_CURRENT_SOURCE_DIR}/BookkeeperTests.cpp
	${CMAKE_BINARY_DIR}/under-test/SaveStateBookkeeper.cpp
	${ES_ROOT}/es-app/src/SaveStateJobQueue.cpp
	${ES_ROOT}/es-core/src/utils/AtomicFileUtil.cpp
	${ES_ROOT}/es-core/src/utils/StringUtil.cpp
)
target_compile_definitions(bookkeeper-tests PRIVATE CLOUD_SYNC_LOCK_PATH="${CMAKE_BINARY_DIR}/cloud_sync.lock")
target_include_directories(bookkeeper-tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
target_include_directories(bookkeeper-tests PRIVATE
	${ES_ROOT}/es-app/src
	${ES_ROOT}/es-core/src
	${ES_ROOT}/external
)
target_link_libraries(bookkeeper-tests Threads::Threads)
