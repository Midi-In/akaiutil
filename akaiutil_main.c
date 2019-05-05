/*
* Copyright (C) 2008-2019 Klaus Michael Indlekofer. All rights reserved.
*
* m.indlekofer@gmx.de
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* akaiutil: access to AKAI S900/S1000/S3000 filesystems */



#include "akaiutil_io.h"
#include "akaiutil.h"
#include "akaiutil_tar.h"
#include "akaiutil_file.h"
#include "akaiutil_take.h"

#if (!defined(WIN32))&&(!defined(__CYGWIN__))
/* e.g. UNIX-like systems */

/* getopt */
#include <unistd.h>
extern int getopt(int argc,char * const argv[],const char *optstring);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
#endif /* !WIN32 */



/* local directory/device */
#ifndef LCHDIR
#define LCHDIR(d) chdir(d)
#endif
#ifndef LDIR
#define LDIR system("ls -la");
#endif



void
print_progressbar(u_int end,u_int now)
{
	static double xold; /* must be static */

	if (end==0){
		return;
	}
	if (now>end){
		now=end;
	}

	if (now==0){
		printf("0         20        40        60        80        100%%\n.");
		xold=0.0;
	}

	for (;((double)now)/(double)end-xold>=0.02;xold+=0.02){
		printf(".");
		fflush(NULL);
	}
}



void
usage(char *name)
{

	if (name==NULL){
		return;
	}

#if defined(WIN32)||defined(__CYGWIN__)
	fprintf(stderr,"usage: %s [-h] [-r] [-f] [-c <cdrom-nr> ...] [-p <physdrive-nr> ...] [<disk-file> ...]\n",name);
#else
	fprintf(stderr,"usage: %s [-h] [-r] [-f] <disk-file> ...\n",name);
#endif
	fprintf(stderr,"\t-h\tprint this info\n");
	fprintf(stderr,"\t-r\tread-only mode\n");
	fprintf(stderr,"\t-f\tenable floppy format\n");
#if defined(WIN32)||defined(__CYGWIN__)
	fprintf(stderr,"\t-c\tCD-ROM drive\n");
	fprintf(stderr,"\t-p\tphysical drive\n");
#endif
}

