using System.Diagnostics;
using System.IO;
using Path = System.Windows.Shapes.Path;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Shapes;
using System.Windows.Threading;
using Microsoft.Win32;

namespace HarpiaBuilder;

/// The installer: one 960x600 window. It opens with the harpia in the middle while it looks for the game files, then walks
/// through Game files > Build > Play. The game's update offer (Updater.cs) opens the same window on its update screen.
public partial class MainWindow : Window
{
    const long NeededBytes = 60L << 30;
    Language lang = Strings.Default();   // (the game's language: --language)
    static Language UiLang => Ui.Pt ? Strings.Default() : Strings.English;   // (the window's: the PT | EN switch)
    string T(S s) => (int)s < UiLang.Text.Length ? UiLang.Text[(int)s] : Strings.All[0].Text[(int)s];

    readonly DispatcherTimer clock = new() { Interval = TimeSpan.FromSeconds(1) };
    readonly Stopwatch elapsed = new();
    readonly Stopwatch compileClock = new();
    readonly Dictionary<string, StepRow> rows = new();
    readonly Dictionary<string, (bool Ok, string Text)?> checks = new(StringComparer.OrdinalIgnoreCase);
    readonly List<Dump> dumps = new();
    readonly System.Text.StringBuilder log = new();
    Runner? runner;
    string? current;
    string finished = "";
    string? chosenNup, romfs, exefs;
    bool updating, checking;
    double compiled;
    int filesDone, filesOf;
    int filesChanged = -1, filesCompiled;
    double compileEta = -1;
    long thisCompile, previousTotal = -1;
    string lastFailure = "";

    public MainWindow()
    {
        InitializeComponent();
        OutPath.Text = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Games", "Harpia");
        LanguageBox.ItemsSource = Strings.All;
        LanguageBox.SelectedItem = lang;
        WorkerBox.ItemsSource = BuildWorkers.Choices(Environment.ProcessorCount);
        WorkerBox.SelectedIndex = 0;
        clock.Tick += (_, _) => Tick();
        Drop += Dropped;
        SourceInitialized += (_, _) =>
        {
            if (!Glass.Apply(this))   // (Windows 10, or no acrylic: plain Slate)
            {
                Background = (Brush)FindResource("Page");
                Root.Background = (Brush)FindResource("Page");
            }
        };
        Loaded += (_, _) => Start();
        SetStep(0);
        RefreshUiTexts();
    }

    // ================= the start =================

