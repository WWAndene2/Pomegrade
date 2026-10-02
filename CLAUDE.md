# Project Rules — Master Template

Read this before making any code, UI, or styling change. These are hard rules, not suggestions — do not bypass them by reaching for a default utility class, an arbitrary one-off pixel value, a "close enough" number, or a lazy workaround.

Rules are grouped into domains below, each grounded in an established professional discipline rather than ad hoc preference. Within a domain, more specific rules refine — never override — the general ones above them.

---

## 1. Work Habits — process discipline

Discipline: **quality assurance / engineering process discipline** (cf. the verification and root-cause-analysis practices underlying ISO 9001 process control and standard code-review methodology).

1. **Verify before and after every finding or fix (pre-/post-verification).** Confirm the defect is real, reproducible, and understood before acting on it; confirm the fix actually resolves it afterward via a positive check (a passing test, a reproduced-then-cleared repro case, a before/after diff of the actual numbers). Neither end is ever assumed.
2. **Cross-check and corroborate sources.** Do not rely on a single read, a hand-copied reimplementation, or a remembered assumption. Verify claims against the authoritative source — the real file, the real test, the real data — before relying on them. If a hand-rolled diagnostic script disagrees with the real code path, trust the real code path and find out why the script was wrong, not the reverse.
3. **Perform due diligence on source data, and apply it consistently.** Read and understand reference data thoroughly before using it, and apply that understanding at every decision point in the task where it is relevant — not as a one-time lookup consulted only when convenient.
4. **Prefer root-cause remediation over palliative workarounds.** Where a genuine, sourced fix is available, implement it rather than deferring, suppressing, or working around the underlying issue. This is standard root-cause-analysis practice: a workaround treats a symptom and leaves the defect live.
5. **State uncertainty explicitly; never present a guess as a verified fact.** If something wasn't checked, say so. A confident-sounding but unverified claim is a worse outcome than an honest "I haven't confirmed this yet."
6. **Ask rather than assume when a decision materially changes behavior, scope, or risk.** Use judgment for reversible, low-stakes calls; escalate for anything hard to reverse, ambiguous in intent, or outside the request's stated scope (see §5, Change Scope Discipline).

### 1.1 Definition of Done

A task is not complete until all of the following hold — treat this as the exit checklist for any non-trivial change:

- The root cause (not just the symptom) is identified and addressed, per §1.4.
- The fix is verified against real execution (tests, a reproduced scenario, or measured output), not just read-through review.
- The full relevant test suite passes — not just the test for the immediate change; a regression elsewhere is still a regression.
- Any newly-introduced non-obvious behavior, tradeoff, or known limitation is documented **in the code or commit message**, not only explained conversationally. If asked "did you document it?", the answer should already be yes.
- The diff is reviewed for scope: no unrelated cleanup, no drive-by refactors, no speculative abstractions bundled into the same change (see §5).
- Naming and structure of anything touched or added conform to §4.

### 1.7 Skill or playbook usage — invoke the right one proactively, don't wait to be asked

If this project defines reusable skills, playbooks, or checklists for recurring kinds of work (an audit procedure, a restructuring procedure, a design-review procedure, a scope-disambiguation procedure), recognizing which situation calls for which one — and invoking it without waiting for the user to name it — is itself part of doing the work well. Treating them as optional extras that only run when explicitly requested is how audits get skipped and drift accumulates.

This project defines no skills of its own yet. The Claude Code skills below are used when the session offers them (they are environment skills, not files in this repository):

| Skill/playbook | Location | Invoke when |
|---|---|---|
| Correctness review of a diff | `/code-review` | Before proposing a merge into `main`, and after any change touching JNI, native code or launch routing between the cores |
| Security review | `/security-review` | Any change to file access (SAF, `Android/data`), network code, signing or the CI workflow's secrets |
| Broad app-wide audit | `anthropic-skills:app-audit` | Before a release, and as the ~50-commit maintenance pass (§4.9) |
| Visual/aesthetic audit | `anthropic-skills:design-aesthetic-audit` | Any change to Pomegrade's own screens, theme or icon |
| Scope/ambiguity resolution | `anthropic-skills:scope-context` | A request phrased as "all X" / "every Y" across the three codebases (app, melonDS core, Azahar) |

