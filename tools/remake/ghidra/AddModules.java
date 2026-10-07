// AddModules: the code modules linked by prototype/cro_link.py added to the .code's program as one block at 0x10000000,
// each export named (modules.tsv) and disassembled from, so that auto-analysis follows them with the .code
// args: <modules.bin> <modules.tsv>
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import java.io.*;
import java.nio.file.*;
public class AddModules extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] image = Files.readAllBytes(Paths.get(a[0]));
        Address base = toAddr(0x10000000L);
        if (getMemoryBlock(base) == null) {
            MemoryBlock b = currentProgram.getMemory().createInitializedBlock("modules", base, new ByteArrayInputStream(image), image.length, monitor, false);
            b.setRead(true); b.setWrite(true); b.setExecute(true);
        }
        String module = "";
        for (String line : Files.readAllLines(Paths.get(a[1]))) {
            String[] f = line.split("\t");
            if (!line.startsWith("\t")) { module = f[0]; createLabel(toAddr(Long.decode(f[1])), module.replace("|", "_") + "_start", true, SourceType.IMPORTED); continue; }
            Address at = toAddr(Long.decode(f[2]));
            try { createLabel(at, (module + "_" + f[1]).replaceAll("[^A-Za-z0-9_]", "_"), true, SourceType.IMPORTED); } catch (Exception e) { }
            new DisassembleCommand(at, null, true).applyTo(currentProgram, monitor);
        }
    }
}
