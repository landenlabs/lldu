<table border="0">
  <tr>
    <td>
      <!-- VERSION -->v6.09.10<br>
      <!-- DATE -->13-Sep-2026<br>
      Win & MacOS<br>
      <a href="https://landenlabs.com">Home</a>
    </td>
    <td>
      <a href="https://landenlabs.com">
        <img src="screens/landen_labs_300.webp" width="300" alt="LanDen Labs">
      </a>
    </td>
  </tr>
</table>

# lldu
OSX / Linux / DOS  Directory (disk) used space inventory

  [![Build status](https://github.com/landenlabs/lldu/actions/workflows/build.yml/badge.svg)](https://github.com/landenlabs/lldu/actions/workflows/build.yml)
  [![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](llcommon/LICENSE)
  ![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
  ![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey.svg)

Count files and their size grouped by their file extension.

### Dependencies
* [llcommon](https://github.com/landenlabs/llcommon) - shared LanDen Labs utility library (git submodule)

Visit home website

[https://landenlabs.com](https://landenlabs.com)


Help Banner:
<pre>
lldu  Dennis Lang v6.08.01 (LandenLabs.com) Aug 13 2026

Des: Directory (disk) used space inventory
Use: lldu [options] directories...   or  files

 Options (only first unique characters required, options can be repeated):
   -includeItem=&lt;fileOrDirPattern>
   -excludeItem=&lt;fileOrDirPattern> ; Exclude file or directory item
   Note - following start with uppercase I or E
   -IncludePath=&lt;pathPattern>      ; Match against full dir path
   -ExcludePath=&lt;pathPattern>      ; Match against full dir path
   NOTE - Patterns above - remember to escape backslash as \\
   -verbose
   -progress                       ; Show scan progress every 30 sec
   -pick=&lt;fromPat>;&lt;toStr>         ; Def: ..*[.](.+);$1
   -format=&lt;format-3-values>       ; Def: %8.8e\t%8c\t%15s\n
        e=ext, c=count, l=links, s=size, n=name
   -format=&lt;format-3-values>       ; Second format for Total
   -FormatSummary=&lt;format-1-value> ; Summary Format, Def: "%15s Files:%5c \t%n"
   -sort=ext|count|size            ; Def: ext
   -reverse=ext|count|size         ; Reverse sort
   -header=&lt;header>                ; Def: Ext\tCount\tSize\n
   -total                          ; Single report for all inputs
   -summary                        ; Single row for each path
   -summary=&lt;dirPat>               ; Sumarize matching dirs
   -table=count|size|links         ; Present results in table
   -tree &lt;depth>  or  -tree=&lt;depth>  ; Show directory tree, file count & size
                                       ; per level, Def depth=3
   -divide                         ; Divide size by hardlink count

   -column=access|create|modify|size|link ; Side-by-size 2 or more dirs
   -CFMT=%15.15s\t               ; 1st col format name

   -regex                       ; Use regex pattern not DOS pattern
   NOTE - Default DOS pattern internally treats * at .*, . at [.] and ? at .
           If using -regex specify before pattern options
          Use -regex if you need advance pattern syntax
   Example to ignore all dot directories and files:
          -regex -exclude="[.].*"
        or with DOS pattern
          -exclude=".*"

 Special Commands:
    -list                          ; List devices & storage size

 Example:
   lldu  -tree 5 some-directory    ; Tree view, 5 levels deep
   lldu  -sum -Exc=*.git  *
   lldu  -sum -Exc='*/.git'  *
   lldu  -sum -Exc='*/.(git|vs)' *
   lldu  -sum -regex -Exc='.*/[.](git|vs)' *
   lldu  -regex -sum='*/[.](git|vs)' *
   lldu  -sum -regex -exc="[.](git||vs)" *
   lldu  -formatSum="%15s Files:%5c Links:%l\t %n\n" -sum ..\*
   lldu '-inc=*.bak' -ex=foo.json '-ex=*/subdir2' dir1/subdir dir2 *.txt file2.json
   lldu '-exclude=\.*' '-pick=[^.]+[.](.{4,});other' .
   lldu '-exclude=\.*' '-pick=[^.]+[.](.{4,});other' -sort=size -rev=count .
   lldu  -rev=size -rev=count -format='%8e %6c %20s\n' -for='\n' -head=' ' .
   lldu  -format="%9.9e\t%8c\t%15s\n" -format="%9.9e\t%8c\t%15s\n"  .
   lldu  -FormatSummary="%8.8n\t%8c\t%15s\n"  .
   lldu  -ver -Include='*/[.][a-zA-Z]*' ~/

 Show hardlinks (%l or %L format)
   lldu  -header="   Exten\tFileSize\tLinks\n" -format="%8.8e\t%8s\t%5L\n"  .

 Side-by-side
   lldu  -CFMT=" % 25.25s\t"  -col=size -d=2 dir1 dir2

 Format:
    uses standard printf formatting except for these special cases
    e=file extension, c=count, s=size, l=links, n=name (with summary)
    lowercase c,s,l  format with commas
    uppercase  C,S,L  format without commas
    precede with width, ex %12.12e\t%8c\t%15s\n

 Output:
    Ext  Count  Size
    ext1 count1 size1
    ext2 count2 size2
    Total count size
</pre>

### License

```
Copyright 2026 Dennis Lang

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
See [llcommon/LICENSE](llcommon/LICENSE) for the full license text.
