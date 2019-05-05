/*
* Copyright (C) 2008,2010,2012,2018,2019 Klaus Michael Indlekofer. All rights reserved.
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
#include "akaiutil.h"
#include "akaiutil_file.h"
#include "akaiutil_take.h"



/* disks */
struct disk_s disk[DISK_NUM_MAX]; /* disks, Note: one for each disk, system-wide */
u_int disk_num; /* number of disks */

/* partitions */
struct part_s part[PART_NUM_MAX]; /* partitions, Note: one for each partition, system-wide */
u_int part_num; /* number of partitions */

/* current directory */
struct disk_s *curdiskp; /* current disk pointer into disk[], NULL if system-level */
						 /* Note: unique, fixed mapping to each file */
struct part_s *curpartp; /* current partition pointer into part[], NULL if disk-level */
						 /* Note: unique, fixed mapping to each partition */
struct vol_s *curvolp; /* current volume pointer into curvol_buf, NULL if disk- or partition-level */
					   /* Note: neither fixed nor unique mapping to each volume!!! */
struct vol_s curvol_buf; /* current volume buffer, Note: only one buffer, might change volume identity!!! */

/* for operations with current directory, Note: one instance only, not recursive!!! */
struct disk_s *savecurdiskp;
struct part_s *savecurpartp;
struct vol_s *savecurvolp;
struct vol_s savecurvol_buf;
int savecurvolmodflag; /* modifier flag */
char savecurvolname[AKAI_NAME_LEN+1]; /* +1 for '\0' */
u_int savecurvolindex;

char dirnamebuf[DIRNAMEBUF_LEN+1]; /* +1 for '\0' */

/* filter tags */
u_char curfiltertag[AKAI_FILE_TAGNUM];



int
open_disk(char *name,int readonly)
{

	if (name==NULL){
		return -1;
	}
	if (disk_num>=DISK_NUM_MAX){
		return -1;
	}

	disk[disk_num].index=disk_num; /* index */
	disk[disk_num].type=DISK_TYPE_HD; /* XXX default type */
	disk[disk_num].blksize=AKAI_HD_BLOCKSIZE; /* XXX default blocksize */
	disk[disk_num].totsize=AKAI_HD_MAXSIZE; /* XXX max. size */
	disk[disk_num].readonly=readonly;
	if (readonly){
		disk[disk_num].fd=OPEN(name,O_RDONLY|O_BINARY,0,0);
	}else{
		disk[disk_num].fd=OPEN(name,O_RDWR|O_BINARY,0666,0);
	}
	if (disk[disk_num].fd<0){
		fprintf(stderr,"disk%u: error\n",disk_num);
		perror("open");
		/* discard disk, keep old disk_num */
		return -1;
	}

	disk_num++; /* found one */

	return 0;
}



u_int
akai_disksize(struct disk_s *dp)
{
	u_int offmax,off,siz;
	static u_char bbuf[AKAI_HD_BLOCKSIZE];

	if ((dp==NULL)||(dp->blksize==0)||(dp->blksize>AKAI_HD_BLOCKSIZE)){
		return 0;
	}

	/* >=max. disk size, must be power of 2 */
	offmax=0x10000; /* XXX */
#if 1
	if (offmax<AKAI_HD_MAXSIZE){
		/* XXX should never happen */
		fprintf(stderr,"akai_disksize: internal error\n");
		return 0;
	}
#endif
	off=offmax;
	siz=off;
	for (;;){
		siz>>=1;
		/* can read 1 block at off ? */
		if ((off>=offmax) /* beyond max. disk limit? */
			||(LSEEK(dp->fd,(OFF_T)(off*dp->blksize),SEEK_SET)<0)
			||(READ(dp->fd,bbuf,dp->blksize)!=(int)dp->blksize)){
			/* block at off is not readable -> off is too high */
#if 0
			fprintf(stderr,"akai_disksize: off=%08x siz=%08x high\n",off,siz);
#endif
			if (siz==0){
				if (off==0){ /* even block at 0 is not readable? */
					/* no blocks are readable */
					return 0; /* exit */
				}
				off--; /* block at off is not readable */
				/* Note: must check if block at new off is readable */
			}else{
				off-=siz;
			}
		}else{
			/* block at off is readable -> off is OK or too low */
#if 0
			fprintf(stderr,"akai_disksize: off=%08x siz=%08x low\n",off,siz);
#endif
			if (siz==0){
				break; /* end of iteration */
			}
			off+=siz;
		}
	}
	/* now, off is last readable disk block */

	/* return value in blocksize */
	if (off<AKAI_HD_MAXSIZE){
		off++; /* !!! */
	}else{
		off=AKAI_HD_MAXSIZE;
	}

#if 0
	fprintf(stderr,"akai_disksize done: size=%08x (%u)\n",off,off*dp->blksize);
#endif
	return off;
}



int
akai_io_blks(struct part_s *pp,u_char *buf,u_int bstart,u_int bsize,int cachealloc,int mode)
{

	if ((pp==NULL)||(pp->diskp==NULL)||(pp->diskp->fd<0)){
		return -1;
	}

	if ((pp->diskp->readonly)&&(mode==IO_BLKS_WRITE)){
		fprintf(stderr,"disk%u: read-only, cannot write\n",pp->diskp->index);
		return -1;
	}

	if (((bstart+bsize)>pp->bsize)
		||((pp->bstart+bstart+bsize)>pp->diskp->totsize)){
		fprintf(stderr,"disk%u: cannot %s outside partition/disk\n",
			pp->diskp->index,
			(mode==IO_BLKS_WRITE)?"write":"read");
		return -1;
	}

	return io_blks(pp->fd,buf,pp->bstart+bstart,bsize,pp->blksize,cachealloc,mode);
}



void
akai_countfree_part(struct part_s *pp)
{
	u_int i;
	u_int n;

	/* Note: allow invalid partitions!!! */
	if ((pp==NULL)||(pp->fd<0)||(pp->fat==NULL)){
		return;
	}

	pp->bfree=0;

	if (pp->type==PART_TYPE_DD){
		/* S1100/S3000 harddisk DD partition */
		/* Note: start at cluster 1 in order to skip reserved system cluster 0 which contains partition header */
		for (i=1;i<pp->csize;i++){
			n=(pp->fat[i][1]<<8)+pp->fat[i][0];
			if (n==AKAI_DDFAT_CODE_FREE){
				pp->bfree+=AKAI_DDPART_CBLKS; /* +1 cluster */
			}
		}
		return;
	}

	/* Note: start at block pp->bsyssize in order to skip reserved system blocks */
	/*       => also avoids problem due to AKAI_FAT_CODE_SYS900FL==AKAI_FAT_CODE_FREE */
	for (i=pp->bsyssize;i<pp->bsize;i++){
		n=(pp->fat[i][1]<<8)+pp->fat[i][0];
		if (n==AKAI_FAT_CODE_FREE){
			pp->bfree++;
		}
	}
}



int
akai_check_fatblk(u_int blk,u_int bsize,u_int bsyssize)
{

	if (  (blk==AKAI_FAT_CODE_FREE)
		||(blk==AKAI_FAT_CODE_SYS900FL) /* XXX same as AKAI_FAT_CODE_FREE */
		||(blk==AKAI_FAT_CODE_SYS900HD)
		||(blk==AKAI_FAT_CODE_SYS)
		||(blk==AKAI_FAT_CODE_DIREND900HD)
		||(blk==AKAI_FAT_CODE_DIREND1000HD)
		||(blk==AKAI_FAT_CODE_DIREND3000)
		||(blk==AKAI_FAT_CODE_FILEEND900)
		||(blk==AKAI_FAT_CODE_FILEEND)
		||(blk>=bsize)
		||(blk<bsyssize)){
		return -1; /* invalid */
	}

	return 0; /* OK */
}



int
print_fatchain(struct part_s *pp,u_int blk)
{
	u_int i;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}

	printf("lblk    pblk\n");
	printf("--------------\n");
	for (i=0;i<pp->bsize;i++){ /* XXX to avoid loop */
		if (akai_check_fatblk(blk,pp->bsize,pp->bsyssize)<0){
			break; /* end of chain */
		}
		printf("0x%04x  0x%04x\n",i,blk);
		/* next block */
		blk=(pp->fat[blk][1]<<8)+pp->fat[blk][0];
	}
	printf("--------------\n");
#if 0
	printf("END=    0x%04x\n",blk);
#endif

	if (i==pp->bsize){ /* XXX stuck in loop? */
		return 1;
	}

	return 0;
}



