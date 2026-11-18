using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32.SafeHandles;
using Microsoft.Win32;

[assembly: AssemblyTitle("Pitfall Modified Setup")]
[assembly: AssemblyDescription("Installer and uninstaller for Pitfall: The Mayan Adventure — Modified")]
[assembly: AssemblyCompany("Pitfall Modified Project")]
[assembly: AssemblyProduct("Pitfall Modified")]
[assembly: AssemblyCopyright("Copyright © 2026 Pitfall Modified Project")]
[assembly: AssemblyVersion("0.3.3.0")]
[assembly: AssemblyFileVersion("0.3.3.0")]
[assembly: AssemblyInformationalVersion("0.3.3")]

namespace PitfallModifiedInstaller
{
    internal sealed class InstallerForm : Form
    {
        private const string OriginalHash = "5055FACE831286FBE3C30D9E78681DE0FCE819736119035EEBCA5A51E5A9239D";
        private const string InstallMarker = "PitfallModifiedInstaller-2026-v1";
        private const string UninstallRegistryPath = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\PitfallModified";
        private readonly TextBox sourceBox = new TextBox();
        private readonly Label cdStatus = new Label();
        private readonly TextBox destinationBox = new TextBox();
        private readonly ComboBox audioBox = new ComboBox();
        private readonly Label musicLabel = new Label();
        private readonly TextBox musicBox = new TextBox();
        private readonly Button musicBrowse = new Button();
        private readonly CheckBox desktopShortcut = new CheckBox();
        private readonly CheckBox startShortcut = new CheckBox();
        private readonly TextBox logBox = new TextBox();
        private readonly Button installButton = new Button();
        private readonly Button closeButton = new Button();
        private bool installationRunning;
        private string detectedCdDrive;
        private bool detectedCdCanExtract;

        private const uint GenericRead = 0x80000000;
        private const uint FileShareRead = 0x00000001;
        private const uint FileShareWrite = 0x00000002;
        private const uint OpenExisting = 3;
        private const uint IoctlCdromReadToc = 0x00024000;
        private const uint IoctlCdromRawRead = 0x0002403E;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern SafeFileHandle CreateFile(string name, uint access, uint share,
            IntPtr security, uint creation, uint flags, IntPtr template);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(SafeFileHandle device, uint control,
            IntPtr input, int inputSize, byte[] output, int outputSize,
            out int returned, IntPtr overlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(SafeFileHandle device, uint control,
            byte[] input, int inputSize, byte[] output, int outputSize,
            out int returned, IntPtr overlapped);

        public InstallerForm()
        {
            Text = "Pitfall Modified Installer";
            ClientSize = new Size(650, 572);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Segoe UI", 9F);

            Label title = NewLabel("Pitfall: The Mayan Adventure — Modified", 20, 14, 610, 28);
            title.Font = new Font(Font.FontFamily, 15F, FontStyle.Bold);
            Controls.Add(title);
            Controls.Add(NewLabel("Installs from an original CD or existing ASSETS folder. The source is never modified.", 22, 48, 610, 34));

            Controls.Add(NewLabel("Original game source", 20, 92, 200, 22));
            sourceBox.SetBounds(20, 116, 500, 25);
            Controls.Add(sourceBox);
            Button sourceBrowse = NewButton("Browse…", 530, 114, 100, 28);
            sourceBrowse.Click += delegate { BrowseSource(); };
            Controls.Add(sourceBrowse);
            Button detect = NewButton("Detect CD", 530, 146, 100, 28);
            detect.Click += delegate { DetectCd(true); };
            Controls.Add(detect);
            Controls.Add(NewLabel("Select the disc root, its ASSETS folder, or an existing local ASSETS folder.", 22, 144, 490, 22));
            cdStatus.SetBounds(22, 168, 600, 24);
            cdStatus.ForeColor = Color.DimGray;
            Controls.Add(cdStatus);

            Controls.Add(NewLabel("Install location", 20, 200, 200, 22));
            destinationBox.SetBounds(20, 224, 500, 25);
            destinationBox.Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "My Games", "Pitfall Modified");
            Controls.Add(destinationBox);
            Button destinationBrowse = NewButton("Browse…", 530, 222, 100, 28);
            destinationBrowse.Click += delegate { BrowseDestination(); };
            Controls.Add(destinationBrowse);
            Controls.Add(NewLabel("Game files will be installed in an ASSETS subfolder here.", 22, 252, 480, 22));

            Controls.Add(NewLabel("Music source", 20, 288, 200, 22));
            audioBox.SetBounds(20, 312, 250, 25);
            audioBox.DropDownStyle = ComboBoxStyle.DropDownList;
            audioBox.Items.AddRange(new object[] {
                "Automatic (files, then CD)", "Play from physical CD",
                "Copy music from physical CD", "Import BIN/CUE image",
                "Use existing WAV files", "No music"
            });
            audioBox.SelectedIndex = 0;
            audioBox.SelectedIndexChanged += delegate { UpdateMusicControls(); };
            Controls.Add(audioBox);

            musicLabel.Text = "Music source";
            musicLabel.SetBounds(290, 288, 150, 22);
            Controls.Add(musicLabel);
            musicBox.SetBounds(290, 312, 230, 25);
            Controls.Add(musicBox);
            musicBrowse.Text = "Browse…";
            musicBrowse.SetBounds(530, 310, 100, 28);
            musicBrowse.Click += delegate { BrowseMusic(); };
            Controls.Add(musicBrowse);

            desktopShortcut.Text = "Create desktop shortcut";
            desktopShortcut.SetBounds(20, 352, 210, 24);
            Controls.Add(desktopShortcut);
            startShortcut.Text = "Create Start Menu shortcut";
            startShortcut.SetBounds(250, 352, 220, 24);
            startShortcut.Checked = true;
            Controls.Add(startShortcut);

            logBox.SetBounds(20, 390, 610, 118);
            logBox.Multiline = true;
            logBox.ReadOnly = true;
            logBox.ScrollBars = ScrollBars.Vertical;
            logBox.BackColor = SystemColors.Window;
            Controls.Add(logBox);

            installButton.Text = "Install";
            installButton.SetBounds(420, 524, 100, 32);
            installButton.Click += delegate { Install(); };
            Controls.Add(installButton);
            closeButton.Text = "Close";
            closeButton.SetBounds(530, 524, 100, 32);
            closeButton.Click += delegate { Close(); };
            Controls.Add(closeButton);
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if (installationRunning) {
                    e.Cancel = true;
                    MessageBox.Show(this,
                        "Installation is still in progress. Please wait for it to finish.",
                        Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            };

            DetectCd(false);
            UpdateMusicControls();
        }

        private static Label NewLabel(string text, int x, int y, int width, int height)
        {
            Label label = new Label();
            label.Text = text;
            label.SetBounds(x, y, width, height);
            return label;
        }

        private static Button NewButton(string text, int x, int y, int width, int height)
        {
            Button button = new Button();
            button.Text = text;
            button.SetBounds(x, y, width, height);
            return button;
        }

        private void BrowseSource()
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog()) {
                dialog.Description = "Select the Pitfall CD, game folder, or ASSETS folder";
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    sourceBox.Text = dialog.SelectedPath;
            }
        }

