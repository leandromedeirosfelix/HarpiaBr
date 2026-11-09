using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32;

namespace HarpiaBuilder;

/// (local: Harpia) Verificação do PC / PC check, before a build: CPU (name, cores, a few seconds of integer work on one thread and
/// on all), memory, the work folder's drive (kind, free space, a short read/write test in a temp file deleted after), the GPU's
/// Vulkan 1.3 and mesh shaders (vulkan-1.dll asked directly) and the compiler / Windows SDK (make_game.py --check-pc). Every part
/// is cancellable and needs no admin rights: what Windows will not say is "desconhecido / unknown".
static class PcCheck
{
    public sealed record Item(string Label, string Value, bool? Ok);
    /// Minutes: Automático (RecommendedJobs compilers); MaxMinutes: Máximo (every thread)
    public sealed record Report(List<Item> Items, string Verdict, bool? CanBuild, int RecommendedJobs, double? Minutes, double? MaxMinutes, string? Gpu);

    const long NeededWorkBytes = 60L << 30;
    // shortcut: the estimate is scaled from one PC's measured builds, not a model of the compile; recalibrate with more PCs'
    // times. Measured 2026-11-09 through the Builder from the .nup, nothing cached: an Intel Core Ultra 9 275HX (24 threads,
    // 31 GB, NVMe), ~16 200 points on all threads; Automatico 21.4 min, Maximo 16.2 min (both include ~7 min of reading and
    // translating the game). Scaled by the all-thread score only: Automatico's own memory gate adapts its compilers while it
    // builds, so the compilers recommended at the check time do not predict its time (24/7 would have said 55 min, not 21)
    const double RefMultiScore = 16200, RefAutoMinutes = 21.4, RefMaxMinutes = 16.2;

    public static async Task<Report> Run(string workFolder, Tools? tools, CancellationToken token)
    {
        var items = new List<Item>();
        var (cpuName, cores, gpuName, driver, diskKind, gpus) = await Task.Run(() => Wmi(workFolder), token);
        var threads = Environment.ProcessorCount;
        items.Add(new(Ui.L("Processor"), $"{cpuName} · {(cores > 0 ? cores.ToString() : Ui.L("unknown"))} {Ui.L("cores")} · {threads} threads", threads >= 4));
        var (single, multi) = await Task.Run(() => CpuScore(token), token);
        items.Add(new(Ui.L("CPU speed"), Ui.F("{0:0} single · {1:0} all threads (points)", single, multi), null));

        var (total, free) = Memory();
        items.Add(new(Ui.L("Memory"), Ui.F("{0:0.0} GB total · {1:0.0} GB free", total / 1073741824.0, free / 1073741824.0), total >= 12L << 30));

        var root = Path.GetPathRoot(Path.GetFullPath(workFolder)) ?? workFolder;
        long diskFree = -1;
        try { diskFree = new DriveInfo(root).AvailableFreeSpace; } catch (Exception) { }
        var (write, read, iops) = await Task.Run(() => DiskSpeed(workFolder, token), token);
        items.Add(new(Ui.L("Build drive"), Ui.F("{0} · {1} · {2} GB free (needs 60)", root.TrimEnd('\\'), diskKind, diskFree < 0 ? "?" : (diskFree >> 30).ToString()),
                      diskFree < 0 ? null : diskFree >= NeededWorkBytes));
        items.Add(new(Ui.L("Drive speed"), write > 0 ? Ui.F("write {0:0} MB/s · read {1:0} MB/s · {2:0} random reads/s", write, read, iops) : Ui.L("unknown"),
                      write <= 0 ? null : write >= 100));

        var (vk13, mesh, vkDevice) = Vulkan();
        // (the driver of the card Vulkan picked: WMI's largest AdapterRAM is capped at 4 GB and gave a laptop's Intel driver for its RTX)
        var picked = gpus.FirstOrDefault(g => vkDevice != null && g.Name.Equals(vkDevice, StringComparison.OrdinalIgnoreCase));
        if (picked.Name != null) driver = picked.Driver;
        items.Add(new(Ui.L("Graphics card"), $"{vkDevice ?? gpuName} · {Ui.L("driver")} {driver}", null));
        items.Add(new("Vulkan 1.3 / mesh shaders", vk13 == null ? Ui.L("unknown") : $"{(vk13 == true ? "✓" : "✗")} Vulkan 1.3 · {(mesh == true ? "✓" : "✗")} mesh shaders",
                      vk13 == null ? null : vk13 == true && mesh == true));

        (bool Ok, string Text) sdk = tools == null ? (false, Ui.L("The build tools are not in Harpia's folder.")) : await Runner.Check(tools, new[] { "--check-pc" });
        items.Add(new(Ui.L("Compiler and Windows SDK"), sdk.Ok ? Ui.L("found") : sdk.Text, sdk.Ok));

        // (as nx2/build.py's memory gate: a compiler needs about 1.5 GB, with 1.5 GB kept free)
        var jobs = Math.Clamp((int)((free / 1048576.0 - 1536) / 1500), 1, threads);
        double? maxMinutes = RefMultiScore > 0 && multi > 0 ? RefMaxMinutes * RefMultiScore / multi : null;
        double? minutes = RefMultiScore > 0 && multi > 0 ? RefAutoMinutes * RefMultiScore / multi : null;
        bool? can = items.Any(i => i.Ok == false && (i.Label == "Vulkan 1.3 / mesh shaders" || i.Label == Ui.L("Compiler and Windows SDK")
                                                     || i.Label == Ui.L("Build drive") || i.Label == Ui.L("Memory"))) ? false
                  : items.Any(i => i.Ok == false) || diskKind.Contains("HDD") || jobs < 4 ? null : true;
        var verdict = can == false ? Ui.L("Won't work on this PC: fix the item marked ✗ first.")
                    : can == null ? Ui.L("Will compile, but slowly.")
                    : Ui.L("Compiles fine on this PC.");
        if (minutes is double m && maxMinutes is double mx) verdict += " " + Ui.F("Automatic: about {0:0} min · Maximum: about {1:0} min.", m, mx);
        return new(items, verdict, can, jobs, minutes, maxMinutes, vkDevice ?? gpuName);
    }

