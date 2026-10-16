// Install engine: finds the game, reads what is installed, and runs each mod package's own install.ps1/uninstall.ps1
// (the same scripts as the release zips, already tested), so the exe and the zips always behave the same way.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.Win32;

namespace RebornSetup
{
    public enum ModKind { Dll, Rail, Leader }

    public sealed class ModDef
    {
        public string Key, Version, Resource;     // Key = zip key (AraQoL, CpuFix, Railroads, Brazil, SovietUnion)
        public ModKind Kind;
        public string Proxy;                       // game DLL replaced by the mod
        public string DataDir;                     // folder under Documents\My Games\Ara History Untold\Mods
        public long Size;
        public string Package => "Reborn-" + Key + "-" + Version;
    }

    public static class Engine
    {
        public const string AppId = "ProjectARAReborn";
        public const string RegRoot = @"Software\Project ARA Reborn";
        public const string SteamAppId = "2021880";
        public static readonly string PackVersion = Assembly.GetExecutingAssembly().GetName().Version.ToString(3);

        public static readonly string LogPath = Path.Combine(Path.GetTempPath(), "ProjectARA-Reborn-Setup.log");
        static readonly object LogLock = new object();
        public static void Log(string s)
        {
            lock (LogLock) { try { File.AppendAllText(LogPath, DateTime.Now.ToString("HH:mm:ss ") + s + Environment.NewLine, Encoding.UTF8); } catch { } }
        }

        // ---------- catalog (from the embedded zips: payload\Reborn-<Key>-<Version>.zip) ----------
        static readonly Dictionary<string, (ModKind kind, string proxy, string data)> Known = new Dictionary<string, (ModKind, string, string)>
        {
            ["AraQoL"] = (ModKind.Dll, "swresample-4.dll", ""),
            ["CpuFix"] = (ModKind.Dll, "avutil-58.dll", ""),
            ["Railroads"] = (ModKind.Rail, "avformat-60.dll", "Ferrovias"),
            ["Brazil"] = (ModKind.Leader, "swscale-7.dll", "Brasil"),
            ["SovietUnion"] = (ModKind.Leader, "swscale-7.dll", "URSS"),
        };
        public static readonly string[] Order = { "AraQoL", "CpuFix", "Railroads", "Brazil", "SovietUnion" };

        public static List<ModDef> LoadCatalog()
        {
            var asm = Assembly.GetExecutingAssembly();
            var list = new List<ModDef>();
            foreach (var res in asm.GetManifestResourceNames())
            {
                var m = Regex.Match(res, @"^payload\\Reborn-([A-Za-z]+)-([0-9.]+)\.zip$");
                if (!m.Success || !Known.ContainsKey(m.Groups[1].Value)) continue;
                var k = Known[m.Groups[1].Value];
                long size; using (var s = asm.GetManifestResourceStream(res)) size = s.Length;
                list.Add(new ModDef { Key = m.Groups[1].Value, Version = m.Groups[2].Value, Resource = res, Kind = k.kind, Proxy = k.proxy, DataDir = k.data, Size = size });
            }
            return list.OrderBy(d => Array.IndexOf(Order, d.Key)).ToList();
        }

        // ---------- game location ----------
        public static bool IsGameDir(string p)
        {
            try { return !string.IsNullOrWhiteSpace(p) && File.Exists(Path.Combine(p, "bin", "AraHistoryUntold.exe")); } catch { return false; }
        }

