# Working with several agents

Set up on 20 October at the owner's request: three Claude agents work on Pomegrade at once, each on its own branch,
asynchronously. The owner is away and does not test on the phone during this period, so every check is a headless run in
a session (§3 below). This file is the process; each agent's messages are its log (§2).

**Stopped on 20 October at 17:10 (owner's decision): back to one agent, A's session, working every lane of
DEVELOPMENT_NOTE.md 3e itself.** Why: messages missed or read late (pings and check-ins that did not wake), about 40
minutes idle per agent and lane, and work stacked untested (gym modules ported before one ran in its gym). B and C were
told to push, log their last state and stop; their branches were merged into `main` at 17:25 (B: its last code push of 17:15; C: its step 2 unfinished, logged). The run recipes of §3.1 stay valid.

## 1. Who does what

One owner per area. An agent changes files outside its area only as §4 says.

| Agent | Branch | Area | Its files |
|---|---|---|---|
| A, coordinator | `claude/inspiring-keller-280lek` | World, battles, game data (DEVELOPMENT_NOTE.md 3e, lane A); merging into `main` (§5) | `tools/remake/src/OrasXyTrainers.*`, `OrasEngine.*`, `OrasXyWorld.*`, `RemakeTool.cpp`, new game-data files `OrasData*.*` |
| B | `claude/hopeful-pascal-x96527` | X/Y's own features (3e, lane B): the natives Omega Ruby has none of, as engine code | `tools/remake/src/OrasXyNatives.*`, `OrasXy<Feature>.*` per feature, their engine patches beside `OrasEngine.cpp` |
| C | `ccr-52ddfc61-gcuszh` | Story, gameplay path, text and music tools (3e, lane C) | `tools/remake/src/OrasXyEvents.*`, `OrasXyScript.*` (all but the rename table, §4), `tools/remake/sandbox/`, new path files |

The work: DEVELOPMENT_NOTE.md section 3e, one step at a time per lane, in order; A gives each step when the one before
is merged.

## 2. Messages: one log per agent

Each agent keeps `docs/agents/<its branch's last part>.md` (A: `inspiring-keller-280lek.md`, B: `hopeful-pascal-x96527.md`,
C: `ccr-52ddfc61-gcuszh.md`) and writes
only its own. The others read it with `git fetch origin <branch>` and `git show origin/<branch>:docs/agents/<name>.md`.

An entry, newest at the bottom:

```
## 2026-10-20 14:05 to B (or: to all)
What: one or two lines.
Needs: what is asked of the reader, or "nothing".
Commits: the hashes it concerns.
```

- Read the others' logs at the start of each session and before touching a shared file (§4).
- Answer in your own log, naming the entry answered ("re B 2026-10-20 14:05").
- The log carries messages between agents only. Findings that must last go where CLAUDE.md §4.3 says (a code comment, the
  commit message, the reference document of the subject), never only in a log.

## 3. Checking without the phone

- First time in a session: `tools/remake/headless/session_setup.sh <work dir> all` (the owner's dumps from Drive, remake_tool,
  the headless Azahar core, Ghidra's analysed program; about an hour, once a session). Its header says what each step gives.
- Remake tooling: `cmake -S tests/remake -B <work>/build-tests -G Ninja && ninja -C <work>/build-tests`, every
  `*_test` passing, before any push.
- In the game: a headless run (`tools/remake/headless/fast_run.sh`) of the change itself: a screenshot, `mem`, `report`,
  `audio NAME` (a WAV of the game's sound). Say in the commit what was run and what was not; "not tested on the phone" for
  everything user-facing until the owner is back (CLAUDE.md §8).
- Android app code cannot be compiled in these sessions (no Android SDK): leave it alone, or say it is unbuilt.

### 3.1 Run recipes (20 October; add yours here, not only in a log)

A run that does not show the field is first a recipe question: check against these before debugging the game.

- **An X/Y zone, Continue start** (A's run hTr1, Vaniville):
  `oras-xy-world <oras.3ds> <x.3ds> <mod> ghidra/function_names.tsv --matrices 0 --character-models 64 [--story on|off]`;
  `oras-engine <oras.3ds> <mod> <the "oras-engine options:" line of its log> --character-models 64 --memory 124
  --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000` (without the models and
  memory options the X/Y packs do not fit: the top screen stays black); `oras-save <save> <out> <zone> <x> <z>
  --save-records 32 --characters-from <oras.3ds> <mod>` at a tile of the zone itself (538: 467.5 599.5);
  `POMEGRADE_SAVE=<out> fast_run.sh <work> <name> <mod> "" "wait 300;shot a;report"`. fast_run.sh prints "field not up"
  even when the shot is whole: look at the shot.
- **A new game** (C's run st4, to X/Y's bedroom: about 25 min drawing every frame, 6 min (352 s, B's run ng7) with `draw every 15` from the script's start instead of `draw off`/`draw on`): `POMEGRADE_SAVE=none run_local.sh ...` (no save), the
  presses in C's log entry of 20 October 09:00, ending with `save NAME`; later runs start from it in about 2 min
  (`POMEGRADE_SAVE=none POMEGRADE_STATE=<work>/runs/<run>/NAME.state run_local.sh ... "wait 120;load NAME;..."`). The crash
  such a state gave on load was the software renderer's (fixed in 61ca0ae4: a core built before it still crashes, rebuild
  it with session_setup.sh's `core` step).

- **An X/Y gym, end to end** (A, 21 October, Grant's: X zone 160, mod zone 538): `oras-xy-world ... --matrices 58
  --character-models 64 --story on`; `oras-engine` with its log's options plus `--xy-gyms <x.3ds>` and the memory options
  above; `oras-save <save> <out> 538 20.5 17.5 --save-records 32 --height 414 --characters-from <oras.3ds> <mod>` (Grant's
  floor: a character's height is its record's float at word 22, in pixels); `fast_run.sh ... "wait 300;press up;press a;
  mash a 30;..."` with a shot every 10-20 s. For a won battle, his team made weak: `oras-sandbox <oras.3ds> <mod> weak.txt --onto <mod>`, weak.txt
  `team 1026` / `pokemon 129 1` / `end` (Grant: X's trainer 76, the one his zone script fights, 950 + 76 in the mod,
  named Lino in French, Amaura and Tyrunt at 25; ORAS_ENGINE.md 0.1). Runs hG22 (lost:
  home), hG23 (won: the arena, his victory scene, badge, TM), hG24 (won the same way, his team made weak by the
  `--onto` command above: one Magikarp at level 1).
- **A scene with a choice** (A, 21 October): Hoenn, `oras-sandbox <oras.3ds> <mod> sandbox/scene_choice.txt` (no engine
  options), `oras-save <save> <out> 6 100.5 172.5`, then `"wait 120;hold up 20;wait 40;press a;wait 30;<press a | press
  down;wait 20;press a>;...;mem 0x8C7DF78 4"`. Kalos, `sandbox/scene_choice_xy.txt` or `scene_full_xy.txt` with `--xy
  <x.3ds> --character-models 64`, `oras-engine <oras.3ds> <mod> --zone-rows 545 --script-mask 538:0xA47 --script-mask
  539:0xA43 --story large` plus the models and memory options, `oras-save <save> <out> 538 20.5 48.5 --save-records 32
  --characters-from <oras.3ds> <mod>`. Runs hC4_yes, hC4_no, hCX4, hCX5, hCF6.
- **Speed** (A's runs fps_*, 20 October, the idle field): drawing every frame 2 frames/s, `draw every 15` 18-24 (one frame
  in 15 drawn, shots whole), `draw off` about 67. Use `draw off` wherever no picture is needed, else `draw every 15`
  (the title and Continue menu, scenes waited on by shots); plain drawing only for a check of every frame.

### 3.2 Asking and waking

- **Pinging through GitHub** (owner's go-ahead, 20 October; the agents are on different accounts, so no direct messages):
  each agent pushes a branch of its own for it (`claude/agent-<a|b|c>-ping`, from `main` plus one empty commit, so it
  always differs from `main`: a pull request of the work branch closes itself as merged once `main` holds it, as A's #126
  did), opens one draft pull request of it into `main` (never merged),
  subscribes its session to it (`subscribe_pr_activity`), and gives its number in its log. To wake an agent, comment on
  its pull request: one or two lines, "Needs: ..." and the log entry to read; the full message stays in your log. Only
  one session follows a pull request, so never subscribe to another agent's.
- Times in log entries are the real clock's (`date -u +%H:%M`), not estimated.

- Write `Needs: nothing` when nothing is asked. A is woken only by an entry to A (or to all) that needs something, or one
  asking for a merge; other entries are read at A's next reading.
- Merge early: ask for a merge as soon as a piece builds, passes the tests and was run headless, not when a whole feature
  is done, so the others build on it. A merges each such branch the same session.

## 4. Shared files

`OrasXyWorld.cpp`, `OrasXyEvents.cpp`, `OrasXyScript.cpp` (its native rename table), `OrasEngine.cpp`, `RemakeTool.cpp`,
`tools/remake/CMakeLists.txt`, `tests/remake/CMakeLists.txt`, `.github/workflows/remake-mod.yml`:

- Before changing one outside your area, write it in your log ("to all: I will add X to OrasXyWorld.cpp's zone loop") and
  keep the change small: a call into your own file, a line in a table, an option.
- Add to a list (CMake sources, the rename table, options) at its end, so two agents' additions merge without conflict.
- Merge `main` into your branch at the start of each session and before asking for a merge (§5).

## 5. Into `main`

- A decides when to merge into `main` (the owner's decision, 20 October: no approval of the owner needed): a branch whose
  agent asks in its log, or A's own, once the merged tree builds and every test passes; A writes in its own log what was
  merged. Conflicts in a file of another agent's area are resolved by that agent.
- Nobody rewrites `main`'s history or pushes to another agent's branch.
- The owner's rules (CLAUDE.md) apply to all three agents as written.