**How this interacts with the rest of this document:**
- Any maintenance cadence defined elsewhere in this document (e.g. §4's "every ~50 commits") is a *floor*, not the only trigger — a listed skill should run sooner whenever its specific trigger condition is hit mid-task.
- Running an overly broad skill when a narrower one actually fits wastes the same effort §5 (Change Scope Discipline) warns against — match the skill to the actual scope of concern, not the biggest hammer available.
- These skills are companions to, not substitutes for, §1.6 (ask rather than assume): a skill's own verification gates do not remove the obligation to flag a genuinely ambiguous or high-stakes call to the project owner.

---

## 2. Verification & Testing Discipline

Discipline: **software testing and validation methodology** — the practice of treating automated tests and reproducible measurements as the primary evidence of correctness, not narrative confidence.

1. **Run the real test suite before making a change and after.** A pre-existing failure is not yours to silently absorb into your diff's "before" state without noting it; a new failure after your change is yours to explain or fix before calling the task done.
2. **Prefer measured evidence over inferred behavior.** When a value, a rate, or a count matters, obtain it via the actual code path (a temporary, always-reverted diagnostic if needed) rather than computing it by hand from assumptions about how the system behaves.
3. **When a change causes divergence from a previous baseline (a golden file, a snapshot, a fixture), never widen the tolerance or update the baseline without first establishing why the divergence is expected.** Document the specific, cited reason next to the widened tolerance or updated fixture — a bare "widened for now" is not acceptable.
4. **Delete throwaway diagnostics.** Debug-only instrumentation (temporary throws, dumps, one-off scripts) is removed once it has served its verification purpose — it does not linger in the codebase or get mistaken for permanent code.
5. **A test suite passing is necessary, not sufficient, for UI/UX work.** For anything user-facing, exercise the actual feature (start the app, interact with it) before reporting success — passing type checks or unit tests verifies correctness of code, not correctness of experience.

---

## 3. Security — access control & least privilege

Discipline: **information security / access control**, specifically the principle of least privilege and change-authorization control found in frameworks such as ISO/IEC 27001 and NIST SP 800-53 (access control family).

Do not unilaterally provision any privileged access path — debug bridges, backend doors, or equivalent — without explicit authorization from the project owner. If diagnosing a defect appears to require this class of access, stop and request authorization first. Do not enable it preemptively, present it as a fait accompli, or reclassify it as "temporary" or "diagnostic" to sidestep this control.

---

## 4. Code Hygiene, Naming & Structure — records & configuration management

Discipline: **records management and information architecture** (the classification, naming, and lifecycle-maintenance practices formalized in ISO 15489) applied to source code, combined with standard **software configuration management** practice (naming-convention and code-organization guidance as codified in style guides such as Google's engineering style guides and Clean Code's naming principles).

Structuring, organizing, segmenting, naming, and classifying files, folders, and code is **mandatory and non-negotiable**, regardless of deadline, urgency, or how small a change seems. A quick fix does not excuse misplacing a file, reusing a misleading identifier, or leaving a new symbol unclassified "for now."

Hygiene, ownership clarity, coherence, and — above all — **consistency** (of naming, of structure, and of process) are a standing priority that no task, deadline, or request implicitly waives. If a request would require violating this, flag the conflict before proceeding rather than complying silently.

The rest of this section makes that mandate concrete: where a new file or folder goes, what it must be named, and what is and isn't allowed to exist as a standalone file — rather than leaving "structure it properly" as an unstructured instruction. Populate the tables and examples below with this project's own conventions the first time this template is adopted; keep the structure (taxonomy → naming → persistent-documentation homes → new-top-level-entry gate) intact.

### 4.1 Directory taxonomy — where a new file goes

Every codebase should be organized by **purpose**, not by file type. Before creating anything, classify what you're building against this table and place it in the matching existing directory — never guess, and never default to wherever feels adjacent to the thing you're already touching.

Pomegrade is three codebases in one Gradle build, rooted at `melonDS-android/` (the Android app and build), with the 3DS core imported beside it:

| Purpose | Location |
|---|---|
| Android app: UI, settings, game list, launch routing between cores (Kotlin) | `melonDS-android/app/src/main/java/me/magnum/melonds/<existing package by purpose>` (`ui/<screen>/`, `domain/model/`, `impl/`, `common/`, `di/`, …) |
| App ↔ DS core bridge (JNI, C++) | `melonDS-android/app/src/main/cpp/` (upstream melonDS-android code; `oboe/`, `faad2/`, `enet/` there are third-party submodules) |
| DS/DSi emulator core, including HD texture replacement (C++) | `melonDS-android/melonDS-android-lib/src/` |
| 3DS core sources (Azahar, C++ and its Android frontend) | `azahar/` — consumed by the `:azahar` Gradle module |
| `:azahar` Gradle module definition (manifest, build script) | `melonDS-android/azahar/` |
| Build configuration shared by modules (`AppConfig`) | `melonDS-android/buildSrc/src/main/kotlin/` |
| CI workflows | `.github/workflows/` |
| Kotlin unit tests | `melonDS-android/app/src/test/java/`, mirroring the package of the class under test |
| Android instrumented tests | `melonDS-android/app/src/androidTest/java/`, same mirroring |
| Desktop tests of the DS core (CMake, no Android), one folder per feature | `tests/<feature>/` (`hd-textures/`, `polygon-multiplier/`) |
| Golden/reference fixtures a test compares against | A `fixtures/` directory beside the tests that use them |
| Project-wide documentation | Root: `README.md` (users), `CLAUDE.md` (rules), `DEVELOPMENT_NOTE.md` (roadmap) |

**Decision procedure, in order:**
1. Does an existing directory already match this artifact's purpose by the table above? Use it.
2. Is it a genuinely new *kind* of purpose the table doesn't cover (not just a new instance of an existing kind)? Then creating a new top-level directory is itself a structural decision — flag it and get confirmation before creating it (per the general hygiene mandate above), rather than deciding unilaterally.
3. Never create a "misc," "helpers," "common," or "stuff" catch-all directory. If something doesn't fit the existing taxonomy, that is a signal to ask, not to invent a dumping ground.

### 4.2 Naming conventions — how a new file is named

Naming is not a stylistic afterthought; a misleading or inconsistent name is a hygiene violation on the same footing as a misplaced file. This project's conventions:

- **Kotlin:** one top-level class per file, file named after it in PascalCase (`RomPlatform.kt`). Packages follow the existing `me.magnum.melonds` tree; the upstream package name is kept so melonDS updates still merge.
- **Compose UI:** `@Composable` functions in PascalCase named after what they render (`RomHeaderUi`), as upstream does.
- **C++ (DS core):** the existing melonDS style — `PascalCase` file names prefixed by subsystem (`GPU3D_TextureReplacement.cpp`).
- **Console names in identifiers:** `Nds` for DS/DSi and `N3ds` for 3DS in Kotlin/C++ identifiers (an identifier cannot start with a digit); `three_ds` for 3DS in Android resource names, after the category prefix upstream uses when there is one (`three_ds_unsupported`, menu action `action_three_ds_games`); "3DS" in user-visible text. Never a second spelling.
- **Azahar resources that clash with app resources:** renamed with the `citra_` prefix, so neither module overrides the other's.
- **Build switches read from the environment:** `POMEGRADE_<WHAT>` (`POMEGRADE_CCACHE`, `POMEGRADE_NATIVE_BUILD_TYPE`).
- **Tests:** `<ClassUnderTest>Test.kt`; C++ tests `<subject>_test.cpp`.
- **Fixtures:** named after their subject, beside the tests that use them, never inline-duplicated elsewhere.
- **Localized strings:** `values-b+<locale>/strings.xml` with the same keys as `values/strings.xml`.

### 4.3 Persistent documentation — where it lives, and what "persistent" means

This is the rule that exists specifically to stop notes, logs, and one-off summaries from accumulating across the repo over time.

**A finding, decision, or piece of reasoning that must survive the current task belongs in exactly one of these, and nowhere else:**
1. **A code comment**, at the exact line the reasoning concerns, when the point is "why this specific line is the way it is."
2. **The commit message**, when the point is "why this change was made" and doesn't need to be visible to someone just reading the code later.
3. **An existing, purpose-named reference document already living beside the code it documents.** Extend one of these when the new information belongs to its existing subject, rather than starting a new file.
4. **A new reference document**, only when the information is a genuine, reusable deliverable with a clear subject and no existing home for it — named after that subject (not "notes" or "log"), placed beside the code it documents, and only after flagging its creation to the project owner, since a new standalone doc is a structural addition, not an incidental byproduct of the task at hand.

**What must never happen, because this is the specific failure mode this rule exists to prevent:**
- Creating files like `NOTES.md`, `TODO.md`, `progress.md`, `fix-log.md`, `investigation.md`, `summary.md`, or any other generically-named scratch or status file inside the repository, at any location, for any reason. If you want to remember something across steps of the same task, use an out-of-repository scratch location — never a tracked file.
- Leaving a diagnostic-only script or test in the tree after it has served its verification purpose (see §2.4). A temporary diagnostic test, a debug-only trace, or a hand-rolled reproduction script is deleted before the task is considered done — it does not get committed "just in case," and it is never a substitute for one of the four permanent homes above.
- Writing the same finding into more than one of the above without reason — pick the one destination that fits, rather than a code comment *and* a new doc *and* a chat explanation of the same fact.

### 4.4 Module boundaries — allowed dependency directions

A directory taxonomy only holds if files placed correctly are also only *importing* from directories they're allowed to depend on. This project's dependency graph, bottom-up:

```
azahar/externals/*, app/src/main/cpp/<submodules>  →  (third-party, depend on nothing of ours)
melonDS-android-lib (DS core, C++)  →  its own externals only — never Android or app code
:azahar module (3DS core + its Android frontend)  →  azahar/ sources — never the app module
:masterswitch, :common, :rcheevos-api  →  as upstream melonDS-android defines them
:app (me.magnum.melonds)  →  all of the above
```

The app module is the only place the two cores meet: launch routing (`ui/common/rom/N3dsLauncher.kt`) and the unified game list call into Azahar's public entry points (`CitraApplication`, `EmulationActivity`, `PermissionsHandler`), never the reverse.

Concretely:
- **A lower layer never imports from a higher one.** If a lower-layer module seems to need something from a higher layer, that's a sign the shared piece belongs in the lower layer instead — move it down, don't import up.
- **The cross-feature-reuse layer (`shared/` or equivalent) never imports from a specific feature.** The moment it depends on one feature, it is no longer shared — either the code isn't actually generic and belongs in that feature, or the feature-specific part must be extracted out first.
- **One feature never imports another feature's internals directly.** Cross-feature reuse goes through the shared layer, promoting the reused piece there first.
- **A subsystem's internals stay internal to it.** Code outside a subsystem interacts with it through its published entry points (e.g. a barrel/index file), not by reaching into files it doesn't own.

When in doubt about which layer a file belongs to, its allowed imports are the answer: a module that needs to import from a feature cannot live in a lower layer, no matter how "core" its purpose feels.

### 4.5 File decomposition — when a file must be split

A file is a unit of hygiene, not just of code. Split a file once any of these thresholds is crossed, rather than letting it grow indefinitely:

- **It serves more than one responsibility.** If describing the file's purpose in one sentence requires "and," the responsibilities are separable — split along that seam.
- **Its tests need multiple, unrelated top-level test groups** to cover genuinely different concerns rather than different cases of the same concern. That's a proxy for the file itself covering more than one concern.
- **A change to one part of the file routinely has no effect on, and no relation to, another part of the same file.** Unrelated change-reasons are a decomposition signal (the same principle behind the Single Responsibility Principle).
- **It has grown large enough that a reader must scroll past unrelated code to find the part relevant to their task.** This is a readability failure, not a badge of thoroughness.

When splitting, keep the resulting files inside the same directory (per §4.1) unless the split reveals a genuinely new purpose category, and preserve the naming convention in §4.2 for each resulting file — a split is not an excuse for an ad hoc name.

### 4.6 Generated files and barrel/index files

- **Generated output is never hand-edited.** Anything under a directory explicitly designated for build output is produced by an explicit generation step. If it needs to change, change the generator and regenerate — editing the output directly creates silent drift the next time it's regenerated, and is treated as a hygiene violation, not a shortcut.
- **Barrel/index files re-export; they do not implement.** Where this project uses aggregation files, they collect and re-export the real modules in that directory. New logic never gets written directly into a barrel file; it goes into a properly named module that the barrel then re-exports.

### 4.7 Deprecation and deletion — retiring a file or dead code correctly

Removing code is as much a structural act as adding it, and gets the same rigor rather than being treated as a free action:

1. **Confirm zero remaining references** before deleting a file or export — search the codebase, don't rely on memory of what you think still uses it (per §1.2, corroborate before acting).
2. **Migrate consumers first, delete second**, in that order within the same piece of work — never leave a codebase in a state where both the old and new paths are live "just in case" beyond the migration itself.
3. **No commented-out code as a soft delete.** Dead code is removed outright; version control, not a comment block, is the record of what used to be there. A comment explaining *why* something was removed is fine; the removed code left in place as a comment is not.
4. **A deletion that removes an entire file's worth of exports is its own reviewable change** (see §5, Change Scope Discipline) unless it's the direct, necessary conclusion of the migration it follows.

### 4.8 New top-level files and directories

The repository root and the top level of the source tree are reserved for canonical, whole-project artifacts (a README, this rules file, and the top-level source directories in §4.1). Adding anything new at either level — a new root document, a new top-level source directory — is a structural decision on the same footing as changing the taxonomy itself: flag it and get confirmation before creating it, don't add it as a side effect of an unrelated task.

### 4.9 Enforcement — structural rules should be checkable, not just followed

Prose discipline alone is a weaker guarantee than a rule a tool can actually verify. Where practical, back the rules above with something mechanical rather than relying solely on manual compliance:

- A lint rule or dependency-boundary checker (e.g. an import-boundary ESLint configuration, or a dependency-graph tool) to catch a §4.4 layering violation automatically.
- A naming-convention check extended to cover each artifact type from §4.2 as it's introduced.
- Treating the absence of such tooling as a gap to close during the maintenance cadence below, not a permanent excuse to fall back on discipline alone.

**Maintenance cadence (lifecycle management):** run a project restructuring/audit skill (if one exists) and a code-audit pass on a regular cadence — as a floor, every ~50 commits — to prevent structural drift from accumulating. This is scheduled maintenance, analogous to a records-retention review, not something to defer until requested.

**Standing exceptions:** any file or module the project owner designates off-limits should be listed here explicitly (path, scope of what's covered, and the exact condition under which it may be touched). Do not touch such files outside that condition, even for this maintenance cadence, an unrelated defect fix, or an adjacent "quick" change.

| Path | Scope | May be touched only when |
|---|---|---|
| Git submodules (`azahar/externals/*`, `azahar/dist/compatibility_list`, `melonDS-android/app/src/main/cpp/{oboe,faad2,enet}`) | Pinned third-party code | Bumping the pinned commit as its own change |
| `melonDS-master.zip`, `melonDS.v.2.0.1.PS.apk` | Reference copies of upstream melonDS | Never modified; removed only on the owner's request |
| Upstream melonDS-android and Azahar code | Code Pomegrade did not write | Required by a Pomegrade feature or fix: keep the edit minimal so upstream updates still merge, and say why in the commit message. Upstream style and naming are kept rather than "corrected" to this document |

---

## 5. Change Scope Discipline

Discipline: **change management** — keeping each change unit minimal, reviewable, and traceable to a single intent, per standard configuration-management practice.

1. **Match the diff to the request.** Do not bundle unrelated refactors, renames, or "while I'm here" cleanups into a change whose purpose is something else — file such improvements as their own follow-up instead.
2. **No speculative generality.** Do not add abstractions, configuration options, or extensibility hooks for hypothetical future needs. Build for the requirement in front of you.
3. **Keep commits atomic and their messages accurate.** A commit message describes what actually changed and why; it is not aspirational or a summary of unrelated work swept in alongside.
4. **When a fix surfaces a second, adjacent issue**, decide deliberately whether it belongs in the same change (it directly blocks correctness) or a separate one (it is independently valuable but not required) — and say which, rather than silently expanding scope either way.

---

## 6. App & Feature Development — functional completeness

Discipline: **requirements engineering and interaction design** — specifically the *completeness* criterion for a well-formed requirement (ISO/IEC/IEEE 29148: a requirement set is incomplete if it omits functionality the user needs but didn't think to state explicitly), combined with the standard interaction-design practice of matching a UI request to its recognized **component archetype** rather than only its literal wording, and with Nielsen's usability heuristics — in particular *user control and freedom* and *consistency and standards*, which are exactly what's missing when a feature is built with no way to operate it fully.

This section exists because a request named after a familiar kind of feature ("a color selector," "a music player," "a settings panel") carries an implicit spec far larger than its literal words: real-world convention already defines what that archetype minimally includes. Implementing only the literally-named mechanic and skipping the rest is an **incompleteness defect** — a feature is not done because it compiles and shows something; it is done when it satisfies the implicit requirements of the archetype actually asked for, per Definition of Done (§1.1) and §2 (verification against real execution) — not just its most literal reading.

### 6.1 Resolve the request to its component archetype before building

Before writing any code for a named feature, identify what that request actually names: a recognized class of UI/interaction pattern with well-established conventions, not just the single mechanic that makes it distinctive. Two features can share a name and still differ in exact needs, but the baseline set is what makes something recognizable as that archetype at all — omitting from it isn't a smaller version of the feature, it's a different, broken thing wearing its name.

Representative examples (non-exhaustive — apply the same reasoning to any archetype not listed):

| Requested feature | The literal, distinctive mechanic | The implicit baseline the archetype also requires |
|---|---|---|
| A color selector/picker | A way to choose a color value | A trigger to open it, a way to confirm/apply the choice, a visible current-value indicator, and a wired destination that actually receives the chosen value |
| A music/audio player | Play/pause of a track | Volume control, a way to mute, a progress/seek indicator, and the actual audio source wired in — a player with nothing to play is not a player |
| A settings panel | The individual settings/toggles | A way to open/close or navigate to it, persistence of the choices made, and each setting actually wired to the behavior it claims to control |
| A form | The input fields | Validation feedback, a submit action, a cancel/dismiss path, and a wired destination for the submitted data |
| A modal/dialog | The content inside it | An open trigger, a close affordance (explicit control, not just an assumed outside-click), and a defined return path for whatever the modal was for |

### 6.2 Verify data flow end-to-end, not just the presence of a control

A control that exists but does nothing, or a value that is produced but never consumed, is exactly the same class of defect as a missing control — both leave the feature functionally incomplete. Before calling a feature done:
1. Trace the full path from user input to observable effect (a color chosen is applied and/or stored somewhere real; a volume slider actually changes the audio output level; a submitted form actually reaches its destination).
2. Confirm there is no dangling half: no UI control with no wired effect, and no wired capability with no UI control to reach it.
3. This is the feature-level instance of §2.1/§2.5 (verify against real execution; a passing build is not evidence the feature works) — exercise the actual interaction, don't infer wiring from the code looking plausible.

### 6.3 This is not a license for scope creep — reconcile with §5

§5.2 ("no speculative generality... build for the requirement in front of you") and this section are not in tension, and neither overrides the other:

- **§5.2 forbids building for hypothetical future requirements** — options, hooks, or abstractions nothing currently asks for.
- **§6 requires building the actual current requirement completely** — the baseline parts in §6.1 are not hypothetical or future; they are already implied by the feature named, whether or not each part was said out loud. Skipping them isn't restraint, it's under-delivery against what was actually asked.

The line between the two: if a real-world instance of the named archetype would be considered broken or unusable without a part, that part is in scope now, per §6.1 — not a speculative extra.

### 6.4 When the archetype's baseline is genuinely ambiguous or intentionally reduced

If a request explicitly scopes a feature down ("just the color swatches, no picker UI yet") or the right baseline for an unfamiliar/novel archetype isn't obvious, don't guess silently in either direction — per §1.6, ask, and state explicitly which parts of the usual baseline are being deliberately deferred and why, so the gap is a documented decision rather than an unnoticed omission.

---

## 7. Design System — design-token governance

Discipline: **design-token systems**, the standard mechanism (used across major design systems, e.g. Material Design, Salesforce Lightning) for enforcing a single source of truth for spacing, sizing, and typography scales, ensuring pixel-accurate consistency across a UI.

> Define a single numeric scale for the project. This template uses "PerfectSuite" as an example instance — swap in the project's own scale if different, but keep the same enforcement structure below.

> **In Pomegrade this scale is a graphic guideline, not a hard gate** (owner's decision): apply it to Pomegrade's own screens and judge it by the visual result. Upstream melonDS and Azahar layouts keep their values unless a Pomegrade change redesigns them (see the standing exceptions in §4.9). Android dimensions are in `dp`/`sp`, where the scale applies to the number.

Every numeric dimension in the app — font-size, width, height, padding, margin, gap, icon size, border-radius input, anything measured in px — **must** resolve to one of the scale's defined values. Example scale organized in three tiers per power-of-2 octave — `[Primary]`, `(Secondary)`, `{Tertiary}`:

```
[1]
[2]  (3)
[4]  (6)
[8]  (12)  {14}
[16] (24)  {30}
[32] (48)  {62}
[64] (96)  {126}
[128] (192) {254}
[256] (384) {510}
[512] (768) {1022}
[1024]
```

No other number is permitted. This applies to existing code as well as new code — when an off-scale value is touched, correct it to the nearest scale value as part of that change rather than leaving it.

**Before setting any dimension:**
- Never assume a default utility class is compliant — verify its computed px value against the defined scale. A framework's default spacing/typography scale will often not map cleanly onto a project's own token system.
- If a token (`--font-*`, `--size-*`, `--space-*`, etc.) already resolves to a scale value, use it — this is the token layer doing its job.
- If the nearest existing token is off-scale, either correct the token (when the change should propagate to every usage) or apply an explicit, scoped override for just that element (when the change is local only).

### 7.1 Rounding priority (tie-breaking)

When correcting an off-scale value, round to the mathematically nearest scale value. When two candidates are **equidistant**, resolve the tie by a defined priority order. Example, for the PerfectSuite scale above — **Nearest beats Primary beats Secondary beats Tertiary**:

- **`[Primary]`** (base 2): `1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024`
- **`(Secondary)`** (base 2 intermediate — the midpoint between two consecutive primaries): `3, 6, 12, 24, 48, 96, 192, 384, 768`
- **`{Tertiary}`** (base 2 additional — sum of the primary steps below the next primary, e.g. `8+4+2=14`, `16+8+4+2=30`): `14, 30, 62, 126, 254, 510, 1022`

Worked examples:
- `10` → tie between `8` (primary) and `12` (secondary), both distance 2 → primary wins → **8**
- `13` → tie between `12` (secondary) and `14` (tertiary), both distance 1 → secondary wins → **12**
- `15` → tie between `14` (tertiary) and `16` (primary), both distance 1 → primary wins → **16**
- `11` → `12` is distance 1, `8` is distance 3 → not a tie, nearest wins → **12**
- `17` → `16` is distance 1, `24` is distance 7 → not a tie, nearest wins → **16**

This can and will collapse previously-distinct values onto the same scale number (e.g. two font sizes both rounding to `12`) — that is an accepted outcome of strict scale compliance, not a bug to work around by picking a different rounding.

### 7.2 Exception — values above the first primary-doubling threshold

For a value greater than the scale's second primary step (e.g. `16` in the example scale), do not snap to the single nearest scale number. Instead: take the nearest `[Primary]` at or below the value, then add another `[Primary]` on top to close the remaining gap as tightly as possible.

- Example (PerfectSuite scale): `150` → nearest primary at/below is `128`; `128 + 16 = 144` is the closest reachable primary+primary sum → **144**.

### 7.3 Corner radius

Define a formula relating radius to a component's own dimensions, e.g. `radius = 0.24 × the element's height`, then round to the nearest scale value (apply the tie-break priority in §7.1).

### 7.4 Aspect ratios — preferred, not mandatory

Define a short priority list of preferred aspect ratios to reach for — not a hard constraint; apply engineering judgment rather than forcing a mismatch. Example: `3:2`, `4:3`, `5:4`, `3:1` (the last reserved for wide/short bars).

### 7.5 Design objectives behind the design-token system

Standardization · coherency · consistency · pixel-accurate precision · symmetry · disciplined aesthetic proportion · deliberate, documented art-direction decisions — never a default or "close enough" value.

---

## 8. Project-Specific Standards

Use this section to record any reference proportions, component precedents, or exceptions unique to this project (e.g. header/navbar reference dimensions, known off-scale tokens not yet fixed, other standing exceptions). Keep such content isolated here so it stays easy to identify and to strip out when reusing this document as a template for a different project.

- **Builds are expensive.** A clean build compiles two emulator cores (~2,500 C++ files). Verify locally (`POMEGRADE_ABIS=arm64-v8a ./gradlew :app:assemblePlayStoreProdDebug`) before pushing; GitHub Actions only builds `main` and manual runs, to spare the owner's Actions minutes.
- **Nothing in this container runs the app.** "Verified" for user-facing behaviour (§2.5) means tested on the owner's phone; until then, report it as untested.
- **Launcher icon:** pomegranate on a transparent adaptive-icon background; foreground 62dp tall inside the 66dp safe zone of the 108dp canvas; themed (monochrome) layer provided.
- **Requirements:** Android 10+ (3DS core); 3DS games need a 64-bit device.
