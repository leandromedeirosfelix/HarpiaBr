using System.IO;
using System.Net.Http;
using System.Windows;

namespace HarpiaBuilder;

/// (local: Harpia) NVIDIA DLSS, optional (NvidiaDlss.cs): offered after a build (the ready page) and for a game folder already
/// built (the first page, beside Play), only on an NVIDIA RTX card. Its own page: what it is and where it comes from, then the
/// download (cancellable), then NVIDIA's licences with an unticked "I accept" that only the player ticks, then the copy.
public partial class MainWindow
{
    enum DlssStep { Offer, Downloading, Licence, Remove, Done }
    DlssStep dlssStep;
    string dlssFolder = "";
    FrameworkElement? dlssFrom;
    CancellationTokenSource? dlssCancel;

    /// The two buttons' words and whether they show: an RTX card (the PC check's), the folder with DLSS or not
    void RefreshDlss()
    {
        var supported = NvidiaDlss.Supported(pcReport?.Gpu);
        var readyFolder = finished.Length > 2 ? finished : OutPath.Text;
        DlssReadyButton.Content = readyFolder.Length > 2 && NvidiaDlss.Installed(readyFolder) ? Ui.L("Remove DLSS") : Ui.L("Enable NVIDIA DLSS (optional)");
        DlssFilesButton.Content = OutPath.Text.Length > 2 && NvidiaDlss.Installed(OutPath.Text) ? Ui.L("Remove DLSS") : Ui.L("Enable NVIDIA DLSS (optional)");
        DlssReadyButton.Visibility = supported ? Visibility.Visible : Visibility.Collapsed;
        DlssFilesButton.Visibility = supported && mode == Mode.UpToDate ? Visibility.Visible : Visibility.Collapsed;
        DlssReadyNote.Text = supported || pcReport == null ? ""
            : Ui.F("NVIDIA DLSS needs an NVIDIA RTX card (this PC: {0}).", pcReport.Gpu ?? Ui.L("unknown"));
    }

    void DlssOpen(object sender, RoutedEventArgs e)
    {
        dlssFrom = sender == DlssReadyButton ? ReadyPage : FilesPage;
        dlssFolder = sender == DlssReadyButton && finished.Length > 2 ? finished : OutPath.Text;
        DlssLicenceBox.Visibility = DlssAccept.Visibility = DlssBar.Visibility = Visibility.Collapsed;
        DlssAccept.IsChecked = false;   // (never ticked for the player)
        DlssGo.IsEnabled = true;
        if (NvidiaDlss.Installed(dlssFolder))
        {
            dlssStep = DlssStep.Remove;
            DlssSub.Text = Ui.F("NVIDIA DLSS is on in {0}.", dlssFolder);
            DlssStatus.Text = Ui.L("Remove DLSS deletes only NVIDIA's files the Builder copied (9 DLLs and their 2 licence files). The game keeps working, without DLSS.");
            DlssGo.Content = Ui.L("Remove DLSS");
        }
        else
        {
            dlssStep = DlssStep.Offer;
            DlssSub.Text = Ui.L("NVIDIA's DLSS sharpens the image and raises the frame rate on RTX cards; Frame Generation (RTX 40 and newer) shows extra frames.");
            DlssStatus.Text = Ui.F("It comes from NVIDIA's official package (Streamline {0}, {1} MB, github.com/NVIDIA-RTX/Streamline), checked by its SHA-256, and goes only into your game folder. NVIDIA's terms are shown before any file is copied: you read and accept them yourself.",
                                   NvidiaDlss.Version, NvidiaDlss.Size >> 20);
            DlssGo.Content = File.Exists(NvidiaDlss.Zip) ? Ui.L("Continue") : Ui.F("Download from NVIDIA ({0} MB)", NvidiaDlss.Size >> 20);
        }
        ShowPage(DlssPage);
    }

