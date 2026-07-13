<table border="0">
  <tr>
    <td>
      <!-- VERSION -->v6.07.10<br>
      <!-- DATE -->12-Jul-2026<br>
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

  [![Build status](https://travis-ci.org/landenlabs/lldu.svg?branch=master)](https://travis-ci.org/landenlabs/lldu)
  [![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE.txt)
  ![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
  ![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey.svg)

Count files and their size grouped by their file extension.

### Dependencies
* [llcommon](https://github.com/landenlabs/llcommon) - shared LanDen Labs utility library (git submodule)

Visit home website

[https://landenlabs.com](https://landenlabs.com)


Help Banner:
<pre>
lldu  Dennis Lang v2.5 (LandenLabs.com) Dec 26 2024

Des: Directory (disk) used space inventory
Use: lldu [options] directories...   or  files

 Options (only first unique characters required, options can be repeated):
   -includeFile=&lt;filePattern>
   -excludeFile=&lt;filePattern>
   -IncludeDir=&lt;dirPattern>        ; Match against full dir path
   -ExcludeDir=&lt;dirPattern>        ; Match against full dir path
   NOTE - Patterns above - remember to escape backslash as \\
   -verbose
   -progress                       ; Show scan progress every 30 sec
   -pick=&lt;fromPat>;&lt;toStr>         ; Def: ..*[.](.+);$1
   -format=&lt;format-3-values>       ; Def: %8.8e\t%8c\t%15s\n
        e=ext, c=count, l=links, s=size, n=name
   -format=&lt;format-3-values>       ; Second format for Total
   -FormatSummary=&lt;format-1-value> ; Summary Format, Def: "%S %e\n"
   -sort=ext|count|size            ; Def: ext
   -reverse=ext|count|size         ; Reverse sort
   -header=&lt;header>                ; Def: Ext\tCount\tSize\n
   -total                          ; Single report for all inputs
   -summary                        ; Single row for each path
   -table=count|size|links         ; Present results in table
   -divide                         ; Divide size by hardlink count
   -regex                          ; FilePattern us native regex
   NOTE - default patterns convert * to .*, . to [.] and ? to .

 Example:
   lldu  -sum -Exc=*.git  *
   lldu  -sum -Exc=*\\.git  *
   lldu  -sum -Exc=*\\.(git||vs) *
   lldu  -sum -regex -Exc=.*\\[.](git||vs) *
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
    uppercase  C,S,L  format without commas
    precede with width, ex %12.12e\t%8c\t%10s\n

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
See [LICENSE.txt](LICENSE.txt) for the full license text.