/* Note: if writeflag==0, free block counter of partition is not updated!!! */
int
akai_free_fatchain(struct part_s *pp,u_int bstart,int writeflag)
{
	int ret;
	u_int fblk,nblk;
	u_int bc;
	u_int i;
	u_int hdsiz;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)||(bstart>pp->bsize)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}

	ret=0; /* OK so far */

	/* free FAT chain */
	fblk=bstart;
	bc=0; /* block counter */
	for (i=0;i<pp->bsize;i++){ /* XXX to avoid loop */
		/* check fblk */
		if ( /* Note: don't check for AKAI_FAT_CODE_SYS900FL here since it is ==AKAI_FAT_CODE_FREE !!! */
			  (fblk==AKAI_FAT_CODE_SYS900HD)
			||(fblk==AKAI_FAT_CODE_SYS)
			||(fblk==AKAI_FAT_CODE_DIREND900HD)
			||(fblk==AKAI_FAT_CODE_DIREND1000HD)
			||(fblk==AKAI_FAT_CODE_DIREND3000)
			||(fblk==AKAI_FAT_CODE_FILEEND900)
			||(fblk==AKAI_FAT_CODE_FILEEND)){ /* valid end of chain? */
			break; /* done */
		}
		if ((fblk==AKAI_FAT_CODE_FREE)||(fblk>=pp->bsize)){ /* invalid? */
			fprintf(stderr,"invalid block in chain\n");
			ret=-1;
			break;
		}
		/* next block */
		nblk=(pp->fat[fblk][1]<<8)+pp->fat[fblk][0];
		/* free block in FAT */
		pp->fat[fblk][1]=0xff&(AKAI_FAT_CODE_FREE>>8);
		pp->fat[fblk][0]=0xff&AKAI_FAT_CODE_FREE;
		bc++;
		/* advance */
		fblk=nblk;
	}

	if (writeflag){
		/* update free block counter */
		pp->bfree+=bc;

		/* write new FAT to partition */
		if ((pp->type==PART_TYPE_FLL)||(pp->type==PART_TYPE_FLH)){
			/* write floppy header */
			if (pp->type==PART_TYPE_FLL){
				hdsiz=AKAI_FLLHEAD_BLKS; /* floppy header */
			}else{
				hdsiz=AKAI_FLHHEAD_BLKS; /* floppy header */
			}
			if (akai_io_blks(pp,(u_char *)&pp->head.flh,
							 0,
							 hdsiz,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}else if (pp->type==PART_TYPE_HD9){
			/* copy first FAT entries */
			bcopy((u_char *)&pp->head.hd9.fatblk,(u_char *)&pp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
			/* write S900 harddisk header */
			if (akai_io_blks(pp,(u_char *)&pp->head.hd9,
							 0,
							 AKAI_HD9HEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}else if (pp->type==PART_TYPE_HD){
			/* write S1000/S3000 partition header */
			if (akai_io_blks(pp,(u_char *)&pp->head.hd,
							 0,
							 AKAI_PARTHEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
	}

	return ret;
}



/* returns first block in *bstartp */
int
akai_allocate_fatchain(struct part_s *pp,u_int bsize,u_int *bstartp,u_int bcont0,u_int endcode)
{
	int ret;
	u_int fblk,pblk;
	u_int bc;
	u_int hdsiz;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}
	if (bstartp==NULL){
		return -1;
	}

	if (bcont0>bsize){ /* want more contiguous blocks than total? */
		return -1;
	}

	/* check if enough space left */
	if (bsize>pp->bfree){
		fprintf(stderr,"not enough space left\n");
		return -1;
	}
	/* now there should be enough free blocks if there was no bcont0 constraint */

	/* check code for end of chain */
	if ( /* Note: don't check for AKAI_FAT_CODE_SYS900FL here since it is ==AKAI_FAT_CODE_FREE !!! */
		  (endcode!=AKAI_FAT_CODE_SYS900HD)
		&&(endcode!=AKAI_FAT_CODE_SYS)
		&&(endcode!=AKAI_FAT_CODE_DIREND900HD)
		&&(endcode!=AKAI_FAT_CODE_DIREND1000HD)
		&&(endcode!=AKAI_FAT_CODE_DIREND3000)
		&&(endcode!=AKAI_FAT_CODE_FILEEND900)
		&&(endcode!=AKAI_FAT_CODE_FILEEND)){
		return -1;
	}

	if (bsize==0){
		return 0; /* done */
	}

	/* scan FAT */
	ret=0; /* no error so far */
	bc=0; /* block counter */
	*bstartp=pp->bsize; /* invalid */
	pblk=0;
	/* Note: start at block pp->bsyssize in order to skip reserved system blocks */
	/*       => also avoids problem due to AKAI_FAT_CODE_SYS900FL==AKAI_FAT_CODE_FREE */
	for (fblk=pp->bsyssize;fblk<pp->bsize;fblk++){ /* block */
		/* check if fblk is free */
		if (((pp->fat[fblk][1]<<8)+pp->fat[fblk][0])!=AKAI_FAT_CODE_FREE){
			continue; /* not free, next */
		}
		/* is free */

		/* allocate */
		if (bc==0){ /* is first free block? */
			/* start of chain */
			*bstartp=fblk;
		}else{
			if ((bcont0<=1)||(fblk==(pblk+1))){ /* don't care or contiguous to previous block? */
				/* link chain in previous free block */
				pp->fat[pblk][1]=0xff&(fblk>>8);
				pp->fat[pblk][0]=0xff&fblk;
			}else{
				/* we care and this is a non-contiguous block */
				if (bc<bcont0){ /* not enough so far? */
					/* must discard chain allocated so far */
					/* free chain */
					if (akai_free_fatchain(pp,*bstartp,0)<0){ /* 0: don't write FAT yet */
						ret=-1;
						goto akai_allocate_fatchain_done;
					}
					/* nothing anymore, new start */
					*bstartp=fblk;
					bc=0;
				}else{
					/* enough contiguous blocks, non-contiguous blocks may follow now */
					bcont0=0; /* don't care anymore */
				}
			}
		}
		bc++; /* found one */
		/* temporarily mark end of chain */
		pp->fat[fblk][1]=0xff&(endcode>>8);
		pp->fat[fblk][0]=0xff&endcode;

		if (bc==bsize){ /* enough? */
			/* keep end mark */
			break; /* done */
		}
		pblk=fblk; /* save for next element */
	}

	if (bc!=bsize){ /* not enough? */
		if (*bstartp<pp->bsize){ /* valid chain? */
			/* free chain */
			if (akai_free_fatchain(pp,*bstartp,0)<0){ /* 0: don't write FAT yet */
				ret=-1;
				goto akai_allocate_fatchain_done;
			}
		}
	}else{
		/* enough */
		/* update free block counter */
		pp->bfree-=bc;
	}

akai_allocate_fatchain_done:
	if (ret<0){
		/* XXX in case something went wrong */
		akai_countfree_part(pp);
	}
	/* write new FAT to partition */
	if ((pp->type==PART_TYPE_FLL)||(pp->type==PART_TYPE_FLH)){
		/* write floppy header */
		if (pp->type==PART_TYPE_FLL){
			hdsiz=AKAI_FLLHEAD_BLKS; /* floppy header */
		}else{
			hdsiz=AKAI_FLHHEAD_BLKS; /* floppy header */
		}
		if (akai_io_blks(pp,(u_char *)&pp->head.flh,
						 0,
						 hdsiz,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			ret=-1;
		}
	}else if (pp->type==PART_TYPE_HD9){
		/* copy first FAT entries */
		bcopy((u_char *)&pp->head.hd9.fatblk,(u_char *)&pp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
		/* write S900 harddisk header */
		if (akai_io_blks(pp,(u_char *)&pp->head.hd9,
						 0,
						 AKAI_HD9HEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			ret=-1;
		}
	}else if (pp->type==PART_TYPE_HD){
		/* write S1000/S3000 partition header */
		if (akai_io_blks(pp,(u_char *)&pp->head.hd,
						 0,
						 AKAI_PARTHEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			ret=-1;
		}
	}

	return ret;
}



int
akai_read_file(int outfd,u_char *outbuf,struct file_s *fp,u_int begin,u_int end)
{
	u_char fbuf[AKAI_HD_BLOCKSIZE];
	u_int fblk,fchunk,fremain,skipbyte;
	u_char (*fatp)[2];

	if ((outfd<0)&&(outbuf==NULL)){
		return -1;
	}

	if (fp==NULL){
		return -1;
	}
	if ((fp->volp==NULL)||(fp->volp->type==AKAI_VOL_TYPE_INACT)){
		return -1;
	}
	if ((fp->volp->partp==NULL)||(!fp->volp->partp->valid)||(fp->volp->partp->fd<0)){
		return -1;
	}
	fatp=fp->volp->partp->fat;
	if (fatp==NULL){
		return -1;
	}
	if ((fp->volp->partp->blksize==0)||(fp->volp->partp->blksize>AKAI_HD_BLOCKSIZE)){
		return -1;
	}

	if (fp->type==AKAI_FTYPE_FREE){
		return -1;
	}

	if ((end<begin)||(end>fp->size)){
		fprintf(stderr,"invalid limits\n");
		return -1;
	}

	fblk=fp->bstart; /* start block */
	fremain=end; /* remaining bytes */
	skipbyte=begin; /* bytes to skip */
	for (;fremain>0;){ /* byte counter */
		if (fremain>=fp->volp->partp->blksize){
			fchunk=fp->volp->partp->blksize;
		}else{
			fchunk=fremain;
		}
		/* check fblk */
		if (akai_check_fatblk(fblk,fp->volp->partp->bsize,fp->volp->partp->bsyssize)<0){
			fprintf(stderr,"invalid block in file\n");
			return -1;
		}
		if (skipbyte<fchunk){
			/* read block */
			if (akai_io_blks(fp->volp->partp,(u_char *)fbuf,
							 fblk,
							 1,
							 0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
				return -1;
			}
			if (outbuf!=NULL){
				/* to buffer */
				bcopy(fbuf+skipbyte,outbuf,fchunk-skipbyte);
				outbuf+=fchunk-skipbyte;
			}else{
				/* write to file */
				if (WRITE(outfd,(void *)(fbuf+skipbyte),fchunk-skipbyte)!=(int)(fchunk-skipbyte)){
					perror("write");
					return -1;
				}
			}
			skipbyte=0; /* done */
		}else{
			skipbyte-=fchunk;
		}
		/* next block */
		fblk=(fatp[fblk][1]<<8)+fatp[fblk][0];
		/* chunk done */
		fremain-=fchunk;
	}

	return 0;
}



int
akai_write_file(int inpfd,u_char *inpbuf,struct file_s *fp,u_int begin,u_int end)
{
	u_char fbuf[AKAI_HD_BLOCKSIZE];
	u_int fblk,fchunk,fremain,skipbyte;
	u_char (*fatp)[2];

	if ((inpfd<0)&&(inpbuf==NULL)){
		return -1;
	}

	if (fp==NULL){
		return -1;
	}
	if ((fp->volp==NULL)||(fp->volp->type==AKAI_VOL_TYPE_INACT)){
		return -1;
	}
	if ((fp->volp->partp==NULL)||(!fp->volp->partp->valid)||(fp->volp->partp->fd<0)){
		return -1;
	}
	fatp=fp->volp->partp->fat;
	if (fatp==NULL){
		return -1;
	}
	if ((fp->volp->partp->blksize==0)||(fp->volp->partp->blksize>AKAI_HD_BLOCKSIZE)){
		return -1;
	}

	if (fp->type==AKAI_FTYPE_FREE){
		return -1;
	}

	if ((end<begin)||(end>fp->size)){
		fprintf(stderr,"invalid limits\n");
		return -1;
	}

	fblk=fp->bstart; /* start block */
	fremain=end; /* remaining bytes */
	skipbyte=begin; /* bytes to skip */
	for (;fremain>0;){ /* byte counter */
		if (fremain>=fp->volp->partp->blksize){
			fchunk=fp->volp->partp->blksize;
		}else{
			fchunk=fremain;
		}
		/* check fblk */
		if (akai_check_fatblk(fblk,fp->volp->partp->bsize,fp->volp->partp->bsyssize)<0){
			fprintf(stderr,"invalid block in file\n");
			return -1;
		}
		if (skipbyte<fchunk){
			if ((skipbyte>0)||(fchunk<fp->volp->partp->blksize)){ /* need to read? */
				/* read block */
				if (akai_io_blks(fp->volp->partp,(u_char *)fbuf,
								 fblk,
								 1,
								 0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
					return -1;
				}
			}
			if (inpbuf!=NULL){
				/* from buffer */
				bcopy(inpbuf,fbuf+skipbyte,fchunk-skipbyte);
				inpbuf+=fchunk-skipbyte;
			}else{
				/* read file */
				if (READ(inpfd,(void *)(fbuf+skipbyte),fchunk-skipbyte)!=(int)(fchunk-skipbyte)){
					perror("read");
					return -1;
				}
			}
			/* write block */
			if (akai_io_blks(fp->volp->partp,(u_char *)fbuf,
							 fblk,
							 1,
							 0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
				return -1;
			}
			skipbyte=0; /* done */
		}else{
			skipbyte-=fchunk;
		}
		/* next block */
		fblk=(fatp[fblk][1]<<8)+fatp[fblk][0];
		/* chunk done */
		fremain-=fchunk;
	}

	return 0;
}



int
print_ddfatchain(struct part_s *pp,u_int cstart)
{
	u_int i;
	u_int cl,nextcl;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}
	if (cstart>=pp->csize){
		return -1;
	}

	printf("lcl     pcl\n");
	printf("--------------\n");
	cl=cstart;
	for (i=0;i<pp->csize;i++){ /* XXX to avoid loop */
		/* check cl */
		if ((cl==AKAI_DDFAT_CODE_FREE)
			||(cl==AKAI_DDFAT_CODE_SYS)
			||(cl==AKAI_DDFAT_CODE_END)
			||(cl>=pp->csize)){ /* invalid? */
			break; /* exit */
		}
		/* next cluster */
		nextcl=(pp->fat[cl][1]<<8)+pp->fat[cl][0];
		/* print */
		printf("0x%04x  0x%04x\n",i,cl);
		/* advance */
		if (nextcl==AKAI_DDFAT_CODE_END){
			break; /* end of chain */
		}
		cl=nextcl;
	}
	printf("--------------\n");
#if 0
	printf("END=    0x%04x\n",cl);
#endif

	if (i==pp->csize){ /* XXX stuck in loop? */
		return 1;
	}

	return 0;
}



u_int
akai_count_ddfatchain(struct part_s *pp,u_int cstart)
{
	u_int cc;
	u_int i;
	u_int cl,nextcl;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return 0;
	}
	if (pp->type!=PART_TYPE_DD){
		return 0;
	}
	if (cstart>=pp->csize){
		return 0;
	}

	cc=0;
	cl=cstart;
	for (i=0;i<pp->csize;i++){ /* XXX to avoid loop */
		/* check cl */
		if ((cl==AKAI_DDFAT_CODE_FREE)
			||(cl==AKAI_DDFAT_CODE_SYS)
			||(cl==AKAI_DDFAT_CODE_END)
			||(cl>=pp->csize)){ /* invalid? */
			break; /* exit */
		}
		/* next cluster */
		nextcl=(pp->fat[cl][1]<<8)+pp->fat[cl][0];
		cc++; /* +1 cluster */
		/* advance */
		if (nextcl==AKAI_DDFAT_CODE_END){
			break; /* end of chain, done */
		}
		cl=nextcl;
	}

	return cc;
}



/* Note: if writeflag==0, free block counter of partition is not updated!!! */
int
akai_free_ddfatchain(struct part_s *pp,u_int cstart,int writeflag)
{
	int ret;
	u_int cl,nextcl;
	u_int cc;
	u_int i;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)||(cstart>pp->csize)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	ret=0; /* OK so far */

	/* free DD FAT chain */
	cl=cstart;
	cc=0; /* cluster counter */
	for (i=0;i<pp->csize;i++){ /* XXX to avoid loop */
		/* check cl */
		if ((cl==AKAI_DDFAT_CODE_FREE)
			||(cl==AKAI_DDFAT_CODE_SYS)
			||(cl==AKAI_DDFAT_CODE_END)
			||(cl>=pp->csize)){ /* invalid? */
			fprintf(stderr,"invalid cluster in chain\n");
			ret=-1;
			break;
		}
		/* next cluster */
		nextcl=(pp->fat[cl][1]<<8)+pp->fat[cl][0];
		/* free cluster in FAT */
		pp->fat[cl][1]=0xff&(AKAI_DDFAT_CODE_FREE>>8);
		pp->fat[cl][0]=0xff&AKAI_DDFAT_CODE_FREE;
		cc++;
		/* advance */
		if (nextcl==AKAI_DDFAT_CODE_END){
			break; /* end of chain, done */
		}
		cl=nextcl;
	}

	if (writeflag){
		/* update free block counter */
		pp->bfree+=cc*AKAI_DDPART_CBLKS;

		/* write partition header */
		if (akai_io_blks(pp,(u_char *)&pp->head.dd,
						 0,
						 AKAI_DDPARTHEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}

	return ret;
}



/* returns first cluster in *cstartp */
int
akai_allocate_ddfatchain(struct part_s *pp,u_int csize,u_int *cstartp,u_int ccont0)
{
	int ret;
	u_int fcl,pcl;
	u_int cc;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}
	if (cstartp==NULL){
		return -1;
	}

	if (ccont0>csize){ /* want more contiguous clusters than total? */
		return -1;
	}

	/* check if enough space left */
	if (csize*AKAI_DDPART_CBLKS>pp->bfree){
		fprintf(stderr,"not enough space left\n");
		return -1;
	}
	/* now there should be enough free clusters if there was no ccont0 constraint */

	if (csize==0){
		return 0; /* done */
	}

	/* scan FAT */
	ret=0; /* no error so far */
	cc=0; /* cluster counter */
	*cstartp=pp->csize; /* invalid */
	pcl=0;
	/* Note: start at cluster 1 in order to skip reserved system cluster 0 which contains partition header */
	for (fcl=1;fcl<pp->csize;fcl++){ /* cluster */
		/* check if fcl is free */
		if (((pp->fat[fcl][1]<<8)+pp->fat[fcl][0])!=AKAI_DDFAT_CODE_FREE){
			continue; /* not free, next */
		}
		/* is free */

		/* allocate */
		if (cc==0){ /* is first free cluster? */
			/* start of chain */
			*cstartp=fcl;
		}else{
			if ((ccont0<=1)||(fcl==(pcl+1))){ /* don't care or contiguous to previous cluster? */
				/* link chain in previous free cluster */
				pp->fat[pcl][1]=0xff&(fcl>>8);
				pp->fat[pcl][0]=0xff&fcl;
			}else{
				/* we care and this is a non-contiguous cluster */
				if (cc<ccont0){ /* not enough so far? */
					/* must discard chain allocated so far */
					/* free chain */
					if (akai_free_ddfatchain(pp,*cstartp,0)<0){ /* 0: don't write FAT yet */
						ret=-1;
						goto akai_allocate_ddfatchain_done;
					}
					/* nothing anymore, new start */
					*cstartp=fcl;
					cc=0;
				}else{
					/* enough contiguous clusters, non-contiguous clusters may follow now */
					ccont0=0; /* don't care anymore */
				}
			}
		}
		cc++; /* found one */
		/* temporarily mark end of chain */
		pp->fat[fcl][1]=0xff&(AKAI_DDFAT_CODE_END>>8);
		pp->fat[fcl][0]=0xff&AKAI_DDFAT_CODE_END;

		if (cc==csize){ /* enough? */
			/* keep end mark */
			break; /* done */
		}
		pcl=fcl; /* save for next element */
	}

	if (cc!=csize){ /* not enough? */
		if (*cstartp<pp->csize){ /* valid chain? */
			/* free chain */
			if (akai_free_ddfatchain(pp,*cstartp,0)<0){ /* 0: don't write FAT yet */
				ret=-1;
				goto akai_allocate_ddfatchain_done;
			}
		}
	}else{
		/* enough */
		/* update free block counter */
		pp->bfree-=cc*AKAI_DDPART_CBLKS;
	}

akai_allocate_ddfatchain_done:
	if (ret<0){
		/* XXX in case something went wrong */
		akai_countfree_part(pp);
	}
	/* write partition header */
	if (akai_io_blks(pp,(u_char *)&pp->head.dd,
					 0,
					 AKAI_DDPARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		ret=-1;
	}

	return ret;
}



int
akai_export_ddfatchain(struct part_s *pp,u_int cstart,u_int bstart,u_int bsize,int outfd,u_char *outbuf)
{
	static u_char buf[AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE];
	u_int bufsiz;
	u_int i,i0,i1;
	u_int cl,nextcl;
	u_int chunkoff,chunksiz;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}
	if (cstart>=pp->csize){
		return -1;
	}
	if ((outfd<0)&&(outbuf==NULL)){
		return -1;
	}

	if (bsize==0){
		return 0;
	}

	bufsiz=AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE; /* 1 cluster in bytes */

	i0=bstart/bufsiz; /* first cluster for export */
	i1=(bstart+bsize-1)/bufsiz; /* last cluster for export */
	if (i1>=pp->csize){
		return -1;
	}

	cl=cstart;
	for (i=0;i<=i1;i++){
		/* check cl */
		if ((cl==AKAI_DDFAT_CODE_FREE)
			||(cl==AKAI_DDFAT_CODE_SYS)
			||(cl==AKAI_DDFAT_CODE_END)
			||(cl>=pp->csize)){ /* invalid? */
			return -1;
		}
		/* next cluster */
		nextcl=(pp->fat[cl][1]<<8)+pp->fat[cl][0];

		if (i>=i0){ /* far enough? */
			/* read cluster */
			if (akai_io_blks(pp,buf,
							 cl*AKAI_DDPART_CBLKS, /* block offset */
							 AKAI_DDPART_CBLKS, /* 1 cluster */
							 1,IO_BLKS_READ)<0){  /* 1: alloc cache if possible */
				return -1;
			}

			if (i==i0){ /* first cluster? */
				chunkoff=bstart-i0*bufsiz;
				chunksiz=bufsiz-chunkoff;
				/* Note: chunkoff and chunksiz <= bufsiz by definition of i0 */
				if (chunksiz>bsize){
					chunksiz=bsize;
				}
			}else if (i==i1){ /* last cluster? */
				chunkoff=0;
				chunksiz=bstart+bsize-i1*bufsiz;
				/* Note: chunksiz <= bufsiz by definition of i1 */
			}else{
				chunkoff=0;
				chunksiz=bufsiz; /* 1 cluster */
			}

			if (outbuf!=NULL){
				/* to buffer */
				bcopy(buf+chunkoff,outbuf,chunksiz);
				outbuf+=chunksiz;
			}else{
				/* write to file */
				if (WRITE(outfd,buf+chunkoff,chunksiz)!=(int)chunksiz){
					perror("write");
					return -1;
				}
			}
		}

		/* advance */
		if (nextcl==AKAI_DDFAT_CODE_END){
			break; /* end of chain, done */
		}
		cl=nextcl;
	}

	return 0;
}



int
akai_import_ddfatchain(struct part_s *pp,u_int cstart,u_int bstart,u_int bsize,int inpfd,u_char *inpbuf)
{
	static u_char buf[AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE];
	u_int bufsiz;
	u_int i,i0,i1;
	u_int cl,nextcl;
	u_int chunkoff,chunksiz;
	int readflag;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}
	if (cstart>=pp->csize){
		return -1;
	}
	if ((inpfd<0)&&(inpbuf==NULL)){
		return -1;
	}

	if (bsize==0){
		return 0;
	}

	bufsiz=AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE; /* 1 cluster in bytes */

	i0=bstart/bufsiz; /* first cluster for export */
	i1=(bstart+bsize-1)/bufsiz; /* last cluster for export */
	if (i1>=pp->csize){
		return -1;
	}

	cl=cstart;
	for (i=0;i<=i1;i++){
		/* check cl */
		if ((cl==AKAI_DDFAT_CODE_FREE)
			||(cl==AKAI_DDFAT_CODE_SYS)
			||(cl==AKAI_DDFAT_CODE_END)
			||(cl>=pp->csize)){ /* invalid? */
			return -1;
		}
		/* next cluster */
		nextcl=(pp->fat[cl][1]<<8)+pp->fat[cl][0];

		if (i>=i0){ /* far enough? */
			if (i==i0){ /* first cluster? */
				chunkoff=bstart-i0*bufsiz;
				chunksiz=bufsiz-chunkoff;
				/* Note: chunkoff and chunksiz <= bufsiz by definition of i0 */
				if (chunksiz>bsize){
					chunksiz=bsize;
				}
				readflag=1;
			}else if (i==i1){ /* last cluster? */
				chunkoff=0;
				chunksiz=bstart+bsize-i1*bufsiz;
				/* Note: chunksiz <= bufsiz by definition of i1 */
				readflag=1;
			}else{
				chunkoff=0;
				chunksiz=bufsiz; /* 1 cluster */
				readflag=0;
			}

			if (readflag){
				/* read cluster */
				if (akai_io_blks(pp,buf,
								 cl*AKAI_DDPART_CBLKS, /* block offset */
								 AKAI_DDPART_CBLKS, /* 1 cluster */
								 1,IO_BLKS_READ)<0){  /* 1: alloc cache if possible */
					return -1;
				}
			}

			if (inpbuf!=NULL){
				/* from buffer */
				bcopy(inpbuf,buf+chunkoff,chunksiz);
				inpbuf+=chunksiz;
			}else{
				/* read from file */
				if (READ(inpfd,buf+chunkoff,chunksiz)!=(int)chunksiz){
					perror("write");
					return -1;
				}
			}

			/* write cluster */
			if (akai_io_blks(pp,buf,
							 cl*AKAI_DDPART_CBLKS, /* block offset */
							 AKAI_DDPART_CBLKS, /* 1 cluster */
							 1,IO_BLKS_WRITE)<0){  /* 1: alloc cache if possible */
				return -1;
			}
		}

		/* advance */
		if (nextcl==AKAI_DDFAT_CODE_END){
			break; /* end of chain, done */
		}
		cl=nextcl;
	}

	return 0;
}



/* convert char for S1000/S3000 */
char
akai2ascii(u_char c)
{
	char a;

	if (c<=9){
		a='0'+c-0;
	}else if (c==10){
		a=' ';
	}else if ((c>=11)&&(c<=36)){
		a='A'+c-11;
	}else if (c==37){
		a='#';
	}else if (c==38){
		a='+';
	}else if (c==39){
		a='-';
	}else if (c==40){
		a='.';
	}else{
		a='.'; /* XXX */
	}

	return a;
}

/* convert char for S900 */
char
akai2ascii900(u_char c)
{
	char a;

	if (((c>='0')&&(c<='9'))
		||(c==' ')
		||((c>='A')&&(c<='Z'))
		||((c>='a')&&(c<='z'))
		||(c=='#')
		||(c=='+')
		||(c=='-')
		||(c=='.')){
		a=c;
	}else if (c=='\0'){
		a=' '; /* XXX */
	}else{
		a='.'; /* XXX */
	}

	return a;
}

void
akai2ascii_name(u_char *aname,char *name,int s900flag)
{
	int i;
	int len;

	if (name==NULL){
		return;
	}
	if (aname==NULL){
		name[0]='\0';
		return;
	}

	if (s900flag){
		len=AKAI_NAME_LEN_S900;
	}else{
		len=AKAI_NAME_LEN;
	}
	for (i=0;i<len;i++){
		if (s900flag){
#if 1
			if (aname[i]=='\0'){ /* end? */
				len=i;
				break; /* done */
			}
#endif
			name[i]=akai2ascii900(aname[i]);
		}else{
			name[i]=akai2ascii(aname[i]);
		}
	}
	name[len]='\0';

	/* remove trailing ' ' */
	for (i=0;i<len;i++){
		if (name[len-i-1]==' '){
			name[len-i-1]='\0';
		}else{
			break;
		}
	}
}

void
akai2ascii_filename(u_char *aname,u_char ft,u_int osver,char *name,int s900flag)
{
	int l;

	if (name==NULL){
		return;
	}

	/* Note: aname==NULL is also OK */
	if (aname==NULL){
		name[0]='\0';
	}else{
		akai2ascii_name(aname,name,s900flag);
	}

	/* type */
	l=(int)strlen(name);
	if (ft==AKAI_SAMPLE900_FTYPE){
		/* S900 sample file */
		if (osver!=0){
			/* S900 compressed sample format */
			sprintf(name+l,".S9C");
		}else{
			/* S900 non-compressed sample format */
			sprintf(name+l,".S9");
		}
	}else if (ft==AKAI_CDSETUP3000_FTYPE){
		/* CD3000 CD-ROM setup file */
		/* Note: not ".T9" */
		sprintf(name+l,".CD");
	}else if (ft==AKAI_CDSAMPLE3000_FTYPE){
		/* CD3000 CD-ROM sample parameters file */
		/* Note: not ".H3" */
		sprintf(name+l,".s+");
	}else if ((ft>=AKAI_S900_FTYPE_MIN)&&(ft<=AKAI_S900_FTYPE_MAX)){
		/* S900 */
		/* Note: 'T'==AKAI_CDSETUP3000_FTYPE was handled above */
		sprintf(name+l,".%c9",(char)('A'+ft-AKAI_S900_FTYPE_MIN));
	}else if ((ft>=AKAI_S1000_FTYPE_MIN)&&(ft<=AKAI_S1000_FTYPE_MAX)){
		/* S1000 */
#if 1
		if ((ft=='p')||(ft=='s')){
			sprintf(name+l,".%c1",(char)('A'+ft-AKAI_S1000_FTYPE_MIN));
		}else
#endif
		{
			sprintf(name+l,".%c",(char)('A'+ft-AKAI_S1000_FTYPE_MIN));
		}
	}else if ((ft>=(u_char)AKAI_S3000_FTYPE_MIN)&&(ft<=(u_char)AKAI_S3000_FTYPE_MAX)){
		/* S3000 */
		/* Note: 'h'+0x80==AKAI_CDSAMPLE3000_FTYPE was handled above */
		sprintf(name+l,".%c3",(char)('A'+ft-AKAI_S3000_FTYPE_MIN));
	}else{
		/* unknown */
		sprintf(name+l,".x%02x",(u_int)ft);
	}
}

/* convert char for S1000/S3000 */
u_char
ascii2akai(char a)
{
	char c;

	if (a=='_'){ /* for keyboard entry */
		a=' ';
	}

	if ((a>='0')&&(a<='9')){
		c=a-'0';
	}else if (a==' '){
		c=10;
	}else if ((a>='A')&&(a<='Z')){
		c=11+a-'A';
	}else if ((a>='a')&&(a<='z')){
		c=11+a-'a';
	}else if (a=='#'){
		c=37;
	}else if (a=='+'){
		c=38;
	}else if (a=='-'){
		c=39;
	}else if (a=='.'){
		c=40;
	}else{
		c=40; /* XXX */
	}

	return c;
}

/* convert char for S900 */
u_char
ascii2akai900(char a)
{
	char c;

	if (a=='_'){ /* for keyboard entry */
		a=' ';
	}

	if (((a>='0')&&(a<='9'))
		||(a==' ')
		||((a>='A')&&(a<='Z'))
		||((a>='a')&&(a<='z'))
		||(a=='#')
		||(a=='+')
		||(a=='-')
		||(a=='.')){
		c=a;
	}else{
		c='.'; /* XXX */
	}

	return c;
}

void
ascii2akai_name(char *name,u_char *aname,int s900flag)
{
	int i;
	u_char blank;
	int len;

	if (aname==NULL){
		return;
	}

	/* fill with default ' ' */
	if (s900flag){
		blank=ascii2akai900(' ');
		len=AKAI_NAME_LEN_S900;
	}else{
		blank=ascii2akai(' ');
		len=AKAI_NAME_LEN;
	}
	for (i=0;i<len;i++){
		if ((name==NULL)||(name[i]=='\0')){ /* nothing or end? */
			aname[i]=blank; /* default */
			name=NULL; /* don't want to go beyond end */
		}else{
			if (s900flag){
				aname[i]=ascii2akai900(name[i]);
			}else{
				aname[i]=ascii2akai(name[i]);
			}
		}
	}
}

u_char
ascii2akai_filename(char *name,u_char *aname,u_int *osverp,int s900flag)
{
	char *tn;
	u_int tl;
	u_int i,l;
	u_int u;
	u_char ft;
	char namebuf[AKAI_NAME_LEN+4+1]; /* +4 for ".<type>", +1 for '\0' */

	if ((aname==NULL)||(osverp==NULL)){
		return 0; /* XXX */
	}

	l=(u_int)strlen(name);
	/* check prior to strcpy */
	if (l>AKAI_NAME_LEN+4){ /* too long for namebuf? */
		/* invalid length */
		return AKAI_FTYPE_FREE;
	}

	/* copy name for possible manipulation below */
	strcpy(namebuf,name);

	/* check length and suffix */
	if ((l>=2)&&(l<=AKAI_NAME_LEN+2)&&(namebuf[l-2]=='.')){
		namebuf[l-2]='\0'; /* separate */
		tn=&namebuf[l-1]; /* type field */
		tl=1; /* type length */
	}else if ((l>=3)&&(l<=AKAI_NAME_LEN+3)&&(namebuf[l-3]=='.')){
		namebuf[l-3]='\0'; /* separate */
		tn=&namebuf[l-2]; /* type field */
		tl=2; /* type length */
	}else if ((l>=4)&&(l<=AKAI_NAME_LEN+4)&&(namebuf[l-4]=='.')){
		namebuf[l-4]='\0'; /* separate */
		tn=&namebuf[l-3]; /* type field */
		tl=3; /* type length */
	}else{
		/* no valid suffix */
		return AKAI_FTYPE_FREE;
	}
	l=(u_int)strlen(namebuf);
	/* remove trailing ' ' or '_' (XXX don't care how many) */
	for (i=0;i<l;i++){
		if ((namebuf[l-i-1]==' ')||(namebuf[l-i-1]=='_')){
			namebuf[l-i-1]='\0';
		}else{
			break;
		}
	}
	/* check name length */
	l=(u_int)strlen(namebuf);
	if (l>(u_int)(s900flag?AKAI_NAME_LEN_S900:AKAI_NAME_LEN)){
		/* invalid length */
		return AKAI_FTYPE_FREE;
	}

	/* type */
	if ((tl==2)&&(strcasecmp(tn,"S9")==0)){
		/* S900 sample file, non-compressed sample format */
		ft=AKAI_SAMPLE900_FTYPE;
		/* Note: set osver, even if not S900 volume */
		*osverp=0; /* non-compressed */
	}else if ((tl==3)&&(strcasecmp(tn,"S9C")==0)){
		/* S900 sample file, compressed sample format */
		ft=AKAI_SAMPLE900_FTYPE;
		/* Note: check/set osver, even if not S900 volume */
		if (*osverp==0){ /* unsuitable osver? */
			*osverp=1; /* XXX non zero */
		}
	}else if ((tl==2)&&(strcasecmp(tn,"CD")==0)){
		/* CD3000 CD-ROM setup file */
		/* Note: AKAI_CDSETUP3000_FTYPE=='T' => ".T9" will also lead to this type */
		ft=AKAI_CDSETUP3000_FTYPE;
		if (s900flag){
			*osverp=0; /* non-compressed */
		}
	}else if ((tl==2)&&(strcasecmp(tn,"s+")==0)){
		/* CD3000 CD-ROM sample parameters file */
		/* Note: AKAI_CDSAMPLE3000_FTYPE=='h'+0x80 => ".H3" will also lead to this type */
		ft=AKAI_CDSAMPLE3000_FTYPE;
		if (s900flag){
			*osverp=0; /* non-compressed */
		}
	}else if ((tl==1)||(tl==2)){
		ft=(u_char)tn[0];
		if ((ft>='A')&&(ft<='Z')){
			ft+='a'-'A'; /* to lower case */
		}
		if ((ft<'a')||(ft>'z')){
			/* invalid suffix */
			ft=AKAI_FTYPE_FREE;
		}
		if ((tl==1)||(tn[1]=='1')){
			/* S1000 */
			ft=AKAI_S1000_FTYPE_MIN+ft-'a';
		}else if (tn[1]=='9'){
			/* S900 */
			ft=AKAI_S900_FTYPE_MIN+ft-'a';
			*osverp=0; /* non-compressed */
		}else if (tn[1]=='3'){
			/* S3000 */
			ft=AKAI_S3000_FTYPE_MIN+ft-'a';
		}else{
			/* invalid suffix */
			ft=AKAI_FTYPE_FREE;
		}
		if (s900flag){
			*osverp=0; /* non-compressed */
		}
	}else{ /* must be tl==3 now */
		if (((tn[0]!='X')&&(tn[0]!='x'))
			||
			(((tn[1]<'0')||(tn[1]>'9'))
			 &&((tn[1]<'A')||(tn[1]>'F'))
			 &&((tn[1]<'a')||(tn[1]>'f')))
			||
			(((tn[2]<'0')||(tn[2]>'9'))
			 &&((tn[2]<'A')||(tn[2]>'F'))
			 &&((tn[2]<'a')||(tn[2]>'f')))){
			/* invalid suffix */
			ft=AKAI_FTYPE_FREE;
		}else{
			u=AKAI_FTYPE_FREE; /* default: invalid */
			sscanf(tn+1,"%x",&u);
			/* Note: if sscanf finds nothing, u remains! */
			ft=(u_char)u;
			/* Note: might still be invalid */
		}
		if (s900flag){
			*osverp=0; /* non-compressed */
		}
	}

	/* name */
	ascii2akai_name(namebuf,aname,s900flag);

#if 1
	if (ft==AKAI_VOLDIR3000FL_FTYPE){ /* S3000 floppy flag? */
		ft=AKAI_FTYPE_FREE; /* invalid */
	}
#endif

	/* Note: caller must check for AKAI_FTYPE_FREE as invalid code */
	return ft;
}



void
akai_vol_info(struct vol_s *vp,u_int ai,int verbose)
{

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)
		||(vp->partp==NULL)||(!vp->partp->valid)){
		return;
	}

	if (verbose){
		printf("vnr:         %u\n",vp->index+1);
		printf("vname:       \"%s\"\n",vp->name);
		if (vp->partp->type==PART_TYPE_HD9){
			if (ai>0){
				printf("alias:       V%u\n",ai);
			}
		}else if (vp->type!=AKAI_VOL_TYPE_S900){
			printf("load number: ");
			akai_print_lnum(vp->lnum);
			printf("\n");
		}
		printf("type:        ");
	}else{
		printf("%3u   %-12s   ",vp->index+1,vp->name);
		if (vp->partp->type==PART_TYPE_HD9){
			printf("V%-3u",ai);
		}else{
			akai_print_lnum(vp->lnum);
		}
		printf("    0x%04x  ",vp->dirblk[0]);
		if (vp->type!=AKAI_VOL_TYPE_S900){
			printf("%2u.%02u  ",0xff&(vp->osver>>8),0xff&vp->osver);
		}else{
			printf(" -     ");
		}
	}
	switch (vp->type){
		case AKAI_VOL_TYPE_INACT:
			printf("INACT");
			break;
		case AKAI_VOL_TYPE_S1000:
			printf("S1000");
			break;
		case AKAI_VOL_TYPE_S3000:
			printf("S3000");
			break;
		case AKAI_VOL_TYPE_CD3000:
			printf("CD3000");
			break;
		case AKAI_VOL_TYPE_S900:
			printf("S900");
			break;
		default:
			printf("(%02x)",vp->type);
			break;
	}
	printf("\n");
	if (verbose){
		if (vp->type!=AKAI_VOL_TYPE_S900){
			printf("osver:       %u.%02u\n",0xff&(vp->osver>>8),0xff&vp->osver);
		}
		printf("start:       block 0x%04x\n",vp->dirblk[0]);
		printf("max. files:  %u\n",vp->fimax);
	}
}

void
akai_list_vol(struct vol_s *vp,u_char *filtertagp)
{
	u_int fi;
	struct file_s tmpfile; /* current file */
	u_int totf,totb;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)
		||(vp->partp==NULL)||(!vp->partp->valid)
		||(vp->partp->diskp==NULL)||(vp->partp->fd<0)){
		return;
	}

	printf("/disk%u/%c/%s\n",vp->partp->diskp->index,vp->partp->letter,vp->name);
	if (vp->type==AKAI_VOL_TYPE_S900){
		printf("fnr  fname               size/B  startblk  uncompr\n");
		printf("--------------------------------------------------\n");
	}else{
		printf("fnr  fname               size/B  startblk  osver  tags\n");
		printf("--------------------------------------------------------------\n");
	}

	/* files in volume directory */
	totf=0;
	totb=0;
	for (fi=0;fi<vp->fimax;fi++){
		/* get file */
		if (akai_get_file(vp,&tmpfile,fi)<0){
			continue; /* next file */
		}
		/* test if tag-filter matches */
		if (akai_match_filetags(filtertagp,tmpfile.tag)<0){ /* no match? */
			continue; /* next file */
		}
		totf++;
		totb+=tmpfile.size;
		/* print file info */
		akai_file_info(&tmpfile,0);
	}

	if (vp->type==AKAI_VOL_TYPE_S900){
		printf("--------------------------------------------------\n");
	}else{
		printf("--------------------------------------------------------------\n");
	}
	printf("total: %3u file(s) (max. %3u), %9u bytes\n\n",totf,vp->fimax,totb);
}



int
akai_read_voldir(struct vol_s *vp)
{
	u_int i,imax;
	u_int blk;
	u_char *addr;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)||(vp->file==NULL)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)||(vp->partp->blksize==0)){
		return -1;
	}

	/* number of blocks */
	imax=vp->fimax*sizeof(struct akai_voldir_entry_s);
	imax=(imax+vp->partp->blksize-1)/vp->partp->blksize; /* round up */
	if (imax>VOL_DIRBLKS){
		return -1;
	}

	/* Note: first file starts at byte 0 in first block */
	addr=(u_char *)vp->file;
	for (i=0;i<imax;i++){
		blk=vp->dirblk[i];
#ifdef DEBUG
		printf("read dir block %2u: 0x%04x\n",i,blk);
#endif
		/* read volume directory block */
		if (akai_io_blks(vp->partp,addr,
						 blk,
						 1,
						 1,IO_BLKS_READ)<0){ /* 1: alloc cache if possible */
			return -1;
		}

		addr+=vp->partp->blksize; /* next */
	}

	return 0;
}

	
	
int
akai_write_voldir(struct vol_s *vp,u_int fi)
{
	u_int blk0,blk1;
	u_int blk;
	u_char *addr;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)||(vp->file==NULL)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)||(vp->partp->blksize==0)){
		return -1;
	}

	if (fi>=vp->fimax){
		return -1;
	}

	/* Note: file entry might overlap two adjacent blocks !!! */
	/* block(s) for file fi */
	blk0=fi*sizeof(struct akai_voldir_entry_s);
	blk1=blk0+sizeof(struct akai_voldir_entry_s)-1;
	blk0/=vp->partp->blksize;
	blk1/=vp->partp->blksize;
	if ((blk0>=VOL_DIRBLKS)||(blk1>=VOL_DIRBLKS)){
		return -1;
	}

	/* write volume directory block 0 to partition */
	blk=vp->dirblk[blk0];
	/* Note: first file starts at byte 0 in first block */
	addr=((u_char *)vp->file)+blk0*vp->partp->blksize;
#ifdef DEBUG
	printf("write dir block %2u: 0x%04x\n",blk0,blk);
#endif
	if (akai_io_blks(vp->partp,addr,
					 blk,
					 1,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return -1;
	}
	if (blk1!=blk0){
		/* write volume directory block 1 to partition */
		blk=vp->dirblk[blk1];
		addr+=vp->partp->blksize; /* next */
#ifdef DEBUG
		printf("write dir block %2u: 0x%04x\n",blk1,blk);
#endif
		if (akai_io_blks(vp->partp,addr,
						 blk,
						 1,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}

	return 0;
}



/* Note: see comment in akaiutil.h for struct vol_s about copying vol_s!!! */
void
akai_copy_structvol(struct vol_s *srcvp,struct vol_s *dstvp)
{

	if ((srcvp==NULL)||(srcvp->partp==NULL)||(!srcvp->partp->valid)||(srcvp->partp->fd<0)){
		return;
	}
	if (dstvp==NULL){
		return;
	}

	/* copy */
	*dstvp=*srcvp;

	/* fix pointers for copy to be self-contained */
	if ((srcvp->partp->type==PART_TYPE_FLL)||(srcvp->partp->type==PART_TYPE_FLH)){
		/* floppy */
		if ((srcvp->type==AKAI_VOL_TYPE_S3000)||(srcvp->type==AKAI_VOL_TYPE_CD3000)){
			/* S3000 */
			/* start of files */
			dstvp->file=&dstvp->dir.s3000fl.file[0];
		}
		/* Note: pointers into floppy header (in partition) remain the same!!! */
	}else if ((srcvp->partp->type==PART_TYPE_HD9)||(srcvp->partp->type==PART_TYPE_HD)){
		/* harddisk */
		if (srcvp->type==AKAI_VOL_TYPE_S900){
			/* S900 */
			/* start of files */
			dstvp->file=&dstvp->dir.s900hd.file[0];
			/* volume parameters */
			dstvp->param=NULL; /* S900 has no volume parameters */
		}else if (srcvp->type==AKAI_VOL_TYPE_S1000){
			/* S1000 */
			/* start of files */
			dstvp->file=&dstvp->dir.s1000hd.file[0];
			/* volume parameters */
			dstvp->param=&dstvp->dir.s1000hd.param;
		}else{
			/* S3000 */
			/* start of files */
			dstvp->file=&dstvp->dir.s3000hd.file[0];
			/* volume parameters */
			dstvp->param=&dstvp->dir.s3000hd.param;
		}
	}
}



/* get volume with index vi */
/* saves result in *vp */
int
akai_get_vol(struct part_s *pp,struct vol_s *vp,u_int vi)
{
	u_int i,j;
	struct akai_flvol_label_s *lp;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}
	if (vp==NULL){
		return -1;
	}

	/* in partition */
	vp->partp=pp;

	if ((pp->type==PART_TYPE_FLL)||(pp->type==PART_TYPE_FLH)){
		/* S900/S1000/S3000 floppy */
	
		/* fake index */
		if (vi!=0){
			return -1;
		}
		vp->index=vi;

		/* Note: low- and high-density floppy headers are identical */
		/*       up to first AKAI_FAT_ENTRIES_FLL FAT-entries */

		/* floppy volume label */
		if (pp->type==PART_TYPE_FLL){
			lp=&pp->head.fll.label;
		}else{
			lp=&pp->head.flh.label;
		}

		/* load number */
		vp->lnum=AKAI_VOL_LNUM_OFF;

		/* OS version in floppy header */
		vp->osver=(lp->osver[1]<<8)+lp->osver[0];

		/* determine floppy volume type */
		if (pp->head.flh.file[0].type==AKAI_VOLDIR3000FL_FTYPE){ /* S3000 flag? */
			/* S3000 */

			/* volume type */
			vp->type=AKAI_VOL_TYPE_S3000;

			/* number of volume directory entries */
			vp->fimax=AKAI_VOLDIR_ENTRIES_S3000FL;
			/* start of files */
			vp->file=&vp->dir.s3000fl.file[0];
			/* set volume directory blocks */
			if (pp->type==PART_TYPE_FLL){
				j=AKAI_VOLDIR3000FLL_BSTART;
			}else{
				j=AKAI_VOLDIR3000FLH_BSTART;
			}
			for (i=0;i<AKAI_VOLDIR3000FL_BLKS;i++){
				vp->dirblk[i]=j+i;
			}
		}else{
			/* S900 or S1000 */

			/* determine volume type */
			if (vp->osver==AKAI_OSVER_S900VOL){
				/* S900 */
				vp->type=AKAI_VOL_TYPE_S900;
			}else{
				/* S1000 */
				vp->type=AKAI_VOL_TYPE_S1000;
			}

			/* number of volume directory entries */
			vp->fimax=AKAI_VOLDIR_ENTRIES_S1000FL;
			/* start of files */
			/* Note: see comment above: start of files is the same for low- and high density */
			/* Note: S900 and S1000 floppy volume directory is within floppy header */
			vp->file=&pp->head.flh.file[0]; /* Note: in floppy header */
			/* set volume directory blocks */
			if (pp->type==PART_TYPE_FLL){
				j=AKAI_FLLHEAD_BLKS;
			}else{
				j=AKAI_FLHHEAD_BLKS;
			}
			for (i=0;i<j;i++){
				vp->dirblk[i]=i; /* start at offset 0 */
			}
		}
		
		/* name in floppy header */
		akai2ascii_name(lp->name,vp->name,vp->type==AKAI_VOL_TYPE_S900);
		if (vp->name[0]=='\0'){ /* empty volume name? */
			/* use default volume name as fake volume name */
			sprintf(vp->name,"VOLUME %03u",vp->index+1);
		}

		/* volume parameters */
		if (vp->type==AKAI_VOL_TYPE_S900){
			/* S900 */
			vp->param=NULL; /* S900 has no volume parameters */
		}else{
			/* S1000 or S3000 */
			vp->param=&lp->param; /* Note: in floppy header */
		}

		if (vp->type==AKAI_VOL_TYPE_S3000){
			/* S3000 */
			/* read volume directory */
			if (akai_read_voldir(vp)<0){
				fprintf(stderr,"S3000 volume directory missing\n");
				vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
				return -1;
			}
		} /* else: S900 and S1000 floppy volume directory is within floppy header */

		return 0;
	}

	if (pp->type==PART_TYPE_HD9){
		/* S900 harddisk */

		/* index */
		if (vi>=pp->volnummax){
			return -1;
		}
		vp->index=vi;

		/* get volume infos from root directory entry for volume */
		
		/* volume type */
		vp->type=AKAI_VOL_TYPE_S900;
		
		/* load number */
		vp->lnum=AKAI_VOL_LNUM_OFF;
		
		/* OS version */
		/* Note: no floppy volume labels for volumes on harddisk => cannot get OS version */
		vp->osver=AKAI_OSVER_S900VOL;

		/* name */
		/* Note: volume names in root directory use S1000/S3000 char conversion */
		akai2ascii_name(vp->partp->head.hd9.vol[vp->index].name,vp->name,1); /* 1: S900 */
		if (vp->name[0]=='\0'){ /* empty volume name? */
			/* use default volume name as fake volume name */
			sprintf(vp->name,"VOLUME %03u",vp->index+1);
		}

		/* volume directory block */
		vp->dirblk[0]=(vp->partp->head.hd9.vol[vp->index].start[1]<<8)
			+vp->partp->head.hd9.vol[vp->index].start[0];
		if (vp->dirblk[0]==AKAI_VOL_START_INACT){ /* volume inactive? */
			vp->type=AKAI_VOL_TYPE_INACT;
			return -1;
		}
		/* check vp->dirblk[0] */
		if (akai_check_fatblk(vp->dirblk[0],vp->partp->bsize,vp->partp->bsyssize)<0){
			fprintf(stderr,"invalid dir block 0: 0x%04x\n",vp->dirblk[0]);
			vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
			return -1;
		}
		/* number of volume directory entries */
		vp->fimax=AKAI_VOLDIR_ENTRIES_S900HD;
		/* start of files */
		vp->file=&vp->dir.s900hd.file[0];
		
		/* volume parameters */
		vp->param=NULL; /* S900 has no volume parameters */
		
		/* read volume directory */
		if (akai_read_voldir(vp)<0){
			vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
			return -1;
		}
		
		return 0;
	}

	if (pp->type==PART_TYPE_HD){
		/* S1000/S3000 harddisk sampler partition */

		/* index */
		if (vi>=pp->volnummax){
			return -1;
		}
		vp->index=vi;

		/* get volume infos from root directory entry for volume */
		
		/* volume type */
		vp->type=vp->partp->head.hd.vol[vp->index].type;
		
		/* load number */
		vp->lnum=vp->partp->head.hd.vol[vp->index].lnum;
		
		/* OS version */
		/* Note: no floppy volume labels for volumes on harddisk => cannot get OS version */
		/* derive OS version from volume type in root directory entry */
		if ((vp->type==AKAI_VOL_TYPE_S3000)||(vp->type==AKAI_VOL_TYPE_CD3000)){
			vp->osver=AKAI_OSVER_S3000MAX; /* XXX */
		}else{
			/* XXX assume S1000 */
			vp->osver=AKAI_OSVER_S1000MAX; /* XXX */
		}

		/* name */
		/* Note: volume names in root directory entries use S1000/S3000 char conversion */
		akai2ascii_name(vp->partp->head.hd.vol[vp->index].name,vp->name,0); /* 0: not S900 */
		if (vp->name[0]=='\0'){ /* empty volume name? */
			/* use default volume name as fake volume name */
			sprintf(vp->name,"VOLUME %03u",vp->index+1);
		}

		switch (vp->type){
		case AKAI_VOL_TYPE_INACT:
			return -1; /* invalid */
		case AKAI_VOL_TYPE_S1000:
		case AKAI_VOL_TYPE_S3000:
		case AKAI_VOL_TYPE_CD3000:
			break;
		default:
#if 0
			/* XXX assume S1000 */
			vp->type=AKAI_VOL_TYPE_S1000;
			break;
#else
			printf("unknown volume type (0x%04x)\n",vp->type);
			vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark inactive */
			return -1;
#endif
		}
		
		/* volume directory block(s) */
		vp->dirblk[0]=(vp->partp->head.hd.vol[vp->index].start[1]<<8)
			+vp->partp->head.hd.vol[vp->index].start[0];
		/* check vp->dirblk[0] */
		if (akai_check_fatblk(vp->dirblk[0],vp->partp->bsize,vp->partp->bsyssize)<0){
			fprintf(stderr,"invalid dir block 0: 0x%04x\n",vp->dirblk[0]);
			vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
			return -1;
		}
		/* volume directory entries */
		if (vp->type==AKAI_VOL_TYPE_S1000){
			/* S1000 */
			/* number of volume directory entries */
			vp->fimax=AKAI_VOLDIR_ENTRIES_S1000HD;
			/* start of files */
			vp->file=&vp->dir.s1000hd.file[0];
		}else{
			/* should be S3000 */
			vp->dirblk[1]=(vp->partp->fat[vp->dirblk[0]][1]<<8)
				+vp->partp->fat[vp->dirblk[0]][0];
			/* check vp->dirblk[1] */
			if (akai_check_fatblk(vp->dirblk[1],vp->partp->bsize,vp->partp->bsyssize)<0){ /* block 1 not found? */
				/* Note: S3000 format should always have block 1 */
#if 1
				printf("S3000 volume directory block 1 missing, assuming S1000 volume\n");
				/* XXX assume S1000 */
				vp->type=AKAI_VOL_TYPE_S1000;
				/* number of volume directory entries */
				vp->fimax=AKAI_VOLDIR_ENTRIES_S1000HD;
				/* start of files */
				vp->file=&vp->dir.s1000hd.file[0];
#else
				fprintf(stderr,"S3000 volume directory block 1 missing\n");
				vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
				return -1;
#endif
			}else{
				/* S3000 */
				/* vp->dirblk[1] should be volume directory block 1 now */
				/* number of volume directory entries */
				vp->fimax=AKAI_VOLDIR_ENTRIES_S3000HD; /* block 0 and 1 */
				/* start of files */
				vp->file=&vp->dir.s3000hd.file[0];
			}
		}
		
		/* volume parameters */
		if (vp->type==AKAI_VOL_TYPE_S1000){
			/* S1000 harddisk */
			vp->param=&vp->dir.s1000hd.param;
		}else{
			/* S3000 harddisk */
			vp->param=&vp->dir.s3000hd.param;
		}
		
		/* read volume directory */
		if (akai_read_voldir(vp)<0){
			vp->type=AKAI_VOL_TYPE_INACT; /* XXX mark invalid */
			return -1;
		}
		
		return 0;
	}

	return -1;
}



/* find first volume with given name */
/* saves result in *vp */
int
akai_find_vol(struct part_s *pp,struct vol_s *vp,char *name)
{
	u_int osver;
	u_int vi;
	u_int ai,aivifound;
	char namebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	char vnamebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	u_int i,j,l;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}
	if (vp==NULL){
		return -1;
	}
	if (name==NULL){
		return -1;
	}

	/* fix name */
	l=(u_int)strlen(name);
	if (l>AKAI_NAME_LEN){ /* too long for namebuf? */
		return -1;
	}
	for (i=0;i<l;i++){
		if (name[i]=='_'){ /* for keyboard entry */
			namebuf[i]=' ';
		}else{
			namebuf[i]=name[i];
		}
	}
	namebuf[i]='\0';
	/* remove trailing ' ' (XXX don't care how many) */
	for (i=0;i<l;i++){
		if (namebuf[l-i-1]==' '){
			namebuf[l-i-1]='\0';
		}else{
			break;
		}
	}

	/* volumes in partition */
	aivifound=pp->volnummax; /* invalid */
	ai=0;
	for (vi=0;vi<pp->volnummax;vi++){
		if (pp->type==PART_TYPE_FLL){
			/* OS version in floppy header */
			osver=(pp->head.fll.label.osver[1]<<8)+pp->head.fll.label.osver[0];
			/* volume name in floppy header */
			akai2ascii_name(pp->head.fll.label.name,vnamebuf,osver==AKAI_OSVER_S900VOL);
		}else if (pp->type==PART_TYPE_FLH){
			/* OS version in floppy header */
			osver=(pp->head.flh.label.osver[1]<<8)+pp->head.flh.label.osver[0];
			/* volume name in floppy header */
			akai2ascii_name(pp->head.flh.label.name,vnamebuf,osver==AKAI_OSVER_S900VOL);
		}else if (pp->type==PART_TYPE_HD9){
			/* S900 harddisk */
			j=vp->dirblk[0]=(pp->head.hd9.vol[vi].start[1]<<8)
				+pp->head.hd9.vol[vi].start[0];
			if (j==AKAI_VOL_START_INACT){ /* inactive? */
				continue; /* next */
			}
			/* volume name in root directory entry */
			akai2ascii_name(pp->head.hd9.vol[vi].name,vnamebuf,1); /* 1: S900 */
		}else if (pp->type==PART_TYPE_HD){
			/* S1000/S3000 harddisk sampler partition */
			if (pp->head.hd.vol[vi].type==AKAI_VOL_TYPE_INACT){ /* inactive? */
				continue; /* next */
			}
			/* volume name in root directory */
			akai2ascii_name(pp->head.hd.vol[vi].name,vnamebuf,0); /* 0: not S900 */
		}else{
			continue; /* next */
		}
		ai++;

		if (vnamebuf[0]=='\0'){ /* empty volume name? */
			/* use default volume name as fake volume name */
			sprintf(vnamebuf,"VOLUME %03u",vi+1);
		}

		/* compare name (case-insensitive) */
		if (strcasecmp(vnamebuf,namebuf)==0){ /* match? */
			break; /* found */
		}

		if ((pp->type==PART_TYPE_HD9)&&(aivifound>=pp->volnummax)){ /* S900 harddisk and no alias name match yet? */
			/* alias name for volume on S900 harddisk */
			sprintf(vnamebuf,"V%u",ai);
			/* compare name (case-insensitive) */
			if (strcasecmp(vnamebuf,namebuf)==0){ /* match? */
				/* Note: name match takes precendence over alias name match */
				aivifound=vi; /* save for later */
			}
		}
	}
	if (vi==pp->volnummax){ /* no name match? */
		if ((pp->type==PART_TYPE_HD9)&&(aivifound<pp->volnummax)){ /* S900 harddisk and alias name match? */
			vi=aivifound;
		}else{
			return -1;
		}
	}

	/* get volume */
	return akai_get_vol(pp,vp,vi);
}



int
akai_rename_vol(struct vol_s *vp,char *name,u_int lnum,u_int osver,struct akai_volparam_s *parp)
{
	int modflag;
	u_int blk;
	u_char *addr;
	struct akai_flvol_label_s *lp;
	u_int hdsiz;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)){
		return -1;
	}

	if ((vp->partp->type==PART_TYPE_FLL)||(vp->partp->type==PART_TYPE_FLH)){
		/* floppy */

		modflag=0; /* floppy header not modified yet */

		/* floppy volume label */
		if (vp->partp->type==PART_TYPE_FLL){
			lp=&vp->partp->head.fll.label;
			hdsiz=AKAI_FLLHEAD_BLKS;
		}else{
			lp=&vp->partp->head.flh.label;
			hdsiz=AKAI_FLHHEAD_BLKS;
		}

		/* Note: no load number on floppy */

		/* Note: cannot switch volume type S900/S1000/S3000 */
		if (vp->type==AKAI_VOL_TYPE_S900){
			/* S900 floppy */
			/* correct OS version if necessary */
			osver=AKAI_OSVER_S900VOL;
			if (osver!=vp->osver){ /* need to modify? */
				vp->osver=osver;
				/* Note: S900 has no floppy volume label, all zero */
				bzero(lp,sizeof(struct akai_flvol_label_s));
				modflag=1;
			}
		}else{
			/* S1000/S300 floppy */
			/* correct OS version if necessary */
			if (vp->type==AKAI_VOL_TYPE_S1000){
				/* S1000 floppy */
				if ((osver==AKAI_OSVER_S900VOL)||(osver>AKAI_OSVER_S1100MAX)){
					osver=AKAI_OSVER_S1000MAX; /* XXX */
				}
			}else{
				/* S3000 floppy */
				if ((osver==AKAI_OSVER_S900VOL)||(osver>AKAI_OSVER_S3000MAX)){
					osver=AKAI_OSVER_S3000MAX; /* XXX */
				}
			}
			/* OS version */
			if (osver!=vp->osver){ /* new OS version? */
				vp->osver=osver;
				lp->osver[1]=0xff&(osver>>8);
				lp->osver[0]=0xff&osver;
				modflag=1;
			}

			/* name */
			if (name!=NULL){ /* new name? */
				if (strlen(name)>(u_int)AKAI_NAME_LEN){
					fprintf(stderr,"invalid name length\n");
					return -1;
				}
				if (name!=vp->name){ /* not already in vp->name? */
					/* copy */
					strcpy(vp->name,name); /* Note: name has been checked above */
				}
				ascii2akai_name(name,lp->name,0); /* 0: not S900 */
				modflag=1;
			}

			/* volume parameters */
			if ((parp!=NULL)&&(vp->param!=NULL)){ /* new volume parameters? */
				if (parp!=vp->param){ /* not already vp->param? */
					/* copy */
					bcopy(parp,vp->param,sizeof(struct akai_volparam_s));
				} /* else: vp->param[] possibly modified */
				modflag=1;
			}
		}

		if (modflag){
			/* write floppy header */
			if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.flh,
							 0,
							 hdsiz,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
		
		return 0;
	}
	
	if (vp->partp->type==PART_TYPE_HD9){
		/* S900 harddisk */

		/* set vol_s */
		modflag=0; /* root directory not modified yet */
		
		/* Note: no load number on S900 harddisk */

		/* correct type and OS version */
		vp->type=AKAI_VOL_TYPE_S900;
		vp->osver=AKAI_OSVER_S900VOL;
		/* Note: no floppy volume labels for volumes on harddisk => cannot save OS version */

		/* name */
		if (name!=NULL){
			if (strlen(name)>AKAI_NAME_LEN_S900){
				fprintf(stderr,"invalid name length\n");
				return -1;
			}
			if (name!=vp->name){ /* not already in vp->name? */
				/* copy */
				strcpy(vp->name,name); /* Note: name has been checked above */
			}
			/* change root directory entry in partition */
			ascii2akai_name(name,vp->partp->head.hd9.vol[vp->index].name,1); /* 1: S900 */
			modflag=1;
		}
		
		if (modflag){
			/* write new root directory to partition */
			/* copy first FAT entries */
			bcopy((u_char *)&vp->partp->head.hd9.fatblk,(u_char *)&vp->partp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
			/* write harddisk header */
			if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd9,
							 0,
							 AKAI_HD9HEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
		
		/* Note: no volume parameters on S900 harddisk */
		
		return 0;
	}

	if (vp->partp->type==PART_TYPE_HD){
		/* S1000/S3000 harddisk sampler partition */

		/* set vol_s */
		modflag=0; /* root directory not modified yet */
		
		/* load number */
		if (lnum!=vp->lnum){ /* need to modify? */
			if ((lnum!=AKAI_VOL_LNUM_OFF)
				&&((lnum<AKAI_VOL_LNUM_MIN)||(lnum>AKAI_VOL_LNUM_MAX))){
				return -1;
			}
			vp->lnum=lnum;
			/* change root directory entry in partition */
			vp->partp->head.hd.vol[vp->index].lnum=lnum;
			modflag=1;
		}

		/* correct type and OS version */
		if ((vp->type==AKAI_VOL_TYPE_S900)||(vp->osver==AKAI_OSVER_S900VOL)){ /* incorrect for partition type? */
			vp->type=AKAI_VOL_TYPE_S1000; /* XXX */
			vp->osver=AKAI_OSVER_S1000MAX; /* XXX */
		}
		/* Note: no floppy volume labels for volumes on harddisk => cannot save OS version */

		/* name */
		if (name!=NULL){
			if (strlen(name)>AKAI_NAME_LEN){
				fprintf(stderr,"invalid name length\n");
				return -1;
			}
			if (name!=vp->name){ /* not already in vp->name? */
				/* copy */
				strcpy(vp->name,name); /* Note: name has been checked above */
			}
			/* change root directory entry in partition */
			ascii2akai_name(name,vp->partp->head.hd.vol[vp->index].name,0); /* 0: not S900 */
			modflag=1;
		}
		
		if (modflag){
			/* write new root directory to partition */
			/* write partition header */
			if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd,
							 0,
							 AKAI_PARTHEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
		
		/* volume parameters */
		if ((parp!=NULL)&&(vp->param!=NULL)){
			if (parp!=vp->param){ /* not already vp->param? */
				/* copy */
				bcopy(parp,vp->param,sizeof(struct akai_volparam_s));
			} /* else: vp->param[] possibly modified */
			
			/* get volume directory block of volume parameters */
			if ((vp->type==AKAI_VOL_TYPE_S3000)||(vp->type==AKAI_VOL_TYPE_CD3000)){
				blk=vp->dirblk[1];
				addr=((u_char *)&vp->dir)+AKAI_HD_BLOCKSIZE;
			}else{
				blk=vp->dirblk[0];
				addr=(u_char *)&vp->dir;
			}
			
			/* write new volume directory block to partition */
			if (akai_io_blks(vp->partp,addr,
							 blk,
							 1,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
		
		return 0;
	}

	return -1;
}



/* uses *vp as output */
int
akai_create_vol(struct part_s *pp,struct vol_s *vp,u_int type,u_int index,char *name,u_int lnum,struct akai_volparam_s *parp)
{
	u_char *addr;
	u_int i,j;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (vp==NULL){
		return -1;
	}

	if ((pp->type!=PART_TYPE_HD9)&&(pp->type!=PART_TYPE_HD)){
		fprintf(stderr,"cannot create additional volume for this partition type\n");
		return -1;
	}
	/* now, harddisk */

	/* partition */
	vp->partp=pp;

	/* type */
	if (pp->type==PART_TYPE_HD9){
		/* S900 */
		if (type!=AKAI_VOL_TYPE_S900){
			fprintf(stderr,"cannot create this volume type for this partition type\n");
			return -1;
		}
	}else if (pp->type==PART_TYPE_HD){
		/* S1000/S3000 */
		if ((type!=AKAI_VOL_TYPE_S1000)&&(type!=AKAI_VOL_TYPE_S3000)&&(type!=AKAI_VOL_TYPE_CD3000)){
			fprintf(stderr,"cannot create this volume type for this partition type\n");
			return -1;
		}
	}
	vp->type=type;

	/* load number */
	if (pp->type==PART_TYPE_HD9){
		/* S900 */
		vp->lnum=AKAI_VOL_LNUM_OFF;
	}else{
		/* S1000/S3000 */
		if ((lnum!=AKAI_VOL_LNUM_OFF)
			&&((lnum<AKAI_VOL_LNUM_MIN)||(lnum>AKAI_VOL_LNUM_MAX))){
			return -1;
		}
		vp->lnum=lnum;
	}

	/* OS version */
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		vp->osver=AKAI_OSVER_S900VOL;
	}else if (vp->type==AKAI_VOL_TYPE_S1000){
		/* S1000 */
		vp->osver=AKAI_OSVER_S1000MAX; /* XXX */
	}else{
		/* S3000 */
		vp->osver=AKAI_OSVER_S3000MAX; /* XXX */
	}

	/* name */
	if ((name!=NULL)&&(name[0]!='\0')){ /* volume name given? */
		if (strlen(name)>(u_int)((vp->type==AKAI_VOL_TYPE_S900)?AKAI_NAME_LEN_S900:AKAI_NAME_LEN)){
			fprintf(stderr,"invalid name length\n");
			return -1;
		}
		strcpy(vp->name,name);
	}

	/* index */
	if (index==AKAI_CREATE_VOL_NOINDEX){ /* no user-supplied volume index? */
		/* find inactive entry in root directory in partition */
		for (i=0;i<pp->volnummax;i++){
			if (pp->type==PART_TYPE_HD9){
				/* S900 */
				j=vp->dirblk[0]=(vp->partp->head.hd9.vol[i].start[1]<<8)
					+vp->partp->head.hd9.vol[i].start[0];
				if (j==AKAI_VOL_START_INACT){ /* inactive? */
					/* index */
					vp->index=i;
					break; /* found one */
				}
			}else{
				/* S1000/S3000 */
				if (vp->partp->head.hd.vol[i].type==AKAI_VOL_TYPE_INACT){ /* inactive? */
					/* index */
					vp->index=i;
					break; /* found one */
				}
			}
		}
		if (i==pp->volnummax){ /* none found? */
			printf("no inactive volume found");
			return -1;
		}
	}else{
		/* user-supplied index */
		if (index>=pp->volnummax){
			return -1;
		}
		if (pp->type==PART_TYPE_HD9){
			/* S900 */
			j=vp->dirblk[0]=(vp->partp->head.hd9.vol[index].start[1]<<8)
				+vp->partp->head.hd9.vol[index].start[0];
			if (j!=AKAI_VOL_START_INACT){ /* not inactive? */
				printf("volume not inactive\n");
				return -1;
			}
		}else{
			/* S1000/S3000 */
			if (vp->partp->head.hd.vol[index].type!=AKAI_VOL_TYPE_INACT){ /* not inactive? */
				printf("volume not inactive\n");
				return -1;
			}
		}
		/* index */
		vp->index=index;
	}

	if ((name==NULL)||(name[0]=='\0')){ /* no volume name given? */
		/* default volume name */
		sprintf(vp->name,"VOLUME %03u",vp->index+1);
	}

	/* allocate volume directory */
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		if (akai_allocate_fatchain(vp->partp,
									AKAI_VOLDIR900HD_BLKS,
									&vp->dirblk[0],
									AKAI_VOLDIR900HD_BLKS,
									AKAI_FAT_CODE_DIREND900HD)<0){ /* end code */
			return -1;
		}
		/* number of volume directory entries */
		vp->fimax=AKAI_VOLDIR_ENTRIES_S900HD;
		/* start of files */
		vp->file=&vp->dir.s900hd.file[0];
	}else if (vp->type==AKAI_VOL_TYPE_S1000){
		/* S1000 */
		if (akai_allocate_fatchain(vp->partp,
									AKAI_VOLDIR1000HD_BLKS,
									&vp->dirblk[0],
									AKAI_VOLDIR1000HD_BLKS,
									AKAI_FAT_CODE_DIREND1000HD)<0){ /* end code */
									return -1;
		}
		/* number of volume directory entries */
		vp->fimax=AKAI_VOLDIR_ENTRIES_S1000HD;
		/* start of files */
		vp->file=&vp->dir.s1000hd.file[0];
	}else{
		/* S3000 */
		if (akai_allocate_fatchain(vp->partp,
									AKAI_VOLDIR3000HD_BLKS,
									&vp->dirblk[0],
									AKAI_VOLDIR3000HD_BLKS, /* XXX want contiguous blocks */
									AKAI_FAT_CODE_DIREND3000)<0){ /* end code */
			return -1;
		}
		/* number of volume directory entries */
		vp->fimax=AKAI_VOLDIR_ENTRIES_S3000HD;
		/* start of files */
		vp->file=&vp->dir.s3000hd.file[0];
	}

	/* initialize volume directory */
	bzero(&vp->dir,sizeof(union akai_voldir_u));
	for (i=0;i<vp->fimax;i++){
		vp->file[i].type=AKAI_FTYPE_FREE;
	}

	/* volume parameters */
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		vp->param=NULL; /* S900 has no volume paramaters */
	}else if (vp->type==AKAI_VOL_TYPE_S1000){
		/* S1000 */
		vp->param=&vp->dir.s1000hd.param;
	}else{
		/* S3000 */
		vp->param=&vp->dir.s3000hd.param;
	}
	if (vp->param!=NULL){
		if (parp!=NULL){
			/* copy volume parameters */
			bcopy(parp,vp->param,sizeof(struct akai_volparam_s));
		}else{
			/* default volume parameters */
			akai_init_volparam(vp->param,(vp->type==AKAI_VOL_TYPE_S3000)||(vp->type==AKAI_VOL_TYPE_CD3000));
		}
	}

	/* write new volume directory block 0 to partition */
	addr=(u_char *)&vp->dir;
	if (akai_io_blks(vp->partp,addr,
					 vp->dirblk[0],
					 1,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return -1;
	}

	if (vp->fimax>AKAI_VOLDIR_ENTRIES_1BLKHD){ /* second volume directory block? */
		/* block 1 from FAT */
		vp->dirblk[1]=(vp->partp->fat[vp->dirblk[0]][1]<<8)
					  +vp->partp->fat[vp->dirblk[0]][0];
		/* write new volume directory block 1 to partition */
		addr+=AKAI_HD_BLOCKSIZE;
		if (akai_io_blks(vp->partp,addr,
						 vp->dirblk[1],
						 1,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}

	/* update root directory entry in partition */
	if (pp->type==PART_TYPE_HD9){
		/* S900 */
		/* Note: no OS version nor load number on S900 harddisk */
		ascii2akai_name(vp->name,vp->partp->head.hd9.vol[vp->index].name,1); /* 1: S900 */
		/* start */
		vp->partp->head.hd9.vol[vp->index].start[1]=0xff&(vp->dirblk[0]>>8);
		vp->partp->head.hd9.vol[vp->index].start[0]=0xff&vp->dirblk[0];

		/* write new root directory to harddisk */
		/* copy first FAT entries */
		bcopy((u_char *)&vp->partp->head.hd9.fatblk,(u_char *)&vp->partp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
		/* write harddisk header */
		if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd9,
						 0,
						 AKAI_HD9HEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}else{
		/* S1000/S3000 */
		vp->partp->head.hd.vol[vp->index].type=(u_char)vp->type;
		/* Note: no floppy volume labels for volumes on harddisk => cannot write OS version */
		/*       akai_get_vol() will derive OS version from volume type in root directory entry */
		vp->partp->head.hd.vol[vp->index].lnum=(u_char)vp->lnum;
		ascii2akai_name(vp->name,vp->partp->head.hd.vol[vp->index].name,0); /* 0: not S900 */
		/* start */
		vp->partp->head.hd.vol[vp->index].start[1]=0xff&(vp->dirblk[0]>>8);
		vp->partp->head.hd.vol[vp->index].start[0]=0xff&vp->dirblk[0];

		/* write new root directory to partition */
		/* write partition header */
		if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd,
						 0,
						 AKAI_PARTHEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}

	return 0;
}



int
akai_wipe_vol(struct vol_s *vp,int delflag)
{
	u_int fi;
	struct file_s tmpfile;
	u_int bc;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)||(vp->partp->fat==NULL)){
		return -1;
	}

	if (delflag&&(vp->partp->type!=PART_TYPE_HD9)&&(vp->partp->type!=PART_TYPE_HD)){
		fprintf(stderr,"cannot remove floppy volume\n");
		return -1;
	}

	/* delete all files in volume */
	for (fi=0;fi<vp->fimax;fi++){
		if (akai_get_file(vp,&tmpfile,fi)<0){
			continue; /* next file */
		}
		if (akai_delete_file(&tmpfile)<0){
			return -1;
		}
	}

	if (delflag){
		/* now, harddisk */

		/* free FAT block(s) of volume */
		vp->partp->fat[vp->dirblk[0]][1]=0xff&(AKAI_FAT_CODE_FREE>>8);
		vp->partp->fat[vp->dirblk[0]][0]=0xff&AKAI_FAT_CODE_FREE;
		bc=1;
		if ((vp->fimax>AKAI_VOLDIR_ENTRIES_1BLKHD)
			&&(akai_check_fatblk(vp->dirblk[1],vp->partp->bsize,vp->partp->bsyssize)==0)){ /* also block 1? */
			vp->partp->fat[vp->dirblk[1]][1]=0xff&(AKAI_FAT_CODE_FREE>>8);
			vp->partp->fat[vp->dirblk[1]][0]=0xff&AKAI_FAT_CODE_FREE;
			bc++;
		}
		/* update free block counter */
		vp->partp->bfree+=bc;

		/* free root directory entry in partition */
		if (vp->partp->type==PART_TYPE_HD9){
			/* S900 */
			vp->partp->head.hd9.vol[vp->index].start[1]=0xff&(AKAI_VOL_START_INACT>>8);
			vp->partp->head.hd9.vol[vp->index].start[0]=0xff&AKAI_VOL_START_INACT;
			/* XXX keep name */

			/* write new FAT and root directory to partition */
			/* copy first FAT entries */
			bcopy((u_char *)&vp->partp->head.hd9.fatblk,(u_char *)&vp->partp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
			/* write harddisk header */
			if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd9,
							 0,
							 AKAI_HD9HEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}else{
			/* S1000/S3000 */
			vp->partp->head.hd.vol[vp->index].type=AKAI_VOL_TYPE_INACT;
			/* XXX keep name etc. */

			/* write new FAT and root directory to partition */
			/* write partition header */
			if (akai_io_blks(vp->partp,(u_char *)&vp->partp->head.hd,
							 0,
							 AKAI_PARTHEAD_BLKS,
							 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
				return -1;
			}
		}
	}

	return 0;
}



void
akai_list_volparam(struct akai_volparam_s *parp)
{
	u_int i;

	if (parp==NULL){
		return;
	}

	printf("idx   val\n---------\n");
	for (i=0;i<(int)sizeof(struct akai_volparam_s);i++){
		/* XXX */
		printf("%3u  0x%02x\n",i,parp->dummy1[i]);
	}
	printf("---------\n");
}

void
akai_init_volparam(struct akai_volparam_s *parp,int s3000flag)
{

	if (parp==NULL){
		return;
	}

	/* default volume parameters */
	bzero(parp,sizeof(struct akai_volparam_s));
	if (s3000flag){
		/* S3000 */
		parp->dummy1[1]=0x01; /* XXX */
		parp->dummy1[2]=0x01; /* XXX */
		parp->dummy1[6]=0x32; /* XXX */
		parp->dummy1[7]=0x09; /* XXX */
		parp->dummy1[8]=0x0c; /* XXX */
		parp->dummy1[9]=0xff; /* XXX */
	}else{
		/* S1000 */
		parp->dummy1[1]=0x01; /* XXX */
		parp->dummy1[2]=0x01; /* XXX */
	}
}



void
akai_print_lnum(u_int lnum)
{

	if (lnum==AKAI_VOL_LNUM_OFF){
		printf("-  ");
	}else{
		printf("%03u",lnum);
	}
}

u_int
akai_get_lnum(char *name)
{
	u_int lnum;

	if (name==NULL){
		return AKAI_VOL_LNUM_OFF;
	}

	if (strcasecmp(name,"OFF")==0){
		return AKAI_VOL_LNUM_OFF;
	}

	lnum=(u_int)atoi(name);
	if ((lnum<AKAI_VOL_LNUM_MIN)||(lnum>AKAI_VOL_LNUM_MAX)){
		fprintf(stderr,"invalid load number, setting OFF\n");
		return AKAI_VOL_LNUM_OFF;
	}

	return lnum;
}



void
akai_list_tags(struct part_s *pp)
{
	u_int i;
	char namebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)){
		return;
	}

	if (pp->type!=PART_TYPE_HD){
		printf("no tags in this partition type\n");
		return;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	/* check magic */
	if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)pp->head.hd.tagsmagic,4)!=0){
		printf("no tags present\n");
		return;
	}

	printf("tag  name\n");
	printf("-----------------\n");
	for (i=0;i<AKAI_PARTHEAD_TAGNUM;i++){
		/* Note: tag names use S1000/S3000 char conversion */
		akai2ascii_name(pp->head.hd.tag[i],namebuf,0); /* 0: not S900 */
		printf(" %02u  %s\n",i+1,namebuf); /* XXX default tag name */
	}
	printf("-----------------\n");
}



int
akai_rename_tag(struct part_s *pp,char *name,u_int ti,int wipeflag)
{
	u_int i;
	char namebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)){
		return -1;
	}

	if (pp->type!=PART_TYPE_HD){
		printf("no tags in this partition type\n");
		return -1;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	if (name!=NULL){
		if ((strlen(name)>AKAI_NAME_LEN)||(ti>=AKAI_PARTHEAD_TAGNUM)){
			return -1;
		}
	}

	if (wipeflag){
		bcopy(AKAI_PARTHEAD_TAGSMAGIC,pp->head.hd.tagsmagic,4); /* magic */
		for (i=0;i<AKAI_PARTHEAD_TAGNUM;i++){
			sprintf(namebuf,"TAG %c       ",(char)('A'+i)); /* XXX default tag name */
			/* Note: tag names use S1000/S3000 char conversion */
			ascii2akai_name(namebuf,pp->head.hd.tag[i],0); /* 0: not S900 */
		}
	}

	if (name!=NULL){
		/* check magic */
		if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)pp->head.hd.tagsmagic,4)!=0){
			printf("no tags present\n");
			return -1;
		}

		/* set tag */
		/* Note: tag names use S1000/S3000 char conversion */
		ascii2akai_name(name,pp->head.hd.tag[ti],0); /* 0: not S900 */
	}

	/* write new tag(s) to partition */
	/* write partition header */
	return akai_io_blks(pp,(u_char *)&pp->head.hd,
						0,
						AKAI_PARTHEAD_BLKS,
						1,IO_BLKS_WRITE); /* 1: allocate cache if possible */
}



