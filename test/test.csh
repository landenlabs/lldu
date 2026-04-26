#
#  Compare old and new version of lldupdir
#  Generate 1k files and time
#  Generate 8k files and time
#

rm file*
(dd if=/dev/urandom bs=1024 count=10240 | split -a 4 -b 1k - file.) >& /dev/null

echo "=== 1k old ===="
time lldupdirOld -all -file . > /dev/null
time lldupdirOld -all -file . > /dev/null
time lldupdirOld -all -file . > /dev/null

echo "=== 1k new ===="
time lldupdir -all -file . > /dev/null
time lldupdir -all -file . > /dev/null
time lldupdir -all -file . > /dev/null


rm file*
(dd if=/dev/urandom bs=1024 count=80240 | split -a 4 -b 8k - file.) >& /dev/null

echo "=== 8k old ===="
time lldupdirOld -all -file . > /dev/null
time lldupdirOld -all -file . > /dev/null
time lldupdirOld -all -file . > /dev/null

echo "=== 8k new ===="
time lldupdir -all -file . > /dev/null
time lldupdir -all -file . > /dev/null
time lldupdir -all -file . > /dev/null

rm file*