    async void DlssGoClick(object sender, RoutedEventArgs e)
    {
        switch (dlssStep)
        {
            case DlssStep.Offer:
                await DlssFetch();
                return;
            case DlssStep.Downloading:
                dlssCancel?.Cancel();
                return;
            case DlssStep.Licence:
                if (DlssAccept.IsChecked != true) return;
                DlssGo.IsEnabled = false;
                DlssStatus.Text = Ui.L("Copying NVIDIA's files and checking their signatures…");
                try
                {
                    var folder = dlssFolder;
                    await Task.Run(() => NvidiaDlss.Install(folder));
                    DlssDone(Ui.L("NVIDIA DLSS is on. In the game, open the menu (Insert) and choose DLSS in Graphics."));
                }
                catch (Exception x)
                {
                    AddLog("DLSS not installed: " + x.Message);
                    DlssDone(x is InvalidDataException ? x.Message : Ui.F("NVIDIA DLSS could not be installed ({0}). The game works without it.", x.Message));
                }
                return;
            case DlssStep.Remove:
                try
                {
                    NvidiaDlss.Remove(dlssFolder);
                    DlssDone(Ui.L("DLSS removed. The game works as before, without DLSS."));
                }
                catch (Exception x) { DlssDone(Ui.F("DLSS could not be removed ({0}). Close the game and try again.", x.Message)); }
                return;
            case DlssStep.Done:
                DlssBackClick(sender, e);
                return;
        }
    }

    async Task DlssFetch()
    {
        dlssStep = DlssStep.Downloading;
        dlssCancel = new CancellationTokenSource();
        DlssGo.Content = Ui.L("Cancel download");
        DlssBar.Value = 0;
        DlssBar.Visibility = Visibility.Visible;
        DlssStatus.Text = Ui.L("Downloading NVIDIA's package…");
        var progress = new Progress<double>(f =>
        {
            DlssBar.Value = f;
            DlssStatus.Text = Ui.F("Downloading NVIDIA's package… {0:0} of {1} MB", f * NvidiaDlss.Size / 1048576.0, NvidiaDlss.Size >> 20);
        });
        try
        {
            await NvidiaDlss.Fetch(progress, dlssCancel.Token);
            DlssStatus.Text = Ui.L("Checking the package…");
            DlssLicence.Text = await Task.Run(NvidiaDlss.LicenceText);
            dlssStep = DlssStep.Licence;
            DlssBar.Visibility = Visibility.Collapsed;
            DlssLicenceBox.Visibility = DlssAccept.Visibility = Visibility.Visible;
            DlssAccept.IsChecked = false;
            DlssStatus.Text = Ui.L("NVIDIA's package is here and checked. Read NVIDIA's terms below: DLSS is installed only if you accept them.");
            DlssGo.Content = Ui.L("Enable DLSS");
            DlssGo.IsEnabled = false;
        }
        catch (OperationCanceledException)
        {
            DlssOffer(Ui.L("Download cancelled. Nothing was installed."));
        }
        catch (HttpRequestException x)
        {
            AddLog("DLSS download: " + x.Message);
            DlssOffer(Ui.L("Could not reach NVIDIA's download (no internet, or GitHub did not answer). Nothing was installed: the game works without DLSS."));
        }
        catch (Exception x)
        {
            AddLog("DLSS download: " + x.Message);
            DlssOffer(x is InvalidDataException ? x.Message : Ui.F("The download failed ({0}). Nothing was installed: the game works without DLSS.", x.Message));
        }
    }

    void DlssOffer(string status)
    {
        dlssStep = DlssStep.Offer;
        DlssBar.Visibility = Visibility.Collapsed;
        DlssStatus.Text = status;
        DlssGo.Content = Ui.L("Try again");
        DlssGo.IsEnabled = true;
    }

    void DlssDone(string status)
    {
        dlssStep = DlssStep.Done;
        DlssLicenceBox.Visibility = DlssAccept.Visibility = DlssBar.Visibility = Visibility.Collapsed;
        DlssStatus.Text = status;
        DlssGo.Content = Ui.L("Done");
        DlssGo.IsEnabled = true;
    }

    void DlssAcceptChanged(object sender, RoutedEventArgs e) => DlssGo.IsEnabled = dlssStep == DlssStep.Licence && DlssAccept.IsChecked == true;

    void DlssBackClick(object sender, RoutedEventArgs e)
    {
        dlssCancel?.Cancel();
        RefreshDlss();
        ShowPage(dlssFrom ?? FilesPage);
    }
}