int
copy_tags(struct part_s *srcpp,struct part_s *dstpp)
{

	if ((srcpp==NULL)||(!srcpp->valid)||(srcpp->fd<0)){
		return -1;
	}
	if ((dstpp==NULL)||(!dstpp->valid)||(dstpp->fd<0)){
		return -1;
	}

	if ((srcpp->type!=PART_TYPE_HD)||(dstpp->type!=PART_TYPE_HD)){
		printf("no tags in this partition type\n");
		return -1;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	/* copy tags */
	bcopy(srcpp->head.hd.tagsmagic,dstpp->head.hd.tagsmagic,4); /* magic */
	bcopy(srcpp->head.hd.tag[0],dstpp->head.hd.tag[0],
		AKAI_PARTHEAD_TAGNUM*AKAI_NAME_LEN); /* tag names */

	/* write new tag(s) to partition */
	/* write partition header */
	return akai_io_blks(dstpp,(u_char *)&dstpp->head.hd,
						0,
						AKAI_PARTHEAD_BLKS,
						1,IO_BLKS_WRITE);/* 1: allocate cache if possible */
}



void
akai_print_cdinfo(struct part_s *pp,int verbose)
{
	u_int c;
	u_int i;
	u_int fnum;
	u_int bn;
	char namebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	char fnamebuf[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */
	u_char buf[AKAI_HD_BLOCKSIZE];
	struct akai_cdinfohead_s *hp;
	struct akai_voldir_entry_s *dep;
	u_char *buf2;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return;
	}

	if (pp->type!=PART_TYPE_HD){
		printf("no CD-ROM info in this partition type\n");
		return;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	/* check if S3000 with CD-ROM info: */
	/* check if tags magic */
	if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)pp->head.hd.tagsmagic,4)!=0){
		printf("no CD-ROM info in this partition\n");
		return;
	}
	/* check if at least minimum number of blocks for CD-ROM info have FAT code SYS */
	/* Note: S1000 volume also uses SYS for volume directory */
	for (i=AKAI_CDINFO_BLK;i<AKAI_CDINFO_BLK+AKAI_CDINFO_MINSIZB;i++){
		c=(pp->fat[i][1]<<8)+pp->fat[i][0];
		if (c!=AKAI_FAT_CODE_SYS){
			printf("no CD-ROM info in this partition\n");
			return;
		}
	}

	/* read CD-ROM info header */
	if (akai_io_blks(pp,buf,
					 AKAI_CDINFO_BLK,
					 1,
					 0,IO_BLKS_READ)<0){ /* 0: don't allocate new cache */
		fprintf(stderr,"cannot read CD-ROM info\n");
		return;
	}

	/* CD-ROM info header */
	hp=(struct akai_cdinfohead_s *)buf;

	/* CD-ROM label */
	/* Note: CD-ROM label uses S1000/S3000 char conversion */
	akai2ascii_name(hp->cdlabel,namebuf,0); /* 0: not S900 */
	printf("CD-ROM label: \"%s\"\n",namebuf);

	/* number of files */
	fnum=(hp->fnum[1]<<8)+hp->fnum[0];
	printf("\nnumber of files in partition %c: %u\n",pp->letter,fnum);
	/* required number of blocks */
	bn=sizeof(struct akai_cdinfohead_s)+fnum*sizeof(struct akai_voldir_entry_s);
	bn=(bn+AKAI_HD_BLOCKSIZE-1)/AKAI_HD_BLOCKSIZE; /* round up */
	if (AKAI_CDINFO_BLK+bn>pp->bsize){
		fprintf(stderr,"invalid number of files for partition size\n");
		return;
	}
	/* check if all required blocks for CD-ROM info have FAT code SYS */
	/* Note: S1000 volume also uses SYS for volume directory */
	for (i=1;i<bn;i++){
		c=(pp->fat[AKAI_CDINFO_BLK+i][1]<<8)
			+pp->fat[AKAI_CDINFO_BLK+i][0];
		if (c!=AKAI_FAT_CODE_SYS){
			/* truncate number of blocks */
			bn=i;
			/* truncate number of files */
			fnum=((bn*AKAI_HD_BLOCKSIZE)-sizeof(struct akai_cdinfohead_s))/sizeof(struct akai_voldir_entry_s);
			printf("CD-ROM info is incomplete, truncated to %u files\n",fnum);
			break;
		}
	}

	if (verbose){
		/* print volume sizes */
		printf("\nvolume files\n------------\n");
		for (i=0;i<pp->volnummax;i++){
			c=(hp->volesiz[i][1]<<8)+hp->volesiz[i][0]; /* size of volume directory entries for files in bytes */
			if (c!=0){ /* used? */
				c/=sizeof(struct akai_voldir_entry_s); /* number of file entries */
				printf("%03u      %3u\n",i+1,c);
			}
		}
		printf("------------\n");

		if ((buf2=malloc(bn*AKAI_HD_BLOCKSIZE))==NULL){
			perror("malloc");
			return;
		}

		/* read directory of files from CD-ROM info */
		if (akai_io_blks(pp,buf2,
						 AKAI_CDINFO_BLK,
						 bn,
						 0,IO_BLKS_READ)<0){ /* 0: don't allocate new cache */
			fprintf(stderr,"cannot read CD-ROM directory of files\n");
			free(buf2);
			return;
		}

		/* print directory of files */
		printf("\ndirectory of files:\n---------------------------------------------------------------------\n");
		for (i=0;i<fnum;i++){
			dep=(struct akai_voldir_entry_s *)(buf2+sizeof(struct akai_cdinfohead_s));
			/* name */
			/* Note: file names in CD-ROM info use S1000/S3000 char conversion */
			akai2ascii_filename(dep[i].name,dep[i].type,AKAI_OSVER_S3000MAX,fnamebuf,0); /* 0: not S900 */
			printf("%-16s  ",fnamebuf);
			if (i%4==3){
				printf("\n");
			}
		}
		if (i%4!=0){
			printf("\n");
		}
		printf("---------------------------------------------------------------------\n");

		free(buf2);
	}
}



