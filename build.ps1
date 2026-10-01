$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
$files = @("MainSystem.c", "TeacherGeneration.c", "ExamRoomGeneration.c")
foreach ($f in $files) {
    if (Test-Path $f) {
        $content = [System.IO.File]::ReadAllText($f)
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        [System.IO.File]::WriteAllText($f, $content, $utf8NoBom)
    }
}
gcc -finput-charset=UTF-8 -fexec-charset=GBK -g -o MainSystem.exe MainSystem.c -mwindows -luser32 -lcomctl32
$e1 = $LASTEXITCODE
gcc -finput-charset=UTF-8 -fexec-charset=GBK -g -o TeacherGeneration.exe TeacherGeneration.c
$e2 = $LASTEXITCODE
gcc -finput-charset=UTF-8 -fexec-charset=GBK -g -o ExamRoomGeneration.exe ExamRoomGeneration.c
$e3 = $LASTEXITCODE
Write-Host "MainSystem_exit=$e1"
Write-Host "TeacherGeneration_exit=$e2"
Write-Host "ExamRoomGeneration_exit=$e3"