    async void Start()
    {
        // (development) HARPIA_TEST_SHORTCUTS=<game folder>: both shortcuts made for it, the result in the log beside the app, and out
        if (Environment.GetEnvironmentVariable("HARPIA_TEST_SHORTCUTS") is { Length: > 0 } testFolder)
        {
            Finder.Shortcuts(testFolder, true, true);
            File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory, "shortcut-test.txt"),
                              $"desktop {File.Exists(Finder.DesktopLink)} start {File.Exists(Finder.StartLink)} error {Finder.ShortcutError}");
            Application.Current.Shutdown();
            return;
        }
        Animate();
        var started = Stopwatch.StartNew();
        if (Environment.GetCommandLineArgs().Contains("--update"))
        {
            SplashText.Text = Ui.L("Getting the update…");
            await Task.Delay(900);
            ShowMain();
            Updater.FromCommandLine(this);
            return;
        }
        if (LaunchArg() is { } launchFolder)   // (Harpia Launcher.exe in the game folder)
        {
            SplashText.Text = Ui.L("Starting Harpia…");
            await Task.Delay(700);
            ShowMain();
            ShowLauncher(launchFolder);
            return;
        }
        SplashText.Text = Ui.L("Looking for your game files…");
        var found = await Task.Run(Finder.FindDumps);
        dumps.AddRange(found);
        SplashText.Text = found.Count switch { 0 => Ui.L("No game files found yet"), 1 => Ui.L("Found 1 game file"), _ => Ui.F("Found {0} game files", found.Count) };
        var wait = 1700 - (int)started.ElapsedMilliseconds;
        if (wait > 0) await Task.Delay(wait);
        ApplySettings();
        FillDumps();
        ShowMain();
        if (Environment.GetEnvironmentVariable("HARPIA_UI_PREVIEW") is { Length: > 0 } preview) { Preview(preview); return; }
        CheckAll();
        latestTag = Environment.GetEnvironmentVariable("HARPIA_LATEST_TAG") is { Length: > 0 } fake ? (fake == "none" ? null : fake) : await Updater.LatestTag();
        RefreshMode();
    }

    // ================= what the button does: build, update the game, play it, or get the newer builder =================
    // (as NX2navi's launcher: whatever version the player opens, it checks the latest release and says what is there)

    enum Mode { Build, UpdateGame, UpToDate, UpdateBuilder }
    Mode mode = Mode.Build;
    string? latestTag, updateTarget;
    bool sourcePicked;   // (the player chose game files here: a build from them, even over a game that is up to date)

    /// The release a game folder was built with (its harpia_version.txt), when it keeps its game files for updates
    static string? GameVersion(string folder)
    {
        try
        {
            if (!Directory.Exists(System.IO.Path.Combine(folder, ".harpia_exefs")) || !Directory.Exists(System.IO.Path.Combine(folder, "romfs")))
                return null;
            foreach (var line in File.ReadAllLines(System.IO.Path.Combine(folder, "harpia_version.txt")))
                if (line.StartsWith("version=")) return line[8..].Trim() is { Length: > 0 } v ? v : null;
        }
        catch (Exception) { }
        return null;
    }

    void RefreshMode()
    {
        if (BuildButton == null || updating || runner != null) return;
        var game = OutPath.Text.Length > 2 ? GameVersion(OutPath.Text) : null;
        var builder = Tools.Find()?.Version;
        // (the newest release known: GitHub's, else this builder's own. v2.0.0 took a GitHub that did not answer for "up to date")
        updateTarget = latestTag ?? builder;
        if (game != null && updateTarget != null && game != updateTarget && !sourcePicked) mode = Mode.UpdateGame;
        else if (game != null && !sourcePicked) mode = Mode.UpToDate;
        else if (latestTag != null && builder != null && builder != latestTag) mode = Mode.UpdateBuilder;
        else mode = Mode.Build;
        switch (mode)
        {
            case Mode.UpdateGame:
                FilesTitle.Text = Ui.L("Update available");
                FilesSub.Text = Ui.F("Harpia {0} is available. Your game is {1}: Update builds it again from the game files kept in its folder, compiling only what changed.", updateTarget, game);
                BuildButton.Content = Ui.L("Update");
                break;
            case Mode.UpToDate:
                FilesTitle.Text = Ui.L("Your game is up to date");
                FilesSub.Text = latestTag == null
                    ? (Updater.Enabled ? Ui.F("Your game is {0}, as this builder. GitHub did not answer, so newer versions could not be checked.", game)
                                       : Ui.F("Your game is {0}, the same version as this builder.", game))   // (local: Harpia: no update check)
                    : Ui.F("{0} is the latest version. Pick game files below only to build it again from scratch.", game);
                BuildButton.Content = Ui.L("Play");
                break;
            case Mode.UpdateBuilder:
                FilesTitle.Text = Ui.L("Build your game");
                FilesSub.Text = Ui.F("Harpia Builder {0} is available (this one is {1}): update it first, then build.", latestTag, builder);
                BuildButton.Content = Ui.L("Update builder");
                break;
            default:
                // (local: Harpia) a folder that already holds a game (one built before harpia_version.txt, or by hand): said, and the
                // button names it; the build swaps only the game's program and image.bin (nx2/make_game.py), everything else stays
                var existing = OutPath.Text.Length > 2 && (File.Exists(System.IO.Path.Combine(OutPath.Text, "harpia.exe")) ||
                                                           File.Exists(System.IO.Path.Combine(OutPath.Text, "image.bin")));
                FilesTitle.Text = Ui.L("Build your game");
                FilesSub.Text = existing
                    ? Ui.L("This folder already holds a Harpia game: only the game's program (harpia.exe and its libraries) and image.bin are replaced. Your settings, saves, shader caches and NVIDIA's DLLs stay.")
                    : Ui.L("Pick your game files and where the game goes. Harpia does the rest, once.");
                BuildButton.Content = existing ? Ui.L("Update the game in this folder") : Ui.L("Build the game");
                break;
        }
        BuildButton.IsEnabled = mode != Mode.Build || (SourceReady && OutPath.Text.Length > 2);
        AboutText.Visibility = mode is Mode.Build ? Visibility.Visible : Visibility.Collapsed;
        RefreshDlss();
    }

    // ---- the launcher: Harpia Launcher.exe in a game folder starts this with --launch <folder> (builder/setup/setup.c) ----
    string launchFolder = "";

    static string? LaunchArg()
    {
        var a = Environment.GetCommandLineArgs();
        var i = Array.IndexOf(a, "--launch");
        return i >= 0 && i + 1 < a.Length ? a[i + 1].TrimEnd('\\', '/') : null;
    }

    async void ShowLauncher(string folder)
    {
        launchFolder = folder;
        OutPath.Text = folder;
        StepPill.Visibility = Visibility.Collapsed;
        var game = GameVersion(folder) ?? ReadVersion(folder);
        LaunchVersion.Text = (game != null ? $"Harpia {game}" : "Harpia") + " · " + folder;
        LaunchPlay.IsEnabled = File.Exists(GameExe(folder));
        ShowPage(LaunchPage);
        latestTag = Environment.GetEnvironmentVariable("HARPIA_LATEST_TAG") is { Length: > 0 } fake ? (fake == "none" ? null : fake) : await Updater.LatestTag();
        updateTarget = latestTag ?? Tools.Find()?.Version;   // (GitHub's newest, else this builder's own)
        if (updateTarget == null) LaunchStatus.Text = Updater.Enabled ? Ui.L("Could not check for updates (GitHub did not answer)")
                                                                       : Ui.L("Automatic updates are off");
        else if (game != null && game != updateTarget && GameVersion(folder) != null)
        {
            LaunchStatus.Text = Ui.F("Update available: {0}", updateTarget);
            LaunchStatus.Foreground = new SolidColorBrush(Color.FromRgb(0xB9, 0xC8, 0xFF));
            LaunchStatus.FontWeight = FontWeights.SemiBold;
            LaunchUpdate.Visibility = Visibility.Visible;
        }
        else if (game != null && game != updateTarget)
            LaunchStatus.Text = Ui.F("{0} is out: build this folder again with the Harpia Builder to get it", updateTarget);
        else LaunchStatus.Text = latestTag == null ? Ui.L("Up to date with this builder (GitHub did not answer)") : Ui.L("Up to date");
    }

    /// (a folder built before it kept its game files has a version too: only shown)
    static string? ReadVersion(string folder)
    {
        try
        {
            foreach (var line in File.ReadAllLines(System.IO.Path.Combine(folder, "harpia_version.txt")))
                if (line.StartsWith("version=")) return line[8..].Trim() is { Length: > 0 } v ? v : null;
        }
        catch (Exception) { }
        return null;
    }

    /// The game in a folder: bin\harpia.exe below its launcher (hookshot.exe), or harpia.exe in a folder built before the launcher
    static string GameExe(string folder)
    {
        var game = System.IO.Path.Combine(folder, "bin", "harpia.exe");
        return File.Exists(game) ? game : System.IO.Path.Combine(folder, "harpia.exe");
    }

    /// (the launcher already checked for updates: the game does not ask again)
    static void StartGame(string folder)
    {
        var si = new ProcessStartInfo(GameExe(folder)) { UseShellExecute = false, WorkingDirectory = folder };
        si.Environment["HARPIA_NO_UPDATE_CHECK"] = "1";
        Process.Start(si);
    }

    void LaunchUpdateClick(object sender, RoutedEventArgs e) => Updater.UpdateFolder(this, launchFolder, updateTarget, fetched: false);

    void LaunchPlayClick(object sender, RoutedEventArgs e)
    {
        StartGame(launchFolder);
        Application.Current.Shutdown();
    }

    void LaunchFolderClick(object sender, RoutedEventArgs e) =>
        Process.Start(new ProcessStartInfo(launchFolder) { UseShellExecute = true });

    // ---- what the player chose last time (the install folder, the game files): builder-settings.json beside the versions ----
    sealed class BuilderSettings
    {
        public string? Out { get; set; }
        public string? Nup { get; set; }
        public string? Romfs { get; set; }
        public string? Exefs { get; set; }
        public string? WindowLanguage { get; set; }   // (local: Harpia) "pt" or "en": the PT | EN switch
    }
    static string SettingsPath => System.IO.Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HarpiaBuilder", "builder-settings.json");

    void ApplySettings()
    {
        BuilderSettings? s = null;
        try { s = System.Text.Json.JsonSerializer.Deserialize<BuilderSettings>(File.ReadAllText(SettingsPath)); } catch (Exception) { }
        if (s == null) return;
        if (s.WindowLanguage == "en" && Ui.Pt) { Ui.Use(Strings.English); RefreshUiTexts(); }
        if (!string.IsNullOrEmpty(s.Out)) OutPath.Text = s.Out;
        if (s.Romfs != null && s.Exefs != null && Directory.Exists(s.Romfs) && File.Exists(System.IO.Path.Combine(s.Exefs, "main")))
        {
            romfs = s.Romfs;
            exefs = s.Exefs;
            DumpList.Children.Insert(0, new TextBlock { Text = Ui.F("Using the folders {0} and {1} (as last time)", romfs, exefs), Style = (Style)FindResource("Note"),
                                                        Margin = new Thickness(16, 12, 16, 12) });
        }
        else if (s.Nup != null && File.Exists(s.Nup))
        {
            if (!dumps.Any(d => string.Equals(d.Path, s.Nup, StringComparison.OrdinalIgnoreCase)))
            {
                var info = new FileInfo(s.Nup);
                dumps.Insert(0, new Dump(info.FullName, Ui.L("Last time"), info.Length, info.LastWriteTime));
            }
            chosenNup = s.Nup;
        }
    }

    void SaveSettings()
    {
        try
        {
            var s = new BuilderSettings { Out = OutPath.Text, Nup = romfs == null ? chosenNup : null, Romfs = romfs, Exefs = exefs,
                                          WindowLanguage = Ui.Pt ? "pt" : "en" };
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(SettingsPath)!);
            File.WriteAllText(SettingsPath, System.Text.Json.JsonSerializer.Serialize(s));
        }
        catch (Exception) { }
    }

    /// (development) HARPIA_UI_PREVIEW=build|ready|problem|update: one screen with made-up progress, nothing run
    void Preview(string what)
    {
        switch (what)
        {
            case "build":
                MakeRows(update: false);
                SetStep(1);
                ShowPage(BuildPage);
                rows["read"].Begin(); rows["read"].Done(); rows["read"].Small("Version checked");
                rows["lift"].Begin(); rows["lift"].Done();
                rows["compile"].Begin(); rows["compile"].Small("31 of 58 files");
                SetProgress(0.54);
                BuildNote.Text = "About 19 minutes left · your PC stays usable";
                break;
            case "ready":
                finished = OutPath.Text;
                ShowReady();
                break;
            case "problem":
                ShowProblem("Files of the builder's own compiler are missing (clang.exe in tools\\llvm-mingw\\bin).", tools: true);
                break;
            case "update":
                BeginUpdate(OutPath.Text, "honest-eta");
                rows["download"].Done(); rows["download"].Small("3 MB · build tools unchanged");
                rows["read"].Begin(); rows["read"].Done();
                rows["lift"].Begin(); rows["lift"].Done();
                rows["compile"].Begin(); rows["compile"].Small("7 of 516 files");
                SetProgress(0.62);
                BuildNote.Text = "About 3 minutes left. Your current game stays as it is until the new one is complete.";
                break;
        }
    }

    void Animate()
    {
        var forever = RepeatBehavior.Forever;
        var ease = new SineEase { EasingMode = EasingMode.EaseInOut };
        HaloScale.BeginAnimation(ScaleTransform.ScaleXProperty, new DoubleAnimation(0.94, 1.04, TimeSpan.FromSeconds(1.2)) { AutoReverse = true, RepeatBehavior = forever, EasingFunction = ease });
        HaloScale.BeginAnimation(ScaleTransform.ScaleYProperty, new DoubleAnimation(0.94, 1.04, TimeSpan.FromSeconds(1.2)) { AutoReverse = true, RepeatBehavior = forever, EasingFunction = ease });
        Halo.BeginAnimation(OpacityProperty, new DoubleAnimation(0.6, 1, TimeSpan.FromSeconds(1.2)) { AutoReverse = true, RepeatBehavior = forever, EasingFunction = ease });
        BobLift.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(0, -6, TimeSpan.FromSeconds(1.2)) { AutoReverse = true, RepeatBehavior = forever, EasingFunction = ease });
        BobTurn.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(-4, 2, TimeSpan.FromSeconds(1.2)) { AutoReverse = true, RepeatBehavior = forever, EasingFunction = ease });
        Sweep.BeginAnimation(TranslateTransform.XProperty, new DoubleAnimation(-88, 208, TimeSpan.FromSeconds(1.2)) { RepeatBehavior = forever, EasingFunction = ease });
    }

    /// The harpia fades out and the first step fades in, in the same window
    // ================= (local: Harpia) the PC check (PcCheck.cs), on the files page, before a build =================

    CancellationTokenSource? pcCheck;

    async Task RunPcCheck()
    {
        pcCheck?.Cancel();
        var cts = pcCheck = new CancellationTokenSource();
        PcCheckButton.IsEnabled = false;
        PcCheckText.Text = Ui.L("Checking this PC…");
        try
        {
            var r = await PcCheck.Run(Storage.Work, Tools.Find(), cts.Token);
            var summary = r.Verdict + " " + (r.RecommendedJobs == 1 ? Ui.L("Recommended speed: 1 compiler") : Ui.F("Recommended speed: {0} compilers", r.RecommendedJobs));
            var bad = r.Items.Where(i => i.Ok == false).Select(i => "✗ " + i.Label).ToList();
            pcCheckLines = r.Items.Select(i => $"{(i.Ok == true ? "✓" : i.Ok == false ? "✗" : "·")} {i.Label}: {i.Value}").Append("").Append(summary).ToList();
            PcCheckText.Text = bad.Count > 0 ? string.Join(" · ", bad) + ". " + summary : summary;
            PcCheckDetails.Visibility = Visibility.Visible;
            PcCheckText.Foreground = (Brush)FindResource(r.CanBuild == false ? "Bad" : "InkSoft");
            pcReport = r;
            ShowEstimate();
            RefreshDlss();
        }
        catch (OperationCanceledException) { }
        finally
        {
            if (pcCheck == cts) PcCheckButton.IsEnabled = true;
        }
    }

    void PcCheckAgain(object sender, RoutedEventArgs e) => _ = RunPcCheck();

    List<string> pcCheckLines = new();
    void PcCheckShowDetails(object sender, RoutedEventArgs e)
    {
        PcCheckFull.Text = string.Join("\n", pcCheckLines);
        PcCheckPopup.IsOpen = !PcCheckPopup.IsOpen;
    }

    void ShowMain()
    {
        _ = RunPcCheck();
        var fade = new DoubleAnimation(0, TimeSpan.FromMilliseconds(450));
        fade.Completed += (_, _) => { Splash.Visibility = Visibility.Collapsed; StopSplash(); };
        Splash.BeginAnimation(OpacityProperty, fade);
        SplashScale.BeginAnimation(ScaleTransform.ScaleXProperty, new DoubleAnimation(1.04, TimeSpan.FromMilliseconds(500)));
        SplashScale.BeginAnimation(ScaleTransform.ScaleYProperty, new DoubleAnimation(1.04, TimeSpan.FromMilliseconds(500)));
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
        Main.BeginAnimation(OpacityProperty, new DoubleAnimation(1, TimeSpan.FromMilliseconds(500)) { BeginTime = TimeSpan.FromMilliseconds(150) });
        MainLift.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(0, TimeSpan.FromMilliseconds(600)) { BeginTime = TimeSpan.FromMilliseconds(150), EasingFunction = ease });
        Main.IsHitTestVisible = true;
    }

    void StopSplash()
    {
        foreach (var (o, p) in new (Animatable, DependencyProperty)[] { (HaloScale, ScaleTransform.ScaleXProperty), (HaloScale, ScaleTransform.ScaleYProperty),
                     (BobLift, TranslateTransform.YProperty), (BobTurn, RotateTransform.AngleProperty), (Sweep, TranslateTransform.XProperty) })
            o.BeginAnimation(p, null);
        Halo.BeginAnimation(OpacityProperty, null);
    }

    // ================= the step pill: Game files > Build > Play =================

    void SetStep(int on)
    {
        StepItems.Children.Clear();
        var names = new[] { Ui.L("Game files"), Ui.L("Build"), Ui.L("Play") };
        for (var i = 0; i < names.Length; i++)
        {
            if (i > 0)
                StepItems.Children.Add(new Path { Data = Geometry.Parse("M0,0 L4,4 L0,8"), Stroke = (Brush)FindResource("InkSoft"), StrokeThickness = 1.6,
                                                  Opacity = 0.45, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(5, 0, 5, 0) });
            var done = i < on;
            var text = new TextBlock { Text = names[i], FontSize = 13, VerticalAlignment = VerticalAlignment.Center,
                                       FontWeight = i == on ? FontWeights.SemiBold : FontWeights.Medium,
                                       Foreground = (Brush)FindResource(i == on ? "Ink" : done ? "InkSoft" : "InkFaint") };
            var inner = new StackPanel { Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center };
            if (done)
            {
                var tick = new Grid { Width = 22, Height = 22, Margin = new Thickness(0, 0, 8, 0) };
                tick.Children.Add(new Ellipse { Fill = (Brush)FindResource("GoSoft") });
                tick.Children.Add(new Path { Data = Geometry.Parse("M0,4 L3.5,7.5 L10,0"), Stroke = (Brush)FindResource("Go"), StrokeThickness = 1.8,
                                             HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center });
                inner.Children.Add(tick);
            }
            inner.Children.Add(text);
            var pill = new Border { Height = 30, CornerRadius = new CornerRadius(15), Padding = done ? new Thickness(4, 0, 14, 0) : new Thickness(14, 0, 14, 0), Child = inner };
            if (i == on)
            {
                text.Foreground = (Brush)FindResource("BrandOn");   // (local: Harpia) the selected step in gold, dark ink on it
                pill.Background = new LinearGradientBrush(Color.FromRgb(0xF6, 0xCC, 0x78), Color.FromRgb(0xD9, 0xA8, 0x48), 90);
                pill.Effect = new DropShadowEffect { Color = Color.FromRgb(0xE8, 0xB8, 0x60), BlurRadius = 18, ShadowDepth = 6, Direction = 270, Opacity = 0.35 };
            }
            StepItems.Children.Add(pill);
        }
        StepPill.Visibility = Visibility.Visible;
        VersionChip.Visibility = Visibility.Collapsed;
    }

    void ShowPage(FrameworkElement page)
    {
        foreach (var p in new FrameworkElement[] { FilesPage, BuildPage, ReadyPage, ProblemPage, LaunchPage, DlssPage })
            p.Visibility = p == page ? Visibility.Visible : Visibility.Collapsed;
        page.Opacity = 0;
        page.BeginAnimation(OpacityProperty, new DoubleAnimation(1, TimeSpan.FromMilliseconds(250)));
    }

    // ================= 1: game files =================

    void FillDumps()
    {
        DumpList.Children.Clear();
        if (dumps.Count == 0)
        {
            DumpList.Children.Add(new TextBlock
            {
                Text = Ui.L("No .nup files in Downloads, Desktop, Documents or the top folders of your drives. Browse to yours, or drop it on this window."),
                Style = (Style)FindResource("Note"), Margin = new Thickness(16, 14, 16, 14),
            });
            return;
        }
        for (var i = 0; i < dumps.Count; i++)
        {
            var d = dumps[i];
            if (i > 0) DumpList.Children.Add(new Border { Height = 1, Background = (Brush)FindResource("GlassLine") });
            var name = new TextBlock { Text = System.IO.Path.GetFileName(d.Path), FontWeight = FontWeights.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis };
            var meta = new TextBlock { Text = $"{Ui.L(d.Where)} · {Finder.Size(d.Size)} · {Finder.Ago(d.Modified)}", Style = (Style)FindResource("Faint") };
            var texts = new StackPanel { Children = { name, meta }, Margin = new Thickness(0, 0, 12, 0) };
            // (issue 563: Ui.L("Can't be read") said why only in the chip's tooltip, which players did not find; the reason is in the row)
            if (checks.TryGetValue(d.Path, out var why) && why is { Ok: false } && !why.Value.Text.StartsWith("This is build"))
                texts.Children.Add(new TextBlock { Text = why.Value.Text, Style = (Style)FindResource("Faint"), TextWrapping = TextWrapping.Wrap,
                                                   Foreground = (Brush)FindResource("Bad"), Margin = new Thickness(0, 4, 0, 0) });
            var grid = new Grid { ColumnDefinitions = { new ColumnDefinition(), new ColumnDefinition { Width = GridLength.Auto } } };
            var chip = Chip(d.Path);
            Grid.SetColumn(chip, 1);
            grid.Children.Add(texts);
            grid.Children.Add(chip);
            var row = new RadioButton { Style = (Style)FindResource("DumpRow"), GroupName = "Dumps", Content = grid, Tag = d.Path, ToolTip = d.Path,
                                        IsChecked = string.Equals(d.Path, chosenNup, StringComparison.OrdinalIgnoreCase) };
            row.Checked += (_, _) => Choose(d.Path);
            DumpList.Children.Add(row);
        }
    }

    Border Chip(string path)
    {
        checks.TryGetValue(path, out var c);
        var (text, brush) = c == null ? (Ui.L("Checking…"), "InkFaint")
            : c.Value.Ok ? (Ui.L("✓ Supported version"), "Go")
            : c.Value.Text.StartsWith("This is build") ? (Ui.L("Not the supported version"), "Warn")
            : (Ui.L("Can't be read"), "Bad");
        var chip = new Border { Style = (Style)FindResource("Chip"), Child = new TextBlock { Text = text, FontSize = 12, Foreground = (Brush)FindResource(brush), VerticalAlignment = VerticalAlignment.Center } };
        if (c is { Ok: false }) chip.ToolTip = c.Value.Text;
        if (c is { Ok: true }) chip.Background = (Brush)FindResource("GoSoft");
        return chip;
    }

    /// Each found file's version, one at a time (the check unpacks into one probe folder); the first good one is chosen
    async void CheckAll()
    {
        if (checking) return;
        checking = true;
        var tools = Tools.Find();
        while (tools != null && dumps.FirstOrDefault(d => !checks.ContainsKey(d.Path)) is { } d)
        {
            checks[d.Path] = null;
            FillDumps();
            var r = await Runner.Check(tools, new[] { "--check", "--nup", d.Path });
            checks[d.Path] = r;
            if (!r.Ok && r.Text.Contains("antivirus", StringComparison.OrdinalIgnoreCase))
            {
                checking = false;
                ShowProblem(r.Text, tools: true);
                return;
            }
            if (chosenNup == null && r.Ok) chosenNup = d.Path;
            FillDumps();
            Refresh();
        }
        if (chosenNup == null && dumps.Count > 0 && tools != null) { chosenNup = dumps[0].Path; FillDumps(); }
        checking = false;
        Refresh();
    }

    void Choose(string path)
    {
        sourcePicked = true;
        chosenNup = path;
        romfs = exefs = null;
        Refresh();
    }

    void AddDump(string path, string where)
    {
        var info = new FileInfo(path);
        if (!dumps.Any(d => string.Equals(d.Path, info.FullName, StringComparison.OrdinalIgnoreCase)))
            dumps.Insert(0, new Dump(info.FullName, where, info.Length, info.LastWriteTime));
        sourcePicked = true;
        chosenNup = info.FullName;
        romfs = exefs = null;
        FillDumps();
        CheckAll();
        Refresh();
    }

    bool SourceReady => romfs != null && exefs != null
        || chosenNup != null && checks.TryGetValue(chosenNup, out var c) && c is { Ok: true };

    void PathsChanged(object sender, RoutedEventArgs e) => Refresh();

    void Refresh()
    {
        if (BuildButton == null) return;
        BuildButton.IsEnabled = SourceReady && OutPath.Text.Length > 2;
        ShowWork();
        try
        {
            var root = System.IO.Path.GetPathRoot(System.IO.Path.GetFullPath(OutPath.Text))!;
            var free = new DriveInfo(root).AvailableFreeSpace;
            SpaceBar.Value = Math.Min(1, NeededBytes / (double)Math.Max(1, free));
            SpaceBar.Foreground = (Brush)FindResource(free >= NeededBytes ? "Go" : "Bad");
            SpaceText.Text = Ui.F("Needs 60 GB · {0} has {1} GB free", root.TrimEnd('\\'), free >> 30);
            SpaceText.Foreground = (Brush)FindResource(free >= NeededBytes ? "InkSoft" : "Bad");
        }
        catch (Exception)
        {
            SpaceText.Text = Ui.L("Needs 60 GB");
        }
        RefreshMode();
    }

    void Dropped(object sender, DragEventArgs e)
    {
        if (runner != null || e.Data.GetData(DataFormats.FileDrop) is not string[] { Length: > 0 } paths) return;
        foreach (var p in paths)
        {
            if (p.EndsWith(".nup", StringComparison.OrdinalIgnoreCase) && File.Exists(p)) AddDump(p, "Dropped");
            else if (Directory.Exists(System.IO.Path.Combine(p, "romfs")) && Directory.Exists(System.IO.Path.Combine(p, "exefs")))
                UseFolders(System.IO.Path.Combine(p, "romfs"), System.IO.Path.Combine(p, "exefs"));
        }
    }

    void BrowseNup(object sender, RoutedEventArgs e)
    {
        var d = new OpenFileDialog { Filter = Ui.L("Game file (*.nup)|*.nup"), Title = Ui.L("Choose your game file") };
        if (d.ShowDialog(this) == true) AddDump(d.FileName, "Chosen");
    }

    /// The other way in: an unpacked game, its romfs and exefs folders (or one folder holding both)
    void BrowseFolders(object sender, RoutedEventArgs e)
    {
        var both = PickFolder(Ui.L("Choose the folder with romfs and exefs (or the romfs folder)"));
        if (both == null) return;
        if (Directory.Exists(System.IO.Path.Combine(both, "romfs")) && Directory.Exists(System.IO.Path.Combine(both, "exefs")))
        {
            UseFolders(System.IO.Path.Combine(both, "romfs"), System.IO.Path.Combine(both, "exefs"));
            return;
        }
        var ex = PickFolder(Ui.L("Now choose the exefs folder"));
        if (ex != null) UseFolders(both, ex);
    }

    void UseFolders(string r, string x)
    {
        if (!File.Exists(System.IO.Path.Combine(x, "main")))
        {
            MessageBox.Show(this, Ui.L("That exefs folder has no file named main."), "Harpia");
            return;
        }
        sourcePicked = true;
        romfs = r;
        exefs = x;
        chosenNup = null;
        foreach (var rb in DumpList.Children.OfType<RadioButton>()) rb.IsChecked = false;
        DumpList.Children.Insert(0, new TextBlock { Text = Ui.F("Using the folders {0} and {1}", r, x), Style = (Style)FindResource("Note"), Margin = new Thickness(16, 12, 16, 12) });
        Refresh();
    }

    static string? PickFolder(string title)
    {
        var d = new OpenFolderDialog { Title = title };
        return d.ShowDialog() == true ? d.FolderName : null;
    }

    void BrowseOut(object sender, RoutedEventArgs e) => OutPath.Text = PickFolder(Ui.L("Choose where the game goes")) ?? OutPath.Text;

    void BrowseWork(object sender, RoutedEventArgs e)
    {
        if (PickFolder(Ui.L("Choose where Harpia keeps its build files")) is not { } picked) return;
        ChangeWork(picked);
    }

    void ResetWork(object sender, RoutedEventArgs e) => ChangeWork(null);

    void ChangeWork(string? picked)
    {
        try { Storage.MoveWork(picked); }
        catch (Exception x) when (x is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, Ui.F("Harpia cannot keep its build files there: {0}", x.Message), "Harpia");
        }
        Refresh();
    }

    /// How much the build files take and where, said before the build (they stay after it, for quick updates)
    void ShowWork()
    {
        if (WorkNote == null) return;
        var work = Storage.Work;
        WorkReset.Visibility = Storage.WorkChosen ? Visibility.Visible : Visibility.Collapsed;
        WorkNote.Text = Ui.F("Build files: about {0}, kept in {1} so that updates are quick. They can be freed after the build.", Storage.Gb(Storage.BuildFilesBytes), work);
        WorkNote.Foreground = (Brush)FindResource("InkSoft");
        try
        {
            var root = System.IO.Path.GetPathRoot(System.IO.Path.GetFullPath(work))!;
            var free = new DriveInfo(root).AvailableFreeSpace;
            if (free < Storage.BuildFilesBytes)
            {
                WorkNote.Text += Ui.F(" {0} has only {1} free.", root.TrimEnd('\\'), Storage.Gb(free));
                WorkNote.Foreground = (Brush)FindResource("Bad");
            }
        }
        catch (Exception) { }
    }

    void LanguageChanged(object sender, SelectionChangedEventArgs e)
    {
        if (LanguageBox.SelectedItem is not Language l) return;
        lang = l;   // (the game's language only: the window's is the PT | EN switch)
    }

    // ---- (local: Harpia) the window's language: PT | EN in its corner, kept in builder-settings.json; pt-BR first ----
    void UiLanguage(object sender, RoutedEventArgs e)
    {
        if (sender is not FrameworkElement { Tag: string code } || (code == "pt") == Ui.Pt) return;
        Ui.Use(code == "pt" ? Strings.Default() : Strings.English);
        SaveSettings();
        RefreshUiTexts(check: true);
    }

    void RefreshUiTexts(bool check = false)
    {
        var speed = WorkerBox.SelectedIndex;   // (the speeds' names in the window's language: the list made again)
        WorkerBox.ItemsSource = BuildWorkers.Choices(Environment.ProcessorCount);
        WorkerBox.SelectedIndex = Math.Max(0, speed);
        UiPt.Foreground = (Brush)FindResource(Ui.Pt ? "Brand" : "InkFaint");
        UiEn.Foreground = (Brush)FindResource(Ui.Pt ? "InkFaint" : "Brand");
        ToolsNote.Text = Tools.Find() != null
            ? Ui.L("Nothing else to install: Harpia's own build tools came with its installer, each one checked.")
            : Ui.L("Harpia's build tools are missing from its folder. Press Build and Harpia offers to download them again.");
        RefreshMode();
        Refresh();
        if (check && runner == null && !updating) _ = RunPcCheck();   // (its lines are in the window's language)
        CancelButton.Content = T(S.Cancel);
        BackButton.Content = T(S.Back);
        OpenButton.Content = T(S.OpenFolder);
        PlayText.Text = T(S.Play);
        ShowLog.Content = T(ShowLog.IsChecked == true ? S.HideDetails : S.ShowDetails);
    }

    // ================= 2: the build =================

    async void StartBuild(object sender, RoutedEventArgs e)
    {
        switch (mode)
        {
            case Mode.UpdateGame:   // (the kept game files; the newer builder first when this one is older)
                SaveSettings();
                Updater.UpdateFolder(this, OutPath.Text, updateTarget, fetched: false);
                return;
            case Mode.UpToDate:
                finished = OutPath.Text;
                Play(sender, e);
                return;
            case Mode.UpdateBuilder:
                SaveSettings();
                BuildButton.IsEnabled = false;
                BuildButton.Content = Ui.L("Getting the new builder…");
                try { await Updater.RunLatestSetup(this, Array.Empty<string>()); }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message + Ui.L(" You can also download HarpiaBuilder.exe from the releases page."), "Harpia");
                    RefreshMode();
                }
                return;
        }
        SaveSettings();
        var tools = Tools.Find();
        if (tools == null)
        {
            ShowProblem(Ui.L("The build tools are not in Harpia's folder."), tools: true);
            return;
        }
        var args = new List<string>();
        if (romfs != null && exefs != null) args.AddRange(new[] { "--romfs", romfs, "--exefs", exefs });
        else if (chosenNup != null) args.AddRange(new[] { "--nup", chosenNup });
        else return;
        args.AddRange(new[] { "--out", OutPath.Text, "--language", lang.Code });
        Run(tools, args);
    }

    static readonly (string Id, string Build, string Update)[] Stages =
    {
        ("download", "", "Download the update"),
        ("read", "Read your game files", "Read the game files kept in the folder"),
        ("lift", "Translate the game to PC code", "Translate the game to PC code"),
        ("compile", "Compile", "Compile changed files"),
        ("finish", "Put the game together", "Swap in the new game"),
    };
    static readonly Dictionary<string, (double From, double Size)> Weight = new()
    {
        ["download"] = (0, 0.04), ["read"] = (0.04, 0.04), ["lift"] = (0.08, 0.14), ["compile"] = (0.22, 0.74), ["finish"] = (0.96, 0.04),
    };

    void MakeRows(bool update)
    {
        StageList.Children.Clear();
        rows.Clear();
        var list = Stages.Where(s => update || s.Id != "download").ToList();
        for (var i = 0; i < list.Count; i++)
        {
            var row = new StepRow(i + 1, Ui.L(update ? list[i].Update : list[i].Build), last: i == list.Count - 1);
            rows[list[i].Id] = row;
            StageList.Children.Add(row.Root);
        }
        if (update) rows["read"].Small(Ui.L("Your game files are kept: the .nup isn't needed"));
    }

    void Run(Tools tools, List<string> args, bool keepRows = false)
    {
        pcCheck?.Cancel();   // (its CPU and disk test would slow the build down)
        int? workers;
        try { workers = BuildWorkers.Selected(WorkerBox.SelectedItem); }
        catch (ArgumentException e)
        {
            MessageBox.Show(this, e.Message, "Harpia");
            return;
        }
        if (!keepRows)
        {
            updating = false;
            MakeRows(update: false);
            SetStep(1);
            BuildTitle.Text = Ui.L("Building your game");
            UpdateBar.Visibility = Visibility.Collapsed;
            RingBox.Visibility = Visibility.Visible;
        }
        log.Clear();
        Log.Clear();
        current = null;
        finished = "";
        compiled = 0;
        filesDone = filesOf = 0;
        filesChanged = -1;
        filesCompiled = 0;
        compileEta = -1;
        thisCompile = 0;
        previousTotal = -1;
        ShowPage(BuildPage);
        CancelButton.Visibility = Visibility.Visible;
        SetProgress(updating ? Weight["read"].From : 0);
        elapsed.Restart();
        compileClock.Reset();
        clock.Start();
        Tick();
        if (Storage.WorkChosen && !args.Contains("--work")) args.AddRange(new[] { "--work", Storage.Work });   // (the build files where the player chose)
        runner = new Runner(tools, args, workers);
        runner.Line += l => Dispatcher.BeginInvoke(() => Line(l));
        runner.Finished += failure => Dispatcher.BeginInvoke(() => Ended(failure));
        runner.Start();
    }

    static readonly Regex FilesLine = new(@"^(\d+) files, (?:(\d+) jobs)?"), FileLine = new(@"^\s+\S+\s+\d+ MB\s+\d+s");
    int compilers;   // (build.py's "N files, J jobs": the compilers it runs at once, shown beside the files)
    string Compilers => compilers > 0 ? " · " + Ui.F("{0} compilers", compilers) : "";

    void Line(string line)
    {
        if (line.StartsWith("@@stage "))
        {
            var id = line.Split(' ')[1];
            if (current != null && rows.TryGetValue(current, out var was)) was.Done();
            current = id;
            if (rows.TryGetValue(id, out var row)) row.Begin();
            if (id == "compile") compileClock.Restart();
            if (Weight.TryGetValue(id, out var w)) SetProgress(w.From);
        }
        else if (line.StartsWith("@@progress "))
        {
            if (double.TryParse(line[11..], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var f) && current != null
                && Weight.TryGetValue(current, out var w))
            {
                if (current == "compile") compiled = f;
                SetProgress(w.From + w.Size * Math.Clamp(f, 0, 1));
            }
            return;
        }
        else if (line.StartsWith("@@plan ") || line.StartsWith("@@file ") || line.StartsWith("@@eta ") || line.StartsWith("@@times "))
        {
            Number(line);
            return;
        }
        else if (line.StartsWith("@@done ")) finished = line[7..];
        else if (FilesLine.Match(line) is { Success: true } m)
        {
            filesOf = int.Parse(m.Groups[1].Value);
            if (m.Groups[2].Success) compilers = int.Parse(m.Groups[2].Value);
        }
        else if (FileLine.IsMatch(line)) filesDone++;
        if (current == "compile" && filesChanged < 0 && filesOf > 0 && rows.TryGetValue("compile", out var c))
            c.Small(Ui.F("{0} of {1} files", filesDone, filesOf) + Compilers);
        AddLog(line);
    }

    /// The compile timer's lines: build.py says which files this run really rebuilds and how long each took before (the
    /// countdown), and make_game.py gives this build's times beside the last one's. Not shown in details: the compiler's
    /// own lines there already carry each file's seconds.
    void Number(string line)
    {
        var p = line.Split(' ');
        switch (p[0])
        {
            case "@@plan" when p.Length == 3 && int.TryParse(p[1], out var changed) && int.TryParse(p[2], out var total):
                filesChanged = changed;
                filesOf = total;
                filesCompiled = 0;
                ChangedFiles();
                break;
            case "@@file" when p.Length == 4 && (p[3] is "new" or "again"):
                filesCompiled++;
                ChangedFiles();
                break;
            case "@@eta" when p.Length == 2 && double.TryParse(p[1], System.Globalization.NumberStyles.Float,
                                                              System.Globalization.CultureInfo.InvariantCulture, out var eta):
                compileEta = eta;
                break;
            case "@@times" when p.Length == 4:
                long.TryParse(p[2], out thisCompile);
                long.TryParse(p[3], out previousTotal);
                break;
        }
    }

    void ChangedFiles()
    {
        if (filesChanged < 0 || !rows.TryGetValue("compile", out var row)) return;
        row.Small(filesChanged == 0 ? Ui.L("Everything is already built")
                : filesChanged >= filesOf ? Ui.F("{0} of {1} files", filesCompiled, filesChanged) + Compilers
                : Ui.F("{0} of {1} changed files", filesCompiled, filesChanged) + Compilers);
    }

    void AddLog(string line)
    {
        log.AppendLine(line);
        if (log.Length > 600_000) log.Remove(0, log.Length - 400_000);
        if (Log.Visibility == Visibility.Visible)
        {
            Log.AppendText(line + "\n");
            Log.ScrollToEnd();
        }
    }

    void SetProgress(double f)
    {
        f = Math.Clamp(f, 0, 1);
        RingText.Text = $"{(int)(f * 100)}%";
        UpdateBar.Value = f;
        const double r = 55, c = 60;
        if (f >= 0.999) { RingArc.Data = new EllipseGeometry(new Point(c, c), r, r); return; }
        var a = f * 2 * Math.PI;
        var end = new Point(c + r * Math.Sin(a), c - r * Math.Cos(a));
        var fig = new PathFigure { StartPoint = new Point(c, c - r), IsClosed = false };
        fig.Segments.Add(new ArcSegment(end, new Size(r, r), 0, f > 0.5, SweepDirection.Clockwise, true));
        RingArc.Data = f < 0.002 ? null : new PathGeometry(new[] { fig });
    }

    void Tick()
    {
        foreach (var r in rows.Values) r.Tick();
        if (runner == null) return;
        var tail = updating ? Ui.L(". Your current game stays as it is until the new one is complete.") : Ui.L(" · your PC stays usable");
        if (current == "compile")
        {
            // (the timer of the compile itself, and the countdown build.py works out from what this PC measured before)
            var left = compileEta >= 0 ? TimeLeft(compileEta)
                : compiled > 0.03 ? TimeLeft(compileClock.Elapsed.TotalSeconds / compiled * (1 - compiled) + 60)
                : Ui.L("Estimating…");
            BuildNote.Text = Ui.F("Compiling {0} · {1}{2}", Clock(compileClock.Elapsed), left, tail);
            return;
        }
        BuildNote.Text = (updating ? Ui.L("Working…") : EstimateMinutes() is double m ? Ui.F("About {0:0} minutes in all", m) : Ui.L("Estimating…")) + tail;
    }

    // (local: Harpia) the PC check's estimate for the speed chosen (PcCheck.cs; none until calibrated): no fixed "45 minutes"
    PcCheck.Report? pcReport;
    double? EstimateMinutes()
    {
        if (pcReport == null) return null;
        int? workers = null;
        try { workers = BuildWorkers.Selected(WorkerBox.SelectedItem); } catch (ArgumentException) { }
        return workers is int n && n >= Environment.ProcessorCount ? pcReport.MaxMinutes : pcReport.Minutes;
    }
    void ShowEstimate() => AboutText.Text = EstimateMinutes() is double m ? Ui.F("About {0:0} min", m) : "";
    void WorkerChanged(object sender, SelectionChangedEventArgs e) => ShowEstimate();

    static string Clock(TimeSpan t) => $"{(int)t.TotalMinutes}:{t.Seconds:00}";

    static string TimeLeft(double seconds)
    {
        if (seconds < 45) return Ui.L("Less than a minute left");
        if (seconds < 90) return Ui.L("About a minute left");
        var minutes = (int)Math.Round(seconds / 60);
        return minutes >= 60 ? Ui.F("About {0} h {1} min left", minutes / 60, minutes % 60) : Ui.F("About {0} minutes left", minutes);
    }

    void Ended(string? failure)
    {
        clock.Stop();
        elapsed.Stop();
        runner = null;
        if (finished.Length > 0)
        {
            foreach (var r in rows.Values) r.Done();
            SetProgress(1);
            ShowReady();
            return;
        }
        if (failure == "cancelled")
        {
            SetStep(0);
            ShowPage(FilesPage);
            return;
        }
        if (current != null && rows.TryGetValue(current, out var row)) row.Stopped();
        ShowProblem(failure ?? Ui.L("The build tools stopped without saying why."), tools: failure?.Contains("antivirus", StringComparison.OrdinalIgnoreCase) == true);
    }

    void LogToggled(object sender, RoutedEventArgs e)
    {
        var show = ShowLog.IsChecked == true;
        Log.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        StepCard.Visibility = show ? Visibility.Collapsed : Visibility.Visible;
        ShowLog.Content = T(show ? S.HideDetails : S.ShowDetails);
        if (show)
        {
            Log.Text = log.ToString();
            Log.ScrollToEnd();
        }
    }

    void CancelBuild(object sender, RoutedEventArgs e)
    {
        download?.Cancel();
        runner?.Cancel();
    }

    // ================= 3: play =================

    void ShowReady()
    {
        SetStep(2);
        ReadyTitle.Text = updating ? Ui.L("Your game is updated") : Ui.L("Your game is ready");
        var compiledNote = thisCompile > 0 ? Ui.F(" (compiled in {0})", Span(TimeSpan.FromSeconds(thisCompile))) : "";
        ReadyNote.Text = Ui.F(updating ? "Updated in {0}{1}{2} · {3}" : "Built in {0}{1}{2} · {3}", Span(elapsed.Elapsed), compiledNote, Against(elapsed.Elapsed), finished);
        if (!updating && Environment.GetEnvironmentVariable("HARPIA_UI_PREVIEW") == null)
            Finder.Shortcuts(finished, DesktopTick.IsChecked == true, StartTick.IsChecked == true);
        else if (updating && Environment.GetEnvironmentVariable("HARPIA_UI_PREVIEW") == null)
        {   // (the ones the player has, onto the folder's launcher; the boxes say which there are, and ticking one makes it)
            DesktopTick.IsChecked = File.Exists(Finder.DesktopLink);
            StartTick.IsChecked = File.Exists(Finder.StartLink);
            Finder.RefreshShortcuts(finished);
        }
        if (Finder.ShortcutError != null) AddLog("shortcut not made: " + Finder.ShortcutError);
        RefreshDlss();
        ShowPage(ReadyPage);
        _ = ShowStorageUse();
    }

    // (issue: 5 GB and more left in %LOCALAPPDATA% and never said. Said here, with the way to let it go)
    async Task ShowStorageUse()
    {
        StorageRow.Visibility = Visibility.Collapsed;
        var (total, freeable) = await Task.Run(Storage.Measure);
        if (total < 100L << 20) return;
        StorageNote.Text = freeable >= 100L << 20
            ? Ui.F("Harpia Builder keeps {0} on this PC ({1}). {2} of it only makes the next update quicker and can be freed: your game keeps playing, and the next update then takes about as long as this build.", Storage.Gb(total), Storage.Base, Storage.Gb(freeable))
            : Ui.F("Harpia Builder keeps {0} on this PC ({1}): the builder itself.", Storage.Gb(total), Storage.Base);
        FreeButton.Visibility = freeable >= 100L << 20 ? Visibility.Visible : Visibility.Collapsed;
        FreeButton.IsEnabled = true;
        FreeButton.Content = Ui.L("Free up space");
        StorageRow.Visibility = Visibility.Visible;
    }

    async void FreeSpace(object sender, RoutedEventArgs e)
    {
        FreeButton.IsEnabled = false;
        FreeButton.Content = Ui.L("Freeing…");
        var left = await Task.Run(Storage.Free);
        await ShowStorageUse();
        if (left >= 100L << 20)
            StorageNote.Text += Ui.F(" ({0} could not be deleted: a file is in use. Close the game and the builder, and delete the folder's contents by hand.)", Storage.Gb(left));
    }

    void ShowStorage(object sender, RoutedEventArgs e) =>
        Process.Start(new ProcessStartInfo("explorer.exe", $"\"{Storage.Base}\"") { UseShellExecute = true });

    void ShortcutsChanged(object sender, RoutedEventArgs e)
    {
        if (finished.Length > 0) Finder.Shortcuts(finished, DesktopTick.IsChecked == true, StartTick.IsChecked == true);
    }

    static string Span(TimeSpan t) => t.TotalHours >= 1 ? Ui.F("{0} h {1} min", (int)t.TotalHours, t.Minutes) : t.TotalMinutes >= 1 ? Ui.F("{0} minutes", (int)t.TotalMinutes) : Ui.F("{0} seconds", t.Seconds);

    /// This build beside the last one on this PC (build_times.json, through make_game.py's @@times): "last time 45 minutes
    /// (4 minutes faster)"
    string Against(TimeSpan now)
    {
        if (previousTotal <= 0) return "";
        var last = TimeSpan.FromSeconds(previousTotal);
        var diff = last.TotalSeconds - now.TotalSeconds;
        var how = Math.Abs(diff) < 30 ? Ui.L("about the same")
                : diff > 0 ? Ui.F("{0} faster", Span(TimeSpan.FromSeconds(diff))) : Ui.F("{0} slower", Span(TimeSpan.FromSeconds(-diff)));
        return Ui.F(" · last time {0} ({1})", Span(last), how);
    }

    void OpenFolder(object sender, RoutedEventArgs e) =>
        Process.Start(new ProcessStartInfo("explorer.exe", $"\"{finished}\"") { UseShellExecute = true });

    void Play(object sender, RoutedEventArgs e)
    {
        StartGame(finished);
        WindowState = WindowState.Minimized;
    }

    // ================= when something goes wrong =================

    bool problemIsTools;

    /// (local: Harpia) make_game.py's Windows SDK check (check_windows_sdk) failing: Microsoft's compiler and SDK are not installed
    static bool NeedsBuildTools(string why) =>
        new[] { "Visual Studio", "Windows SDK", "Build Tools", "vcvars", "stdio.h", "windows.h" }.Any(k => why.Contains(k, StringComparison.OrdinalIgnoreCase));

    void ShowProblem(string why, bool tools)
    {
        lastFailure = why;
        problemIsTools = tools;
        ProblemSteps.Children.Clear();
        ProblemSub.Text = updating ? Ui.L("Your current game is unchanged and still plays.") : Ui.L("Nothing was changed in your game folder.");
        if (tools)
        {
            ProblemTitle.Text = Ui.L("The build tools need repairing");
            ProblemHead.Text = Ui.L("Your antivirus removed part of the build tools");
            ProblemText.Text = why + "\n\n" + Ui.L("Antivirus programs sometimes flag unsigned developer tools by mistake.");
            var base_ = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HarpiaBuilder");
            foreach (var (n, s) in new[]
                     {
                         (1, Ui.L("Open Windows Security → Protection history, and restore the removed files.")),
                         (2, Ui.F("Or add {0} to its exclusions.", base_)),
                         (3, Ui.L("Then press Repair: Harpia downloads the tools again from their official releases and checks each one.")),
                     })
                ProblemSteps.Children.Add(new TextBlock { Text = $"{n}.  {s}", Style = (Style)FindResource("Note"), FontSize = 14, Margin = new Thickness(0, 6, 0, 0) });
        }
        else if (NeedsBuildTools(why))
        {
            ProblemTitle.Text = Ui.L("The build stopped");
            ProblemHead.Text = Ui.L("Visual Studio Build Tools or the Windows SDK is missing");
            ProblemText.Text = Ui.L("Harpia compiles the game with Microsoft's C++ compiler, which Microsoft does not allow to be bundled.") + "\n\n" + why;
            foreach (var (n, s) in new[]
                     {
                         (1, Ui.L("Install Visual Studio Build Tools: visualstudio.microsoft.com/downloads (Tools for Visual Studio).")),
                         (2, Ui.L("In its installer, choose \"Desktop development with C++\" and keep the Windows SDK it selects.")),
                         (3, Ui.L("Then press Build again: nothing needs to be downloaded twice.")),
                     })
                ProblemSteps.Children.Add(new TextBlock { Text = $"{n}.  {s}", Style = (Style)FindResource("Note"), FontSize = 14, Margin = new Thickness(0, 6, 0, 0) });
        }
        else
        {
            ProblemTitle.Text = updating ? Ui.L("The update stopped") : Ui.L("The build stopped");
            ProblemHead.Text = Ui.L("What went wrong");
            ProblemText.Text = why;
        }
        RepairButton.Visibility = tools ? Visibility.Visible : Visibility.Collapsed;
        ProblemDetails.Content = Ui.L("Show details");
        if (!updating) SetStep(1);   // (the update keeps its versions in the head)
        ShowPage(ProblemPage);
    }

    void ProblemShowDetails(object sender, RoutedEventArgs e)
    {
        var lines = log.ToString().Split('\n');
        ProblemSteps.Children.Clear();
        ProblemSteps.Children.Add(new TextBox
        {
            Text = string.Join("\n", lines.Skip(Math.Max(0, lines.Length - 80))), IsReadOnly = true, FontFamily = (FontFamily)FindResource("Mono"), FontSize = 11,
            Foreground = (Brush)FindResource("InkSoft"), Background = new SolidColorBrush(Color.FromArgb(0x33, 0, 0, 0)), BorderThickness = new Thickness(0),
            Padding = new Thickness(8), TextWrapping = TextWrapping.NoWrap, HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
        });
    }

    void CopyForReport(object sender, RoutedEventArgs e)
    {
        var lines = log.ToString().Split('\n');
        var text = $"Harpia Builder {Tools.Find()?.Version ?? "(source)"} on Windows {Environment.OSVersion.Version}\n" +
                   $"{ProblemTitle.Text}: {lastFailure}\n\n" + string.Join("\n", lines.Skip(Math.Max(0, lines.Length - 200)));
        try { Clipboard.SetText(text); CopyReport.Content = Ui.L("Copied"); } catch (Exception) { }
    }

    void Back(object sender, RoutedEventArgs e)
    {
        SetStep(0);
        ShowPage(FilesPage);
    }

    /// The setup (HarpiaBuilder-Setup.exe beside this app) checks every part against its checksum and downloads the missing ones
    void Repair(object sender, RoutedEventArgs e)
    {
        var setup = System.IO.Path.Combine(AppContext.BaseDirectory, "HarpiaBuilder-Setup.exe");
        if (File.Exists(setup))
        {
            Process.Start(new ProcessStartInfo(setup, "--repair") { UseShellExecute = false, WorkingDirectory = AppContext.BaseDirectory });
            Application.Current.Shutdown();
            return;
        }
        Process.Start(new ProcessStartInfo("https://github.com/leandromedeirosfelix/HarpiaBr/releases/latest") { UseShellExecute = true });
    }

    // ================= the update (Updater.cs): the game started this app with --update <folder> =================

    CancellationTokenSource? download;

    public void BeginUpdate(string folder, string? tag)
    {
        updating = true;
        OutPath.Text = folder;
        finished = "";
        var to = tag ?? Ui.L("the latest version");
        var from = GameVersion(folder) ?? Tools.Find()?.Version;
        StepPill.Visibility = Visibility.Collapsed;
        VersionChip.Visibility = Visibility.Visible;
        VersionChipText.Text = from != null && from != to ? $"{from} → {to}" : to;
        MakeRows(update: true);
        BuildTitle.Text = Ui.L("Updating your game");
        BuildNote.Text = Ui.L("Getting the update…");
        RingBox.Visibility = Visibility.Collapsed;
        UpdateBar.Visibility = Visibility.Visible;
        SetProgress(0);
        rows["download"].Begin();
        CancelButton.Visibility = Visibility.Visible;
        ShowPage(BuildPage);
    }

    public CancellationToken DownloadToken => (download = new CancellationTokenSource()).Token;

    public void DownloadProgress(double f)
    {
        rows.GetValueOrDefault("download")?.Small($"{(int)(f * 100)}%");
        SetProgress(Weight["download"].Size * f);
    }

    public void Note(string line)
    {
        AddLog(line);
        rows.GetValueOrDefault("download")?.Small(line);
    }

    public void DownloadFailed(string why)
    {
        download = null;
        rows.GetValueOrDefault("download")?.Stopped();
        ShowProblem(why, tools: false);
        ProblemTitle.Text = Ui.L("The update could not be downloaded");
    }

    /// The new tools are here: the folder built again with them (only what changed is compiled)
    internal void UpdateBuild(Tools tools, string folder)
    {
        download = null;
        if (rows.TryGetValue("download", out var d)) { d.Done(); d.Small(Ui.L("Build tools checked")); }
        Run(tools, new List<string> { "--update", folder }, keepRows: true);
    }

    // ================= the window =================

    void Minimize(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

    void CloseWindow(object sender, RoutedEventArgs e)
    {
        if (runner != null && MessageBox.Show(this, T(S.CloseQuestion), "Harpia", MessageBoxButton.YesNo) != MessageBoxResult.Yes) return;
        pcCheck?.Cancel();
        runner?.Cancel();
        Close();
    }
}