int
akai_set_cdinfo(struct part_s *pp,char *cdlabel)
{
	u_int c;
	u_int i,j;
	u_int fnum,fnummax;
	u_int vsiz;
	static u_char buf[AKAI_CDINFO_MINSIZB*AKAI_HD_BLOCKSIZE]; /* XXX minimum size */
	u_char *bp;
	struct akai_cdinfohead_s *hp;
	struct vol_s vol;
	struct file_s file;
	u_char oldcdlabel[AKAI_NAME_LEN];

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}

	if (pp->type!=PART_TYPE_HD){
		printf("no CD-ROM info in this partition type\n");
		return -1;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	/* check if S3000 with CD-ROM info: */
	/* check if tags magic */
	if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)pp->head.hd.tagsmagic,4)!=0){
		printf("no CD-ROM info in this partition\n");
		return -1;
	}
	/* check if at least minimum number of blocks for CD-ROM info have FAT code SYS */
	/* Note: S1000 volume also uses SYS for volume directory */
	for (i=AKAI_CDINFO_BLK;i<AKAI_CDINFO_BLK+AKAI_CDINFO_MINSIZB;i++){
		c=(pp->fat[i][1]<<8)+pp->fat[i][0];
		if (c!=AKAI_FAT_CODE_SYS){
			printf("no CD-ROM info in this partition\n");
			return -1;
		}
	}

	/* CD-ROM info header */
	hp=(struct akai_cdinfohead_s *)buf;

	if (cdlabel==NULL){
		/* read old CD-ROM info */
		if (akai_io_blks(pp,buf,
						 AKAI_CDINFO_BLK,
						 1, /* XXX 1 block is enough for header */
						 0,IO_BLKS_READ)<0){ /* 0: don't allocate new cache */
			fprintf(stderr,"cannot read CD-ROM info\n");
			return -1;
		}
		/* save old label */
		bcopy(hp->cdlabel,oldcdlabel,AKAI_NAME_LEN);
	}

	/* wipe CD-ROM info */
	bzero(buf,AKAI_CDINFO_MINSIZB*AKAI_HD_BLOCKSIZE);

	/* max. number of files in CD-ROM info */
	/* XXX minimum size */
	fnummax=((AKAI_CDINFO_MINSIZB*AKAI_HD_BLOCKSIZE)-sizeof(struct akai_cdinfohead_s))/sizeof(struct akai_voldir_entry_s);

	/* read all volumes in partition */
	fnum=0;
	bp=buf+sizeof(struct akai_cdinfohead_s); /* behind header */
	for (i=0;i<pp->volnummax;i++){
		vsiz=0;
		if (akai_get_vol(pp,&vol,i)<0){
			continue; /* ignore error, next */
		}
		/* check all files in volume */
		for (j=0;j<vol.fimax;j++){
			if (akai_get_file(&vol,&file,j)<0){
				continue; /* ignore error, next */
			}
			/* copy volume entry */
			bcopy(&vol.file[j],bp,sizeof(struct akai_voldir_entry_s));
			bp+=sizeof(struct akai_voldir_entry_s);
			vsiz+=sizeof(struct akai_voldir_entry_s);
			fnum++;
			if (fnum>=fnummax){
				break; /* exit, ignore rest */
			}
		}
		/* size of volume directory entries for files in bytes */
		hp->volesiz[i][1]=0xff&(vsiz>>8);
		hp->volesiz[i][0]=0xff&vsiz;
		if (fnum>=fnummax){
			break; /* exit, ignore rest */
		}
	}
	/* total number of files */
	hp->fnum[1]=0xff&(fnum>>8);
	hp->fnum[0]=0xff&fnum;

	/* CD-ROM label */
	if (cdlabel!=NULL){
		/* Note: CD-ROM label uses S1000/S3000 char conversion */
		ascii2akai_name(cdlabel,hp->cdlabel,0); /* 0: not S900 */
	}else{
		/* restore old label */
		bcopy(oldcdlabel,hp->cdlabel,AKAI_NAME_LEN);
	}

	/* write CD-ROM info */
	if (akai_io_blks(pp,buf,
					 AKAI_CDINFO_BLK,
					 AKAI_CDINFO_MINSIZB,
					 0,IO_BLKS_WRITE)<0){ /* 0: don't allocate new cache */
		fprintf(stderr,"cannot write CD-ROM info\n");
		return -1;
	}

	return 0;
}



