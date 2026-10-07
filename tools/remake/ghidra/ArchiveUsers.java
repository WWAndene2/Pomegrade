// ArchiveUsers: every call that opens one of the game's archives by number (FUN_0011c94c(obj, heap, id, flag); the number is
// the path's digits, a/X/Y/Z = X*100 + Y*10 + Z, from the path table at 0x5F5050: ORAS_ENGINE.md 5) with the function making
// it, so that each archive is tied to the code that reads it. args: <out file>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.util.*;
import java.util.regex.*;
public class ArchiveUsers extends GhidraScript {
    public void run() throws Exception {
        Address open = toAddr(0x11c94cL);
        Set<Address> targets = new LinkedHashSet<>(List.of(open));
        for (Function f : currentProgram.getFunctionManager().getFunctions(true))
            if (f.isThunk() && f.getThunkedFunction(true) != null && f.getThunkedFunction(true).getEntryPoint().equals(open)) targets.add(f.getEntryPoint());
        Set<Function> callers = new LinkedHashSet<>();
        for (Address t : targets) for (Reference r : getReferencesTo(t)) {
            Function f = getFunctionContaining(r.getFromAddress());
            if (f != null && !targets.contains(f.getEntryPoint())) callers.add(f);
        }
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        Pattern call = Pattern.compile("FUN_0011c94c\\(([^;]*)\\)");
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0])) {
            for (Function f : callers) {
                DecompileResults r = d.decompileFunction(f, 30, monitor);
                if (!r.decompileCompleted()) { w.printf("%s\t%s\t?%n", f.getEntryPoint(), f.getName()); continue; }
                Matcher m = call.matcher(r.getDecompiledFunction().getC());
                while (m.find()) {
                    String[] a = m.group(1).split(",");
                    w.printf("%s\t%s\t%s%n", f.getEntryPoint(), f.getName(), a.length > 2 ? a[2].trim() : "?");
                }
            }
        }
    }
}
