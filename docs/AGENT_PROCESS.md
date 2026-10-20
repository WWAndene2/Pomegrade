# Working with several agents

Set up on 20 October at the owner's request: three Claude agents work on Pomegrade at once, each on its own branch,
asynchronously. The owner is away and does not test on the phone during this period, so every check is a headless run in
a session (§3 below). This file is the process; each agent's messages are its log (§2).

## 1. Who does what

One owner per area. An agent changes files outside its area only as §4 says.

| Agent | Branch | Area | Its files |
|---|---|---|---|
| A, coordinator | `claude/inspiring-keller-280lek` | X/Y's trainers, teams, trainer classes, battle backgrounds and battle music; merging into `main` (§5) | `tools/remake/src/OrasXyTrainers.*` (new), `OrasEngine.*`, `OrasXyWorld.*`, `RemakeTool.cpp` |
| B | `claude/hopeful-pascal-x96527` | X/Y's own gameplay features: the natives Omega Ruby has none of (121; commit 0f94846a "X/Y scripts: 8 more natives renamed" says how their names were found: X/Y's tables' strings hashed; Lumiose's gym, Battle Chateau, clothes and salon, the Elite Four's elevator, berry survey...), as new engine code | new `tools/remake/src/OrasXy<Feature>.*` per feature, its engine patches in a new file beside `OrasEngine.cpp` |
| C | `ccr-52ddfc61-gcuszh` | X/Y's story (`oras-xy-world --story on`): the new game's start in Vaniville, the scenes, story flags and works across a save | `tools/remake/src/OrasXyEvents.*`, `OrasXyScript.*` (all but the rename table, §4), `tools/remake/sandbox/` |

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

## 4. Shared files

`OrasXyWorld.cpp`, `OrasXyEvents.cpp`, `OrasXyScript.cpp` (its native rename table), `OrasEngine.cpp`, `RemakeTool.cpp`,
`tools/remake/CMakeLists.txt`, `tests/remake/CMakeLists.txt`, `.github/workflows/remake-mod.yml`:

- Before changing one outside your area, write it in your log ("to all: I will add X to OrasXyWorld.cpp's zone loop") and
  keep the change small: a call into your own file, a line in a table, an option.
- Add to a list (CMake sources, the rename table, options) at its end, so two agents' additions merge without conflict.
- Merge `main` into your branch at the start of each session and before asking for a merge (§5).

## 5. Into `main`

- A merges a branch into `main` when its agent asks in its log, after the merged tree builds and every test passes; A
  writes in its own log what was merged. Conflicts in a file of another agent's area are resolved by that agent.
- Nobody rewrites `main`'s history or pushes to another agent's branch.
- The owner's rules (CLAUDE.md) apply to all three agents as written.
