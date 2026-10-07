// CallGraph: the Ghidra program's calls and no-return marks, for the name review (prototype/name_review.py,
// ORAS_ENGINE.md 6) and for checking FixNoReturn. Writes, in the given folder:
//   edges.tsv     caller<TAB>callee, one line per call; a call to a thunk (a module's import stub) is written to the
//                 function it jumps to, so that a module function and the .code function it calls are neighbours
//   noreturn.tsv  entry<TAB>name<TAB>size<TAB>returns<TAB>references, one line per function marked no-return; "returns" is
//                 1 when its body holds a return (a terminal flow that is not a call): such a mark is wrong (FixNoReturn)
// args: <out dir>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
public class CallGraph extends GhidraScript {
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        Listing listing = currentProgram.getListing();
        int edges = 0, marked = 0, wrong = 0;
        try (PrintWriter e = new PrintWriter(out + "/edges.tsv"); PrintWriter n = new PrintWriter(out + "/noreturn.tsv")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                for (Function c : f.getCalledFunctions(monitor)) {
                    Function t = c.isThunk() ? c.getThunkedFunction(true) : c;
                    e.printf("%s\t%s%n", f.getEntryPoint(), t.getEntryPoint());
                    edges++;
                }
                if (!f.hasNoReturn()) continue;
                int returns = 0;
                for (Instruction i : listing.getInstructions(f.getBody(), true))
                    if (i.getFlowType().isTerminal() && !i.getFlowType().isCall()) { returns = 1; break; }
                n.printf("%s\t%s\t%d\t%d\t%d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(), returns,
                    getReferencesTo(f.getEntryPoint()).length);
                marked++;
                wrong += returns;
            }
        }
        println("CallGraph: " + edges + " calls, " + marked + " functions marked no-return, " + wrong + " of them return");
    }
}