        private void BrowseDestination()
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog()) {
                dialog.Description = "Choose where Pitfall Modified will be installed";
                dialog.SelectedPath = Directory.Exists(destinationBox.Text) ? destinationBox.Text : "";
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    destinationBox.Text = dialog.SelectedPath;
            }
        }

        private void BrowseMusic()
        {
            if (audioBox.SelectedIndex == 3) {
                using (OpenFileDialog dialog = new OpenFileDialog()) {
                    dialog.Title = "Select the mixed-mode Pitfall CUE file";
                    dialog.Filter = "CUE sheets (*.cue)|*.cue|All files (*.*)|*.*";
                    if (dialog.ShowDialog(this) == DialogResult.OK)
                        musicBox.Text = dialog.FileName;
                }
                return;
            }
            using (FolderBrowserDialog dialog = new FolderBrowserDialog()) {
                dialog.Description = "Select the folder containing track02.wav, track03.wav, and so on";
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    musicBox.Text = dialog.SelectedPath;
            }
        }

        private void DetectCd(bool reportFailure)
        {
            detectedCdDrive = null;
            detectedCdCanExtract = false;
            foreach (DriveInfo drive in DriveInfo.GetDrives()) {
                try {
                    if (drive.DriveType == DriveType.CDRom && drive.IsReady &&
                        File.Exists(Path.Combine(drive.RootDirectory.FullName, "ASSETS", "PITFALL.EXE"))) {
                        sourceBox.Text = drive.RootDirectory.FullName;
                        string reason;
                        detectedCdDrive = drive.Name.Substring(0, 2);
                        detectedCdCanExtract = ProbeCdda(detectedCdDrive, out reason);
                        if (detectedCdCanExtract) {
                            cdStatus.Text = "Pitfall CD detected in " + detectedCdDrive + " — digital audio extraction supported.";
                            cdStatus.ForeColor = Color.DarkGreen;
                        } else {
                            cdStatus.Text = "Pitfall CD detected in " + detectedCdDrive + " — playback only: " + reason;
                            cdStatus.ForeColor = Color.DarkOrange;
                        }
                        Log(cdStatus.Text);
                        UpdateMusicControls();
                        return;
                    }
                } catch { }
            }
            cdStatus.Text = "No mounted Pitfall CD was detected.";
            cdStatus.ForeColor = Color.DimGray;
            UpdateMusicControls();
            if (reportFailure)
                MessageBox.Show(this, "No mounted disc containing ASSETS\\PITFALL.EXE was found.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void UpdateMusicControls()
        {
            bool enabled = audioBox.SelectedIndex == 3 || audioBox.SelectedIndex == 4;
            musicLabel.Text = audioBox.SelectedIndex == 3 ? "CUE file" : "Music folder";
            musicLabel.Enabled = enabled;
            musicBox.Enabled = enabled;
            musicBrowse.Enabled = enabled;
            installButton.Enabled = audioBox.SelectedIndex != 2 || detectedCdCanExtract;
            if (audioBox.SelectedIndex == 2 && !detectedCdCanExtract)
                cdStatus.Text = detectedCdDrive == null
                    ? "Detect a Pitfall CD before selecting digital audio extraction."
                    : "This drive can play the disc but did not accept a raw CDDA test read.";
        }

        private string ResolveAssets(string selected)
        {
            if (String.IsNullOrWhiteSpace(selected)) throw new InvalidOperationException("Select the original game source.");
            string path = Path.GetFullPath(selected.Trim());
            if (File.Exists(Path.Combine(path, "PITFALL.EXE"))) return path;
            string assets = Path.Combine(path, "ASSETS");
            if (File.Exists(Path.Combine(assets, "PITFALL.EXE"))) return assets;
            throw new InvalidOperationException("The selected location does not contain ASSETS\\PITFALL.EXE.");
        }

        private void Install()
        {
            installationRunning = true;
            SetInstallationControls(false);
            try {
                string source = ResolveAssets(sourceBox.Text);
                string originalExe = Path.Combine(source, "PITFALL.EXE");
                Log("Checking the original executable…");
                if (!String.Equals(HashFile(originalExe), OriginalHash, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("This PITFALL.EXE is not the supported retail version. The source was not changed.");

                if (String.IsNullOrWhiteSpace(destinationBox.Text))
                    throw new InvalidOperationException("Choose an installation folder.");
                string root = Path.GetFullPath(destinationBox.Text.Trim());
                string destination = Path.Combine(root, "ASSETS");
                if (IsSameOrChild(destination, source) || IsSameOrChild(source, destination))
                    throw new InvalidOperationException("Choose an installation folder separate from the original source.");

                if (audioBox.SelectedIndex == 2) {
                    string reason;
                    string drive = FindPitfallCd(out reason);
                    if (drive == null || !ProbeCdda(drive, out reason))
                        throw new InvalidOperationException("Physical-CD extraction is unavailable: " + reason);
                    detectedCdDrive = drive;
                    detectedCdCanExtract = true;
                } else if (audioBox.SelectedIndex == 3) {
                    if (String.IsNullOrWhiteSpace(musicBox.Text) || !File.Exists(musicBox.Text) ||
                        !String.Equals(Path.GetExtension(musicBox.Text), ".cue", StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Choose a valid CUE file.");
                } else if (audioBox.SelectedIndex == 4) {
                    if (String.IsNullOrWhiteSpace(musicBox.Text) || !Directory.Exists(musicBox.Text))
                        throw new InvalidOperationException("Choose a valid music folder.");
                }

                if (File.Exists(Path.Combine(destination, "PITFALL.EXE"))) {
                    DialogResult answer = MessageBox.Show(this,
                        "The destination already contains Pitfall. Update that installation? Existing PITMOD.INI settings will be preserved.",
                        Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                    if (answer != DialogResult.Yes) return;
                }

                Log("Copying original game files…");
                CopyDirectory(source, destination);
                string targetExe = Path.Combine(destination, "PITFALL.EXE");
                if (!String.Equals(HashFile(targetExe), OriginalHash,
                                   StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException(
                        "Installed executable no longer matches the verified retail disc.");
                string originalWail = Path.Combine(destination, "WAIL32.DLL");
                string backupWail = Path.Combine(destination, "WAIL32.original.dll");
                if (File.Exists(originalWail) && !File.Exists(backupWail))
                    File.Copy(originalWail, backupWail, false);

                Log("Retail PITFALL.EXE verified; leaving it byte-for-byte unchanged.");

                Log("Installing PITMOD and replacement audio library…");
                WriteResource("PitfallInstaller.PITMOD.DLL", Path.Combine(destination, "PITMOD.DLL"));
                WriteResource("PitfallInstaller.WAIL32.DLL", Path.Combine(destination, "WAIL32.DLL"));
                WriteResource("PitfallInstaller.WINMM.dll", Path.Combine(destination, "WINMM.dll"));
                string iniPath = Path.Combine(destination, "PITMOD.INI");
                if (!File.Exists(iniPath)) WriteResource("PitfallInstaller.PITMOD.INI", iniPath);
                WriteResource("PitfallInstaller.THIRD_PARTY_NOTICES.txt",
                    Path.Combine(root, "THIRD_PARTY_NOTICES.txt"));
                if (audioBox.SelectedIndex == 2) {
                    Log("Extracting CD audio from " + detectedCdDrive + "…");
                    ExtractPhysicalCd(detectedCdDrive, Path.Combine(destination, "music"));
                } else if (audioBox.SelectedIndex == 3) {
                    Log("Extracting CD audio from the BIN/CUE image…");
                    ExtractCue(Path.GetFullPath(musicBox.Text.Trim()), Path.Combine(destination, "music"));
                }
                ConfigureIni(iniPath);

                string installedExe = Path.Combine(destination, "PITFALL.EXE");
                if (desktopShortcut.Checked)
                    CreateShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Pitfall Modified.lnk"), installedExe, destination);
                if (startShortcut.Checked) {
                    string menu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "Pitfall Modified");
                    Directory.CreateDirectory(menu);
                    CreateShortcut(Path.Combine(menu, "Pitfall Modified.lnk"), installedExe, destination);
                }
                string uninstallCache = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "Pitfall Modified Installer");
                Directory.CreateDirectory(uninstallCache);
                string uninstaller = Path.Combine(uninstallCache,
                    "Pitfall-Modified-Setup.exe");
                File.Copy(Application.ExecutablePath, uninstaller, true);
                File.WriteAllText(Path.Combine(root, "pitfall-modified.install"),
                    InstallMarker + Environment.NewLine, new UTF8Encoding(false));
                WriteInstallManifest(root);
                RegisterUninstaller(root, uninstaller, installedExe);

                Log("Installation completed successfully.");
                DialogResult run = MessageBox.Show(this, "Pitfall Modified was installed successfully. Run it now?", Text,
                    MessageBoxButtons.YesNo, MessageBoxIcon.Information);
                if (run == DialogResult.Yes)
                    Process.Start(new ProcessStartInfo(installedExe) { WorkingDirectory = destination, UseShellExecute = true });
            }
            catch (Exception ex) {
                Log("ERROR: " + ex.Message);
                MessageBox.Show(this, ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally {
                installationRunning = false;
                SetInstallationControls(true);
            }
        }

        private void SetInstallationControls(bool enabled)
        {
            foreach (Control control in Controls) {
                if (control != logBox) control.Enabled = enabled;
            }
            logBox.Enabled = true;
            if (!enabled) {
                installButton.Enabled = false;
                closeButton.Enabled = false;
                UseWaitCursor = true;
            } else {
                UseWaitCursor = false;
                closeButton.Enabled = true;
                UpdateMusicControls();
            }
        }

        private void ConfigureIni(string path)
        {
            string value = audioBox.SelectedIndex == 1 ? "CD" :
                           (audioBox.SelectedIndex == 2 || audioBox.SelectedIndex == 3 || audioBox.SelectedIndex == 4) ? "Files" :
                           audioBox.SelectedIndex == 5 ? "Off" : "Auto";
            string text = File.ReadAllText(path);
            text = SetIniValue(text, "MusicBackend", value);
            text = SetIniValue(text, "Option_CDMusic",
                audioBox.SelectedIndex == 5 ? "0" : "1");
            text = SetIniValue(text, "Option_CDAmbient", "0");
            text = SetIniValue(text, "Option_FrameRate", "0");
            if (audioBox.SelectedIndex == 2 || audioBox.SelectedIndex == 3)
                text = SetIniValue(text, "MusicFolder", "music");
            else if (audioBox.SelectedIndex == 4)
                text = SetIniValue(text, "MusicFolder", Path.GetFullPath(musicBox.Text.Trim()));
            /* A release installation starts with clean level progress and lets
               the game create its own encrypted Option_Level value. */
            text = Regex.Replace(text, @"(?m)^Option_Level=.*(?:\r?\n)?", "");
            File.WriteAllText(path, text, new UTF8Encoding(false));
        }

        private static string SetIniValue(string text, string key, string value)
        {
            string pattern = "(?m)^" + Regex.Escape(key) + "=.*$";
            if (Regex.IsMatch(text, pattern)) return Regex.Replace(text, pattern, key + "=" + value);
            return text.TrimEnd() + Environment.NewLine + key + "=" + value + Environment.NewLine;
        }

        private static void CopyDirectory(string source, string destination)
        {
            Directory.CreateDirectory(destination);
            foreach (string file in Directory.GetFiles(source)) {
                string copied = Path.Combine(destination, Path.GetFileName(file));
                File.Copy(file, copied, true);
                /* Files copied from optical media retain the read-only bit.
                   Clear it only on the local copy so PITFALL.EXE can be
                   patched and PITMOD can later update its local files. */
                FileAttributes attributes = File.GetAttributes(copied);
                if ((attributes & FileAttributes.ReadOnly) != 0)
                    File.SetAttributes(copied, attributes & ~FileAttributes.ReadOnly);
            }
            foreach (string directory in Directory.GetDirectories(source))
                CopyDirectory(directory, Path.Combine(destination, Path.GetFileName(directory)));
        }

        private sealed class CdTrack
        {
            public int Number;
            public int Lba;
        }

        private string FindPitfallCd(out string reason)
        {
            reason = "No mounted Pitfall CD was found.";
            foreach (DriveInfo drive in DriveInfo.GetDrives()) {
                try {
                    if (drive.DriveType != DriveType.CDRom || !drive.IsReady) continue;
                    string exe = Path.Combine(drive.RootDirectory.FullName, "ASSETS", "PITFALL.EXE");
                    if (!File.Exists(exe)) continue;
                    if (!String.Equals(HashFile(exe), OriginalHash, StringComparison.OrdinalIgnoreCase)) {
                        reason = "A disc was found, but its PITFALL.EXE is not the supported retail version.";
                        continue;
                    }
                    return drive.Name.Substring(0, 2);
                } catch (Exception ex) { reason = ex.Message; }
            }
            return null;
        }

        private static SafeFileHandle OpenCd(string drive, out string reason)
        {
            reason = null;
            SafeFileHandle handle = CreateFile("\\\\.\\" + drive.TrimEnd('\\'), GenericRead,
                FileShareRead | FileShareWrite, IntPtr.Zero, OpenExisting, 0, IntPtr.Zero);
            if (handle.IsInvalid) {
                reason = "The drive could not be opened for raw reading (Windows error " + Marshal.GetLastWin32Error() + ").";
                handle.Dispose();
                return null;
            }
            return handle;
        }

        private static List<CdTrack> ReadCdToc(SafeFileHandle handle, out string reason)
        {
            reason = null;
            byte[] toc = new byte[4 + 8 * 100];
            int returned;
            if (!DeviceIoControl(handle, IoctlCdromReadToc, IntPtr.Zero, 0,
                    toc, toc.Length, out returned, IntPtr.Zero)) {
                reason = "The drive did not return a CD table of contents (Windows error " + Marshal.GetLastWin32Error() + ").";
                return null;
            }
            int first = toc[2];
            int last = toc[3];
            int records = last - first + 2; /* includes lead-out */
            if (first <= 0 || last < first || returned < 4 + records * 8) {
                reason = "The drive returned an invalid CD table of contents.";
                return null;
            }
            List<CdTrack> result = new List<CdTrack>();
            for (int i = 0; i < records; ++i) {
                int offset = 4 + i * 8;
                int absolute = ((toc[offset + 5] * 60) + toc[offset + 6]) * 75 + toc[offset + 7];
                result.Add(new CdTrack { Number = toc[offset + 2], Lba = absolute - 150 });
            }
            return result;
        }

        private static bool IsPitfallToc(List<CdTrack> tracks, out string reason)
        {
            reason = null;
            if (tracks == null || tracks.Count != 25 || tracks[0].Number != 1 ||
                tracks[23].Number != 24 || tracks[24].Number != 0xAA) {
                reason = "The disc does not have Pitfall's expected 24-track layout.";
                return false;
            }
            int track8Length = tracks[8].Lba - tracks[7].Lba;
            if (track8Length != (4 * 60 + 32) * 75 + 12) {
                reason = "The disc's audio track lengths do not match Pitfall.";
                return false;
            }
            return true;
        }

        private static byte[] RawRead(SafeFileHandle handle, int lba, int sectors, out string reason)
        {
            reason = null;
            byte[] request = new byte[16];
            Buffer.BlockCopy(BitConverter.GetBytes((long)lba * 2048L), 0, request, 0, 8);
            Buffer.BlockCopy(BitConverter.GetBytes(sectors), 0, request, 8, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(2), 0, request, 12, 4); /* CDDA */
            byte[] output = new byte[sectors * 2352];
            int returned;
            if (!DeviceIoControl(handle, IoctlCdromRawRead, request, request.Length,
                    output, output.Length, out returned, IntPtr.Zero)) {
                reason = "Raw CDDA reading failed (Windows error " + Marshal.GetLastWin32Error() + ").";
                return null;
            }
            if (returned != output.Length) {
                reason = "The drive returned an incomplete raw audio sector.";
                return null;
            }
            return output;
        }

        private static bool ProbeCdda(string drive, out string reason)
        {
            using (SafeFileHandle handle = OpenCd(drive, out reason)) {
                if (handle == null) return false;
                List<CdTrack> tracks = ReadCdToc(handle, out reason);
                if (!IsPitfallToc(tracks, out reason)) return false;
                byte[] test = RawRead(handle, tracks[1].Lba, 1, out reason);
                return test != null && test.Length == 2352;
            }
        }

        private void ExtractPhysicalCd(string drive, string outputDirectory)
        {
            string reason;
            using (SafeFileHandle handle = OpenCd(drive, out reason)) {
                if (handle == null) throw new InvalidOperationException(reason);
                List<CdTrack> tracks = ReadCdToc(handle, out reason);
                if (!IsPitfallToc(tracks, out reason)) throw new InvalidOperationException(reason);
                Directory.CreateDirectory(outputDirectory);
                for (int index = 1; index <= 23; ++index) {
                    CdTrack track = tracks[index];
                    int sectors = tracks[index + 1].Lba - track.Lba;
                    if (sectors <= 0) throw new InvalidOperationException("Invalid length for CD track " + track.Number + ".");
                    int dataBytes = checked(sectors * 2352);
                    string output = Path.Combine(outputDirectory, String.Format("track{0:00}.wav", track.Number));
                    Log("Extracting " + Path.GetFileName(output) + " from " + drive + "…");
                    using (FileStream wav = File.Create(output)) {
                        WriteWavHeader(wav, dataBytes);
                        int current = track.Lba;
                        int remaining = sectors;
                        while (remaining > 0) {
                            int batch = Math.Min(16, remaining);
                            byte[] audio = RawRead(handle, current, batch, out reason);
                            if (audio == null) throw new InvalidOperationException("Track " + track.Number + ": " + reason);
                            wav.Write(audio, 0, audio.Length);
                            current += batch;
                            remaining -= batch;
                            Application.DoEvents();
                        }
                    }
                }
            }
        }

        private sealed class CueTrack
        {
            public int Number;
            public bool Audio;
            public string File;
            public long StartSector = -1;
        }

        private void ExtractCue(string cuePath, string outputDirectory)
        {
            List<CueTrack> tracks = new List<CueTrack>();
            string cueDirectory = Path.GetDirectoryName(cuePath);
            string currentFile = null;
            CueTrack currentTrack = null;
            foreach (string raw in File.ReadAllLines(cuePath)) {
                Match file = Regex.Match(raw, "^\\s*FILE\\s+\\\"([^\\\"]+)\\\"\\s+(?:BINARY|MOTOROLA|WAVE)\\s*$", RegexOptions.IgnoreCase);
                if (file.Success) {
                    currentFile = Path.GetFullPath(Path.Combine(cueDirectory, file.Groups[1].Value));
                    continue;
                }
                Match track = Regex.Match(raw, "^\\s*TRACK\\s+(\\d+)\\s+([^\\s]+)", RegexOptions.IgnoreCase);
                if (track.Success) {
                    if (currentFile == null) throw new InvalidOperationException("CUE TRACK appears before FILE.");
                    currentTrack = new CueTrack();
                    currentTrack.Number = Int32.Parse(track.Groups[1].Value);
                    currentTrack.Audio = String.Equals(track.Groups[2].Value, "AUDIO", StringComparison.OrdinalIgnoreCase);
                    currentTrack.File = currentFile;
                    tracks.Add(currentTrack);
                    continue;
                }
                Match index = Regex.Match(raw, "^\\s*INDEX\\s+01\\s+(\\d+):(\\d+):(\\d+)", RegexOptions.IgnoreCase);
                if (index.Success && currentTrack != null) {
                    currentTrack.StartSector = (Int64.Parse(index.Groups[1].Value) * 60L +
                        Int64.Parse(index.Groups[2].Value)) * 75L + Int64.Parse(index.Groups[3].Value);
                }
            }
            if (tracks.Count == 0) throw new InvalidOperationException("The CUE file contains no tracks.");
            foreach (CueTrack track in tracks) {
                if (!File.Exists(track.File)) throw new FileNotFoundException("The BIN file referenced by the CUE was not found.", track.File);
                if (track.StartSector < 0) throw new InvalidOperationException("Track " + track.Number + " has no INDEX 01.");
            }
            int audioCount = 0;
            Directory.CreateDirectory(outputDirectory);
            for (int i = 0; i < tracks.Count; ++i) {
                CueTrack track = tracks[i];
                if (!track.Audio) continue;
                long fileSectors = new FileInfo(track.File).Length / 2352L;
                long endSector = fileSectors;
                for (int n = i + 1; n < tracks.Count; ++n) {
                    if (String.Equals(tracks[n].File, track.File, StringComparison.OrdinalIgnoreCase)) {
                        endSector = tracks[n].StartSector;
                        break;
                    }
                }
                long sectors = endSector - track.StartSector;
                if (sectors <= 0 || sectors > Int32.MaxValue / 2352L)
                    throw new InvalidOperationException("Invalid length for audio track " + track.Number + ".");
                int dataBytes = checked((int)(sectors * 2352L));
                string output = Path.Combine(outputDirectory, String.Format("track{0:00}.wav", track.Number));
                Log("Extracting " + Path.GetFileName(output) + "…");
                using (FileStream input = File.Open(track.File, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (FileStream wav = File.Create(output)) {
                    input.Position = track.StartSector * 2352L;
                    WriteWavHeader(wav, dataBytes);
                    byte[] buffer = new byte[2352 * 32];
                    int remaining = dataBytes;
                    while (remaining > 0) {
                        int wanted = Math.Min(buffer.Length, remaining);
                        int read = input.Read(buffer, 0, wanted);
                        if (read <= 0) throw new EndOfStreamException("Unexpected end of BIN while extracting track " + track.Number + ".");
                        wav.Write(buffer, 0, read);
                        remaining -= read;
                    }
                }
                ++audioCount;
            }
            if (audioCount == 0) throw new InvalidOperationException("The CUE file contains no AUDIO tracks.");
        }

        private static void WriteWavHeader(Stream output, int dataBytes)
        {
            using (BinaryWriter writer = new BinaryWriter(output, Encoding.ASCII, true)) {
                writer.Write(Encoding.ASCII.GetBytes("RIFF"));
                writer.Write(dataBytes + 36);
                writer.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
                writer.Write(16);
                writer.Write((short)1);
                writer.Write((short)2);
                writer.Write(44100);
                writer.Write(44100 * 4);
                writer.Write((short)4);
                writer.Write((short)16);
                writer.Write(Encoding.ASCII.GetBytes("data"));
                writer.Write(dataBytes);
            }
        }


        private static bool IsSameOrChild(string candidate, string parent)
        {
            string a = Path.GetFullPath(candidate).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string b = Path.GetFullPath(parent).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            return a.StartsWith(b, StringComparison.OrdinalIgnoreCase);
        }

        private static string HashFile(string path)
        {
            using (SHA256 sha = SHA256.Create())
            using (FileStream stream = File.OpenRead(path))
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "");
        }

        private static void WriteResource(string name, string path)
        {
            using (Stream input = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) {
                if (input == null) throw new InvalidOperationException("Installer payload is missing: " + name);
                using (FileStream output = File.Create(path)) input.CopyTo(output);
            }
        }

        private static void CreateShortcut(string shortcutPath, string target, string workingDirectory)
        {
            Type type = Type.GetTypeFromProgID("WScript.Shell");
            dynamic shell = Activator.CreateInstance(type);
            dynamic shortcut = shell.CreateShortcut(shortcutPath);
            shortcut.TargetPath = target;
            shortcut.WorkingDirectory = workingDirectory;
            shortcut.Description = "Pitfall: The Mayan Adventure — Modified";
            shortcut.Save();
        }

        private static void RegisterUninstaller(string root, string uninstaller, string gameExe)
        {
            using (RegistryKey key = Registry.CurrentUser.CreateSubKey(UninstallRegistryPath)) {
                if (key == null) throw new InvalidOperationException("Windows could not register the uninstaller.");
                key.SetValue("DisplayName", "Pitfall Modified", RegistryValueKind.String);
                key.SetValue("DisplayVersion", "0.3.3", RegistryValueKind.String);
                key.SetValue("Publisher", "Pitfall Modified Project", RegistryValueKind.String);
                key.SetValue("InstallLocation", root, RegistryValueKind.String);
                key.SetValue("DisplayIcon", gameExe, RegistryValueKind.String);
                key.SetValue("UninstallString", "\"" + uninstaller +
                    "\" --uninstall \"" + root + "\"", RegistryValueKind.String);
                key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                long bytes = DirectorySize(root);
                key.SetValue("EstimatedSize", (int)Math.Min(Int32.MaxValue, (bytes + 1023) / 1024), RegistryValueKind.DWord);
            }
        }

        private static void WriteInstallManifest(string root)
        {
            string manifest = Path.Combine(root, "pitfall-modified.files");
            string prefix = root.TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            List<string> files = new List<string>();
            foreach (string path in Directory.GetFiles(root, "*", SearchOption.AllDirectories)) {
                if (!String.Equals(path, manifest, StringComparison.OrdinalIgnoreCase))
                    files.Add(path.Substring(prefix.Length));
            }
            files.Sort(StringComparer.OrdinalIgnoreCase);
            File.WriteAllLines(manifest, files.ToArray(), new UTF8Encoding(false));
        }

        private static long DirectorySize(string root)
        {
            long total = 0;
            foreach (string file in Directory.GetFiles(root, "*", SearchOption.AllDirectories)) {
                try { total += new FileInfo(file).Length; } catch { }
            }
            return total;
        }

        private void Log(string value)
        {
            logBox.AppendText(value + Environment.NewLine);
            logBox.SelectionStart = logBox.TextLength;
            logBox.ScrollToCaret();
            Application.DoEvents();
        }
    }

    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            if (args.Length >= 2 && args[0] == "--uninstall") {
                InstallerUninstall.Run(args[1]);
                return;
            }
            Application.Run(new InstallerForm());
        }
    }

    internal static class InstallerUninstall
    {
        private const string MarkerValue = "PitfallModifiedInstaller-2026-v1";
        private const string RegistryPath = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\PitfallModified";
        private const string ManifestName = "pitfall-modified.files";

        public static void Run(string root)
        {
            string error;
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            if (!Validate(root, out error)) {
                MessageBox.Show(error, "Pitfall Modified Uninstaller",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (MessageBox.Show(
                    "This will remove Pitfall Modified, its local settings and level progress, and shortcuts.\r\n\r\n" +
                    root + "\r\n\r\nContinue?", "Uninstall Pitfall Modified",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                return;
            try {
                DeleteShortcutIfOwned(Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                    "Pitfall Modified.lnk"), root);
                string menu = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.Programs),
                    "Pitfall Modified");
                DeleteShortcutIfOwned(Path.Combine(menu, "Pitfall Modified.lnk"), root);
                if (Directory.Exists(menu) && Directory.GetFileSystemEntries(menu).Length == 0)
                    Directory.Delete(menu);
                RemoveInstalledFiles(root);
                Registry.CurrentUser.DeleteSubKeyTree(RegistryPath, false);
                MessageBox.Show("Pitfall Modified was removed successfully.",
                    "Pitfall Modified Uninstaller", MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            } catch (Exception ex) {
                MessageBox.Show("Uninstallation was not completed: " + ex.Message,
                    "Pitfall Modified Uninstaller", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private static bool Validate(string root, out string error)
        {
            error = null;
            try {
                string full = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
                string drive = Path.GetPathRoot(full).TrimEnd(Path.DirectorySeparatorChar);
                if (String.Equals(full, drive, StringComparison.OrdinalIgnoreCase) ||
                    full.Length < drive.Length + 4) {
                    error = "The installation path is too broad to remove safely.";
                    return false;
                }
                string marker = Path.Combine(full, "pitfall-modified.install");
                if (!File.Exists(marker) || File.ReadAllText(marker).Trim() != MarkerValue ||
                    !File.Exists(Path.Combine(full, ManifestName)) ||
                    !File.Exists(Path.Combine(full, "ASSETS", "PITFALL.EXE")) ||
                    !File.Exists(Path.Combine(full, "ASSETS", "PITMOD.DLL"))) {
                    error = "This folder is not a verified Pitfall Modified installation. Nothing was removed.";
                    return false;
                }
                return true;
            } catch (Exception ex) {
                error = ex.Message;
                return false;
            }
        }

        private static void RemoveInstalledFiles(string root)
        {
            string manifest = Path.Combine(root, ManifestName);
            string prefix = root.TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            foreach (string relative in File.ReadAllLines(manifest)) {
                if (String.IsNullOrWhiteSpace(relative)) continue;
                string path = Path.GetFullPath(Path.Combine(root, relative));
                if (!path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("The uninstall manifest contains an unsafe path.");
                if (File.Exists(path)) {
                    File.SetAttributes(path, FileAttributes.Normal);
                    File.Delete(path);
                }
            }

            // Runtime-created files are intentionally limited to these known names.
            string assets = Path.Combine(root, "ASSETS");
            foreach (string name in new[] { "pitmod.log", "pitmod-timing.csv" }) {
                string path = Path.Combine(assets, name);
                if (File.Exists(path)) File.Delete(path);
            }
            File.Delete(manifest);

            List<string> directories = new List<string>(
                Directory.GetDirectories(root, "*", SearchOption.AllDirectories));
            directories.Sort(delegate(string a, string b) { return b.Length.CompareTo(a.Length); });
            foreach (string directory in directories)
                if (Directory.Exists(directory) && Directory.GetFileSystemEntries(directory).Length == 0)
                    Directory.Delete(directory);
            if (Directory.Exists(root) && Directory.GetFileSystemEntries(root).Length == 0)
                Directory.Delete(root);
        }

        private static void DeleteShortcutIfOwned(string path, string root)
        {
            if (!File.Exists(path)) return;
            try {
                Type type = Type.GetTypeFromProgID("WScript.Shell");
                dynamic shell = Activator.CreateInstance(type);
                dynamic shortcut = shell.CreateShortcut(path);
                string target = Convert.ToString(shortcut.TargetPath);
                string prefix = Path.GetFullPath(root).TrimEnd(
                    Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                if (Path.GetFullPath(target).StartsWith(prefix,
                        StringComparison.OrdinalIgnoreCase))
                    File.Delete(path);
            } catch { }
        }
    }
}
