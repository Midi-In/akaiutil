# akaiutil - access to AKAI S900/S1000/S3000 filesystems
### Copyright (c) 2008-2020 by Klaus Michael Indlekofer. All rights reserved.
Note: Special restrictions apply. See disclaimers below and within the distribution.

* Release akaiutil-3.6.2 (17-MAR-2019)
* Pimped a bit by neoman
  * Support stereo samples
  * Support looped samples

## Usage

```
akaiutil [-h] [-r] [-f] [-c <cdrom-nr> ...] [-p <physdrive-nr> ...] [<disk-file> ...]
        -h      print this info
        -r      read-only mode
        -f      enable floppy format
        -c      CD-ROM drive
        -p      physical drive
```


## Commands

```
help [<cmd>]                    print help information for a command
=man

exit                            exit program
=quit
=bye
=q

df                              print disk info

dinfo                           print current directory info
=pwd

cd [<path>]                     change current directory

cdi <volume-index>              change current volume

dir [<path>]                    list directory
=ls

dirrec                          list current directory recursively
=lsrec

lstags                          list tags in partition
=dirtags

inittags                        initialize tags of disk or partition

rentag <tag-index> <tag-name>   rename tag in partition

cdinfo                          print CD3000 CD-ROM info

vcdinfo                         print verbose CD3000 CD-ROM info

setcdinfo [<cdlabel>]           set CD3000 CD-ROM info of disk or partition

lcd <dir-path>                  change current local (external) directory

ldir                            list files in current local (external) directory
=lls

lsfati <file-index>             print FAT of file

lstfati <take-index>            print FAT of DD take

infoi <file-index>              print information for file

infoall                         print information for all files in current volume

tinfoi <take-index>             print information for DD take

tinfoall                        print information for all DD takes in current partition

del <file-path>                 delete file
=rm

deli <file-index>               delete file
rmi

tdeli <take-index>              delete DD take
trmi

ren <old-file-path> <new-file-path> [<new-file-index>]          rename/move file
=mv

reni <old-file-index> <new-file-path> [<new-file-index>]        rename/move file
=mvi

setosveri <file-index> <new-os-version>                         set OS version of file in S1000/S3000 volume

setosverall <new-os-version>                                    set OS version of all files in current S1000/S3000 volume

setuncompri <file-index> <new-uncompr>                          set uncompr value of compressed file in S900 volume

updateuncompri <file-index>                                     update uncompr value of compressed file in S900 volume

updateuncomprall                                                update uncompr value of all compressed files in current S900 volume

sample900uncompr <file-path> [<dest-vol-path>]                  uncompress S900 compressed sample file
=s9uncompr

sample900uncompri <file-index> [<dest-vol-path>]                uncompress S900 compressed sample file
=s9uncompri

sample900uncomprall [<dest-vol-path>]                           uncompress all S900 compressed sample files in current volume
=s9uncomprall

sample900compr <file-path> [<dest-vol-path>]                    compress S900 non-compressed sample file
=s9compr

sample900compri <file-index> [<dest-vol-path>]                  compress S900 non-compressed sample file
=s9compri

sample900comprall [<dest-vol-path>]                             compress all S900 non-compressed sample files in current volume
=s9comprall

fixramname <file-path>                                          fix name in file header

fixramnamei <file-index>                                        fix name in file header

fixramnameall                                                   fix name in file header of all files in current volume

clrtagi <file-index> {<tag-index>|all}                          untag file

clrtagall {<tag-index>|all}                                     untag all files in current volume

settagi <file-index> <tag-index>                                tag file

settagall <tag-index>                                           tag all files in current volume

treni <take-index> <new-name>                                   rename DD take
=tmvi

clrfiltertag {<tag-index>|all}                                  remove tag from file filter

setfiltertag <tag-index>                                        add tag to file filter

copy <src-file-path> <new-file-path> [<new-file-index>]         copy file
=cp

copyi <src-file-index> <new-file-path> [<new-file-index>]       copy file
=cpi

copyvol <src-volume-path> <new-volume-path>                     copy volume (with all of its files)
=cpvol

copyvoli <src-volume-index> <new-volume-path>                   copy volume (with all of its files)
=cpvoli

copypart <src-partition-path> <dst-partition-path>              copy all volumes of a partition
=cppart

copytags <src-partition-path> <dst-partition-path>              copy all tags of a partition
=cptags

wipevol <volume-path>                           delete all files in volume
=wipedir
=rmall

wipevoli <volume-index>                         delete all files in volume
=wipediri
=rmalli

delvol <volume-path>                            delete volume and all of its files
=deldir
=rmvol
=rmdir

delvoli <volume-index>                          delete volume and all of its files
=deldiri
=rmvoli
=rmdiri

fixpart <partition-path>                        fix partition

wipepart <partition-path>                       wipe partition

wipepart3cd <partition-path>                    wipe partition for CD3000 CD-ROM

fixdisk <disk-path> [<max-number>]              fix disk

formatfloppyl9                                          format low-density floppy for S900/S950
=formatfl9

formatfloppyl1                                          format low-density floppy for S1000
=formatfl1

formatfloppyl3                                          format low-density floppy for S3000
=formatfl3

formatfloppyh9                                          format high-density floppy for S950
=formatfh9

formatfloppyh1                                          format high-density floppy for S1000
=formatfh1

formatfloppyh3                                          format high-density floppy for S3000
=formatfh3

formatharddisk9 [<total-size>[M]]                       format harddisk for S900/S950 (size in blocks or MB)
=formathd9

formatharddisk1 [<part-size>[M] [<total-size>[M]]]      format harddisk for S1000 (size in blocks or MB)
=formathd1

formatharddisk3 [<part-size>[M] [<total-size>[M]]]      format harddisk for S3000 or CD3000 (size in blocks or MB)
=formathd3

formatharddisk3cd [<part-size>[M] [<total-size>[M]]]    format harddisk for CD3000 CD-ROM (size in blocks or MB)
=formatcd

getdisk <file-name>                             get disk (to external file)
=dget
=dexport

putdisk <file-name>                             put disk (from external file)
=dput
=dimport

getpart <partition-path> <file-name>            get partition (to external file)
=pget
=pexport

putpart <file-name> <partition-path>            put partition (from external file)
=pput
=pimport

gettags <file-name>                             get tags from partition (to external file)
=tagsget
=tagsexport

puttags <file-name>                             put tags to partition (from external file)
=tagsput
=tagsimport

renvol <old-path> <new-name>                    rename volume
=rendir
=mvvol
=mvdir

renvoli <volume-index> <new-name>               rename volume
=rendiri
=mvvoli
=mvdiri

setosvervol <new-os-version>                            set OS version of current volume

setosvervoli <volume-index> <new-os-version>            set OS version of volume

setlnum <new-load-number>                               set load number of current volume (OFF for none)

setlnumi <volume-index> <new-load-number>               set load number of volume (OFF for none)

lsparam                                                 list parameters in current volume

lsparami <volume-index>                                 list parameters in volume

initparam                                               initialize parameters in current volume

initparami <volume-index>                               initialize parameters in volume

setparam <par-index> <par-value>                        set parameters in current volume

setparami <volume-index> <par-index> <par-value>        set parameters in volume

getparam <file-name>                                    get parameters from current volume (to external file)
=paramget
=paramexport

getparami <volume-index> <file-name>                    get parameters from volume (to external file)
=paramgeti
=paramexporti

putparam <file-name>                                    put parameters to current volume (from external file)
=paramput
=paramimport

putparami <volume-index> <file-name>                    put parameters to volume (from external file)
=paramputi
=paramimporti

get <file-path> [<begin-byte> [<end-byte>]]     get file (to external)
=export

geti <file-index> [<begin-byte> [<end-byte>]]   get file (to external)
=exporti

sample2wav <file-path>                          convert sample file into external WAV file
=s2wav
=getwav

sample2wavi <file-index>                        convert sample file into external WAV file
=s2wavi
=getwavi

sample2wavall                                   convert all sample files into external WAV files
=s2wavall
=getwavall

put <file-name> [<file-index>]                  put file (from external)
=import

wav2sample <wav-file> [<file-index>]            convert external WAV file into sample file
=wav2s
=putwav

wav2sample9 <wav-file> [<file-index>]           convert external WAV file into S900 non-compressed sample file
=wav2s9
=putwav9

wav2sample9c <wav-file> [<file-index>]          convert external WAV file into S900 compressed sample file
=wav2s9c
=putwav9c

wav2sample1 <wav-file> [<file-index>]           convert external WAV file into S1000 sample file
=wav2s1
=putwav1

wav2sample3 <wav-file> [<file-index>]           convert external WAV file into S3000 sample file
=wav2s3
=putwav3

tgeti <take-index>                              get DD take (to external)
=texporti

take2wavi <take-index>                          convert DD take into external WAV file
=t2wavi
=tgetwavi
=getwavti

take2wavall                                     convert all DD takes into external WAV files
=t2wavall
=tgetwavall
=getwavtall

tput <file-name>                                put DD take (from external)
=timport

wav2take <wav-name>                             convert external WAV file into DD take
=wav2t
=tputwav
=putwavt

tarc <tar-file>         tar c from current directory (to external)
=target
=gettar

tarcwav <tar-file>      tar c from current directory (to external) with WAV conversion
=targetwav
=gettarwav

tarx <tar-file>         tar x in current directory (from external)
=tarput
=puttar

tarx9 <tar-file>        tar x in current directory (from external) for S900
=tarput9
=puttar9

tarx1 <tar-file>        tar x in current directory (from external) for S1000
=tarput1
=puttar1

tarx3 <tar-file>        tar x in current directory (from external) for S3000 or CD3000
=tarput3
=puttar3

tarx3cd <tar-file>      tar x in current directory (from external) for CD3000 CD-ROM
=tarput3cd
=puttar3cd

tarxwav <tar-file>      tar x in current directory (from external) with WAV conversion
=tarputwav
=puttarwav

tarxwav9 <tar-file>     tar x in current directory (from external) with WAV conversion to S900 non-compressed sample
=tarputwav9
=puttarwav9

tarxwav9c <tar-file>    tar x in current directory (from external) with WAV conversion to S900 compressed sample
=tarputwav9c
=puttarwav9c

tarxwav1 <tar-file>     tar x in current directory (from external) with WAV conversion to S1000 sample
=tarputwav1
=puttarwav1

tarxwav3 <tar-file>     tar x in current directory (from external) with WAV conversion to S3000 sample
=tarputwav3
=puttarwav3

mkvol [<volume-path>]                                           create new volume
=mkdir

mkvol9 [<volume-path>]                                          create new volume for S900
=mkdir9

mkvol1 [<volume-path> [<load-number>]]                          create new volume for S1000
=mkdir1

mkvol3 [<volume-path> [<load-number>]]                          create new volume for S3000 or CD3000
=mkdir3

mkvol3cd [<volume-path> [<load-number>]]                        create new volume for CD3000 CD-ROM
=mkdir3cd

mkvoli <volume-index> [<volume-name> [<load-number>]]           create new volume at index
=mkdiri

mkvoli9 <volume-index> [<volume-name>]                          create new volume for S900 at index
=mkdiri9

mkvoli1 <volume-index> [<volume-name> [<load-number>]]          create new volume for S1000 at index
=mkdiri1

mkvoli3 <volume-index> [<volume-name> [<load-number>]]          create new volume for S3000 or CD3000 at index
=mkdiri3

mkvoli3cd <volume-index> [<volume-name> [<load-number>]]        create new volume for CD3000 CD-ROM
=mkdiri3cd

dircache                print cache information
=lscache
```

