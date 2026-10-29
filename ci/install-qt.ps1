# Ставит готовый Qt для MSVC x64 прямо из официального репозитория Qt
# (тот же, из которого качает Qt Online Installer), без учётной записи Qt.
#
#   pwsh ci/install-qt.ps1 -Version 6.11.2 -OutDir C:\Qt
#   → C:\Qt\6.11.2\msvc2022_64 (его и передавать в CMAKE_PREFIX_PATH)
#
# Почему не aqtinstall / install-qt-action: aqtinstall 3.3.0 не знает новую
# раскладку репозитория Qt 6.11 (папка на архитектуру: qt6_6112/qt6_6112_msvc2022_64).
#
# Как работает: читает Updates.xml архитектуры, берёт базовый пакет
# qt.qt6.<ver>.<arch> (qtbase, qtdeclarative, qtsvg, qttools, …) и распаковывает
# его архивы туда, куда велят операции Extract из того же Updates.xml.
# Каждый архив сверяется с SHA-1 из репозитория. Документация (qtdoc) пропускается.

param(
    [Parameter(Mandatory)] [string] $Version,         # 6.11.2
    [Parameter(Mandatory)] [string] $OutDir,          # каталог, где появится <Version>\msvc2022_64
    [string] $Arch = 'win64_msvc2022_64',
    [string] $Mirror = 'https://download.qt.io',
    [string[]] $Skip = @('qtdoc')                     # архивы (по началу имени), которые не нужны
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'              # без этого Invoke-WebRequest в разы медленнее

$v = [version]$Version
$tag = "qt$($v.Major)_$($v.Major)$($v.Minor)$($v.Build)"            # qt6_6112
$archDir = $Arch -replace '^win64_', ''                              # msvc2022_64
$repo = "$Mirror/online/qtsdkrepository/windows_x86/desktop/$tag/${tag}_$archDir"
$package = "qt.qt$($v.Major).$($v.Major)$($v.Minor)$($v.Build).$Arch"

function Get-Text([string] $url) {
    for ($i = 1; ; $i++) {
        try {
            $c = (Invoke-WebRequest $url -UseBasicParsing -TimeoutSec 120).Content
            # Для application/octet-stream (.sha1) приходят байты, не строка.
            if ($c -is [byte[]]) { $c = [Text.Encoding]::UTF8.GetString($c) }
            return $c
        }
        catch { if ($i -ge 3) { throw "Не скачать $url : $_" }; Start-Sleep -Seconds (5 * $i) }
    }
}

function Save-File([string] $url, [string] $path) {
    for ($i = 1; ; $i++) {
        try { Invoke-WebRequest $url -OutFile $path -UseBasicParsing -TimeoutSec 900; return }
        catch { if ($i -ge 3) { throw "Не скачать $url : $_" }; Start-Sleep -Seconds (5 * $i) }
    }
}

$sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source
if (-not $sevenZip -and (Test-Path "$env:ProgramFiles\7-Zip\7z.exe")) { $sevenZip = "$env:ProgramFiles\7-Zip\7z.exe" }
if (-not $sevenZip) { throw 'Нужен 7-Zip (7z.exe)' }

Write-Host "Qt $Version $Arch из $repo"
$updates = [xml](Get-Text "$repo/Updates.xml")
$pkg = $updates.Updates.PackageUpdate | Where-Object { $_.Name -eq $package }
if (-not $pkg) { throw "В $repo/Updates.xml нет пакета $package" }

$prefix = Join-Path $OutDir "$Version\$archDir"
$tmp = Join-Path ([IO.Path]::GetTempPath()) "rfvk-qt-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Force $tmp | Out-Null

try {
    foreach ($op in $pkg.Operations.Operation | Where-Object { $_.name -eq 'Extract' }) {
        $target, $archive = $op.Argument
        if ($Skip | Where-Object { $archive.StartsWith($_) }) { Write-Host "  пропуск $archive"; continue }

        # @TargetDir@/6.11.2/msvc2022_64[/bin] → <OutDir>\6.11.2\msvc2022_64[\bin]
        $dest = Join-Path $OutDir ($target -replace '^@TargetDir@/?', '')
        $url = "$repo/$package/$($pkg.Version)$archive"
        $file = Join-Path $tmp $archive

        Write-Host "  $archive"
        Save-File $url $file
        $expected = ((Get-Text "$url.sha1") -split '\s+')[0].Trim().ToLower()
        $actual = (Get-FileHash $file -Algorithm SHA1).Hash.ToLower()
        if ($expected -ne $actual) { throw "SHA-1 не совпал для $archive (ожидался $expected, получен $actual)" }

        & $sevenZip x $file "-o$dest" -y -bso0 -bsp0
        if ($LASTEXITCODE -ne 0) { throw "7z не распаковал $archive (код $LASTEXITCODE)" }
        Remove-Item $file
    }
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

if (-not (Test-Path "$prefix\lib\cmake\Qt6\Qt6Config.cmake")) { throw "После установки нет $prefix\lib\cmake\Qt6\Qt6Config.cmake" }
Write-Host "Готово: $prefix"