void
akai_part_info(struct part_s *pp,int verbose)
{

	if (pp==NULL){
		return;
	}

	if (verbose){
		printf("part:      ");
		if (pp->type==PART_TYPE_DD){
			/* harddisk DD partition */
			if (pp->letter==0){
				printf("DD\n");
			}else{
				printf("DD%u\n",(u_int)pp->letter);
			}
		}else{
			/* floppy or harddisk sampler partition */
			printf("%c\n",pp->letter);
		}
		if (!pp->valid){
			printf("*** marked invalid ***\n");
		}
		printf("type:      ");
		switch (pp->type){
		case PART_TYPE_FLL:
			printf("low-density floppy");
			break;
		case PART_TYPE_FLH:
			printf("high-density floppy");
			break;
		case PART_TYPE_HD9:
			printf("S900 harddisk");
			break;
		case PART_TYPE_HD:
			printf("S1000/S3000 harddisk sampler partition");
			break;
		case PART_TYPE_DD:
			printf("S1100/S3000 harddisk DD parttion");
			break;
		default:
			printf("\?\?\?");
		}
		printf("\nstart:     block 0x%04x (byte 0x%08x)\n",
			pp->bstart,
			pp->bstart*pp->blksize);
		printf("size:      0x%04x blocks (%9u bytes)\n",
			pp->bsize,
			pp->bsize*pp->blksize);
		printf("free:      0x%04x blocks (%9u bytes) = %5.1lf%%\n",
			pp->bfree,
			pp->bfree*pp->blksize,
			(pp->bsize>0)?(100.0*((double)pp->bfree)/((double)pp->bsize)):0.0);
		if (pp->type!=PART_TYPE_DD){
		printf("max. vols: %u\n",pp->volnummax);
		}
	}else{
		if (pp->type==PART_TYPE_DD){
			/* harddisk DD partition */
			if (pp->letter==0){
				printf("  DD");
			}else if (pp->letter<=9){
				printf(" DD%u",(u_int)pp->letter);
			}else{
				printf("DD%u",(u_int)pp->letter);
			}
		}else{
			/* floppy or harddisk sampler partition */
			printf("   %c",pp->letter);
		}
		printf("%c  ",pp->valid?' ':'*');
		switch (pp->type){
		case PART_TYPE_FLL:
			printf("FLL");
			break;
		case PART_TYPE_FLH:
			printf("FLH");
			break;
		case PART_TYPE_HD9:
			printf("HD9");
			break;
		case PART_TYPE_HD:
			printf(" HD");
			break;
		case PART_TYPE_DD:
			printf(" DD");
			break;
		default:
			printf("\?\?\?");
		}
		printf("   0x%04x    0x%04x    %5.1lf     0x%04x   %5.1lf  %5.1lf\n",
			pp->bstart,
			pp->bsize,
			((double)pp->bsize*pp->blksize)/((double)1024*1024),
			pp->bfree,
			((double)pp->bfree*pp->blksize)/((double)1024*1024),
			(pp->bsize>0)?(100.0*((double)pp->bfree)/((double)pp->bsize)):0.0);
	}
}

void
akai_list_part(struct part_s *pp,int recflag,u_char *filtertagp)
{
	u_int vi;
	u_int ai;
	struct vol_s tmpvol;

	if ((pp==NULL)||(!pp->valid)
		||(pp->diskp==NULL)||(pp->fd<0)){
		return;
	}

	if (pp->type==PART_TYPE_DD){
		/* harddisk DD partition */
		u_int cstarts;
		u_int tott;
		u_int i;

		printf("/disk%u/DD",pp->diskp->index);
		if (pp->letter!=0){
			printf("%u",(u_int)pp->letter);
		}
		printf("\ntnr  tname         cstarts cstarte csizes  csizee     scount  srate  stype\n");
		printf("---------------------------------------------------------------------------\n");
		/* DD takes in partition */
		tott=0;
		for (i=0;i<AKAI_DDTAKE_MAXNUM;i++){
			cstarts=(pp->head.dd.take[i].cstarts[1]<<8)
				    +pp->head.dd.take[i].cstarts[0];
			if (cstarts==0){ /* empty? */
				continue; /* next */
			}
			tott++;
			/* print DD take info */
			akai_ddtake_info(pp,i,0);
		}
		printf("---------------------------------------------------------------------------\n");
		printf("total: %3u take(s)\n\n",tott);

		return;
	}

	/* now, floppy or harddisk sampler partition */

	printf("/disk%u/%c\n",pp->diskp->index,pp->letter);
	printf("vnr   vname         %s  startblk  osver  type\n",
		(pp->type==PART_TYPE_HD9)?"alias":"lnum");
	printf("---------------------------------------------------\n");
	/* volumes in partition */
	ai=0;
	for (vi=0;vi<pp->volnummax;vi++){
		/* get volume */
		if (akai_get_vol(pp,&tmpvol,vi)<0){
			continue; /* next volume */
		}
		ai++;
		/* print volume info */
		akai_vol_info(&tmpvol,ai,0);
	}
	printf("---------------------------------------------------\n");
	printf("total: %3u volume(s) (max. %3u)\n\n",ai,pp->volnummax);

	if (recflag){
		/* volumes in partition */
		ai=0;
		for (vi=0;vi<pp->volnummax;vi++){
			/* get volume */
			if (akai_get_vol(pp,&tmpvol,vi)<0){
				continue; /* next volume */
			}
			if (tmpvol.type!=AKAI_VOL_TYPE_INACT){
				/* list volume directory */
				akai_list_vol(&tmpvol,filtertagp);
			}
		}
	}
}



/* find partition with given name (letter) */
struct part_s *
akai_find_part(struct disk_s *dp,char *name)
{
	u_int p;
	char namebuf[8]; /* XXX */

	if ((dp==NULL)||(name==NULL)){
		return NULL;
	}

	for (p=0;p<part_num;p++){
		/* Note: allow even invalid partition to be found!!! */

		if (part[p].diskp!=dp){ /* not on this disk? */
			continue; /* next */
		}

		/* name */
		if (part[p].type==PART_TYPE_DD){
			if (part[p].letter==0){
				sprintf(namebuf,"DD");
			}else{
				sprintf(namebuf,"DD%u",(u_int)part[p].letter);
			}
		}else{
			sprintf(namebuf,"%c",part[p].letter);
		}

		/* compare name (case-insensitive) */
		if (strcasecmp(namebuf,name)==0){ /* match? */
			return &part[p]; /* found */
		}
	}

	return NULL; /* not found */
}



void
akai_disk_info(struct disk_s *dp,int verbose)
{
	u_int totp;
	u_int totf;
	u_int pi;

	if (dp==NULL){
		return;
	}

	/* scan all partitions in system */
	totp=0;
	totf=0;
	for (pi=0;pi<part_num;pi++){
		if (part[pi].diskp!=dp){ /* not this disk? */
			continue;
		}
		totp++;
		if (part[pi].valid){ /* valid? */
			totf+=part[pi].bfree;
		}
	}
	if (verbose){
		printf("disk:    %u\n",dp->index);
		printf("parts:   %u\n",totp);
		printf("type:    ");
		switch (dp->type){
		case DISK_TYPE_FLL:
			printf("low-density floppy");
			break;
		case DISK_TYPE_FLH:
			printf("high-density floppy");
			break;
		case DISK_TYPE_HD9:
			printf("S900 harddisk");
			break;
		case DISK_TYPE_HD:
			printf("S1000/S3000 harddisk");
			break;
		default:
			printf("\?\?\?");
		}
		printf("\nblksize: 0x%04x bytes\n",dp->blksize);
		printf("total:   0x%04x blocks (%9u bytes)\n",
			dp->totsize,
			dp->totsize*dp->blksize);
		printf("free:    0x%04x blocks (%9u bytes) = %5.1lf%%\n",
			totf,
			totf*dp->blksize,
			(dp->totsize>0)?(100.0*((double)totf)/((double)dp->totsize)):0.0);
	}else{
		printf(" %3u   ",dp->index);
		switch (dp->type){
		case DISK_TYPE_FLL:
			printf("FLL");
			break;
		case DISK_TYPE_FLH:
			printf("FLH");
			break;
		case DISK_TYPE_HD9:
			printf("HD9");
			break;
		case DISK_TYPE_HD:
			printf(" HD");
			break;
		default:
			printf("\?\?\?");
		}
		printf("   %3u   0x%04x   0x%04x   %5.1lf     0x%04x   %5.1lf  %5.1lf\n",
			totp,
			dp->blksize,
			dp->totsize,
			((double)dp->totsize*dp->blksize)/((double)1024*1024),
			totf,
			((double)totf*dp->blksize)/((double)1024*1024),
			(dp->totsize>0)?(100.0*((double)totf)/((double)dp->totsize)):0.0);
	}
}

void
akai_list_disk(struct disk_s *dp,int recflag,u_char *filtertagp)
{
	u_int p;
	u_int totp;

	if (dp==NULL){
		return;
	}

	printf("/disk%u\n",dp->index);
	printf("part  type startblk size/blks  size/MB  free/blks free/MB free/%%\n");
	printf("----------------------------------------------------------------\n");
	/* partitions on disk */
	totp=0;
	for (p=0;p<part_num;p++){
		if (part[p].diskp==dp){ /* on this disk? */
			/* print partition info */
			akai_part_info(&part[p],0);
			totp++;
		}
	}
	printf("----------------------------------------------------------------\n");
	printf("total: %3u partition(s)\n\n",totp);

	if (recflag){
		/* partitions on disk */
		for (p=0;p<part_num;p++){
			if ((part[p].diskp==dp) /* on this disk? */
				&&(part[p].valid)){
				/* list root directory in partition */
				akai_list_part(&part[p],recflag,filtertagp);
			}
		}
	}
}

void
akai_list_alldisks(int recflag,u_char *filtertagp)
{
	u_int di;

	printf("/\n");
	printf("disk  type parts  blksize tot/blks  tot/MB  free/blks free/MB free/%%\n");
	printf("--------------------------------------------------------------------\n");
	/* disks */
	for (di=0;di<disk_num;di++){
		/* print disk info */
		akai_disk_info(&disk[di],0);
	}
	printf("--------------------------------------------------------------------\n");
	printf("total: %3u disk(s)\n\n",disk_num);

	if (recflag){
		/* disks */
		for (di=0;di<disk_num;di++){
			/* list disk */
			akai_list_disk(&disk[di],recflag,filtertagp);
		}
	}
}



void
akai_sort_filetags(u_char *tagp)
{
	u_int i,j;
	u_char b;

	if (tagp==NULL){
		return;
	}

	for (i=0;i<(AKAI_FILE_TAGNUM-1);i++){
		for (j=i+1;j<AKAI_FILE_TAGNUM;j++){
			if (tagp[i]>tagp[j]){
				/* swap */
				b=tagp[i];
				tagp[i]=tagp[j];
				tagp[j]=b;
			}
		}
	}
}



int
akai_match_filetags(u_char *filtertagp,u_char *testtagp)
{
	u_int i,j;

	if (filtertagp==NULL){
		return 0; /* no filter, matches always */
	}
	if (testtagp==NULL){
		return -1; /* no match */
	}

	if (testtagp[0]==AKAI_FILE_TAGS1000){ /* XXX check if testtagp[] stems from S1000 */
		/* now, match only if all entries in filtertagp[] are free */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			if ((filtertagp[i]!=AKAI_FILE_TAGFREE)&&(filtertagp[i]!=AKAI_FILE_TAGS1000)){ /* not free? */
				return -1; /* no match */
			}
		}

		return 0; /* match */
	}

	for (i=0;i<AKAI_FILE_TAGNUM;i++){
		if ((filtertagp[i]==AKAI_FILE_TAGFREE)||(filtertagp[i]==AKAI_FILE_TAGS1000)){ /* free? */
			continue; /* next */
		}
		for (j=0;j<AKAI_FILE_TAGNUM;j++){
			if ((testtagp[i]==AKAI_FILE_TAGFREE)||(testtagp[i]==AKAI_FILE_TAGS1000)){ /* free? */
				continue; /* next */
			}
			if (testtagp[j]==filtertagp[i]){ /* found? */
				break; /* found entry i */
			}
		}
		if (j==AKAI_FILE_TAGNUM){ /* not found? */
			return -1; /* no match */
		}
	}
	/* now, all non-free entries of filtertagp[] are found in testtagp[] => match */

	return 0; /* match */
}



int
akai_set_filetag(u_char *tagp,u_int ti)
{
	u_int i;

	if (tagp==NULL){
		return -1;
	}

	if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
		fprintf(stderr,"invalid tag index\n");
		return -1;
	}

	/* check if already present */
	for (i=0;i<AKAI_FILE_TAGNUM;i++){
		if (tagp[i]==ti){
			return 0; /* nothing to do */
		}
	}

	/* find free entry */
	for (i=0;i<AKAI_FILE_TAGNUM;i++){
		if ((tagp[i]==AKAI_FILE_TAGFREE)||(tagp[i]==AKAI_FILE_TAGS1000)){ /* free? */
			break; /* found */
		}
	}
	if (i==AKAI_FILE_TAGNUM){ /* none found? */
		printf("no free tag entry\n");
		return -1;
	}
	/* set tag */
	tagp[i]=ti;

	akai_sort_filetags(tagp);

	return 0;
}



int
akai_clear_filetag(u_char *tagp,u_int ti)
{
	u_int i;

	if (tagp==NULL){
		return -1;
	}

	/* XXX allow to clear invalid tag index */

	if (ti==AKAI_FILE_TAGFREE){
		/* clear all */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			tagp[i]=AKAI_FILE_TAGFREE;
		}
	}else if (ti==AKAI_FILE_TAGS1000){
		/* clear all */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			tagp[i]=AKAI_FILE_TAGS1000;
		}
	}else{
		/* find entry */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			if (tagp[i]==ti){
				/* clear */
				tagp[i]=AKAI_FILE_TAGFREE;
				/* Note: must not break, must find (although invalid) repitions as well */
			}
		}
	}

	akai_sort_filetags(tagp);

	return 0;
}



/* get file with index fi */
/* saves result in *fp */
int
akai_get_file(struct vol_s *vp,struct file_s *fp,u_int fi)
{

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)||(vp->file==NULL)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)){
		return -1;
	}
	if (fp==NULL){
		return -1;
	}

	if (fi>=vp->fimax){
		return -1;
	}

	/* in volume */
	fp->volp=vp;

	/* index */
	fp->index=fi;

	/* type */
	fp->type=vp->file[fi].type;
	if (fp->type==AKAI_FTYPE_FREE){
		return -1;
	}

	/* tags */
	bcopy(vp->file[fi].tag,fp->tag,AKAI_FILE_TAGNUM);

	/* osver: */
	/* if S1000/S3000: OS version */
	/* if S900 compressed file: number of un-compressed floppy blocks */
	/* else: zero */
	fp->osver=(vp->file[fi].osver[1]<<8)
			  +vp->file[fi].osver[0];

	/* name */
	akai2ascii_filename(vp->file[fi].name,(u_char)fp->type,fp->osver,fp->name,vp->type==AKAI_VOL_TYPE_S900);

	/* size */
	fp->size=(vp->file[fi].size[2]<<16)
			 +(vp->file[fi].size[1]<<8)
			 +vp->file[fi].size[0];

	/* start */
	fp->bstart=(vp->file[fi].start[1]<<8)
			   +vp->file[fi].start[0];

	return 0;
}



/* find file with given name */
/* saves result in *fp */
int
akai_find_file(struct vol_s *vp,struct file_s *fp,char *name)
{
	u_int fi;
	char namebuf[AKAI_NAME_LEN+4+1]; /* +4 for ".<type>", +1 for '\0' */
	u_int i,j,k,l;

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)){
		return -1;
	}
	if (fp==NULL){
		return -1;
	}
	if (name==NULL){
		return -1;
	}

	/* fix name */
	l=(u_int)strlen(name);
	if (l>AKAI_NAME_LEN+4){ /* too long for namebuf? */
		return -1;
	}
	j=l; /* end */
	for (i=0;i<l;i++){
		if (name[i]=='_'){ /* for keyboard entry */
			namebuf[i]=' ';
		}else{
			namebuf[i]=name[i];
			if (name[i]=='.'){
				j=i; /* last '.' so far */
			}
		}
	}
	namebuf[i]='\0';
	/* remove trailing ' ' (XXX don't care how many) before last '.' (or end if no '.') */
	k=j;
	for (i=0;i<j;i++){
		if (namebuf[j-i-1]==' '){
			namebuf[j-i-1]='\0';
			k--;
		}else{
			break;
		}
	}
	if ((k<j)&&(j<l)){
		/* move ".<type>" */
		for (i=0;(k+i<l)&&(j+i<l);i++){
			namebuf[k+i]=namebuf[j+i];
		}
		namebuf[k+i]='\0';
	}

	/* files in volume directory */
	for (fi=0;fi<vp->fimax;fi++){
		/* get file */
		if (akai_get_file(vp,fp,fi)<0){
			continue; /* next file */
		}

		/* compare name (case-insensitive) */
		if (strcasecmp(fp->name,namebuf)==0){ /* match? */
			return 0; /* found */
		}
	}

	return -1; /* not found */
}



int
akai_rename_file(struct file_s *fp,char *name,struct vol_s *vp,u_int dstindex,u_char *tagp,u_int osver)
{
	u_int i;
	u_int diffvolflag;
	u_int fisrc,fidst;
	struct vol_s *volsrcp,*voldstp;
	u_char ft;
	u_char anamebuf[AKAI_NAME_LEN];

	if ((fp==NULL)||(fp->type==AKAI_FTYPE_FREE)){
		return -1;
	}
	if ((fp->volp==NULL)||(fp->volp->type==AKAI_VOL_TYPE_INACT)||(fp->volp->file==NULL)){
		return -1;
	}
	if ((fp->volp->partp==NULL)||(!fp->volp->partp->valid)||(fp->volp->partp->fd<0)){
		return -1;
	}

	if ((vp!=NULL)&&((vp->type==AKAI_VOL_TYPE_INACT)||(vp->file==NULL))){
		return -1;
	}
	if ((vp!=NULL)&&(vp->partp!=fp->volp->partp)){ /* if vp specified: not on same partition? */
		printf("must be in same partition\n");
		return -1;
	}

	/* Note: must not compare pointers (vp and fp->volp) if the buffer behind volume pointer is not unique for each volume!!! */
	/*       use dirblk[0] instead, which is unique if same partition (see check above) */
	if ((vp!=NULL)&&(vp->dirblk[0]!=fp->volp->dirblk[0])){ /* different volume specified? */
		diffvolflag=1;
	}else{
		diffvolflag=0;
	}

	if ((name==NULL)&&((osver!=fp->osver)||diffvolflag)){ /* no new name but different OS version or different volume? */
		/* possibly different char conversion or possibly need to correct osver -> handle as new name below */
		name=fp->name;
	}

	if ((diffvolflag==0)&&(name==NULL)&&(tagp==NULL)){ /* nothing to do? */
		return 0; /* done */
	}

	/* source file */
	fisrc=fp->index;
	volsrcp=fp->volp;

	if (diffvolflag){ /* different volume specified? */
		voldstp=vp;
		if (dstindex==AKAI_CREATE_FILE_NOINDEX){ /* no user-supplied index? */
			/* find free destination volume directory entry */
			for (fidst=0;fidst<voldstp->fimax;fidst++){
				if (voldstp->file[fidst].type==AKAI_FTYPE_FREE){ /* free? */
					break; /* found one */
				}
			}
			if (fidst==voldstp->fimax){ /* none found? */
				printf("no free file entry found\n");
				return -1;
			}
		}else{
			if (voldstp->file[dstindex].type!=AKAI_FTYPE_FREE){ /* not free? */
				printf("destination index not free\n");
				return -1;
			}
			fidst=dstindex;
		}
	}else{
		/* same volume */
		voldstp=volsrcp;
		if ((dstindex==AKAI_CREATE_FILE_NOINDEX) /* no user-supplied index? */
			||(dstindex==fisrc)){ /* or same as before? */
			fidst=fisrc; /* keep index */
		}else{
			if (voldstp->file[dstindex].type!=AKAI_FTYPE_FREE){ /* not free? */
				printf("destination index not free\n");
				return -1;
			}
			fidst=dstindex; /* new index */
		}
	}

	/* name and type */
	ft=0;
	if (name!=NULL){ /* new name? */
		/* new name and type and possibly correct osver */
		ft=ascii2akai_filename(name,anamebuf,&osver,voldstp->type==AKAI_VOL_TYPE_S900);
		if (ft==AKAI_FTYPE_FREE){
			fprintf(stderr,"invalid name\n");
			return -1;
		}
	}

	/* update volume entries (after having checked the name!!!) */

	if (diffvolflag||(fidst!=fisrc)){ /* different volume specified, or different index within same volume? */
		/* create destination volume directory entry */
		bcopy(&volsrcp->file[fisrc],&voldstp->file[fidst],sizeof(struct akai_voldir_entry_s));

		/* new volume */
		fp->volp=voldstp;

		/* new index */
		fp->index=fidst;

		/* free old volume directory entry */
		bzero(&volsrcp->file[fisrc],sizeof(struct akai_voldir_entry_s));
		volsrcp->file[fisrc].type=AKAI_FTYPE_FREE;

		/* write source volume directory */
		if (akai_write_voldir(volsrcp,fisrc)<0){
			return -1;
		}
	}

	/* set destination volume directory entry */

	if (osver!=fp->osver){ /* different OS version? */
		/* new OS version */
		fp->osver=osver;
		voldstp->file[fidst].osver[1]=0xff&(fp->osver>>8);
		voldstp->file[fidst].osver[0]=0xff&fp->osver;
	}

	/* name and type */
	if (name!=NULL){ /* new name/type? */
		/* new name */
		if (vp->type==AKAI_VOL_TYPE_S900){
			bcopy(anamebuf,voldstp->file[fidst].name,AKAI_NAME_LEN_S900);
			bzero(voldstp->file[fidst].name+AKAI_NAME_LEN_S900,AKAI_NAME_LEN-AKAI_NAME_LEN_S900);
		}else{
			bcopy(anamebuf,voldstp->file[fidst].name,AKAI_NAME_LEN);
		}
		if (name!=fp->name){ /* not already in fp->name? */
			/* copy */
			strcpy(fp->name,name); /* Note: name has been checked above */
		}

		/* new type */
		voldstp->file[fidst].type=ft;
		fp->type=ft;
	}
	/* else: keep old name and type */
	/*       No need to copy since whole dir entry has been copied above if necessary */

	/* tags */
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		/* no tags */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			fp->tag[i]=AKAI_FILE_TAGFREE;
		}
		bzero(voldstp->file[fidst].tag,AKAI_FILE_TAGNUM);
	}else{
		/* S1000/S3000 */
		if (tagp!=NULL){ /* new tags? */
			/* copy tags */
			bcopy(tagp,voldstp->file[fidst].tag,AKAI_FILE_TAGNUM);
			akai_sort_filetags(voldstp->file[fidst].tag);
			if (tagp!=fp->tag){ /* not already in fp->tag? */
				/* copy */
				bcopy(voldstp->file[fidst].tag,fp->tag,AKAI_FILE_TAGNUM);
				/* XXX no check for validity of tag indices */
			}
		}
		/* else: keep old tags */
		/*       No need to copy since whole dir entry has been copied above if necessary */
	}

	/* write destination volume directory */
	/* Note: the same partition for source and destination */
	if (akai_write_voldir(voldstp,fidst)<0){
		return -1;
	}

	return 0;
}