## Examples

Example 1: access floppy image fff.img
akaiutil -f fff.img
(Note: -f enables access to floppy formats)

Example 2: access harddisk images hhh1.img and hhh2.img
akaiutil hhh1.img hhh2.img

Example 3: access harddisk image hhh.img and floppy image fff.img
akaiutil -f hhh.img fff.img
(Note: -f enables access to floppy formats)

Example 4: access Windows physical drive 1 and Windows CD-ROM drive 0
akaiutil -p 1 -c 0

Example 5: access Windows physical drive 2, harddisk image hhh.img, and floppy image fff.img
akaiutil -f -p 2 hhh.img fff.img
(Note: -f enables access to floppy formats)

Example 6: access UNIX harddisks /dev/da1, /dev/da2, and harddisk image hhh.img
akaiutil /dev/da1 /dev/da2 hhh.img

Example 7: access Darwin disk2 in read-only mode
akaiutil -r /dev/disk2s0

## Warning

Accessing image files or physical drives might destroy their contents!!! Use at your own risk!!!
It is strongly recommended to use the read-only mode (-r) for testing and for accessing valuable drives or files.

## Transferring files

* individual files can be copied via "copy"
* individual files can be imported/exported via "put"/"get"
* sample files can be exported to WAV files via "sample2wav", "sample2wavall"
* WAV files can be imported to sample files via "wav2sample", "wav2sample9", "wav2sample1","wav2sample3"
* whole volume and partition trees can be copied via "copyvol" and "copypart"
* whole volume and partition trees can be imported/exported from/to tar archives via "tarput"/"target"
* WAV file conversion for tar archvives via "tarputwav"/"targetwav"
* whole partitions can be imported/exported via "putpart"/"getpart"
* path names in akaiutil are of the form "/disk/partition/volume/file", e.g. "/disk2/C/VOLUME_007/SINE.S"
* for access to files/volumes via index, some commands have an "i" version
* abbreviations:
  ".." = one level up, "." = stay in same directory
  "/N" = "/diskN"
  "/floppyN" = "/diskN/A/<volume1>"
  '_' can be used as a typable replacement for ' ' (space) in file or volume names
