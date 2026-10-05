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
        foreach ($n in 'NlToyBox.dump.menu.json', 'NlToyBox.dump.late2.json') {      # probe 가 도중에 죽어 남은 덤프
            [IO.File]::WriteAllText((Join-Path $fake "mods\Aurie\$n"), '{}')
        }
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

    Test-Case 'data-edit 은 BOM 을 보존하고, UTF-8 이 아닌 파일은 건드리지 않는다' {
        $hex = { param($b) [BitConverter]::ToString([byte[]]$b) }
        [byte[]]$bom = [byte[]](0xEF, 0xBB, 0xBF) + [Text.Encoding]::ASCII.GetBytes('AAA=1')
        [IO.File]::WriteAllBytes($x, $bom)
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find AAA=1 -Replace AAA=7'
        Assert-Equal $r.Exit 0 "BOM 이 있는 파일의 edit 종료 코드`n$($r.Out)"
        Assert-Equal (& $hex ([IO.File]::ReadAllBytes($x))) (& $hex ([byte[]](0xEF, 0xBB, 0xBF) + [Text.Encoding]::ASCII.GetBytes('AAA=7'))) 'BOM 이 남고 값만 바뀌어야 한다'

        [byte[]]$bad = [Text.Encoding]::ASCII.GetBytes('AAA=1 ') + [byte[]](0xFF)
        [IO.File]::WriteAllBytes($x, $bad)
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find AAA=1 -Replace AAA=7'
        Assert-Equal $r.Exit 1 "UTF-8 이 아닌 파일은 거부해야 한다`n$($r.Out)"
        Assert-Equal (& $hex ([IO.File]::ReadAllBytes($x))) (& $hex $bad) '거부했으면 바이트가 그대로여야 한다'
    }

    Test-Case 'data-restore 는 스냅샷으로 되돌린다' {
        $r = Invoke-Tool 'data-restore.ps1'
        Assert-Equal $r.Exit 0 "restore 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=1' '스냅샷의 내용으로 돌아와야 한다'
    }

    Test-Case 'data-restore 는 지금 버전의 스냅샷이 없고 다른 버전 것만 있으면 거부한다' {
        $dataDir = Join-Path $backupDir 'data'
        Rename-Item -LiteralPath (Join-Path $dataDir $orig.Version) -NewName '0.0.0.1-test'     # 게임이 갱신돼 버전이 바뀐 상황
        try {
            $r = Invoke-Tool 'data-restore.ps1'
            Assert-True ($r.Exit -eq 1 -and $r.Out -match '다른 버전') "아무것도 안 하고 성공이라고 하면 안 된다`n$($r.Out)"
        }
        finally { Rename-Item -LiteralPath (Join-Path $dataDir '0.0.0.1-test') -NewName $orig.Version }
    }

    Test-Case '게임 폴더 밖을 가리키는 경로는 경로 검사에서 거부한다' {
        $bait = Join-Path (Split-Path -Parent $fake) 'nl-bait.json'      # 가짜 게임 폴더의 바로 밖에 실제로 있는 파일
        [IO.File]::WriteAllText($bait, 'AAA=1')
        try {
            foreach ($rel in '..\nl-bait.json', '../nl-bait.json', 'sub\..\..\nl-bait.json', $bait) {
                $r = Invoke-Tool 'data-snapshot.ps1' "-Files '$rel'"
                Assert-True ($r.Exit -eq 1 -and $r.Out -match '상대경로여야') "snapshot 은 경로 검사에서 거부해야 한다: $rel`n$($r.Out)"
                $r = Invoke-Tool 'data-edit.ps1' "-File '$rel' -Find AAA=1 -Replace AAA=9"
                Assert-True ($r.Exit -eq 1 -and $r.Out -match '상대경로여야') "edit 은 경로 검사에서 거부해야 한다: $rel`n$($r.Out)"
            }
            Assert-Equal ([IO.File]::ReadAllText($bait)) 'AAA=1' '밖의 파일은 그대로여야 한다'
        }
        finally { Remove-Item -LiteralPath $bait -Force }
    }

    Test-Case 'probe 는 켜는 데 실패해도 요청 파일을 게임 폴더에 남기지 않는다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out.json')' -TimeoutSec 4" $noLaunch
        Assert-True ($r.Out -match 'LAUNCH-ATTEMPTED') "사전 검사를 지나 켜는 단계에 닿아야 한다`n$($r.Out)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fake 'mods\Aurie\NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    # 모듈과 게임 프로세스를 흉내 낸다. 게임을 켜는 대신 로그와 덤프를 mods\Aurie 에 써 넣고,
    # AliveSec 동안 게임이 떠 있는 것처럼 보이게 한다. Get-Process 와 Stop-Process 를 가로챌 뿐 실제 프로세스는 없다.
    $modDir = Join-Path $fake 'mods\Aurie'
    function New-FakeModule([string[]]$LogLines, [hashtable]$Dumps = @{}, [int]$AliveSec = 0) {
        $body = "Set-Content -LiteralPath '$modDir\NlToyBox.log' -Value @($(($LogLines | ForEach-Object { "'$_'" }) -join ', '));"
        foreach ($name in $Dumps.Keys) { $body += " Set-Content -LiteralPath '$modDir\$name' -Value '$($Dumps[$name])';" }
        $body += " `$global:NlFakeUntil = (Get-Date).AddSeconds($AliveSec);"
        "function Start-Process { $body }; " +
        "function Get-Process { if (`$global:NlFakeUntil -and (Get-Date) -lt `$global:NlFakeUntil) { [pscustomobject]@{ Name = 'Norland-fake' } } }; " +
        "function Stop-Process { };"
    }

    Test-Case 'probe 는 덤프를 이름대로 -Out 옆으로 가져오고 게임 폴더에서 지운다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1', 'dump done') @{ 'NlToyBox.dump.menu.json' = 'MENU' }
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\one.json')' -TimeoutSec 20" $prelude
        Assert-Equal $r.Exit 0 "probe 종료 코드`n$($r.Out)"
        Assert-True (Test-Path -LiteralPath (Join-Path $fake 'out\one.menu.json')) "덤프를 이름대로 가져와야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $fake 'out\one.menu.json')).Trim()) 'MENU' '덤프의 내용'
        Assert-Equal @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.dump*').Count 0 '덤프가 게임 폴더에 남으면 안 된다'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    Test-Case 'probe 는 되풀이 요청이면 게임이 꺼질 때까지 기다렸다가 덤프를 모두 가져온다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`nrepeat_seconds=45`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1', 'dump late0 done seq 2') @{
            'NlToyBox.dump.menu.json' = 'MENU'; 'NlToyBox.dump.late0.json' = 'L0'; 'NlToyBox.dump.late1.json' = 'L1' } 6
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\rep.json')' -TimeoutSec 30" $prelude
        Assert-Equal $r.Exit 0 "probe 종료 코드`n$($r.Out)"
        Assert-True ($watch.Elapsed.TotalSeconds -ge 5) "게임이 꺼지기 전에 끝내면 안 된다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        Assert-True ($watch.Elapsed.TotalSeconds -lt 25) "게임이 꺼진 뒤에는 기다리지 않는다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        foreach ($pair in @{ 'menu' = 'MENU'; 'late0' = 'L0'; 'late1' = 'L1' }.GetEnumerator()) {
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $fake "out\rep.$($pair.Key).json")).Trim()) $pair.Value "덤프 $($pair.Key) 의 내용"
        }
        Assert-True ($r.Out -match '새 게임') "되풀이 요청이면 새 게임을 시작하라고 알려야 한다`n$($r.Out)"
        Assert-True ($r.Out -match '게임 종료: not-running') "이 도구가 게임을 끄면 안 된다. 사용자가 끈 뒤여야 한다`n$($r.Out)"
        Assert-Equal @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.dump*').Count 0 '덤프가 게임 폴더에 남으면 안 된다'
    }

    Test-Case 'probe 는 되풀이 요청인데 뒤의 덤프를 하나도 받지 못하면 실패한다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`nrepeat_seconds=45`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1') @{ 'NlToyBox.dump.menu.json' = 'MENU' } 4
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\none.json')' -TimeoutSec 30" $prelude
        Assert-True ($r.Exit -eq 1 -and $r.Out -match 'FAIL: 되풀이 덤프') "메뉴 덤프만으로 통과하면 안 된다`n$($r.Out)"
    }

    Test-Case 'probe 는 모듈이 dump failed 를 적으면 기다리지 않고 실패한다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $prelude = New-FakeModule @('request consumed', 'dump failed: bad request')
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\bad.json')' -TimeoutSec 40" $prelude
        Assert-True ($r.Exit -eq 1 -and $r.Out -match 'FAIL: dump failed: bad request') "모듈이 적은 이유로 실패해야 한다`n$($r.Out)"
        Assert-True ($watch.Elapsed.TotalSeconds -lt 25) "시간이 다 될 때까지 기다리면 안 된다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    Test-Case 'probe 는 덤프를 복사하지 못하면 게임 폴더의 덤프를 지우지 않는다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $blocked = Join-Path $fake 'blocked'
        [IO.File]::WriteAllText($blocked, 'x')      # 폴더가 와야 할 자리에 파일이 있다. 복사가 실패한다
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1', 'dump done') @{ 'NlToyBox.dump.menu.json' = 'MENU' }
        try {
            $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $blocked 'x.json')' -TimeoutSec 20" $prelude
            Assert-Equal $r.Exit 1 "복사하지 못했으면 실패여야 한다`n$($r.Out)"
            Assert-True (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.dump.menu.json')) "하나뿐인 덤프를 지우면 안 된다`n$($r.Out)"
            Assert-True (-not (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.probe.txt'))) '요청 파일은 그래도 지워야 한다'
        }
        finally {
            Remove-Item -LiteralPath (Join-Path $modDir 'NlToyBox.dump.menu.json') -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $blocked -Force
        }
    }

    Test-Case 'saves-backup 은 사본을 뜨고 달라진 것을 알려 주며, 세이브 폴더에는 쓰지 않는다' {
        $saves = Join-Path $fake 'saves-src'
        New-Item -ItemType Directory -Force -Path (Join-Path $saves 'saves') | Out-Null
        [IO.File]::WriteAllText((Join-Path $saves 'game_settings.json'), 'A')
        [IO.File]::WriteAllText((Join-Path $saves 'saves\one.norland'), 'B')
        $copy = Join-Path $fake 'saves-copy'
        $env:NORLAND_SAVES_DIR = $saves
        try {
            $r = Invoke-Tool 'saves-backup.ps1' "-To '$copy'"
            Assert-Equal $r.Exit 0 "saves-backup 종료 코드`n$($r.Out)"
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $copy 'saves\one.norland'))) 'B' '사본의 내용이 같아야 한다'
            Assert-Equal @(Get-ChildItem -LiteralPath $saves -Recurse -File).Count 2 '세이브 폴더에 파일을 만들면 안 된다'

            [IO.File]::WriteAllText((Join-Path $saves 'game_settings.json'), 'A2')      # 게임이 고친 파일
            [IO.File]::WriteAllText((Join-Path $saves 'saves\auto.norland'), 'C')       # 게임이 새로 쓴 세이브
            $r = Invoke-Tool 'saves-backup.ps1' "-Diff '$copy'"
            Assert-True ($r.Exit -eq 0 -and $r.Out -match '바뀜: game_settings\.json' -and $r.Out -match '새로 생김: saves\\auto\.norland' -and $r.Out -match 'saves diff \(2\)') "달라진 두 파일을 알려야 한다`n$($r.Out)"
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $saves 'game_settings.json'))) 'A2' '세이브 폴더의 파일은 그대로여야 한다'
            Assert-Equal @(Get-ChildItem -LiteralPath $saves -Recurse -File).Count 3 '세이브 폴더에 파일을 만들면 안 된다'
        }
        finally { $env:NORLAND_SAVES_DIR = $null }
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
