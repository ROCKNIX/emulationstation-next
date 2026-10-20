# On-screen help prompt research

Research notes for diagnosing mismatches between EmulationStation's help bar and actual controller behavior. This records the current source paths and confirmed behavior; it does not change input behavior.

## Data flow

1. A view (or topmost GUI) supplies `std::vector<HelpPrompt>` from `getHelpPrompts()`. A prompt stores a first string (control/icon key), second string (label), and optional callback (`es-core/src/HelpPrompt.h`). Controller input is dispatched separately by the active view; `HelpComponent::onMouseClick()` can invoke the callback when the prompt is clicked with a mouse.
2. `ViewController::getHelpPrompts()` delegates to `mCurrentView->getHelpPrompts()` and appends `start` / `MENU` except in Kid mode (`es-app/src/views/ViewController.cpp`). This is why the system selector's menu prompt is present regardless of `SystemView`'s own prompt list.
3. `GuiComponent::updateHelpPrompts()` forwards the active component's prompts/style to `Window`; `Window::pushGui`, `removeGui`, view transitions, and other update sites refresh them. `Window::setHelpPrompts()` deduplicates repeated first keys, combines up/down and left/right only when labels match, sorts by a fixed control order, then passes the result to `HelpComponent`.
4. `HelpComponent::updateGrid()` converts each first key to an icon key via `InputConfig::buttonLabel()`, looks up a themed icon override from `HelpStyle::iconMap`, then falls back to built-in SVGs. It uppercases the second string for display. The bar is therefore generated from the active view's prompt list, not inferred from the input handler.

On screens with proportion below `1.4`, `HelpComponent::updateGrid()` truncates each displayed label at the first `(` (and at `/`). Therefore a source prompt such as `SAVE STATES (HOLD)` is rendered simply as `SAVE STATES`; the missing “HOLD” text does not show that the underlying behavior is a short press. Inspect the matching `MultiStateInput` handling before inferring what the control does.

## Where labels and controls come from

- System selector: `SystemView::getHelpPrompts()` (`es-app/src/views/SystemView.cpp`) starts with carousel prompts, then adds `BUTTON_OK / SELECT`, `x / RANDOM` (or NETPLAY), optional `y / SEARCH`, and a hard-coded `b` prompt for the system navigation drawer when manufacturer support is enabled. Its proposed user-facing label is `SYSTEMS LIST`; keep the prompt key as `b` when updating that label.
- System selector input: `SystemView::input()` dispatches `BUTTON_OK` to game-list selection, `BUTTON_BACK` to `showNavigationBar()`, `x` to random system, and `select` to the quit menu. It does not dispatch navigation-bar behavior on literal `b`; it dispatches on `BUTTON_BACK`.
- Game list: `ISimpleGameListView::getHelpPrompts()` (`es-app/src/views/gamelist/ISimpleGameListView.cpp`) composes the D-pad prompt, `BUTTON_BACK / BACK`, an OK-button long-press label controlled by `GameOptionsAtNorth`, `select / OPTIONS`, conditional `x` label (favorite, save states, or game options depending on settings/save-state availability/Kid mode), and `y / SEARCH / RANDOM`.
- Game-list input: `ISimpleGameListView::input()` dispatches short press via `mOKButton` to launch; `mXButton` to options or save snapshots according to `GameOptionsAtNorth`; `mYButton` to search; select to gamelist options; and `BUTTON_BACK` to back. These dispatch controls are not derived from prompt callbacks.

## Button naming and icon rules

`BUTTON_OK` and `BUTTON_BACK` are global logical action names assigned by `InputConfig::AssignActionButtons()` (`es-core/src/InputConfig.cpp`). This uses `InvertButtons`; current defaults in `Settings.cpp` are `InvertButtons = true` and `GameOptionsAtNorth = true`. On non-Windows builds, `INVERTEDINPUTCONFIG` is not defined (`InputConfig.h`), so with the default invert setting OK is `b` and BACK is `a`. With invert disabled those swap. Windows defines `INVERTEDINPUTCONFIG`, which changes the assignment and SDL mapping behavior.

`InputConfig::buttonLabel()` may swap `a`/`b` only on builds with `INVERTEDINPUTCONFIG` when inversion is off. It chooses an icon resource key; it does not change the input action. Default help SVG resources are named by logical key (`button_a.svg`, etc.), and themes can override icons using `<helpsystem iconA=... iconB=... iconX=... iconY=...>` through `HelpStyle`.

There are multiple naming vocabularies: internal actions (`a/b/x/y`), SDL-to-EmulationStation mapping (`InputManager.cpp`), physical/controller position names (east/south/north/west), and icon artwork. `InputConfig::buttonDisplayName()` and `buttonImage()` are used by mapping UI, but the help bar normally shows icon art, not a text label such as “SOUTH”. Device/controller mapping configuration determines which physical input emits each internal action.

## Confirmed source-level mismatch candidates

These are concrete paths to investigate when fixing behavior later:

1. Keep the system drawer prompt bound to the hard-coded `b` key. The intended prompt-label update is `SYSTEMS LIST`; do not replace the key with `BUTTON_BACK` as part of that label change. Separately, `SystemView::input()` currently dispatches the drawer through `BUTTON_BACK`, whose value depends on `InvertButtons` and build configuration. If investigating whether the displayed `b` glyph matches the physical trigger, treat that as a distinct input-mapping question rather than changing the help prompt key.
2. `ISimpleGameListView::getHelpPrompts()` has a subtle pairing to verify against the desired UX: it puts `BUTTON_OK` under `SAVE STATES (HOLD)` when `GameOptionsAtNorth` is true, while `mXButton` (literal `x`) opens game options in that mode. It puts the `x` prompt under `GAME OPTIONS` in that mode, but appends `/ FAVORITE`; `x` input is intercepted for options, so the favorite text may not describe the action. When the setting is false, OK is labeled GAME OPTIONS (HOLD) and `x` is save states/favorite, while input uses the inverse for `x`.
3. The game-list favorite prompt uses `x` in both settings states (or options/save-state prompts on x). Check any selected game's save-state eligibility and mode because prompt construction is conditional.
4. `Window::setHelpPrompts()` deduplicates by the first key before rendering. Two actions sharing a key won't both display; callbacks on HelpPrompt are not consulted for dispatch.

## Diagnostic route for a future fix

When comparing a shown icon with an observed action, record all of the following in the same run:

- active view class and its exact `getHelpPrompts()` output (first key, label);
- `BUTTON_OK`, `BUTTON_BACK`, `InvertButtons`, `GameOptionsAtNorth`, Kid mode, netplay, manufacturer support, and save-state eligibility;
- active controller's input-config mappings from physical button/axis/hat to internal actions, including SDL mapping and whether its device configuration was loaded approximately;
- actual `InputConfig` action observed at `SystemView::input()` / `ISimpleGameListView::input()`;
- help icon key after `buttonLabel()`, plus any theme `<helpsystem>` icon overrides.

This separates a stale/wrong prompt list from an icon-mapping problem and from controller-to-action mapping. The display code is in `es-core/src/components/HelpComponent.cpp`; prompt assembly and action dispatch are in the view files above; controller mapping is in `es-core/src/InputManager.cpp` and `InputConfig`.

## Proposed fix (design only; no code changed)

The durable direction is to make each view describe the same logical actions that its input handler dispatches, and then render the control glyph for that logical action consistently. Do not treat `HelpPrompt::action` as the input binding: it is not invoked by the input handlers. Before changing the on-device physical glyph mapping, capture the RG DS Plus input-config entries and determine which internal actions its north/west/south/east buttons produce. Source inspection alone cannot identify that device-specific mapping.

### System selector

- Keep the system drawer prompt key hard-coded as `b` and change its label from NAVIGATION BAR to `SYSTEMS LIST`. The drawer presents the available systems navigation choices; the button opens that list rather than immediately selecting or jumping to an item.
  - Drawback: the label is concise but could be mistaken for the main list of systems rather than the drawer. It is still more direct about what appears than “Jump to” or “Navigation bar”. The separate `BUTTON_BACK` dispatch path may remain a mapping concern if the physical button does not match the `b` glyph.
- Display RANDOM only when the bound action can produce a meaningful result. `SystemData::getRandomSystem()` chooses from all game systems and can return the current system (and returns null if there are none), so a practical availability check should require at least two eligible game systems. Also verify that the controller action actually reaches that branch. If the physical north input does not emit `x`, hideing the prompt alone would conceal an input-mapping defect; correct the mapping or move both prompt and dispatch together.
  - Drawback: if systems are added/removed dynamically, prompt availability must refresh with the system list. A count-based check also means RANDOM disappears with one system, even though pressing it could technically select the current system again.

### Game list

- Keep the Favorites long-press functionality, but do not mention it in the help label. The x prompt should name only its short-press action: `GAME OPTIONS` when `GameOptionsAtNorth` is true, or `SAVE STATES` when false and save states are available. In Kid mode, follow the actual short-press branch and save-state availability. This makes the label match the immediate result while leaving the existing long-press favorite shortcut intact.
  - Drawback: users will not discover Favorites from this help prompt; preserving discoverability would require a separate hint or UI affordance, which is outside this label correction.
- For the WEST “Save states” prompt followed by game launch, compare the emitted logical action with `mOKButton` and `mXButton`. The short-press launch path is `mOKButton`; the save-state short press is the OK action when `GameOptionsAtNorth` is true, and the x action when false. Save-state / options prompts are long-press behaviors on OK, while x has its own short and long press behaviors. If the west physical input is mapped to OK, a short press launches as expected even if the displayed art suggests another physical position. Correct the controller-to-action mapping or make prompt construction use the actual chosen action; do not change launch semantics based only on the icon's apparent position.
  - Drawback: changing the shared controller mapping can alter OK/Back behavior across all screens and games. Changing per-view dispatch to compensate can make the same physical face button mean different things between systems or devices.
- Derive prompt control keys and dispatch from a shared per-view binding/action definition (or at minimum centralize the decision for `GameOptionsAtNorth` so prompt construction and input handling cannot drift). Keep labels conditional on the same eligibility/mode predicates used by the action.
  - Drawback: broader refactoring touches common input behavior and needs regression checks across grid, basic, carousel, Kid mode, systems with/without save states, and both `GameOptionsAtNorth` values. A minimal label correction is lower risk but does not solve a controller mapping mismatch.

### Validation needed when implementation is authorized

On the target device, log prompt tuples immediately before `Window::setHelpPrompts()` and log the received internal action at the relevant view input handler for each physical button. Record build flags, `InvertButtons`, `GameOptionsAtNorth`, controller identity/config, and theme icon overrides. Then verify the displayed icon, emitted action, and resulting operation together on both system selector and game list. This determines whether the observed problem consists of source prompt mismatches, device mapping, theme glyphs, or more than one of these.

## Scope and limitations

This is static source research only. No build, device connection, runtime logging, or handheld test was performed. It documents what this checkout implements; the exact on-device icon appearance and physical button mapping still depend on the active theme, build flags, settings, and controller configuration.
