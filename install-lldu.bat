@echo off

@echo Copy Release to d:\opt\bin

cd lldu-ms

echo "Clean up remove x64"
lr -rfq x64
echo "Build release target"
F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe lldu.sln /Build  "Release|x64"
cd ..

copy lldu-ms\x64\Release\lldu.exe d:\opt\bin\lldu.exe


@echo
@echo Compare md5 hash
cmp -h lldu-ms\x64\Release\lldu.exe d:\opt\bin\lldu.exe
ld -a d:\opt\bin\lldu.exe

