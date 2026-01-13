<#
.SYNOPSIS
    GUI Tool zum Anzeigen extrahierter COM CLSIDs
#>

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

function Show-CLSIDViewer {
    param([string]$IniFile)
    
    # Form erstellen
    $form = New-Object System.Windows.Forms.Form
    $form.Text = "COM CLSID Viewer"
    $form.Size = New-Object System.Drawing.Size(900, 600)
    $form.StartPosition = "CenterScreen"
    
    # TreeView
    $treeView = New-Object System.Windows.Forms.TreeView
    $treeView.Location = New-Object System.Drawing.Point(10, 40)
    $treeView.Size = New-Object System.Drawing.Size(350, 500)
    $treeView.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor 
                       [System.Windows.Forms.AnchorStyles]::Bottom -bor
                       [System.Windows.Forms.AnchorStyles]::Left
    
    # Details TextBox
    $textBox = New-Object System.Windows.Forms.TextBox
    $textBox.Location = New-Object System.Drawing.Point(370, 40)
    $textBox.Size = New-Object System.Drawing.Size(510, 500)
    $textBox.Multiline = $true
    $textBox.ScrollBars = "Both"
    $textBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $textBox.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor 
                      [System.Windows.Forms.AnchorStyles]::Bottom -bor
                      [System.Windows.Forms.AnchorStyles]::Left -bor
                      [System.Windows.Forms.AnchorStyles]::Right
    
    # Toolbar
    $toolStrip = New-Object System.Windows.Forms.ToolStrip
    $toolStrip.Location = New-Object System.Drawing.Point(0, 0)
    $toolStrip.Size = New-Object System.Drawing.Size(900, 25)
    
    $btnLoad = New-Object System.Windows.Forms.ToolStripButton
    $btnLoad.Text = "Load INI"
    $btnLoad.Add_Click({
        $openFileDialog = New-Object System.Windows.Forms.OpenFileDialog
        $openFileDialog.Filter = "INI files (*.ini)|*.ini|All files (*.*)|*.*"
        
        if ($openFileDialog.ShowDialog() -eq "OK") {
            Load-INIData -FilePath $openFileDialog.FileName
        }
    })
    
    $toolStrip.Items.Add($btnLoad) | Out-Null
    
    # INI laden
    function Load-INIData {
        param([string]$FilePath)
        
        $treeView.Nodes.Clear()
        $content = Get-Content $FilePath
        
        $currentDll = $null
        $currentNode = $null
        
        foreach ($line in $content) {
            if ($line -match '^\[(.+)\]$') {
                $dllName = $Matches[1]
                $currentNode = $treeView.Nodes.Add($dllName)
                $currentDll = @{ Name = $dllName }
            }
            elseif ($line -match '^(.+)=(.+)$' -and $currentNode) {
                $key = $Matches[1]
                $value = $Matches[2]
                
                $currentDll[$key] = $value
                
                if ($key -match '^CLSID\d+$') {
                    $clsidNode = $currentNode.Nodes.Add($value)
                    $clsidNode.Tag = $currentDll
                }
            }
        }
        
        $form.Text = "COM CLSID Viewer - $([System.IO.Path]::GetFileName($FilePath))"
    }
    
    # TreeView Selection Changed
    $treeView.Add_AfterSelect({
        param($sender, $e)
        
        $node = $e.Node
        $details = ""
        
        if ($node.Level -eq 0) {
            # DLL Node
            $details = "DLL Information`r`n"
            $details += "=" * 50 + "`r`n`r`n"
            $details += "Name: $($node.Text)`r`n"
            
            # Alle Properties aus Tag
            if ($node.Tag) {
                foreach ($key in $node.Tag.Keys | Sort-Object) {
                    $details += "$key : $($node.Tag[$key])`r`n"
                }
            }
        }
        else {
            # CLSID Node
            $details = "CLSID Details`r`n"
            $details += "=" * 50 + "`r`n`r`n"
            $details += "CLSID: $($node.Text)`r`n"
            
            if ($node.Tag) {
                $index = $node.Index
                $progIdKey = "ProgID$index"
                $classNameKey = "ClassName$index"
                
                if ($node.Tag.ContainsKey($progIdKey)) {
                    $details += "ProgID: $($node.Tag[$progIdKey])`r`n"
                }
                
                if ($node.Tag.ContainsKey($classNameKey)) {
                    $details += "Class: $($node.Tag[$classNameKey])`r`n"
                }
            }
        }
        
        $textBox.Text = $details
    })
    
    # Form aufbauen
    $form.Controls.Add($toolStrip)
    $form.Controls.Add($treeView)
    $form.Controls.Add($textBox)
    
    # Initiales Laden
    if ($IniFile -and (Test-Path $IniFile)) {
        Load-INIData -FilePath $IniFile
    }
    
    $form.ShowDialog() | Out-Null
}

# Wenn INI-Datei als Parameter übergeben
if ($args.Count -gt 0 -and (Test-Path $args[0])) {
    Show-CLSIDViewer -IniFile $args[0]
} else {
    Show-CLSIDViewer
}
