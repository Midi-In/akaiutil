/*
* Copyright (C) 2012,2018,2019 Klaus Michael Indlekofer. All rights reserved.
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
#include "akaiutil_take.h"
#include "akaiutil_wav.h"



int
akai_ddtake_info(struct part_s *pp,u_int ti,int verbose)
{
	char namebuf[DIRNAMEBUF_LEN+1]; /* +1 for '\0' */
	struct akai_ddtake_s *tp;
	u_int cstarts,cstarte;
	u_int csizes,csizee;
	u_int scount;
	u_int srate;
	double vspeed;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		if (verbose){
			fprintf(stderr,"invalid take index\n");
		}
		return -1;
	}

	tp=&pp->head.dd.take[ti];

	/* clusters */
	cstarts=(pp->head.dd.take[ti].cstarts[1]<<8)
		    +pp->head.dd.take[ti].cstarts[0];
	if (cstarts==0){ /* empty? */
		if (verbose){
			printf("take not found\n");
		}
		return -1;
	}
	csizes=akai_count_ddfatchain(pp,cstarts);
	cstarte=(pp->head.dd.take[ti].cstarte[1]<<8)
		    +pp->head.dd.take[ti].cstarte[0];
	csizee=akai_count_ddfatchain(pp,cstarte);

	/* used sample count */
	scount=(pp->head.dd.take[ti].wend[3]<<24)
		+(pp->head.dd.take[ti].wend[2]<<16)
		+(pp->head.dd.take[ti].wend[1]<<8)
		+pp->head.dd.take[ti].wend[0]
		-(pp->head.dd.take[ti].wstart[3]<<24)
		-(pp->head.dd.take[ti].wstart[2]<<16)
		-(pp->head.dd.take[ti].wstart[1]<<8)
		-pp->head.dd.take[ti].wstart[0];

	/* samplerate */
	srate=(pp->head.dd.take[ti].srate[1]<<8)
		  +pp->head.dd.take[ti].srate[0];

	/* name */
	akai2ascii_name(tp->name,namebuf,0); /* 0: not S900 */

	if (verbose){
		printf("%s%s\n",namebuf,AKAI_DDTAKE_FNAMEEND);
		printf("tnr:      %u\n",ti+1);
		printf("tname:    \"%s\"\n",namebuf);
		printf("cstarts:  cluster 0x%04x\n",cstarts);
		printf("cstarte:  cluster 0x%04x\n",cstarte);
		printf("csizes:   0x%04x clusters (%9u bytes)\n",
			csizes,
			csizes*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE);
		printf("csizee:   0x%04x clusters (%9u bytes)\n",
			csizee,
			csizee*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE);
		printf("scount:   0x%08x (%9u)\n",scount,scount);
		printf("stype:    %s\n",tp->stype?"STEREO":"MONO");
		printf("srate:    %uHz\n",srate);
		vspeed=((double)((((int)(char)tp->vspeed[1])<<8)+((int)(u_char)tp->vspeed[0])))/256.0; /* in % */
		printf("vspeed:   %+.2lf%%\n",vspeed);
		printf("wstart:   0x%02x%02x%02x%02x\n",tp->wstart[3],tp->wstart[2],tp->wstart[1],tp->wstart[0]);
		printf("wend:     0x%02x%02x%02x%02x\n",tp->wend[3],tp->wend[2],tp->wend[1],tp->wend[0]);
		printf("wstartm:  0x%02x%02x%02x%02x\n",tp->wstartm[3],tp->wstartm[2],tp->wstartm[1],tp->wstartm[0]);
		printf("wendm:    0x%02x%02x%02x%02x\n",tp->wendm[3],tp->wendm[2],tp->wendm[1],tp->wendm[0]);
		printf("fadein:   %ums\n",(tp->fadein[1]<<8)+tp->fadein[0]);
		printf("fadeout:  %ums\n",(tp->fadeout[1]<<8)+tp->fadeout[0]);
		printf("stlvl:    %u\n",tp->stlvl);
		printf("pan:      %+i\n",(int)(char)tp->pan);
		printf("midich:   %u\n",tp->midich1+1);
		printf("midinote: %u\n",tp->midinote);
		printf("startm:   ");
		switch (tp->startm){
		case 0x00:
			printf("IMMEDIATE\n");
			break;
		case 0x01:
			printf("MIDI FOOTSW\n");
			break;
		case 0x02:
			printf("MIDI NOTE\n");
			break;
		case 0x03:
			printf("M.NOTE+DEL\n");
			break;
		case 0x04:
			printf("START SONG\n");
			break;
		default:
			printf("\?\?\?\n");
		}
		printf("predel:   %u\n",(tp->predel[1]<<8)+tp->predel[0]);
		printf("outlvl:   %u\n",tp->outlvl);
		printf("outch:    ");
		switch (tp->outch){
		case 0x00:
			printf("OFF\n");
			break;
		case 0x01:
			printf("1/2\n");
			break;
		case 0x02:
			printf("3/4\n");
			break;
		case 0x03:
			printf("5/6\n");
			break;
		case 0x04:
			printf("7/8\n");
			break;
		default:
			printf("\?\?\?\n");
		}
		printf("fxbus:    ");
		switch (tp->fxbus){
		case 0x00:
			printf("OFF\n");
			break;
		case 0x01:
			printf("FX1\n");
			break;
		case 0x02:
			printf("FX2\n");
			break;
		case 0x03:
			printf("RV3\n");
			break;
		case 0x04:
			printf("RV4\n");
			break;
		default:
			printf("\?\?\?\n");
		}
		printf("sendlvl:  %u\n",tp->sendlvl);
	}else{
		printf("%3u  %-12s  0x%04x  0x%04x  0x%04x  0x%04x  %9u  %5u  %s\n",
			ti+1,namebuf,
			cstarts,cstarte,
			csizes,csizee,
			scount,
			srate,
			pp->head.dd.take[ti].stype?"STEREO":"MONO  ");
	}

	return 0;
}



