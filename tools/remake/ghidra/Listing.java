// Listing: the functions at the given addresses as Ghidra disassembles them, for code its decompiler drops (the VFP float
// instructions of the camera natives, ORAS_ENGINE.md 6), into <out dir>/listing.s. Each instruction with its address,
// the function and label names of the addresses it reaches, and the words it loads from the program's literal pools,
// also read as floats (a constant 1.5 shows as "=0x3fc00000 (1.5)"). Read-only.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.*;
import java.io.*;

public class Listing extends GhidraScript {
    private String nameAt(Address a) {
        Function f = getFunctionAt(a);
        if (f != null) return f.getName();
        Symbol s = getSymbolAt(a);
        return s != null ? s.getName() : null;
    }

    public void run() throws Exception {
        String out = getScriptArgs()[0];
        try (PrintWriter w = new PrintWriter(out + "/listing.s")) {
            for (int i = 1; i < getScriptArgs().length; i++) {
                Function f = getFunctionContaining(toAddr(getScriptArgs()[i]));
                if (f == null) { w.println("; no function at " + getScriptArgs()[i]); continue; }
                w.println("; ---- " + f.getEntryPoint() + " " + f.getName());
                InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
                while (it.hasNext()) {
                    Instruction ins = it.next();
                    StringBuilder line = new StringBuilder(String.format("%s  %-40s", ins.getAddress(), ins));
                    for (Reference r : ins.getReferencesFrom()) {
                        Address to = r.getToAddress();
                        String name = nameAt(to);
                        if (r.getReferenceType().isFlow()) {
                            if (name != null) line.append(" ; -> ").append(name);
                        } else if (r.getReferenceType().isRead() && currentProgram.getMemory().contains(to)) {
                            try {
                                int word = currentProgram.getMemory().getInt(to);
                                line.append(String.format(" ; =0x%08x (%s)", word, Float.intBitsToFloat(word)));
                                Address target = toAddr(word & 0xFFFFFFFFL);
                                String targetName = currentProgram.getMemory().contains(target) ? nameAt(target) : null;
                                if (targetName != null) line.append(" ").append(targetName);
                            } catch (MemoryAccessException | ghidra.program.model.address.AddressOutOfBoundsException e) {
                                line.append(" ; [").append(to).append("]");
                            }
                        }
                    }
                    w.println(line);
                }
            }
        }
    }
}
