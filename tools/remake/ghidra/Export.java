// Export: every function (entry, size, callers, callees) to functions.tsv; with args, those addresses decompiled to decomp.c
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.app.decompiler.*;
import java.io.*;
public class Export extends GhidraScript {
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        try (PrintWriter w = new PrintWriter(out + "/functions.tsv")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true))
                w.printf("%s\t%d\t%d\t%d%n", f.getEntryPoint(), f.getBody().getNumAddresses(),
                    f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size());
        }
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(out + "/decomp.c")) {
            for (int i = 1; i < getScriptArgs().length; i++) {
                Function f = getFunctionContaining(toAddr(getScriptArgs()[i]));
                if (f == null) { w.println("// no function at " + getScriptArgs()[i]); continue; }
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                w.println("// ---- " + getScriptArgs()[i] + " in " + f.getName());
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed: " + r.getErrorMessage());
            }
        }
    }
}
