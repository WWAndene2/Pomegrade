// spica: what SPICA (external/SPICA) reads of a 3DS file, from the command line (SpicaCli.csproj).
//   spica info <file> [--skeleton <file>]          the scene: models (meshes, bones, materials), textures, animations
//   spica dae <file> <out.dae> [--model N] [--anim N] [--skeleton <file>]   COLLADA export (a model, an animation on it)
//   spica smd <file> <out.smd> [--model N] [--anim N] [--skeleton <file>]   StudioMdl export
//   spica motions <file>                            a GF motion pack (a Pokemon PB pack's file 0, a title pack): each
//                                                   motion's frames and animated bones (GF1Motion)
// <file>: a BCH, CGFX, GF package (PC Pokemon model, PB/PK animations, GR map piece, MM/CM characters, AD/PT textures,
// BG, BS), or a bare GF model/motion. --skeleton: a model file (a PC pack) whose skeleton animations are read against.
using SPICA.Formats.CtrGfx;
using SPICA.Formats.CtrH3D;
using SPICA.Formats.CtrH3D.Animation;
using SPICA.Formats.CtrH3D.Model;
using SPICA.Formats.Generic.COLLADA;
using SPICA.Formats.Generic.StudioMdl;
using SPICA.Formats.GFL;
using SPICA.Formats.GFL.Motion;
using SPICA.Formats.GFL2;
using SPICA.Formats.GFL2.Model;
using SPICA.Formats.CtrH3D.Model.Material;
using SPICA.WinForms.Formats;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

static class SpicaCli
{
    static int Main(string[] args)
    {
        if (args.Length < 2) return Usage();
        var rest = new List<string>(args);
        string Option(string name)
        {
            int at = rest.IndexOf(name);
            if (at < 0 || at + 1 >= rest.Count) return null;
            string v = rest[at + 1];
            rest.RemoveRange(at, 2);
            return v;
        }
        try
        {
            string skeletonFile = Option("--skeleton");
            int model = int.Parse(Option("--model") ?? "0"), anim = int.Parse(Option("--anim") ?? "-1");
            H3DDict<H3DBone> skeleton = null;
            if (skeletonFile != null)
            {
                H3D s = Open(skeletonFile, null);
                if (s == null || s.Models.Count == 0) throw new InvalidDataException(skeletonFile + " holds no model to take a skeleton from");
                skeleton = s.Models[0].Skeleton;
            }
            switch (rest[0])
            {
                case "info": Info(Open(rest[1], skeleton) ?? throw new InvalidDataException("format not recognised")); return 0;
                case "dae" when rest.Count >= 3: new DAE(Open(rest[1], skeleton), model, anim).Save(rest[2]); return 0;
                case "smd" when rest.Count >= 3: new SMD(Open(rest[1], skeleton), model, anim).Save(rest[2]); return 0;
                case "motions": Motions(rest[1]); return 0;
            }
            return Usage();
        }
        catch (Exception e) when (e is InvalidDataException || e is IOException || e is FormatException || e is EndOfStreamException)
        {
            Console.Error.WriteLine("error: " + e.Message);
            return 1;
        }
    }

    static int Usage()
    {
        Console.Error.WriteLine("usage: spica info|dae|smd|motions <file> [out] [--model N] [--anim N] [--skeleton <file>]");
        return 2;
    }

    // the file's format, by magic (SPICA.WinForms' FormatIdentifier, without its UI)
    static H3D Open(string path, H3DDict<H3DBone> skeleton)
    {
        byte[] data = File.ReadAllBytes(path);
        if (data.Length < 4) return null;
        string magic = Encoding.ASCII.GetString(data, 0, 4);
        if (magic.StartsWith("BCH")) return H3D.Open(data);
        var fs = new MemoryStream(data);
        if (magic.StartsWith("CGFX")) return Gfx.Open(fs);
        if (GFPackage.IsValidPackage(fs))
        {
            GFPackage.Header header = GFPackage.GetPackageHeader(fs);
            switch (header.Magic)
            {
                case "BG": return GFL2OverWorld.OpenAsH3D(fs, header, skeleton);
                case "BS": return GFBtlSklAnim.OpenAsH3D(fs, header, skeleton);
                case "CM": return GFCharaModel.OpenAsH3D(fs, header);
                case "GR": return GFOWMapModel.OpenAsH3D(fs, header);
                case "MM": return GFOWCharaModel.OpenAsH3D(fs, header);
                case "PC": return GFPkmnModel.OpenAsH3D(fs, header, skeleton);
                case "PK":
                case "PB": return GFPkmnSklAnim.OpenAsH3D(fs, header, skeleton);
            }
            throw new InvalidDataException("GF package " + header.Magic + " not read by SPICA");
        }
        var reader = new BinaryReader(fs);
        switch (BitConverter.ToUInt32(data, 0))
        {
            case 0x15122117: { var h = new H3D(); h.Models.Add(new GFModel(reader, "Model").ToH3DModel()); return h; }
            case 0x00010000: return new GFModelPack(reader).ToH3D();
        }
        return null;
    }

