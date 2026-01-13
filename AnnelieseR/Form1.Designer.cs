namespace AnnelieseR
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;



        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            ListViewItem listViewItem1 = new ListViewItem(new string[] { "gh", "", "", "", "" }, -1);
            ListViewItem listViewItem2 = new ListViewItem("ghhgj");
            chkRecursive = new CheckBox();
            btnSelectFolder = new Button();
            btnExport = new Button();
            btnClear = new Button();
            lblStatus = new Label();
            progressBar = new ProgressBar();
            listView = new ListView();
            Assembly = new ColumnHeader();
            CLSID = new ColumnHeader();
            ProgID = new ColumnHeader();
            Typename = new ColumnHeader();
            Platform = new ColumnHeader();
            Arch = new ColumnHeader();
            columnHeader7 = new ColumnHeader();
            columnHeader8 = new ColumnHeader();
            txtFolder = new TextBox();
            SuspendLayout();
            // 
            // chkRecursive
            // 
            chkRecursive.AutoSize = true;
            chkRecursive.Location = new Point(39, 65);
            chkRecursive.Name = "chkRecursive";
            chkRecursive.Size = new Size(67, 19);
            chkRecursive.TabIndex = 0;
            chkRecursive.Text = "rekursiv";
            chkRecursive.UseVisualStyleBackColor = true;
            // 
            // btnSelectFolder
            // 
            btnSelectFolder.Location = new Point(39, 36);
            btnSelectFolder.Name = "btnSelectFolder";
            btnSelectFolder.Size = new Size(25, 23);
            btnSelectFolder.TabIndex = 1;
            btnSelectFolder.Text = "...";
            btnSelectFolder.UseVisualStyleBackColor = true;
            btnSelectFolder.Click += BtnSelectFolder_Click;
            // 
            // btnExport
            // 
            btnExport.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            btnExport.Location = new Point(729, 526);
            btnExport.Name = "btnExport";
            btnExport.Size = new Size(75, 23);
            btnExport.TabIndex = 2;
            btnExport.Text = "Export";
            btnExport.UseVisualStyleBackColor = true;
            btnExport.Click += BtnExport_Click;
            // 
            // btnClear
            // 
            btnClear.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            btnClear.Location = new Point(648, 526);
            btnClear.Name = "btnClear";
            btnClear.Size = new Size(75, 23);
            btnClear.TabIndex = 3;
            btnClear.Text = "Clear";
            btnClear.UseVisualStyleBackColor = true;
            btnClear.Click += BtnClear_Click;
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.Location = new Point(39, 87);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(38, 15);
            lblStatus.TabIndex = 4;
            lblStatus.Text = "label1";
            // 
            // progressBar
            // 
            progressBar.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            progressBar.Location = new Point(39, 105);
            progressBar.Name = "progressBar";
            progressBar.Size = new Size(765, 18);
            progressBar.TabIndex = 5;
            // 
            // listView
            // 
            listView.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            listView.Columns.AddRange(new ColumnHeader[] { Assembly, CLSID, ProgID, Typename, Platform, Arch });
            listView.Items.AddRange(new ListViewItem[] { listViewItem1, listViewItem2 });
            listView.Location = new Point(46, 138);
            listView.Name = "listView";
            listView.Size = new Size(758, 382);
            listView.TabIndex = 6;
            listView.UseCompatibleStateImageBehavior = false;
            listView.View = View.Details;
            // 
            // Assembly
            // 
            Assembly.Text = "Assembly";
            Assembly.Width = 200;
            // 
            // CLSID
            // 
            CLSID.Text = "CLSID";
            CLSID.Width = 200;
            // 
            // ProgID
            // 
            ProgID.Text = "ProgID";
            ProgID.Width = 100;
            // 
            // Typename
            // 
            Typename.Text = "Typename";
            Typename.Width = 100;
            // 
            // Platform
            // 
            Platform.Text = "Platform";
            Platform.Width = 80;
            // 
            // Arch
            // 
            Arch.Text = "Arch";
            // 
            // txtFolder
            // 
            txtFolder.Location = new Point(79, 37);
            txtFolder.Name = "txtFolder";
            txtFolder.Size = new Size(805, 23);
            txtFolder.TabIndex = 7;
            // 
            // Form1
            // 
            ClientSize = new Size(858, 561);
            Controls.Add(txtFolder);
            Controls.Add(listView);
            Controls.Add(progressBar);
            Controls.Add(lblStatus);
            Controls.Add(btnClear);
            Controls.Add(btnExport);
            Controls.Add(btnSelectFolder);
            Controls.Add(chkRecursive);
            MinimumSize = new Size(800, 600);
            Name = "Form1";
            StartPosition = FormStartPosition.CenterScreen;
            Text = "btnClear";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private CheckBox chkRecursive;
        private Button btnSelectFolder;
        private Button btnExport;
        private Button btnClear;
        private Label lblStatus;
        private ProgressBar progressBar;
        private ListView listView;
        private TextBox txtFolder;
        private ColumnHeader Assembly;
        private ColumnHeader CLSID;
        private ColumnHeader ProgID;
        private ColumnHeader Typename;
        private ColumnHeader Platform;
        private ColumnHeader Arch;
        private ColumnHeader columnHeader7;
        private ColumnHeader columnHeader8;
    }
}
