using System.Text;

namespace AnnelieseR
{
    public partial class Form1 : Form
    {


        private List<COMClassInfo> results = new List<COMClassInfo>();

        private void SetupUI()
        {
            // Top Panel
           
        }

        private void BtnSelectFolder_Click(object sender, EventArgs e)
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog())
            {
                dialog.Description = "Select folder to scan for COM DLLs";
                dialog.ShowNewFolderButton = false;

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    txtFolder.Text = dialog.SelectedPath;
                    ScanFolder(dialog.SelectedPath);
                }
            }
        }

        private void ScanFolder(string path)
        {
            results.Clear();
            listView.Items.Clear();
            btnExport.Enabled = false;
            btnClear.Enabled = false;

            lblStatus.Text = "Scanning...";
            progressBar.Value = 0;
            Application.DoEvents();

            try
            {
                SearchOption searchOption = chkRecursive.Checked
                    ? SearchOption.AllDirectories
                    : SearchOption.TopDirectoryOnly;

                string[] dlls = Directory.GetFiles(path, "*.dll", searchOption);

                if (dlls.Length == 0)
                {
                    lblStatus.Text = "No DLL files found.";
                    MessageBox.Show("No DLL files found in the selected folder.",
                        "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }

                progressBar.Maximum = dlls.Length;

                for (int i = 0; i < dlls.Length; i++)
                {
                    string dll = dlls[i];
                    progressBar.Value = i + 1;
                    lblStatus.Text = $"Processing {i + 1} of {dlls.Length}: {Path.GetFileName(dll)}";
                    Application.DoEvents();

                    try
                    {
                        AnalyzeDLL(dll);
                    }
                    catch (Exception ex)
                    {
                        // Log error but continue
                        System.Diagnostics.Debug.WriteLine($"Error analyzing {dll}: {ex.Message}");
                    }
                }

                // Populate ListView
                foreach (var result in results.OrderBy(r => r.DllName))
                {
                    ListViewItem item = new ListViewItem(result.DllName);
                    item.SubItems.Add(result.CLSID);
                    item.SubItems.Add(result.ProgID ?? "");
                    item.SubItems.Add(result.ClassName ?? "");
                    item.SubItems.Add(result.Type);
                    item.SubItems.Add(result.Architecture);
                    item.Tag = result;

                    // Color coding
                    if (result.Type.Contains(".NET"))
                    {
                        item.ForeColor = Color.Blue;
                    }
                    else if (result.Type.Contains("Native COM"))
                    {
                        item.ForeColor = Color.Green;
                    }

                    listView.Items.Add(item);
                }

                int comCount = results.Count(r => !string.IsNullOrEmpty(r.CLSID));
                lblStatus.Text = $"Scan complete. Found {dlls.Length} DLLs, {comCount} COM classes.";

                btnExport.Enabled = results.Count > 0;
                btnClear.Enabled = results.Count > 0;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error scanning folder: {ex.Message}",
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                lblStatus.Text = "Error during scan.";
            }
            finally
            {
                progressBar.Value = 0;
            }
        }
        private void AnalyzeDLL(string dllPath)
        {
            string dllName = Path.GetFileName(dllPath);

            // Analyze PE
            bool isDotNet = PEAnalyzer.IsDotNetAssembly(dllPath);
            string architecture = PEAnalyzer.GetArchitecture(dllPath);

            if (isDotNet)
            {
                // .NET Assembly
                List<DotNetCOMClass> dotnetClasses = DotNetAnalyzer.GetCOMClasses(dllPath);

                foreach (var comClass in dotnetClasses)
                {
                    results.Add(new COMClassInfo
                    {
                        DllName = dllName,
                        DllPath = dllPath,
                        CLSID = comClass.CLSID,
                        ProgID = comClass.ProgID,
                        ClassName = comClass.ClassName,
                        Type = ".NET COM",
                        Architecture = architecture
                    });
                }
            }
            else
            {
                // Native DLL
                var exports = NativeCOMAnalyzer.AnalyzeCOMExports(dllPath);
                bool hasCOM = exports.ContainsKey("DllGetClassObject") && exports["DllGetClassObject"];

                if (hasCOM)
                {
                    // Try to get CLSIDs from registry
                    List<RegistryCOMClass> registryClasses = RegistryAnalyzer.GetCLSIDsForDLL(dllPath);

                    if (registryClasses.Count > 0)
                    {
                        foreach (var regClass in registryClasses)
                        {
                            results.Add(new COMClassInfo
                            {
                                DllName = dllName,
                                DllPath = dllPath,
                                CLSID = regClass.CLSID,
                                ProgID = regClass.ProgID,
                                ClassName = regClass.ClassName,
                                Type = "Native COM",
                                Architecture = architecture
                            });
                        }
                    }
                    else
                    {
                        // Has COM exports but not registered
                        results.Add(new COMClassInfo
                        {
                            DllName = dllName,
                            DllPath = dllPath,
                            CLSID = "(Not Registered)",
                            ProgID = null,
                            ClassName = null,
                            Type = "Native COM (Unregistered)",
                            Architecture = architecture
                        });
                    }
                }
            }
        }

        private void BtnExport_Click(object sender, EventArgs e)
        {
            if (results.Count == 0)
            {
                MessageBox.Show("No results to export.", "Info",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = "INI Files (*.ini)|*.ini|All Files (*.*)|*.*";
                dialog.DefaultExt = "ini";
                dialog.FileName = "COM_CLSIDs.ini";

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    try
                    {
                        ExportToINI(dialog.FileName);
                        MessageBox.Show($"Successfully exported {results.Count} entries to:\n{dialog.FileName}",
                            "Export Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Error exporting: {ex.Message}",
                            "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
        }

        private void ExportToINI(string filePath)
        {
            StringBuilder sb = new StringBuilder();

            sb.AppendLine("; COM CLSIDs extracted on " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            sb.AppendLine("; Generated by COM CLSID Extractor");
            sb.AppendLine($"; Total entries: {results.Count}");
            sb.AppendLine();

            // Group by DLL
            var grouped = results.GroupBy(r => r.DllName).OrderBy(g => g.Key);

            foreach (var group in grouped)
            {
                sb.AppendLine($"[{group.Key}]");
                sb.AppendLine($"Path={group.First().DllPath}");
                sb.AppendLine($"Type={group.First().Type}");
                sb.AppendLine($"Architecture={group.First().Architecture}");
                sb.AppendLine($"CLSIDCount={group.Count()}");

                int index = 0;
                foreach (var item in group)
                {
                    if (!string.IsNullOrEmpty(item.CLSID) && item.CLSID != "(Not Registered)")
                    {
                        sb.AppendLine($"CLSID{index}={item.CLSID}");

                        if (!string.IsNullOrEmpty(item.ProgID))
                        {
                            sb.AppendLine($"ProgID{index}={item.ProgID}");
                        }

                        if (!string.IsNullOrEmpty(item.ClassName))
                        {
                            sb.AppendLine($"ClassName{index}={item.ClassName}");
                        }

                        index++;
                    }
                }

                sb.AppendLine();
            }

            File.WriteAllText(filePath, sb.ToString(), Encoding.UTF8);
        }

        private void BtnClear_Click(object sender, EventArgs e)
        {
            results.Clear();
            listView.Items.Clear();
            txtFolder.Text = "";
            lblStatus.Text = "Ready. Select a folder to scan for COM DLLs.";
            btnExport.Enabled = false;
            btnClear.Enabled = false;
        }
    

        public Form1()
        {
            InitializeComponent();
            SetupUI();
        }
    }
}
