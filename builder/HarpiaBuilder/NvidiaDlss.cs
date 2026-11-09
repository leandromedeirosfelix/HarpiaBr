using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;

namespace HarpiaBuilder;

/// (local: Harpia) NVIDIA DLSS, optional, from NVIDIA's own package: never in Harpia's repository or release. The player
/// asks for it, the Builder downloads NVIDIA's official Streamline SDK release (the version whose headers the game is
/// compiled against: tools/third_party/streamline, 2.14.16), checks its SHA-256, shows NVIDIA's licences from it, and only
/// after the player accepts them copies the production DLLs the game loads (gpu/vk/vk_dlss.cpp dlss_load) and their licence
/// files into the game folder, each DLL's Authenticode signature checked (valid, NVIDIA Corporation). Remove deletes exactly
/// those files. HARPIA_DLSS_ZIP=<a local copy of the zip>: used instead of the download (tests; it is checked the same way).
static class NvidiaDlss
{
    public const string Version = "2.14.16";
    public const string Url = "https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.16/streamline-sdk-v2.14.16.zip";
    public const long Size = 276016170;
    public const string Sha256 = "32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b";

    /// What the game loads: Streamline's interposer and core, DLSS, DLSS Frame Generation, Reflex and PCL (DLSS-G needs them)
    public static readonly string[] Dlls =
        { "sl.interposer.dll", "sl.common.dll", "sl.dlss.dll", "nvngx_dlss.dll", "sl.dlss_g.dll", "nvngx_dlssg.dll", "sl.reflex.dll", "sl.pcl.dll", "NvLowLatencyVk.dll" };
    /// NVIDIA's licence files that go beside them (the zip's bin/x64)
    public static readonly string[] Licences = { "nvngx_dlss.license.txt", "reflex.license.txt" };
    /// The licences shown before anything is copied: the DLSS one (NVIDIA RTX SDKs licence), Reflex's, Streamline's (MIT)
    static readonly (string Entry, string Title)[] Shown =
        { ("bin/x64/nvngx_dlss.license.txt", "NVIDIA DLSS / RTX SDK"), ("bin/x64/reflex.license.txt", "NVIDIA Reflex"), ("license.txt", "NVIDIA Streamline (MIT)") };

    public static IEnumerable<string> Files => Dlls.Concat(Licences);
    static bool LocalCopy => Environment.GetEnvironmentVariable("HARPIA_DLSS_ZIP") is { Length: > 0 };
    public static string Zip => LocalCopy ? Environment.GetEnvironmentVariable("HARPIA_DLSS_ZIP")!
        : Path.Combine(Storage.Base, "downloads", $"streamline-sdk-v{Version}.zip");

    public static bool Installed(string gameFolder) => Dlls.All(d => File.Exists(Path.Combine(gameFolder, d)));

    /// An NVIDIA RTX card (the PC check's Vulkan device): DLSS SR needs RTX 20 or newer, Frame Generation RTX 40 or newer
    public static bool Supported(string? gpu) => gpu != null && gpu.Contains("NVIDIA", StringComparison.OrdinalIgnoreCase)
                                                  && gpu.Contains("RTX", StringComparison.OrdinalIgnoreCase);

    static string BadHash => Ui.L("The NVIDIA package did not pass the check (its SHA-256 differs). Nothing was installed: the game works without DLSS.");