* for detailed information about individual akaiutil commands please read the online help infos

## Formatting drives and images

* Low-density floppy size: 800 blocks (1KB) = 800KB
* High-density floppy size: 1600 blocks (1KB) = 1600KB
* Note: in order to access a floppy drive to format, read, or write an AKAI floppy,
a special floppy device driver is required since the AKAI low-level format differs
from the standard PC low-level format
* Max. S1000/S3000 harddisk (or CD-ROM) size: 65535 blocks (8KB) = approx. 512MB
* ZIP100 disk size: 12288 blocks (8KB) = 96MB
* Number of blocks for S1000/S3000 standard harddisk partition sizes:
  * 30MB    ->      3840 blks
  * 40MB    ->      5120 blks
  * 50MB    ->      6400 blks
  * 60MB    ->      7680 blks

Example 1: create and format S1000 high-density floppy image
dd if=/dev/zero of=fff.img bs=1024 count=1600
akaiutil -f fff.img
-> formatfloppyh1

Example 2: create and format S900 low-density floppy image
dd if=/dev/zero of=fff.img bs=1024 count=800
akaiutil -f fff.img
-> formatfloppyl9

Example 3: create and format CD3000 CD-ROM image of max. size
dd if=/dev/zero of=hhh.img bs=8192 count=65535
akaiutil hhh.img
-> formatharddisk3cd 60M
(this gives 8 partitions of 60MB and 1 partition of approx. 32MB)
-> setcdinfo MYCDROM
(this must be done after all volumes/files have been transferred)

