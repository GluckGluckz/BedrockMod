# PebbleCore Real-Time Memory Editor
# A tiny live memory viewer/editor for Minecraft.Windows.exe.
# Run it (right-click > Run with PowerShell, or: powershell -ExecutionPolicy Bypass -File MemoryEditor.ps1)
# while Minecraft is running. Same-user, no admin needed for same-session UWP.

Add-Type -Namespace Mem -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr OpenProcess(uint a, bool inh, uint pid);
[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out IntPtr read);
[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool WriteProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out IntPtr written);
[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool CloseHandle(IntPtr h);
'@

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$script:Handle = [IntPtr]::Zero
$script:ProcId = 0

# PROCESS_VM_READ | VM_WRITE | VM_OPERATION | QUERY_INFORMATION
$ACCESS = 0x0010 -bor 0x0020 -bor 0x0008 -bor 0x0400

function Attach {
    $p = Get-Process Minecraft.Windows -ErrorAction SilentlyContinue
    if (-not $p) { return $false }
    if ($script:Handle -ne [IntPtr]::Zero) { [void][Mem.Native]::CloseHandle($script:Handle) }
    $script:Handle = [Mem.Native]::OpenProcess($ACCESS, $false, [uint32]$p.Id)
    $script:ProcId = $p.Id
    return ($script:Handle -ne [IntPtr]::Zero)
}

function ParseAddr([string]$s) {
    if ([string]::IsNullOrWhiteSpace($s)) { return 0 }
    $s = $s.Trim().Replace('0x','').Replace('0X','')
    try { return [Convert]::ToUInt64($s, 16) } catch { return 0 }
}

function ReadBytes([uint64]$addr, [int]$count) {
    if ($script:Handle -eq [IntPtr]::Zero -or $addr -eq 0) { return $null }
    $buf = New-Object byte[] $count
    $read = [IntPtr]::Zero
    $ok = [Mem.Native]::ReadProcessMemory($script:Handle, [IntPtr][long]$addr, $buf, $count, [ref]$read)
    if (-not $ok) { return $null }
    return $buf
}

function WriteBytes([uint64]$addr, [byte[]]$bytes) {
    if ($script:Handle -eq [IntPtr]::Zero -or $addr -eq 0) { return $false }
    $written = [IntPtr]::Zero
    return [Mem.Native]::WriteProcessMemory($script:Handle, [IntPtr][long]$addr, $bytes, $bytes.Length, [ref]$written)
}

# ---------- UI ----------
$form = New-Object Windows.Forms.Form
$form.Text = "PebbleCore Memory Editor"
$form.Size = New-Object Drawing.Size(640, 560)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object Drawing.Font("Segoe UI", 9)

$lblStatus = New-Object Windows.Forms.Label
$lblStatus.Location = New-Object Drawing.Point(12, 12)
$lblStatus.Size = New-Object Drawing.Size(500, 20)
$lblStatus.Text = "Not attached."
$form.Controls.Add($lblStatus)

$btnAttach = New-Object Windows.Forms.Button
$btnAttach.Text = "Attach"
$btnAttach.Location = New-Object Drawing.Point(520, 8)
$btnAttach.Size = New-Object Drawing.Size(90, 26)
$form.Controls.Add($btnAttach)

$lblAddr = New-Object Windows.Forms.Label
$lblAddr.Text = "Address (hex):"
$lblAddr.Location = New-Object Drawing.Point(12, 46)
$lblAddr.Size = New-Object Drawing.Size(90, 20)
$form.Controls.Add($lblAddr)

$txtAddr = New-Object Windows.Forms.TextBox
$txtAddr.Location = New-Object Drawing.Point(104, 44)
$txtAddr.Size = New-Object Drawing.Size(220, 22)
$txtAddr.Text = "0x0"
$form.Controls.Add($txtAddr)

$chkLive = New-Object Windows.Forms.CheckBox
$chkLive.Text = "Live refresh (0.4s)"
$chkLive.Location = New-Object Drawing.Point(340, 45)
$chkLive.Size = New-Object Drawing.Size(150, 22)
$form.Controls.Add($chkLive)

$btnLoadPlayer = New-Object Windows.Forms.Button
$btnLoadPlayer.Text = "Load player from log"
$btnLoadPlayer.Location = New-Object Drawing.Point(490, 42)
$btnLoadPlayer.Size = New-Object Drawing.Size(120, 26)
$form.Controls.Add($btnLoadPlayer)

# Interpretation labels
$grp = New-Object Windows.Forms.GroupBox
$grp.Text = "Value at address"
$grp.Location = New-Object Drawing.Point(12, 76)
$grp.Size = New-Object Drawing.Size(600, 120)
$form.Controls.Add($grp)

$lblInterp = New-Object Windows.Forms.Label
$lblInterp.Location = New-Object Drawing.Point(12, 22)
$lblInterp.Size = New-Object Drawing.Size(576, 90)
$lblInterp.Font = New-Object Drawing.Font("Consolas", 10)
$grp.Controls.Add($lblInterp)

# Hex dump
$txtHex = New-Object Windows.Forms.TextBox
$txtHex.Location = New-Object Drawing.Point(12, 204)
$txtHex.Size = New-Object Drawing.Size(600, 150)
$txtHex.Multiline = $true
$txtHex.ScrollBars = "Vertical"
$txtHex.Font = New-Object Drawing.Font("Consolas", 9)
$txtHex.ReadOnly = $true
$form.Controls.Add($txtHex)

# Write row
$lblType = New-Object Windows.Forms.Label
$lblType.Text = "Write as:"
$lblType.Location = New-Object Drawing.Point(12, 366)
$lblType.Size = New-Object Drawing.Size(60, 20)
$form.Controls.Add($lblType)

$cmbType = New-Object Windows.Forms.ComboBox
$cmbType.Location = New-Object Drawing.Point(74, 364)
$cmbType.Size = New-Object Drawing.Size(90, 22)
$cmbType.DropDownStyle = "DropDownList"
[void]$cmbType.Items.AddRange(@("Int32","UInt32","Int64","Float","Double","Bytes"))
$cmbType.SelectedIndex = 0
$form.Controls.Add($cmbType)

$txtValue = New-Object Windows.Forms.TextBox
$txtValue.Location = New-Object Drawing.Point(174, 364)
$txtValue.Size = New-Object Drawing.Size(240, 22)
$form.Controls.Add($txtValue)

$btnWrite = New-Object Windows.Forms.Button
$btnWrite.Text = "Write"
$btnWrite.Location = New-Object Drawing.Point(424, 362)
$btnWrite.Size = New-Object Drawing.Size(90, 26)
$form.Controls.Add($btnWrite)

# Pointer resolver
$grp2 = New-Object Windows.Forms.GroupBox
$grp2.Text = "Pointer chain resolver:  final = [[base]+o0]+o1 ... + last offset"
$grp2.Location = New-Object Drawing.Point(12, 398)
$grp2.Size = New-Object Drawing.Size(600, 110)
$form.Controls.Add($grp2)

$lblBase = New-Object Windows.Forms.Label
$lblBase.Text = "Base (hex):"
$lblBase.Location = New-Object Drawing.Point(12, 26)
$lblBase.Size = New-Object Drawing.Size(70, 20)
$grp2.Controls.Add($lblBase)

$txtBase = New-Object Windows.Forms.TextBox
$txtBase.Location = New-Object Drawing.Point(84, 24)
$txtBase.Size = New-Object Drawing.Size(200, 22)
$grp2.Controls.Add($txtBase)

$lblOff = New-Object Windows.Forms.Label
$lblOff.Text = "Offsets (hex, comma):"
$lblOff.Location = New-Object Drawing.Point(12, 58)
$lblOff.Size = New-Object Drawing.Size(130, 20)
$grp2.Controls.Add($lblOff)

$txtOff = New-Object Windows.Forms.TextBox
$txtOff.Location = New-Object Drawing.Point(144, 56)
$txtOff.Size = New-Object Drawing.Size(200, 22)
$txtOff.Text = "0x10, 0x0"
$grp2.Controls.Add($txtOff)

$btnResolve = New-Object Windows.Forms.Button
$btnResolve.Text = "Resolve -> Address"
$btnResolve.Location = New-Object Drawing.Point(360, 40)
$btnResolve.Size = New-Object Drawing.Size(150, 26)
$grp2.Controls.Add($btnResolve)

# ---------- Logic ----------
function RefreshView {
    $addr = ParseAddr $txtAddr.Text
    if ($addr -eq 0) { $lblInterp.Text = "(enter an address)"; return }
    $b = ReadBytes $addr 64
    if ($null -eq $b) { $lblInterp.Text = "read failed (bad address or detached)"; $txtHex.Text = ""; return }
    $i32 = [BitConverter]::ToInt32($b,0)
    $u32 = [BitConverter]::ToUInt32($b,0)
    $i64 = [BitConverter]::ToInt64($b,0)
    $flt = [BitConverter]::ToSingle($b,0)
    $dbl = [BitConverter]::ToDouble($b,0)
    $ptr = [BitConverter]::ToUInt64($b,0)
    $lblInterp.Text = ("Int32 : {0}`r`nUInt32: {1}`r`nInt64 : {2}`r`nFloat : {3}`r`nDouble: {4}`r`nPtr   : 0x{5:X}" -f $i32,$u32,$i64,$flt,$dbl,$ptr)
    $sb = New-Object System.Text.StringBuilder
    for ($i=0; $i -lt 64; $i+=16) {
        $line = "+0x{0:X2}: " -f $i
        $asc = ""
        for ($j=0; $j -lt 16; $j++) {
            $v = $b[$i+$j]
            $line += "{0:X2} " -f $v
            $asc += if ($v -ge 32 -and $v -le 126) { [char]$v } else { "." }
        }
        [void]$sb.AppendLine("$line $asc")
    }
    $txtHex.Text = $sb.ToString()
}

$btnAttach.Add_Click({
    if (Attach) { $lblStatus.Text = "Attached to Minecraft.Windows.exe (PID $script:ProcId)"; RefreshView }
    else { $lblStatus.Text = "Could not attach. Is Minecraft running?" }
})

$btnLoadPlayer.Add_Click({
    $log = Join-Path $env:TEMP 'PebbleCore.log'
    if (-not (Test-Path $log)) { $lblStatus.Text = "No PebbleCore.log found (inject + join a world first)."; return }
    $m = Select-String -Path $log -Pattern 'player=([0-9A-Fa-f]+)' | Select-Object -Last 1
    if ($m) { $txtAddr.Text = "0x" + $m.Matches[0].Groups[1].Value; RefreshView; $lblStatus.Text = "Loaded player pointer from log." }
    else { $lblStatus.Text = "No player pointer in log yet." }
})

$btnWrite.Add_Click({
    $addr = ParseAddr $txtAddr.Text
    if ($addr -eq 0) { $lblStatus.Text = "Enter an address first."; return }
    $t = $cmbType.SelectedItem
    $v = $txtValue.Text.Trim()
    try {
        switch ($t) {
            "Int32"  { $bytes = [BitConverter]::GetBytes([int]$v) }
            "UInt32" { $bytes = [BitConverter]::GetBytes([uint32]$v) }
            "Int64"  { $bytes = [BitConverter]::GetBytes([long]$v) }
            "Float"  { $bytes = [BitConverter]::GetBytes([single]$v) }
            "Double" { $bytes = [BitConverter]::GetBytes([double]$v) }
            "Bytes"  { $bytes = ($v -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { [Convert]::ToByte($_.Replace('0x',''),16) }) }
        }
        if (WriteBytes $addr $bytes) { $lblStatus.Text = "Wrote $($bytes.Length) bytes to 0x$($addr.ToString('X'))."; RefreshView }
        else { $lblStatus.Text = "Write failed." }
    } catch { $lblStatus.Text = "Bad value for type $t : $($_.Exception.Message)" }
})

$btnResolve.Add_Click({
    $cur = ParseAddr $txtBase.Text
    if ($cur -eq 0) { $lblStatus.Text = "Enter a base address."; return }
    $offs = @($txtOff.Text -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { ParseAddr $_ })
    if ($offs.Count -eq 0) { $txtAddr.Text = "0x" + $cur.ToString('X'); RefreshView; return }
    # [base] then deref through all but last offset, add last offset
    $p = $cur
    for ($i=0; $i -lt $offs.Count; $i++) {
        if ($i -lt $offs.Count - 1) {
            $b = ReadBytes ($p + $offs[$i]) 8
            if ($null -eq $b) { $lblStatus.Text = "Deref failed at step $i."; return }
            $p = [BitConverter]::ToUInt64($b,0)
        } else {
            $p = $p + $offs[$i]
        }
    }
    $txtAddr.Text = "0x" + $p.ToString('X')
    $lblStatus.Text = "Resolved to 0x$($p.ToString('X'))."
    RefreshView
})

$timer = New-Object Windows.Forms.Timer
$timer.Interval = 400
$timer.Add_Tick({ if ($chkLive.Checked) { RefreshView } })
$timer.Start()

$txtAddr.Add_KeyDown({ if ($_.KeyCode -eq "Return") { RefreshView; $_.SuppressKeyPress = $true } })

# Try to attach on launch
if (Attach) { $lblStatus.Text = "Attached to Minecraft.Windows.exe (PID $script:ProcId)" }

[void]$form.ShowDialog()
if ($script:Handle -ne [IntPtr]::Zero) { [void][Mem.Native]::CloseHandle($script:Handle) }
