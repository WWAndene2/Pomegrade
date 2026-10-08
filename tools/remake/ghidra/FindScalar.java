// FindScalar: every instruction of the program (code Ghidra disassembled, not data) with a constant operand equal to one of
// the values given, and the function holding it, to find the code that handles a structure of a known size or stride
// (ORAS_ENGINE.md 0: a save block's size, a record stride). code_find.py --imm sweeps the raw bytes and also matches data
// read as instructions; this does not, and also sees constants Ghidra resolved from literal pools.
// args: <out file> <value>... (hex with 0x, or decimal)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.util.*;
public class FindScalar extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Long> values = new HashSet<>();
        for (int i = 1; i < args.length; i++) values.add(Long.decode(args[i]));
        Listing listing = currentProgram.getListing();
        int hits = 0;
        try (PrintWriter w = new PrintWriter(args[0])) {
            InstructionIterator it = listing.getInstructions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Instruction ins = it.next();
                for (int op = 0; op < ins.getNumOperands(); op++)
                    for (Object o : ins.getOpObjects(op))
                        if (o instanceof Scalar && values.contains(((Scalar) o).getUnsignedValue())) {
                            Function f = getFunctionContaining(ins.getAddress());
                            w.printf("%s\t0x%X\t%s\t%s%n", ins.getAddress(), ((Scalar) o).getUnsignedValue(),
                                f == null ? "(no function)" : f.getName() + "@" + f.getEntryPoint(), ins);
                            hits++;
                        }
            }
        }
        println("FindScalar: " + hits + " uses");
    }
}
