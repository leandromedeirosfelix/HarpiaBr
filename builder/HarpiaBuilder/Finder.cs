using System.IO;

namespace HarpiaBuilder;

/// A .nup found on this PC
sealed record Dump(string Path, string Where, long Size, DateTime Modified);

/// The player's game files where people keep them, and the game's shortcuts.
static class Finder
{
    /// .nup files in Downloads, Desktop and Documents three folder levels down (local: Harpia: "Downloads\jogos\emulador\x.nup"
    /// was missed at one level), and in the home folder and the top of each drive one level down, newest first. Only names and
    /// sizes are read (the version is checked one file at a time afterwards); hidden, system and tool folders (node_modules,
    /// .git) and links are skipped, and each place stops after Budget folders, so a huge tree costs little.
    public static List<Dump> FindDumps()
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var places = new List<(string Dir, string Name, int Depth)>
        {
            (Path.Combine(home, "Downloads"), "Downloads", 3),
            (Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Desktop", 3),
            (Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Documents", 3),
            (home, "Home", 1),
        };
        foreach (var d in DriveInfo.GetDrives())
        {
            try
            {
                if (d.IsReady && d.DriveType is DriveType.Fixed or DriveType.Removable) places.Add((d.RootDirectory.FullName, d.Name.TrimEnd('\\'), 1));
            }
            catch (Exception) { }
        }
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var found = new List<Dump>();
        foreach (var (dir, name, depth) in places)
        {
            if (!Directory.Exists(dir) || !seen.Add(Path.GetFullPath(dir))) continue;
            foreach (var sub in Folders(dir, depth))
            {
                try
                {
                    foreach (var f in Directory.EnumerateFiles(sub, "*.nup"))
                    {
                        var info = new FileInfo(f);
                        if (seen.Add(info.FullName)) found.Add(new Dump(info.FullName, name, info.Length, info.LastWriteTime));
                    }
                }
                catch (Exception) { }
            }
        }
        return found.OrderByDescending(d => d.Modified).ToList();
    }

    const int Budget = 4000;
    static readonly HashSet<string> SkipNames = new(StringComparer.OrdinalIgnoreCase)
        { "Windows", "node_modules", "__pycache__", "AppData", "site-packages", "Library", "obj", "bin" };

    /// The folder and its subfolders, level by level down to depth, at most Budget in all
    public static List<string> Folders(string root, int depth)
    {
        var all = new List<string> { root };
        var level = new List<string> { root };
        for (var d = 0; d < depth && level.Count > 0 && all.Count < Budget; d++)
        {
            var next = new List<string>();
            foreach (var dir in level)
            {
                try
                {
                    foreach (var s in Directory.EnumerateDirectories(dir))
                    {
                        if (all.Count + next.Count >= Budget) break;
                        var n = Path.GetFileName(s);
                        if (n.StartsWith('.') || n.StartsWith('$') || n.StartsWith("Program", StringComparison.OrdinalIgnoreCase) || SkipNames.Contains(n)
                            || (File.GetAttributes(s) & FileAttributes.ReparsePoint) != 0) continue;
                        next.Add(s);
                    }
                }
                catch (Exception) { }
            }
            all.AddRange(next);
            level = next;
        }
        return all;
    }

    public static string Ago(DateTime t)
    {
        var d = DateTime.Now - t;   // (local: Harpia) in the window's language (Ui)
        if (d.TotalDays < 1 && t.Date == DateTime.Today) return Ui.L("modified today");
        if (d.TotalDays < 2) return Ui.L("modified yesterday");
        if (d.TotalDays < 14) return Ui.F("{0} days ago", (int)d.TotalDays);
        if (d.TotalDays < 60) return Ui.F("{0} weeks ago", (int)(d.TotalDays / 7));
        if (d.TotalDays < 365) return Ui.F("{0} months ago", (int)(d.TotalDays / 30));
        return t.ToString("d MMM yyyy", Ui.Pt ? new System.Globalization.CultureInfo("pt-BR") : System.Globalization.CultureInfo.InvariantCulture);
    }

    public static string Size(long bytes) => bytes >= 1L << 30 ? $"{bytes / (double)(1L << 30):0.0} GB" : $"{bytes >> 20} MB";

    // ---- shortcuts to the built game ----

    public static string DesktopLink => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Harpia.lnk");
    public static string StartLink => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "Harpia.lnk");

    /// Made or removed as the two ticks say (Windows' own shell object writes the .lnk)
    /// The shortcuts the player has, made again (onto the folder's launcher now that it has one); none made where there was none
    public static void RefreshShortcuts(string gameFolder) => Shortcuts(gameFolder, File.Exists(DesktopLink), File.Exists(StartLink));

    public static void Shortcuts(string gameFolder, bool desktop, bool start)
    {
        // (hookshot.exe: the game's launcher in a folder built since it came (Play, and Update when a newer release is out), else the
        // game. The icon is the game's own: bin\harpia.exe when it is there)
        var exe = Path.Combine(gameFolder, "harpia.exe");
        var icon = File.Exists(Path.Combine(gameFolder, "bin", "harpia.exe")) ? Path.Combine(gameFolder, "bin", "harpia.exe") : exe;
        foreach (var (link, want) in new[] { (DesktopLink, desktop), (StartLink, start) })
        {
            try
            {
                if (!want)
                {
                    if (File.Exists(link)) File.Delete(link);
                    continue;
                }
                if (!File.Exists(exe)) continue;
                // (the shell's own shortcut object (IShellLinkW + IPersistFile): the scripting host's, late-bound through dynamic,
                // made none for players and said nothing)
                var sl = (IShellLinkW)new ShellLink();
                sl.SetPath(exe);
                sl.SetWorkingDirectory(gameFolder);
                sl.SetIconLocation(icon, 0);
                sl.SetDescription("The Legend of Zelda: Ocarina of Time, built by Harpia");
                Directory.CreateDirectory(Path.GetDirectoryName(link)!);
                ((System.Runtime.InteropServices.ComTypes.IPersistFile)sl).Save(link, true);
            }
            catch (Exception e)
            {
                ShortcutError = $"{Path.GetFileName(link)}: {e.Message}";
            }
        }
    }

    /// The last shortcut that could not be made, for the log
    public static string? ShortcutError;

    [System.Runtime.InteropServices.ComImport, System.Runtime.InteropServices.Guid("00021401-0000-0000-C000-000000000046")]
    class ShellLink { }

    [System.Runtime.InteropServices.ComImport, System.Runtime.InteropServices.Guid("000214F9-0000-0000-C000-000000000046"),
     System.Runtime.InteropServices.InterfaceType(System.Runtime.InteropServices.ComInterfaceType.InterfaceIsIUnknown)]
    interface IShellLinkW
    {
        void GetPath([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder file, int max, IntPtr fd, uint flags);
        void GetIDList(out IntPtr ppidl);
        void SetIDList(IntPtr pidl);
        void GetDescription([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder name, int max);
        void SetDescription([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string name);
        void GetWorkingDirectory([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder dir, int max);
        void SetWorkingDirectory([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string dir);
        void GetArguments([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder args, int max);
        void SetArguments([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string args);
        void GetHotkey(out short hotkey);
        void SetHotkey(short hotkey);
        void GetShowCmd(out int show);
        void SetShowCmd(int show);
        void GetIconLocation([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder path, int max, out int index);
        void SetIconLocation([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string path, int index);
        void SetRelativePath([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string path, uint reserved);
        void Resolve(IntPtr hwnd, uint flags);
        void SetPath([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)] string file);
    }
}
