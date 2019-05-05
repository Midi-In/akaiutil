/*
* Copyright (C) 2008,2019 Klaus Michael Indlekofer. All rights reserved.
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



#include "akaiutil_io.h"



/* cache */

struct blk_cache_s blk_cache[BLK_CACHE_NUM];



int
init_blk_cache(void)
{
	int i;

	for (i=0;i<BLK_CACHE_NUM;i++){
		blk_cache[i].valid=0; /* free entry */
		blk_cache[i].buf=NULL; /* not allocated yet */
	}

	return 0;
}



void
print_blk_cache(void)
{
	int i;

	printf("nr    fd   blksize blk     age         mod\n");
	printf("------------------------------------------\n");
	for (i=0;i<BLK_CACHE_NUM;i++){
		if (!blk_cache[i].valid){ /* free entry? */
			continue; /* next */
		}
		printf("%4i  %3i  0x%04x  0x%04x  0x%08x  %i\n",
			i,
			blk_cache[i].fd,
			blk_cache[i].blksize,
			blk_cache[i].blk,
			blk_cache[i].age,
			blk_cache[i].modified);
	}
	printf("------------------------------------------\n");
}



int
find_blk_cache(int fd,u_int blk,u_int blksize)
{
	int i;

	/* scan cache */
	for (i=0;i<BLK_CACHE_NUM;i++){
		if (blk_cache[i].buf==NULL){
			continue; /* next */
		}
		if (!blk_cache[i].valid){ /* free? */
			continue; /* next */
		}
		/* check if contained within cache */
		/* Note: must check for blksize as well!!! */
		if ((fd==blk_cache[i].fd)&&(blk==blk_cache[i].blk)&&(blksize==blk_cache[i].blksize)){
			break; /* found it */
		}
	}

	if (i==BLK_CACHE_NUM){ /* none found? */
		return -1;
	}

	return i;
}



void
blk_cache_aging(int i)
{
	int j;
	u_int agemin;

	if ((i<0)||(i>=BLK_CACHE_NUM)||(!blk_cache[i].valid)){
		return;
	}

	blk_cache[i].age=0; /* reset age */

	/* find the youngest of the others */
	agemin=BLK_CACHE_AGE_MAX;
	for (j=0;j<BLK_CACHE_NUM;j++){
		if (!blk_cache[j].valid){ /* free? */
			continue; /* next */
		}
		if (j==i){
			continue; /* next */
		}
		if (blk_cache[j].age<agemin){
			agemin=blk_cache[j].age;
		}
	}
	if (agemin==0){ /* somebody else too young? */
		/* increment age of the others */
		for (j=0;j<BLK_CACHE_NUM;j++){
			if (j==i){
				continue; /* next */
			}
			if (blk_cache[j].age<BLK_CACHE_AGE_MAX){
				blk_cache[j].age++; /* increase age */
			}
		}
	}
}



int
io_blks_direct(int fd,u_char *buf,u_int bstart,u_int bsize,u_int blksize,int cachealloc,int mode)
{
	int err;
	int j,jmax;
	u_int agemax;
	u_int blk,blkmin,blkmax,blkchunk;

	if (buf==NULL){
		return -1;
	}
	if (fd<0){
		return -1;
	}

	if ((mode!=IO_BLKS_READ)&&(mode!=IO_BLKS_WRITE)){
		return -1;
	}

	/* Note: OFF_T against overflow */
	if ((((OFF_T)bstart)+((OFF_T)bsize))>(OFF_T)0xffffffff){ /* XXX */
		return -1;
	}

	if (bsize==0){
		return 0; /* done */
	}

	if (cachealloc){
		/* must allocate cache */
		blkmin=bstart;
		blkmax=bstart+bsize;
		blkchunk=1;
	}else{
		/* no need to allocate cache, all in one */
		blkmin=bstart;
		blkmax=bstart+1;
		blkchunk=bsize;
	}

	for (blk=blkmin;blk<blkmax;blk++){
		/* goto blk */
		if (LSEEK(fd,(OFF_T)(blk*blksize),SEEK_SET)<0){
			perror("lseek");
			return -1;
		}
		if (mode==IO_BLKS_WRITE){
			/* write block(s) */
			err=WRITE(fd,(void *)(buf+(blk-blkmin)*blksize),blkchunk*blksize);
			if (err<0){
				perror("write");
				return -1;
			}
			if (err!=(int)(blkchunk*blksize)){
				fprintf(stderr,"write: incomplete\n");
				return -1;
			}
		}else{
			/* read block(s) */
			err=READ(fd,(void *)(buf+(blk-blkmin)*blksize),blkchunk*blksize);
			if (err<0){
				perror("read");
				return -1;
			}
			if (err!=(int)(blkchunk*blksize)){
				fprintf(stderr,"read: incomplete\n");
				return -1;
			}
		}
		if (cachealloc){ /* must allocate cache? */
			/* find free entry or the oldest one */
			agemax=0;
			jmax=0; /* first guess */
			for (j=0;j<BLK_CACHE_NUM;j++){
				if (!blk_cache[j].valid){ /* free? */
					jmax=j;
					break; /* found one */
				}
				if (blk_cache[j].age>=agemax){ /* older or equal? */
					agemax=blk_cache[j].age;
					jmax=j;
				}
			}
			/* old cache entry not free and modified? */
			err=0;
			if ((blk_cache[jmax].valid)&&(blk_cache[jmax].modified)&&(blk_cache[jmax].buf!=NULL)){
				/* must flush this block */
				if (io_blks_direct(blk_cache[jmax].fd,
									(u_char *)blk_cache[jmax].buf,
									blk_cache[jmax].blk,
									1,
									blk_cache[jmax].blksize,
									0, /* don't alloc cache */
									IO_BLKS_WRITE)<0){
					fprintf(stderr,"cannot flush cache block 0x%08x of fd %i\n",blk_cache[jmax].blk,blk_cache[jmax].fd);
					err=1; /* cannot allocate below */
				}
			}
			if (!err){
				if ((blk_cache[jmax].buf!=NULL)&&(blk_cache[jmax].blksize!=blksize)){ /* not compatible? */
					/* free buffer */
					free(blk_cache[jmax].buf);
					blk_cache[jmax].buf=NULL;
				}
				if (blk_cache[jmax].buf==NULL){ /* buffer not allocated yet/anymore? */
					/* need to allocate buffer */
					if ((blk_cache[jmax].buf=(u_char *)malloc(blksize))==NULL){
						perror("malloc");
						err=1; /* cannot allocate below */
					}
					blk_cache[jmax].valid=0; /* free */
					blk_cache[jmax].blksize=blksize;
				}
			}
			if (!err){
				/* allocate cache entry */
				blk_cache[jmax].valid=1;
				blk_cache[jmax].modified=0;
				blk_cache[jmax].fd=fd;
				blk_cache[jmax].blk=blk;
				blk_cache_aging(jmax); /* adjust ages */
				/* copy data */
				bcopy(buf+(blk-blkmin)*blksize,blk_cache[jmax].buf,blksize);
			}
		}
	}

	return 0;
}



