// ApplyNames: the function names of tools/remake/ghidra/function_names.tsv given to the Ghidra program (ORAS_ENGINE.md 6), so
// that a session's decompilation reads in the engine's own terms; each line "address<TAB>name<TAB>role<TAB>read|guess", the
// role kept as the function's comment. A program built by session_setup.sh takes them after its analysis.
// args: <function_names.tsv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
public class ApplyNames extends GhidraScript {
    public void run() throws Exception {
        int named = 0, missing = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            if (line.isBlank() || line.startsWith("#")) continue;
            String[] f = line.split("\t");
            if (f.length < 2) continue;
            Function fn = getFunctionAt(toAddr(Long.parseLong(f[0], 16)));
            if (fn == null) { missing++; continue; }
            fn.setName(f[1], SourceType.USER_DEFINED);
            if (f.length > 2) fn.setComment(f[2] + (f.length > 3 ? " (" + f[3] + ")" : ""));
            named++;
        }
        println("named " + named + " functions, " + missing + " addresses with no function");
    }
}
