#lldu
OSX / Linux / DOS  Directory (disk) used space inventory 

Count files and their size grouped by their file extension.

  [![Build status](https://travis-ci.org/landenlabs/lldu.svg?branch=master)](https://travis-ci.org/landenlabs/lldu)
  

Visit home website

[http://landenlabs.com](http://landenlabs.com)


Help Banner:
<pre>
lldu  Dennis Lang v1.1 (LandenLabs.com) May 20 2020

Des: Directory (disk) used space inventory
Use: lldu [options] directories...   or  files

 Options (only first unique characters required, options can be repeated):
   -includefile=&lt;filePattern>
   -excludefile=&ltfilePattern>
   -verbose
   -pick=&ltfromPat>;&lttoStr>         ; Def: [^.]*[.](.+);$1
   -format=&ltformat-3-values>       ; Def: %e	%c	%s\n
        e=ext, c=count, s=size   
   -header=&ltReportHeader>          ; Def: Ext\tCount\tSize\n 

 Example:
   lldu '-inc=*.bak' -ex=foo.json '-ex=*/subdir2' dir1/subdir dir2 *.txt file2.json
   lldu '-exclude=\.*' '-pick=([^.]+)[.](.{4,});$1.other' .

  Output:
    Ext  Count  Size
    ext1 count1 size1
    ext2 count2 size2

</pre>
