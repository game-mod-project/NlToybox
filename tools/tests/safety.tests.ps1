# setup-aurie / restore-game / check-load 의 안전 동작을 게임을 건드리지 않고 시험한다.
# 임시 폴더에 가짜 게임(작은 x64 exe 를 Norland.exe 로 복사)을 만들고 NORLAND_GAME_DIR 로 가리킨다.
# 게임을 켜지 않는다. check-load 를 돌릴 때는 Start-Process 를 가로채 켜려는 시도만 잡아낸다.
#
# 사용: pwsh -File tools/tests/safety.tests.ps1     (게임이 꺼져 있어야 한다)
$ErrorActionPreference = 'Stop'
$tools = Split-Path -Parent $PSScriptRoot
. (Join-Path $tools 'common.ps1')

$realGame = $script:NlDefaultGameDir
$fake = Join-Path ([IO.Path]::GetTempPath()) 'nl-fakegame'
$pristine = Join-Path ([IO.Path]::GetTempPath()) 'nl-fakegame-orig.exe'
if (Test-Path -LiteralPath $fake) { Remove-Item -LiteralPath $fake -Recurse -Force }
New-Item -ItemType Directory -Force -Path $fake | Out-Null
Copy-Item -LiteralPath (Get-Process -Id $PID).Path -Destination $pristine -Force      # pwsh.exe: 작은 x64 PE
Copy-Item -LiteralPath $pristine -Destination (Join-Path $fake 'Norland.exe')

$savedEnv = $env:NORLAND_GAME_DIR
$env:NORLAND_GAME_DIR = $fake
$fake = Get-NlGameDir
Assert-True ($fake -ne $realGame) '가짜 게임 폴더가 진짜 게임 폴더와 달라야 한다'

# 도구를 새 pwsh 로 돌리고 출력과 종료 코드를 돌려준다. NORLAND_GAME_DIR 은 자식이 물려받는다.
function Invoke-Tool([string]$Name, [string]$ToolArgs = '', [string]$Prelude = '') {
    $out = pwsh -NoProfile -Command "$Prelude & '$(Join-Path $tools $Name)' $ToolArgs" 2>&1 | Out-String
    [pscustomobject]@{ Exit = $LASTEXITCODE; Out = $out }
}

# check-load 가 게임을 켜려 하면 이 문자열이 출력에 남는다. 실제로 켜지지는 않는다.
$noLaunch = "function Start-Process { throw 'LAUNCH-ATTEMPTED' };"

$script:passed = 0
function Test-Case([string]$Name, [scriptblock]$Body) {
    & $Body
    $script:passed++
    Write-Host "ok - $Name"
}

$orig = Get-NlExeInfo
Assert-True ($orig.Path.StartsWith($fake)) '시험 대상 exe 가 가짜 게임 폴더 안에 있어야 한다'   # 아래 정리 단계가 이 버전의 백업을 지운다
$backupDir = Join-Path (Get-NlRepoRoot) 'backups'
$backupPath = Join-Path $backupDir "Norland.exe.$($orig.Version).$($orig.Sha256.Substring(0, 12))"
$garbage = [byte[]](1..10)

