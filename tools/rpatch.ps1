# Applier for .rpatch files (built by tools\make_rpatch.py), written in C# inside PowerShell (no Python needed).
# Apply: checks the SHA-1 of the user's original file, builds the new file in a temporary file, checks the SHA-1 of the
# result and only then swaps it in. Before swapping, the original bytes that change (and the original size) are saved to a
# LOCAL .rbak file; Revert uses it to bring back a file identical to the original (checked by SHA-1).
# Returns: Apply 0 = patched, 1 = already patched, 2 = file is not the expected original, 3 = wrong result.
#          Revert 0 = restored, 1 = already original, 2 = file is not in the patched state, 3 = check failed.
Add-Type -Language CSharp -TypeDefinition @'
using System; using System.IO; using System.Collections.Generic; using System.Security.Cryptography;
public static class RPatch {
    public static string Sha1(string path) {
        using (var s = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20))
        using (var h = SHA1.Create()) return BitConverter.ToString(h.ComputeHash(s)).Replace("-", "").ToLowerInvariant();
    }
    static string Hex(byte[] b) { return BitConverter.ToString(b).Replace("-", "").ToLowerInvariant(); }
    class Hdr { public long OrigSize, NewSize; public string OrigSha, NewSha; public int Nops; public long OpsAt; }
    static Hdr Read(BinaryReader r) {
        if (new string(r.ReadChars(8)) != "ARAPATCH") throw new Exception("invalid patch");
        var h = new Hdr(); r.ReadUInt32(); h.OrigSize = (long)r.ReadUInt64(); h.OrigSha = Hex(r.ReadBytes(20));
        h.NewSize = (long)r.ReadUInt64(); h.NewSha = Hex(r.ReadBytes(20)); h.Nops = (int)r.ReadUInt32(); h.OpsAt = r.BaseStream.Position; return h;
    }
    public static string Info(string patch) {
        using (var r = new BinaryReader(File.OpenRead(patch))) { var h = Read(r); return h.OrigSha + " " + h.NewSha; }
    }
    static void Copy(Stream from, Stream to, long n, byte[] buf) {
        while (n > 0) { int k = (int)Math.Min(n, buf.Length); int got = from.Read(buf, 0, k); if (got <= 0) throw new Exception("short read"); to.Write(buf, 0, got); n -= got; }
    }
    public static int Apply(string target, string patch, string backup) {
        string cur = Sha1(target); Hdr h;
        using (var r = new BinaryReader(File.OpenRead(patch))) h = Read(r);
        if (cur == h.NewSha) return 1;
        if (cur != h.OrigSha) return 2;
        string tmp = target + ".reborn_tmp"; var ranges = new List<long[]>(); var buf = new byte[1 << 20];
        using (var r = new BinaryReader(new FileStream(patch, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20)))
        using (var o = new FileStream(target, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20))
        using (var t = new FileStream(tmp, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 20)) {
            r.BaseStream.Position = h.OpsAt; long pos = 0;
            for (int i = 0; i < h.Nops; i++) {
                byte typ = r.ReadByte(); long src = 0, len;
                if (typ == 0) { src = (long)r.ReadUInt64(); len = (long)r.ReadUInt64(); o.Position = src; Copy(o, t, len, buf); }
                else { len = (long)r.ReadUInt64(); Copy(r.BaseStream, t, len, buf); }
                bool changed = typ != 0 || src != pos;
                if (changed && pos < h.OrigSize) {
                    long a = pos, b = Math.Min(pos + len, h.OrigSize);
                    if (ranges.Count > 0 && ranges[ranges.Count - 1][1] == a) ranges[ranges.Count - 1][1] = b; else ranges.Add(new long[] { a, b });
                }
                pos += len;
            }
        }
        if (Sha1(tmp) != h.NewSha) { File.Delete(tmp); return 3; }
        using (var o = new FileStream(target, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20))
        using (var w = new BinaryWriter(File.Create(backup))) {
            w.Write("ARARBAK1".ToCharArray()); w.Write(h.OrigSize); w.Write(h.OrigSha); w.Write(h.NewSha); w.Write(ranges.Count);
            foreach (var g in ranges) { w.Write(g[0]); w.Write(g[1] - g[0]); o.Position = g[0]; Copy(o, w.BaseStream, g[1] - g[0], buf); }
        }
        File.Delete(target); File.Move(tmp, target);
        return 0;
    }
    public static int Revert(string target, string backup) {
        using (var r = new BinaryReader(File.OpenRead(backup))) {
            if (new string(r.ReadChars(8)) != "ARARBAK1") throw new Exception("invalid backup");
            long osz = r.ReadInt64(); string osha = r.ReadString(), nsha = r.ReadString(); int n = r.ReadInt32();
            string cur = Sha1(target);
            if (cur == osha) return 1;
            if (cur != nsha) return 2;
            var buf = new byte[1 << 20];
            using (var t = new FileStream(target, FileMode.Open, FileAccess.ReadWrite, FileShare.None, 1 << 20)) {
                for (int i = 0; i < n; i++) { long a = r.ReadInt64(), len = r.ReadInt64(); t.Position = a; Copy(r.BaseStream, t, len, buf); }
                t.SetLength(osz);
            }
            return Sha1(target) == osha ? 0 : 3;
        }
    }
}
'@