int
main(int argc, char **argv)
{
	int op;
	int errflag;
	int readonly;
	int floppyenable;
	u_int i;

	if (argc<=1){
		usage(argv[0]);
		exit(1);
	}

	readonly=0; /* R/W so far */
	disk_num=0; /* no disks so far */
	floppyenable=0; /* start with harddisk format only */

	errflag=0;
#if defined(WIN32)||defined(__CYGWIN__)
#define OPT_STRING "hrfc:p:"
#else
#define OPT_STRING "hrf"
#endif
	while ((op=getopt(argc,argv,OPT_STRING))!=EOF){
		switch (op){
#if defined(WIN32)||defined(__CYGWIN__)
		case 'c':
			/* open CD-ROM drive */
			sprintf(dirnamebuf,"\\\\.\\cdrom%i",atoi(optarg));
			if (open_disk(dirnamebuf,1)<0){ /* 1: read-only */
				exit(1);
			}
			break;
		case 'p':
			/* open physical drive, Note: use with care!!! */
			sprintf(dirnamebuf,"\\\\.\\physicaldrive%i",atoi(optarg));
			if (open_disk(dirnamebuf,readonly)<0){
				exit(1);
			}
			break;
#endif
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'r':
			if ((!readonly)&&(disk_num>0)){ /* some disks already opened R/W ? */
				fprintf(stderr,"-r option must be prior to any drive/disk-file arguments\n");
				exit(1);
			}
			readonly=1; /* read-only mode */
			break;
		case 'f':
			floppyenable=1; /* allow floppy format */
			break;
		case '?':
			/* FALL THROUGH */
		default:
			errflag++;
		}
	}
	
	if (errflag){
		usage(argv[0]);
		exit(1);
	}
	
	if (optind>argc){
		usage(argv[0]);
		exit(1);
	}

	/* open disk files */
	for (;(optind<argc)&&(disk_num<DISK_NUM_MAX);optind++){
		if (open_disk(argv[optind],readonly)<0){
			exit(1);
		}
	}

	printf("\n"
		"****************************************************\n"
		"*                                                  *\n"
		"*  AKAI S900/S1000/S3000 File Manager, Rev. 3.6.2  *\n"
		"*                                                  *\n"
		"****************************************************\n"
		"\n");
	printf("Copyright (c) 2008-2019 Klaus Michael Indlekofer. All rights reserved.\n\n");
#ifdef ADDITIONAL_COPYRIGHT_TEXT
	printf(ADDITIONAL_COPYRIGHT_TEXT);
#endif /* ADDITIONAL_COPYRIGHT_TEXT */
	printf("\n");

	if (readonly){
		printf("--- read-only mode ---\n\n");
	}else{
		printf("--- read/write mode ---\n\n");
	}

main_restart: /* restart with opened disks */

	/* init cache */
	if (init_blk_cache()<0){
		exit(1);
	}

	/* scan disks */
	printf("\n");
	part_num=0; /* no partitions found so far */
	for (i=0;i<disk_num;i++){
		printf("scanning disk%u\n",i);
		if (akai_scan_disk(&disk[i],floppyenable)<0){
			exit(1);
		}
	}
	printf("\n");

	/* print disks */
	printf("\n");
	akai_list_alldisks(0,NULL);

	/* set current directory */
	change_curdir_home(); /* ignore error */

	/* clear filter tags */
	akai_clear_filetag(curfiltertag,AKAI_FILE_TAGFREE); /* clear all */

	fflush(NULL);

	/* command interpreter */
	{
#define CMDLEN 256 /* XXX */
		char cmd[CMDLEN+1]; /* +1 for '\0' */
#define CMDTOKMAX 16
		char *(cmdtok[CMDTOKMAX]);
		int cmdtoknr;
		int i;
		char *p;

		enum cmd_e{
			CMD_HELP,
			CMD_EXIT,
			CMD_DINFO,
			CMD_DF,
			CMD_CD,
			CMD_CDI,
			CMD_DIR,
			CMD_DIRREC,
			CMD_LSTAGS,
			CMD_INITTAGS,
			CMD_RENTAG,
			CMD_CDINFO,
			CMD_VCDINFO,
			CMD_SETCDINFO,
			CMD_LCD,
			CMD_LDIR,
			CMD_LSFATI,
			CMD_LSTFATI,
			CMD_INFOI,
			CMD_INFOALL,
			CMD_TINFOI,
			CMD_TINFOALL,
			CMD_DEL,
			CMD_DELI,
			CMD_TDELI,
			CMD_REN,
			CMD_RENI,
			CMD_SETOSVERI,
			CMD_SETOSVERALL,
			CMD_SETUNCOMPRI,
			CMD_UPDATEUNCOMPRI,
			CMD_UPDATEUNCOMPRALL,
			CMD_SAMPLE900UNCOMPR,
			CMD_SAMPLE900UNCOMPRI,
			CMD_SAMPLE900UNCOMPRALL,
			CMD_SAMPLE900COMPR,
			CMD_SAMPLE900COMPRI,
			CMD_SAMPLE900COMPRALL,
			CMD_FIXRAMNAME,
			CMD_FIXRAMNAMEI,
			CMD_FIXRAMNAMEALL,
			CMD_CLRTAGI,
			CMD_CLRTAGALL,
			CMD_SETTAGI,
			CMD_SETTAGALL,
			CMD_TRENI,
			CMD_CLRFILTERTAG,
			CMD_SETFILTERTAG,
			CMD_COPY,
			CMD_COPYI,
			CMD_COPYVOL,
			CMD_COPYVOLI,
			CMD_COPYPART,
			CMD_COPYTAGS,
			CMD_WIPEVOL,
			CMD_WIPEVOLI,
			CMD_DELVOL,
			CMD_DELVOLI,
			CMD_FIXPART,
			CMD_WIPEPART,
			CMD_WIPEPART3CD,
			CMD_FIXDISK,
			CMD_FORMATFLOPPYL9,
			CMD_FORMATFLOPPYL1,
			CMD_FORMATFLOPPYL3,
			CMD_FORMATFLOPPYH9,
			CMD_FORMATFLOPPYH1,
			CMD_FORMATFLOPPYH3,
			CMD_FORMATHARDDISK9,
			CMD_FORMATHARDDISK1,
			CMD_FORMATHARDDISK3,
			CMD_FORMATHARDDISK3CD,
			CMD_GETDISK,
			CMD_PUTDISK,
			CMD_GETPART,
			CMD_PUTPART,
			CMD_GETTAGS,
			CMD_PUTTAGS,
			CMD_GETVOLPARAM,
			CMD_PUTVOLPARAM,
			CMD_RENVOL,
			CMD_RENVOLI,
			CMD_SETOSVERVOL,
			CMD_SETOSVERVOLI,
			CMD_SETLNUM,
			CMD_SETLNUMI,
			CMD_LSPARAM,
			CMD_LSPARAMI,
			CMD_INITPARAM,
			CMD_INITPARAMI,
			CMD_SETPARAM,
			CMD_SETPARAMI,
			CMD_GETPARAM,
			CMD_GETPARAMI,
			CMD_PUTPARAM,
			CMD_PUTPARAMI,
			CMD_GET,
			CMD_GETI,
			CMD_SAMPLE2WAV,
			CMD_SAMPLE2WAVI,
			CMD_SAMPLE2WAVALL,
			CMD_PUT,
			CMD_WAV2SAMPLE,
			CMD_WAV2SAMPLE9,
			CMD_WAV2SAMPLE9C,
			CMD_WAV2SAMPLE1,
			CMD_WAV2SAMPLE3,
			CMD_TGETI,
			CMD_TAKE2WAVI,
			CMD_TAKE2WAVALL,
			CMD_TPUT,
			CMD_WAV2TAKE,
			CMD_TARC,
			CMD_TARCWAV,
			CMD_TARX,
			CMD_TARX9,
			CMD_TARX1,
			CMD_TARX3,
			CMD_TARX3CD,
			CMD_TARXWAV,
			CMD_TARXWAV9,
			CMD_TARXWAV9C,
			CMD_TARXWAV1,
			CMD_TARXWAV3,
			CMD_MKVOL,
			CMD_MKVOL9,
			CMD_MKVOL1,
			CMD_MKVOL3,
			CMD_MKVOL3CD,
			CMD_MKVOLI,
			CMD_MKVOLI9,
			CMD_MKVOLI1,
			CMD_MKVOLI3,
			CMD_MKVOLI3CD,
			CMD_DIRCACHE,
			CMD_NULL
		};
		enum cmd_e cmdnr;

		struct cmdtab_s{
			enum cmd_e cmdnr;
			char *cmdstr;
			int cmdtokmin;
			int cmdtokmax;
			char *cmduse;
			char *cmdhelp;
		}cmdtab[]={
			{CMD_HELP,"help",1,2,"[<cmd>]","print help information for a command"},
			{CMD_HELP,"man",1,2,NULL,NULL},
			{CMD_EXIT,"exit",1,1,"","exit program"},
			{CMD_EXIT,"quit",1,1,NULL,NULL},
			{CMD_EXIT,"bye",1,1,NULL,NULL},
			{CMD_EXIT,"q",1,1,NULL,NULL},
			{CMD_DF,"df",1,1,"","print disk info"},
			{CMD_DINFO,"dinfo",1,1,"","print current directory info"},
			{CMD_DINFO,"pwd",1,1,NULL,NULL},
			{CMD_CD,"cd",1,2,"[<path>]","change current directory"},
			{CMD_CDI,"cdi",2,2,"<volume-index>","change current volume"},
			{CMD_DIR,"dir",1,2,"[<path>]","list directory"},
			{CMD_DIR,"ls",1,2,NULL,NULL},
			{CMD_DIRREC,"dirrec",1,1,"","list current directory recursively"},
			{CMD_DIRREC,"lsrec",1,1,NULL,NULL},
			{CMD_LSTAGS,"lstags",1,1,"","list tags in partition"},
			{CMD_LSTAGS,"dirtags",1,1,NULL,NULL},
			{CMD_INITTAGS,"inittags",1,1,"","initialize tags of disk or partition"},
			{CMD_RENTAG,"rentag",3,3,"<tag-index> <tag-name>","rename tag in partition"},
			{CMD_CDINFO,"cdinfo",1,1,"","print CD3000 CD-ROM info"},
			{CMD_VCDINFO,"vcdinfo",1,1,"","print verbose CD3000 CD-ROM info"},
			{CMD_SETCDINFO,"setcdinfo",1,2,"[<cdlabel>]","set CD3000 CD-ROM info of disk or partition"},
			{CMD_LCD,"lcd",2,2,"<dir-path>","change current local (external) directory"},
			{CMD_LDIR,"ldir",1,1,"","list files in current local (external) directory"},
			{CMD_LDIR,"lls",1,1,NULL,NULL},
			{CMD_LSFATI,"lsfati",2,2,"<file-index>","print FAT of file"},
			{CMD_LSTFATI,"lstfati",2,2,"<take-index>","print FAT of DD take"},
			{CMD_INFOI,"infoi",2,2,"<file-index>","print information for file"},
			{CMD_INFOALL,"infoall",1,1,"","print information for all files in current volume"},
			{CMD_TINFOI,"tinfoi",2,2,"<take-index>","print information for DD take"},
			{CMD_TINFOALL,"tinfoall",1,1,"","print information for all DD takes in current partition"},
			{CMD_DEL,"del",2,2,"<file-path>","delete file"},
			{CMD_DEL,"rm",2,2,NULL,NULL},
			{CMD_DELI,"deli",2,2,"<file-index>","delete file"},
			{CMD_DELI,"rmi",2,2,NULL,NULL},
			{CMD_TDELI,"tdeli",2,2,"<take-index>","delete DD take"},
			{CMD_TDELI,"trmi",2,2,NULL,NULL},
			{CMD_REN,"ren",3,4,"<old-file-path> <new-file-path> [<new-file-index>]","rename/move file"},
			{CMD_REN,"mv",3,4,NULL,NULL},
			{CMD_RENI,"reni",3,4,"<old-file-index> <new-file-path> [<new-file-index>]","rename/move file"},
			{CMD_RENI,"mvi",3,4,NULL,NULL},
			{CMD_SETOSVERI,"setosveri",3,3,"<file-index> <new-os-version>","set OS version of file in S1000/S3000 volume"},
			{CMD_SETOSVERALL,"setosverall",2,2,"<new-os-version>","set OS version of all files in current S1000/S3000 volume"},
			{CMD_SETUNCOMPRI,"setuncompri",3,3,"<file-index> <new-uncompr>","set uncompr value of compressed file in S900 volume"},
			{CMD_UPDATEUNCOMPRI,"updateuncompri",2,2,"<file-index>","update uncompr value of compressed file in S900 volume"},
			{CMD_UPDATEUNCOMPRALL,"updateuncomprall",1,1,"","update uncompr value of all compressed files in current S900 volume"},
			{CMD_SAMPLE900UNCOMPR,"sample900uncompr",2,3,"<file-path> [<dest-vol-path>]","uncompress S900 compressed sample file"},
			{CMD_SAMPLE900UNCOMPR,"s9uncompr",2,3,NULL,NULL},
			{CMD_SAMPLE900UNCOMPRI,"sample900uncompri",2,3,"<file-index> [<dest-vol-path>]","uncompress S900 compressed sample file"},
			{CMD_SAMPLE900UNCOMPRI,"s9uncompri",2,3,NULL,NULL},
			{CMD_SAMPLE900UNCOMPRALL,"sample900uncomprall",1,2,"[<dest-vol-path>]","uncompress all S900 compressed sample files in current volume"},
			{CMD_SAMPLE900UNCOMPRALL,"s9uncomprall",1,2,NULL,NULL},
			{CMD_SAMPLE900COMPR,"sample900compr",2,3,"<file-path> [<dest-vol-path>]","compress S900 non-compressed sample file"},
			{CMD_SAMPLE900COMPR,"s9compr",2,3,NULL,NULL},
			{CMD_SAMPLE900COMPRI,"sample900compri",2,3,"<file-index> [<dest-vol-path>]","compress S900 non-compressed sample file"},
			{CMD_SAMPLE900COMPRI,"s9compri",2,3,NULL,NULL},
			{CMD_SAMPLE900COMPRALL,"sample900comprall",1,2,"[<dest-vol-path>]","compress all S900 non-compressed sample files in current volume"},
			{CMD_SAMPLE900COMPRALL,"s9comprall",1,2,NULL,NULL},
			{CMD_FIXRAMNAME,"fixramname",2,2,"<file-path>","fix name in file header"},
			{CMD_FIXRAMNAMEI,"fixramnamei",2,2,"<file-index>","fix name in file header"},
			{CMD_FIXRAMNAMEALL,"fixramnameall",1,1,"","fix name in file header of all files in current volume"},
			{CMD_CLRTAGI,"clrtagi",3,3,"<file-index> {<tag-index>|all}","untag file"},
			{CMD_CLRTAGALL,"clrtagall",2,2,"{<tag-index>|all}","untag all files in current volume"},
			{CMD_SETTAGI,"settagi",3,3,"<file-index> <tag-index>","tag file"},
			{CMD_SETTAGALL,"settagall",2,2,"<tag-index>","tag all files in current volume"},
			{CMD_TRENI,"treni",3,3,"<take-index> <new-name>","rename DD take"},
			{CMD_TRENI,"tmvi",3,3,NULL,NULL},
			{CMD_CLRFILTERTAG,"clrfiltertag",2,2,"{<tag-index>|all}","remove tag from file filter"},
			{CMD_SETFILTERTAG,"setfiltertag",2,2,"<tag-index>","add tag to file filter"},
			{CMD_COPY,"copy",3,4,"<src-file-path> <new-file-path> [<new-file-index>]","copy file"},
			{CMD_COPY,"cp",3,4,NULL,NULL},
			{CMD_COPYI,"copyi",3,4,"<src-file-index> <new-file-path> [<new-file-index>]","copy file"},
			{CMD_COPYI,"cpi",3,4,NULL,NULL},
			{CMD_COPYVOL,"copyvol",3,3,"<src-volume-path> <new-volume-path>","copy volume (with all of its files)"},
			{CMD_COPYVOL,"cpvol",3,3,NULL,NULL},
			{CMD_COPYVOLI,"copyvoli",3,3,"<src-volume-index> <new-volume-path>","copy volume (with all of its files)"},
			{CMD_COPYVOLI,"cpvoli",3,3,NULL,NULL},
			{CMD_COPYPART,"copypart",3,3,"<src-partition-path> <dst-partition-path>","copy all volumes of a partition"},
			{CMD_COPYPART,"cppart",3,3,NULL,NULL},
			{CMD_COPYTAGS,"copytags",3,3,"<src-partition-path> <dst-partition-path>","copy all tags of a partition"},
			{CMD_COPYTAGS,"cptags",3,3,NULL,NULL},
			{CMD_WIPEVOL,"wipevol",2,2,"<volume-path>","delete all files in volume"},
			{CMD_WIPEVOL,"wipedir",2,2,NULL,NULL},
			{CMD_WIPEVOL,"rmall",2,2,NULL,NULL},
			{CMD_WIPEVOLI,"wipevoli",2,2,"<volume-index>","delete all files in volume"},
			{CMD_WIPEVOLI,"wipediri",2,2,NULL,NULL},
			{CMD_WIPEVOLI,"rmalli",2,2,NULL,NULL},
			{CMD_DELVOL,"delvol",2,2,"<volume-path>","delete volume and all of its files"},
			{CMD_DELVOL,"deldir",2,2,NULL,NULL},
			{CMD_DELVOL,"rmvol",2,2,NULL,NULL},
			{CMD_DELVOL,"rmdir",2,2,NULL,NULL},
			{CMD_DELVOLI,"delvoli",2,2,"<volume-index>","delete volume and all of its files"},
			{CMD_DELVOLI,"deldiri",2,2,NULL,NULL},
			{CMD_DELVOLI,"rmvoli",2,2,NULL,NULL},
			{CMD_DELVOLI,"rmdiri",2,2,NULL,NULL},
			{CMD_FIXPART,"fixpart",2,2,"<partition-path>","fix partition"},
			{CMD_WIPEPART,"wipepart",2,2,"<partition-path>","wipe partition"},
			{CMD_WIPEPART3CD,"wipepart3cd",2,2,"<partition-path>","wipe partition for CD3000 CD-ROM"},
			{CMD_FIXDISK,"fixdisk",2,3,"<disk-path> [<max-number>]"," fix disk"},
			{CMD_FORMATFLOPPYL9,"formatfloppyl9",1,1,"","format low-density floppy for S900/S950"},
			{CMD_FORMATFLOPPYL9,"formatfl9",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYL1,"formatfloppyl1",1,1,"","format low-density floppy for S1000"},
			{CMD_FORMATFLOPPYL1,"formatfl1",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYL3,"formatfloppyl3",1,1,"","format low-density floppy for S3000"},
			{CMD_FORMATFLOPPYL3,"formatfl3",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH9,"formatfloppyh9",1,1,"","format high-density floppy for S950"},
			{CMD_FORMATFLOPPYH9,"formatfh9",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH1,"formatfloppyh1",1,1,"","format high-density floppy for S1000"},
			{CMD_FORMATFLOPPYH1,"formatfh1",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH3,"formatfloppyh3",1,1,"","format high-density floppy for S3000"},
			{CMD_FORMATFLOPPYH3,"formatfh3",1,1,NULL,NULL},
			{CMD_FORMATHARDDISK9,"formatharddisk9",1,2,"[<total-size>[M]]","format harddisk for S900/S950 (size in blocks or MB)"},
			{CMD_FORMATHARDDISK9,"formathd9",1,2,NULL,NULL},
			{CMD_FORMATHARDDISK1,"formatharddisk1",1,3,"[<part-size>[M] [<total-size>[M]]]","format harddisk for S1000 (size in blocks or MB)"},
			{CMD_FORMATHARDDISK1,"formathd1",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3,"formatharddisk3",1,3,"[<part-size>[M] [<total-size>[M]]]","format harddisk for S3000 or CD3000 (size in blocks or MB)"},
			{CMD_FORMATHARDDISK3,"formathd3",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3CD,"formatharddisk3cd",1,3,"[<part-size>[M] [<total-size>[M]]]","format harddisk for CD3000 CD-ROM (size in blocks or MB)"},
			{CMD_FORMATHARDDISK3CD,"formatcd",1,3,NULL,NULL},
			{CMD_GETDISK,"getdisk",2,2,"<file-name>","get disk (to external file)"},
			{CMD_GETDISK,"dget",2,2,NULL,NULL},
			{CMD_GETDISK,"dexport",2,2,NULL,NULL},
			{CMD_PUTDISK,"putdisk",2,2,"<file-name>","put disk (from external file)"},
			{CMD_PUTDISK,"dput",2,2,NULL,NULL},
			{CMD_PUTDISK,"dimport",2,2,NULL,NULL},
			{CMD_GETPART,"getpart",3,3,"<partition-path> <file-name>","get partition (to external file)"},
			{CMD_GETPART,"pget",3,3,NULL,NULL},
			{CMD_GETPART,"pexport",3,3,NULL,NULL},
			{CMD_PUTPART,"putpart",3,3,"<file-name> <partition-path>","put partition (from external file)"},
			{CMD_PUTPART,"pput",3,3,NULL,NULL},
			{CMD_PUTPART,"pimport",3,3,NULL,NULL},
			{CMD_GETTAGS,"gettags",2,2,"<file-name>","get tags from partition (to external file)"},
			{CMD_GETTAGS,"tagsget",2,2,NULL,NULL},
			{CMD_GETTAGS,"tagsexport",2,2,NULL,NULL},
			{CMD_PUTTAGS,"puttags",2,2,"<file-name>","put tags to partition (from external file)"},
			{CMD_PUTTAGS,"tagsput",2,2,NULL,NULL},
			{CMD_PUTTAGS,"tagsimport",2,2,NULL,NULL},
			{CMD_RENVOL,"renvol",3,3,"<old-path> <new-name>","rename volume"},
			{CMD_RENVOL,"rendir",3,3,NULL,NULL},
			{CMD_RENVOL,"mvvol",3,3,NULL,NULL},
			{CMD_RENVOL,"mvdir",3,3,NULL,NULL},
			{CMD_RENVOLI,"renvoli",3,3,"<volume-index> <new-name>","rename volume"},
			{CMD_RENVOLI,"rendiri",3,3,NULL,NULL},
			{CMD_RENVOLI,"mvvoli",3,3,NULL,NULL},
			{CMD_RENVOLI,"mvdiri",3,3,NULL,NULL},
			{CMD_SETOSVERVOL,"setosvervol",2,2,"<new-os-version>","set OS version of current volume"},
			{CMD_SETOSVERVOLI,"setosvervoli",3,3,"<volume-index> <new-os-version>","set OS version of volume"},
			{CMD_SETLNUM,"setlnum",2,2,"<new-load-number>","set load number of current volume (OFF for none)"},
			{CMD_SETLNUMI,"setlnumi",3,3,"<volume-index> <new-load-number>","set load number of volume (OFF for none)"},
			{CMD_LSPARAM,"lsparam",1,1,"","list parameters in current volume"},
			{CMD_LSPARAMI,"lsparami",2,2,"<volume-index>","list parameters in volume"},
			{CMD_INITPARAM,"initparam",1,1,"","initialize parameters in current volume"},
			{CMD_INITPARAMI,"initparami",2,2,"<volume-index>","initialize parameters in volume"},
			{CMD_SETPARAM,"setparam",3,3,"<par-index> <par-value>","set parameters in current volume"},
			{CMD_SETPARAMI,"setparami",4,4,"<volume-index> <par-index> <par-value>","set parameters in volume"},
			{CMD_GETPARAM,"getparam",2,2,"<file-name>","get parameters from current volume (to external file)"},
			{CMD_GETPARAM,"paramget",2,2,NULL,NULL},
			{CMD_GETPARAM,"paramexport",2,2,NULL,NULL},
			{CMD_GETPARAMI,"getparami",3,3,"<volume-index> <file-name>","get parameters from volume (to external file)"},
			{CMD_GETPARAMI,"paramgeti",3,3,NULL,NULL},
			{CMD_GETPARAMI,"paramexporti",3,3,NULL,NULL},
			{CMD_PUTPARAM,"putparam",2,2,"<file-name>","put parameters to current volume (from external file)"},
			{CMD_PUTPARAM,"paramput",2,2,NULL,NULL},
			{CMD_PUTPARAM,"paramimport",2,2,NULL,NULL},
			{CMD_PUTPARAMI,"putparami",3,3,"<volume-index> <file-name>","put parameters to volume (from external file)"},
			{CMD_PUTPARAMI,"paramputi",3,3,NULL,NULL},
			{CMD_PUTPARAMI,"paramimporti",3,3,NULL,NULL},
			{CMD_GET,"get",2,4,"<file-path> [<begin-byte> [<end-byte>]]","get file (to external)"},
			{CMD_GET,"export",2,4,NULL,NULL},
			{CMD_GETI,"geti",2,4,"<file-index> [<begin-byte> [<end-byte>]]","get file (to external)"},
			{CMD_GETI,"exporti",2,4,NULL,NULL},
			{CMD_SAMPLE2WAV,"sample2wav",2,2,"<file-path>","convert sample file into external WAV file"},
			{CMD_SAMPLE2WAV,"s2wav",2,2,NULL,NULL},
			{CMD_SAMPLE2WAV,"getwav",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVI,"sample2wavi",2,2,"<file-index>","convert sample file into external WAV file"},
			{CMD_SAMPLE2WAVI,"s2wavi",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVI,"getwavi",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVALL,"sample2wavall",1,1,NULL,"convert all sample files into external WAV files"},
			{CMD_SAMPLE2WAVALL,"s2wavall",1,1,NULL,NULL},
			{CMD_SAMPLE2WAVALL,"getwavall",1,1,NULL,NULL},
			{CMD_PUT,"put",2,3,"<file-name> [<file-index>]","put file (from external)"},
			{CMD_PUT,"import",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE,"wav2sample",2,3,"<wav-file> [<file-index>]","convert external WAV file into sample file"},
			{CMD_WAV2SAMPLE,"wav2s",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE,"putwav",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9,"wav2sample9",2,3,"<wav-file> [<file-index>]","convert external WAV file into S900 non-compressed sample file"},
			{CMD_WAV2SAMPLE9,"wav2s9",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9,"putwav9",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9C,"wav2sample9c",2,3,"<wav-file> [<file-index>]","convert external WAV file into S900 compressed sample file"},
			{CMD_WAV2SAMPLE9C,"wav2s9c",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9C,"putwav9c",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE1,"wav2sample1",2,3,"<wav-file> [<file-index>]","convert external WAV file into S1000 sample file"},
			{CMD_WAV2SAMPLE1,"wav2s1",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE1,"putwav1",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE3,"wav2sample3",2,3,"<wav-file> [<file-index>]","convert external WAV file into S3000 sample file"},
			{CMD_WAV2SAMPLE3,"wav2s3",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE3,"putwav3",2,3,NULL,NULL},
			{CMD_TGETI,"tgeti",2,2,"<take-index>","get DD take (to external)"},
			{CMD_TGETI,"texporti",2,2,NULL,NULL},
			{CMD_TAKE2WAVI,"take2wavi",2,2,"<take-index>","convert DD take into external WAV file"},
			{CMD_TAKE2WAVI,"t2wavi",2,2,NULL,NULL},
			{CMD_TAKE2WAVI,"tgetwavi",2,2,NULL,NULL},
			{CMD_TAKE2WAVI,"getwavti",2,2,NULL,NULL},
			{CMD_TAKE2WAVALL,"take2wavall",1,1,NULL,"convert all DD takes into external WAV files"},
			{CMD_TAKE2WAVALL,"t2wavall",1,1,NULL,NULL},
			{CMD_TAKE2WAVALL,"tgetwavall",1,1,NULL,NULL},
			{CMD_TAKE2WAVALL,"getwavtall",1,1,NULL,NULL},
			{CMD_TPUT,"tput",2,2,"<file-name>","put DD take (from external)"},
			{CMD_TPUT,"timport",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"wav2take",2,2,"<wav-name>","convert external WAV file into DD take"},
			{CMD_WAV2TAKE,"wav2t",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"tputwav",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"putwavt",2,2,NULL,NULL},
			{CMD_TARC,"tarc",2,2,"<tar-file>","tar c from current directory (to external)"},
			{CMD_TARC,"target",2,2,NULL,NULL},
			{CMD_TARC,"gettar",2,2,NULL,NULL},
			{CMD_TARCWAV,"tarcwav",2,2,"<tar-file>","tar c from current directory (to external) with WAV conversion"},
			{CMD_TARCWAV,"targetwav",2,2,NULL,NULL},
			{CMD_TARCWAV,"gettarwav",2,2,NULL,NULL},
			{CMD_TARX,"tarx",2,2,"<tar-file>","tar x in current directory (from external)"},
			{CMD_TARX,"tarput",2,2,NULL,NULL},
			{CMD_TARX,"puttar",2,2,NULL,NULL},
			{CMD_TARX9,"tarx9",2,2,"<tar-file>","tar x in current directory (from external) for S900"},
			{CMD_TARX9,"tarput9",2,2,NULL,NULL},
			{CMD_TARX9,"puttar9",2,2,NULL,NULL},
			{CMD_TARX1,"tarx1",2,2,"<tar-file>","tar x in current directory (from external) for S1000"},
			{CMD_TARX1,"tarput1",2,2,NULL,NULL},
			{CMD_TARX1,"puttar1",2,2,NULL,NULL},
			{CMD_TARX3,"tarx3",2,2,"<tar-file>","tar x in current directory (from external) for S3000 or CD3000"},
			{CMD_TARX3,"tarput3",2,2,NULL,NULL},
			{CMD_TARX3,"puttar3",2,2,NULL,NULL},
			{CMD_TARX3CD,"tarx3cd",2,2,"<tar-file>","tar x in current directory (from external) for CD3000 CD-ROM"},
			{CMD_TARX3CD,"tarput3cd",2,2,NULL,NULL},
			{CMD_TARX3CD,"puttar3cd",2,2,NULL,NULL},
			{CMD_TARXWAV,"tarxwav",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion"},
			{CMD_TARXWAV,"tarputwav",2,2,NULL,NULL},
			{CMD_TARXWAV,"puttarwav",2,2,NULL,NULL},
			{CMD_TARXWAV9,"tarxwav9",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S900 compressed sample"},
			{CMD_TARXWAV9,"tarputwav9",2,2,NULL,NULL},
			{CMD_TARXWAV9,"puttarwav9",2,2,NULL,NULL},
			{CMD_TARXWAV9C,"tarxwav9c",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S900 compressed sample"},
			{CMD_TARXWAV9C,"tarputwav9c",2,2,NULL,NULL},
			{CMD_TARXWAV9C,"puttarwav9c",2,2,NULL,NULL},
			{CMD_TARXWAV1,"tarxwav1",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S1000 sample"},
			{CMD_TARXWAV1,"tarputwav1",2,2,NULL,NULL},
			{CMD_TARXWAV1,"puttarwav1",2,2,NULL,NULL},
			{CMD_TARXWAV3,"tarxwav3",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S3000 sample"},
			{CMD_TARXWAV3,"tarputwav3",2,2,NULL,NULL},
			{CMD_TARXWAV3,"puttarwav3",2,2,NULL,NULL},
			{CMD_MKVOL,"mkvol",1,2,"[<volume-path>]","create new volume"},
			{CMD_MKVOL,"mkdir",1,2,NULL,NULL},
			{CMD_MKVOL9,"mkvol9",1,2,"[<volume-path>]","create new volume for S900"},
			{CMD_MKVOL9,"mkdir9",1,2,NULL,NULL},
			{CMD_MKVOL1,"mkvol1",1,3,"[<volume-path> [<load-number>]]","create new volume for S1000"},
			{CMD_MKVOL1,"mkdir1",1,3,NULL,NULL},
			{CMD_MKVOL3,"mkvol3",1,3,"[<volume-path> [<load-number>]]","create new volume for S3000 or CD3000"},
			{CMD_MKVOL3,"mkdir3",1,3,NULL,NULL},
			{CMD_MKVOL3CD,"mkvol3cd",1,3,"[<volume-path> [<load-number>]]","create new volume for CD3000 CD-ROM"},
			{CMD_MKVOL3CD,"mkdir3cd",1,3,NULL,NULL},
			{CMD_MKVOLI,"mkvoli",2,3,"<volume-index> [<volume-name>]","create new volume at index"},
			{CMD_MKVOLI,"mkdiri",2,3,NULL,NULL},
			{CMD_MKVOLI9,"mkvoli9",2,3,"<volume-index> [<volume-name>]","create new volume for S900 at index"},
			{CMD_MKVOLI9,"mkdiri9",2,3,NULL,NULL},
			{CMD_MKVOLI1,"mkvoli1",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for S1000 at index"},
			{CMD_MKVOLI1,"mkdiri1",2,4,NULL,NULL},
			{CMD_MKVOLI3,"mkvoli3",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for S3000 or CD3000 at index"},
			{CMD_MKVOLI3,"mkdiri3",2,4,NULL,NULL},
			{CMD_MKVOLI3CD,"mkvoli3cd",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for CD3000 CD-ROM at index"},
			{CMD_MKVOLI3CD,"mkdiri3cd",2,4,NULL,NULL},
			{CMD_DIRCACHE,"dircache",1,1,"","print cache information"},
			{CMD_DIRCACHE,"lscache",1,1,NULL,NULL},
			{CMD_NULL,NULL,0,0,NULL,NULL}
		};

		printf("\ntry \"help\" for infos\n\n");
		
		for (;;){
			/* get command */
			curdir_info(0);
			printf(" > ");
			fflush(NULL);
			fgets(cmd,CMDLEN,stdin);
			if (strlen(cmd)==0){
				goto main_parser_next;
			}
			cmd[strlen(cmd)-1]='\0';
			if (strlen(cmd)==0){
				goto main_parser_next;
			}
			/* parse command line */
			for (i=0;i<CMDTOKMAX;i++){
				if (i==0){
					p=(char *)strtok(cmd," \t");
					if (p==NULL){
						/* no tokens at all */
						goto main_parser_next;
					}
					cmdtok[i]=p;
				}else{
					p=(char *)strtok(NULL," \t");
					if (p==NULL){
						/* done */
						break; /* parse command line loop */
					}
					cmdtok[i]=p;
				}
			}
			cmdtoknr=i; /* number of tokens */
#ifdef DEBUGPARSER
			printf("parser: cmdtoknr: %i\n",cmdtoknr);
			for (i=0;i<cmdtoknr;i++){
				printf("%i: \"%s\"\n",i,cmdtok[i]);
			}
			printf("\n");
#endif
			
			/* find command */
			cmdnr=CMD_NULL; /* nothing found yet */
			for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
				if (cmdtab[i].cmdstr!=NULL){
					if (strcmp(cmdtab[i].cmdstr,cmdtok[0])==0){
						/* found command string */
						cmdnr=cmdtab[i].cmdnr;
						break; /* done */
					}
				}
			}

			/* check number of arguments */
			if ((cmdnr!=CMD_NULL)&&((cmdtoknr<cmdtab[i].cmdtokmin)||(cmdtoknr>cmdtab[i].cmdtokmax))){
				for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
					if ((cmdtab[i].cmdnr==cmdnr)&&(cmdtab[i].cmdstr!=NULL)&&(cmdtab[i].cmduse!=NULL)){
						printf("usage: %s %s\n\n",cmdtab[i].cmdstr,cmdtab[i].cmduse);
						break;
					}
				}
				goto main_parser_next;
			}

			/* execute command */
			switch (cmdnr){
			case CMD_EXIT:
				flush_blk_cache(); /* XXX if error, too late */
				exit(0);
			case CMD_HELP:
				if (cmdtoknr==1){
					int i,j,k;

					printf("\ncommands:\n");
					k=0;
					for (i=0,j=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdstr!=NULL){
							if (j%5==0){
								putchar('\n');
								k=0;
							}
							for (;k<((j%5)*15+2);k++){
								putchar(' ');
							}
							printf("%s ",cmdtab[i].cmdstr);
							k+=(int)strlen(cmdtab[i].cmdstr)+1;
							j++;
						}
					}
					printf("\n\ntry \"help <cmd>\" for more information about <cmd>\n\n");
				}else if (cmdtoknr==2){
					int i,flag;

					/* find command */
					cmdnr=CMD_NULL; /* nothing found yet */
					for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdstr!=NULL){
							if (strcmp(cmdtab[i].cmdstr,cmdtok[1])==0){
								/* found command string */
								cmdnr=cmdtab[i].cmdnr;
								break; /* done */
							}
						}
					}
					if (cmdnr==CMD_NULL){
						printf("unknown command, try \"help\"\n\n");
						goto main_parser_next;
					}
					printf("\nusage: ");
					flag=0;
					for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdnr==cmdnr){
							if (flag==0){
								/* first */
								if (cmdtab[i].cmdstr!=NULL){
									if (cmdtab[i].cmduse!=NULL){
										printf("%s %s\n",cmdtab[i].cmdstr,cmdtab[i].cmduse);
									}else{
										printf("%s\n",cmdtab[i].cmdstr);
									}
								}
								if (cmdtab[i].cmdhelp!=NULL){
									printf("\n%s\n",cmdtab[i].cmdhelp);
								}
								flag=1;
							}else{
								/* found alias */
								if (flag==1){
									printf("\naliases:\n");
									flag=2;
								}
								if (cmdtab[i].cmdstr!=NULL){
									printf("  %s\n",cmdtab[i].cmdstr);
								}
							}
						}
					}
					printf("\n");
				}
				break;
			case CMD_DF:
				{
					u_int di;

					printf("\n");
					akai_list_alldisks(0,NULL);
					for (di=0;di<disk_num;di++){
						akai_list_disk(&disk[di],0,NULL);
					}
				}
				break;
			case CMD_DINFO:
				printf("\n");
				curdir_info(1); /* 1: verbose */
				break;
			case CMD_CD:
				if (cmdtoknr>1){
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"directory not found\n");
					}
				}else{
					change_curdir_home(); /* ignore error */
				}
				break;
			case CMD_CDI:
				{
					u_int vi;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"must be on sampler partition level\n");
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						fprintf(stderr,"invalid volume index\n");
						goto main_parser_next;
					}
					if (change_curdir(NULL,vi-1,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"directory not found\n");
					}
				}
				break;
			case CMD_DIR:
				save_curdir(0); /* 0: no modifications */
				if (cmdtoknr>1){
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
				}
				printf("\n");
				list_curdir(0); /* 0: non-recursive */
				restore_curdir();
				break;
			case CMD_DIRREC:
				printf("\n");
				list_curdir(1); /* 1: recursive */
				break;
			case CMD_LSTAGS:
				if (curpartp!=NULL){ /* inside a parition or volume? */
					printf("\n");
					akai_list_tags(curpartp);
				}
				printf("\n");
				list_curfiltertags();
				printf("\n");
				break;
			case CMD_INITTAGS:
				{
					u_int i;

					if ((curdiskp!=NULL)&&(curpartp==NULL)){ /* on disk level? */
						/* all S1000/S3000 harddisk sampler partitions on current disk */
						for (i=0;i<part_num;i++){
							/* check if same disk and correct partition type */
							if ((part[i].diskp!=curdiskp)||(part[i].type!=PART_TYPE_HD)){
								continue; /* next */
							}
							/* now, S1000/S3000 harddisk sampler partition */
							printf("partition %c:\n",part[i].letter);
							akai_rename_tag(&part[i],NULL,0,1); /* 1: wipe, XXX ignore error */
						}
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on disk level or sampler partition level\n");
							goto main_parser_next;
						}
						akai_rename_tag(curpartp,NULL,0,1); /* 1: wipe */
					}
					break;
				}
			case CMD_RENTAG:
				{
					u_int ti;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"must be on sampler partition level\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
						fprintf(stderr,"invalid volume index\n");
						goto main_parser_next;
					}
					akai_rename_tag(curpartp,cmdtok[2],ti-1,0); /* 0: don't wipe */
				}
				break;
			case CMD_CDINFO:
			case CMD_VCDINFO:
				if (check_curnosamplerpart()){ /* not on sampler partition level? */
					fprintf(stderr,"must be on sampler partition level\n");
					goto main_parser_next;
				}
				printf("\n");
				akai_print_cdinfo(curpartp,(cmdnr==CMD_VCDINFO)?1:0);
				printf("\n");
				break;
			case CMD_SETCDINFO:
				{
					u_int i;

					if ((curdiskp!=NULL)&&(curpartp==NULL)){ /* on disk level? */
						/* all harddisk sampler partitions on current disk */
						for (i=0;i<part_num;i++){
							/* check if same disk and correct partition type */
							if ((part[i].diskp!=curdiskp)||(part[i].type!=PART_TYPE_HD)){
								continue; /* next */
							}
							/* now, S1000/S3000 harddisk sampler partition */
							printf("partition %c:\n",part[i].letter);
							akai_set_cdinfo(&part[i],(cmdtoknr>=2)?cmdtok[1]:NULL); /* XXX ignore error */
						}
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on disk level or on sampler partition level\n");
							goto main_parser_next;
						}
						akai_set_cdinfo(curpartp,(cmdtoknr>=2)?cmdtok[1]:NULL);
					}
					break;
				}
			case CMD_LCD:
				if (LCHDIR(cmdtok[1])<0){
					fprintf(stderr,"invalid directory path\n");
				}
				break;
			case CMD_LDIR:
				LDIR;
				break;
			case CMD_LSFATI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						fprintf(stderr,"invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						fprintf(stderr,"file not found\n");
						goto main_parser_next;
					}
					printf("\n");
					print_fatchain(curpartp,tmpfile.bstart);
					printf("\n");
				}
				break;
			case CMD_LSTFATI:
				{
					u_int ti;
					u_int cstarts,cstarte;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* find take in current partition */
					cstarts=(curpartp->head.dd.take[ti-1].cstarts[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarts[0];
					if (cstarts==0){ /* empty? */
						fprintf(stderr,"take not found\n");
						goto main_parser_next;
					}
					printf("\n");
					printf("sample:\n");
					print_ddfatchain(curpartp,cstarts);
					printf("\n");
					cstarte=(curpartp->head.dd.take[ti-1].cstarte[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarte[0];
					if (cstarte==0){ /* empty? */
						goto main_parser_next;
					}
					printf("envelope:\n");
					print_ddfatchain(curpartp,cstarte);
					printf("\n");
				}
				break;
			case CMD_INFOI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						fprintf(stderr,"invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						fprintf(stderr,"file not found\n");
						goto main_parser_next;
					}
					/* print info */
					printf("\n");
					curdir_info(0);
					printf("/");
					akai_file_info(&tmpfile,1); /* 1: verbose */
				}
				break;
			case CMD_INFOALL:
				{
					struct file_s tmpfile;
					u_int sfi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					for (sfi=0;sfi<curvolp->fimax;sfi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi)<0){
							continue; /* next */
						}
						/* print info */
						printf("\n");
						curdir_info(0);
						printf("/");
						akai_file_info(&tmpfile,1); /* 1: verbose */
					}
				}
				break;
			case CMD_TINFOI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* print info */
					printf("\n");
					curdir_info(0);
					printf("/");
					akai_ddtake_info(curpartp,ti-1,1); /* 1: verbose */
				}
				break;
			case CMD_TINFOALL:
				{
					u_int ti;
					u_int cstarts;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* all DD takes in current DD partition */
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						cstarts=(curpartp->head.dd.take[ti].cstarts[1]<<8)
							    +curpartp->head.dd.take[ti].cstarts[0];
						if (cstarts==0){ /* empty? */
							continue; /* next */
						}
						/* print info */
						printf("\n");
						curdir_info(0);
						printf("/");
						akai_ddtake_info(curpartp,ti,1); /* 1: verbose */
					}
				}
				break;
			case CMD_DEL:
				{
					struct file_s tmpfile;

					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be a file in a sampler volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){
						fprintf(stderr,"invalid file name\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* find file in current directory */
					if (akai_find_file(curvolp,&tmpfile,dirnamebuf)<0){
						fprintf(stderr,"file not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* delete file */
					if (akai_delete_file(&tmpfile)<0){
						fprintf(stderr,"delete error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_DELI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						fprintf(stderr,"invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						fprintf(stderr,"file not found\n");
						goto main_parser_next;
					}
					/* delete file */
					if (akai_delete_file(&tmpfile)<0){
						fprintf(stderr,"delete error\n");
					}
				}
				break;
			case CMD_TDELI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* delete take */
					if (akai_delete_ddtake(curpartp,ti-1)<0){
						fprintf(stderr,"cannot delete take\n");
					}
				}
				break;
			case CMD_REN:
			case CMD_RENI:
				{
					struct file_s tmpfile,dummyfile;
					struct vol_s tmpvol;
					u_int sfi,dfi;

					/* source */
					if (cmdnr==CMD_RENI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						/* source file index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							fprintf(stderr,"invalid source file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						/* must save current volume for tmpfile, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_get_file(&tmpvol,&tmpfile,sfi-1)<0){
							fprintf(stderr,"file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"source must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							fprintf(stderr,"invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							fprintf(stderr,"file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						fprintf(stderr,"destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpfile.name);
					}
					if (cmdtoknr>3){
						/* destination file index */
						dfi=(u_int)atoi(cmdtok[3]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							fprintf(stderr,"invalid destination file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						dfi--;
						/* XXX akai_rename_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
#if 1
						/* check if new file name already used */
						if (akai_find_file(curvolp,&dummyfile,dirnamebuf)==0){
							fprintf(stderr,"file name already used\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif
					}
					/* rename file */
					if (akai_rename_file(&tmpfile,dirnamebuf,curvolp,dfi,NULL,tmpfile.osver)<0){
						fprintf(stderr,"cannot rename file\n");
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&tmpfile); /* ignore error */
					restore_curdir();
				}
				break;
			case CMD_SETOSVERI:
			case CMD_SETUNCOMPRI:
			case CMD_UPDATEUNCOMPRI:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int osver;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						fprintf(stderr,"invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						fprintf(stderr,"file not found\n");
						goto main_parser_next;
					}
					if (cmdnr==CMD_SETOSVERI){
						if (curvolp->type==AKAI_VOL_TYPE_S900){
							fprintf(stderr,"must be an S1000/S3000 volume\n");
							goto main_parser_next;
						}
						/* OS version of file in S1000/S3000 volume */
						if (strstr(cmdtok[2],".")!=NULL){
							/* format: "XX.XX" */
							osver=(u_int)(100.0*atof(cmdtok[2]));
							osver=(((osver/100))<<8)|(osver%100);
						}else{
							/* integer format */
							osver=(u_int)atoi(cmdtok[2]);
						}
					}else{
						if (curvolp->type!=AKAI_VOL_TYPE_S900){
							fprintf(stderr,"must be an S900 volume\n");
							goto main_parser_next;
						}
						if (tmpfile.osver==0){
							fprintf(stderr,"not a compressed file\n");
							goto main_parser_next;
						}
						if (cmdtoknr>=3){
							/* given uncompr value in integer format */
							osver=(u_int)atoi(cmdtok[2]);
						}else{
							/* update uncompr value */
							if (akai_s900comprfile_updateuncompr(&tmpfile)<0){
								fprintf(stderr,"cannot set uncompr value\n");
							}
							goto main_parser_next; /* done */
						}
					}
					/* set osver of file */
					/* Note: akai_create_file() will correct osver if necessary */
					if (akai_rename_file(&tmpfile,NULL,curvolp,AKAI_CREATE_FILE_NOINDEX,NULL,osver)<0){
						if (cmdnr==CMD_SETOSVERI){
							fprintf(stderr,"cannot set OS version\n");
						}else{
							fprintf(stderr,"cannot set uncompr value\n");
						}
					}
				}
				break;
			case CMD_SETOSVERALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int osver;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						fprintf(stderr,"must be an S1000/S3000 volume\n");
						goto main_parser_next;
					}
					/* OS version of file in S1000/S3000 volume */
					if (strstr(cmdtok[2],".")!=NULL){
						/* format: "XX.XX" */
						osver=(u_int)(100.0*atof(cmdtok[2]));
						osver=(((osver/100))<<8)|(osver%100);
					}else{
						/* integer format */
						osver=(u_int)atoi(cmdtok[2]);
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						printf("updating \"%s\"\n",tmpfile.name);
						fflush(NULL);
						/* set osver of file */
						/* Note: akai_create_file() will correct osver if necessary */
						if (akai_rename_file(&tmpfile,NULL,curvolp,AKAI_CREATE_FILE_NOINDEX,NULL,osver)<0){
							fprintf(stderr,"cannot set OS version\n");
							continue; /* next */
						}
						fcount++;
					}
					printf("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_UPDATEUNCOMPRALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type!=AKAI_VOL_TYPE_S900){
						fprintf(stderr,"must be an S900 volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						if (tmpfile.osver!=0){ /* compressed file? */
							printf("updating \"%s\"\n",tmpfile.name);
							fflush(NULL);
							/* update uncompr value */
							if (akai_s900comprfile_updateuncompr(&tmpfile)<0){
								fprintf(stderr,"cannot set uncompr value\n");
								continue; /* next */
							}
							fcount++;
						}
					}
					printf("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_SAMPLE900UNCOMPR:
			case CMD_SAMPLE900UNCOMPRI:
			case CMD_SAMPLE900COMPR:
			case CMD_SAMPLE900COMPRI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					struct vol_s destvol;
					struct vol_s *volp;
					u_int fi;

					if ((cmdnr==CMD_SAMPLE900UNCOMPRI)||(cmdnr==CMD_SAMPLE900COMPRI)){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						/* index */
						fi=(u_int)atoi(cmdtok[1]);
						if ((fi<1)||(fi>curvolp->fimax)){
							fprintf(stderr,"invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
							fprintf(stderr,"file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							fprintf(stderr,"invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							fprintf(stderr,"file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					if (cmdtoknr>=3){
						save_curdir(0); /* 0: no modifications */
						/* given destination volume */
						if (change_curdir(cmdtok[2],0,NULL,1)<0){ /* NULL,1: check last */
							fprintf(stderr,"destination volume not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"destination must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* must save current volume */
						akai_copy_structvol(curvolp,&destvol);
						volp=&destvol;
						restore_curdir();
					}else{
						/* replace file in same volume */
						volp=NULL;
					}
					save_curdir(1); /* 1: could be modifications */
					if ((cmdnr==CMD_SAMPLE900UNCOMPR)||(cmdnr==CMD_SAMPLE900UNCOMPRI)){
						printf("uncompressing \"%s\"\n",tmpfile.name);
						fflush(NULL);
						/* uncompress S900 compressed sample file */
						akai_sample900_compr2noncompr(&tmpfile,volp); /* ignore error */
					}else{
						printf("compressing \"%s\"\n",tmpfile.name);
						fflush(NULL);
						/* compress S900 non-compressed sample file */
						akai_sample900_noncompr2compr(&tmpfile,volp); /* ignore error */
					}
					restore_curdir();
				}
				break;
			case CMD_SAMPLE900UNCOMPRALL:
			case CMD_SAMPLE900COMPRALL:
				{
					struct file_s tmpfile;
					struct vol_s destvol;
					struct vol_s *volp;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					if (cmdtoknr>=2){
						save_curdir(0); /* 0: no modifications */
						/* given destination volume */
						if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
							fprintf(stderr,"destination volume not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"destination must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* must save current volume */
						akai_copy_structvol(curvolp,&destvol);
						volp=&destvol;
						restore_curdir();
					}else{
						/* replace files in same volume */
						volp=NULL;
					}
					/* all files in current volume */
					save_curdir(1); /* 1: could be modifications */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						if (cmdnr==CMD_SAMPLE900UNCOMPRALL){
							if ((tmpfile.type!=AKAI_SAMPLE900_FTYPE)||(tmpfile.osver==0)){ /* not S900 compressed sample file? */
								continue; /* next */
							}
							printf("uncompressing \"%s\"\n",tmpfile.name);
							fflush(NULL);
							/* uncompress S900 compressed sample file */
							if (akai_sample900_compr2noncompr(&tmpfile,volp)<0){
								continue; /* next */
							}
							fcount++;
						}else{
							if ((tmpfile.type!=AKAI_SAMPLE900_FTYPE)||(tmpfile.osver!=0)){ /* not S900 non-compressed sample file? */
								continue; /* next */
							}
							printf("compressing \"%s\"\n",tmpfile.name);
							fflush(NULL);
							/* compress S900 non-compressed sample file */
							if (akai_sample900_noncompr2compr(&tmpfile,volp)<0){
								continue; /* next */
							}
							fcount++;
						}
					}
					restore_curdir();
					if (cmdnr==CMD_SAMPLE900UNCOMPRALL){
						printf("uncompressed %u file(s)\n",fcount);
					}else{
						printf("compressed %u file(s)\n",fcount);
					}
				}
				break;
			case CMD_FIXRAMNAME:
			case CMD_FIXRAMNAMEI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					u_int fi;

					if (cmdnr==CMD_FIXRAMNAMEI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						/* index */
						fi=(u_int)atoi(cmdtok[1]);
						if ((fi<1)||(fi>curvolp->fimax)){
							fprintf(stderr,"invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
							fprintf(stderr,"file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							fprintf(stderr,"invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							fprintf(stderr,"file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&tmpfile); /* ignore error */
				}
				break;
			case CMD_FIXRAMNAMEALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						printf("fixing \"%s\"\n",tmpfile.name);
						fflush(NULL);
						/* fix name in header (if necessary) */
						if (akai_fixramname(&tmpfile)<0){
							continue; /* next */
						}
						fcount++;
					}
					printf("fixed %u file(s)\n",fcount);
				}
				break;
			case CMD_CLRTAGI:
			case CMD_SETTAGI:
				{
					struct file_s tmpfile;
					u_int fi,ti;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						fprintf(stderr,"no tags for this volume type\n");
						goto main_parser_next;
					}
					/* file index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						fprintf(stderr,"invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						fprintf(stderr,"file not found\n");
						goto main_parser_next;
					}
					/* tag index */
					if ((cmdnr==CMD_CLRTAGI)&&(strcasecmp(cmdtok[2],"all")==0)){
						/* clear all */
						if (tmpfile.osver<=AKAI_OSVER_S1100MAX){ /* XXX */
							ti=AKAI_FILE_TAGS1000;
						}else{
							ti=AKAI_FILE_TAGFREE;
						}
					}else{
						ti=(u_int)atoi(cmdtok[2]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							fprintf(stderr,"invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* set/clear tag */
					if (cmdnr==CMD_SETTAGI){
						if (akai_set_filetag(tmpfile.tag,ti)<0){
							goto main_parser_next;
						}
					}else{
						if (akai_clear_filetag(tmpfile.tag,ti)<0){
							goto main_parser_next;
						}
					}
					/* update file */
					if (akai_rename_file(&tmpfile,NULL,NULL,AKAI_CREATE_FILE_NOINDEX,tmpfile.tag,tmpfile.osver)<0){
						fprintf(stderr,"cannot update file\n");
					}
				}
				break;
			case CMD_CLRTAGALL:
			case CMD_SETTAGALL:
				{
					struct file_s tmpfile;
					u_int fi,ti;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						fprintf(stderr,"no tags for this volume type\n");
						goto main_parser_next;
					}
					/* tag index */
					if ((cmdnr==CMD_CLRTAGALL)&&(strcasecmp(cmdtok[1],"all")==0)){
						/* clear all */
						ti=AKAI_FILE_TAGFREE;
						/* Note: ti will be adjusted below if necessary */
					}else{
						ti=(u_int)atoi(cmdtok[1]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							fprintf(stderr,"invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						/* adjust tag index if necessary */
						if ((cmdnr==CMD_CLRTAGALL)&&((ti==AKAI_FILE_TAGFREE)||(ti==AKAI_FILE_TAGS1000))){
							/* clear all */
							if (tmpfile.osver<=AKAI_OSVER_S1100MAX){ /* XXX */
								ti=AKAI_FILE_TAGS1000;
							}else{
								ti=AKAI_FILE_TAGFREE;
							}
						}
						printf("updating \"%s\"\n",tmpfile.name);
						fflush(NULL);
						/* set/clear tag */
						if (cmdnr==CMD_SETTAGALL){
							if (akai_set_filetag(tmpfile.tag,ti)<0){
								fprintf(stderr,"cannot set tag\n");
								continue; /* next */
							}
						}else{
							if (akai_clear_filetag(tmpfile.tag,ti)<0){
								fprintf(stderr,"cannot clear tag\n");
								continue; /* next */
							}
						}
						/* update file */
						if (akai_rename_file(&tmpfile,NULL,NULL,AKAI_CREATE_FILE_NOINDEX,tmpfile.tag,tmpfile.osver)<0){
							fprintf(stderr,"cannot update file\n");
							continue; /* next */
						}
						fcount++;
					}
					printf("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_TRENI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* rename take */
					if (akai_rename_ddtake(curpartp,ti-1,cmdtok[2])<0){
						fprintf(stderr,"cannot rename take\n");
					}
				}
				break;
			case CMD_CLRFILTERTAG:
			case CMD_SETFILTERTAG:
				{
					u_int ti;

					/* tag index */
					if ((cmdnr==CMD_CLRFILTERTAG)&&(strcasecmp(cmdtok[1],"all")==0)){
						ti=AKAI_FILE_TAGFREE; /* clear all */
					}else{
						ti=(u_int)atoi(cmdtok[1]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							fprintf(stderr,"invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* set/clear tag */
					if (cmdnr==CMD_SETFILTERTAG){
						if (akai_set_filetag(curfiltertag,ti)<0){
							goto main_parser_next;
						}
					}else{
						if (akai_clear_filetag(curfiltertag,ti)<0){
							goto main_parser_next;
						}
					}
				}
				break;
			case CMD_COPY:
			case CMD_COPYI:
				{
					struct file_s tmpfile,dstfile;
					struct vol_s tmpvol;
					u_int sfi,dfi;

					/* source */
					if (cmdnr==CMD_COPYI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						/* source file index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							fprintf(stderr,"invalid source file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						/* must save current volume for tmpfile, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_get_file(&tmpvol,&tmpfile,sfi-1)<0){
							fprintf(stderr,"file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"source must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							fprintf(stderr,"invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							fprintf(stderr,"source file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						fprintf(stderr,"destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpfile.name);
					}
					if (cmdtoknr>3){
						/* destination file index */
						dfi=(u_int)atoi(cmdtok[3]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							fprintf(stderr,"invalid destination file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						dfi--;
						/* XXX copy_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
					}
					/* copy file */
					if (copy_file(&tmpfile,curvolp,&dstfile,dfi,dirnamebuf,1)<0){ /* 1: overwrite */
						fprintf(stderr,"cannot copy file\n");
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&dstfile); /* ignore error */
					restore_curdir();
				}
				break;
			case CMD_COPYVOL:
			case CMD_COPYVOLI:
				{
					struct vol_s tmpvol;
					u_int vi;

					/* source */
					if (cmdnr==CMD_COPYVOLI){
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid source volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"source directory not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
							fprintf(stderr,"source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"source must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* must save current volume, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						fprintf(stderr,"destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"destination must be a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpvol.name);
					}
					if ((strcmp(dirnamebuf,".")==0)||(strcmp(dirnamebuf,"..")==0)){
						fprintf(stderr,"invalid destination volume name\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_vol_allfiles(&tmpvol,curpartp,dirnamebuf,1,1)<0){ /* 1: overwrite, 1: verbose */
						fprintf(stderr,"copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_COPYPART:
				{
					struct part_s *tmppartp;

					/* source */
					save_curdir(0); /* 0: no modifications */
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"source directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"source must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					tmppartp=curpartp; /* source partition */
					restore_curdir();
					/* destination */
					/* Note: allow invalid destination partion!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"destination must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_part_allvols(tmppartp,curpartp,1,2)<0){ /* 1: overwrite, 2: verbose */
						fprintf(stderr,"copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_COPYTAGS:
				{
					struct part_s *tmppartp;

					/* source */
					save_curdir(0); /* 0: no modifications */
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"source directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"source must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					tmppartp=curpartp; /* source partition */
					restore_curdir();
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,NULL,1)<0){ /* NULL,1: check last */
						fprintf(stderr,"destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"destination must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_tags(tmppartp,curpartp)<0){
						fprintf(stderr,"copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_WIPEVOL:
			case CMD_DELVOL:
				{

					/* Note: allow invalid volume to be deleted!!! */
					/* Note: might delete original current volume!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* delete volume */
					if (akai_wipe_vol(curvolp,(cmdnr==CMD_DELVOL)?1:0)<0){
						fprintf(stderr,"delete error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_WIPEVOLI:
			case CMD_DELVOLI:
				{
					struct vol_s tmpvol;
					u_int vi;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"must be on sampler partition level\n");
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						fprintf(stderr,"invalid volume index\n");
						goto main_parser_next;
					}
					/* find volume in current partition */
					if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
						fprintf(stderr,"volume not found\n");
						goto main_parser_next;
					}
					/* Note: volume may be corrupt */
					/* delete volume */
					if (akai_wipe_vol(&tmpvol,(cmdnr==CMD_DELVOLI)?1:0)<0){
						fprintf(stderr,"delete error\n");
					}
				}
				break;
			case CMD_FIXPART:
			case CMD_WIPEPART:
			case CMD_WIPEPART3CD:
				{
					int wipeflag;
					int cdromflag;

					/* Note: allow invalid partion to be fixed/wiped!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnopart()){ /* not on partition level (sampler or DD)? */
						fprintf(stderr,"must be a partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdnr==CMD_WIPEPART){
						wipeflag=1;
						cdromflag=0;
					}else if (cmdnr==CMD_WIPEPART3CD){
						wipeflag=1;
						cdromflag=1;
					}else{
						wipeflag=0;
						cdromflag=0;
					}
					/* wipe partition, supply part[] to fix partition table if necessary */
					if (akai_wipe_part(curpartp,wipeflag,&part[0],part_num,cdromflag)<0){
						fprintf(stderr,"cannot wipe partition\n");
					}
					if (cmdnr==CMD_WIPEPART3CD){
						/* XXX init tags (checked by cdinfo) */
						akai_rename_tag(curpartp,NULL,0,1); /* 1: wipe */
					}
					restore_curdir();
				}
				break;
			case CMD_FIXDISK:
				{
					u_int pi;
					u_int i;

					/* Note: allow invalid disk to be fixed!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp==NULL){
						fprintf(stderr,"must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdtoknr>2){ /* last partition number specified? */
						i=(u_int)atoi(cmdtok[2]); /* partition number limit */
						if ((i<1)||(i>AKAI_PART_NUM)){
							fprintf(stderr,"invalid partition number limit\n");
							restore_curdir();
							goto main_parser_next;
						}
					}else{
						i=AKAI_PART_NUM;
					}
					/* scan partitions: disable if necessary */
					for (pi=0;pi<part_num;pi++){
						if (part[pi].diskp!=curdiskp){ /* not on same disk? */
							continue; /* next partition */
						}
						if (part[pi].type==PART_TYPE_DD){
							continue; /* next partition */
						}
						if (part[pi].index>=i){ /* limit? */
							/* disable partition */
							part[pi].fd=-1;
							part[pi].valid=0;
						}
					}
					/* now, all partitions known for a new partition table in first partition */
					/* scan partitions again: fix */
					for (pi=0;pi<part_num;pi++){
						if (part[pi].diskp!=curdiskp){ /* not on same disk? */
							continue; /* next partition */
						}
						if (part[pi].type==PART_TYPE_DD){
							continue; /* next partition */
						}
						/* Note: allow invalid partitions to be fixed */
						/* fix partition, supply part[] to fix partition table if necessary */
						if (akai_wipe_part(&part[pi],0,&part[0],part_num,0)<0){ /* 0: fix only */
							fprintf(stderr,"cannot fix partition %c\n",part[pi].letter);
							/* XXX ignore, continue with others */
						}
					}
					restore_curdir();
					/* Note: will end up on disk level in case we have just deleted the current partition!!! */
				}
				break;
			case CMD_FORMATFLOPPYL9:
			case CMD_FORMATFLOPPYL1:
			case CMD_FORMATFLOPPYL3:
			case CMD_FORMATFLOPPYH9:
			case CMD_FORMATFLOPPYH1:
			case CMD_FORMATFLOPPYH3:
				{
					int ret;
					int lodensflag,s3000flag,s900flag;

					if (curdiskp==NULL){
						fprintf(stderr,"must be on a disk\n");
						goto main_parser_next;
					}
					flush_blk_cache(); /* XXX if error, maybe next time more luck */

					switch (cmdnr){
					case CMD_FORMATFLOPPYL9:
						lodensflag=1;
						s3000flag=0;
						s900flag=1;
						break;
					case CMD_FORMATFLOPPYL1:
						lodensflag=1;
						s3000flag=0;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYL3:
						lodensflag=1;
						s3000flag=1;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYH9:
						lodensflag=0;
						s3000flag=0;
						s900flag=1;
						break;
					case CMD_FORMATFLOPPYH1:
						lodensflag=0;
						s3000flag=0;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYH3:
					default:
						lodensflag=0;
						s3000flag=1;
						s900flag=0;
						break;
					}

					ret=format_floppy(curdiskp,lodensflag,s3000flag,s900flag);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						fprintf(stderr,"format error\n");
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					flush_blk_cache(); /* XXX if error, too late */
#if 1
					if (ret>0){ /* fatal? */
						exit(ret);
					}
					printf("\nrestarting system\n");
					goto main_restart; /* restart whole system */
#else
					exit(ret);
#endif
				}
				break;
			case CMD_FORMATHARDDISK9:
				{
					u_int l;
					u_int totb;
					int ret;

					if (curdiskp==NULL){
						fprintf(stderr,"must be on a disk\n");
						goto main_parser_next;
					}
					flush_blk_cache(); /* XXX if error, maybe next time more luck */

					/* total size to format */
					if (cmdtoknr>=2){
						l=(u_int)strlen(cmdtok[1]);
						if ((l>=1)&&((cmdtok[1][l-1]=='M')||(cmdtok[1][l-1]=='m'))){ /* in MB? */
							cmdtok[1][l-1]='\0'; /* XXX remove letter */
							l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
						}else{
							l=1; /* block */
						}
						totb=l*(u_int)atoi(cmdtok[1]);
					}else{
						totb=AKAI_HD9_MAXSIZE; /* maximum */
					}
					/* Note: format_harddisk9() will truncate totb if required */

					ret=format_harddisk9(curdiskp,totb);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						fprintf(stderr,"format error\n");
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					flush_blk_cache(); /* XXX if error, too late */
#if 1
					if (ret>0){ /* fatal? */
						exit(ret);
					}
					printf("\nrestarting system\n");
					goto main_restart; /* restart whole system */
#else
					exit(ret);
#endif
				}
				break;
			case CMD_FORMATHARDDISK1:
			case CMD_FORMATHARDDISK3:
			case CMD_FORMATHARDDISK3CD:
				{
					u_int l,bsize,totb;
					int s3000flag;
					int cdromflag;
					int ret;

					if (curdiskp==NULL){
						fprintf(stderr,"must be on a disk\n");
						goto main_parser_next;
					}
					flush_blk_cache(); /* XXX if error, maybe next time more luck */

					/* partition size */
					if (cmdtoknr>=2){
						l=(u_int)strlen(cmdtok[1]);
						if ((l>=1)&&((cmdtok[1][l-1]=='M')||(cmdtok[1][l-1]=='m'))){ /* in MB? */
							cmdtok[1][l-1]='\0'; /* XXX remove letter */
							l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
						}else{
							l=1; /* block */
						}
						bsize=l*(u_int)atoi(cmdtok[1]);
					}else{
						bsize=AKAI_PART_MAXSIZE; /* maximum */
					}
					/* Note: format_harddisk() will truncate bsize if required */
					/* total size to format */
					if (cmdtoknr>=3){
						l=(u_int)strlen(cmdtok[2]);
						if ((l>=1)&&((cmdtok[2][l-1]=='M')||(cmdtok[2][l-1]=='m'))){ /* in MB? */
							cmdtok[2][l-1]='\0'; /* XXX remove letter */
							l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
						}else{
							l=1; /* block */
						}
						totb=l*(u_int)atoi(cmdtok[2]);
					}else{
						totb=AKAI_HD_MAXSIZE; /* maximum */
					}
					/* Note: format_harddisk() will truncate totb if required */
					if (cmdnr==CMD_FORMATHARDDISK3){
						s3000flag=1;
						cdromflag=0;
					}else if (cmdnr==CMD_FORMATHARDDISK3CD){
						s3000flag=1;
						cdromflag=1;
					}else{
						s3000flag=0;
						cdromflag=0;
					}
					ret=format_harddisk(curdiskp,bsize,totb,s3000flag,cdromflag);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						fprintf(stderr,"format error\n");
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					flush_blk_cache(); /* XXX if error, too late */
#if 1
					if (ret>0){ /* fatal? */
						exit(ret);
					}
					printf("\nrestarting system\n");
					goto main_restart; /* restart whole system */
#else
					exit(ret);
#endif
				}
				break;
			case CMD_GETDISK:
				{
					int outfd;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];

					save_curdir(1); /* 1: could be modifications */
					if (curdiskp==NULL){ /* not on a disk? */
						fprintf(stderr,"must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* create external file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						perror("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* export */
					printf("\n");
					for (blk=0;blk<curdiskp->totsize;blk++){
						print_progressbar(curdiskp->totsize,blk);
						/* read block */
						if (io_blks(curdiskp->fd,fbuf,
									blk,
									1,
									curdiskp->blksize,
									0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
							break;
						}
						/* write block */
						if (WRITE(outfd,fbuf,curdiskp->blksize)!=(int)curdiskp->blksize){
							perror("write");
							break;
						}
					}
					printf(".\n\n");
					/* close file */
					CLOSE(outfd);
					restore_curdir();
				}
				break;
			case CMD_PUTDISK:
				{
					int ret;
					int inpfd;
					struct stat instat;
					u_int size,bsize;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];

					flush_blk_cache(); /* XXX if error, maybe next time more luck */

					save_curdir(1); /* 1: could be modifications */
					if (curdiskp==NULL){ /* not on a disk? */
						fprintf(stderr,"must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=OPEN(cmdtok[1],O_RDONLY|O_BINARY,0))<0){
						perror("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						perror("stat");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					size=(u_int)instat.st_size; /* size in bytes */
					/* check blocksize */
					if (curdiskp->blksize==0){
						if ((curdiskp->type==DISK_TYPE_HD9)||(curdiskp->type==DISK_TYPE_HD)){
							curdiskp->blksize=AKAI_HD_BLOCKSIZE; /* XXX */
						}else{
							curdiskp->blksize=AKAI_FL_BLOCKSIZE; /* XXX */
						}
					}
					bsize=size/curdiskp->blksize; /* size in blocks, round down (blksize>0 see above) */
					/* check bsize */
					if (bsize>curdiskp->totsize){
						bsize=curdiskp->totsize;
						fprintf(stderr,"truncated to disk size\n");
					}
					curdiskp->totsize=bsize; /* new partition size */
					/* import */
					ret=0;
					printf("\n");
					for (blk=0;blk<curdiskp->totsize;blk++){
						print_progressbar(curdiskp->totsize,blk);
						/* read block */
						if (READ(inpfd,fbuf,curdiskp->blksize)!=(int)curdiskp->blksize){
							perror("read");
							ret=1;
							break;
						}
						/* write block */
						if (io_blks(curdiskp->fd,fbuf,
									blk,
									1,
									curdiskp->blksize,
									0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
							ret=1;
							break;
						}
					}
					printf(".\n\n");
					/* close file */
					CLOSE(inpfd);

					/* must exit (or restart) now!!! */
					flush_blk_cache(); /* XXX if error, too late */
#if 1
					if (ret>0){ /* fatal? */
						exit(ret);
					}
					printf("\nrestarting system\n");
					goto main_restart; /* restart whole system */
#else
					exit(ret);
#endif
				}
				break;
			case CMD_GETPART:
				{
					int outfd;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];

					/* Note: allow invalid partion to be exported!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnopart()){ /* not on partition level (sampler or DD)? */
						fprintf(stderr,"must be a partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* create external file */
					if ((outfd=OPEN(cmdtok[2],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						perror("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* export */
					printf("\n");
					for (blk=0;blk<curpartp->bsize;blk++){
						print_progressbar(curpartp->bsize,blk);
						/* read block */
						if (akai_io_blks(curpartp,fbuf,
										 blk,
										 1,
										 0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
							break;
						}
						/* write block */
						if (WRITE(outfd,fbuf,curpartp->blksize)!=(int)curpartp->blksize){
							perror("write");
							break;
						}
					}
					printf(".\n\n");
					/* close file */
					CLOSE(outfd);
					restore_curdir();
				}
				break;
			case CMD_PUTPART:
				{
					int ret;
					int inpfd;
					struct stat instat;
					u_int size,bsize;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];
					struct akai_parthead_s tmppart;

					flush_blk_cache(); /* XXX if error, maybe next time more luck */

					/* Note: allow import to invalid partion!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,NULL,0)<0){ /* NULL,0: don't check last */
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnopart()){ /* not on partition level (sampler or DD)? */
						fprintf(stderr,"must be a partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curpartp->bstart>curdiskp->totsize){
						fprintf(stderr,"invalid partition start\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=OPEN(cmdtok[1],O_RDONLY|O_BINARY,0))<0){
						perror("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						perror("stat");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					size=(u_int)instat.st_size; /* size in bytes */
					/* check blocksize */
					if (curpartp->blksize==0){
						if ((curdiskp->type==DISK_TYPE_HD9)||(curdiskp->type==DISK_TYPE_HD)){
							curdiskp->blksize=AKAI_HD_BLOCKSIZE; /* XXX */
						}else{
							curdiskp->blksize=AKAI_FL_BLOCKSIZE; /* XXX */
						}
						curpartp->blksize=curdiskp->blksize;
					}
					bsize=size/curpartp->blksize; /* size in blocks, round down (blksize>0 see above) */
					/* check bsize */
					if ((curdiskp->type==DISK_TYPE_FLL)&&(bsize>AKAI_FLL_SIZE)){
						bsize=AKAI_FLL_SIZE;
						fprintf(stderr,"truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_FLH)&&(bsize>AKAI_FLH_SIZE)){
						bsize=AKAI_FLH_SIZE;
						fprintf(stderr,"truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_HD9)&&(bsize>AKAI_HD9_MAXSIZE)){
						bsize=AKAI_HD9_MAXSIZE;
						fprintf(stderr,"truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_HD)&&(bsize>AKAI_PART_MAXSIZE)){
						bsize=AKAI_PART_MAXSIZE;
						fprintf(stderr,"truncated to max. partition size\n");
					}
					if (curpartp->bstart+bsize>curdiskp->totsize){
						bsize=curdiskp->totsize-curpartp->bstart;
						fprintf(stderr,"truncated to remaining disk size\n");
					}
					if (bsize>curpartp->bsize){
#if 1
						fprintf(stderr,"import larger than current partition size, ignored\n");
#else
						bsize=pp->bsize;
						fprintf(stderr,"truncated to current partition size\n");
#endif
					}
					curpartp->bsize=bsize; /* new partition size */
					/* import */
					ret=0;
					printf("\n");
					for (blk=0;blk<curpartp->bsize;blk++){
						print_progressbar(curpartp->bsize,blk);
#if 1
						if ((curpartp->type==PART_TYPE_HD)&&(curpartp->index==0) /* first partition on S1000/S3000 harddisk? */
							&&(blk==0)&&(curpartp->bsize>=AKAI_PARTHEAD_BLKS)){ /* in header? */
							/* read partition header */
							if (READ(inpfd,(u_char *)&tmppart,AKAI_PARTHEAD_BLKS*curpartp->blksize)
								!=(int)(AKAI_PARTHEAD_BLKS*curpartp->blksize)){
									perror("read");
									ret=1;
									break;
							}
							/* transfer partition table */
							bcopy(&(curpartp->head.hd.parttab),
								  &(tmppart.parttab),
								  sizeof(struct akai_parttab_s));
							/* write partition header */
							if (akai_io_blks(curpartp,(u_char *)&tmppart,
											 0, /* partition header block */
											 AKAI_PARTHEAD_BLKS,
											 0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
								ret=1;
								break;
							}
							blk=AKAI_PARTHEAD_BLKS-1;
							continue;
						}
#endif
						/* read block */
						if (READ(inpfd,fbuf,curpartp->blksize)!=(int)curpartp->blksize){
							perror("read");
							ret=1;
							break;
						}
						/* write block */
						if (akai_io_blks(curpartp,fbuf,
										 blk,
										 1,
										 0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
							ret=1;
							break;
						}
					}
					printf(".\n\n");
					/* close file */
					CLOSE(inpfd);

					/* must exit (or restart) now!!! */
					flush_blk_cache(); /* XXX if error, too late */
#if 1
					if (ret>0){ /* fatal? */
						exit(ret);
					}
					printf("\nrestarting system\n");
					goto main_restart; /* restart whole system */
#else
					exit(ret);
#endif
				}
				break;
			case CMD_GETTAGS:
				{
					int outfd;
					u_int size;

					if ((curpartp==NULL)||(curpartp->type!=PART_TYPE_HD)){
						fprintf(stderr,"must be inside an S1000/S3000 sampler partition\n");
						goto main_parser_next;
					}
					if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
						fprintf(stderr,"no tags in partition\n");
						goto main_parser_next;
					}
					/* create external file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						perror("open");
						goto main_parser_next;
					}
					/* export tags-file */
					size=4+AKAI_PARTHEAD_TAGNUM*AKAI_NAME_LEN; /* tags magic and tag names */;
					if (WRITE(outfd,(void *)curpartp->head.hd.tagsmagic,size)!=(int)size){
						perror("write");
						goto main_parser_next;
					}
					CLOSE(outfd);
				}
				break;
			case CMD_PUTTAGS:
				{
					int inpfd;
					struct stat instat;
					u_int size;

					/* open external file */
					if ((inpfd=OPEN(cmdtok[1],O_RDONLY|O_BINARY,0))<0){
						perror("open");
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						perror("stat");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					size=(u_int)instat.st_size;
					if (size!=(4+AKAI_PARTHEAD_TAGNUM*AKAI_NAME_LEN)){ /* invalid file size? (Note: tags magic and tag names) */
						fprintf(stderr,"invalid file size of tags-file\n");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					/* read tags-file into partition header */
					if (READ(inpfd,(void *)curpartp->head.hd.tagsmagic,size)!=(int)size){
						perror("read");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					/* close file */
					CLOSE(inpfd);
					/* XXX no check if valid tags magic */
					/* write partition header */
					if (akai_io_blks(curpartp,(u_char *)&curpartp->head.hd,
									 0,
									 AKAI_PARTHEAD_BLKS,
									 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
						fprintf(stderr,"cannot write partition header\n");
						goto main_parser_next;
					}
					printf("tags imported\n");
				}
				break;
			case CMD_RENVOL:
			case CMD_RENVOLI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if ((strcmp(cmdtok[2],".")==0)||(strcmp(cmdtok[2],"..")==0)){
						fprintf(stderr,"invalid volume name\n");
						goto main_parser_next;
					}

					/* Note: allow invalid volume to be renamed!!! */
					if (cmdnr==CMD_RENVOL){
						save_curdir(1); /* 1: could be modifications */
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							fprintf(stderr,"directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
#if 1
					if (curpartp!=NULL){
						struct vol_s tmpvol2;

						/* check if new volume name already used */
						if (akai_find_vol(curpartp,&tmpvol2,cmdtok[2])==0){
							fprintf(stderr,"volume name already used\n");
							if (cmdnr==CMD_RENVOL){
								restore_curdir();
							}
							goto main_parser_next;
						}
					}
#endif
					/* rename volume */
					if (akai_rename_vol(vp,cmdtok[2],vp->lnum,vp->osver,NULL)<0){
						fprintf(stderr,"rename error\n");
					}
					if (cmdnr==CMD_RENVOL){
						restore_curdir();
					}
				}
				break;
			case CMD_SETOSVERVOL:
			case CMD_SETOSVERVOLI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *osvername;
					u_int osver;

					if (cmdnr==CMD_SETOSVERVOL){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						if ((curpartp->type==PART_TYPE_HD9)||(curpartp->type==PART_TYPE_HD)
							||(curvolp->type==AKAI_VOL_TYPE_S900)){
							fprintf(stderr,"must be an S1000/S3000 floppy volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						osvername=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						if ((curpartp->type==PART_TYPE_HD9)||(curpartp->type==PART_TYPE_HD)
							||(curvolp->type==AKAI_VOL_TYPE_S900)){
							fprintf(stderr,"must be an S1000/S3000 floppy volume\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						osvername=cmdtok[2];
					}
					/* OS version */
					if (strstr(osvername,".")!=NULL){
						/* format: "XX.XX" */
						osver=(u_int)(100.0*atof(osvername));
						osver=(((osver/100))<<8)|(osver%100);
					}else{
						/* integer format */
						osver=(u_int)atoi(osvername);
					}
					/* set OS version in volume */
					/* Note: akai_rename_file() will correct osver if necessary */
					if (akai_rename_vol(vp,NULL,vp->lnum,osver,NULL)<0){
						fprintf(stderr,"cannot set OS version\n");
					}
				}
				break;
			case CMD_SETLNUM:
			case CMD_SETLNUMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *lnumname;
					u_int lnum;

					if (cmdnr==CMD_SETLNUM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						if (curpartp->type!=PART_TYPE_HD){
							fprintf(stderr,"must be an S1000/S3000 harddisk volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						lnumname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						if (curpartp->type!=PART_TYPE_HD){
							fprintf(stderr,"must be an S1000/S3000 harddisk volume\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						lnumname=cmdtok[2];
					}
					/* load number */
					lnum=akai_get_lnum(lnumname);
					/* set load number in volume */
					if (akai_rename_vol(vp,NULL,lnum,vp->osver,NULL)<0){
						fprintf(stderr,"cannot set load number\n");
					}
				}
				break;
			case CMD_LSPARAM:
			case CMD_LSPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if (cmdnr==CMD_LSPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
					if (vp->param!=NULL){
						printf("\n");
						akai_list_volparam(vp->param);
						printf("\n");
					}else{
						fprintf(stderr,"volume has no parameters\n");
					}
				}
				break;
			case CMD_INITPARAM:
			case CMD_INITPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if (cmdnr==CMD_INITPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
					if (vp->param!=NULL){
						/* default parameters */
						akai_init_volparam(vp->param,(vp->type==AKAI_VOL_TYPE_S3000)||(vp->type==AKAI_VOL_TYPE_CD3000));
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							fprintf(stderr,"cannot update volume\n");
							goto main_parser_next;
						}
					}else{
						fprintf(stderr,"volume has no parameters\n");
					}
				}
				break;
			case CMD_SETPARAM:
			case CMD_SETPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *piname,*pvname;
					u_int pi,pv;

					if (cmdnr==CMD_SETPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						piname=cmdtok[1];
						pvname=cmdtok[2];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						piname=cmdtok[2];
						pvname=cmdtok[3];
					}
					if (vp->param!=NULL){
						/* parameter index */
						pi=(u_int)atoi(piname);
						if (pi>=sizeof(struct akai_volparam_s)){
							fprintf(stderr,"invalid parameter index\n");
							goto main_parser_next;
						}
						/* parameter value */
						pv=(u_int)atoi(pvname);
						if (pv>=0xff){
							fprintf(stderr,"invalid parameter value\n");
							goto main_parser_next;
						}
						/* set parameter */
						vp->param->dummy1[pi]=(u_char)pv; /* XXX */
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							fprintf(stderr,"cannot update volume\n");
							goto main_parser_next;
						}
					}else{
						fprintf(stderr,"volume has no parameters\n");
					}
				}
				break;
			case CMD_GETPARAM:
			case CMD_GETPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					int outfd;
					u_int size;
					char *fname;

					if (cmdnr==CMD_GETPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						fname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						fname=cmdtok[2];
					}
					if (vp->param!=NULL){
						/* create external file */
						if ((outfd=OPEN(fname,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							perror("open");
							goto main_parser_next;
						}
						/* export volparam-file */
						size=sizeof(struct akai_volparam_s);
						if (WRITE(outfd,(void *)vp->param,size)!=(int)size){
							perror("write");
							goto main_parser_next;
						}
						CLOSE(outfd);
					}else{
						fprintf(stderr,"volume has no parameters\n");
					}
				}
				break;
			case CMD_PUTPARAM:
			case CMD_PUTPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					int inpfd;
					struct stat instat;
					u_int size;
					char *fname;

					if (cmdnr==CMD_PUTPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						fname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							fprintf(stderr,"must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							fprintf(stderr,"invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							fprintf(stderr,"volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						fname=cmdtok[2];
					}
					if (vp->param!=NULL){
						/* open external file */
						if ((inpfd=OPEN(fname,O_RDONLY|O_BINARY,0))<0){
							perror("open");
							goto main_parser_next;
						}
						/* get file size */
						if (fstat(inpfd,&instat)<0){
							perror("stat");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						size=(u_int)instat.st_size;
						if (size!=sizeof(struct akai_volparam_s)){ /* invalid file size? */
							fprintf(stderr,"invalid file size of volparam-file\n");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						/* read volparam-file into volume */
						if (READ(inpfd,(void *)vp->param,size)!=(int)size){
							perror("read");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						/* close file */
						CLOSE(inpfd);
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							fprintf(stderr,"cannot update volume\n");
							goto main_parser_next;
						}
						printf("volume parameters imported\n");
					}else{
						fprintf(stderr,"volume has no parameters\n");
					}
				}
				break;
			case CMD_GET:
			case CMD_GETI:
			case CMD_SAMPLE2WAV:
			case CMD_SAMPLE2WAVI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					u_int sfi;
					int outfd;
					u_int begin,end;

					if ((cmdnr==CMD_GET)||(cmdnr==CMD_SAMPLE2WAV)){
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							fprintf(stderr,"invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							fprintf(stderr,"file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}else{
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							fprintf(stderr,"must be inside a volume\n");
							goto main_parser_next;
						}
						/* index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							fprintf(stderr,"invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi-1)<0){
							fprintf(stderr,"file not found\n");
							goto main_parser_next;
						}
					}
					printf("exporting \"%s\"\n",tmpfile.name);
					fflush(NULL);
					if ((cmdnr==CMD_GET)||(cmdnr==CMD_GETI)){
						/* create external file */
						if ((outfd=OPEN(tmpfile.name,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							perror("open");
							goto main_parser_next;
						}
						/* limits */
						if (cmdtoknr>2){
							begin=(u_int)atoi(cmdtok[2]);
						}else{
							begin=0;
						}
						if (cmdtoknr>3){
							end=(u_int)atoi(cmdtok[3]);
						}else{
							end=tmpfile.size;
						}
						/* export file */
						if (akai_read_file(outfd,NULL,&tmpfile,begin,end)<0){
							fprintf(stderr,"export error\n");
						}
						/* close file */
						CLOSE(outfd);
					}else{
						/* export file */
						if (akai_sample2wav(&tmpfile,-1,NULL,NULL,SAMPLE2WAV_ALL)<0){
							fprintf(stderr,"export error\n");
						}
					}
				}
				break;
			case CMD_SAMPLE2WAVALL:
				{
					struct file_s tmpfile;
					u_int sfi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"must be inside a volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (sfi=0;sfi<curvolp->fimax;sfi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi)<0){
							continue; /* next */
						}
						/* export file */
						printf("exporting \"%s\"\n",tmpfile.name);
						fflush(NULL);
						if (akai_sample2wav(&tmpfile,-1,NULL,NULL,SAMPLE2WAV_ALL)!=0){
							continue; /* next */
						}
						fcount++;
					}
					printf("exported %u file(s)\n",fcount);
				}
				break;
			case CMD_PUT:
				{
					struct file_s tmpfile;
					int inpfd;
					struct stat instat;
					u_int size;
					u_int dfi;

					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){
						fprintf(stderr,"invalid file name\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdtoknr>2){
						/* index */
						dfi=(u_int)atoi(cmdtok[2]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							fprintf(stderr,"invalid file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* XXX akai_create_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
#if 1
						/* check if new file name already used */
						if (akai_find_file(curvolp,&tmpfile,dirnamebuf)==0){
							fprintf(stderr,"file name already used\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif
					}
					/* open external file */
					if ((inpfd=OPEN(dirnamebuf,O_RDONLY|O_BINARY,0))<0){
						u_int k;
						/* try again with fixed name: ' ' instead of '_' for keyboard entry */
						for (k=0;k<strlen(dirnamebuf);k++){
							if (dirnamebuf[k]=='_'){
								dirnamebuf[k]=' ';
							}
						}
						if ((inpfd=OPEN(dirnamebuf,O_RDONLY|O_BINARY,0))<0){
							perror("open");
							restore_curdir();
							goto main_parser_next;
						}
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						perror("stat");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					size=(u_int)instat.st_size;
					/* create file */
					/* Note: akai_create_file() will correct osver if necessary */
					if (akai_create_file(curvolp,&tmpfile,size,
										 dfi,
										 dirnamebuf,
										 (curvolp->type==AKAI_VOL_TYPE_S900)?0:curvolp->osver, /* default: from volume */
										 NULL)<0){
						fprintf(stderr,"cannot create file\n");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					/* import file */
					printf("importing \"%s\"\n",tmpfile.name);
					fflush(NULL);
					if (akai_write_file(inpfd,NULL,&tmpfile,0,tmpfile.size)<0){
						fprintf(stderr,"import error\n");
#if 0
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
#endif /* else: XXX continue */
					}
					/* close file */
					CLOSE(inpfd);
					if ((curvolp->type==AKAI_VOL_TYPE_S900)&&(tmpfile.osver!=0)){ /* compressed file in S900 volume? */
						/* update uncompr value */
						akai_s900comprfile_updateuncompr(&tmpfile); /* ignore error */
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&tmpfile); /* ignore error */
					restore_curdir();
				}
				break;
			case CMD_WAV2SAMPLE:
			case CMD_WAV2SAMPLE9:
			case CMD_WAV2SAMPLE9C:
			case CMD_WAV2SAMPLE1:
			case CMD_WAV2SAMPLE3:
				{
					u_int dfi;
					u_int type;

					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						fprintf(stderr,"directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						fprintf(stderr,"destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){
						fprintf(stderr,"invalid file name\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdtoknr>2){
						/* index */
						dfi=(u_int)atoi(cmdtok[2]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							fprintf(stderr,"invalid file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* XXX akai_create_file() in akai_wav2sample() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
					}
					/* sample file type */
					if ((cmdnr==CMD_WAV2SAMPLE9)||(cmdnr==CMD_WAV2SAMPLE9C)){
						type=AKAI_SAMPLE900_FTYPE;
					}else if (cmdnr==CMD_WAV2SAMPLE1){
						type=AKAI_SAMPLE1000_FTYPE;
					}else if (cmdnr==CMD_WAV2SAMPLE3){
						type=AKAI_SAMPLE3000_FTYPE;
					}else{
						type=AKAI_FTYPE_FREE; /* invalid: derive sample file type from volume type/file osver */
					}
					/* read and parse WAV header, create sample header, import samples */
					printf("importing \"%s\"\n",dirnamebuf);
					fflush(NULL);
					/* Note: akai_wav2sample() will correct osver if necessary */
					if (akai_wav2sample(-1,dirnamebuf,
										curvolp,
										dfi,
										type,
										(cmdnr==CMD_WAV2SAMPLE9C),
										curvolp->osver, /* default: osver from volume */
										NULL, /* no tags */
										NULL,
										WAV2SAMPLE_OPEN)!=0){
						fprintf(stderr,"WAV import error\n");
						restore_curdir();
						goto main_parser_next;
					}
					restore_curdir();
				}
				break;
			case CMD_TGETI:
				{
					u_int ti;
					int outfd;
					u_int cstarts,cstarte;
					u_int csizes,csizee;
					struct akai_ddtake_s t;
					char fnamebuf[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* take */
					cstarts=(curpartp->head.dd.take[ti-1].cstarts[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarts[0];
					if (cstarts==0){
						fprintf(stderr,"take not found\n");
						goto main_parser_next;
					}
					cstarte=(curpartp->head.dd.take[ti-1].cstarte[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarte[0];
					/* create external file */
					akai2ascii_name(curpartp->head.dd.take[ti-1].name,fnamebuf,0); /* 0: not S900 */
					strcpy(fnamebuf+strlen(fnamebuf),AKAI_DDTAKE_FNAMEEND);
					if ((outfd=OPEN(fnamebuf,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						perror("open");
						goto main_parser_next;
					}
					/* get DD take header from directory entry */
					bcopy(&curpartp->head.dd.take[ti-1],&t,sizeof(struct akai_ddtake_s));
					/* determine csizes (sample clusters) and csizee (envelope clusters) */
					csizes=akai_count_ddfatchain(curpartp,cstarts);
					csizee=akai_count_ddfatchain(curpartp,cstarte);
					/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
					t.cstarts[1]=0xff&(csizes>>8);
					t.cstarts[0]=0xff&csizes;
					t.cstarte[1]=0xff&(csizee>>8);
					t.cstarte[0]=0xff&csizee;
					/* export take */
					printf("exporting take %u\n",ti);
					fflush(NULL);
					if (akai_export_take(outfd,curpartp,&t,csizes,csizee,cstarts,cstarte)<0){
						fprintf(stderr,"cannot export take\n");
					}
					/* close file */
					CLOSE(outfd);
				}
				break;
			case CMD_TAKE2WAVI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						fprintf(stderr,"invalid take index\n");
						goto main_parser_next;
					}
					/* export take to WAV */
					printf("exporting take %u\n",ti);
					fflush(NULL);
					if (akai_take2wav(curpartp,ti-1,-1,NULL,NULL,TAKE2WAV_ALL)<0){
						fprintf(stderr,"cannot export take\n");
					}
				}
				break;
			case CMD_TAKE2WAVALL:
				{
					u_int ti;
					u_int cstarts;
					u_int tcount;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* all DD takes in current DD partition */
					tcount=0;
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						cstarts=(curpartp->head.dd.take[ti].cstarts[1]<<8)
							    +curpartp->head.dd.take[ti].cstarts[0];
						if (cstarts==0){ /* empty? */
							continue; /* next */
						}
						/* export take to WAV */
						printf("exporting take %u\n",ti+1);
						fflush(NULL);
						if (akai_take2wav(curpartp,ti,-1,NULL,NULL,TAKE2WAV_ALL)<0){
							continue; /* next */
						}
						tcount++;
					}
					printf("exported %u take(s)\n",tcount);
				}
				break;
			case CMD_TPUT:
				{
					u_int ti;
					int inpfd;
					u_int cstarts;
					u_int csizes,csizee;
					struct akai_ddtake_s t;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* find free index */
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						/* take */
						cstarts=(curpartp->head.dd.take[ti].cstarts[1]<<8)
							    +curpartp->head.dd.take[ti].cstarts[0];
						if (cstarts==0){ /* free? */
							break; /* done */
						}
					}
					if (ti==AKAI_DDTAKE_MAXNUM){ /* no free index? */
						fprintf(stderr,"no free take index\n");
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=OPEN(cmdtok[1],O_RDONLY|O_BINARY,0))<0){
						perror("open");
						goto main_parser_next;
					}
					/* get DD take header */
					if (READ(inpfd,&t,sizeof(struct akai_ddtake_s))!=(int)sizeof(struct akai_ddtake_s)){
						perror("read");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					/* get csizes (sample clusters) and csizee (envelope clusters) */
					/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
					csizes=(t.cstarts[1]<<8)
						   +t.cstarts[0];
					csizee=(t.cstarte[1]<<8)
						   +t.cstarte[0];
					/* import take */
					printf("importing to take %u\n",ti+1);
					fflush(NULL);
					/* Note: akai_import_take() keeps name in DD take header */
					/* Note: don't check if name already used, create new DD take */
					if (akai_import_take(inpfd,curpartp,&t,ti,csizes,csizee)<0){
						fprintf(stderr,"cannot import take\n");
					}
					/* close file */
					CLOSE(inpfd);
				}
				break;
			case CMD_WAV2TAKE:
				{
					u_int ti;
					u_int cstarts;

					if (check_curnoddpart()){ /* not on DD partition level? */
						fprintf(stderr,"must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* find free index */
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						/* take */
						cstarts=(curpartp->head.dd.take[ti].cstarts[1]<<8)
							    +curpartp->head.dd.take[ti].cstarts[0];
						if (cstarts==0){ /* free? */
							break; /* done */
						}
					}
					if (ti==AKAI_DDTAKE_MAXNUM){ /* no free index? */
						fprintf(stderr,"no free take index\n");
						goto main_parser_next;
					}
					/* read and parse WAV header, create take, import samples */
					printf("importing to take %u\n",ti+1);
					fflush(NULL);
					/* Note: don't check if name already used, create new DD take */
					if (akai_wav2take(-1,cmdtok[1],
									  curpartp,
									  ti,
									  NULL,
									  WAV2TAKE_OPEN)!=0){
						fprintf(stderr,"WAV import error\n");
						goto main_parser_next;
					}
				}
				break;
			case CMD_TARC:
			case CMD_TARCWAV:
				{
					int outfd;
					u_int flags;

					/* create tar-file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						perror("open");
						goto main_parser_next;
					}
					/* flags */
					if (cmdnr==CMD_TARCWAV){
						flags=TAR_EXPORT_WAV;
					}else{
						flags=0;
					}
					/* export tar-file */
					if (tar_export_curdir(outfd,1,flags)<0){ /* 1: verbose */
						fprintf(stderr,"tar error\n");
					}
					if (tar_export_tailzero(outfd)<0){
						fprintf(stderr,"tar error\n");
					}
					CLOSE(outfd);
				}
				break;
			case CMD_TARX:
			case CMD_TARX9:
			case CMD_TARX1:
			case CMD_TARX3:
			case CMD_TARX3CD:
			case CMD_TARXWAV:
			case CMD_TARXWAV9:
			case CMD_TARXWAV9C:
			case CMD_TARXWAV1:
			case CMD_TARXWAV3:
				{
					int inpfd;
					u_int vtype;
					u_int flags;

					/* open tar-file */
					if ((inpfd=OPEN(cmdtok[1],O_RDONLY|O_BINARY,0))<0){
						perror("open");
						goto main_parser_next;
					}
					/* volume type */
					if (cmdnr==CMD_TARX9){
						vtype=AKAI_VOL_TYPE_S900; /* force to S900 */
					}else if (cmdnr==CMD_TARX1){
						vtype=AKAI_VOL_TYPE_S1000; /* force to S1000 */
					}else if (cmdnr==CMD_TARX3){
						vtype=AKAI_VOL_TYPE_S3000; /* force to S3000 or CD3000 */
					}else if (cmdnr==CMD_TARX3CD){
						vtype=AKAI_VOL_TYPE_CD3000; /* force to CD3000 CD-ROM */
					}else{
						vtype=AKAI_VOL_TYPE_INACT; /* INACT: auto-detect */
					}
					/* flags */
					if (cmdnr==CMD_TARXWAV){
						flags=TAR_IMPORT_WAV;
					}else if (cmdnr==CMD_TARXWAV9){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS9;
					}else if (cmdnr==CMD_TARXWAV9C){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS9C;
					}else if (cmdnr==CMD_TARXWAV1){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS1;
					}else if (cmdnr==CMD_TARXWAV3){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS3;
					}else{
						flags=0;
					}
					/* import tar-file */
					if (tar_import_curdir(inpfd,vtype,1,flags)<0){ /* 1: verbose */
						fprintf(stderr,"tar error\n");
					}
					CLOSE(inpfd);
				}
				break;
			case CMD_MKVOL:
			case CMD_MKVOL9:
			case CMD_MKVOL1:
			case CMD_MKVOL3:
			case CMD_MKVOL3CD:
				{
					struct vol_s tmpvol;
					u_int lnum;
					u_int vtype;

					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>1){
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							fprintf(stderr,"directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if ((strlen(dirnamebuf)==0)
							||(strcmp(dirnamebuf,".")==0)||(strcmp(dirnamebuf,"..")==0)){
								fprintf(stderr,"invalid volume name\n");
								restore_curdir();
								goto main_parser_next;
						}
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"must be on sampler partition level\n");
						restore_curdir();
						goto main_parser_next;
					}
#if 1
					if (cmdtoknr>1){
						/* check if new volume name already used */
						if (akai_find_vol(curpartp,&tmpvol,dirnamebuf)==0){
							fprintf(stderr,"volume name already used\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
#endif
					/* load number */
					if (cmdtoknr>2){
						lnum=akai_get_lnum(cmdtok[2]);
					}else{
						lnum=AKAI_VOL_LNUM_OFF;
					}
					/* volume type */
					if (cmdnr==CMD_MKVOL){
						/* derive volume type from partition type */
						if (curpartp->type==PART_TYPE_HD9){
							vtype=AKAI_VOL_TYPE_S900;
						}else{
							if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
								vtype=AKAI_VOL_TYPE_S3000;
							}else{
								/* default */
								vtype=AKAI_VOL_TYPE_S1000;
							}
						}
					}else if (cmdnr==CMD_MKVOL9){
						vtype=AKAI_VOL_TYPE_S900;
					}else if (cmdnr==CMD_MKVOL3){
						vtype=AKAI_VOL_TYPE_S3000;
					}else if (cmdnr==CMD_MKVOL3CD){
						vtype=AKAI_VOL_TYPE_CD3000;
					}else{
						vtype=AKAI_VOL_TYPE_S1000;
					}
					/* create volume */
					if (akai_create_vol(curpartp,&tmpvol,
										vtype,
										AKAI_CREATE_VOL_NOINDEX,
										(cmdtoknr>1)?dirnamebuf:NULL,
										lnum,
										NULL)<0){
						fprintf(stderr,"cannot create volume\n");
					}
					restore_curdir();
				}
				break;
			case CMD_MKVOLI9:
			case CMD_MKVOLI1:
			case CMD_MKVOLI3:
			case CMD_MKVOLI3CD:
				{
					struct vol_s tmpvol;
					u_int vi;
					u_int lnum;
					u_int vtype;

					if ((cmdtoknr>2)
						&&((strcmp(cmdtok[2],".")==0)||(strcmp(cmdtok[2],"..")==0))){
						fprintf(stderr,"invalid volume name\n");
						goto main_parser_next;
					}

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						fprintf(stderr,"must be on sampler partition level\n");
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						fprintf(stderr,"invalid volume index\n");
						goto main_parser_next;
					}
					/* load number */
					if (cmdtoknr>3){
						lnum=akai_get_lnum(cmdtok[3]);
					}else{
						lnum=AKAI_VOL_LNUM_OFF;
					}
					/* volume type */
					if (cmdnr==CMD_MKVOLI){
						/* derive volume type from partition type */
						if (curpartp->type==PART_TYPE_HD9){
							vtype=AKAI_VOL_TYPE_S900;
						}else{
							if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
								vtype=AKAI_VOL_TYPE_S3000;
							}else{
								/* default */
								vtype=AKAI_VOL_TYPE_S1000;
							}
						}
					}else if (cmdnr==CMD_MKVOLI9){
						vtype=AKAI_VOL_TYPE_S900;
					}else if (cmdnr==CMD_MKVOLI3){
						vtype=AKAI_VOL_TYPE_S3000;
					}else if (cmdnr==CMD_MKVOLI3CD){
						vtype=AKAI_VOL_TYPE_CD3000;
					}else{
						vtype=AKAI_VOL_TYPE_S1000;
					}
					/* create volume */
					if (akai_create_vol(curpartp,&tmpvol,
										vtype,
										vi-1,
										(cmdtoknr>2)?cmdtok[2]:NULL,
										lnum,
										NULL)<0){
						fprintf(stderr,"cannot create volume\n");
					}
				}
				break;
			case CMD_DIRCACHE:
				printf("\n");
				print_blk_cache();
				printf("\n");
				break;
			default:
				printf("unknown command, try \"help\"\n\n");
				break;
			}
main_parser_next:
			fflush(NULL);
#if 1 /* XXX flush cache every now and then */
			flush_blk_cache(); /* XXX if error, maybe next time more luck */
#endif
			printf("\n");
#ifdef DEBUG
			print_blk_cache();
#endif
		} /* main command loop */
	} /* command interpreter */

	flush_blk_cache(); /* XXX if error, too late */
	exit(0);
}



/* EOF */
