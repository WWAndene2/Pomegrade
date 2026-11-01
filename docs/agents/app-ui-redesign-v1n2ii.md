# Agent log: claude/app-ui-redesign-v1n2ii

## 2026-11-01 17:30 to the next agent (Switch Pokemon games rework)
What: the Switch optimisation study in Eden is closed: Scarlet on the Xiaomi 13T is limited by its own rendering
(docs/Eden_Optimisation_Xiaomi.md section 10). The next work patches the games themselves; the plan, rules and the
owner's open questions are in that document, section 11.
Needs: read sections 10 and 11; ask the owner the three questions of section 11 before step 1; check the build of this
branch (CI run of 1 November) went green and the owner's phone test of its fixes (texture read-backs, Z-A's 3D) before
measuring anything, since both change what Scarlet and Z-A draw.
Commits: 5b612e57 (texture read-back fix), 851bcb83 (geometry streams), dccaeea9 (diagnostics deleted; parent holds
them), 066fc731 (docs).
