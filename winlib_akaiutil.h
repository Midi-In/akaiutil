#ifndef __WINLIB_AKAIUTIL_H
#define __WINLIB_AKAIUTIL_H
/*
* unless noted otherwise: Copyright (C) 2003-2010,2012 Klaus Michael Indlekofer. All rights reserved.
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



#define ADDITIONAL_COPYRIGHT_TEXT \
	"This product includes getopt,strncasecmp developed by\n"\
	"the University of California, Berkeley and its contributors.\n"\
	"\n"\
	"Copyright (c) 1987-2002 The Regents of the University of California.\n"\
	"All rights reserved.\n"\
"\n"



#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
/*#include <unistd.h>*/
/*#include <strings.h>*/
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

typedef unsigned char u_char;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef int ssize_t;
#ifndef SSIZE_MAX
#define SSIZE_MAX INT_MAX
#endif /* !SSIZE_MAX */

#define INT64 __int64
#define U_INT64 unsigned __int64

#define OPEN _open
#define CLOSE _close
#define READ _read
#define WRITE _write
/* default lseek and off_t cannot handle offsets>=2GB! */
#define LSEEK _lseeki64
#define OFF_T __int64

extern void bcopy(const void *src,void *dst,size_t len);
extern void bzero(void *b,size_t len);

extern int strcasecmp(const char *s1,const char *s2);
extern int strncasecmp(const char *s1,const char *s2,size_t n);

/* getopt */
/*#include <unistd.h>*/
extern int getopt(int argc,char * const argv[],const char *optstring);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
#define NEW_GETOPT

/* local directory/device */
#include <direct.h>
#define LCHDIR(d) _chdir(d)
#define LDIR system("dir");



#endif /* !__WINLIB_AKAIUTIL_H */