/* uses *fp as output */
/* size in bytes */
int
akai_create_file(struct vol_s *vp,struct file_s *fp,u_int size,u_int index,char *name,u_int osver,u_char *tagp)
{
	u_char ft;
	u_int bsize;
	u_int fi;
	u_int i;
	u_char anamebuf[AKAI_NAME_LEN];

	if ((vp==NULL)||(vp->type==AKAI_VOL_TYPE_INACT)||(vp->file==NULL)){
		return -1;
	}
	if ((vp->partp==NULL)||(!vp->partp->valid)||(vp->partp->fd<0)||(vp->partp->blksize==0)){
		return -1;
	}
	if (fp==NULL){
		return -1;
	}
	if (name==NULL){
		return -1;
	}

	if (size>AKAI_FILE_SIZEMAX){
		fprintf(stderr,"file size too large\n");
		return -1;
	}

	/* volume */
	fp->volp=vp;

	/* name and type and possibly correct osver */
	ft=ascii2akai_filename(name,anamebuf,&osver,vp->type==AKAI_VOL_TYPE_S900);
	if (ft==AKAI_FTYPE_FREE){
		fprintf(stderr,"invalid name\n");
		return -1;
	}
	/* now, lengths also OK */
	strcpy(fp->name,name);
	fp->type=ft;

	/* osver */
	fp->osver=osver;

	/* index */
	if (index==AKAI_CREATE_FILE_NOINDEX){ /* no user-supplied volume index? */
		/* find free volume directory entry */
		for (fi=0;fi<vp->fimax;fi++){
			if (vp->file[fi].type==AKAI_FTYPE_FREE){ /* free? */
				break; /* found one */
			}
		}
		if (fi==vp->fimax){ /* none found? */
			printf("no free file entry found\n");
			return -1;
		}
	}else{
		/* user-supplied index */
		fi=index;
		if (fi>vp->fimax){
			return -1;
		}
		if (vp->file[fi].type!=AKAI_FTYPE_FREE){ /* not free? */
			printf("file entry not free\n");
			return -1;
		}
	}
	fp->index=fi;

	/* size */
	fp->size=size;

	/* tags */
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		/* no tags */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			fp->tag[i]=AKAI_FILE_TAGFREE;
		}
	}else{
		/* S1000/S3000 */
		if (tagp!=NULL){
			/* correct and copy */
			/* Note: even if tagp==fp->tag */
			for (i=0;i<AKAI_FILE_TAGNUM;i++){
				if ((tagp[i]>=1)&&(tagp[i]<=AKAI_PARTHEAD_TAGNUM)){
					fp->tag[i]=tagp[i];
				}else{
					/* set free */
					if (fp->osver<=AKAI_OSVER_S1100MAX){
						fp->tag[i]=AKAI_FILE_TAGS1000;
					}else{
						fp->tag[i]=AKAI_FILE_TAGFREE;
					}
				}
			}
			akai_sort_filetags(fp->tag);
		}else{
			/* all free */
			for (i=0;i<AKAI_FILE_TAGNUM;i++){
				if (fp->osver<=AKAI_OSVER_S1100MAX){
					fp->tag[i]=AKAI_FILE_TAGS1000;
				}else{
					fp->tag[i]=AKAI_FILE_TAGFREE;
				}
			}
		}
	}

	/* allocate space */
	bsize=(fp->size+fp->volp->partp->blksize-1)/fp->volp->partp->blksize; /* round up */
	if (akai_allocate_fatchain(vp->partp,bsize,
							   &fp->bstart,
							   1,
							   (vp->type==AKAI_VOL_TYPE_S900)?AKAI_FAT_CODE_FILEEND900:AKAI_FAT_CODE_FILEEND)<0){ /* end code */
		return -1;
	}

	/* create volume directory entry */
	bzero(&vp->file[fi],sizeof(struct akai_voldir_entry_s));
	if (vp->type==AKAI_VOL_TYPE_S900){
		/* S900 */
		/* name */
		bcopy(anamebuf,vp->file[fi].name,AKAI_NAME_LEN_S900);
		bzero(vp->file[fi].name+AKAI_NAME_LEN_S900,AKAI_NAME_LEN-AKAI_NAME_LEN_S900);
		/* no tags */
		bzero(vp->file[fi].tag,AKAI_FILE_TAGNUM);
	}else{
		/* S1000/S3000 */
		/* name */
		bcopy(anamebuf,vp->file[fi].name,AKAI_NAME_LEN);
		/* tags */
		bcopy(fp->tag,vp->file[fi].tag,AKAI_FILE_TAGNUM);
	}
	/* type */
	vp->file[fi].type=fp->type;
	/* size */
	vp->file[fi].size[2]=0xff&(fp->size>>16);
	vp->file[fi].size[1]=0xff&(fp->size>>8);
	vp->file[fi].size[0]=0xff&fp->size;
	/* start */
	vp->file[fi].start[1]=0xff&(fp->bstart>>8);
	vp->file[fi].start[0]=0xff&fp->bstart;
	/* OS version */
	vp->file[fi].osver[1]=0xff&(fp->osver>>8);
	vp->file[fi].osver[0]=0xff&fp->osver;

	/* write volume directory */
	if (akai_write_voldir(vp,fi)<0){
		return -1;
	}

	return 0;
}



void
akai_fvol1000_initfile(struct akai_voldir_entry_s *ep,u_int osver,u_int tag)
{
	u_int i;

	if (ep==NULL){
		return;
	}

	/* OS version */
	ep->osver[1]=0xff&(osver>>8);
	ep->osver[0]=0xff&osver;

	if (osver==AKAI_OSVER_S900VOL){
		/* S900 */
		/* zero name and tags */
		bzero(ep->name,AKAI_NAME_LEN);
		bzero(ep->tag,AKAI_FILE_TAGNUM);
	}else{
		/* S1000/S3000 */
		/* default name */
		ascii2akai_name(AKAI_EMPTY1000_FNAME,ep->name,0); /* 0: not S900 */
		/* default tags */
		for (i=0;i<AKAI_FILE_TAGNUM;i++){
			ep->tag[i]=tag;
		}
	}

	/* type: free entry */
	ep->type=AKAI_FTYPE_FREE;
}

int
akai_delete_file(struct file_s *fp)
{
	u_int fi;

	if ((fp==NULL)||(fp->type==AKAI_FTYPE_FREE)){
		return -1;
	}
	if ((fp->volp==NULL)||(fp->volp->type==AKAI_VOL_TYPE_INACT)||(fp->volp->file==NULL)){
		return -1;
	}
	if ((fp->volp->partp==NULL)||(!fp->volp->partp->valid)||(fp->volp->partp->fd<0)){
		return -1;
	}

	/* free FAT chain */
	if (akai_free_fatchain(fp->volp->partp,fp->bstart,1)<0){ /* 1: write FAT */
		return -1;
	}

	/* file index */
	fi=fp->index;

	/* free volume directory entry */
	bzero(&fp->volp->file[fi],sizeof(struct akai_voldir_entry_s));
	if ((fp->volp->partp->type==PART_TYPE_HD9)
		||(fp->volp->partp->type==PART_TYPE_HD)
		||(fp->volp->type==AKAI_VOL_TYPE_S3000)||(fp->volp->type==AKAI_VOL_TYPE_CD3000)){
		/* harddisk and/or S3000 volume */
		fp->volp->file[fi].type=AKAI_FTYPE_FREE;
	}else{
		/* S900/S1000 floppy */
		akai_fvol1000_initfile(&fp->volp->file[fi],fp->volp->osver,AKAI_FILE_TAGS1000);
	}

	/* write volume directory */
	if (akai_write_voldir(fp->volp,fi)<0){
		return -1;
	}

	return 0;
}



int
akai_rename_ddtake(struct part_s *pp,u_int ti,char *name)
{
	u_int cstarts;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		return -1;
	}

	if (name==NULL){
		return -1;
	}

	cstarts=(pp->head.dd.take[ti].cstarts[1]<<8)
		    +pp->head.dd.take[ti].cstarts[0];
	if (cstarts==0){ /* empty? */
		return -1;
	}

	/* rename */
	ascii2akai_name(name,pp->head.dd.take[ti].name,0); /* 0: not S900 */

	/* write partition header */
	if (akai_io_blks(pp,(u_char *)&pp->head.dd,
					 0,
					 AKAI_DDPARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return -1;
	}

	return 0;
}

int
akai_delete_ddtake(struct part_s *pp,u_int ti)
{
	u_int cstarts,cstarte;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		return -1;
	}

	cstarts=(pp->head.dd.take[ti].cstarts[1]<<8)
		    +pp->head.dd.take[ti].cstarts[0];
	if (cstarts==0){ /* empty? */
		return -1;
	}
	cstarte=(pp->head.dd.take[ti].cstarte[1]<<8)
		    +pp->head.dd.take[ti].cstarte[0];

	/* free DD FAT chains */
	if (akai_free_ddfatchain(pp,cstarts,0)<0){ /* 0: don't write header yet */
		return -1;
	}
	if (akai_free_ddfatchain(pp,cstarte,0)<0){ /* 0: don't write header yet */
		return -1;
	}

	/* wipe directory entry for take */
	bzero(&pp->head.dd.take[ti],sizeof(struct akai_ddtake_s));

	/* update free block counter */
	akai_countfree_part(pp);

	/* write partition header */
	if (akai_io_blks(pp,(u_char *)&pp->head.dd,
					 0,
					 AKAI_DDPARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return -1;
	}

	return 0;
}



