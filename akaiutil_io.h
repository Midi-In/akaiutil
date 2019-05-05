#ifndef __AKAIUTIL_IO_H
#define __AKAIUTIL_IO_H
/*
* Copyright (C) 2008,2010,2012 Klaus Michael Indlekofer. All rights reserved.
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



#ifdef WIN32
/* e.g. VC++ */

#include "winlib_akaiutil.h"

#else /* !WIN32 */
/* e.g. UNIX-like systems or cygwin */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#ifndef INT64
#define INT64 __int64_t
#endif
#ifndef U_INT64
#define U_INT64 __uint64_t
#endif

#endif /* !WIN32 */



/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */
#ifndef OPEN
#define OPEN open
#endif
#ifndef CLOSE
#define CLOSE close
#endif
#ifndef READ
#define READ read
#endif
#ifndef WRITE
#define WRITE write
#endif
#ifndef LSEEK
#define LSEEK lseek
#endif
#ifndef OFF_T
#define OFF_T off_t
#endif



/* cache */

struct blk_cache_s{
	int valid;
	int modified;
	int fd;
	u_int blk;
	u_int blksize;
#define BLK_CACHE_AGE_MAX		0xffffffff /* XXX max. age */
	u_int age; /* age */
	u_char *buf;
};

#define BLK_CACHE_NUM	256 /* XXX */
extern struct blk_cache_s blk_cache[BLK_CACHE_NUM];



#define IO_BLKS_READ	0
#define IO_BLKS_WRITE	1



/* Declarations */

extern int init_blk_cache(void);
extern void print_blk_cache(void);
extern int find_blk_cache(int fd,u_int blk,u_int blksize);
extern void blk_cache_aging(int i);
extern int io_blks_direct(int fd,u_char *buf,u_int bstart,u_int bsize,u_int blksize,int cachealloc,int mode);
extern int flush_blk_cache(void);
extern int io_blks(int fd,u_char *buf,u_int bstart,u_int bsize,u_int blksize,int cachealloc,int mode);



#endif /* !__AKAIUTIL_IO_H */