try {
    Test-Case 'setup 은 내용이 원본과 다른 기존 백업을 믿지 않고 다시 만든다' {
        New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
        [IO.File]::WriteAllBytes($backupPath, $garbage)      # 복사가 중간에 끊겨 남은 백업을 흉내 낸다
        $r = Invoke-Tool 'setup-aurie.ps1'
        Assert-Equal $r.Exit 0 "setup 종료 코드`n$($r.Out)"
        Assert-Equal (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash $orig.Sha256 '백업 내용이 원본과 같아야 한다'
        Assert-True (Get-NlExeInfo).Patched 'exe 가 패치되어야 한다'
        Assert-Equal @(Get-ChildItem -LiteralPath $backupDir -Filter '*.partial').Count 0 '임시 파일이 남지 않아야 한다'
    }

    Test-Case 'restore 는 내용이 이름의 해시와 다른 백업으로 덮어쓰지 않는다' {
        $patched = (Get-NlExeInfo).Sha256
        [IO.File]::WriteAllBytes($backupPath, $garbage)      # 패치한 뒤 백업이 망가진 상황
        $r = Invoke-Tool 'restore-game.ps1'
        Assert-Equal $r.Exit 1 "restore 는 거부해야 한다`n$($r.Out)"
        Assert-Equal (Get-NlExeInfo).Sha256 $patched 'exe 는 그대로여야 한다'
    }

    Test-Case 'restore 는 온전한 백업으로 원본을 되살린다' {
        Copy-Item -LiteralPath $pristine -Destination $backupPath -Force
        $r = Invoke-Tool 'restore-game.ps1'
        Assert-Equal $r.Exit 0 "restore 종료 코드`n$($r.Out)"
        Assert-Equal (Get-NlExeInfo).Sha256 $orig.Sha256 'exe 가 원본과 같아야 한다'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fake 'mods'))) 'mods 가 없어야 한다'
    }

    Test-Case 'check-load 는 패치되지 않은 exe 로 게임을 켜지 않는다' {
        # Steam 갱신으로 exe 만 바닐라로 돌아가고 mods 는 남은 상황
        New-Item -ItemType Directory -Force -Path (Join-Path $fake 'mods\Aurie') | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $fake 'mods\Aurie\NlToyBox.dll'), $garbage)
        $r = Invoke-Tool 'check-load.ps1' '-TimeoutSec 4' $noLaunch
        Assert-Equal $r.Exit 1 "check-load 종료 코드`n$($r.Out)"
        Assert-True ($r.Out -notmatch 'LAUNCH-ATTEMPTED') "게임을 켜려 하면 안 된다`n$($r.Out)"
        Assert-True ($r.Out -match 'setup-aurie') "무엇을 해야 하는지 알려 줘야 한다`n$($r.Out)"
    }

    Test-Case 'check-load 는 Aurie DLL 이 없으면 게임을 켜지 않는다' {
        $r = Invoke-Tool 'setup-aurie.ps1'
        Assert-Equal $r.Exit 0 "setup 종료 코드`n$($r.Out)"
        Remove-Item -LiteralPath (Join-Path $fake 'mods\Native\AurieCore.dll') -Force
        $r = Invoke-Tool 'check-load.ps1' '-TimeoutSec 4' $noLaunch
        Assert-Equal $r.Exit 1 "check-load 종료 코드`n$($r.Out)"
        Assert-True ($r.Out -notmatch 'LAUNCH-ATTEMPTED') "게임을 켜려 하면 안 된다`n$($r.Out)"
        Assert-True ($r.Out -match 'AurieCore\.dll') "빠진 파일을 알려 줘야 한다`n$($r.Out)"
    }

    Test-Case 'check-load 는 준비가 다 되어 있으면 게임을 켜는 데까지 간다' {
        $r = Invoke-Tool 'setup-aurie.ps1'                    # 빠진 AurieCore.dll 을 다시 놓는다
        Assert-Equal $r.Exit 0 "setup 종료 코드`n$($r.Out)"
        $r = Invoke-Tool 'check-load.ps1' '-TimeoutSec 4' $noLaunch
        Assert-True ($r.Out -match 'LAUNCH-ATTEMPTED') "사전 검사를 지나 켜는 단계에 닿아야 한다`n$($r.Out)"
    }

    $x = Join-Path $fake 'x.json'

    Test-Case 'data-snapshot 은 바닐라를 보관하고, 달라진 파일에는 거부한다' {
        [IO.File]::WriteAllText($x, 'AAA=1')
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files x.json'
        Assert-Equal $r.Exit 0 "snapshot 종료 코드`n$($r.Out)"
        [IO.File]::WriteAllText($x, 'AAA=2')                     # 스냅샷을 뜬 뒤 고친 파일
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files x.json'
        Assert-Equal $r.Exit 1 "고친 파일을 바닐라로 보관하면 안 된다`n$($r.Out)"
    }

    Test-Case 'data-edit 은 찾는 글이 정확히 한 번일 때만 바꾼다' {
        [IO.File]::WriteAllText($x, 'AAA=1 BBB=1 BBB=1')
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find BBB=1 -Replace BBB=9'
        Assert-Equal $r.Exit 1 "두 번 나오는 글은 거부해야 한다`n$($r.Out)"
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find CCC=1 -Replace CCC=9'
        Assert-Equal $r.Exit 1 "없는 글은 거부해야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=1 BBB=1 BBB=1' '거부했으면 파일은 그대로여야 한다'
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find AAA=1 -Replace AAA=7'
        Assert-Equal $r.Exit 0 "edit 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=7 BBB=1 BBB=1' '한 번 나오는 글만 바뀌어야 한다'
    }

    Test-Case 'data-restore 는 스냅샷으로 되돌린다' {
        $r = Invoke-Tool 'data-restore.ps1'
        Assert-Equal $r.Exit 0 "restore 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=1' '스냅샷의 내용으로 돌아와야 한다'
    }

    Test-Case '게임 폴더 밖을 가리키는 경로는 거부한다' {
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files ..\x.json'
        Assert-Equal $r.Exit 1 "snapshot 은 .. 을 거부해야 한다`n$($r.Out)"
        $r = Invoke-Tool 'data-edit.ps1' '-File ..\x.json -Find A -Replace B'
        Assert-Equal $r.Exit 1 "edit 은 .. 을 거부해야 한다`n$($r.Out)"
    }

    Test-Case 'probe 는 켜는 데 실패해도 요청 파일을 게임 폴더에 남기지 않는다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out.json')' -TimeoutSec 4" $noLaunch
        Assert-True ($r.Out -match 'LAUNCH-ATTEMPTED') "사전 검사를 지나 켜는 단계에 닿아야 한다`n$($r.Out)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fake 'mods\Aurie\NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    Write-Host "safety tests: $($script:passed) passed"
}
finally {
    $env:NORLAND_GAME_DIR = $savedEnv
    if (Test-Path -LiteralPath $fake) { Remove-Item -LiteralPath $fake -Recurse -Force }
    if (Test-Path -LiteralPath $pristine) { Remove-Item -LiteralPath $pristine -Force }
    $fakeData = Join-Path $backupDir "data\$($orig.Version)"   # 가짜 게임의 버전이다. 진짜 게임의 스냅샷은 버전이 달라 걸리지 않는다.
    if (Test-Path -LiteralPath $fakeData) { Remove-Item -LiteralPath $fakeData -Recurse -Force }
    # 가짜 게임의 백업만 지운다. 진짜 게임의 백업은 버전이 달라 이 필터에 걸리지 않는다.
    Get-ChildItem -LiteralPath $backupDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "Norland.exe.$($orig.Version).*" -or $_.Name -like '*.partial' } |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
}