void
akai_fix_partheadmagic(struct part_s *pp)
{
	u_int i;
	u_int m,cs;

	/* Note: allow invalid partitions to be fixed!!! */
	if ((pp==NULL)||(pp->fd<0)){
		return;
	}
	if (pp->type!=PART_TYPE_HD){
		return;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	cs=pp->bsize;

	/* set magic */
	for (i=0;i<AKAI_PARTHEAD_MAGICNUM;i++){
		m=0xffff&(i*AKAI_PARTHEAD_MAGICVAL);
		pp->head.hd.magic[i][1]=0xff&(m>>8);
		pp->head.hd.magic[i][0]=0xff&m;
		cs+=m;
	}

	/* set checksum */
	pp->head.hd.chksum[3]=0xff&(cs>>24);
	pp->head.hd.chksum[2]=0xff&(cs>>16);
	pp->head.hd.chksum[1]=0xff&(cs>>8);
	pp->head.hd.chksum[0]=0xff&cs;
}

int
akai_check_partheadmagic(struct part_s *pp)
{
	u_int i;
	u_int m,cs,c;

	/* Note: allow invalid partitions to be checked!!! */
	if ((pp==NULL)||(pp->fd<0)){
		return -1;
	}
	if (pp->type==PART_TYPE_DD){
		return -1;
	}

	cs=pp->bsize;

	/* magic */
	for (i=0;i<AKAI_PARTHEAD_MAGICNUM;i++){
		m=(pp->head.hd.magic[i][1]<<8)
		  +pp->head.hd.magic[i][0];
		if (m!=(0xffff&(i*AKAI_PARTHEAD_MAGICVAL))){
			return -1; /* magic error */
		}
		cs+=m;
	}

	/* checksum */
	c=(pp->head.hd.chksum[3]<<24)
	  +(pp->head.hd.chksum[2]<<16)
	  +(pp->head.hd.chksum[1]<<8)
	  +pp->head.hd.chksum[0];
	if (c!=cs){
		return -2; /* only checksum error */
	}

	return 0; /* OK */
}

void
akai_fix_parttabmagic(struct akai_parttab_s *ptp)
{
	u_int i;
	u_int m;

	if (ptp==NULL){
		return;
	}

	/* set magic */
	for (i=0;i<AKAI_PARTTAB_MAGICNUM;i++){
		m=0xffff&(i*AKAI_PARTTAB_MAGICVAL);
		ptp->magic[i][1]=0xff&(m>>8);
		ptp->magic[i][0]=0xff&m;
	}
}

int
akai_check_parttabmagic(struct akai_parttab_s *ptp)
{
	u_int i;
	u_int m;

	if (ptp==NULL){
		return -1;
	}

	/* magic */
	for (i=0;i<AKAI_PARTTAB_MAGICNUM;i++){
		m=(ptp->magic[i][1]<<8)+ptp->magic[i][0];
		if (m!=(0xffff&(i*AKAI_PARTTAB_MAGICVAL))){
			return -1; /* magic error */
		}
	}

	return 0; /* OK */
}



int
akai_scan_floppy(struct disk_s *dp)
{
	u_int i,n;

	if (dp==NULL){
		return -1;
	}
	
	/* set disk type: first guess: high-density */
	dp->type=DISK_TYPE_FLH;

	/* disk blocksize and total size */
	if (dp->blksize!=AKAI_FL_BLOCKSIZE){ /* wrong blocksize? */
		/* set new blksize */
		dp->blksize=AKAI_FL_BLOCKSIZE;
		/* determine disk total size */
		dp->totsize=akai_disksize(dp);
	} /* else: assume that totsize is valid */
	if (dp->totsize<AKAI_FLHHEAD_BLKS){ /* see below */
		return -1;
	}

	/* disk pointer */
	part[part_num].diskp=dp;

	part[part_num].valid=1; /* OK so far */

	/* disk file descriptor */
	part[part_num].fd=dp->fd;
	
	/* set partition type (first guess) */
	part[part_num].type=dp->type;

	/* fake index on disk */
	part[part_num].index=0;

	/* block size */
	part[part_num].blksize=dp->blksize;
	
	/* start block */
	part[part_num].bstart=0;
	
	/* size (Note: also needed to read floppy header below) */
	/* first guess: max. available size */
	part[part_num].bsize=dp->totsize-part[part_num].bstart;
	
	/* read floppy header at block 0 relative to start */
	/* Note: low- and high-density floppy headers are identical */
	/*       up to first AKAI_FAT_ENTRIES_FLL FAT-entries!!! */
	/* => read the largest header: high-density */
	if (akai_io_blks(&part[part_num],(u_char *)&part[part_num].head.flh,
					 0,
					 AKAI_FLHHEAD_BLKS,
					 1,IO_BLKS_READ)<0){ /* 1: alloc cache if possible */
		/* assume end of disk, done */
		/* discard this partition, keep old part_num */
		return -1;
	}

	/* start of FAT */
	/* Note: see comment above: FAT-start same for low- and high density */
	part[part_num].fat=&part[part_num].head.flh.fatblk[0];

	part[part_num].bsize=AKAI_FLH_SIZE;
	/* count S1000/S3000 system blocks in FAT */
	for (i=0;i<(AKAI_FLHHEAD_BLKS+AKAI_VOLDIR3000FL_BLKS);i++){
		n=(part[part_num].fat[i][1]<<8)+part[part_num].fat[i][0];
		if (n!=AKAI_FAT_CODE_SYS){ /* not reserved for system anymore? */
			break; /* end of system blocks */
		}
	}
	if (i>0){
		/* assume S1000/S3000 */
		/* reserved blocks for system */
		part[part_num].bsyssize=i;
		/* check floppy type */
		if ((dp->totsize>=AKAI_FLL_SIZE)&&(dp->totsize<AKAI_FLH_SIZE)
			&&((i==AKAI_FLLHEAD_BLKS)||(i==(AKAI_FLLHEAD_BLKS+AKAI_VOLDIR3000FL_BLKS)))){ /* S1000 or S3000 low-density? */
			/* if correct, must be low-density now */
			dp->type=DISK_TYPE_FLL; /* disk type */
			part[part_num].bsize=AKAI_FLL_SIZE;
			/* Note: see comment above: no need to read low-density header again */
		}else if ((dp->totsize>=AKAI_FLH_SIZE)
				  &&((i==AKAI_FLHHEAD_BLKS)||(i==(AKAI_FLHHEAD_BLKS+AKAI_VOLDIR3000FL_BLKS)))){ /* S1000 or S3000 high-density? */
			/* if correct, must be high-density floppy now */
			dp->type=DISK_TYPE_FLH; /* disk type */
			part[part_num].bsize=AKAI_FLH_SIZE;
		}else{
			printf("disk%u: invalid floppy format\n",dp->index);
			/* XXX mark invalid, but keep it as high-density */
			part[part_num].valid=0;
			dp->type=DISK_TYPE_FLH; /* disk type */
			part[part_num].bsize=AKAI_FLH_SIZE;
		}
	}else{
		/* count S900 system blocks in FAT */
		for (i=0;i<AKAI_FLHHEAD_BLKS;i++){
			n=(part[part_num].fat[i][1]<<8)+part[part_num].fat[i][0];
			if (n!=AKAI_FAT_CODE_SYS900FL){ /* not reserved for system anymore? */
				break; /* end of system blocks */
			}
		}
		/* reserved blocks for system */
		part[part_num].bsyssize=i;
		/* check floppy type */
		/* Note: since AKAI_FAT_CODE_SYS900FL==AKAI_FAT_CODE_FREE, */
		/*       a low-density floppy may have i>=AKAI_FLLHEAD_BLKS here */
		if ((dp->totsize>=AKAI_FLL_SIZE)&&(dp->totsize<AKAI_FLH_SIZE)&&(i>=AKAI_FLLHEAD_BLKS)){ /* low-density? */
			/* if correct, must be low-density now */
			dp->type=DISK_TYPE_FLL; /* disk type */
			part[part_num].bsize=AKAI_FLL_SIZE;
			/* Note: AKAI_FLLHEAD_BLKS system blocks, i may differ */
			part[part_num].bsyssize=AKAI_FLLHEAD_BLKS;
			/* Note: see comment above: no need to read low-density header again */
		}else if ((dp->totsize>=AKAI_FLH_SIZE)&&(i==AKAI_FLHHEAD_BLKS)){ /* high-density? */
			/* if correct, must be high-density floppy now */
			dp->type=DISK_TYPE_FLH; /* disk type */
			part[part_num].bsize=AKAI_FLH_SIZE;
		}else{
			printf("disk%u: invalid floppy format\n",dp->index);
			/* XXX mark invalid, but keep it as high-density */
			part[part_num].valid=0;
			dp->type=DISK_TYPE_FLH; /* disk type */
			part[part_num].bsize=AKAI_FLH_SIZE;
		}
	}
	if ((part[part_num].bstart+part[part_num].bsize)>dp->totsize){
		printf("disk%u: expected size (0x%04x) of floppy is too large",
			dp->index,part[part_num].bsize);
		/* truncate */
		part[part_num].bsize=dp->totsize-part[part_num].bstart;
		printf(", truncated (0x%04x)\n",part[part_num].bsize);
	}

	/* actual type */
	part[part_num].type=dp->type;
	
	/* count free blocks, even if not valid */
	akai_countfree_part(&part[part_num]);
	
	/* number of volumes */
	part[part_num].volnummax=1;

	/* fake partition letter */
	part[part_num].letter='A';

	part_num++; /* found one */

	return 0;
}



int
akai_scan_harddisk9(struct disk_s *dp)
{
	u_int i,n;

	if (dp==NULL){
		return -1;
	}
	
	/* set disk type */
	dp->type=DISK_TYPE_HD9;

	/* disk blocksize and total size */
	if (dp->blksize!=AKAI_HD_BLOCKSIZE){ /* wrong blocksize? */
		/* set new blksize */
		dp->blksize=AKAI_HD_BLOCKSIZE;
		/* determine disk total size */
		dp->totsize=akai_disksize(dp);
	} /* else: assume that totsize is valid */
	if (dp->totsize<AKAI_HD9HEAD_BLKS){ /* see below */
		return -1;
	}

	/* disk pointer */
	part[part_num].diskp=dp;

	part[part_num].valid=1; /* OK so far */

	/* disk file descriptor */
	part[part_num].fd=dp->fd;
	
	/* set partition type */
	part[part_num].type=dp->type;

	/* fake index on disk */
	part[part_num].index=0;

	/* block size */
	part[part_num].blksize=dp->blksize;
	
	/* start block */
	part[part_num].bstart=0;
	
	/* size (Note: also needed to read harddisk header below) */
	/* first guess: max. available size */
	part[part_num].bsize=dp->totsize-part[part_num].bstart;

	/* read harddisk header at block 0 relative to start */
	if (akai_io_blks(&part[part_num],(u_char *)&part[part_num].head.hd9,
					 0,
					 AKAI_HD9HEAD_BLKS,
					 1,IO_BLKS_READ)<0){ /* 1: alloc cache if possible */
		/* assume end of disk, done */
		/* discard this partition, keep old part_num */
		return -1;
	}

	/* start of FAT */
	part[part_num].fat=&part[part_num].head.hd9.fatblk[0];

	/* count system blocks in FAT */
	for (i=0;i<AKAI_HD9HEAD_BLKS;i++){
		n=(part[part_num].fat[i][1]<<8)+part[part_num].fat[i][0];
		if (n!=AKAI_FAT_CODE_SYS900HD){ /* not reserved for system anymore? */
			/* discard this partition, keep old part_num */
			return -1;
		}
	}

	/* size */
	if (part[part_num].head.hd9.flag1==AKAI_HD9FLAG1_SIZEVALID){
		/* get size from header */
		part[part_num].bsize=(((u_int)part[part_num].head.hd9.size[1])<<8)
			+((u_int)part[part_num].head.hd9.size[0]);
		if (part[part_num].bsize>AKAI_HD9_MAXSIZE){
			printf("disk%u: invalid size (0x%04x) of harddisk",
				dp->index,part[part_num].bsize);
			part[part_num].bsize=AKAI_HD9_MAXSIZE;
			printf(", truncated (0x%04x)\n",part[part_num].bsize);
		}
	}else{
		/* default size */
		part[part_num].bsize=AKAI_HD9_DEFSIZE;
	}
	if ((part[part_num].bstart+part[part_num].bsize)>dp->totsize){
		printf("disk%u: expected size (0x%04x) of harddisk is too large",
			dp->index,part[part_num].bsize);
		/* truncate */
		part[part_num].bsize=dp->totsize-part[part_num].bstart;
		printf(", truncated (0x%04x)\n",part[part_num].bsize);
	}

	/* reserved blocks for system */
	part[part_num].bsyssize=AKAI_HD9HEAD_BLKS;

	/* count free blocks, even if not valid */
	akai_countfree_part(&part[part_num]);
	
	/* number of volumes */
	part[part_num].volnummax=AKAI_HD9ROOTDIR_ENTRIES;

	/* fake partition letter */
	part[part_num].letter='A';

	part_num++; /* found one */

	return 0;
}



int
akai_scan_ddpart(struct disk_s *dp,struct akai_parttab_s *ptp,u_int bstart,u_int pi)
{
	u_int ptcsize;
	u_int ptbsize;
	u_int totsize;
	u_int di,dimax;

	if ((dp==NULL)||(ptp==NULL)){
		return -1;
	}
	if (bstart>dp->totsize){
		return -1;
	}

	/* check for DD partitions */
	dimax=ptp->ddpartnum;
	if (dimax==0){
		return 0;
	}
	if (dimax>AKAI_DDPART_NUM){
		printf("disk%u: invalid DD partition number (%u), truncated (%u)\n",
			dp->index,dimax,AKAI_DDPART_NUM);
		dimax=AKAI_PART_NUM; /* XXX truncate */
	}

	totsize=0;
	for (di=0;(di<dimax)&&(part_num<PART_NUM_MAX)&&(bstart<dp->totsize);di++){
		/* disk pointer */
		part[part_num].diskp=dp;

		part[part_num].valid=1; /* OK so far */

		/* disk file descriptor */
		part[part_num].fd=dp->fd;

		/* type */
		part[part_num].type=PART_TYPE_DD;

		/* index on disk */
		part[part_num].index=pi;

		/* block size */
		part[part_num].blksize=dp->blksize;

		/* start block */
		part[part_num].bstart=bstart;

		/* partition size according to partition table */
		ptcsize=(ptp->ddpart[di][1]<<8)+ptp->ddpart[di][0]; /* in clusters */
		ptbsize=ptcsize*AKAI_DDPART_CBLKS; /* in blocks */
		/* check size */
		if (ptbsize==0){
			printf("disk%u: premature end of DD partitions\n",dp->index);
			/* end of partions, done */
			/* no further partition, keep old part_num */
			break; /* done */
		}
		if ((part[part_num].bstart+ptbsize)>dp->totsize){
			printf("disk%u: expected size (0x%04x) of DD partition %u is too large",
				dp->index,di,ptbsize);
			/* truncate */
			ptbsize=dp->totsize-part[part_num].bstart;
			ptcsize=ptbsize/AKAI_DDPART_CBLKS; /* in clusters */
			ptbsize=ptcsize*AKAI_DDPART_CBLKS; /* in blocks */
			printf(", truncated (0x%04x)\n",ptbsize);
			/* Note: this will be last partition */
			/* XXX keep valid */
		}
		part[part_num].bsize=ptbsize;
		part[part_num].csize=ptcsize;

		/* read DD partition header at block 0 relative to start */
		if (akai_io_blks(&part[part_num],(u_char *)&part[part_num].head.dd,
						 0,
						 AKAI_DDPARTHEAD_BLKS,
						 1,IO_BLKS_READ)<0){ /* 1: alloc cache if possible */
			/* discard this partition, keep old part_num */
			break; /* assume end of disk, done */
		}

		/* start of FAT */
		part[part_num].fat=&part[part_num].head.dd.fatcl[0];

		/* count free blocks, even if not valid */
		akai_countfree_part(&part[part_num]);

		/* no volumes */
		part[part_num].volnummax=0;

		/* fake partition letter */
		part[part_num].letter=di; /* XXX */

		part_num++; /* found one */
		pi++; /* absolute index */
		bstart+=ptbsize;
		totsize+=ptbsize;
	}

	/* check total block count */
	/* total size according to partition table */
	ptcsize=(ptp->ddpart[di][1]<<8)+ptp->ddpart[di][0]; /* in clusters */
	ptbsize=ptcsize*AKAI_DDPART_CBLKS; /* in blocks */
	if (ptbsize!=totsize){
		printf("disk%u: inconsistent total DD size (0x%04x instead of 0x%04x), ignored\n",
			dp->index,totsize,ptbsize);
		/* XXX ignore */
	}

	return 0;
}



int
akai_scan_disk(struct disk_s *dp,int floppyenable)
{
	u_int bstart;
	u_int pi,pimax;
	u_int ptbsize;
	struct akai_parttab_s *ptp;

	if (dp==NULL){
		return -1;
	}

	/* set disk type */
	/* first guess: S1000/S3000 harddisk */
	dp->type=DISK_TYPE_HD;

	/* set disk blocksize */
	/* Note: blksize may change now!!! */
	dp->blksize=AKAI_HD_BLOCKSIZE;

	/* determine disk total size */
	dp->totsize=akai_disksize(dp);
	if (dp->totsize==0){
		printf("disk%u: no blocks\n",dp->index);
		return 0;
	}

	/* scan for harddisk sampler partitions or floppy */
	bstart=0; /* first partition */
	pimax=AKAI_PART_NUM; /* first guess for number of partitions */
	ptp=NULL; /* no partition table found so far */
	for (pi=0;(pi<pimax)&&(part_num<PART_NUM_MAX)&&(bstart<dp->totsize);pi++,part_num++){
		/* Note: members of current part[part_num] may be overwritten in akai_scan_harddisk9() or akai_scan_floppy() below */

		/* disk pointer */
		part[part_num].diskp=dp;

		part[part_num].valid=1; /* OK so far */

		/* disk file descriptor */
		part[part_num].fd=dp->fd;

		/* type */
		/* first guess */
		part[part_num].type=dp->type;

		/* index on disk */
		part[part_num].index=pi;

		/* block size */
		part[part_num].blksize=dp->blksize;

		/* start block */
		part[part_num].bstart=bstart;

		/* partition size (Note: also needed to read sample partition header below) */
		/* first guess: max. available size */
		part[part_num].bsize=dp->totsize-part[part_num].bstart;

		/* read sampler partition header at block 0 relative to start */
		if (akai_io_blks(&part[part_num],(u_char *)&part[part_num].head.hd,
						 0,
						 AKAI_PARTHEAD_BLKS,
						 1,IO_BLKS_READ)<0){ /* 1: alloc cache if possible */
			/* discard this partition, keep old part_num */
			break; /* assume end of disk, done */
		}

		/* partition size (Note: also needed for checksum in magic check below) */
		/* first guess, will be checked below */
		part[part_num].bsize=(part[part_num].head.hd.size[1]<<8)+part[part_num].head.hd.size[0];

		/* reserved blocks for system */
		/* first guess */
		part[part_num].bsyssize=AKAI_PARTHEAD_BLKS;

		/* number of volumes */
		/* first guess */
		part[part_num].volnummax=AKAI_ROOTDIR_ENTRIES;

		/* partition letter */
		part[part_num].letter='A'+pi;

		/* check magic: indicates S1000/S3000 harddisk partition */
		if (akai_check_partheadmagic(&part[part_num])<0){
			/* not a valid S1000/S3000 harddisk sampler partition */
			if (pi==0){ /* first partition on disk? */
				/* should be S900 harddisk or S900/S1000/S3000 floppy now */
				if ((!floppyenable)||(dp->totsize*dp->blksize>AKAI_FLH_SIZE*AKAI_FL_BLOCKSIZE)){
					/* scan for S900 harddisk format */
					/* Note: akai_scan_harddisk9() may overwrite members of current disk and/or part[part_num] if necessary */
					if (akai_scan_harddisk9(dp)==0){ /* success? */
						return 0; /* done */
					}
					printf("disk%u: possibly floppy format in partition %c, ignored\n",dp->index,'A'+pi);
					/* XXX invalid, but keep it */
					part[part_num].valid=0; /* invalid partition so far */
					part_num++;
					break; /* assume end of disk, done */
				}else{
					/* scan for floppy format */
					/* Note: akai_scan_floppy() may overwrite members of current disk and/or part[part_num] if necessary */
					/* XXX first blocks with wrong blocksize cached, no problem, will be re-used */
					/* Note: assume end of disk, return via akai_scan_floppy() */
					return akai_scan_floppy(dp);
				}
			}else{
				printf("disk%u: invalid format in partition %c, ignored\n",dp->index,'A'+pi);
				/* XXX invalid, but keep it */
				part[part_num].valid=0; /* invalid partition so far */
				/* Note: if non-empty, part_num++ via for-loop */
			}
		}

		/* end of S1000/S3000 harddisk sampler partitions? */
		if (part[part_num].bsize==0){
			/* Note: if partition table present, this is a premature end, since pimax should be correct */
			if (ptp!=NULL){ /* partition table present? */
				printf("disk%u: premature end of partitions\n",dp->index);
				/* XXX ignore */
			}
			/* end of partions, done */
			/* no further partition, keep old part_num */
			break; /* done */
		}

		ptp=NULL; /* no partition table so far */
		if ((part[part_num].type==PART_TYPE_HD)&&(pi==0)){ /* first partition on S1000/S3000 harddisk? */
			/* now, first partition header was read and checked */

			/* check magic in parttab: indicates partition table */
			ptp=&(part[part_num].head.hd.parttab); /* try */
			if (akai_check_parttabmagic(ptp)==0){
				/* get partition number: first entry in partition table */
				pimax=ptp->partnum;
				if (pimax==0){
					printf("disk%u: empty partition table, ignored\n",dp->index);
					pimax=AKAI_PART_NUM; /* XXX try all */
				}
				if (pimax>AKAI_PART_NUM){
					printf("disk%u: invalid partition number (%u), truncated (%u)\n",
						dp->index,pimax,AKAI_PART_NUM);
					pimax=AKAI_PART_NUM; /* XXX truncate */
				}
			}else{
				/* if correct, no partition table present, e.g. for old harddisk format */
				ptp=NULL;
				/* XXX nevertheless, try without partition table to find valid partitions */
				/* XXX no need to mark invalid */
				printf("disk%u: no partition table present, ignored\n",dp->index);
			}
		}

		if (ptp!=NULL){
			/* partition size according to partition table */
			ptbsize=(ptp->part[pi][1]<<8)+ptp->part[pi][0];
			/* compare */
			if (part[part_num].bsize!=ptbsize){
				printf("disk%u: inconsistent size of partition %c (0x%04x instead of 0x%04x), ignored\n",
					dp->index,part[part_num].letter,part[part_num].bsize,ptbsize);
				/* XXX keep it valid */
			}
		}

		/* check size */
		if (part[part_num].bsize>AKAI_PART_MAXSIZE){
			printf("disk%u: invalid size of partition %c (0x%04x)",
				dp->index,part[part_num].letter,part[part_num].bsize);
			part[part_num].bsize=AKAI_PART_MAXSIZE;
			printf(", truncated (0x%04x)\n",part[part_num].bsize);
		}
		if ((part[part_num].bstart+part[part_num].bsize)>dp->totsize){
			printf("disk%u: expected size (0x%04x) of partition %c size is too large",
				dp->index,part[part_num].letter,part[part_num].bsize);
			/* truncate */
			part[part_num].bsize=dp->totsize-part[part_num].bstart;
			printf(", truncated (0x%04x)\n",part[part_num].bsize);
			/* Note: this will be last partition */
			/* XXX mark invalid, but keep it */
			part[part_num].valid=0;
		}

		/* start of FAT */
		part[part_num].fat=&part[part_num].head.hd.fatblk[0];

		/* count free blocks, even if not valid */
		akai_countfree_part(&part[part_num]);

		/* XXX trust part[part_num].bsize instead of ptbsize */
		/* next partition bstart */
		bstart+=part[part_num].bsize;
	}
	/* Note: bstart is total size in blocks now */

	if (ptp!=NULL){
		/* check total number of S1000/S3000 sampler partitions on disk */
		if (pi!=pimax){
			printf("disk%u: inconsistent partition number (%u instead of %u), ignored\n",
				dp->index,pi,pimax);
			/* XXX ignore */
		}

		/* check total number of blocks */
		/* total size according to partition table */
		ptbsize=(ptp->part[pimax][1]<<8)+ptp->part[pimax][0];
		if (ptbsize!=bstart){
			printf("disk%u: inconsistent total size (0x%04x instead of 0x%04x), ignored\n",
				dp->index,bstart,ptbsize);
			/* XXX ignore */
		}
	}

	if (ptp!=NULL){
		/* scan for S1100/S3000 harddisk DD partitions */
		if (akai_scan_ddpart(dp,ptp,bstart,pi)<0){
			return -1;
		}
	}
	
	return 0;
}



int
akai_wipe_part(struct part_s *pp,int wipeflag,struct part_s *plistp,u_int pimax,int cdromflag)
{
	int i;
	char namebuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	u_int pi,siz;
	u_int pnum,totb;
	u_int dnum,totdb;

	/* Note: allow invalid partitions to be wiped!!! */
	if ((pp==NULL)||(pp->fd<0)||(pp->blksize==0)){
		return -1;
	}

	if (pp->type==PART_TYPE_DD){
		/* S1100/S3000 harddisk DD partition */

		if (wipeflag){
			static u_char buf[AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE];
			struct akai_ddparthead_s *hp;

			/* wipe cluster 0: contains partition header */
			bzero(buf,AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE);

			hp=(struct akai_ddparthead_s *)buf;

			/* mark cluster 0 as reserved for system: contains partition header */
			hp->fatcl[0][1]=0xff&(AKAI_DDFAT_CODE_SYS>>8);
			hp->fatcl[0][0]=0xff&(AKAI_DDFAT_CODE_SYS);

			/* update partition header in memory */
			bcopy(hp,&pp->head.dd,sizeof(struct akai_ddparthead_s));

			/* write cluster 0 */
			if (akai_io_blks(pp,buf,
							 0,
							 AKAI_DDPART_CBLKS, /* 1 cluster */
							 0,IO_BLKS_WRITE)<0){ /* 0: don't allocate new cache */
				return -1;
			}
		}

		/* count free blocks */
		akai_countfree_part(pp);

		pp->volnummax=0;
		pp->valid=1; /* fixed */

		return 0;
	}

	if ((pp->type==PART_TYPE_HD9)&&(pp->fat!=NULL)){
		/* S900 harddisk */

		/* number of volumes */
		pp->volnummax=AKAI_HD9ROOTDIR_ENTRIES;

		if (wipeflag){
			/* wipe whole header */
			bzero(&pp->head.hd9,sizeof(struct akai_hd9head_s));

			/* allocate blocks for harddisk header */
			for (i=0;i<AKAI_HD9HEAD_BLKS;i++){
				pp->fat[i][1]=0xff&(AKAI_FAT_CODE_SYS900HD>>8);
				pp->fat[i][0]=0xff&AKAI_FAT_CODE_SYS900HD;
			}
		}

		/* set harddisk size */
		if (pp->bsize>AKAI_HD9_MAXSIZE){
			pp->bsize=AKAI_HD9_MAXSIZE;
		}
		pp->head.hd9.size[1]=0xff&(pp->bsize>>8);
		pp->head.hd9.size[0]=0xff&pp->bsize;
		pp->head.hd9.flag1=AKAI_HD9FLAG1_SIZEVALID;

		/* reserved blocks for system */
		pp->bsyssize=AKAI_HD9HEAD_BLKS;

		/* count free blocks */
		akai_countfree_part(pp);

		if (wipeflag){
			/* wipe root directory */
			bzero(&pp->head.hd9.vol[0],pp->volnummax*sizeof(struct akai_hd9rootdir_entry_s));
			for (i=0;i<(int)pp->volnummax;i++){
				/* default volume name */
				sprintf(namebuf,"VOLUME %03i",i+1);
				ascii2akai_name(namebuf,pp->head.hd9.vol[i].name,1); /* 1: S900 */
				pp->head.hd9.vol[i].start[1]=0xff&(AKAI_VOL_START_INACT>>8);
				pp->head.hd9.vol[i].start[0]=0xff&AKAI_VOL_START_INACT;
			}
			/* XXX must erase all vol_s which link to this partition */
		}

		/* copy first FAT entries */
		bcopy((u_char *)&pp->head.hd9.fatblk,(u_char *)&pp->head.hd9.fatblk0,AKAI_HD9FAT0_ENTRIES*2);
		/* write new harddisk header to harddisk */
		if (akai_io_blks(pp,(u_char *)&pp->head.hd9,
						 0,
						 AKAI_HD9HEAD_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}

		pp->valid=1; /* fixed */

		return 0;
	}

	if ((pp->type!=PART_TYPE_HD)||(pp->fat==NULL)){
		return -1;
	}
	/* now, S1000/S3000 harddisk sampler partition */

	/* number of volumes */
	pp->volnummax=AKAI_ROOTDIR_ENTRIES;

	if (wipeflag){
		/* wipe whole header */
		bzero(&pp->head.hd,sizeof(struct akai_parthead_s));
		/* Note: tagsmagic is cleared as well */

		/* allocate blocks for partition header */
		for (i=0;i<AKAI_PARTHEAD_BLKS;i++){
			pp->fat[i][1]=0xff&(AKAI_FAT_CODE_SYS>>8);
			pp->fat[i][0]=0xff&AKAI_FAT_CODE_SYS;
		}

		if (cdromflag){
			/* allocate blocks for CD-ROM info */
			/* XXX minimum size */
			for (i=AKAI_CDINFO_BLK;i<AKAI_CDINFO_BLK+AKAI_CDINFO_MINSIZB;i++){
				pp->fat[i][1]=0xff&(AKAI_FAT_CODE_SYS>>8);
				pp->fat[i][0]=0xff&AKAI_FAT_CODE_SYS;
			}
		}
	}

	/* set partition size */
	if (pp->bsize>AKAI_PART_MAXSIZE){
		pp->bsize=AKAI_PART_MAXSIZE;
	}
	pp->head.hd.size[1]=0xff&(pp->bsize>>8);
	pp->head.hd.size[0]=0xff&pp->bsize;

	/* reserved blocks for system */
	pp->bsyssize=AKAI_PARTHEAD_BLKS;

	/* count free blocks */
	akai_countfree_part(pp);

	/* fix magic: indicated harddisk */
	akai_fix_partheadmagic(pp);

	if (wipeflag){
		/* wipe root directory */
		bzero(&pp->head.hd.vol[0],pp->volnummax*sizeof(struct akai_rootdir_entry_s));
		for (i=0;i<(int)pp->volnummax;i++){
			/* default volume name */
			sprintf(namebuf,"VOLUME %03i",i+1);
			ascii2akai_name(namebuf,pp->head.hd.vol[i].name,0); /* 0: not S900 */
			pp->head.hd.vol[i].type=AKAI_VOL_TYPE_INACT;
		}
		/* XXX must erase all vol_s which link to this partition */
	}

	/* first partition on disk and want to fix partition table? */
	if ((pp->index==0)&&(plistp!=NULL)){
		/* initialize partition table */
		bzero(&(pp->head.hd.parttab),sizeof(struct akai_parttab_s));

		/* fix parttab magic: indicates partition table */
		akai_fix_parttabmagic(&(pp->head.hd.parttab));

		/* count number of partitions on same disk */
		pnum=0;
		totb=0;
		dnum=0;
		totdb=0;
		for (pi=0;pi<pimax;pi++){
			if (plistp[pi].fd==pp->fd){ /* same disk? */
				if (plistp[pi].type==PART_TYPE_DD){
					/* S1100/S3000 harddisk DD partition */
					if (dnum>=AKAI_DDPART_NUM){ /* already enough and another one? */
						printf("too many DD partitions on disk, truncated\n");
						continue; /* next */
					}
					siz=plistp[pi].bsize/AKAI_DDPART_CBLKS; /* XXX round down */
					/* set partition size in partition table */
					pp->head.hd.parttab.ddpart[dnum][1]=0xff&(siz>>8);
					pp->head.hd.parttab.ddpart[dnum][0]=0xff&siz;
					/* accumulate total size */
					totdb+=siz;
					dnum++; /* found one */
				}else{
					if (pnum>=AKAI_PART_NUM){ /* already enough and another one? */
						printf("too many partitions on disk, truncated\n");
						continue; /* next */
					}
					siz=plistp[pi].bsize;
					/* set partition size in partition table */
					pp->head.hd.parttab.part[pnum][1]=0xff&(siz>>8);
					pp->head.hd.parttab.part[pnum][0]=0xff&siz;
					/* accumulate total size */
					totb+=siz;
					pnum++; /* found one */
				}
			}
		}

		/* set partition number */
		pp->head.hd.parttab.partnum=0xff&pnum;
		/* append total size at end of table */
		/* Note: pnum has been checked above */
		pp->head.hd.parttab.part[pnum][1]=0xff&(totb>>8);
		pp->head.hd.parttab.part[pnum][0]=0xff&totb;

		/* set DD partition number */
		pp->head.hd.parttab.ddpartnum=0xff&dnum;
		/* append total size at end of DD table */
		/* Note: dnum has been checked above */
		pp->head.hd.parttab.ddpart[dnum][1]=0xff&(totdb>>8);
		pp->head.hd.parttab.ddpart[dnum][0]=0xff&totdb;
	}

	/* write new partition header to partition */
	if (akai_io_blks(pp,(u_char *)&pp->head.hd,
					 0,
					 AKAI_PARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return -1;
	}

	if (cdromflag){
		static u_char buf[AKAI_CDINFO_MINSIZB*AKAI_HD_BLOCKSIZE]; /* XXX minimum size */
		struct akai_cdinfohead_s *p;

		/* wipe CD-ROM info */
		/* XXX minimum size */
		bzero(buf,AKAI_CDINFO_MINSIZB*AKAI_HD_BLOCKSIZE);
		p=(struct akai_cdinfohead_s *)buf;
		/* default label */
		/* Note: CD-ROM label uses S1000/S3000 char conversion */
		ascii2akai_name(AKAI_CDINFO_DEFLABEL,p->cdlabel,0); /* 0: not S900 */
		if (akai_io_blks(pp,buf,
						 AKAI_CDINFO_BLK,
						 AKAI_CDINFO_MINSIZB,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return -1;
		}
	}

	pp->valid=1; /* fixed */

	return 0;
}



/* Note: blksize might change, must flush prior to formatting!!! */
/* Note: must flush cache and exit (or restart) if return value >=0 !!! */
int
format_harddisk(struct disk_s *dp,u_int bsize,u_int totb,int s3000flag,int cdromflag)
{
	u_int pi,pimax;
	u_int bc,bremain,bchunk,bmin;

	if (dp==NULL){
		return -1;
	}

	/* S1000/S3000 harddisk */

	if (totb*AKAI_HD_BLOCKSIZE>dp->totsize*dp->blksize){
		totb=(dp->totsize*dp->blksize)/AKAI_HD_BLOCKSIZE; /* truncate */
	}
	if (totb>AKAI_HD_MAXSIZE){
		totb=AKAI_HD_MAXSIZE; /* truncate */
	}
	if (bsize>totb){
		bsize=totb; /* truncate */
	}
	if (bsize>AKAI_PART_MAXSIZE){
		bsize=AKAI_PART_MAXSIZE; /* truncate */
	}
	if (bsize<AKAI_PARTHEAD_BLKS){
		fprintf(stderr,"invalid partition size\n");
		return -1;
	}

	/* set disk blocksize */
	dp->totsize*=dp->blksize;
	dp->blksize=AKAI_HD_BLOCKSIZE;
	dp->totsize/=dp->blksize;

	/* minimum harddisk sampler partition size */
	if (cdromflag){
		/* with min. CD-ROM info behind header */
		bmin=AKAI_PARTHEAD_BLKS+AKAI_CDINFO_MINSIZB;
	}else{
		bmin=AKAI_PARTHEAD_BLKS;
	}

	/* Note: overwrite part[] */
	bremain=totb;
	bc=0; /* block counter */
	for (pi=0;(pi<AKAI_PART_NUM)&&(pi<PART_NUM_MAX);){
		if (bremain>bsize){
			bchunk=bsize;
		}else{
			bchunk=bremain;
		}
		if (bchunk<bmin){
			/* not enough for harddisk sampler partition */
			break; /* done */
		}

		/* initialize partition */
		part[pi].diskp=dp;
		part[pi].valid=1;
		part[pi].fd=dp->fd;
		part[pi].type=PART_TYPE_HD;
		part[pi].index=pi;
		part[pi].blksize=dp->blksize;
		part[pi].bstart=bc;
		part[pi].bsize=bchunk;
		part[pi].bsyssize=AKAI_PARTHEAD_BLKS;
		part[pi].fat=&part[pi].head.hd.fatblk[0];
		part[pi].volnummax=AKAI_ROOTDIR_ENTRIES;
		part[pi].letter='A'+pi;

		pi++; /* created one */

		bremain-=bchunk;
		bc+=bchunk;
		if (bremain==0){
			break; /* done */
		}
	}

	if (pi==0){
		printf("no partitions created\n");
		return 0;
	}

	/* space for DD */
	/* XXX check totb alignment to AKAI_DDPART_BLKS granularity */
	bremain=dp->totsize-totb;
	/* fix size */
	bremain/=AKAI_DDPART_CBLKS; /* round down */
	if (bremain>0){
		bremain--; /* XXX */
	}
	bremain*=AKAI_DDPART_CBLKS;
	if ((pi<PART_NUM_MAX)&&(bremain>0)){ /* space for DD left at end of disk? */
		printf("0x%04x blocks for DD at end of disk\n",bremain);
		/* initialize one DD partition at end of disk */
		part[pi].diskp=dp;
		part[pi].valid=1;
		part[pi].fd=dp->fd;
		part[pi].type=PART_TYPE_DD;
		part[pi].index=pi;
		part[pi].blksize=dp->blksize;
		part[pi].bstart=totb;
		part[pi].bsize=bremain;
		part[pi].csize=bremain/AKAI_DDPART_CBLKS;
		part[pi].fat=NULL;
		part[pi].volnummax=0;
		part[pi].letter=0;

		pi++; /* created one */
	}

	/* now, all partitions known for partition table */
	pimax=pi;
	for (pi=0;pi<pimax;pi++){
		/* wipe partition */
		if (akai_wipe_part(&part[pi],1,&part[0],pimax,cdromflag)<0){ /* 1: wipe */
			if (part[pi].type==PART_TYPE_DD){
				if (part[pi].letter==0){
					fprintf(stderr,"cannot wipe partition DD\n");
				}else{
					fprintf(stderr,"cannot wipe partition DD%u\n",(u_int)part[pi].letter);
				}
			}else{
				fprintf(stderr,"cannot wipe partition %c\n",part[pi].letter);
			}
			return 1; /* fatal */
		}
		if ((part[pi].type==PART_TYPE_HD)&&s3000flag){
			if (akai_rename_tag(&part[pi],NULL,0,1)<0){ /* 1: wipe */
				fprintf(stderr,"cannot initialize tags in partition %c\n",part[pi].letter);
				/* XXX ignore error */
			}
		}
	}

	printf("created %u partition(s)\n",pimax);
	return 0;
}



/* Note: blksize might change, must flush prior to formatting!!! */
/* Note: must flush cache and exit (or restart) if return value >=0 !!! */
int
format_harddisk9(struct disk_s *dp,u_int totb)
{

	if (dp==NULL){
		return -1;
	}

	/* S900 harddisk */

	/* Note: overwrite part[] */
	if (PART_NUM_MAX<1){
		fprintf(stderr,"not enough partition space allocated\n");
		return -1;
	}

	if (totb*AKAI_HD_BLOCKSIZE>dp->totsize*dp->blksize){
		totb=(dp->totsize*dp->blksize)/AKAI_HD_BLOCKSIZE; /* truncate */
	}
	if (totb>AKAI_HD9_MAXSIZE){
		totb=AKAI_HD9_MAXSIZE; /* truncate */
	}
	if (totb<AKAI_HD9HEAD_BLKS){
		fprintf(stderr,"invalid size\n");
		return -1;
	}

	/* set disk blocksize */
	dp->totsize*=dp->blksize;
	dp->blksize=AKAI_HD_BLOCKSIZE;
	dp->totsize/=dp->blksize;

	/* Note: overwrite part[] */
	/* initialize partition */
	part[0].diskp=dp;
	part[0].valid=1;
	part[0].fd=dp->fd;
	part[0].type=PART_TYPE_HD9;
	part[0].index=0;
	part[0].blksize=dp->blksize;
	part[0].bstart=0;
	part[0].bsize=totb;
	part[0].bsyssize=AKAI_HD9HEAD_BLKS;
	part[0].fat=&part[0].head.hd9.fatblk[0];
	part[0].volnummax=AKAI_HD9ROOTDIR_ENTRIES;
	part[0].letter='A'; /* fake partition letter */

	/* wipe partition */
	if (akai_wipe_part(&part[0],1,NULL,0,0)<0){ /* 1: wipe */
		return 1; /* fatal */
	}

	return 0;
}



/* Note: blksize might change, must flush prior to formatting!!! */
/* Note: must flush cache and exit (or restart) if return value >=0 !!! */
int
format_floppy(struct disk_s *dp,int lodensflag,int s3000flag,int s900flag)
{
	struct akai_voldir3000fl_s tmpdir3000;
	struct akai_flvol_label_s *lp;
	u_int hdsiz;
	u_int voldir3000bstart;
	u_int i,imax;
	u_int osver;
	u_int fatcodesys;

	if (dp==NULL){
		return -1;
	}

	/* floppy */

	/* Note: overwrite part[] */
	if (PART_NUM_MAX<1){
		fprintf(stderr,"not enough partition space allocated\n");
		return -1;
	}

	if (((!lodensflag)&&(dp->totsize*dp->blksize<AKAI_FLH_SIZE*AKAI_FL_BLOCKSIZE))
		||((lodensflag)&&(dp->totsize*dp->blksize<AKAI_FLL_SIZE*AKAI_FL_BLOCKSIZE))){
		fprintf(stderr,"invalid disk size\n");
		return -1;
	}

	/* set disk blocksize */
	dp->totsize*=dp->blksize;
	dp->blksize=AKAI_FL_BLOCKSIZE;
	dp->totsize/=dp->blksize;

	/* floppy header */
	if (lodensflag){
		lp=&part[0].head.fll.label;
		hdsiz=AKAI_FLLHEAD_BLKS;
		voldir3000bstart=AKAI_VOLDIR3000FLL_BSTART; /* for S3000 */
	}else{
		lp=&part[0].head.flh.label;
		hdsiz=AKAI_FLHHEAD_BLKS;
		voldir3000bstart=AKAI_VOLDIR3000FLH_BSTART; /* for S3000 */
	}
	/* Note: low- and high-density floppy headers are identical */
	/*       up to first AKAI_FAT_ENTRIES_FLL FAT-entries */
	/* => set files and system blocks in FAT in high-density header */

	/* initialize partition */
	part[0].diskp=dp;
	part[0].valid=1;
	part[0].fd=dp->fd;
	if (lodensflag){
		part[0].type=PART_TYPE_FLL;
	}else{
		part[0].type=PART_TYPE_FLH;
	}
	part[0].index=0;
	part[0].blksize=dp->blksize;
	part[0].bstart=0;
	if (lodensflag){
		part[0].bsize=AKAI_FLL_SIZE;
	}else{
		part[0].bsize=AKAI_FLH_SIZE;
	}
	/* Note: see comment above: FAT-start same for low- and high density */
	part[0].fat=&part[0].head.flh.fatblk[0];

	/* number of volumes */
	part[0].volnummax=1;

	/* fake partition letter */
	part[0].letter='A';

	/* wipe floppy header */
	/* Note: high-density floppy header enough, also for low-density floppy case */
	bzero(&part[0].head.flh,sizeof(struct akai_flhhead_s));

	/* OS version */
	if (s3000flag){
		/* S3000 */
		osver=AKAI_OSVER_S3000MAX; /* XXX */
	}else if (s900flag){
		/* S900 */
		osver=AKAI_OSVER_S900VOL;
	}else{
		/* S1000 */
		osver=AKAI_OSVER_S1000MAX; /* XXX */
	}
	lp->osver[1]=0xff&(osver>>8);
	lp->osver[0]=0xff&osver;

	/* wipe S900/S1000 volume directory */
	for (i=0;i<AKAI_VOLDIR_ENTRIES_S1000FL;i++){
		akai_fvol1000_initfile(&part[0].head.flh.file[i],osver,
							   s3000flag?AKAI_FILE_TAGFREE:AKAI_FILE_TAGS1000);
	}
	if (s3000flag){
		/* mark S3000 floppy */
		part[0].head.flh.file[0].type=AKAI_VOLDIR3000FL_FTYPE;
	}

	if (!s900flag){
		/* S1000/S3000 */

		/* default volume name */
		ascii2akai_name("NOT NAMED   ",lp->name,0); /* 0: not S900 */

		/* default volume parameters */
		akai_init_volparam(&lp->param,s3000flag);
	}

	/* allocate blocks in FAT */
	if (s900flag){
		/* S900 */
		fatcodesys=AKAI_FAT_CODE_SYS900FL;
	}else{
		/* S1000/S3000 */
		fatcodesys=AKAI_FAT_CODE_SYS;
	}
	imax=hdsiz; /* floppy header */
	if (s3000flag){
		/* S3000 volume directory is directly behind floppy header */
		imax+=AKAI_VOLDIR3000FL_BLKS;
	}
	for (i=0;i<imax;i++){
		part[0].fat[i][1]=0xff&(fatcodesys>>8);
		part[0].fat[i][0]=0xff&fatcodesys;
	}
	/* reserved blocks for system */
	part[0].bsyssize=imax;

	/* write floppy header */
	if (akai_io_blks(&part[0],(u_char *)&part[0].head.flh,
					 0,
					 hdsiz,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		return 1; /* fatal */
	}

	if (s3000flag){
		/* wipe S3000 volume directory */
		bzero(&tmpdir3000,sizeof(struct akai_voldir3000fl_s));
		for (i=0;i<AKAI_VOLDIR_ENTRIES_S3000FL;i++){
			tmpdir3000.file[i].type=AKAI_FTYPE_FREE;
		}

		/* write S3000 volume directory */
		if (akai_io_blks(&part[0],(u_char *)&tmpdir3000,
						 voldir3000bstart,
						 AKAI_VOLDIR3000FL_BLKS,
						 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
			return 1; /* fatal */
		}
	}

	return 0;
}



/* XXX no check for recursion overflow */
int
change_curdir(char *name,u_int vi,char *lastname,int checklast)
{
	/* Note: variables below must be auto, for recursion!!! */
	char namebuf[DIRNAMEBUF_LEN+1]; /* +1 for '\0' */
	u_int l;
	u_int i;
	int j;
	char *np0,*np1;
	struct disk_s *tmpcurdiskp;
	struct part_s *tmpcurpartp;
	struct vol_s *tmpcurvolp;
	struct vol_s tmpcurvol_buf; /* change_curdir does not modify directories => OK to save and restore buffer */
	int ret;

	/* save state, needed in case of error */
	tmpcurdiskp=curdiskp;
	tmpcurpartp=curpartp;
	tmpcurvolp=curvolp;
	if (curvolp!=NULL){ /* need to save? */
		tmpcurvol_buf=*curvolp; /* !!! */
		/* Note: save original, to be restored later on, copy not to be used!!! */
	}

	ret=0; /* no error so far */
	np0=namebuf; /* first token */
	np1=NULL; /* no recursion so far */

	if (name==NULL){
		/* must be in partition */
		if ((curpartp==NULL)||(curvolp!=NULL)){
			ret=-1;
			goto change_curdir_done;
		}
		/* user-supplied volume index */
		if (vi>=curpartp->volnummax){
			ret=-1;
			goto change_curdir_done;
		}
		if (akai_get_vol(curpartp,&curvol_buf,vi)<0){ /* not found? */
			ret=-1;
			goto change_curdir_done;
		}
		curvolp=&curvol_buf;
		goto change_curdir_done;
	}

	/* copy name for possible manupulation */
	strncpy(namebuf,name,DIRNAMEBUF_LEN);
	namebuf[DIRNAMEBUF_LEN]='\0'; /* just in case */

	/* check if absolute path */
	if (np0[0]=='/'){
		/* goto system level */
		curdiskp=NULL;
		curpartp=NULL;
		curvolp=NULL;
		np0++; /* '\0' in worst case */
	}
	l=(u_int)strlen(np0);

	/* search first delimiter */
	for (i=0;i<l;i++){
		if (np0[i]=='/'){
			break;
		}
	}
	if (i<l){ /* found one? */
		/* separate strings */
		np0[i]='\0';
		np1=&np0[i+1]; /* '\0' in worst case, since i<l */
		/* Note: np1 must not be absolute path again!!! */
		if (np1[0]=='/'){ /* first char again delimiter? */
			ret=-1;
			goto change_curdir_done;
		}
		/* take care of np1 below */
	}
	/* now, np0 contains only one hirarchy level, but not the last one if np1!=NULL */

	if ((np1==NULL)&&(lastname!=NULL)){ /* is last hirarchy level and want to extract it? */
		/* Note: in particular if l==0, we end up here (for lastname!=NULL) */
		/* XXX no overflow check in strcpy */
		strcpy(lastname,np0);
		goto change_curdir_done;
	}
	/* Note: checklast irrelevant if lastname!=NULL */

	/* now, change current directory */

	if (l==0){
		/* Note: we do not search for disk, partition, or volume with empty name */
		/* nothing to do */
		goto change_curdir_done; /* done */
	}

	if (strcmp(np0,".")==0){
		/* nothing to do */
		goto change_curdir_done;
	}

	if (strcmp(np0,"..")==0){
		/* one level up */
		if (curdiskp==NULL){
			/* nothing to do */
			goto change_curdir_done;
		}
		if (curpartp==NULL){
			curdiskp=NULL;
			goto change_curdir_done;
		}
		if (curvolp==NULL){
			curpartp=NULL;
			goto change_curdir_done;
		}
		curvolp=NULL;
		goto change_curdir_done;
	}

	if (curdiskp==NULL){
#if 1
		if (strncasecmp(np0,"floppy",6)==0){
			np0+=6;
			/* find floppy */
			if (strlen(np0)==0){ /* empty? */
				/* "/floppy" -> "/disk0/A/<volume1>" */
				j=0; /* disk0 */
			}else{
				/* "/floppyN" -> "/diskN/A/<volume1>" */
				j=atoi(np0);
			}
			/* find disk */
			if ((j<0)||(j>=(int)disk_num)){
				fprintf(stderr,"invalid disk\n");
				ret=-1;
				goto change_curdir_done;
			}
			curdiskp=&disk[j];
			/* find partition */
			curpartp=akai_find_part(curdiskp,"A");
			if (curpartp==NULL){ /* not found? */
				ret=-1;
				goto change_curdir_done;
			}
			if ((np1!=NULL)||checklast){ /* not last or need to check last? */
				if (!curpartp->valid){
					fprintf(stderr,"invalid partition\n");
					ret=-1;
					goto change_curdir_done;
				}
				if ((curpartp->type!=PART_TYPE_FLL)&&(curpartp->type!=PART_TYPE_FLH)){
					printf("disk%i is not a floppy\n",j);
					ret=-1;
					goto change_curdir_done;
				}
			}
			/* get volume1 (volume index 0) */
			if (akai_get_vol(curpartp,&curvol_buf,0)<0){ /* not found? */
				fprintf(stderr,"invalid volume\n");
				ret=-1;
				goto change_curdir_done;
			}
			curvolp=&curvol_buf; /* only this one!!! */
			goto change_curdir_done;
		}
#endif
		/* find disk */
		if (strncasecmp(np0,"disk",4)==0){
			np0+=4;
		}
		if (strlen(np0)==0){ /* empty? */
			ret=-1;
			goto change_curdir_done;
		}
		j=atoi(np0);
		if ((j<0)||(j>=(int)disk_num)){
			fprintf(stderr,"invalid disk\n");
			ret=-1;
			goto change_curdir_done;
		}
		curdiskp=&disk[j];
		goto change_curdir_done;
	}

	if (curpartp==NULL){
		/* find partition */
		curpartp=akai_find_part(curdiskp,np0);
		if (curpartp==NULL){ /* not found? */
			ret=-1;
			goto change_curdir_done;
		}
		if ((np1!=NULL)||checklast){ /* not last or need to check last? */
			if (!curpartp->valid){
				fprintf(stderr,"invalid partition\n");
				ret=-1;
				goto change_curdir_done;
			}
		}
		goto change_curdir_done;
	}

	if (curvolp==NULL){
		if (curpartp->type==PART_TYPE_DD){
			printf("no volumes in DD partition\n");
			ret=-1;
			goto change_curdir_done;
		}
		/* find volume */
		if (akai_find_vol(curpartp,&curvol_buf,np0)<0){ /* not found? */
			ret=-1;
			goto change_curdir_done;
		}
		/* Note: akai_find_volume has checked for AKAI_VOL_TYPE_INACT */
		curvolp=&curvol_buf; /* only this one!!! */
		goto change_curdir_done;
	}

	/* no further directories */
	ret=-1;
	goto change_curdir_done;

change_curdir_done:
	if ((ret==0)&&(np1!=NULL)){ /* recursion requested? */
		ret=change_curdir(np1,vi,lastname,checklast); /* recursion */
	}
	if (ret<0){
		/* error, must restore state */
		curdiskp=tmpcurdiskp;
		curpartp=tmpcurpartp;
		curvolp=tmpcurvolp;
		if (curvolp!=NULL){ /* was saved? */
			*curvolp=tmpcurvol_buf; /* !!! */
			/* Note: restore orginal from copy!!! */
		}
	}
	return ret;
}

int
change_curdir_home(void)
{

	/* goto system level */
	curdiskp=NULL;
	curpartp=NULL;
	curvolp=NULL;

	if (disk_num==1){
		/* goto first disk */
		curdiskp=&disk[0];

		if (part_num==1){
			if (((part[0].type==PART_TYPE_FLL)||(part[0].type==PART_TYPE_FLH))){
				/* goto floppy volume */
				return change_curdir("/floppy",0,NULL,1); /* NULL,1: check last */
			}else if (part[0].type==PART_TYPE_HD9){
				/* goto fake partition A */
				return change_curdir("A",0,NULL,1); /* NULL,1: check last */
			}
		}
	}

	return 0;
}



void
save_curdir(int modflag)
{

	savecurdiskp=curdiskp;
	savecurpartp=curpartp;
	savecurvolp=curvolp;
	if (!modflag){
		/* will not modify volumes => OK to save and restore buffer */
		savecurvol_buf=curvol_buf;
	}else{
		/* save name */
		if (curvolp!=NULL){
			strcpy(savecurvolname,curvolp->name);
			/* save index for case of empty volume name */
			savecurvolindex=curvolp->index;
		}
	}
	savecurvolmodflag=modflag;
}

void
restore_curdir(void)
{
	char *np;

	 /* Note: OS keeps track of changes, no need to reload */
	curdiskp=savecurdiskp;

	/* Note: part[] keeps track of changes, no need to reload */
	curpartp=savecurpartp;
	if ((curpartp!=NULL)&&(curpartp->fd<0)){
		/* could happen */
		/* goto disk */
		curpartp=NULL;
		savecurvolp=NULL;
		savecurvolmodflag=0;
		printf("partition has changed, now in upper disk\n");
		/* XXX just stay in partition */
	}

	/* Note: only one volume buffer, saved contents might be out-of-date (e.g. rename, delete, create)!!! */
	curvolp=savecurvolp;
	if (!savecurvolmodflag){
		/* has not modified volumes => OK to save and restore buffer */
		curvol_buf=savecurvol_buf;
	}else{
		/* volume buffer could be out-of-date, must reload */
		if (curvolp!=NULL){
			curvolp=NULL; /* goto partition */
			if (savecurvolname[0]=='\0'){ /* empty volume name? */
				np=NULL; /* => change_curdir() below will use savecurvolindex */
			}else{
				np=savecurvolname; /* => change_curdir() below will use savecurvolname */
			}
			if (change_curdir(np,savecurvolindex,NULL,1)<0){ /* 1: check */
				/* could happen */
				printf("volume has changed, now in upper partition\n");
				/* XXX just stay in partition */
			}
		}
	}
}



void
curdir_info(int verbose)
{
	struct vol_s tmpvol;
	u_int vi;
	u_int ai;

	printf("/");
	if (curdiskp!=NULL){
		printf("disk%u",curdiskp->index);
		if (curpartp!=NULL){
			if (curpartp->type==PART_TYPE_DD){
				if (curpartp->letter==0){
					printf("/DD");
				}else{
					printf("/DD%u",(u_int)curpartp->letter);
				}
			}else{
				printf("/%c",curpartp->letter);
			}
			if (curvolp!=NULL){
				printf("/%s",curvolp->name);
			}
		}
	}

	if (verbose){
		printf("\n");
		if (curvolp!=NULL){
			ai=0; /* volume alias number unknown */
			if ((curpartp!=NULL)&&(curpartp->type==PART_TYPE_HD9)){
				/* S900 harddisk */
				/* determine volume alias number */
				ai=1; /* start number */
				/* add number of active volumes in partition before current volume */
				for (vi=0;(vi<curvolp->index)&&(vi<curpartp->volnummax);vi++){
					/* try to get volume */
					if (akai_get_vol(curpartp,&tmpvol,vi)<0){
						continue; /* next volume */
					}
					ai++; /* one more active volume */
				}
			}
			akai_vol_info(curvolp,ai,1); /* 1: verbose */
		}else if (curpartp!=NULL){
			akai_part_info(curpartp,1); /* 1: verbose */
		}else if (curdiskp!=NULL){
			akai_disk_info(curdiskp,1); /* 1: verbose */
		}else{
			printf("disks: %u\n",disk_num);
		}
	}
}

void
list_curdir(int recflag)
{

	if (curdiskp==NULL){
		akai_list_alldisks(recflag,curfiltertag);
		return;
	}

	if (curpartp==NULL){
		akai_list_disk(curdiskp,recflag,curfiltertag);
		return;
	}

	if (curvolp==NULL){
		akai_list_part(curpartp,recflag,curfiltertag);
		return;
	}

	akai_list_vol(curvolp,curfiltertag);
}



void
list_curfiltertags(void)
{
	u_int i;
	u_int noneflag;

	printf("filter tags: ");
	noneflag=1; /* none so far */
	for (i=0;i<AKAI_FILE_TAGNUM;i++){
		if ((curfiltertag[i]!=AKAI_FILE_TAGFREE)&&(curfiltertag[i]!=AKAI_FILE_TAGS1000)){ /* not free? */
			printf("%02u ",curfiltertag[i]);
			noneflag=0;
		}
	}
	if (noneflag){
		printf("none");
	}
	printf("\n");
}



int
copy_file(struct file_s *srcfp,struct vol_s *dstvp,struct file_s *dstfp,u_int dstindex,char *dstname,int delflag)
{
	struct file_s tmpfile;
	u_char *tmpbuf;
	int existsflag;

	if ((srcfp==NULL)||(dstvp==NULL)){
		return -1;
	}

	if (dstname==NULL){ /* no user-supplied name? */
		dstname=srcfp->name;
	}

	/* allocate buffer */
	if ((tmpbuf=(u_char *)malloc(srcfp->size))==NULL){
		perror("malloc");
		return -1;
	}

	/* check if destination file already exists */
	existsflag=0;
	if (dstindex==AKAI_CREATE_FILE_NOINDEX){ /* no user-supplied index? */
		/* check if file name already used in destination volume */
		if (akai_find_file(dstvp,&tmpfile,dstname)==0){
			existsflag=1;
		}
	}else{
		/* check if file with given index already exists in destination volume */
		if (akai_get_file(dstvp,&tmpfile,dstindex)==0){
			existsflag=1;
		}
	}
	if (existsflag){
		/* same file? Note: same partition and same block should be unique */
		if ((srcfp->volp->partp==tmpfile.volp->partp)&&(srcfp->bstart==tmpfile.bstart)){
			fprintf(stderr,"cannot copy to same file\n");
			free(tmpbuf);
			return -1;
		}
		/* exists */
		if (delflag){
			/* delete file */
			if (akai_delete_file(&tmpfile)<0){
				fprintf(stderr,"cannot overwrite existing file\n");
				free(tmpbuf);
				return -1;
			}
		}else{
			printf("file name already used\n");
			free(tmpbuf);
			return -1;
		}
	}

	/* create file */
	/* Note: akai_create_file() will correct osver if necessary */
	if (akai_create_file(dstvp,&tmpfile,srcfp->size,dstindex,dstname,
						 srcfp->osver, /* default: from source file */
						 (srcfp->volp->type==AKAI_VOL_TYPE_S900)?NULL:srcfp->tag)<0){ /* no tags from S900 */
		fprintf(stderr,"cannot create file\n");
		free(tmpbuf);
		return -1;
	}

	if (dstfp!=NULL){
		/* copy for information */
		*dstfp=tmpfile;
	}

	/* read file */
	if (akai_read_file(0,tmpbuf,srcfp,0,srcfp->size)<0){
		printf("read error\n");
		free(tmpbuf);
		return -1;
	}

	/* write file */
	if (akai_write_file(0,tmpbuf,&tmpfile,0,tmpfile.size)<0){
		printf("write error\n");
		free(tmpbuf);
		return -1;
	}

	free(tmpbuf);
	return 0;
}



int
copy_vol_allfiles(struct vol_s *srcvp,struct part_s *dstpp,char *dstname,int delflag,int verbose)
{
	struct vol_s tmpvol;
	struct file_s tmpfile;
	u_int fi;

	if ((srcvp==NULL)||(dstpp==NULL)){
		return -1;
	}
	if (dstpp->type==PART_TYPE_DD){
		return -1;
	}

	if (dstname==NULL){ /* no user-supplied name? */
		dstname=srcvp->name;
	}

	/* check if destination volume already exists */
	if (akai_find_vol(dstpp,&tmpvol,dstname)<0){
		/* does not exist (or error in finding it, don't care) */
		/* create volume */
		if (akai_create_vol(dstpp,&tmpvol,
							srcvp->type,
							AKAI_CREATE_VOL_NOINDEX,
							dstname,
							srcvp->lnum,
							srcvp->param)<0){
			fprintf(stderr,"cannot create destination volume\n");
			return -1;
		}
	}else{
		/* volume already exists */
		/* copy load number and volume parameters */
		if (akai_rename_vol(&tmpvol,NULL,
							srcvp->lnum,
							srcvp->osver,
							srcvp->param)<0){
			fprintf(stderr,"cannot update volume\n");
			return -1;
		}
	}

	/* same volume? Note: same partition and same block should be unique */
	if ((srcvp->partp==tmpvol.partp)&&(srcvp->dirblk[0]==tmpvol.dirblk[0])){
		fprintf(stderr,"cannot copy into same volume\n");
		return -1;
	}

	/* files in volume directory */
	for (fi=0;fi<srcvp->fimax;fi++){
		/* get source file */
		if (akai_get_file(srcvp,&tmpfile,fi)<0){
			continue; /* next file */
		}

		if (verbose>0){
			if (verbose>1){
				printf("%s/",srcvp->name);
			}
			printf("%s\n",tmpfile.name);
		}

		if (copy_file(&tmpfile,&tmpvol,NULL,AKAI_CREATE_FILE_NOINDEX,NULL,delflag)<0){
			return -1;
		}
	}

	return 0;
}


int
copy_part_allvols(struct part_s *srcpp,struct part_s *dstpp,int delflag,int verbose)
{
	u_int vi;
	struct vol_s tmpvol;

	if ((srcpp==NULL)||(dstpp==NULL)){
		return -1;
	}
	if ((srcpp->type==PART_TYPE_DD)||(dstpp->type==PART_TYPE_DD)){
		return -1;
	}

	/* same partition? Note: partition pointer is unique */
	if (srcpp==dstpp){
		fprintf(stderr,"cannot copy into same partition\n");
		return -1;
	}

	/* volumes in root directory */
	for (vi=0;(vi<srcpp->volnummax)&&(vi<dstpp->volnummax);vi++){
		/* get volume */
		if (akai_get_vol(srcpp,&tmpvol,vi)<0){
			continue; /* next volume */
		}

		if (verbose>1){
			printf("%s/\n",tmpvol.name);
		}

		/* copy all files in volume */
		if (copy_vol_allfiles(&tmpvol,dstpp,NULL,delflag,verbose)<0){
			return -1;
		}
	}

	return 0;
}



int
check_curnosamplervol(void)
{

	if (curpartp==NULL){ /* no partition? */
		return 1;
	}
#if 1
	/* XXX actually, curvolp==NULL should suffice */
	if (curpartp->type==PART_TYPE_DD){ /* DD partition? */
		return 1;
	}
#endif
	if (curvolp==NULL){ /* no sampler volume? */
		return 1;
	}
	/* now, inside a sampler volume */
	return 0;
}

int
check_curnosamplerpart(void)
{

	if (curpartp==NULL){ /* no partition? */
		return 1;
	}
	if (curpartp->type==PART_TYPE_DD){ /* DD partition? */
		return 1;
	}
	if (curvolp!=NULL){ /* sampler volume? */
		return 1;
	}
	/* now, on sampler partition level */
	return 0;
}

int
check_curnoddpart(void)
{

	if (curpartp==NULL){ /* no partition? */
		return 1;
	}
	if (curpartp->type!=PART_TYPE_DD){ /* no DD partition? */
		return 1;
	}
	/* now, on DD partition level */
	return 0;
}

int
check_curnopart(void)
{

	if (curpartp==NULL){ /* no partition? */
		return 1;
	}
	if (curvolp!=NULL){ /* sampler volume? */
		return 1;
	}
	/* now, on partition level (sampler or DD) */
	return 0;
}



/* EOF */