Example 4: format ZIP100 drive at Windows physical drive 1 for S1000
akaiutil -p 1
-> formatharddisk1 60M
(this gives 1 partition of 60MB and 1 partition of 36MB)

Example 5: format drive at Windows physical drive 1 for S1100 with DD space
akaiutil -p 1
-> formatharddisk1 1M 1M
(this gives 1 partition of 1MB, rest of disk is allocated for DD)

## Compilation

Depending on the operating system and the programming environment,
use Makefile (for make) or the .vcproj file.

## References

* documentation, manuals, and webpages for AKAI samplers
* floppies, harddisks, CD-ROMs, and files for AKAI samplers
* "AkaiDisk" and its documentation, by Paul Kellett, accessed 09-JAN-2008, 06-MAR-2008, 27-MAR-2010
* "akaitools", by Hiroyuki Ohsaki, accessed 09-JAN-2008
* "akaifs", by Raymond Dresens and Roger Karis, accessed 18-JAN-2008
* "libakai", by S�bastien M�trot and Christian Schoenebeck, accessed 09-JAN-2008



---
The following holds for all files in this distribution (unless stated otherwise on an
individual basis for each file and statement):

These program/data/document/HTML/picture/media files (materials) are distributed in the
hope that they will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. In no event shall the author be
liable for any direct, indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitue goods or services; loss of use,
data, or profits; or business interruption) however caused and on any theory of liability,
whether in contract, strict liability, or tort (including negligence or otherwise) arising
in any way out of the use of data/information/software from this distribution, even if
advised of the possibility of such damage.

The contents of this distribution are intended for educational, non-commercial purposes
only. Materials contained herein are property of their respective owners.
All brand names and trademarks are property of their respective owners.
If any copyrighted works/trademarks have been used, please contact the authors
and the item will be either removed or properly credited (at the copyright/trademark
owner's discretion). We have no intention of violating any copyrights or trademarks.
This distribution might use inlining and deep-linking, i.e. links in this distribution
might lead directly to materials on other web sites/distributions (in which case the
target page normally should be listed/credited in a "links" section). The author does
not take responsibility for the contents of any links referred to. We do not necessarily
endorse, sanction, support, encourage, verify or agree with the contents, opinions or
statements of/on any of the linked pages. These statements hold for all links/references
in all files in this distribution. We are in no way affiliated with any
companies/institutions/individuals which might be mentioned in any manner in this
distribution.

The author does not take responsibility for incorrect, incomplete or misleading information.
Statements are to be considered as the author's free personal opinion. The author does not
necessarly possess any of the items mentioned in files in this distribution.

Files (and the information therein) created by the authors are copyright
(c) by the authors. Unless protected/restricted otherwise, the author permits
reproduction/redistribution of material contained in this distribution under the condition
that the item is properly credited. Links to items/materials in this distribution are welcome.
Projects/publications/papers that make use of materials, programs, or generated output
of this distribution must properly credit the author and mention the usage of this distribution.
Please contact the authors for comments or further questions
and permission to use materials/information from this distribution.



---
This product includes software developed by the University of California, Berkeley
and its contributors.

Copyright (c) 1987-2002 The Regents of the University of California.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. [rescinded 22 July 1999]
4. Neither the name of the University nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

---
End of file
