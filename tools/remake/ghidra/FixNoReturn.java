// FixNoReturn: clears the no-return mark Ghidra's analysis puts on functions that do return (ORAS_ENGINE.md 6), and reads
// again the code it had left out after their calls. Checked on 7 October: 8 of the 172 functions it marked no-return hold a
// return instruction (7, then 1 more once their callers were read again), among them memclr (0x301FBC, 357 callers), two
// global getters (0x14E348, 0x139660) and 0x3FE5C8; every caller stopped at the call, so the script natives decompiled as
// one-line "no-return stubs". A function marked no-return is cleared when its body holds a return (a terminal flow that is
// not a call); clearing one lengthens its callers, which can hold a return in turn, so the pass repeats until nothing
// changes. Runs after the analysis, before ApplyNames.
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.util.*;
public class FixNoReturn extends GhidraScript {
    boolean returns(Function f) {
        for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true))
            if (i.getFlowType().isTerminal() && !i.getFlowType().isCall()) return true;
        return false;
    }
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        int cleared = 0, pass = 0;
        while (true) {
            List<Function> wrong = new ArrayList<>();
            for (Function f : currentProgram.getFunctionManager().getFunctions(true))
                if (f.hasNoReturn() && returns(f)) wrong.add(f);
            if (wrong.isEmpty()) break;
            pass++;
            Set<Function> callers = new HashSet<>();
            for (Function f : wrong) {
                f.setNoReturn(false);
                cleared++;
                // the function and its thunks (a module's import stubs, which take the mark from it): the code modules
                // call 0x3FE5C8 through DllField's stub 0x10243040
                List<Reference> refs = new ArrayList<>(Arrays.asList(getReferencesTo(f.getEntryPoint())));
                Address[] thunks = f.getFunctionThunkAddresses(true);
                if (thunks != null) for (Address t : thunks) refs.addAll(Arrays.asList(getReferencesTo(t)));
                for (Reference r : refs) {
                    if (!r.getReferenceType().isCall()) continue;
                    Instruction call = listing.getInstructionAt(r.getFromAddress());
                    if (call == null) continue;
                    if (call.getFlowOverride() == FlowOverride.CALL_RETURN) call.setFlowOverride(FlowOverride.NONE);
                    Address next = call.getMaxAddress().next();
                    if (next != null && listing.getInstructionAt(next) == null) disassemble(next);
                    Function caller = getFunctionContaining(r.getFromAddress());
                    if (caller != null) callers.add(caller);
                }
            }
            for (Function c : callers) CreateFunctionCmd.fixupFunctionBody(currentProgram, c, monitor);
            println("pass " + pass + ": " + wrong.size() + " cleared, " + callers.size() + " callers re-read");
        }
        println("cleared the no-return mark of " + cleared + " functions in " + pass + " passes");
    }
}