        public static string FindGame()
        {
            var libs = new List<string>();
            void AddReg(RegistryKey hive, string key, string name)
            {
                try { using (var k = hive.OpenSubKey(key)) { var v = k?.GetValue(name) as string; if (!string.IsNullOrEmpty(v)) libs.Add(v.Replace('/', '\\')); } } catch { }
            }
            AddReg(Registry.CurrentUser, @"Software\Valve\Steam", "SteamPath");
            AddReg(Registry.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath");
            AddReg(Registry.LocalMachine, @"SOFTWARE\Valve\Steam", "InstallPath");
            // the uninstall entry Steam writes for the game names its folder directly
            AddReg(Registry.LocalMachine, @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App " + SteamAppId, "InstallLocation");
            foreach (var s in libs.ToArray())
            {
                try
                {
                    var vdf = Path.Combine(s, "steamapps", "libraryfolders.vdf");
                    if (!File.Exists(vdf)) continue;
                    foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\""))
                        libs.Add(m.Groups[1].Value.Replace(@"\\", @"\"));
                }
                catch { }
            }
            foreach (var l in libs)
            {
                if (IsGameDir(l)) return l;
                try { var g = Path.Combine(l, "steamapps", "common", "Ara History Untold"); if (IsGameDir(g)) return g; } catch { }
            }
            return null;
        }

        public static string ModsDir => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "My Games", "Ara History Untold", "Mods");

        public static bool GameRunning() => Process.GetProcessesByName("AraHistoryUntold").Length > 0;

        public static long FreeBytes(string path)
        {
            try { return new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))).AvailableFreeSpace; } catch { return long.MaxValue; }
        }

        public static bool PythonAvailable()
        {
            try
            {
                var psi = new ProcessStartInfo("python", "-c \"import sys; print(sys.version_info[0])\"")
                { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
                using (var p = Process.Start(psi))
                {
                    var o = p.StandardOutput.ReadToEnd(); p.WaitForExit(8000);
                    return p.ExitCode == 0 && o.Trim() == "3";
                }
            }
            catch { return false; }
        }

        // ---------- installed state ----------
        static bool HasMarker(string file, string marker)
        {
            try { return File.Exists(file) && Encoding.ASCII.GetString(File.ReadAllBytes(file)).Contains(marker); } catch { return false; }
        }

        static bool ProxyInstalled(string game, string proxy)
        {
            var bin = Path.Combine(game, "bin");
            return File.Exists(Path.Combine(bin, proxy.Replace(".dll", "_orig.dll"))) && HasMarker(Path.Combine(bin, proxy), proxy.Replace(".dll", "_orig."));
        }

        public static bool IsInstalled(ModDef d, string game)
        {
            if (!IsGameDir(game)) return false;
            switch (d.Kind)
            {
                case ModKind.Dll: return ProxyInstalled(game, d.Proxy);
                case ModKind.Rail: return ProxyInstalled(game, d.Proxy) && Directory.Exists(Path.Combine(ModsDir, d.DataDir));
                default:
                    return File.Exists(Path.Combine(ModsDir, "_RebornLeaders", d.DataDir + ".installed")) && Directory.Exists(Path.Combine(ModsDir, d.DataDir));
            }
        }

        public static bool LeaderBaseInstalled(string game) => IsGameDir(game) && ProxyInstalled(game, "swscale-7.dll");

        public static string InstalledVersion(string key)
        {
            try { using (var k = Registry.CurrentUser.OpenSubKey(RegRoot + @"\Installed")) return k?.GetValue(key) as string; } catch { return null; }
        }

        static void SetInstalledVersion(string key, string ver)
        {
            using (var k = Registry.CurrentUser.CreateSubKey(RegRoot + @"\Installed"))
            {
                if (ver == null) k.DeleteValue(key, false); else k.SetValue(key, ver);
            }
        }

        public static string LastGamePath
        {
            get { try { using (var k = Registry.CurrentUser.OpenSubKey(RegRoot)) return k?.GetValue("GamePath") as string; } catch { return null; } }
            set { try { using (var k = Registry.CurrentUser.CreateSubKey(RegRoot)) k.SetValue("GamePath", value ?? ""); } catch { } }
        }

        // ---------- running a package script ----------
        public sealed class Result { public bool Ok; public string Error; public List<string> Warnings = new List<string>(); }

        /// <summary>Extracts the mod's embedded zip and runs its install.ps1 or uninstall.ps1. onLine gets every output line.</summary>
        public static Task<Result> RunAsync(ModDef d, bool install, string game, bool station3D, Action<string> onLine)
        {
            return Task.Run(() =>
            {
                var res = new Result();
                var work = Path.Combine(Path.GetTempPath(), "ProjectARA-Reborn", Guid.NewGuid().ToString("N"));
                try
                {
                    Directory.CreateDirectory(work);
                    using (var s = Assembly.GetExecutingAssembly().GetManifestResourceStream(d.Resource))
                    using (var z = new ZipArchive(s, ZipArchiveMode.Read))
                        z.ExtractToDirectory(work);
                    var script = Path.Combine(work, d.Package, install ? "install.ps1" : "uninstall.ps1");
                    if (!File.Exists(script)) throw new FileNotFoundException("Package script missing: " + script);

                    var args = new StringBuilder("-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script + "\" -GamePath \"" + game.TrimEnd('\\') + "\"");
                    if (install && d.Kind == ModKind.Rail && station3D) args.Append(" -Station3D");
                    var ps = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"WindowsPowerShell\v1.0\powershell.exe");
                    var psi = new ProcessStartInfo(ps, args.ToString())
                    {
                        UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Path.GetDirectoryName(script),
                        RedirectStandardOutput = true, RedirectStandardError = true,
                        StandardOutputEncoding = Encoding.Default, StandardErrorEncoding = Encoding.Default,
                    };
                    Log("> " + d.Package + (install ? " install" : " uninstall"));
                    var err = new StringBuilder();
                    using (var p = new Process { StartInfo = psi })
                    {
                        p.OutputDataReceived += (o, e) =>
                        {
                            if (e.Data == null) return;
                            Log("  " + e.Data);
                            if (e.Data.StartsWith("WARNING:", StringComparison.OrdinalIgnoreCase)) res.Warnings.Add(e.Data.Substring(8).Trim());
                            onLine?.Invoke(e.Data);
                        };
                        p.ErrorDataReceived += (o, e) => { if (e.Data == null) return; Log("! " + e.Data); lock (err) err.AppendLine(e.Data); };
                        p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine();
                        p.WaitForExit();
                        res.Ok = p.ExitCode == 0;
                        if (!res.Ok) res.Error = CleanError(err.ToString());
                    }
                    if (res.Ok) SetInstalledVersion(d.Key, install ? d.Version : null);
                }
                catch (Exception ex) { res.Ok = false; res.Error = ex.Message; Log("! " + ex); }
                finally { try { Directory.Delete(work, true); } catch { } }
                return res;
            });
        }

        // PowerShell prints a thrown error as "<message>\nAt line:... + CategoryInfo ..."; keep just the message
        static string CleanError(string e)
        {
            var lines = e.Replace("\r", "").Split('\n').Select(l => l.Trim()).Where(l => l.Length > 0).ToList();
            var msg = lines.TakeWhile(l => !l.StartsWith("At ") && !l.StartsWith("+ ")).ToList();
            if (msg.Count == 0) msg = lines.Take(1).ToList();
            var s = string.Join(" ", msg);
            var idx = s.IndexOf(" : ", StringComparison.Ordinal);                 // "C:\...\install.ps1 : message"
            if (idx > 0 && s.Substring(0, idx).EndsWith(".ps1", StringComparison.OrdinalIgnoreCase)) s = s.Substring(idx + 3);
            return s.Length > 0 ? s : "Unknown error (see the log).";
        }

        // ---------- Apps & Features entry (per user, points at a copy of this exe) ----------
        public static string InstalledExe => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Project ARA Reborn", "ProjectARA-Reborn-Setup.exe");

        public static void Register(long estimatedKb)
        {
            try
            {
                var self = Assembly.GetExecutingAssembly().Location;
                Directory.CreateDirectory(Path.GetDirectoryName(InstalledExe));
                if (!string.Equals(Path.GetFullPath(self), Path.GetFullPath(InstalledExe), StringComparison.OrdinalIgnoreCase))
                    File.Copy(self, InstalledExe, true);
                using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\" + AppId))
                {
                    k.SetValue("DisplayName", "Project ARA: Reborn");
                    k.SetValue("DisplayVersion", PackVersion);
                    k.SetValue("Publisher", "Spartak (Arxz0)");
                    k.SetValue("DisplayIcon", InstalledExe + ",0");
                    k.SetValue("InstallLocation", Path.GetDirectoryName(InstalledExe));
                    k.SetValue("UninstallString", "\"" + InstalledExe + "\" --uninstall");
                    k.SetValue("ModifyPath", "\"" + InstalledExe + "\"");
                    k.SetValue("EstimatedSize", (int)Math.Max(1, estimatedKb), RegistryValueKind.DWord);
                    k.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                    k.SetValue("URLInfoAbout", "https://github.com/Arxz0/Project-ARA-Reborn");
                }
            }
            catch (Exception ex) { Log("! register: " + ex.Message); }
        }

        public static void Unregister()
        {
            try { Registry.CurrentUser.DeleteSubKeyTree(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\" + AppId, false); } catch { }
            try { Registry.CurrentUser.DeleteSubKeyTree(RegRoot, false); } catch { }
            try
            {
                var dir = Path.GetDirectoryName(InstalledExe);
                if (!Directory.Exists(dir)) return;
                var self = Assembly.GetExecutingAssembly().Location;
                if (string.Equals(Path.GetDirectoryName(Path.GetFullPath(self)), dir, StringComparison.OrdinalIgnoreCase))
                    // the running copy cannot delete itself: a hidden cmd removes the folder a few seconds after exit
                    Process.Start(new ProcessStartInfo("cmd.exe", "/c ping 127.0.0.1 -n 4 >nul & rd /s /q \"" + dir + "\"") { CreateNoWindow = true, UseShellExecute = false });
                else Directory.Delete(dir, true);
            }
            catch { }
        }
    }
}
