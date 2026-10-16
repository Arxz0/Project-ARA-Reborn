// View model of one content card.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace RebornSetup
{
    public enum ModAction { None, Install, Update, Remove }

    public sealed class ModItem : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        void On(params string[] names) { foreach (var n in names) PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n)); }

        public ModDef Def { get; }
        public ModItem(ModDef d) { Def = d; }

        public static readonly Dictionary<string, string> Icons = new Dictionary<string, string>
        {
            ["AraQoL"] = "M3,6 L13,6 M17,6 L21,6 M3,12 L6,12 M10,12 L21,12 M3,18 L14,18 M18,18 L21,18 M15,4 L15,8 M8,10 L8,14 M16,16 L16,20",
            ["CpuFix"] = "M7,7 L17,7 L17,17 L7,17 Z M10,10 L14,10 L14,14 L10,14 Z M9.5,3 L9.5,7 M14.5,3 L14.5,7 M9.5,17 L9.5,21 M14.5,17 L14.5,21 M3,9.5 L7,9.5 M3,14.5 L7,14.5 M17,9.5 L21,9.5 M17,14.5 L21,14.5",
            ["Railroads"] = "M10,2 L5,22 M14,2 L19,22 M9.2,5 L14.8,5 M8.4,9 L15.6,9 M7.4,13 L16.6,13 M6.4,17 L17.6,17 M5.4,21 L18.6,21",
        };
        static readonly Dictionary<string, string> Images = new Dictionary<string, string>
        {
            ["Brazil"] = "getulio_sq.png",
            ["SovietUnion"] = "stalin_sq.png",
        };

        public static ImageSource Asset(string file) => new BitmapImage(new Uri("pack://application:,,,/Assets/" + file));

        public string Title => L.T(Def.Key + ".name");
        public string VersionText => "v" + Def.Version;
        public string Description => L.T(Def.Key + ".desc");
        public Geometry Icon => Icons.TryGetValue(Def.Key, out var g) ? Geometry.Parse(g) : null;
        public ImageSource Image => Images.TryGetValue(Def.Key, out var f) ? Asset(f) : null;
        public Visibility IconVisibility => Images.ContainsKey(Def.Key) ? Visibility.Collapsed : Visibility.Visible;
        public Visibility ImageVisibility => Images.ContainsKey(Def.Key) ? Visibility.Visible : Visibility.Collapsed;
        public string SizeText => (Def.Size < 1048576 ? Math.Max(1, Def.Size / 1024) + " KB" : L.F("mb", Def.Size / 1048576.0)) + (Def.Kind == ModKind.Leader ? "  " + L.T("temp_space") : "");

        public IEnumerable<string> Tags
        {
            get
            {
                if (Def.Kind == ModKind.Dll) yield return L.T("tag_dll");
                else
                {
                    yield return L.T("tag_dll");
                    yield return L.T("tag_content");
                }
                if (Def.Kind == ModKind.Leader) { yield return L.T("tag_narration"); yield return L.T("tag_ptbr"); }
            }
        }

        // ---- railroads option ----
        public bool PythonFound;
        bool _opt;
        public bool OptionSelected { get => _opt; set { _opt = value; On(nameof(OptionSelected)); } }
        public bool OptionEnabled => PythonFound && Selected;
        public Visibility OptionVisibility => Def.Kind == ModKind.Rail ? Visibility.Visible : Visibility.Collapsed;
        public string OptionText => L.T(PythonFound ? "opt_station" : "opt_station_nopy");

        // ---- state ----
        bool _installed, _sel;
        string _instVer;
        public bool Installed { get => _installed; set { _installed = value; Changed(); } }
        public string InstalledVersion { get => _instVer; set { _instVer = value; Changed(); } }
        public bool Selected { get => _sel; set { _sel = value; Changed(); SelectionChanged?.Invoke(); } }
        public event Action SelectionChanged;

        public ModAction Action
        {
            get
            {
                if (Installed && !Selected) return ModAction.Remove;
                if (!Installed && Selected) return ModAction.Install;
                if (Installed && Selected && InstalledVersion != null && InstalledVersion != Def.Version) return ModAction.Update;
                return ModAction.None;
            }
        }

        public string StatusText
        {
            get
            {
                switch (Action)
                {
                    case ModAction.Install: return L.T("st_will_install");
                    case ModAction.Update: return L.T("st_will_update");
                    case ModAction.Remove: return L.T("st_will_remove");
                }
                if (Installed) return L.T(InstalledVersion != null && InstalledVersion != Def.Version ? "st_update" : "st_installed");
                return L.T("st_new");
            }
        }

        public Brush StatusBrush
        {
            get
            {
                switch (Action)
                {
                    case ModAction.Install: case ModAction.Update: return (Brush)Application.Current.Resources["GoldLight"];
                    case ModAction.Remove: return (Brush)Application.Current.Resources["Bad"];
                }
                return Installed ? (Brush)Application.Current.Resources["Ok"] : (Brush)Application.Current.Resources["Muted"];
            }
        }

        public Brush CardBorder => Selected ? new SolidColorBrush(Color.FromArgb(0x70, 0xD4, 0xA6, 0x57)) : new SolidColorBrush(Color.FromArgb(0x20, 0xFF, 0xFF, 0xFF));

        void Changed() => On(nameof(Action), nameof(StatusText), nameof(StatusBrush), nameof(CardBorder), nameof(OptionEnabled));

        public void RefreshTexts() => On(nameof(Title), nameof(Description), nameof(Tags), nameof(OptionText), nameof(SizeText), nameof(StatusText));
    }
}