int
akai_import_take(int inpfd,struct part_s *pp,struct akai_ddtake_s *tp,u_int ti,u_int csizes,u_int csizee)
{
	u_int cstarts,cstarte;
	u_int samplesize;
	u_int envsiz;
	u_char *envbuf;
	u_int bsize;
	int ret;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (tp==NULL){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		return -1;
	}

	if (csizes==0){
		return 0;
	}

	/* Note: don't check if name already used, create new DD take */

	ret=-1; /* no success so far */
	envsiz=0;
	envbuf=NULL; /* not allocated yet */

	/* total number of bytes for sample */
	samplesize=csizes*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE;

	if (csizee==0){ /* no envelope in file? */
		/* new envelope */
		envsiz=((samplesize/2)+AKAI_DDTAKE_ENVBLKSIZW-1)/AKAI_DDTAKE_ENVBLKSIZW; /* number of bytes for envelope, round up (Note: /2 for 16bit per sample word) */
		csizee=(envsiz+AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE-1)/(AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE); /* in clusters, round up */
		/* allocate envelope buffer */
		if ((envbuf=malloc(envsiz))==NULL){
			fprintf(stderr,"cannot allocate envelope buffer\n");
			ret=-1;
			goto akai_import_take_exit;
		}
	}

	if ((csizes+csizee)*AKAI_DDPART_CBLKS>pp->bfree){
		fprintf(stderr,"not enough space left\n");
		ret=-1;
		goto akai_import_take_exit;
	}
	/* allocate space for take */
#if 1
	if (akai_allocate_ddfatchain(pp,csizes,&cstarts,1)<0){ /* 1: not necessarily contiguous */
#else
	if (akai_allocate_ddfatchain(pp,csizes,&cstarts,csizes)<0){ /* contiguous */
#endif
		fprintf(stderr,"cannot allocate FAT chain for sample\n");
		ret=-1;
		goto akai_import_take_exit;
	}
#if 1
	if (akai_allocate_ddfatchain(pp,csizee,&cstarte,1)<0){ /* 1: not necessarily contiguous */
#else
	if (akai_allocate_ddfatchain(pp,csizee,&cstarte,csizee)<0){ /* contiguous */
#endif
		fprintf(stderr,"cannot allocate FAT chain for envelope\n");
		/* free FAT at cstarts */
		akai_free_ddfatchain(pp,cstarts,1); /* XXX ignore error */
		ret=-1;
		goto akai_import_take_exit;
	}

	/* set cstarts and cstarte in DD take header */
	tp->cstarts[1]=0xff&(cstarts>>8);
	tp->cstarts[0]=0xff&cstarts;
	tp->cstarte[1]=0xff&(cstarte>>8);
	tp->cstarte[0]=0xff&cstarte;
	/* XXX keep name */
	/* copy DD take header into directory entry */
	bcopy(tp,&pp->head.dd.take[ti],sizeof(struct akai_ddtake_s));
	/* write partition header */
	if (akai_io_blks(pp,(u_char *)&pp->head.dd,
					 0,
					 AKAI_DDPARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		fprintf(stderr,"cannot write partition header\n");
		ret=-1;
		goto akai_import_take_exit;
	}

	if (csizes>0){
		/* sample */
		if (akai_import_ddfatchain(pp,cstarts,0,samplesize,inpfd,NULL)<0){
			fprintf(stderr,"cannot import DD take\n");
			ret=-1;
			goto akai_import_take_exit;
		}
	}

	if (csizee>0){
		/* envelope */
		if (envbuf!=NULL){ /* no envelope in file? (see above) */
			/* calculate envelope from sample */
			if (akai_take_setenv(pp,cstarts,samplesize,envbuf,envsiz)<0){
				ret=-1;
				goto akai_import_take_exit;
			}
			/* write envelope to DD take */
			if (akai_import_ddfatchain(pp,cstarte,0,envsiz,-1,envbuf)<0){
				fprintf(stderr,"cannot save envelope\n");
				ret=-1;
				goto akai_import_take_exit;
			}
		}else{
			/* import envelope from file */
			bsize=csizee*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE;
			if (akai_import_ddfatchain(pp,cstarte,0,bsize,inpfd,NULL)<0){
				fprintf(stderr,"cannot import DD take\n");
				ret=-1;
				goto akai_import_take_exit;
			}
		}
	}

	ret=0; /* success */

akai_import_take_exit:
	if (envbuf!=NULL){
		free(envbuf);
	}
	return ret;
}



int
akai_export_take(int outfd,struct part_s *pp,struct akai_ddtake_s *tp,u_int csizes,u_int csizee,u_int cstarts,u_int cstarte)
{
	u_int bsize;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (tp!=NULL){
		/* export DD take header */
		if (WRITE(outfd,tp,sizeof(struct akai_ddtake_s))!=(int)sizeof(struct akai_ddtake_s)){
			perror("write");
			return -1;
		}
	}

	if (csizes>0){
		/* sample */
		bsize=csizes*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE; /* in bytes */
		if (akai_export_ddfatchain(pp,cstarts,0,bsize,outfd,NULL)<0){
			fprintf(stderr,"cannot export DD take\n");
			return -1;
		}
	}

	if (csizee>0){
		/* envelope */
		bsize=csizee*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE; /* in bytes */
		if (akai_export_ddfatchain(pp,cstarte,0,bsize,outfd,NULL)<0){
			fprintf(stderr,"cannot export DD take\n");
			return -1;
		}
	}

	return 0;
}



int
akai_take2wav(struct part_s *pp,u_int ti,int wavfd,u_int *sizep,char **wavnamep,int what)
{
	/* Note: static for multiple calls with different what */
	static u_int cstarts;
	static u_int samplestart;
	static u_int samplesize;
	static u_int samplerate;
	static u_int samplechnr;
	static int ret;
	static char wavname[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */
	static int nlen;

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		return -1;
	}

	if (what&TAKE2WAV_CHECK){

		cstarts=(pp->head.dd.take[ti].cstarts[1]<<8)
			    +pp->head.dd.take[ti].cstarts[0];
		if (cstarts==0){ /* empty? */
			return -1;
		}

		ret=-1; /* no success so far */

		/* take parameters */
#if 1
		/* Note: words are 16bit */
		/* start word */
		samplestart=(pp->head.dd.take[ti].wstart[3]<<24)
				   +(pp->head.dd.take[ti].wstart[2]<<16)
				   +(pp->head.dd.take[ti].wstart[1]<<8)
				   +pp->head.dd.take[ti].wstart[0];
		/* end word */
		samplesize=(pp->head.dd.take[ti].wend[3]<<24)
				   +(pp->head.dd.take[ti].wend[2]<<16)
				   +(pp->head.dd.take[ti].wend[1]<<8)
				   +pp->head.dd.take[ti].wend[0];
		if (samplesize<samplestart){ /* end<start? */
			return -1;
		}
		samplesize-=samplestart; /* size in words */
		samplestart*=2; /* *2 for 16bit per sample word */
		samplesize*=2; /* *2 for 16bit per sample word */
		/* XXX check samplestart and samplesize */
#else
		/* all clusters */
		samplestart=0;
		samplesize=akai_count_ddfatchain(pp,cstarts)*AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE; /* in bytes */
#endif
		samplerate=(pp->head.dd.take[ti].srate[1]<<8)
				   +pp->head.dd.take[ti].srate[0];
		samplechnr=pp->head.dd.take[ti].stype?2:1;

#ifdef DEBUG
		printf("samplestart: %15u bytes\n",samplestart);
		printf("samplesize:  %15u bytes\n",samplesize);
		printf("samplerate:  %15u Hz\n",samplerate);
		printf("samplechnr:  %15u\n",samplechnr);
#endif

		if (sizep!=NULL){
			/* WAV file size */
			*sizep=WAV_HEAD_SIZE+samplesize;
#ifndef WAV_AKAIHEAD_DISABLE
			*sizep+=sizeof(struct wav_chunkhead_s)+sizeof(struct akai_ddtake_s); /* DD take header chunk (see below) */
#endif
		}

		/* create WAV name */
		akai2ascii_name(pp->head.dd.take[ti].name,wavname,0); /* 0: not S900 */
		nlen=(int)strlen(wavname);
		bcopy(".wav",wavname+nlen,5);

		if (wavnamep!=NULL){
			/* pointer to name (wavname must be static!) */
			*wavnamep=wavname;
		}
	}

	if (what&TAKE2WAV_EXPORT){

		if (what&TAKE2WAV_CREATE){
			/* create WAV file */
			if ((wavfd=OPEN(wavname,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
				perror("create WAV");
				goto akai_take2wav_exit;
			}
		}

		/* write WAV header */
		if (wav_write_head(wavfd,
						   samplesize,samplechnr,samplerate,16, /* 16: 16bit */
#ifndef WAV_AKAIHEAD_DISABLE
						   sizeof(struct wav_chunkhead_s)+sizeof(struct akai_ddtake_s) /* DD take header chunk (see below) */
#else
						   0
#endif
						   )<0){
			fprintf(stderr,"cannot write WAV header\n");
			goto akai_take2wav_exit;
		}

		/* export sample */
		/* Note: no sample format conversion necessary for S1100 and S3000 */
		if (akai_export_ddfatchain(pp,cstarts,samplestart,samplesize,wavfd,NULL)<0){
			perror("write WAV samples");
			goto akai_take2wav_exit;
		}

#ifndef WAV_AKAIHEAD_DISABLE
		{
			struct wav_chunkhead_s wavchunkhead;
			struct akai_ddtake_s t;
			u_int hdrsize;
			u_int csizes;

			/* create DD take header chunk */
			bcopy(WAV_CHUNKHEAD_AKAIDDTAKEHEADSTR,wavchunkhead.typestr,4);
			hdrsize=sizeof(struct akai_ddtake_s);
			wavchunkhead.csize[0]=0xff&hdrsize;
			wavchunkhead.csize[1]=0xff&(hdrsize>>8);
			wavchunkhead.csize[2]=0xff&(hdrsize>>16);
			wavchunkhead.csize[3]=0xff&(hdrsize>>24);
			/* write WAV chunk header */
			if (WRITE(wavfd,(u_char *)&wavchunkhead,sizeof(struct wav_chunkhead_s))!=(int)sizeof(struct wav_chunkhead_s)){
				perror("write WAV chunk");
				goto akai_take2wav_exit;
			}
			/* get DD take header from directory entry */
			bcopy((u_char *)&pp->head.dd.take[ti],&t,hdrsize);
			/* determine csizes (sample clusters) and csizee (envelope clusters) */
			csizes=akai_count_ddfatchain(pp,cstarts);
			/* Note: no envelope in WAV file -> csizee=0 */
			/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
			t.cstarts[1]=0xff&(csizes>>8);
			t.cstarts[0]=0xff&csizes;
			t.cstarte[1]=0x00;
			t.cstarte[0]=0x00;
			/* write DD take header */
			if (WRITE(wavfd,(u_char *)&t,hdrsize)!=(int)hdrsize){
				perror("write WAV chunk");
				goto akai_take2wav_exit;
			}
		}
#endif
#if 1
		printf("DD take exported to WAV\n");
#endif
	}

	ret=0; /* success */

akai_take2wav_exit:
	if (what&TAKE2WAV_CREATE){
		if (wavfd>=0){
			CLOSE(wavfd);
		}
	}
	return ret;
}



int
akai_wav2take(int wavfd,char *wavname,struct part_s *pp,u_int ti,u_int *bcountp,int what)
{
	/* Note: static for multiple calls with different what */
	static struct akai_ddtake_s t;
	static u_int chnr;
	static u_int samplerate;
	static u_int bitnr;
	static u_int samplesize;
	static char *errstrp;
	static int nlen;
	static char name[AKAI_NAME_LEN+1]; /* name (ASCII), +1 for '\0' */
	static u_int cstarts,cstarte;
	static u_int csizes,csizee;
	static u_int envsiz;
	static u_char *envbuf;
	static int ret;
	static u_int bcount;
#ifndef WAV_AKAIHEAD_DISABLE
	static u_int extrasize;
	static int wavakaiheadfound;
#endif

	if (wavname==NULL){
		return -1;
	}

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (ti>=AKAI_DDTAKE_MAXNUM){
		return -1;
	}

	/* Note: don't check if name already used, create new DD take */

	ret=-1; /* no success so far */
	bcount=0; /* no bytes read yet */
	envbuf=NULL; /* not allocated yet */

	/* check WAV name */
	nlen=(int)strlen(wavname);
	if ((nlen<4)||(strncasecmp(wavname+nlen-4,".wav",4)!=0)){
		/* unknown or unsupported */
		ret=1; /* no error */
		goto akai_wav2take_exit;
	}
	nlen-=4; /* remove suffix */

	if (what&WAV2TAKE_OPEN){
		/* open external WAV file */
		if ((wavfd=OPEN(wavname,O_RDONLY|O_BINARY,0))<0){
			perror("open WAV");
			goto akai_wav2take_exit;
		}
	}

	/* read and parse WAV header */
	if (wav_read_head(wavfd,&bcount,
					  &samplesize,&chnr,&samplerate,&bitnr,
#ifndef WAV_AKAIHEAD_DISABLE
					  &extrasize,
#else
					  NULL,
#endif
					  &errstrp, NULL, NULL, NULL)<0){
		if (errstrp!=NULL){
			fprintf(stderr,"%s\n",errstrp);
		}
		/* error, don't know how many bytes read */
		goto akai_wav2take_exit;
	}

	/* check parameters */
	if ((chnr!=1)&&(chnr!=2)){
		fprintf(stderr,"WAV must be mono or stereo\n");
		/* unknown or unsupported */
		ret=1; /* no error */
		goto akai_wav2take_exit;
	}
	if (bitnr!=16){
		fprintf(stderr,"WAV must be 16bit\n");
		/* unknown or unsupported */
		ret=1; /* no error */
		goto akai_wav2take_exit;
	}
	if (samplesize==0){
		fprintf(stderr,"WAV is empty\n");
		ret=1; /* no error */
		goto akai_wav2take_exit;
	}

	/* determine csizes (sample clusters) and csizee (envelope clusters) */
	csizes=(samplesize+AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE-1)/(AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE); /* in clusters, round up */
	envsiz=((samplesize/2)+AKAI_DDTAKE_ENVBLKSIZW-1)/AKAI_DDTAKE_ENVBLKSIZW; /* number of bytes for envelope, round up (Note: /2 for 16bit per sample word) */
	csizee=(envsiz+AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE-1)/(AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE); /* in clusters, round up */

	/* allocate envelope buffer */
	if ((envbuf=malloc(envsiz))==NULL){
		fprintf(stderr,"cannot allocate envelope buffer\n");
		ret=-1;
		goto akai_wav2take_exit;
	}

	/* allocate space for take */
#if 1
	if (akai_allocate_ddfatchain(pp,csizes,&cstarts,1)<0){ /* 1: not necessarily contiguous */
#else
	if (akai_allocate_ddfatchain(pp,csizes,&cstarts,csizes)<0){ /* contiguous */
#endif
		fprintf(stderr,"cannot allocate FAT chain for sample\n");
		ret=-1;
		goto akai_wav2take_exit;
	}
#if 1
	if (akai_allocate_ddfatchain(pp,csizee,&cstarte,1)<0){ /* 1: not necessarily contiguous */
#else
	if (akai_allocate_ddfatchain(pp,csizee,&cstarte,csizee)<0){ /* contiguous */
#endif
		fprintf(stderr,"cannot allocate FAT chain for envelope\n");
		/* free FAT at cstarts */
		akai_free_ddfatchain(pp,cstarts,1); /* XXX ignore error */
		ret=-1;
		goto akai_wav2take_exit;
	}

	if (csizes>0){
		/* read WAV sample and write sample to DD take */
		/* Note: no sample format conversion necessary for S1100 and S3000 */
		if (akai_import_ddfatchain(pp,cstarts,0,samplesize,wavfd,NULL)<0){
			fprintf(stderr,"cannot import DD take\n");
			ret=-1;
			goto akai_wav2take_exit;
		}
		bcount+=samplesize;
	}

	if (csizee>0){
		/* Note: no envelope in WAV file */
		/* calculate envelope from sample */
		if (akai_take_setenv(pp,cstarts,samplesize,envbuf,envsiz)<0){
			ret=-1;
			goto akai_wav2take_exit;
		}
		/* write envelope to DD take */
		if (akai_import_ddfatchain(pp,cstarte,0,envsiz,-1,envbuf)<0){
			fprintf(stderr,"cannot save envelope\n");
			ret=-1;
			goto akai_wav2take_exit;
		}
	}

#ifndef WAV_AKAIHEAD_DISABLE
	/* check for DD take header chunk in WAV file */
	{
		int wavakaiheadtype;
		u_int wavakaiheadsize;
		u_int bc;

		wavakaiheadtype=wav_find_akaihead(wavfd,&bc,&wavakaiheadsize,extrasize,WAV_AKAIHEADTYPE_DDTAKE);
		if (wavakaiheadtype<0){
			goto akai_wav2take_exit;
		}
		bcount+=bc;

		if ((wavakaiheadtype==(int)WAV_AKAIHEADTYPE_DDTAKE)&&(wavakaiheadsize==sizeof(struct akai_ddtake_s))){
			/* found matching DD take header chunk */
			wavakaiheadfound=1;
		}else{
			wavakaiheadfound=0;
		}
	}

	if (wavakaiheadfound){
		/* read DD take header */
		if (READ(wavfd,(u_char *)&t,sizeof(struct akai_ddtake_s))!=(int)sizeof(struct akai_ddtake_s)){
			fprintf(stderr,"cannot read DD take header\n");
			goto akai_wav2take_exit;
		}
		bcount+=sizeof(struct akai_ddtake_s);
#if 1
		printf("DD take header imported from WAV\n");
#endif
		/* Note: ignore cstarts/cstarte fields in DD take header */
		/* Note: keep name in DD take header */
		/* Note: must overwrite some settings in DD take header (see below) */
	}else
#endif
	{
		u_int wstart,wend;
		u_int wstartm,wendm;

		/* create DD take header */
		bzero((void *)&t,sizeof(struct akai_ddtake_s));
		/* default parameter settings */
		wstart=0;
		wend=samplesize/2; /* /2 for 16bit per sample word */
		wstartm=wstart;
		wendm=wend;
		t.wstart[3]=0xff&(wstart>>24);
		t.wstart[2]=0xff&(wstart>>16);
		t.wstart[1]=0xff&(wstart>>8);
		t.wstart[0]=0xff&wstart;
		t.wend[3]=0xff&(wend>>24);
		t.wend[2]=0xff&(wend>>16);
		t.wend[1]=0xff&(wend>>8);
		t.wend[0]=0xff&wend;
		t.dummy1=0x01; /* XXX */
		t.wstartm[3]=0xff&(wstartm>>24);
		t.wstartm[2]=0xff&(wstartm>>16);
		t.wstartm[1]=0xff&(wstartm>>8);
		t.wstartm[0]=0xff&wstartm;
		t.wendm[3]=0xff&(wendm>>24);
		t.wendm[2]=0xff&(wendm>>16);
		t.wendm[1]=0xff&(wendm>>8);
		t.wendm[0]=0xff&wendm;
		t.fadein[1]=0xff&(10>>8); /* XXX */
		t.fadein[0]=0xff&10; /* XXX */
		t.fadeout[1]=0xff&(50>>8); /* XXX */
		t.fadeout[0]=0xff&50; /* XXX */
		t.stlvl=99; /* XXX */
		t.dummy3=0x01; /* XXX */
		t.midich1=16-1; /* XXX */
		t.midinote=60; /* XXX */
		t.startm=0x03; /* XXX */
		t.predel[1]=0xff&(500>>8); /* XXX */
		t.predel[0]=0xff&500; /* XXX */
		t.outlvl=50; /* XXX */
		t.sendlvl=25; /* XXX */
		/* derive DD take name from WAV file name */
		if (nlen>AKAI_NAME_LEN){
			nlen=AKAI_NAME_LEN;
		}
		bcopy(wavname,name,nlen);
		name[nlen]='\0';
		ascii2akai_name(name,t.name,0); /* 0: not S900 */
	}
	/* fundamental settings of DD take */
	t.cstarts[1]=0xff&(cstarts>>8);
	t.cstarts[0]=0xff&cstarts;
	t.cstarte[1]=0xff&(cstarte>>8);
	t.cstarte[0]=0xff&cstarte;
	t.stype=(chnr==2)?0x01:0x00;
	t.srate[1]=0xff&(samplerate>>8);
	t.srate[0]=0xff&samplerate;

	/* copy DD take header into directory entry */
	bcopy(&t,&pp->head.dd.take[ti],sizeof(struct akai_ddtake_s));
	/* write partition header */
	if (akai_io_blks(pp,(u_char *)&pp->head.dd,
					 0,
					 AKAI_DDPARTHEAD_BLKS,
					 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
		fprintf(stderr,"cannot write partition header\n");
		ret=-1;
		goto akai_wav2take_exit;
	}

#if 1
	printf("DD take imported from WAV\n");
#endif

	ret=0; /* success */

akai_wav2take_exit:
	if (what&WAV2TAKE_OPEN){
		if (wavfd>=0){
			CLOSE(wavfd);
		}
	}
	if (bcountp!=NULL){
		*bcountp=bcount;
	}
	if (envbuf!=NULL){
		free(envbuf);
	}
	return ret;
}



/* set envelope for take */
int
akai_take_setenv(struct part_s *pp,u_int cstarts,u_int samplesize,u_char *envbuf,u_int envsiz)
{
	static u_char logabstab[0x100]; /* must be static */
	static int logabstabready=0; /* must be static */
	u_char l,lmax;
	u_int i,j;
	u_int ba,bremain,bchunk,la,lchunk;
	static u_char sbuf[AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE]; /* 1 cluster */

	if (!logabstabready){ /* logabstab not initialized yet? */
		double lconst;

		/* initialize logabstab */
		lconst=((double)(AKAI_DDTAKE_ENVMAXVAL-1))/log((double)0x80);
		logabstab[0x00]=0;
		for (i=0x01;i<=0x7f;i++){
			logabstab[i]=((u_char)(lconst*log((double)i)))+1;
		}
		logabstab[0x80]=AKAI_DDTAKE_ENVMAXVAL;
		for (i=0x81;i<=0xff;i++){
			logabstab[i]=logabstab[0x100-i];
		}

		logabstabready=1; /* logabstab is initialized */
	}

	if ((pp==NULL)||(!pp->valid)||(pp->fd<0)||(pp->fat==NULL)){
		return -1;
	}
	if (pp->type!=PART_TYPE_DD){
		return -1;
	}

	if (envbuf==NULL){
		return -1;
	}

	/* zero envbuf */
	bzero(envbuf,envsiz);

	if (samplesize==0){
		return 0;
	}

	/* calculate envelope of samples */
	ba=0;
	bremain=samplesize;
	i=0;
	while ((bremain>0)&&(i<envsiz)){
		/* cluster */
		bchunk=AKAI_DDPART_CBLKS*AKAI_HD_BLOCKSIZE;
		if (bchunk>bremain){
			bchunk=bremain;
		}

		/* read samples from take */
		if (akai_export_ddfatchain(pp,cstarts,ba,bchunk,-1,sbuf)<0){
			fprintf(stderr,"cannot read take\n");
#if 1
			break;
#else
			return -1;
#endif
		}

		la=0;
		while ((bchunk>0)&&(i<envsiz)){
			/* envelope block */
			lchunk=AKAI_DDTAKE_ENVBLKSIZW<<1; /* *2 for 16bit per sample word */
			if (lchunk>bchunk){
				lchunk=bchunk;
			}
			/* determine peak level within envelope block */
			lmax=0;
			for (j=0;j<lchunk;j+=2){ /* 2 for 16bit per sample word */
				l=logabstab[sbuf[la+j+1]]; /* logabs of MSB */
				if (l>lmax){
					lmax=l;
				}
			}
			envbuf[i]=lmax;

			ba+=lchunk;
			bremain-=lchunk;
			la+=lchunk;
			bchunk-=lchunk;
			i++;
		}
	}

	return 0;
}



/* EOF */