/* Note: prior to changing blksize of any device/file, must flush cache first !!! */
int
flush_blk_cache(void)
{
	int ret;
	int i;

	/* scan cache */
	ret=0; /* no error so far */
	for (i=0;i<BLK_CACHE_NUM;i++){
		if (blk_cache[i].buf==NULL){
			continue; /* next */
		}
		if (!blk_cache[i].valid){ /* free? */
			continue; /* next */
		}
		if (!blk_cache[i].modified){ /* not modified? */
			continue; /* next */
		}
		/* must write */
		if (io_blks_direct(blk_cache[i].fd,
							(u_char *)blk_cache[i].buf,
							blk_cache[i].blk,
							1,
							blk_cache[i].blksize,
							0, /* don't alloc cache */
							IO_BLKS_WRITE)<0){
			fprintf(stderr,"cannot flush cache block 0x%08x of fd %i\n",blk_cache[i].blk,blk_cache[i].fd);
			/* XXX cannot do more now, maybe more luck next time */
			ret=-1;
		}else{
			blk_cache[i].modified=0; /* done */
		}
	}

	return ret;
}



int
io_blks(int fd,u_char *buf,u_int bstart,u_int bsize,u_int blksize,int cachealloc,int mode)
{
	int i;
	u_int blk;
	u_int chunkstart,chunksize;

	if (buf==NULL){
		return -1;
	}

	if ((mode!=IO_BLKS_READ)&&(mode!=IO_BLKS_WRITE)){
		return -1;
	}

	/* Note: OFF_T against overflow */
	if ((((OFF_T)bstart)+((OFF_T)bsize))>(OFF_T)0xffffffff){ /* XXX */
		return -1;
	}
	if (fd<0){
		return -1;
	}

	/* check every block */
	chunkstart=bstart;
	chunksize=0; /* no chunk of missing blocks so far */
	for (blk=bstart;blk<(bstart+bsize);blk++){
		/* look in cache */
		i=find_blk_cache(fd,blk,blksize);
		/* Note: no match for blk if blksize has changed!!! */
		/*       however, these cache entries can be reused if needed */
		if (i<0){ /* not found in cache? */
			if (chunksize==0){ /* new chunk of missing blocks? */
				chunkstart=blk; /* start of chunk */
			}
			chunksize++; /* one more missing block */
		}else{
			/* found in cache */
			/* Note: now, blk_cache[i].buf!=NULL */

			/* Note: must do cache-I/O first, since io_blks_direct could steal i from cache */
			if (mode==IO_BLKS_WRITE){
				/* copy to cache */
				bcopy(buf+(blk-bstart)*blksize,blk_cache[i].buf,blksize);
				blk_cache[i].modified=1; /* modified */
			}else{
				/* copy from cache */
				bcopy(blk_cache[i].buf,buf+(blk-bstart)*blksize,blksize);
			}
			blk_cache_aging(i); /* adjust ages */

			/* chunk of missing blocks pending? */
			if (chunksize>0){
				/* must read or write */
				if (io_blks_direct(fd,
									(u_char *)(buf+(chunkstart-bstart)*blksize),
									chunkstart,
									chunksize,
									blksize,
									cachealloc,
									mode)<0){
					return -1;
				}
				chunksize=0; /* no chunk of missing blocks anymore */
			}
		}
	}
	/* one last time: chunk of missing blocks pending? */
	if (chunksize>0){
		/* must read or write */
		if (io_blks_direct(fd,
							(u_char *)(buf+(chunkstart-bstart)*blksize),
							chunkstart,
							chunksize,
							blksize,
							cachealloc,
							mode)<0){
			return -1;
		}
		chunksize=0; /* no chunk of missing blocks anymore */
	}

	return 0;
}



/* EOF */
