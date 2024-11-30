@echo off

set prog=lldu
cd %prog%-ms

@echo Clean up remove x64
rmdir /s  x64

@echo.
@echo Build release target
F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe %prog%.sln /Build  "Release|x64"
cd ..

@echo Copy Release to d:\opt\bin
copy %prog%-ms\x64\Release\%prog%.exe d:\opt\bin\%prog%.exe

@echo.
@echo Compare md5 hash
cmp -h %prog%-ms\x64\Release\%prog%.exe d:\opt\bin\%prog%.exe
ld -a d:\opt\bin\%prog%.exe