/// One step of the build: its mark (done, now, waiting), its name, a line under it and its time.
sealed class StepRow
{
    public readonly Grid Root = new() { Margin = new Thickness(0, 10, 0, 10) };
    readonly Grid mark = new() { Width = 22, Height = 22, VerticalAlignment = VerticalAlignment.Top, Margin = new Thickness(0, 1, 0, 0) };
    readonly Ellipse disc = new(), halo = new() { Width = 32, Height = 32, Margin = new Thickness(-5) };
    readonly TextBlock number = new() { FontSize = 12, FontWeight = FontWeights.SemiBold, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center };
    readonly Path tick = new() { Data = Geometry.Parse("M0,4 L3.5,7.5 L10,0"), StrokeThickness = 1.8, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center };
    readonly TextBlock name = new() { FontWeight = FontWeights.SemiBold };
    readonly TextBlock small = new() { FontSize = 12, Visibility = Visibility.Collapsed, TextTrimming = TextTrimming.CharacterEllipsis };
    readonly TextBlock time = new() { FontSize = 12, Text = "—", VerticalAlignment = VerticalAlignment.Top, Margin = new Thickness(0, 2, 0, 0) };
    readonly Stopwatch watch = new();
    readonly int n;
    bool running;

    static Brush B(string key) => (Brush)Application.Current.FindResource(key);