    // ---- CPU: integer work for ~1.5 s on one thread, then on every thread; points = millions of steps a second
    static (double Single, double Multi) CpuScore(CancellationToken token)
    {
        static long Work(Stopwatch w, CancellationToken t)
        {
            ulong x = 0x9E3779B97F4A7C15;
            long n = 0;
            while (w.ElapsedMilliseconds < 1500 && !t.IsCancellationRequested)
            {
                for (var i = 0; i < 100_000; i++) x = (x ^ (x >> 31)) * 0xBF58476D1CE4E5B9 + (ulong)i;
                n += 100_000;
            }
            return x == 42 ? n + 1 : n;   // (x used, so the loop is not removed)
        }
        var s = Stopwatch.StartNew();
        var one = Work(s, token) / s.Elapsed.TotalSeconds / 1e6;
        token.ThrowIfCancellationRequested();
        var all = 0L;
        var sw = Stopwatch.StartNew();
        Parallel.For(0, Environment.ProcessorCount, new ParallelOptions { CancellationToken = token }, _ => Interlocked.Add(ref all, Work(sw, token)));
        return (one, all / sw.Elapsed.TotalSeconds / 1e6);
    }

    // ---- memory
    [StructLayout(LayoutKind.Sequential)]
    struct MemoryStatusEx
    {
        public uint Length, Load;
        public ulong TotalPhys, AvailPhys, TotalPageFile, AvailPageFile, TotalVirtual, AvailVirtual, AvailExtendedVirtual;
    }
    [DllImport("kernel32.dll")] static extern bool GlobalMemoryStatusEx(ref MemoryStatusEx m);
    public static (long Total, long Free) Memory()
    {
        var m = new MemoryStatusEx { Length = (uint)Marshal.SizeOf<MemoryStatusEx>() };
        return GlobalMemoryStatusEx(ref m) ? ((long)m.TotalPhys, (long)m.AvailPhys) : (0, 0);
    }