    /// The zip, checked: downloaded unless it is here already with the right SHA-256. Progress 0..1; cancellable.
    public static async Task Fetch(IProgress<double> progress, CancellationToken token)
    {
        if (File.Exists(Zip) && await Task.Run(() => HashOk(Zip), token)) { progress.Report(1); return; }
        if (LocalCopy) throw new InvalidDataException(BadHash);
        Directory.CreateDirectory(Path.GetDirectoryName(Zip)!);
        var part = Zip + ".part";
        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("HarpiaBuilder");
            using var r = await http.GetAsync(Url, HttpCompletionOption.ResponseHeadersRead, token);
            r.EnsureSuccessStatusCode();
            await using (var src = await r.Content.ReadAsStreamAsync(token))
            await using (var dst = new FileStream(part, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 20))
            {
                var buf = new byte[1 << 20];
                long got = 0;
                for (int n; (n = await src.ReadAsync(buf, token)) > 0;)
                {
                    await dst.WriteAsync(buf.AsMemory(0, n), token);
                    got += n;
                    progress.Report(Math.Min(1.0, (double)got / Size));
                }
            }
            if (!await Task.Run(() => HashOk(part), token)) throw new InvalidDataException(BadHash);
            File.Move(part, Zip, overwrite: true);
        }
        finally
        {
            try { if (File.Exists(part)) File.Delete(part); } catch (Exception) { }
        }
    }

    static bool HashOk(string path)
    {
        using var f = File.OpenRead(path);
        return f.Length == Size && Convert.ToHexString(SHA256.HashData(f)).Equals(Sha256, StringComparison.OrdinalIgnoreCase);
    }

    /// NVIDIA's licences, from the checked zip, for the player to read before accepting
    public static string LicenceText()
    {
        using var z = ZipFile.OpenRead(Zip);
        var sb = new StringBuilder();
        foreach (var (entry, title) in Shown)
            if (z.GetEntry(entry) is { } e)
            {
                using var r = new StreamReader(e.Open());
                sb.Append("==== ").Append(title).Append(" ====\r\n\r\n").Append(r.ReadToEnd().Replace("\r\n", "\n").Replace("\n", "\r\n")).Append("\r\n\r\n");
            }
        return sb.ToString();
    }

    /// Only after the player accepted: the DLLs and licence files into the game folder, each DLL's signature checked; on any
    /// failure every file copied here is deleted again and the game stays as it was (without DLSS)
    public static void Install(string gameFolder)
    {
        if (!HashOk(Zip)) throw new InvalidDataException(BadHash);
        var written = new List<string>();
        try
        {
            using var z = ZipFile.OpenRead(Zip);
            foreach (var name in Files)
            {
                var e = z.GetEntry("bin/x64/" + name) ?? throw new InvalidDataException(Ui.F("{0} is missing from NVIDIA's package.", name));
                var dst = Path.Combine(gameFolder, name);
                e.ExtractToFile(dst + ".new", overwrite: true);
                written.Add(dst + ".new");
                if (name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) && !SignedByNvidia(dst + ".new"))
                    throw new InvalidDataException(Ui.F("{0} is not signed by NVIDIA Corporation. Nothing was installed: the game works without DLSS.", name));
            }
            foreach (var name in Files)
            {
                var dst = Path.Combine(gameFolder, name);
                File.Move(dst + ".new", dst, overwrite: true);
                written.Remove(dst + ".new");
                written.Add(dst);
            }
        }
        catch
        {
            foreach (var f in written) try { File.Delete(f); } catch (Exception) { }
            throw;
        }
    }

    /// Exactly the files Install copies: nothing else in the folder
    public static void Remove(string gameFolder)
    {
        foreach (var name in Files)
        {
            var p = Path.Combine(gameFolder, name);
            if (File.Exists(p)) File.Delete(p);
        }
    }

    // ---- Authenticode: WinVerifyTrust (a valid signature with a trusted chain; revocation not fetched, so it works offline),
    //      then the signer's certificate names NVIDIA Corporation
    public static bool SignedByNvidia(string path)
    {
        if (!Verify(path)) return false;
        try
        {
#pragma warning disable SYSLIB0057
            using var cert = new X509Certificate2(X509Certificate.CreateFromSignedFile(path));
#pragma warning restore SYSLIB0057
            return cert.Subject.Contains("O=NVIDIA Corporation", StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception) { return false; }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct WinFileInfo { public uint Size; public string Path; public IntPtr Handle; public IntPtr KnownSubject; }
    [StructLayout(LayoutKind.Sequential)]
    struct TrustData
    {
        public uint Size; public IntPtr Policy, Sip; public uint UiChoice, RevocationChecks, UnionChoice; public IntPtr File;
        public uint StateAction; public IntPtr StateData, UrlReference; public uint ProvFlags, UiContext; public IntPtr Signature;
    }
    [DllImport("wintrust.dll", CharSet = CharSet.Unicode)]
    static extern int WinVerifyTrust(IntPtr hwnd, ref Guid action, ref TrustData data);

    static bool Verify(string path)
    {
        var action = new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");   // WINTRUST_ACTION_GENERIC_VERIFY_V2
        var info = new WinFileInfo { Size = (uint)Marshal.SizeOf<WinFileInfo>(), Path = path };
        var pInfo = Marshal.AllocHGlobal(Marshal.SizeOf<WinFileInfo>());
        try
        {
            Marshal.StructureToPtr(info, pInfo, false);
            var data = new TrustData
            {
                Size = (uint)Marshal.SizeOf<TrustData>(), UiChoice = 2 /* WTD_UI_NONE */, RevocationChecks = 0 /* WTD_REVOKE_NONE */,
                UnionChoice = 1 /* WTD_CHOICE_FILE */, File = pInfo, StateAction = 0, ProvFlags = 0x1000 /* WTD_CACHE_ONLY_URL_RETRIEVAL */,
            };
            return WinVerifyTrust(IntPtr.Zero, ref action, ref data) == 0;
        }
        finally
        {
            Marshal.DestroyStructure<WinFileInfo>(pInfo);
            Marshal.FreeHGlobal(pInfo);
        }
    }
}
