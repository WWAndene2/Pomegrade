// FatalCensus: every call to the game's fatal-error routine (0x11EF3C and its entry 0x11EF4C, ORAS_ENGINE.md 2) with the
// decompiled lines before it: the engine's hard limits, each the condition under which it stops. Calls made through a
// module's import stub (ldr pc, =target) are found by following the stubs' references too.
// args: <out file>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
public class FatalCensus extends GhidraScript {
    public void run() throws Exception {
        Set<Address> targets = new LinkedHashSet<>(List.of(toAddr(0x11EF3CL), toAddr(0x11EF4CL)));
        // thunks and stubs that lead to the routine
        for (Function f : currentProgram.getFunctionManager().getFunctions(true))
            if (f.isThunk() && f.getThunkedFunction(true) != null && targets.contains(f.getThunkedFunction(true).getEntryPoint())) targets.add(f.getEntryPoint());
        Map<Function, List<Address>> calls = new LinkedHashMap<>();
        for (Address t : targets)
            for (Reference r : getReferencesTo(t)) {
                if (!r.getReferenceType().isCall() && !r.getReferenceType().isJump()) continue;
                Function f = getFunctionContaining(r.getFromAddress());
                if (f == null || targets.contains(f.getEntryPoint())) continue;
                calls.computeIfAbsent(f, k -> new ArrayList<>()).add(r.getFromAddress());
            }
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        int sites = 0;
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0])) {
            for (Map.Entry<Function, List<Address>> e : calls.entrySet()) {
                sites += e.getValue().size();
                w.printf("== %s %s calls %s%n", e.getKey().getEntryPoint(), e.getKey().getName(), e.getValue());
                DecompileResults r = d.decompileFunction(e.getKey(), 30, monitor);
                if (!r.decompileCompleted()) { w.println("   (no decompilation)"); continue; }
                String[] lines = r.getDecompiledFunction().getC().split("\n");
                for (int i = 0; i < lines.length; i++)
                    if (lines[i].contains("FUN_0011ef4c") || lines[i].contains("FUN_0011ef3c"))
                        for (int k = Math.max(0, i - 3); k <= i; k++) w.println("   " + lines[k].trim());
            }
            w.printf("%d functions, %d call sites%n", calls.size(), sites);
        }
    }
}
