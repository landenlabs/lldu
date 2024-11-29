#lldu
OSX / Linux / DOS  Directory (disk) used space inventory

Count files and their size grouped by their file extension.

  [![Build status](https://travis-ci.org/landenlabs/lldu.svg?branch=master)](https://travis-ci.org/landenlabs/lldu)


Visit home website

[https://landenlabs.com](https://landenlabs.com)


Help Banner:
<pre>
lldu  Dennis Lang v2.1 (LandenLabs.com) Nov 28 2024

Des: Directory (disk) used space inventory
Use: lldu [options] directories...   or  files

 Options (only first unique characters required, options can be repeated):
   -includefile=<filePattern>
   -excludefile=<filePattern>
   -Includedir=<dirPattern>        ; Match against full dir path
   -Excludedir=<dirPattern>        ; Match against full dir path
   -verbose
   -progress                       ; Show scan progress every 30 sec
   -pick=<fromPat>;<toStr>         ; Def: ..*[.](.+);$1
   -format=<format-3-values>       ; Def: %8.8e\t%8c\t%15s\n
        e=ext, c=count, l=links, s=size, n=name
   -format=<format-3-values>       ; Second format for Total
   -FormatSummary=<format-1-value> ; Summary Format, Def: "%S %e\n"
   -sort=ext|count|size            ; Def: ext
   -reverse=ext|count|size         ; Reverse sort
   -header=<header>                ; Def: Ext\tCount\tSize\n
   -total                          ; Single report for all inputs
   -summary                        ; Single row for each path
   -table=count|size|links         ; Present results in table
   -divide                         ; Divide size by hardlink count

 Example:
   lldu '-inc=*.bak' -ex=foo.json '-ex=*/subdir2' dir1/subdir dir2 *.txt file2.json
   lldu '-exclude=\.*' '-pick=[^.]+[.](.{4,});other' .
   lldu '-exclude=\.*' '-pick=[^.]+[.](.{4,});other' -sort=size -rev=count .
   lldu  -rev=size -rev=count -format='%8e %6c %10s\n' -for='\n' -head=' ' .
   lldu  -format="%9.9e\t%8c\t%15s\n" -format="%9.9e\t%8c\t%15s\n"  .
   lldu  -FormatSummary="%8.8n\t%8c\t%15s\n"  .
   lldu  -ver -Include='*/[.][a-zA-Z]*' ~/

 Show hardlinks (%l or %L format)
   lldu  -header="   Exten\tFileSize\tLinks\n" -format="%8.8e\t%8s\t%5L\n"  .

 Format:
    uses standard printf formatting except for these special cases
    e=file extension, c=count, s=size, l=links
    lowercase c,s,l  format with commas
    uppercas  C,S,L  format without commas
    preceed with width, ex %12.12e\t%8c\t%10s\n

 Output:
    Ext  Count  Size
    ext1 count1 size1
    ext2 count2 size2
    Total count size
</pre>
