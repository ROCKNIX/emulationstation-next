# Unit tests for the pure code (#120)

```sh
cmake -S es-app/tests/unit -B build-tests   # here, not the top level: that needs SDL
cmake --build build-tests --target es-unit-tests
./build-tests/es-unit-tests                 # under a second; --help for options
```

The top level carries them too, under `-DES_BUILD_TESTS=ON` (default OFF, so an
image build never sees them).

**Belongs here:** functions that read nothing, ask nothing and draw nothing -- a
parser, a label, a rule about a string, a schedule. Today `es-app/src/CloudText.cpp`,
`es-app/src/CheevosRetry.cpp` (#175) and `es-app/src/OfflineAchievementsText.cpp`
(#180, the offline proxy's JSON, read with rapidjson -- pass
`-DRAPIDJSON_INCLUDE_DIR=<dir>` on a host without it), `es-app/src/CheevosIndex.cpp`
(#186 PL-08, what the game index owes a game), `es-core/src/utils/TimeText.cpp`
(#195, the time of day beside a save's date: the 12-hour switch and a locale
with no AM/PM), `es-app/src/WifiText.cpp` (#191, the Wi-Fi rows' reading of what
`wifictl` prints: the saved networks, the joined one, a forget's and a join's
outcome, and the picker's rows from what is in range),
`Utils::String::maskSecrets` in `es-core/src/utils/StringUtil.cpp` (#177, the
credential mask every logged command line goes through),
`es-app/src/SaveStateJobQueue.cpp` (#205, the save state manager's job queue:
one job at a time, a file pending deletion hides its tile while a copy hides
nothing, nothing queued for deletion twice, the finished count a page watches),
`es-core/src/utils/OfflineProxyUrl.cpp` (#199, the offline proxy's address and
whether a URL is on it -- the one definition the pages' requests and
`WebImageComponent`'s store-only header share) and the log's rules in
`es-core/src/LogPolicy.h` (#178, header-only: the level tags, the `LogLevel`
setting, which lines also reach stderr), `es-app/src/CaptureRotationText.cpp`
(#245, a game's rotation record and the launch log's turn),
`es-app/src/DisplayAspectText.cpp` (#243, which game a screenshot belongs to),
`es-core/src/utils/CommandLineUtil.h` (#308 F-CS-33, header-only: the option
swap the save state manager makes in a launch command),
`es-app/src/LaunchCommand.h` (#308 F-ES-20, header-only: what a finished launch
command says it launched, read from the same words) and
`es-core/src/resources/TabStops.h` (#308 F-ES-11, header-only: where a tabbed
text's columns start), tested with doctest (`external/doctest/doctest.h`). The
list of record is `add_executable(es-unit-tests ...)` in `CMakeLists.txt`: the
binary compiles the sources it names, and no more. (This page counted them,
and the count and the list went stale apart -- #308 F-ES-29.)

**Does not:** anything touching `Window`, `Settings`, `SystemConf`, a font, a
file or a script. Extract the pure core, leave the shell where it is, and test
the rest on the VM (`tools/vm-qa`).

**The one exception has a binary of its own:** `es-file-tests`
(`AtomicFileTests.cpp`, audit #307) checks `es-core/src/utils/AtomicFileUtil.cpp`
-- the whole-file writer, the reader and the settings lock `SystemConf` and
`Settings` go through -- against real files in a scratch directory it makes
under `$TMPDIR` (or `/tmp`) and removes, forking the processes that play the
other writer or the other waiter. Built by the same `cmake --build build-tests`
and run as `./build-tests/es-file-tests`; POSIX only, like the guarantees it
checks. `es-unit-tests` stays the binary that touches nothing.
`es-file-tests-win32` (`AtomicFileWin32Tests.cpp`) compiles the same
`AtomicFileUtil.cpp` with `_WIN32` defined for that one object, so the
Windows branches' contract in the header is checked on this host too.

`es-conf-tests` (`SystemConfTests.cpp`, the audit of the fix round) goes one
step further and compiles the shipped `es-core/src/SystemConf.cpp` with
`AtomicFileUtil.cpp`, the log, `Settings` and `Paths` defined in the test file
as doubles and SDL's clock under `fakes/SDL_timer.h`: the load, the reload,
the save and the record, as `SystemConf` strings `AtomicFileUtil`'s pieces
together, against a scratch `system.cfg` and a scratch settings lock
(`SystemConfTestAccess`, a friend `SystemConf.h` names, gives each case a
fresh instance). Run as `./build-tests/es-conf-tests`.

**Elsewhere:** `tests/app-unit/` (audit #307, stream E2) builds four more
binaries the same way -- `app-unit-tests` for header-only rules the
application calls (the journey record, the rescan's merge, the run lock, the
long-job pages' fitting, the window's post gate), and `proxycards-tests`,
`bookkeeper-tests` and `jobs-tests`, which compile the shipped
`ProxyCards.cpp`, `SaveStateBookkeeper.cpp`, `CloudTransferJob.cpp` and
`OfflineScanJob.cpp` against doubles under `tests/app-unit/fakes/`, with
real processes where the code runs them. And `tests/*.py` extract a shipped function from a
source file, compile it against doubles and run it (`python3 tests/<name>.py`).
