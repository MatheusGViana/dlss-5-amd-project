$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form=New-Object Windows.Forms.Form
$form.Text='Setup'; $form.StartPosition='CenterScreen'; $form.ClientSize=New-Object Drawing.Size(520,205)
$form.FormBorderStyle='FixedDialog'; $form.MaximizeBox=$false
$label=New-Object Windows.Forms.Label; $label.Location=New-Object Drawing.Point(20,20); $label.Size=New-Object Drawing.Size(220,22); $label.Text='Game executable'; $form.Controls.Add($label)
$path=New-Object Windows.Forms.TextBox; $path.Location=New-Object Drawing.Point(20,43); $path.Size=New-Object Drawing.Size(380,25); $path.ReadOnly=$true; $form.Controls.Add($path)
$browse=New-Object Windows.Forms.Button; $browse.Location=New-Object Drawing.Point(410,41); $browse.Size=New-Object Drawing.Size(90,29); $browse.Text='Browse...'; $form.Controls.Add($browse)
$dllLabel=New-Object Windows.Forms.Label; $dllLabel.Location=New-Object Drawing.Point(20,87); $dllLabel.Size=New-Object Drawing.Size(220,22); $dllLabel.Text='DLL name'; $form.Controls.Add($dllLabel)
$dll=New-Object Windows.Forms.ComboBox; $dll.Location=New-Object Drawing.Point(20,110); $dll.Size=New-Object Drawing.Size(300,25); $dll.DropDownStyle='DropDownList'
@('dxgi.dll','winmm.dll','version.dll','winhttp.dll','wininet.dll','dbghelp.dll') | ForEach-Object {[void]$dll.Items.Add($_)}
$dll.SelectedIndex=0; $form.Controls.Add($dll)
$install=New-Object Windows.Forms.Button; $install.Location=New-Object Drawing.Point(300,151); $install.Size=New-Object Drawing.Size(95,34); $install.Text='Install'; $form.Controls.Add($install)
$cancel=New-Object Windows.Forms.Button; $cancel.Location=New-Object Drawing.Point(405,151); $cancel.Size=New-Object Drawing.Size(95,34); $cancel.Text='Cancel'; $cancel.DialogResult='Cancel'; $form.CancelButton=$cancel; $form.Controls.Add($cancel)
$picker=New-Object Windows.Forms.OpenFileDialog; $picker.Title='Select the game executable'; $picker.Filter='Executable files (*.exe)|*.exe'; $picker.CheckFileExists=$true
$browse.Add_Click({if($picker.ShowDialog() -eq 'OK'){$path.Text=$picker.FileName}})
$install.Add_Click({
    if(!(Test-Path -LiteralPath $path.Text -PathType Leaf)){[void][Windows.Forms.MessageBox]::Show('Select the game executable.','Setup','OK','Warning'); return}
    $install.Enabled=$false; $form.UseWaitCursor=$true
    try {
        & (Join-Path $PSScriptRoot 'Setup.Install.ps1') -GameDir ([IO.Path]::GetDirectoryName($path.Text)) -ProxyName ([string]$dll.SelectedItem) *> $null
        [void][Windows.Forms.MessageBox]::Show('Installed.','Setup','OK','Information'); $form.Close()
    } catch {[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Setup','OK','Error')}
    finally {$form.UseWaitCursor=$false; $install.Enabled=$true}
})
[void]$form.ShowDialog()
