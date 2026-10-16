using System;
using System.Collections.Generic;
using System.Diagnostics;
using Path = System.Windows.Shapes.Path;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace RebornSetup
{
    public partial class MainWindow : Window
    {
        enum Page { Welcome, License, Content, Location, Progress, Finish }
        Page page = Page.Welcome;
        readonly List<ModItem> items;
        bool maintenance;            // something is already installed: no license page, the welcome page offers Modify / Uninstall
        bool busy, uninstallAll;
        string game;
        readonly DispatcherTimer slideTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(7) };
        readonly DispatcherTimer creepTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        int slideIndex;

        public MainWindow()
        {
            InitializeComponent();
            Engine.Log("==== Project ARA: Reborn Setup " + Engine.PackVersion + " ====");
            var argv = Environment.GetCommandLineArgs();
            var oi = Array.FindIndex(argv, a => a.Equals("--only", StringComparison.OrdinalIgnoreCase));   // --only Key1,Key2 (testing)
            var only = oi > 0 && oi + 1 < argv.Length ? argv[oi + 1].Split(',') : null;
            items = Engine.LoadCatalog().Where(d => only == null || only.Contains(d.Key)).Select(d => new ModItem(d)).ToList();
            var py = Engine.PythonAvailable();
            foreach (var it in items) { it.PythonFound = py; it.SelectionChanged += OnSelectionChanged; }
            ModList.ItemsSource = items;

            var args = Environment.GetCommandLineArgs();
            var gi = Array.FindIndex(args, a => a.Equals("--game", StringComparison.OrdinalIgnoreCase));
            var last = gi > 0 && gi + 1 < args.Length ? args[gi + 1] : Engine.LastGamePath;     // --game "<folder>" overrides detection
            game = Engine.IsGameDir(last) ? last : Engine.FindGame();
            GamePathBox.Text = game ?? "";
            Detect();
            maintenance = items.Any(i => i.Installed);
            foreach (var it in items)
            {
                it.Selected = maintenance ? it.Installed : true;     // first install: everything on
                it.OptionSelected = py;
            }

            slideTimer.Tick += (s, e) => ShowSlide(slideIndex + 1);
            creepTimer.Tick += (s, e) => Creep();
            ApplyTexts();
            Loaded += (s, e) => AnimateWelcome();
            var si = Array.FindIndex(argv, a => a.Equals("--shots", StringComparison.OrdinalIgnoreCase));    // --shots <dir> (testing)
            if (si > 0 && si + 1 < argv.Length) Loaded += async (s, e) => await Shots(argv[si + 1]);
            if (Environment.GetCommandLineArgs().Any(a => a.Equals("--uninstall", StringComparison.OrdinalIgnoreCase)) && maintenance)
                Dispatcher.BeginInvoke(new Action(() => BtnUninstallAll_Click(null, null)), DispatcherPriority.ApplicationIdle);
        }

        void Detect()
        {
            foreach (var it in items)
            {
                it.Installed = Engine.IsInstalled(it.Def, game);
                it.InstalledVersion = it.Installed ? Engine.InstalledVersion(it.Def.Key) : null;
            }
        }

        // ---------------- texts ----------------
        void ApplyTexts()
        {
            LangText.Text = L.Pt ? "PT-BR" : "EN";
            WSubtitle.Text = L.T("subtitle").ToUpperInvariant();
            WTagline.Text = L.T("tagline");
            BtnStart.Content = L.T(maintenance ? "modify" : "install");
            BtnUninstallAll.Content = L.T("uninstallAll");
            BtnUninstallAll.Visibility = maintenance ? Visibility.Visible : Visibility.Collapsed;
            WStatusRow.Visibility = maintenance ? Visibility.Visible : Visibility.Collapsed;
            WStatus.Text = L.F("installed_n", items.Count(i => i.Installed), items.Count);
            WVersion.Text = "v" + Engine.PackVersion;
            WDisclaimer.Text = L.T("disclaimer");
            LicenseText.Text = L.License;
            ChkAcceptText.Text = L.T("license_accept");
            BtnBrowse.Content = L.T("browse");
            SummaryTitle.Text = L.T("summary");
            BtnLog.Content = L.T("open_log");
            foreach (var it in items) it.RefreshTexts();
            if (page != Page.Welcome) ShowPage(page);
        }

        void BtnLang_Click(object sender, RoutedEventArgs e) { L.Pt = !L.Pt; ApplyTexts(); if (page == Page.Progress) ShowSlide(slideIndex); }

        // ---------------- navigation ----------------
        Page[] Flow => maintenance ? new[] { Page.Content, Page.Location, Page.Progress, Page.Finish }
                                   : new[] { Page.License, Page.Content, Page.Location, Page.Progress, Page.Finish };

        void ShowPage(Page p)
        {
            var first = page == Page.Welcome && p != Page.Welcome;
            page = p;
            WelcomePage.Visibility = p == Page.Welcome ? Visibility.Visible : Visibility.Collapsed;
            WizardPage.Visibility = p == Page.Welcome ? Visibility.Collapsed : Visibility.Visible;
            if (p == Page.Welcome) { ApplyTexts(); return; }

            PgLicense.Visibility = Vis(p == Page.License);
            PgContent.Visibility = Vis(p == Page.Content);
            PgLocation.Visibility = Vis(p == Page.Location);
            PgProgress.Visibility = Vis(p == Page.Progress);
            PgFinish.Visibility = Vis(p == Page.Finish);

            switch (p)
            {
                case Page.License: PageTitle.Text = L.T("license_title"); PageSub.Text = L.T("license_sub"); break;
                case Page.Content: PageTitle.Text = L.T("content_title"); PageSub.Text = L.T("content_sub"); break;
                case Page.Location: PageTitle.Text = L.T("location_title"); PageSub.Text = L.T("location_sub"); ValidateLocation(); break;
                case Page.Progress:
                    PageTitle.Text = L.T(items.Any(i => i.Action == ModAction.Remove) ? "working_title_rm" : "working_title");
                    PageSub.Text = L.T("working_sub"); break;
            }
            BuildSteps();
            Refresh(null, null);
            if (first) FadeIn(WizardPage);
            else FadeIn(p == Page.License ? (UIElement)PgLicense : p == Page.Content ? PgContent : p == Page.Location ? PgLocation : p == Page.Progress ? PgProgress : (UIElement)PgFinish);
        }

        static Visibility Vis(bool b) => b ? Visibility.Visible : Visibility.Collapsed;

        void BuildSteps()
        {
            Steps.Children.Clear();
            var names = new Dictionary<Page, string> { [Page.License] = "step_license", [Page.Content] = "step_content", [Page.Location] = "step_location", [Page.Progress] = "step_install", [Page.Finish] = "step_finish" };
            var flow = Flow; var cur = Array.IndexOf(flow, page);
            for (int i = 0; i < flow.Length; i++)
            {
                var done = i < cur; var active = i == cur;
                var sp = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(i == 0 ? 0 : 14, 0, 0, 0) };
                var g = new Grid { Width = 22, Height = 22 };
                g.Children.Add(new Ellipse
                {
                    Fill = active ? (Brush)FindResource("GoldGrad") : done ? new SolidColorBrush(Color.FromArgb(0x40, 0xD4, 0xA6, 0x57)) : new SolidColorBrush(Color.FromArgb(0x18, 0xFF, 0xFF, 0xFF)),
                    Stroke = active || done ? (Brush)FindResource("Gold") : new SolidColorBrush(Color.FromArgb(0x40, 0xFF, 0xFF, 0xFF)), StrokeThickness = 1
                });
                if (done) g.Children.Add(new Path { Data = Geometry.Parse("M6.5,11 L9.8,14.2 L15.5,8"), Stroke = (Brush)FindResource("GoldLight"), StrokeThickness = 1.8, StrokeEndLineCap = PenLineCap.Round, StrokeStartLineCap = PenLineCap.Round });
                else g.Children.Add(new TextBlock { Text = (i + 1).ToString(), FontSize = 11, FontWeight = FontWeights.SemiBold, Foreground = active ? new SolidColorBrush(Color.FromRgb(0x1A, 0x13, 0x07)) : (Brush)FindResource("Muted"), HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center });
                sp.Children.Add(g);
                sp.Children.Add(new TextBlock { Text = L.T(names[flow[i]]), FontSize = 12.5, Margin = new Thickness(7, 0, 0, 0), VerticalAlignment = VerticalAlignment.Center, Foreground = active ? (Brush)FindResource("Text") : (Brush)FindResource("Muted") });
                if (i < flow.Length - 1) sp.Children.Add(new Rectangle { Width = 18, Height = 1, Fill = (Brush)FindResource("Line"), Margin = new Thickness(14, 0, 0, 0), VerticalAlignment = VerticalAlignment.Center });
                Steps.Children.Add(sp);
            }
        }

        void Refresh(object sender, RoutedEventArgs e)
        {
            if (page == Page.Welcome || WizardPage == null) return;
            BtnLog.Visibility = Visibility.Collapsed;
            FooterInfo.Text = "";
            BtnBack.Visibility = Visibility.Visible; BtnNext.Visibility = Visibility.Visible;
            BtnBack.Content = L.T("back"); BtnBack.IsEnabled = true;
            switch (page)
            {
                case Page.License:
                    BtnNext.Content = L.T("next"); BtnNext.IsEnabled = ChkAccept.IsChecked == true; break;
                case Page.Content:
                    BtnNext.Content = L.T("next");
                    BtnNext.IsEnabled = maintenance ? items.Any(i => i.Action != ModAction.None) : items.Any(i => i.Selected);
                    var mb = items.Where(i => i.Action == ModAction.Install || i.Action == ModAction.Update).Sum(i => i.Def.Size) / 1048576.0;
                    FooterInfo.Text = items.Any(i => i.Action != ModAction.None) ? SummaryLine() : L.T("sum_nothing");
                    break;
                case Page.Location:
                    var anyInstall = items.Any(i => i.Action == ModAction.Install || i.Action == ModAction.Update);
                    BtnNext.Content = L.T(anyInstall ? (maintenance ? "apply" : "install_now") : (uninstallAll ? "uninstall_now" : "apply"));
                    BtnNext.IsEnabled = locationOk && items.Any(i => i.Action != ModAction.None);
                    break;
                case Page.Progress:
                    BtnBack.Visibility = Visibility.Collapsed; BtnNext.Visibility = Visibility.Collapsed;
                    BtnLog.Visibility = Visibility.Visible; BtnLog.Content = L.T("details"); break;
                case Page.Finish:
                    BtnLog.Visibility = Visibility.Visible; BtnLog.Content = L.T("open_log");
                    BtnBack.Content = L.T("close");
                    BtnNext.Content = L.T("play");
                    BtnNext.Visibility = Vis(finishedOk && items.Any(i => i.Installed));
                    break;
            }
        }

        string SummaryLine()
        {
            int a = items.Count(i => i.Action == ModAction.Install), u = items.Count(i => i.Action == ModAction.Update), r = items.Count(i => i.Action == ModAction.Remove);
            var parts = new List<string>();
            if (a > 0) parts.Add(L.T("sum_install") + ": " + a);
            if (u > 0) parts.Add(L.T("sum_update") + ": " + u);
            if (r > 0) parts.Add(L.T("sum_remove") + ": " + r);
            return string.Join("   •   ", parts);
        }

        void OnSelectionChanged() { if (page == Page.Content) Refresh(null, null); }

        void BtnStart_Click(object sender, RoutedEventArgs e) { uninstallAll = false; ShowPage(Flow[0]); }

        void BtnUninstallAll_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBox.Show(this, L.T("confirm_uninstall"), L.T("err_title"), MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            uninstallAll = true;
            foreach (var it in items) it.Selected = false;
            ShowPage(Page.Location);
        }

        void BtnBack_Click(object sender, RoutedEventArgs e)
        {
            if (page == Page.Finish) { Close(); return; }
            var flow = Flow; var i = Array.IndexOf(flow, page);
            if (page == Page.Location && uninstallAll) { uninstallAll = false; foreach (var it in items) it.Selected = it.Installed; ShowPage(Page.Welcome); return; }
            ShowPage(i <= 0 ? Page.Welcome : flow[i - 1]);
        }

        async void BtnNext_Click(object sender, RoutedEventArgs e)
        {
            if (page == Page.Finish)
            {
                try { Process.Start(new ProcessStartInfo("steam://rungameid/" + Engine.SteamAppId) { UseShellExecute = true }); } catch { }
                Close(); return;
            }
            if (page == Page.Location) { await RunAll(); return; }
            var flow = Flow; ShowPage(flow[Array.IndexOf(flow, page) + 1]);
        }

        // ---------------- location ----------------
        bool locationOk;

        void GamePathBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded) return;
            var p = GamePathBox.Text.Trim().Trim('"');
            if (Engine.IsGameDir(p) && !string.Equals(p, game, StringComparison.OrdinalIgnoreCase))
            {
                game = p;
                Detect();
            }
            else if (!Engine.IsGameDir(p)) game = null;
            if (page == Page.Location) { ValidateLocation(); Refresh(null, null); }
        }

        void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            using (var d = new System.Windows.Forms.FolderBrowserDialog { Description = L.T("pick_folder"), ShowNewFolderButton = false })
            {
                if (Engine.IsGameDir(GamePathBox.Text)) d.SelectedPath = GamePathBox.Text;
                if (d.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                var p = d.SelectedPath;
                // accept the bin folder or the exe's parent too
                if (!Engine.IsGameDir(p) && Engine.IsGameDir(System.IO.Path.GetDirectoryName(p) ?? "")) p = System.IO.Path.GetDirectoryName(p);
                GamePathBox.Text = p;
            }
        }

        void ValidateLocation()
        {
            var ok = Engine.IsGameDir(game);
            var okBrush = (Brush)FindResource("Ok"); var bad = (Brush)FindResource("Bad");
            GameIcon.Data = Geometry.Parse(ok ? "M2,8 L6,12 L14,3" : "M3,3 L13,13 M13,3 L3,13");
            GameIcon.Stroke = ok ? okBrush : bad;
            GameStatus.Text = ok ? L.T("game_found") : L.T("game_missing");
            GameStatus.Foreground = ok ? (Brush)FindResource("Text") : bad;

            var spaceOk = true;
            SpaceStatus.Text = "";
            if (ok)
            {
                var drive = System.IO.Path.GetPathRoot(game).TrimEnd('\\');
                var free = Engine.FreeBytes(game);
                var needBase = items.Any(i => i.Def.Kind == ModKind.Leader && (i.Action == ModAction.Install || i.Action == ModAction.Update));
                spaceOk = !needBase || free >= 4.5 * 1024 * 1024 * 1024;
                SpaceStatus.Text = spaceOk ? L.F("space_ok", drive, free / 1073741824.0) : L.F("space_need", drive, free / 1073741824.0);
                SpaceStatus.Foreground = spaceOk ? (Brush)FindResource("Muted") : bad;
            }
            var running = Engine.GameRunning();
            RunningBox.Visibility = Vis(running);
            RunningText.Text = L.T("game_running");

            SummaryList.Children.Clear();
            var acts = items.Where(i => i.Action != ModAction.None).ToList();
            if (acts.Count == 0) SummaryList.Children.Add(new TextBlock { Text = L.T("sum_nothing"), Foreground = (Brush)FindResource("Muted") });
            foreach (var it in acts)
            {
                var row = new DockPanel { Margin = new Thickness(0, 4, 0, 4) };
                var label = it.Action == ModAction.Install ? "sum_install" : it.Action == ModAction.Update ? "sum_update" : "sum_remove";
                var tag = new TextBlock
                {
                    Text = L.T(label), Width = 110, FontSize = 13, FontWeight = FontWeights.SemiBold,
                    Foreground = it.Action == ModAction.Remove ? bad : (Brush)FindResource("GoldLight")
                };
                row.Children.Add(tag);
                row.Children.Add(new TextBlock { Text = it.Title + "  v" + it.Def.Version, FontSize = 13.5 });
                SummaryList.Children.Add(row);
            }
            locationOk = ok && spaceOk && !running;
        }

        // ---------------- install ----------------
        double baseFrac, sliceFrac, sub, subCap;
        bool finishedOk;

        async Task RunAll()
        {
            if (Engine.GameRunning()) { ValidateLocation(); Refresh(null, null); return; }
            Engine.LastGamePath = game;
            // installs first (a second nation reuses the leader base), removals last
            var plan = items.Where(i => i.Action == ModAction.Install || i.Action == ModAction.Update)
                            .Concat(items.Where(i => i.Action == ModAction.Remove)).Select(i => (item: i, install: i.Action != ModAction.Remove)).ToList();
            var weights = plan.Select(p => Weight(p.item, p.install)).ToList();
            var total = weights.Sum();

            busy = true;
            LogText.Text = "";
            LogBox.Visibility = Visibility.Collapsed; Slide.Visibility = Visibility.Visible;
            ShowPage(Page.Progress);
            ShowSlide(0); slideTimer.Start();
            Prog.Value = 0; baseFrac = 0;
            creepTimer.Start();
            var results = new List<(ModItem item, bool install, Engine.Result res)>();
            double done = 0;
            for (int k = 0; k < plan.Count; k++)
            {
                var (it, install) = plan[k];
                ProgTitle.Text = L.F(install ? "installing" : "removing", it.Title);
                ProgStep.Text = L.F("step_of", k + 1, plan.Count);
                baseFrac = done / total; sliceFrac = weights[k] / total; sub = 0; subCap = 0.9;
                var r = await Engine.RunAsync(it.Def, install, game, it.OptionSelected && it.OptionEnabled, line => Dispatcher.BeginInvoke(new Action(() => OnLine(line))));
                results.Add((it, install, r));
                if (!r.Ok) AppendLog("ERROR: " + r.Error);
                done += weights[k];
                SetProgress(done / total);
            }
            creepTimer.Stop(); slideTimer.Stop();
            SetProgress(1);
            Detect();
            busy = false;

            finishedOk = results.All(r => r.res.Ok);
            if (items.Any(i => i.Installed)) Engine.Register(items.Where(i => i.Installed).Sum(i => i.Def.Size) / 1024);
            else Engine.Unregister();
            foreach (var it in items) it.Selected = it.Installed;
            ShowFinish(results);
        }

        static double Weight(ModItem it, bool install)
        {
            if (it.Def.Kind == ModKind.Leader) return install ? 12 : 6;
            if (it.Def.Kind == ModKind.Rail && install && it.OptionSelected && it.OptionEnabled) return 4;
            return 1;
        }

        static readonly Regex StepRx = new Regex(@"^\[(\d+)/(\d+)\]");
        void OnLine(string line)
        {
            AppendLog(line);
            ProgLine.Text = line.Trim();
            var m = StepRx.Match(line);
            if (m.Success)
            {
                double i = int.Parse(m.Groups[1].Value), n = int.Parse(m.Groups[2].Value);
                sub = Math.Max(sub, 0.9 * i / n); subCap = Math.Min(0.95, 0.9 * (i + 1) / n);
            }
        }

        void Creep()
        {
            sub += (subCap - sub) * 0.012;
            SetProgress(baseFrac + sliceFrac * sub);
        }

        void SetProgress(double f)
        {
            f = Math.Max(0, Math.Min(1, f));
            if (f * 1000 < Prog.Value && f < 1) return;
            Prog.BeginAnimation(System.Windows.Controls.Primitives.RangeBase.ValueProperty, new DoubleAnimation(f * 1000, TimeSpan.FromMilliseconds(250)));
            ProgPct.Text = ((int)Math.Round(f * 100)) + "%";
        }

        void AppendLog(string s) { LogText.AppendText(s + Environment.NewLine); LogText.ScrollToEnd(); }

        void BtnLog_Click(object sender, RoutedEventArgs e)
        {
            if (page == Page.Progress)
            {
                var show = LogBox.Visibility != Visibility.Visible;
                LogBox.Visibility = Vis(show); Slide.Visibility = Vis(!show);
                return;
            }
            try { Process.Start(new ProcessStartInfo("notepad.exe", "\"" + Engine.LogPath + "\"") { UseShellExecute = true }); } catch { }
        }

        void ShowFinish(List<(ModItem item, bool install, Engine.Result res)> results)
        {
            ShowPage(Page.Finish);
            var removedAll = results.All(r => !r.install) && !items.Any(i => i.Installed);
            DoneTitle.Text = L.T(!finishedOk ? "done_title_err" : removedAll ? "done_title_rm" : "done_title");
            DoneSub.Text = L.T(!finishedOk ? "done_sub_err" : removedAll ? "done_sub_rm" : "done_sub");
            PageTitle.Text = L.T("step_finish"); PageSub.Text = "Project ARA: Reborn v" + Engine.PackVersion;
            var ok = (Brush)FindResource("Ok"); var bad = (Brush)FindResource("Bad"); var warn = (Brush)FindResource("Warn");
            DoneCircle.Stroke = finishedOk ? ok : bad; DoneMark.Stroke = finishedOk ? ok : bad;
            DoneCircle.Fill = new SolidColorBrush(finishedOk ? Color.FromArgb(0x22, 0x4C, 0xC3, 0x8A) : Color.FromArgb(0x22, 0xEF, 0x6B, 0x60));
            DoneMark.Data = Geometry.Parse(finishedOk ? "M15,28 L24,36 L40,19" : "M19,19 L35,35 M35,19 L19,35");
            DoneArt.Source = ModItem.Asset(removedAll ? "stalin1924.png" : "vargas.png");

            ResultList.Children.Clear();
            foreach (var (it, install, res) in results)
            {
                var row = new Grid { Margin = new Thickness(0, 5, 0, 5) };
                row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(26) });
                row.ColumnDefinitions.Add(new ColumnDefinition());
                row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                var icon = new Path
                {
                    Data = Geometry.Parse(res.Ok ? "M2,8 L6,12 L14,3" : "M3,3 L13,13 M13,3 L3,13"), Stroke = res.Ok ? ok : bad, StrokeThickness = 2.2,
                    Width = 14, Height = 14, Stretch = Stretch.Uniform, StrokeStartLineCap = PenLineCap.Round, StrokeEndLineCap = PenLineCap.Round, VerticalAlignment = VerticalAlignment.Top, Margin = new Thickness(0, 3, 0, 0)
                };
                row.Children.Add(icon);
                var txt = new StackPanel(); Grid.SetColumn(txt, 1);
                txt.Children.Add(new TextBlock { Text = it.Title + "  v" + it.Def.Version, FontSize = 14 });
                if (!res.Ok) txt.Children.Add(new TextBlock { Text = res.Error, Foreground = bad, FontSize = 12.5, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 2, 0, 0) });
                foreach (var w in res.Warnings) txt.Children.Add(new TextBlock { Text = w, Foreground = warn, FontSize = 12.5, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 2, 0, 0) });
                row.Children.Add(txt);
                var st = new TextBlock { Text = L.T(!res.Ok ? "failed" : install ? "ok_installed" : "ok_removed"), Foreground = res.Ok ? (Brush)FindResource("Muted") : bad, FontSize = 12.5, Margin = new Thickness(12, 2, 0, 0) };
                Grid.SetColumn(st, 2); row.Children.Add(st);
                ResultList.Children.Add(row);
            }

            Tips.Children.Clear();
            if (results.Any(r => r.install && r.res.Ok && r.item.Def.Kind != ModKind.Dll)) Tips.Children.Add(Tip(L.T("tip_newgame")));
            if (items.Any(i => i.Installed)) Tips.Children.Add(Tip(L.T("tip_verify")));
            Refresh(null, null);
        }

        UIElement Tip(string text)
        {
            var sp = new DockPanel { Margin = new Thickness(0, 0, 0, 10) };
            sp.Children.Add(new Border { Width = 3, CornerRadius = new CornerRadius(1.5), Background = (Brush)FindResource("Gold"), Margin = new Thickness(0, 2, 12, 2) });
            sp.Children.Add(new TextBlock { Text = text, Foreground = (Brush)FindResource("Muted"), FontSize = 13, TextWrapping = TextWrapping.Wrap, LineHeight = 19 });
            return sp;
        }

        // ---------------- slideshow ----------------
        void ShowSlide(int i)
        {
            slideIndex = ((i % 4) + 4) % 4;
            var n = slideIndex + 1;
            SlideTitle.Text = L.T("slide" + n + ".t");
            SlideText.Text = L.T("slide" + n + ".d");
            SlideArt.Children.Clear();
            switch (n)
            {
                case 1:
                    SlideArt.Children.Add(Pic("br_emblem.png", 150, -70, 20));
                    SlideArt.Children.Add(Pic("su_emblem.png", 150, 80, 50));
                    break;
                case 2: SlideArt.Children.Add(Pic("estadonovo.png", 210, 0, 10)); break;
                case 3: SlideArt.Children.Add(Glyph(ModItem.Icons["Railroads"], 170)); break;
                case 4:
                    SlideArt.Children.Add(Glyph(ModItem.Icons["AraQoL"], 110, -70));
                    SlideArt.Children.Add(Glyph(ModItem.Icons["CpuFix"], 110, 70));
                    break;
            }
            var sb = new Storyboard();
            foreach (UIElement el in new UIElement[] { SlideTitle, SlideText, SlideArt })
            {
                el.Opacity = 0;
                var a = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(500)) { BeginTime = TimeSpan.FromMilliseconds(el == SlideTitle ? 0 : el == SlideText ? 120 : 200) };
                Storyboard.SetTarget(a, el); Storyboard.SetTargetProperty(a, new PropertyPath(OpacityProperty)); sb.Children.Add(a);
            }
            sb.Begin();
        }

        UIElement Pic(string file, double size, double dx, double dy)
        {
            var b = new Border
            {
                Width = size, Height = size, CornerRadius = new CornerRadius(14), BorderBrush = new SolidColorBrush(Color.FromArgb(0x60, 0xD4, 0xA6, 0x57)), BorderThickness = new Thickness(1),
                Background = new ImageBrush(ModItem.Asset(file)) { Stretch = Stretch.UniformToFill },
                RenderTransform = new TranslateTransform(dx, dy),
                Effect = new System.Windows.Media.Effects.DropShadowEffect { BlurRadius = 30, ShadowDepth = 6, Opacity = 0.6 },
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center
            };
            return b;
        }

        UIElement Glyph(string data, double size, double dx = 0)
        {
            return new Path
            {
                Data = Geometry.Parse(data), Stroke = (Brush)FindResource("GoldGrad"), StrokeThickness = 1.4, Width = size, Height = size, Stretch = Stretch.Uniform,
                StrokeStartLineCap = PenLineCap.Round, StrokeEndLineCap = PenLineCap.Round, StrokeLineJoin = PenLineJoin.Round,
                RenderTransform = new TranslateTransform(dx, 0), HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
                Effect = new System.Windows.Media.Effects.DropShadowEffect { BlurRadius = 24, ShadowDepth = 0, Color = Color.FromRgb(0xC9, 0x97, 0x3F), Opacity = 0.5 }
            };
        }

        // ---------------- animation helpers ----------------
        static void FadeIn(UIElement el)
        {
            el.BeginAnimation(OpacityProperty, new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(280)));
        }

        void AnimateWelcome()
        {
            var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
            void Slide(FrameworkElement el, double fromX, int delay, double toOpacity = 1)
            {
                var tt = el.RenderTransform as TranslateTransform; if (tt == null) return;
                el.Opacity = 0;
                tt.BeginAnimation(TranslateTransform.XProperty, new DoubleAnimation(fromX, 0, TimeSpan.FromMilliseconds(900)) { EasingFunction = ease, BeginTime = TimeSpan.FromMilliseconds(delay) });
                el.BeginAnimation(OpacityProperty, new DoubleAnimation(0, toOpacity, TimeSpan.FromMilliseconds(900)) { EasingFunction = ease, BeginTime = TimeSpan.FromMilliseconds(delay) });
            }
            Slide(WelcomeText, -30, 100);
            Slide(LeaderBack, 40, 250, 0.92);
            Slide(LeaderFront, 60, 400);
        }

        // ---------------- test screenshots (--shots <dir>): walks the real UI and saves each page ----------------
        async Task Shots(string dir)
        {
            System.IO.Directory.CreateDirectory(dir);
            async Task Snap(string name, int wait = 1300)
            {
                await Task.Delay(wait);
                Engine.Log("shot " + name + " layoutValid=" + (Root.IsMeasureValid && Root.IsArrangeValid));
                var dpi = VisualTreeHelper.GetDpi(this);
                var bmp = new System.Windows.Media.Imaging.RenderTargetBitmap((int)(ActualWidth * dpi.DpiScaleX), (int)(ActualHeight * dpi.DpiScaleY), 96 * dpi.DpiScaleX, 96 * dpi.DpiScaleY, PixelFormats.Pbgra32);
                bmp.Render(Root);
                var enc = new System.Windows.Media.Imaging.PngBitmapEncoder(); enc.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bmp));
                using (var f = System.IO.File.Create(System.IO.Path.Combine(dir, name + ".png"))) enc.Save(f);
            }
            await Snap("1_welcome");
            if (!maintenance)
            {
                BtnStart_Click(null, null); await Snap("2_license");
                ChkAccept.IsChecked = true;
                BtnNext_Click(null, null);
            }
            else BtnStart_Click(null, null);
            await Snap("3_content");
            if (maintenance) foreach (var it in items) it.Selected = false;
            BtnNext_Click(null, null); await Snap("4_location");
            var run = RunAll();
            await Snap("5_progress", 900);
            await run;
            await Snap("6_finish");
            ShowPage(Page.Progress); ProgTitle.Text = L.F("installing", items[0].Title); ProgStep.Text = L.F("step_of", 1, 2); SetProgress(0.42); ShowSlide(0);
            await Snap("5b_progress");
            ShowSlide(1); await Snap("5c_slide2"); ShowSlide(2); await Snap("5d_slide3"); ShowSlide(3); await Snap("5e_slide4");
            busy = false; Close();
        }

        // ---------------- title bar ----------------
        void BtnMin_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
        void BtnClose_Click(object sender, RoutedEventArgs e) => Close();

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            if (busy && MessageBox.Show(this, L.T("confirm_close"), L.T("err_title"), MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) e.Cancel = true;
            base.OnClosing(e);
        }
    }
}