    // ---- the work folder's drive: 256 MB written through to the disk and read back, then 2000 random 4 KB reads, in a temp file
    static (double Write, double Read, double Iops) DiskSpeed(string folder, CancellationToken token)
    {
        string? path = null;
        try
        {
            Directory.CreateDirectory(folder);
            path = Path.Combine(folder, $"harpia-pc-check-{Guid.NewGuid():N}.tmp");
            var block = new byte[4 << 20];
            new Random(1).NextBytes(block);
            const int Blocks = 64;
            var w = Stopwatch.StartNew();
            using (var f = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None, 1 << 20, FileOptions.WriteThrough))
                for (var i = 0; i < Blocks; i++) { token.ThrowIfCancellationRequested(); f.Write(block); }
            var write = Blocks * 4.0 / w.Elapsed.TotalSeconds;
            var r = Stopwatch.StartNew();
            using (var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None, 1 << 20, FileOptions.SequentialScan))
                while (f.Read(block) > 0) token.ThrowIfCancellationRequested();
            var read = Blocks * 4.0 / r.Elapsed.TotalSeconds;
            var rnd = new Random(2);
            var small = new byte[4096];
            var q = Stopwatch.StartNew();
            using (var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None, 4096, FileOptions.RandomAccess))
                for (var i = 0; i < 2000; i++)
                {
                    token.ThrowIfCancellationRequested();
                    f.Position = (long)rnd.Next(0, Blocks * 1024) * 4096;
                    f.ReadExactly(small);
                }
            return (write, read, 2000 / q.Elapsed.TotalSeconds);
        }
        catch (OperationCanceledException) { throw; }
        catch (Exception) { return (0, 0, 0); }
        finally
        {
            try { if (path != null) File.Delete(path); } catch (Exception) { }
        }
    }

    // ---- CPU name and cores, the GPU's name and driver, the drive's kind: one PowerShell query, nothing that needs admin
    static (string Cpu, int Cores, string Gpu, string Driver, string Disk, List<(string Name, string Driver)> Gpus) Wmi(string folder)
    {
        var cpu = Registry.GetValue(@"HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0", "ProcessorNameString", null) as string;
        var letter = (Path.GetPathRoot(Path.GetFullPath(folder)) ?? "C").Substring(0, 1);
        var gpus = new List<(string, string)>();
        var script = "$c=(Get-CimInstance Win32_Processor|Select -First 1).NumberOfCores;" +
                     "$all=@(Get-CimInstance Win32_VideoController);$g=$all|Sort-Object AdapterRAM -Descending|Select -First 1;" +
                     $"$d=$null;try{{$n=(Get-Partition -DriveLetter {letter} -ErrorAction Stop|Get-Disk).Number;" +
                     "$d=Get-PhysicalDisk|Where-Object DeviceId -eq \"$n\"|Select -First 1 MediaType,BusType}catch{};" +
                     "@{cores=$c;gpu=$g.Name;driver=$g.DriverVersion;media=\"$($d.MediaType)\";bus=\"$($d.BusType)\";" +
                     "gpus=@($all|ForEach-Object{@{name=$_.Name;driver=$_.DriverVersion}})}|ConvertTo-Json -Compress -Depth 3";
        try
        {
            var si = new ProcessStartInfo("powershell.exe", $"-NoProfile -NonInteractive -Command \"{script.Replace("\"", "\\\"")}\"")
            { RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true };
            using var p = Process.Start(si)!;
            var json = p.StandardOutput.ReadToEnd();
            p.WaitForExit(20000);
            using var doc = JsonDocument.Parse(json);
            var e = doc.RootElement;
            string S(string k) => e.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
            var cores = e.TryGetProperty("cores", out var c) && c.ValueKind == JsonValueKind.Number ? c.GetInt32() : 0;
            var media = S("media");
            var bus = S("bus");
            var kind = bus == "NVMe" ? "NVMe SSD" : media == "SSD" ? (bus == "SATA" ? "SATA SSD" : "SSD") : media == "HDD" ? "HDD"
                     : media.Length > 0 || bus.Length > 0 ? $"{media} {bus}".Trim() : Ui.L("unknown");
            if (e.TryGetProperty("gpus", out var list) && list.ValueKind == JsonValueKind.Array)
                foreach (var g in list.EnumerateArray())
                    gpus.Add((g.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "", g.TryGetProperty("driver", out var v) ? v.GetString() ?? "" : ""));
            return (cpu?.Trim() ?? Ui.L("unknown"), cores, S("gpu"), S("driver"), kind, gpus);
        }
        catch (Exception)
        {
            return (cpu?.Trim() ?? Ui.L("unknown"), 0, Ui.L("unknown"), Ui.L("unknown"), Ui.L("unknown"), gpus);
        }
    }

    // ---- Vulkan: the device the game would pick (a discrete one first), its API version and VK_EXT_mesh_shader
    [StructLayout(LayoutKind.Sequential)]
    struct AppInfo { public int sType; public IntPtr pNext, pName; public uint ver; public IntPtr pEngine; public uint engineVer, apiVersion; }
    [StructLayout(LayoutKind.Sequential)]
    struct InstanceInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr pApp; public uint layers; public IntPtr ppLayers; public uint exts; public IntPtr ppExts; }
    [DllImport("vulkan-1.dll")] static extern int vkCreateInstance(ref InstanceInfo info, IntPtr alloc, out IntPtr instance);
    [DllImport("vulkan-1.dll")] static extern void vkDestroyInstance(IntPtr instance, IntPtr alloc);
    [DllImport("vulkan-1.dll")] static extern int vkEnumeratePhysicalDevices(IntPtr instance, ref uint count, [Out] IntPtr[]? devices);
    [DllImport("vulkan-1.dll")] static extern void vkGetPhysicalDeviceProperties(IntPtr device, [Out] byte[] properties);
    [DllImport("vulkan-1.dll")] static extern int vkEnumerateDeviceExtensionProperties(IntPtr device, IntPtr layer, ref uint count, [Out] byte[]? properties);

    static (bool? Vk13, bool? Mesh, string? Device) Vulkan()
    {
        var instance = IntPtr.Zero;
        var app = new AppInfo { sType = 0, apiVersion = (1u << 22) | (3u << 12) };
        var appPtr = Marshal.AllocHGlobal(Marshal.SizeOf<AppInfo>());
        try
        {
            Marshal.StructureToPtr(app, appPtr, false);
            var info = new InstanceInfo { sType = 1, pApp = appPtr };
            if (vkCreateInstance(ref info, IntPtr.Zero, out instance) != 0) return (false, false, null);
            uint n = 0;
            vkEnumeratePhysicalDevices(instance, ref n, null);
            if (n == 0) return (false, false, null);
            var devices = new IntPtr[n];
            vkEnumeratePhysicalDevices(instance, ref n, devices);
            (IntPtr Dev, uint Api, string Name) best = default;
            foreach (var d in devices)
            {
                var props = new byte[2048];
                vkGetPhysicalDeviceProperties(d, props);
                var api = BitConverter.ToUInt32(props, 0);
                var type = BitConverter.ToInt32(props, 16);   // (2: discrete)
                var name = System.Text.Encoding.UTF8.GetString(props, 20, 256).TrimEnd('\0');
                if (best.Dev == IntPtr.Zero || type == 2) best = (d, api, name);
            }
            uint e = 0;
            vkEnumerateDeviceExtensionProperties(best.Dev, IntPtr.Zero, ref e, null);
            var ext = new byte[e * 260];
            vkEnumerateDeviceExtensionProperties(best.Dev, IntPtr.Zero, ref e, ext);
            var mesh = false;
            for (var i = 0; i < e; i++)
                if (System.Text.Encoding.ASCII.GetString(ext, i * 260, 256).TrimEnd('\0') == "VK_EXT_mesh_shader") mesh = true;
            var vk13 = (best.Api >> 22) > 1 || ((best.Api >> 22) == 1 && ((best.Api >> 12) & 0x3FF) >= 3);
            return (vk13, mesh, best.Name);
        }
        catch (Exception) { return (null, null, null); }   // (no vulkan-1.dll: no Vulkan driver at all, or it would not load)
        finally
        {
            if (instance != IntPtr.Zero) vkDestroyInstance(instance, IntPtr.Zero);
            Marshal.FreeHGlobal(appPtr);
        }
    }
}
