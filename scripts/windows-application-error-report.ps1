param(
    [Parameter(Mandatory = $true)]
    [string]$ApplicationPath,

    [Parameter(Mandatory = $true)]
    [string]$StartTime
)

$ErrorActionPreference = "Stop"

try {
    $start = [datetime]::Parse($StartTime, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)
} catch {
    Write-Output "Could not parse runtime start time: $StartTime"
    exit 0
}

$exeName = Split-Path -Leaf $ApplicationPath
$normalizedPath = [System.IO.Path]::GetFullPath($ApplicationPath)

try {
    $events = Get-WinEvent -FilterHashtable @{ LogName = "Application"; Id = 1000; StartTime = $start } -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Message -like "*Faulting application name: $exeName*" -and
            ($_.Message -like "*$normalizedPath*" -or $_.Message -like "*$exeName*")
        } |
        Select-Object -First 3
} catch {
    Write-Output "Could not query Windows Application Error events: $($_.Exception.Message)"
    exit 0
}

if (-not $events) {
    Write-Output "No matching Windows Application Error event was found after $StartTime."
    exit 0
}

foreach ($event in $events) {
    Write-Output "TimeCreated: $($event.TimeCreated.ToString('o'))"
    foreach ($line in ($event.Message -split "`r?`n")) {
        if ($line -match "Faulting application name:" -or
            $line -match "Faulting module name:" -or
            $line -match "Exception code:" -or
            $line -match "Fault offset:" -or
            $line -match "Faulting process id:" -or
            $line -match "Faulting application path:" -or
            $line -match "Faulting module path:" -or
            $line -match "Report Id:") {
            Write-Output $line.Trim()
        }
    }
    Write-Output ""
}