    public StepRow(int n, string text, bool last)
    {
        this.n = n;
        Root.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(34) });
        Root.ColumnDefinitions.Add(new ColumnDefinition());
        Root.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        mark.Children.Add(halo);
        mark.Children.Add(disc);
        mark.Children.Add(number);
        mark.Children.Add(tick);
        tick.Stroke = B("Go");
        time.FontFamily = (FontFamily)Application.Current.FindResource("Mono");
        var texts = new StackPanel { Children = { name, small } };
        Grid.SetColumn(texts, 1);
        Grid.SetColumn(time, 2);
        if (!last)
        {
            var link = new Rectangle { Width = 1, Fill = B("GlassLine"), HorizontalAlignment = HorizontalAlignment.Left, Margin = new Thickness(10.5, 28, 0, -19) };
            Root.Children.Add(link);
        }
        Root.Children.Add(mark);
        Root.Children.Add(texts);
        Root.Children.Add(time);
        name.Text = text;
        Wait();
    }

    public void Small(string text)
    {
        small.Text = text;
        small.Visibility = Visibility.Visible;
    }

    void Wait()
    {
        disc.Fill = Brushes.Transparent;
        disc.Stroke = B("GlassLineStrong");
        disc.StrokeThickness = 1.5;
        halo.Fill = Brushes.Transparent;
        number.Text = "";
        tick.Visibility = Visibility.Collapsed;
        name.Foreground = B("InkFaint");
        name.FontWeight = FontWeights.Medium;
        small.Foreground = B("InkFaint");
        time.Foreground = B("InkFaint");
    }

    public void Begin()
    {
        running = true;
        watch.Restart();
        disc.Fill = B("Brand");
        disc.Stroke = null;
        halo.Fill = B("BrandSoft");
        number.Text = n.ToString();
        number.Foreground = Brushes.White;
        tick.Visibility = Visibility.Collapsed;
        name.Foreground = B("Ink");
        name.FontWeight = FontWeights.SemiBold;
        time.Foreground = B("Ink");
        Tick();
    }

    public void Tick()
    {
        if (running) time.Text = $"{(int)watch.Elapsed.TotalMinutes}:{watch.Elapsed.Seconds:00}";
    }

    public void Done()
    {
        if (running) Tick();
        running = false;
        watch.Stop();
        disc.Fill = B("GoSoft");
        disc.Stroke = null;
        halo.Fill = Brushes.Transparent;
        number.Text = "";
        tick.Visibility = Visibility.Visible;
        name.Foreground = B("Ink");
        name.FontWeight = FontWeights.SemiBold;
        time.Foreground = B("InkFaint");
    }

    public void Stopped()
    {
        running = false;
        watch.Stop();
        disc.Fill = B("BadSoft");
        halo.Fill = Brushes.Transparent;
        number.Text = "!";
        number.Foreground = B("Bad");
    }
}
