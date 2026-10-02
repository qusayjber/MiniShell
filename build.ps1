# build.ps1 — يكتشف تلقائياً أفضل gcc متاح
$ErrorActionPreference = "Stop"
Set-Location "D:\Dev\Projects\c\myshell"

$gccCandidates = @(
    "C:\msys64\ucrt64\bin\gcc.exe",   # MSYS2 UCRT64
    "C:\msys64\mingw64\bin\gcc.exe",  # MSYS2 MINGW64
    "C:\msys64\clang64\bin\gcc.exe"   # MSYS2 CLANG64
)

$gcc = $null
foreach ($c in $gccCandidates) {
    if (Test-Path $c) { $gcc = $c; break }
}

if (-not $gcc) {
    Write-Host "✘ لم يتم العثور على MSYS2 GCC" -ForegroundColor Red
    Write-Host ""
    Write-Host "الخيارات المتاحة:" -ForegroundColor Yellow
    Write-Host "  A) ثبّت MSYS2:  winget install MSYS2.MSYS2"
    Write-Host "     ثم افتح MSYS2 UCRT64 وشغّل:"
    Write-Host "     pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc make"
    Write-Host ""
    Write-Host "  B) استخدم WSL (أفضل):"
    Write-Host "     wsl --install -d Ubuntu"
    Write-Host "     wsl -d Ubuntu -- bash -lc `"sudo apt install -y build-essential`""
    Write-Host "     wsl -d Ubuntu -- bash -lc `"cd /mnt/d/Dev/Projects/c/myshell && gcc -o minishell shell.c && ./minishell`""
    exit 1
}

Write-Host "✔ GCC: $gcc" -ForegroundColor Green
& $gcc --version | Select-Object -First 1

Write-Host "`n🔨 جارٍ البناء..." -ForegroundColor Cyan
& $gcc -Wall -Wextra -O2 -std=c11 -o minishell.exe shell.c

if ($LASTEXITCODE -ne 0) {
    Write-Host "✘ فشل البناء" -ForegroundColor Red
    exit 1
}

Write-Host "✔ تم البناء بنجاح`n" -ForegroundColor Green
Write-Host "🚀 تشغيل MiniShell...`n" -ForegroundColor Cyan
.\minishell.exe