    static void Info(H3D scene)
    {
        Console.WriteLine($"{scene.Models.Count} models, {scene.Textures.Count} textures, {scene.LUTs.Count} LUTs, {scene.Shaders.Count} shaders");
        for (int i = 0; i < scene.Models.Count; i++)
        {
            H3DModel m = scene.Models[i];
            Console.WriteLine($"model {i} {m.Name}: {m.Meshes.Count} meshes, {m.Materials.Count} materials, {m.Skeleton.Count} bones");
            for (int b = 0; b < m.Skeleton.Count; b++)
            {
                H3DBone bone = m.Skeleton[b];
                Console.WriteLine($"  bone {b} {bone.Name} parent {bone.ParentIndex} T {bone.Translation} R {bone.Rotation} S {bone.Scale}");
            }
            foreach (H3DMaterial mat in m.Materials)
                Console.WriteLine($"  material {mat.Name}: textures {mat.Texture0Name} {mat.Texture1Name} {mat.Texture2Name}");
            for (int k = 0; k < m.Meshes.Count; k++)
                Console.WriteLine($"  mesh {k}: material {m.Meshes[k].MaterialIndex}, {m.Meshes[k].RawBuffer.Length / Math.Max(1, m.Meshes[k].VertexStride)} vertices, {m.Meshes[k].SubMeshes.Count} submeshes");
        }
        foreach (var t in scene.Textures) Console.WriteLine($"texture {t.Name}: {t.Width} x {t.Height} {t.Format}, {t.MipmapSize} mipmaps");
        void Anims(string kind, IEnumerable<H3DAnimation> list)
        {
            foreach (H3DAnimation a in list)
            {
                Console.WriteLine($"{kind} animation {a.Name}: {a.FramesCount} frames, loop {a.AnimationFlags}, {a.Elements.Count} elements");
                foreach (H3DAnimationElement e in a.Elements) Console.WriteLine($"  {e.Name} {e.TargetType} {e.PrimitiveType}");
            }
        }
        Anims("skeletal", scene.SkeletalAnimations);
        foreach (H3DMaterialAnim a in scene.MaterialAnimations) Console.WriteLine($"material animation {a.Name}: {a.FramesCount} frames, {a.Elements.Count} elements");
        Anims("visibility", scene.VisibilityAnimations);
        Anims("camera", scene.CameraAnimations);
        Anims("light", scene.LightAnimations);
        Anims("fog", scene.FogAnimations);
    }

    static void Motions(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read);
        var pack = new GF1MotionPack(new BinaryReader(fs));
        Console.WriteLine($"{pack.Count} motions");
        foreach (GF1Motion m in pack)
        {
            Console.WriteLine($"motion slot {m.Index + 1}: {m.FramesCount} frames, {m.Bones.Count} bones animated");
            foreach (GF1MotBoneTransform b in m.Bones)
            {
                string Track(string n, List<GF1MotKeyFrame> k) => k.Count == 0 ? "" : k.Count == 1 ? $" {n}={k[0].Value:0.###}" : $" {n}:{k.Count}keys";
                Console.WriteLine($"  {b.Name}{(b.IsWorldSpace ? " (world)" : "")}:" + Track("tx", b.TranslationX) + Track("ty", b.TranslationY) + Track("tz", b.TranslationZ)
                    + Track("rx", b.RotationX) + Track("ry", b.RotationY) + Track("rz", b.RotationZ) + Track("sx", b.ScaleX) + Track("sy", b.ScaleY) + Track("sz", b.ScaleZ));
            }
        }
    }
}
