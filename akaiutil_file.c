/*
* Copyright (C) 2010,2012,2018,2019 Klaus Michael Indlekofer. All rights reserved.
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
#include "akaiutil_wav.h"



/* AKAI file info */
int
akai_file_info(struct file_s *fp,int verbose)
{
	static u_char hdrbuf[AKAI_FL_BLOCKSIZE]; /* XXX enough */
	u_int hdrsize;
	struct akai_genfilehdr_s *hdrp;
	static char nbuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	int s900flag;
	int sampleflag;
	int programflag;
	u_int i;

	if ((fp==NULL)
		||(fp->volp==NULL)
		||(fp->index>=fp->volp->fimax)){
		return -1;
	}

	if (verbose){
		printf("%s\n",fp->name);
		printf("fnr:     %u\n",fp->index+1);
		printf("start:   block 0x%04x\n",fp->bstart);
		printf("size:    %u bytes\n",fp->size);
		/* file type */
		printf("type:    ");
		if (fp->type==AKAI_CDSETUP3000_FTYPE){
			/* CD3000 CD-ROM setup file */
			/* Note: generic file header does not apply for this file type! */
			printf("CD3000 CD-ROM setup\n");
			return akai_cdsetup3000_info(fp);
		}
		hdrsize=sizeof(struct akai_genfilehdr_s); /* default */
		s900flag=0; /* default */
		sampleflag=0; /* default */
		programflag=0; /* default */
		switch (fp->type){
		case AKAI_SAMPLE900_FTYPE: /* S900 sample file */
			printf("S900 sample\n");
			hdrsize=sizeof(struct akai_sample900_s);
			s900flag=1;
			sampleflag=1;
			break;
		case AKAI_SAMPLE1000_FTYPE: /* S1000 sample file */
			printf("S1000 sample\n");
			hdrsize=sizeof(struct akai_sample1000_s);
			sampleflag=1;
			break;
		case AKAI_SAMPLE3000_FTYPE: /* S3000 sample file */
			printf("S3000 sample\n");
			hdrsize=sizeof(struct akai_sample3000_s);
			sampleflag=1;
			break;
		case AKAI_CDSAMPLE3000_FTYPE: /* CD3000 CD-ROM sample parameters file */
			printf("CD3000 CD-ROM sample parameters\n");
			break;
		case AKAI_PROGRAM900_FTYPE: /* S900 program file */
			printf("S900 program\n");
			s900flag=1;
			programflag=1;
			break;
		case AKAI_PROGRAM1000_FTYPE: /* S1000 program file */
			printf("S1000 program\n");
			programflag=1;
			break;
		case AKAI_PROGRAM3000_FTYPE: /* S3000 program file */
			printf("S3000 program\n");
			programflag=1;
			break;
		case AKAI_DRUM900_FTYPE: /* S900 drum settings file */
			printf("S900 drum settings\n");
			/*s900flag=1;*/
			return 0; /* done */
		case AKAI_DRUMFILE_FTYPE: /* S1000 or S3000 drum settings file */
			printf("S1000/S3000 drum settings\n");
			break;
		case AKAI_FXFILE_FTYPE: /* S1100 or S3000 effects file */
			printf("S1100/S3000 effects\n");
			break;
		case AKAI_QLFILE_FTYPE: /* S1100 or S3000 cue-list file */
			printf("S1100/S3000 cue-list\n");
			break;
		case AKAI_TLFILE_FTYPE: /* S1100 or S3000 take-list file */
			printf("S1100/S3000 take-list\n");
			break;
		case AKAI_MULTI3000_FTYPE: /* S3000 multi file */
			printf("S3000 multi\n");
			break; /* use generic file header */
		case AKAI_FIXUP900_FTYPE: /* S900 fixup file */
			printf("S900 fixup\n");
			/*s900flag=1;*/
			return 0; /* done */
		case AKAI_MEMIMG900_FTYPE: /* S900 memory image file */
			printf("S900 memory image\n");
			/*s900flag=1;*/
			return 0; /* done */
		case AKAI_SYS1000_FTYPE: /* S1000 operating system file */
			printf("S1000 operating system\n");
			return 0; /* done */
		case AKAI_SYS3000_FTYPE: /* S3000 operating system file */
			printf("S3000 operating system\n");
			return 0; /* done */
		case AKAI_OVS900_FTYPE: /* S900 overall settings file */
			printf("S900 overall settings\n");
			/*s900flag=1;*/
			return 0; /* done */
		default:
			/* unknown or unsupported */
			printf("\?\?\?\n");
			return 1; /* no error */
		}

		/* read header */
		if (akai_read_file(0,hdrbuf,fp,0,hdrsize)<0){
			fprintf(stderr,"cannot read header\n");
			return -1;
		}

		/* name */
		if (s900flag){ /* S900 type? */
			/* Note: name in S900 sample/program file header starts at byte 0 */
			akai2ascii_name(hdrbuf,nbuf,1); /* 1: S900 */
		}else{
			/* generic file header */
			hdrp=(struct akai_genfilehdr_s *)hdrbuf;
			akai2ascii_name(hdrp->name,nbuf,0); /* 0: not S900 */
		}
		printf("ramname: \"%s\"\n",nbuf);
		if (sampleflag){
			/* sample info */
			akai_sample_info(fp,hdrbuf);
		}else if (programflag){
			/* program info */
			return akai_program_info(fp);
		}
	}else{
		printf("%3u  %-16s %9u    0x%04x  ",fp->index+1,fp->name,fp->size,fp->bstart);
		if (fp->volp->type==AKAI_VOL_TYPE_S900){
			/* S900 volume */
			if (fp->osver!=0){ /* compressed file? */
				/* number of un-compressed floppy blocks */
				printf("  %5u",fp->osver);
			}
		}else{
			/* S1000/S3000 volume */
			/* OS version */
			if (((0xff&(fp->osver>>8))<100)&&((0xff&fp->osver)<100)){
				printf("%2u.%02u  ",0xff&(fp->osver>>8),0xff&fp->osver);
			}else{
				printf("%5u  ",fp->osver);
			}
			/* tags */
			for (i=0;i<AKAI_FILE_TAGNUM;i++){
				if ((fp->tag[i]>=1)&&(fp->tag[i]<=AKAI_PARTHEAD_TAGNUM)){
					printf("%02u ",fp->tag[i]);
				}else if ((fp->tag[i]==AKAI_FILE_TAGFREE)||(fp->tag[i]==AKAI_FILE_TAGS1000)){
					printf("   ");
				}else{
					printf("?  ");
				}
			}
		}
		printf("\n");
	}

	return 0;
}

void
akai_sample_info(struct file_s *fp,u_char *hdrp)
{
	struct akai_sample3000_s *s3000hdrp;
	struct akai_sample900_s *s900hdrp;
	u_int samplecount;
	u_int samplerate;
	u_int i,j,k;

	if ((fp==NULL)||(hdrp==NULL)){
		return;
	}

	if (fp->type==AKAI_SAMPLE900_FTYPE){
		/* S900 sample */
		s900hdrp=(struct akai_sample900_s *)hdrp;
		/* number of samples */
		/* XXX should be an even number */
		samplecount=(s900hdrp->slen[3]<<24)
			+(s900hdrp->slen[2]<<16)
			+(s900hdrp->slen[1]<<8)
			+s900hdrp->slen[0];
		printf("scount:  0x%08x (%9u)\n",samplecount,samplecount);
		samplerate=(s900hdrp->srate[1]<<8)+s900hdrp->srate[0];
		printf("srate:   %uHz\n",samplerate);
		if (samplerate>0){
			printf("sdur:    %.3lfms\n",1000.0*((double)samplecount)/((double)samplerate));
		}
		printf("npitch:  %.2lf\n",((double)((s900hdrp->npitch[1]<<8)+s900hdrp->npitch[0]))/16.0);
		printf("loud:    %+i\n",(((int)(char)s900hdrp->loud[1])<<8)+((int)(u_char)s900hdrp->loud[0]));
		printf("dmadesa: 0x%02x%02x\n",
			s900hdrp->dmadesa[1],
			s900hdrp->dmadesa[0]);
		printf("locat:   0x%02x%02x%02x%02x\n",
			s900hdrp->locat[3],
			s900hdrp->locat[2],
			s900hdrp->locat[1],
			s900hdrp->locat[0]);
		printf("start:   0x%02x%02x%02x%02x\n",
			s900hdrp->start[3],
			s900hdrp->start[2],
			s900hdrp->start[1],
			s900hdrp->start[0]);
		printf("end:     0x%02x%02x%02x%02x\n",
			s900hdrp->end[3],
			s900hdrp->end[2],
			s900hdrp->end[1],
			s900hdrp->end[0]);
		printf("llen:    0x%02x%02x%02x%02x\n",
			s900hdrp->llen[3],
			s900hdrp->llen[2],
			s900hdrp->llen[1],
			s900hdrp->llen[0]);
		printf("pmode:   ");
		switch (s900hdrp->pmode){
		case SAMPLE900_PMODE_ONESHOT:
			printf("ONESHOT\n");
			break;
		case SAMPLE900_PMODE_LOOP:
			printf("LOOP\n");
			break;
		case SAMPLE900_PMODE_ALTLOOP:
			printf("ALTLOOP\n");
			break;
		default:
			printf("\?\?\?\n");
			break;
		}
		printf("dir:     ");
		switch (s900hdrp->dir){
		case SAMPLE900_DIR_NORM:
			printf("NORM\n");
			break;
		case SAMPLE900_DIR_REV:
			printf("REV\n");
			break;
		default:
			printf("\?\?\?\n");
			break;
		}
		printf("type:    ");
		switch (s900hdrp->type){
		case SAMPLE900_TYPE_NORM:
			printf("NORM\n");
			break;
		case SAMPLE900_TYPE_VELXF:
			printf("VELXF\n");
			break;
		default:
			printf("\?\?\?\n");
			break;
		}
		printf("compr:   %s\n",
			(fp->osver!=0)?"ON":"OFF"); /* S900 compressed/non-compressed sample format */
	}else if ((fp->type==AKAI_SAMPLE1000_FTYPE)||(fp->type==AKAI_SAMPLE3000_FTYPE)){
		/* S1000/S3000 sample */
		s3000hdrp=(struct akai_sample3000_s *)hdrp;
		/* Note: S1000 header is contained within S3000 header */
		/* number of samples */
		samplecount=(s3000hdrp->s1000.slen[3]<<24)
			+(s3000hdrp->s1000.slen[2]<<16)
			+(s3000hdrp->s1000.slen[1]<<8)
			+s3000hdrp->s1000.slen[0];
		printf("scount:  0x%08x (%9u)\n",samplecount,samplecount);
		samplerate=(s3000hdrp->s1000.srate[1]<<8)+s3000hdrp->s1000.srate[0];
		printf("srate:   %uHz\n",samplerate);
		if (samplerate>0){
			printf("sdur:    %.3lfms\n",1000.0*((double)samplecount)/((double)samplerate));
		}
		printf("bandw:   ");
		switch (s3000hdrp->s1000.bandw){
		case SAMPLE1000_BANDW_10KHZ:
			printf("10kHz\n");
			break;
		case SAMPLE1000_BANDW_20KHZ:
			printf("20kHz\n");
			break;
		default:
			printf("\?\?\?\n");
			break;
		}
		printf("rkey:    %u\n",s3000hdrp->s1000.rkey);
		printf("ctune:   %+i\n",(int)(char)s3000hdrp->s1000.ctune);
		printf("stune:   %+i\n",(int)(char)s3000hdrp->s1000.stune);
		printf("hltoff:  %+i\n",(int)(char)s3000hdrp->s1000.hltoff);
		k=(s3000hdrp->s1000.stpaira[1]<<8)+s3000hdrp->s1000.stpaira[0];
		if (k!=AKAI_SAMPLE1000_STPAIRA_NONE){
			if (fp->type==AKAI_SAMPLE3000_FTYPE){
				k*=SAMPLE3000_STPAIRA_MULT;
				printf("stpaira: 0x%05x\n",k);
			}else{
				printf("stpaira: 0x%04x\n",k);
			}
		}
		printf("locat:   0x%02x%02x%02x%02x\n",
			s3000hdrp->s1000.locat[3],
			s3000hdrp->s1000.locat[2],
			s3000hdrp->s1000.locat[1],
			s3000hdrp->s1000.locat[0]);
		printf("start:   0x%02x%02x%02x%02x\n",
			s3000hdrp->s1000.start[3],
			s3000hdrp->s1000.start[2],
			s3000hdrp->s1000.start[1],
			s3000hdrp->s1000.start[0]);
		printf("end:     0x%02x%02x%02x%02x\n",
			s3000hdrp->s1000.end[3],
			s3000hdrp->s1000.end[2],
			s3000hdrp->s1000.end[1],
			s3000hdrp->s1000.end[0]);
		printf("pmode:   ");
		switch (s3000hdrp->s1000.pmode){
		case SAMPLE1000_PMODE_LOOP:
			printf("LOOP\n");
			break;
		case SAMPLE1000_PMODE_LOOPNOTREL:
			printf("LOOPNOTREL\n");
			break;
		case SAMPLE1000_PMODE_NOLOOP:
			printf("NOLOOP\n");
			break;
		case SAMPLE1000_PMODE_TOEND:
			printf("TOEND\n");
			break;
		default:
			printf("\?\?\?\n");
			break;
		}
		printf("lnum:    %u\n",s3000hdrp->s1000.lnum);
		printf("lfirst:  %u\n",s3000hdrp->s1000.lfirst+1);
		for (i=0;i<AKAI_SAMPLE1000_LOOPNUM;i++){
			k=(s3000hdrp->s1000.loop[i].time[1]<<8)+s3000hdrp->s1000.loop[i].time[0];
			if (k==SAMPLE1000LOOP_TIME_NOLOOP){
				continue; /* next loop */
			}
			j=(s3000hdrp->s1000.loop[i].at[3]<<24)
				+(s3000hdrp->s1000.loop[i].at[2]<<16)
				+(s3000hdrp->s1000.loop[i].at[1]<<8)
				+s3000hdrp->s1000.loop[i].at[0];
			if (j>=samplecount){ /* invalid? */
				continue; /* next loop */
			}
			printf("loop %u:\n",i+1);
			printf("  loopat:  0x%08x\n",j);
			printf("  length:  0x%02x%02x%02x%02x.0x%02x%02x\n",
				s3000hdrp->s1000.loop[i].len[3],
				s3000hdrp->s1000.loop[i].len[2],
				s3000hdrp->s1000.loop[i].len[1],
				s3000hdrp->s1000.loop[i].len[0],
				s3000hdrp->s1000.loop[i].flen[1],
				s3000hdrp->s1000.loop[i].flen[0]);
			if (k==SAMPLE1000LOOP_TIME_HOLD){
				printf("  ltime:   HOLD\n");
			}else{
				printf("  ltime:   %ums\n",k);
			}
		}
	}
}

int
akai_program_info(struct file_s *fp)
{
	static char nbuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	struct akai_program900_s *s900hdrp;
	struct akai_program900kg_s *s900kgp;
	struct akai_program3000_s *s3000hdrp;
	struct akai_program3000kg_s *s3000kgp;
	u_char *buf;
	u_int hdrsiz;
	u_int kgsiz;
	u_int kgnum;
	u_int i,j,k;

	if ((fp==NULL)||(fp->volp==NULL)||(fp->index>=fp->volp->fimax)){
		return -1;
	}

	if (fp->type==AKAI_PROGRAM900_FTYPE){
		hdrsiz=sizeof(struct akai_program900_s);
		kgsiz=sizeof(struct akai_program900kg_s);
	}else if (fp->type==AKAI_PROGRAM1000_FTYPE){
		hdrsiz=sizeof(struct akai_program1000_s);
		kgsiz=sizeof(struct akai_program1000kg_s);
	}else if (fp->type==AKAI_PROGRAM3000_FTYPE){
		hdrsiz=sizeof(struct akai_program3000_s);
		kgsiz=sizeof(struct akai_program3000kg_s);
	}else{
		return -1;
	}

	/* allocate buffer */
	if (fp->size<hdrsiz){
		fprintf(stderr,"invalid file size\n");
		return -1;
	}
	if ((buf=malloc(fp->size))==NULL){
		fprintf(stderr,"cannot allocate memory\n");
		return -1;
	}

	/* read file */
	if (akai_read_file(0,buf,fp,0,fp->size)<0){
		fprintf(stderr,"cannot read file\n");
		free(buf);
		return -1;
	}

	if (fp->type==AKAI_PROGRAM900_FTYPE){
		/* S900 program */
		s900hdrp=(struct akai_program900_s *)buf;
		printf("kgxf:    %s\n",(s900hdrp->kgxf!=0x00)?"ON":"OFF");
		kgnum=s900hdrp->kgnum;
		printf("kgnum:   %u\n",kgnum);
		if (fp->size<hdrsiz+kgnum*kgsiz){
			fprintf(stderr,"invalid file size\n");
			free(buf);
			return -1;
		}
		k=(s900hdrp->kg1a[1]<<8)+s900hdrp->kg1a[0];
		if (k!=PROGRAM900_KGA_NONE){
			printf("kg1a:    0x%04x\n",k);
		}
		for (i=0;i<kgnum;i++){
			printf("keygroup %u:\n",i+1);
			s900kgp=(struct akai_program900kg_s *)(buf+hdrsiz+i*kgsiz);
			printf("  midichoff: %u\n",s900kgp->midichoff);
			printf("  key lo-hi: %u-%u\n",s900kgp->keylo,s900kgp->keyhi);
			printf("  outch:     ");
			switch (s900kgp->outch1){
			case PROGRAM900KG_OUTCH1_LEFT:
				printf("LEFT\n");
				break;
			case PROGRAM900KG_OUTCH1_RIGHT:
				printf("RIGHT\n");
				break;
			case PROGRAM900KG_OUTCH1_ANY:
				printf("ANY\n");
				break;
			default:
				printf("%u\n",s900kgp->outch1+1);
				break;
			}
			printf("  pitch:     %s\n",((PROGRAM900KG_FLAGS_PCONST&s900kgp->flags)!=0x00)?"CONST":"TRACK");
			printf("  oneshot:   %s\n",((PROGRAM900KG_FLAGS_ONESHOT&s900kgp->flags)!=0x00)?"ON":"OFF");
			printf("  velxf:     %s\n",((PROGRAM900KG_FLAGS_VELXF&s900kgp->flags)!=0x00)?"ON":"OFF");
			printf("  velxfv50:  %u\n",s900kgp->velxfv50);
			printf("  velswth:   %u",s900kgp->velswth);
			if (s900kgp->velswth<=PROGRAM900KG_VELSWTH_NOSOFT){
				printf(" (no soft sample)\n");
			}else if (s900kgp->velswth>=PROGRAM900KG_VELSWTH_NOLOUD){
				printf(" (no loud sample)\n");
			}else{
				printf("\n");
			}
			printf("  sample 1 (soft):\n");
			printf("    tune:    %+.2lf\n",((double)((((int)(char)s900kgp->tune1[1])<<8)+((int)(u_char)s900kgp->tune1[0])))/16.0);
			printf("    filter:  %u\n",s900kgp->filter1);
			printf("    loud:    %+i\n",(int)(char)s900kgp->loud1);
			akai2ascii_name(s900kgp->sname1,nbuf,1); /* 1: S900 */
			printf("    sname:   \"%s\"\n",nbuf);
			k=(s900kgp->shdra1[1]<<8)+s900kgp->shdra1[0];
			if (k!=PROGRAM900KG_SHDRA_NONE){
				printf("    shdra:   0x%04x\n",k);
			}
			printf("  sample 2 (loud):\n");
			printf("    tune:    %+.2lf\n",((double)((((int)(char)s900kgp->tune2[1])<<8)+((int)(u_char)s900kgp->tune2[0])))/16.0);
			printf("    filter:  %u\n",s900kgp->filter2);
			printf("    loud:    %+i\n",(int)(char)s900kgp->loud2);
			akai2ascii_name(s900kgp->sname2,nbuf,1); /* 1: S900 */
			printf("    sname:   \"%s\"\n",nbuf);
			k=(s900kgp->shdra2[1]<<8)+s900kgp->shdra2[0];
			if (k!=PROGRAM900KG_SHDRA_NONE){
				printf("    shdra:   0x%04x\n",k);
			}
			k=(s900kgp->kgnexta[1]<<8)+s900kgp->kgnexta[0];
			if (k!=PROGRAM900_KGA_NONE){
				printf("  kgnexta:   0x%04x\n",k);
			}
		}
	}else{
		/* S1000/S3000 program */
		s3000hdrp=(struct akai_program3000_s *)buf;
		/* Note: S1000 header is contained within S3000 header */
		printf("midich:    ");
		if (s3000hdrp->s1000.midich1==PROGRAM1000_MIDICH1_OMNI){
			printf("OMNI\n");
		}else{
			printf("%u\n",s3000hdrp->s1000.midich1+1);
		}
		printf("key lo-hi: %u-%u\n",s3000hdrp->s1000.keylo,s3000hdrp->s1000.keyhi);
		printf("oct:       %+i\n",(int)(char)s3000hdrp->s1000.oct);
		printf("auxch:     ");
		if (s3000hdrp->s1000.auxch1==PROGRAM1000_AUXCH1_OFF){
			printf("OFF\n");
		}else{
			printf("%u\n",s3000hdrp->s1000.auxch1+1);
		}
		printf("kgxf:      %s\n",(s3000hdrp->s1000.kgxf!=0x00)?"ON":"OFF");
		kgnum=s3000hdrp->s1000.kgnum;
		printf("kgnum:     %u\n",kgnum);
		if (fp->size<hdrsiz+kgnum*kgsiz){
			fprintf(stderr,"invalid file size\n");
			free(buf);
			return -1;
		}
		k=(s3000hdrp->s1000.kg1a[1]<<8)+s3000hdrp->s1000.kg1a[0];
		if (k!=PROGRAM1000_KGA_NONE){
			if (fp->type==AKAI_PROGRAM3000_FTYPE){
				k*=PROGRAM3000_KGA_MULT;
				printf("kg1a:      0x%05x\n",k);
			}else{
				printf("kg1a:      0x%04x\n",k);
			}
		}
		for (i=0;i<kgnum;i++){
			printf("keygroup %u:\n",i+1);
			s3000kgp=(struct akai_program3000kg_s *)(buf+hdrsiz+i*kgsiz);
			printf("  key lo-hi: %u-%u\n",s3000kgp->s1000.keylo,s3000kgp->s1000.keyhi);
			printf("  ctune:     %+i\n",(int)(char)s3000kgp->s1000.ctune);
			printf("  stune:     %+i\n",(int)(char)s3000kgp->s1000.stune);
			printf("  filter:    %u\n",s3000kgp->s1000.filter);
			printf("  velxf:     %s\n",(s3000kgp->s1000.velxf!=0x00)?"ON":"OFF");
			for (j=0;j<PROGRAM1000KG_VELZONENUM;j++){
				printf("  velzone %u:\n",j+1);
				akai2ascii_name(s3000kgp->s1000.velzone[j].sname,nbuf,0); /* 0: not S900 */
				printf("    vel lo-hi: %u-%u\n",s3000kgp->s1000.velzone[j].vello,s3000kgp->s1000.velzone[j].velhi);
				printf("    ctune:     %+i\n",(int)(char)s3000kgp->s1000.velzone[j].ctune);
				printf("    stune:     %+i\n",(int)(char)s3000kgp->s1000.velzone[j].stune);
				printf("    loud:      %+i\n",(int)(char)s3000kgp->s1000.velzone[j].loud);
				printf("    filter:    %+i\n",(int)(char)s3000kgp->s1000.velzone[j].filter);
				printf("    pan:       %+i\n",(int)(char)s3000kgp->s1000.velzone[j].pan);
				printf("    auxchoff:  %u\n",s3000kgp->s1000.auxchoff[j]);
				printf("    pitch:     %s\n",(s3000kgp->s1000.pconst[j]!=0x00)?"CONST":"TRACK");
				printf("    pmode:     ");
				switch (s3000kgp->s1000.velzone[j].pmode){
				case PROGRAM1000_PMODE_SAMPLE:
					printf("SAMPLE\n");
					break;
				case PROGRAM1000_PMODE_LOOP:
					printf("LOOP\n");
					break;
				case PROGRAM1000_PMODE_LOOPNOTREL:
					printf("LOOPNOTREL\n");
					break;
				case PROGRAM1000_PMODE_NOLOOP:
					printf("NOLOOP\n");
					break;
				case PROGRAM1000_PMODE_TOEND:
					printf("TOEND\n");
					break;
				default:
					printf("\?\?\?\n");
					break;
				}
				printf("    sname:     \"%s\"\n",nbuf);
				k=(s3000kgp->s1000.velzone[j].shdra[1]<<8)+s3000kgp->s1000.velzone[j].shdra[0];
				if (k!=PROGRAM1000KG_SHDRA_NONE){
					if (fp->type==AKAI_PROGRAM3000_FTYPE){
						k*=PROGRAM3000_SHDRA_MULT;
						printf("    shdra:     0x%05x\n",k);
					}else{
						printf("    shdra:     0x%04x\n",k);
					}
				}
			}
			k=(s3000kgp->s1000.kgnexta[1]<<8)+s3000kgp->s1000.kgnexta[0];
			if (k!=PROGRAM1000_KGA_NONE){
				if (fp->type==AKAI_PROGRAM3000_FTYPE){
					k*=PROGRAM3000_KGA_MULT;
					printf("  kgnexta:   0x%05x\n",k);
				}else{
					printf("  kgnexta:   0x%04x\n",k);
				}
			}
		}
	}

	free(buf);
	return 0;
}

int
akai_cdsetup3000_info(struct file_s *fp)
{
	char nbuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	u_char *buf;
	struct akai_cdsetup3000_s *hp;
	struct akai_cdsetup3000_entry_s *ep;
	u_int enr;
	u_int e,i;

	if ((fp==NULL)||(fp->volp==NULL)||(fp->index>=fp->volp->fimax)){
		return -1;
	}

	/* allocate buffer */
	if (fp->size<sizeof(struct akai_cdsetup3000_s)){
		fprintf(stderr,"invalid file size\n");
		return -1;
	}
	if ((buf=malloc(fp->size))==NULL){
		fprintf(stderr,"cannot allocate memory\n");
		return -1;
	}

	/* header */
	hp=(struct akai_cdsetup3000_s *)buf;
	/* marked file entries */
	ep=(struct akai_cdsetup3000_entry_s *)(buf+sizeof(struct akai_cdsetup3000_s));
	/* number of entries */
	enr=(fp->size-sizeof(struct akai_cdsetup3000_s))/sizeof(struct akai_cdsetup3000_entry_s);

	/* read file */
	if (akai_read_file(0,buf,fp,0,fp->size)<0){
		fprintf(stderr,"cannot read file\n");
		free(buf);
		return -1;
	}

	/* name */
	akai2ascii_name(hp->name,nbuf,fp->volp->type==AKAI_VOL_TYPE_S900);
	printf("ramname: \"%s\"\n\n",nbuf);

	/* CD-ROM label */
	akai2ascii_name(hp->cdlabel,nbuf,fp->volp->type==AKAI_VOL_TYPE_S900);
	printf("CD-ROM label: \"%s\"\n",nbuf);

	/* print marked file entries */
	printf("\nmarked files (part:vol/file):\n---------------------------------------------\n");
	i=0;
	for (e=0;e<enr;e++){
		if (ep[e].parti!=0xff){ /* used entry? */
			printf("%c:%03u/%03u   ",
				'A'+ep[e].parti,
				ep[e].voli+1,
				(ep[e].filei[1]<<8)+ep[e].filei[0]+1);
			if (i%4==3){
				printf("\n");
			}
			i++;
		}
	}
	if (i%4!=0){
		printf("\n");
	}
	printf("---------------------------------------------\n");

	free(buf);
	return 0;
}



/* fix RAM name of AKAI file */
int
akai_fixramname(struct file_s *fp)
{
	static char nbuf[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	static u_char buf[sizeof(struct akai_genfilehdr_s)]; /* XXX enough for all cases */
	u_char *np;
	int s900flag;

	if ((fp==NULL)||(fp->volp==NULL)||(fp->index>=fp->volp->fimax)){
		return -1;
	}

	/* file type */
	s900flag=0; /* default */
	switch (fp->type){
	case AKAI_SAMPLE900_FTYPE: /* S900 sample */
	case AKAI_PROGRAM900_FTYPE: /* S900 program */
		/* Note: name in S900 sample/program file header starts at byte 0 */
		np=buf;
		s900flag=1;
		break;
	case AKAI_SAMPLE1000_FTYPE: /* S1000 sample */
	case AKAI_SAMPLE3000_FTYPE: /* S3000 sample */
	case AKAI_CDSAMPLE3000_FTYPE: /* CD3000 CD-ROM sample parameters */
	case AKAI_PROGRAM1000_FTYPE: /* S1000 program */
	case AKAI_PROGRAM3000_FTYPE: /* S3000 program */
	case AKAI_DRUMFILE_FTYPE: /* S1000 or S3000 drum file */
	case AKAI_FXFILE_FTYPE: /* S1100 or S3000 effects file */
	case AKAI_QLFILE_FTYPE: /* S1100 or S3000 cue-list file */
	case AKAI_TLFILE_FTYPE: /* S1100 or S3000 take-list file */
	case AKAI_MULTI3000_FTYPE: /* S3000 multi file */
		{
			/* use generic file header */
			struct akai_genfilehdr_s *p;
			p=(struct akai_genfilehdr_s *)buf;
			np=p->name;
		}
		break;
	case AKAI_CDSETUP3000_FTYPE:
		{
			/* CD3000 CD-ROM setup file */
			struct akai_cdsetup3000_s *p;
			p=(struct akai_cdsetup3000_s *)buf;
			np=p->name;
		}
		break;
	default:
		/* unknown or unsupported */
		return 1; /* no error */
	}

	/* read header */
	if (akai_read_file(0,buf,fp,0,sizeof(struct akai_genfilehdr_s))<0){
		fprintf(stderr,"cannot read header\n");
		return -1;
	}

	/* copy file name to name in RAM */
	akai2ascii_name(fp->volp->file[fp->index].name,nbuf,fp->volp->type==AKAI_VOL_TYPE_S900);
	ascii2akai_name(nbuf,np,s900flag);

	/* write header */
	if (akai_write_file(0,buf,fp,0,sizeof(struct akai_genfilehdr_s))<0){
		fprintf(stderr,"cannot write header\n");
		return -1;
	}

	return 0;
};



int
akai_s900comprfile_updateuncompr(struct file_s *fp)
{
	u_int osver;

	if (fp==NULL){
		return -1;
	}

	/* check if compressed file in S900 volume */
	if ((fp->volp==NULL)||(fp->volp->type!=AKAI_VOL_TYPE_S900)||(fp->osver==0)){
		return -1;
	}

	/* check if supported file type */
	if (fp->type!=AKAI_SAMPLE900_FTYPE){
		/* no error, keep osver */
		return 0;
	}

	/* S900 sample file, compressed sample format */
	/* non-compressed sample size in bytes */
	osver=akai_sample900_getsamplesize(fp);
	/* number of un-compressed floppy blocks */
	/* Note: without sample header */
	osver=(osver+AKAI_FL_BLOCKSIZE-1)/AKAI_FL_BLOCKSIZE; /* round up */
	if (osver==0){ /* unsuitable osver? */
		osver=1; /* XXX non zero */
	}

	/* set osver of file */
	/* Note: akai_rename_file() will correct osver if necessary */
	if (akai_rename_file(fp,NULL,fp->volp,AKAI_CREATE_FILE_NOINDEX,NULL,osver)<0){
		return -1;
	}

	return 0;
}



u_int
akai_sample900_getsamplesize(struct file_s *fp)
{
	static struct akai_sample900_s s900hdr;
	static u_int samplecount;
	static u_int samplecountpart;
	static u_int samplesize;

	if (fp==NULL){
		return 0;
	}
	if (fp->type!=(u_char)AKAI_SAMPLE900_FTYPE){ /* not S900 sample? */
		return 0;
	}

	/* read header to memory */
	if (akai_read_file(0,(u_char *)&s900hdr,fp,0,sizeof(struct akai_sample900_s))<0){
		fprintf(stderr,"cannot read sample header\n");
		return 0;
	}

	/* number of samples */
	/* XXX should be an even number */
	samplecount=(s900hdr.slen[3]<<24)
		+(s900hdr.slen[2]<<16)
		+(s900hdr.slen[1]<<8)
		+s900hdr.slen[0];
	/* number of samples per part  */
	samplecountpart=(samplecount+1)/2; /* round up */
	/* size in bytes in S900 non-compressed sample format */
	samplesize=3*samplecountpart;
	return samplesize;
}



void
akai_sample900noncompr_sample2wav(u_char *sbuf,u_char *wavbuf,u_int samplecountpart)
{
	u_int i;

	if ((sbuf==NULL)||(wavbuf==NULL)){
		return;
	}
	if (samplecountpart==0){
		return;
	}

	/* convert 12bit S900 non-compressed sample format into 16bit WAV sample format */
	for (i=0;i<samplecountpart;i++){ /* first part */
		wavbuf[i*2+1]=sbuf[i*2+1];
		wavbuf[i*2+0]=0xf0&sbuf[i*2+0];
	}
	for (i=0;i<samplecountpart;i++){ /* second part */
		wavbuf[samplecountpart*2+i*2+1]=sbuf[samplecountpart*2+i];
		wavbuf[samplecountpart*2+i*2+0]=0xf0&(sbuf[i*2+0]<<4);
	}
}

void
akai_sample900noncompr_wav2sample(u_char *sbuf,u_char *wavbuf,u_int samplecountpart)
{
	u_int i;

	if ((sbuf==NULL)||(wavbuf==NULL)){
		return;
	}
	if (samplecountpart==0){
		return;
	}

	/* convert 16bit WAV sample format into 12bit S900 non-compressed sample format */
	for (i=0;i<samplecountpart;i++){ /* first part */
		sbuf[i*2+1]=wavbuf[i*2+1];
		sbuf[i*2+0]=0xf0&wavbuf[i*2+0];
	}
	for (i=0;i<samplecountpart;i++){ /* second part */
		sbuf[samplecountpart*2+i]=wavbuf[samplecountpart*2+i*2+1];
		sbuf[i*2+0]|=0x0f&(wavbuf[samplecountpart*2+i*2+0]>>4);
	}
}



u_int
akai_sample900compr_getbits(u_char *buf,u_int bitpos,u_int bitnum)
{
	u_int bytepos;
	u_char bmask;
	u_int val;
	u_int i;

	if ((buf==NULL)||(bitnum==0)){
		return 0;
	}
	/* XXX no check if bitnum too large for u_int */
	/* XXX no check if bitpos+bitnum too large for buf */

	val=0;
	for (i=0;i<bitnum;i++,bitpos++){
		bytepos=(bitpos>>3); /* /8: 8 bits per byte */
		bmask=(1<<(7-(7&bitpos))); /* Note: upper bit first */
		val<<=1;
		if ((bmask&buf[bytepos])!=0){
			val|=1;
		}
	}

	return val;
}

u_int
akai_sample900compr_sample2wav(u_char *sbuf,u_char *wavbuf,u_int sbufsiz,u_int wavbufsiz)
{
	u_char upsign;
	u_short upabsval;
	short curval;
	short curinc;
	u_int bitremain;
	u_int bitpos;
	u_int wavpos;
	u_char code;
	u_int i,j,n;

	if ((sbuf==NULL)||(wavbuf==NULL)){
		return 0;
	}
	if ((sbufsiz==0)||(wavbufsiz==0)){
		return 0;
	}

	/* convert S900 compressed sample format into 16bit WAV sample format */
	curval=0;
	curinc=0;
	bitremain=sbufsiz*8; /* 8 bits per byte */
	bitpos=0;
	wavpos=0;
	for (;(bitremain>0)&&(wavpos+1<wavbufsiz);){
		/* get code nibble */
		if (bitremain<4){
			break; /* end */
		}
		code=(u_char)akai_sample900compr_getbits(sbuf,bitpos,4);
#ifdef SAMPLE900COMPR_DEBUG
		printf("%08x: code=%x\n",bitpos,code);
#endif
		bitpos+=4;
		bitremain-=4;
		/* parse code */
		if (code<SAMPLE900COMPR_BITNUM_OFF-SAMPLE900COMPR_BITNUM_MAX){
			/* number of samples */
			if (code==0x0){
				j=SAMPLE900COMPR_GROUP_SAMPNUM;
			}else{
				j=code;
			}
			/* generate signal */
			for (i=0;(i<j)&&(wavpos+1<wavbufsiz);i++){
				/* update curval */
				curval+=curinc;
				/* convert 12bit sample into 16bit WAV sample */
				wavbuf[wavpos++]=0xf0&(u_char)(curval<<4);
				wavbuf[wavpos++]=0xff&(u_char)(curval>>4);
			}
		}else{
			/* number of bits per increment update word */
			n=SAMPLE900COMPR_BITNUM_OFF-code;
			/* number of required remaining bits for instruction code */
			j=SAMPLE900COMPR_GROUP_SAMPNUM*(1+n); /* Note: 1 sign flag bit and n bits per word */
			if (bitremain<j){
				break; /* end */
			}
			/* generate signal */
			for (i=0;(i<SAMPLE900COMPR_GROUP_SAMPNUM)&&(wavpos+1<wavbufsiz);i++){
				/* get sign flag bit */
				upsign=(u_char)akai_sample900compr_getbits(sbuf,bitpos+0+i,1);
				/* get value word */
				upabsval=(u_short)akai_sample900compr_getbits(sbuf,bitpos+SAMPLE900COMPR_GROUP_SAMPNUM+i*n,n);
#ifdef SAMPLE900COMPR_DEBUG
				printf("          %c%u\n",(upsign==0)?'+':'-',upabsval);
#endif
				/* update curinc */
				if (upsign==0){
					/* plus */
					curinc+=(short)upabsval;
				}else{
					/* minus */
					curinc-=(short)upabsval;
				}
				/* update curval */
				curval+=curinc;
				/* Note: SAMPLE900COMPR_BITMASK&curval contains sample value */
				/* convert 12bit sample into 16bit WAV sample */
				wavbuf[wavpos++]=0xf0&(u_char)(curval<<4);
				wavbuf[wavpos++]=0xff&(u_char)(curval>>4);
			}
			bitpos+=j;
			bitremain-=j;
		}
	}

	return wavpos;
}

void
akai_sample900compr_setbits(u_char *buf,u_int bitpos,u_int bitnum,u_int val)
{
	u_int bytepos;
	u_char bmask;
	u_int i;

	if ((buf==NULL)||(bitnum==0)){
		return;
	}
	/* XXX no check if bitnum too large for u_int */
	/* XXX no check if bitpos+bitnum too large for buf */

	for (i=0;i<bitnum;i++,bitpos++){
		bytepos=(bitpos>>3); /* /8: 8 bits per byte */
		bmask=(1<<(7-(7&bitpos))); /* Note: upper bit first */
		if (((1<<(bitnum-1-i))&val)!=0){ /* bit set? (Note: upper bit first) */
			/* set bit */
			buf[bytepos]|=bmask;
		}else{
			/* clear bit */
			buf[bytepos]&=~bmask;
		}
	}
}

int
akai_sample900compr_wav2sample(u_char *sbuf,u_char *wavbuf,u_int samplecountpart)
{
	/* must be static for multiple calls, must be initialized */
	static u_char *upbitnumbuf=NULL;
	static u_int *upsignbuf=NULL;
	static u_short *upabsvalbuf=NULL;
	static u_int groupcount=0;

	short sval;
	short curval;
	short curinc;
	short upval;
	u_short upabsval;
	u_char upbitnum;
	u_int upsign;
	u_int bitpos;
	u_char code;
	u_int g,s,i,j;
	int ret;

	if ((sbuf==NULL)&&(wavbuf==NULL)){
		ret=0; /* no error */
		/* free buffers if still allocated */
		goto akai_sample900compr_wav2sample_freebufexit;
	}

	ret=-1; /* no success so far */

	/* convert 16bit WAV sample format into S900 compressed sample format */

	if (sbuf==NULL){
		/* first pass */

		/* free buffers if still allocated */
		if (upbitnumbuf!=NULL){
			free(upbitnumbuf);
			upbitnumbuf=NULL;
		}
		if (upsignbuf!=NULL){
			free(upsignbuf);
			upsignbuf=NULL;
		}
		if (upabsvalbuf!=NULL){
			free(upabsvalbuf);
			upabsvalbuf=NULL;
		}

		/* number of groups */
		/* Note: 2*samplecountpart+1 to encode at least one additional zero sample behind end */
		groupcount=(2*samplecountpart+1+SAMPLE900COMPR_GROUP_SAMPNUM-1)/SAMPLE900COMPR_GROUP_SAMPNUM; /* round up */

		/* allocate buffers */
		upbitnumbuf=(u_char *)malloc(groupcount*sizeof(u_char));
		if (upbitnumbuf==NULL){
			perror("cannot allocate upbitnumbuf");
			goto akai_sample900compr_wav2sample_freebufexit;
		}
		upsignbuf=(u_int *)malloc(groupcount*sizeof(u_int));
		if (upsignbuf==NULL){
			perror("cannot allocate upsignbuf");
			goto akai_sample900compr_wav2sample_freebufexit;
		}
		upabsvalbuf=(u_short *)malloc(groupcount*SAMPLE900COMPR_GROUP_SAMPNUM*sizeof(u_short));
		if (upabsvalbuf==NULL){
			perror("cannot allocate upabsvalbuf");
			goto akai_sample900compr_wav2sample_freebufexit;
		}

		/* gather groups */
		curval=0;
		curinc=0;
		sval=0;
		for (g=0,s=0;g<groupcount;g++){
			upbitnum=0;
			upsign=0;
			/* samples within group */
			for (i=0;i<SAMPLE900COMPR_GROUP_SAMPNUM;i++,s++){
				if (s<2*samplecountpart){
					/* get 12bit sample from 16bit WAV sample */
					sval=(((short)(char)wavbuf[s*2+1])<<4)+(((short)(u_char)wavbuf[s*2+0])>>4);
				}else{
					/* encode zero sample behind end */
					sval=0;
				}

				/* determine required upsign,upabsval */
				upsign<<=1;
				upval=sval-(curval+curinc);
				/* choose optimum interval position for min. abs. value */
				while (upval>SAMPLE900COMPR_INTERVALSIZ2){
					upval-=SAMPLE900COMPR_INTERVALSIZ;
				}
				while (upval<-SAMPLE900COMPR_INTERVALSIZ2){
					upval+=SAMPLE900COMPR_INTERVALSIZ;
				}
				/* Note: max./min. possible value for resulting upval is +/-SAMPLE900COMPR_INTERVALSIZ2 */
				/*       which requires SAMPLE900COMPR_BITNUM_MAX bits for upabsval */
				if (upval>=0){
					/* plus */
					/* upsign|=0; */
					upabsval=SAMPLE900COMPR_BITMASK&(u_short)upval;
					/* update curinc */
					curinc+=(short)upabsval;
				}else{
					/* minus */
					upsign|=1;
					upabsval=SAMPLE900COMPR_BITMASK&(u_short)(-upval);
					/* update curinc */
					curinc-=(short)upabsval;
				}
				upabsvalbuf[s]=upabsval;

				/* determine max. number of required bits for upabsval in group */
				/* start with previous upbitnum */
				for (j=upbitnum;j<SAMPLE900COMPR_BITNUM_MAX;j++){
					if (((1<<j)&upabsval)!=0){ /* bit j set? */
						upbitnum=j+1;
					}
				}

				/* update curval */
				curval+=curinc;
				/* Note: SAMPLE900COMPR_BITMASK&curval contains sample value */
				if ((SAMPLE900COMPR_BITMASK&(curval-sval))!=0){
					/* XXX should not happen */
					fprintf(stderr,"%06x,%02x: error: sval=%i curval=%i\n",g,i,sval,curval);
					goto akai_sample900compr_wav2sample_freebufexit;
				}
			}
#ifdef SAMPLE900COMPR_DEBUG
			printf("%06x: upbitnum=%2u upsign=%04x\n",g,upbitnum,upsign);
#endif
			upbitnumbuf[g]=upbitnum;
			upsignbuf[g]=upsign;
		}

		/* determine number of bits required in sbuf for second pass */
		bitpos=0;
		for (g=0;g<groupcount;g++){
			/* see "encode groups" below */
			bitpos+=4;
			if (upbitnumbuf[g]!=0){
				bitpos+=SAMPLE900COMPR_GROUP_SAMPNUM*(1+upbitnumbuf[g]);
			}
		}
		bitpos+=(7&(8-(7&bitpos))); /* number of bits missing to full byte */

		ret=(int)(bitpos>>3); /* success, return number of required bytes in sbuf for second pass */
		/* keep buffers for second pass */
		goto akai_sample900compr_wav2sample_keepbufexit;
	}else{
		/* second pass */

		if (wavbuf==NULL){
			goto akai_sample900compr_wav2sample_freebufexit;
		}

		/* check allocation of buffers */
		if ((upbitnumbuf==NULL)||(upsignbuf==NULL)||(upabsvalbuf==NULL)){
			fprintf(stderr,"error: buffers not allocated in second pass\n");
			goto akai_sample900compr_wav2sample_freebufexit;
		}

		/* XXX ignore samplecountpart in second pass */

		/* encode groups */
		bitpos=0;
		for (g=0,s=0;g<groupcount;g++){
			/* determine code nibble */
			upbitnum=upbitnumbuf[g];
			if (upbitnum==0){
				code=0x0;
			}else{
				code=SAMPLE900COMPR_BITNUM_OFF-upbitnum;
			}
#ifdef SAMPLE900COMPR_DEBUG
			printf("%08x: code=%x\n",bitpos,code);
#endif
			/* save code nibble */
			akai_sample900compr_setbits(sbuf,bitpos,4,(u_int)code);
			bitpos+=4;

			if (upbitnum==0){
				s+=SAMPLE900COMPR_GROUP_SAMPNUM; /* advance in upabsvalbuf[] */
				/* no further bits to save for this group */
			}else{
				/* save upsign */
				upsign=upsignbuf[g];
				akai_sample900compr_setbits(sbuf,bitpos,SAMPLE900COMPR_GROUP_SAMPNUM,upsign);
				bitpos+=SAMPLE900COMPR_GROUP_SAMPNUM;
				/* samples within group */
				for (i=0;i<SAMPLE900COMPR_GROUP_SAMPNUM;i++,s++){
					/* save upabsval */
					upabsval=upabsvalbuf[s];
					akai_sample900compr_setbits(sbuf,bitpos,(u_int)upbitnum,(u_int)upabsval);
					bitpos+=(u_int)upbitnum;
#ifdef SAMPLE900COMPR_DEBUG
					printf("          %c%u\n",
						(((1<<(SAMPLE900COMPR_GROUP_SAMPNUM-1-i))&upsign)==0)?'+':'-',upabsval); /* Note: upper bit first */
#endif
				}
			}
		}
		j=(7&(8-(7&bitpos))); /* number of bits missing to full byte */
		if (j!=0){
			/* zero padding to full byte */
			akai_sample900compr_setbits(sbuf,bitpos,j,0);
			bitpos+=j;
		}

		ret=(int)(bitpos>>3); /* success, return number of used bytes in sbuf */
		/* free buffers */
		goto akai_sample900compr_wav2sample_freebufexit;
	}

akai_sample900compr_wav2sample_freebufexit:
	if (upbitnumbuf!=NULL){
		free(upbitnumbuf);
		upbitnumbuf=NULL;
	}
	if (upsignbuf!=NULL){
		free(upsignbuf);
		upsignbuf=NULL;
	}
	if (upabsvalbuf!=NULL){
		free(upabsvalbuf);
		upabsvalbuf=NULL;
	}

akai_sample900compr_wav2sample_keepbufexit:
	return ret;
}



int
akai_sample900_compr2noncompr(struct file_s *fp,struct vol_s *volp)
{
	struct file_s tmpfile;
	struct akai_sample900_s s900hdr;
	u_int samplecount;
	u_int samplecountpart;
	u_int samplesizecompr;
	u_char *sbufcompr;
	u_int samplesizenoncompr;
	u_char *sbufnoncompr;
	u_int wavsamplesize;
	u_char *wavbuf;
	static char fname[AKAI_NAME_LEN_S900+3+1]; /* name (ASCII), +3 for ".S9", +1 for '\0' */
	int replaceflag;
	u_int findex;
	u_int i;
	int ret;

	if (fp==NULL){
		return -1;
	}

	if (volp==NULL){
		/* destination volume same as source volume */
		volp=fp->volp;
		replaceflag=1; /* replace file */
	}else{
		replaceflag=0; /* keep source file */
	}
	if ((volp==NULL)||(volp->partp==NULL)){
		return -1;
	}

	/* file type */
	if ((fp->type!=(u_char)AKAI_SAMPLE900_FTYPE)||(fp->osver==0)){ /* not S900 compressed sample file? */
		fprintf(stderr,"not an S900 compressed sample file\n");
		return -1;
	}
	if (fp->size<sizeof(struct akai_sample900_s)){
		fprintf(stderr,"invalid sample size\n");
		return -1;
	}

	sbufcompr=NULL; /* no sample so far */
	sbufnoncompr=NULL; /* no sample so far */
	wavbuf=NULL; /* no sample so far */
	ret=-1; /* no success so far */

	/* read header to memory */
	if (akai_read_file(0,(u_char *)&s900hdr,fp,0,sizeof(struct akai_sample900_s))<0){
		fprintf(stderr,"cannot read sample\n");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* number of samples */
	/* XXX should be an even number */
	samplecount=(s900hdr.slen[3]<<24)
		+(s900hdr.slen[2]<<16)
		+(s900hdr.slen[1]<<8)
		+s900hdr.slen[0];
	/* number of samples per part  */
	samplecountpart=(samplecount+1)/2; /* round up */
	/* size in bytes in S900 non-compressed sample format */
	samplesizenoncompr=3*samplecountpart;
	/* WAV size in bytes */
	wavsamplesize=2*samplecountpart*2; /* *2 for 16bit per WAV sample word */
	/* S900 compressed sample size */
	samplesizecompr=fp->size-sizeof(struct akai_sample900_s);

	/* create non-compressed sample file name */
	/* Note: use RAM name as basis (-> akai_fixramname() not needed afterwards) */
	akai2ascii_name((u_char *)s900hdr.name,fname,1); /* 1: S900 */
	strcat(fname,".S9");

	/* check if destination file already exists */
	if (akai_find_file(volp,&tmpfile,fname)==0){
		/* exists */
		fprintf(stderr,"destination file name \"%s\" already used\n",fname);
		goto akai_sample900_compr2noncompr_exit;
	}

	/* check if enough free blocks */
	/* required size of non-compressed file in blocks */
	i=(sizeof(struct akai_sample900_s)+samplesizenoncompr+volp->partp->blksize-1)/volp->partp->blksize;
	if (replaceflag){
		if (samplesizecompr<samplesizenoncompr){
			/* subtract size of existing compressed file in blocks */
			i-=(sizeof(struct akai_sample900_s)+samplesizecompr+volp->partp->blksize-1)/volp->partp->blksize;
		}else{
			i=0;
		}
	}
	if (volp->partp->bfree<i){
		/* not enough space left */
		fprintf(stderr,"not enough space left for destination file \"%s\"\n",fname);
		goto akai_sample900_compr2noncompr_exit;
	}

	/* allocate compressed sample buffer */
	sbufcompr=(u_char *)malloc(samplesizecompr);
	if (sbufcompr==NULL){
		perror("malloc");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* allocate non-compressed sample buffer */
	sbufnoncompr=(u_char *)malloc(samplesizenoncompr);
	if (sbufnoncompr==NULL){
		perror("malloc");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* allocate WAV sample buffer */
	wavbuf=(u_char *)malloc(wavsamplesize);
	if (wavbuf==NULL){
		perror("malloc");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* read compressed sample to memory */
	if (akai_read_file(0,sbufcompr,fp,sizeof(struct akai_sample900_s),sizeof(struct akai_sample900_s)+samplesizecompr)<0){
		fprintf(stderr,"cannot read sample\n");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* convert S900 compressed sample format into 16bit WAV sample format */
	i=akai_sample900compr_sample2wav(sbufcompr,wavbuf,samplesizecompr,wavsamplesize);
	if (i<wavsamplesize){
		fprintf(stderr,"warning: incomplete sample data\n");
		/* zero padding */
		bzero(wavbuf+i,wavsamplesize-i);
	}

	/* convert 16bit WAV sample format into S900 non-compressed sample format */
	akai_sample900noncompr_wav2sample(sbufnoncompr,wavbuf,samplecountpart);

	if (replaceflag){
		/* keep file index */
		findex=fp->index;
		/* delete source file */
		if (akai_delete_file(fp)<0){
			fprintf(stderr,"cannot overwrite existing file\n");
			goto akai_sample900_compr2noncompr_exit;
		}
	}else{
		findex=AKAI_CREATE_FILE_NOINDEX;
	}
	/* create file */
	/* Note: akai_create_file() will correct osver if necessary */
	if (akai_create_file(volp,&tmpfile,
						 sizeof(struct akai_sample900_s)+samplesizenoncompr,
						 findex,
						 fname,
						 0, /* non-compressed */
						 NULL)<0){
		fprintf(stderr,"cannot create file\n");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* write sample header */
	if (akai_write_file(0,(u_char *)&s900hdr,&tmpfile,0,sizeof(struct akai_sample900_s))<0){
		fprintf(stderr,"cannot write sample header\n");
		goto akai_sample900_compr2noncompr_exit;
	}

	/* write non-compressed sample */
	if (akai_write_file(0,sbufnoncompr,&tmpfile,sizeof(struct akai_sample900_s),sizeof(struct akai_sample900_s)+samplesizenoncompr)<0){
		fprintf(stderr,"cannot write sample\n");
		goto akai_sample900_compr2noncompr_exit;
	}

	ret=0; /* success */

akai_sample900_compr2noncompr_exit:
	if (sbufcompr!=NULL){
		free(sbufcompr);
	}
	if (sbufnoncompr!=NULL){
		free(sbufnoncompr);
	}
	if (wavbuf!=NULL){
		free(wavbuf);
	}
	return ret;
}

int
akai_sample900_noncompr2compr(struct file_s *fp,struct vol_s *volp)
{
	struct file_s tmpfile;
	struct akai_sample900_s s900hdr;
	u_int samplecount;
	u_int samplecountpart;
	u_int samplesizecompr;
	u_char *sbufcompr;
	u_int samplesizenoncompr;
	u_char *sbufnoncompr;
	u_int wavsamplesize;
	u_char *wavbuf;
	static char fname[AKAI_NAME_LEN_S900+4+1]; /* name (ASCII), +4 for ".S9C", +1 for '\0' */
	int replaceflag;
	u_int findex;
	u_int osver;
	u_int i;
	int r;
	int ret;

	if (fp==NULL){
		return -1;
	}

	if (volp==NULL){
		/* destination volume same as source volume */
		volp=fp->volp;
		replaceflag=1; /* replace file */
	}else{
		replaceflag=0; /* keep source file */
	}
	if ((volp==NULL)||(volp->partp==NULL)){
		return -1;
	}

	/* file type */
	if ((fp->type!=(u_char)AKAI_SAMPLE900_FTYPE)||(fp->osver!=0)){ /* not S900 non-compressed sample file? */
		fprintf(stderr,"not an S900 non-compressed sample file\n");
		return -1;
	}
	if (fp->size<sizeof(struct akai_sample900_s)){
		fprintf(stderr,"invalid sample size\n");
		return -1;
	}

	sbufcompr=NULL; /* no sample so far */
	sbufnoncompr=NULL; /* no sample so far */
	wavbuf=NULL; /* no sample so far */
	ret=-1; /* no success so far */

	/* read header to memory */
	if (akai_read_file(0,(u_char *)&s900hdr,fp,0,sizeof(struct akai_sample900_s))<0){
		fprintf(stderr,"cannot read sample\n");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* number of samples */
	/* XXX should be an even number */
	samplecount=(s900hdr.slen[3]<<24)
		+(s900hdr.slen[2]<<16)
		+(s900hdr.slen[1]<<8)
		+s900hdr.slen[0];
	/* number of samples per part  */
	samplecountpart=(samplecount+1)/2; /* round up */
	/* size in bytes in S900 non-compressed sample format */
	samplesizenoncompr=3*samplecountpart;
	/* WAV size in bytes */
	wavsamplesize=2*samplecountpart*2; /* *2 for 16bit per WAV sample word */

	/* allocate non-compressed sample buffer */
	sbufnoncompr=(u_char *)malloc(samplesizenoncompr);
	if (sbufnoncompr==NULL){
		perror("malloc");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* allocate WAV sample buffer */
	wavbuf=(u_char *)malloc(wavsamplesize);
	if (wavbuf==NULL){
		perror("malloc");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* read non-compressed sample to memory */
	if (akai_read_file(0,sbufnoncompr,fp,sizeof(struct akai_sample900_s),sizeof(struct akai_sample900_s)+samplesizenoncompr)<0){
		fprintf(stderr,"cannot read sample\n");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* convert S900 non-compressed sample format into 16bit WAV sample format */
	akai_sample900noncompr_sample2wav(sbufnoncompr,wavbuf,samplecountpart);

	/* first pass to convert 16bit WAV sample format into S900 compressed sample format */
	/* Note: first pass of akai_sample900compr_wav2sample() requires WAV sample to be loaded to wavbuf */
	r=akai_sample900compr_wav2sample(NULL,wavbuf,samplecountpart); /* NULL: first pass */
	if (r<0){
		goto akai_sample900_noncompr2compr_exit;
	}
	/* S900 compressed sample size in bytes */
	samplesizecompr=(u_int)r;

	/* create compressed sample file name */
	/* Note: use RAM name as basis (-> akai_fixramname() not needed afterwards) */
	akai2ascii_name((u_char *)s900hdr.name,fname,1); /* 1: S900 */
	strcat(fname,".S9C");

	/* check if destination file already exists */
	if (akai_find_file(volp,&tmpfile,fname)==0){
		/* exists */
		fprintf(stderr,"destination file name \"%s\" already used\n",fname);
		goto akai_sample900_noncompr2compr_exit;
	}

	/* check if enough free blocks */
	/* required size of compressed file in blocks */
	i=(sizeof(struct akai_sample900_s)+samplesizecompr+volp->partp->blksize-1)/volp->partp->blksize;
	if (replaceflag){
		if (samplesizenoncompr<samplesizecompr){
			/* subtract size of existing non-compressed file in blocks */
			i-=(sizeof(struct akai_sample900_s)+samplesizenoncompr+volp->partp->blksize-1)/volp->partp->blksize;
		}else{
			i=0;
		}
	}
	if (volp->partp->bfree<i){
		/* not enough space left */
		fprintf(stderr,"not enough space left for destination file \"%s\"\n",fname);
		goto akai_sample900_noncompr2compr_exit;
	}

	/* allocate compressed sample buffer */
	sbufcompr=(u_char *)malloc(samplesizecompr);
	if (sbufcompr==NULL){
		perror("malloc");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* second pass to convert 16bit WAV sample format into S900 compressed sample format */
	if (akai_sample900compr_wav2sample(sbufcompr,wavbuf,samplecountpart)<0){ /* sbufcompr!=NULL: second pass */
		goto akai_sample900_noncompr2compr_exit;
	}

	if (replaceflag){
		/* keep file index */
		findex=fp->index;
		/* delete source file */
		if (akai_delete_file(fp)<0){
			fprintf(stderr,"cannot overwrite existing file\n");
			goto akai_sample900_noncompr2compr_exit;
		}
	}else{
		findex=AKAI_CREATE_FILE_NOINDEX;
	}
	/* osver for S900 compressed sample file: number of un-compressed floppy blocks */
	/* Note: without sample header */
	osver=(samplesizenoncompr+AKAI_FL_BLOCKSIZE-1)/AKAI_FL_BLOCKSIZE; /* round up */
	if (osver==0){ /* unsuitable osver? */
		osver=1; /* XXX non zero */
	}
	/* create file */
	/* Note: akai_create_file() will correct osver if necessary */
	if (akai_create_file(volp,&tmpfile,
						 sizeof(struct akai_sample900_s)+samplesizecompr,
						 findex,
						 fname,
						 osver,
						 NULL)<0){
		fprintf(stderr,"cannot create file\n");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* write sample header */
	if (akai_write_file(0,(u_char *)&s900hdr,&tmpfile,0,sizeof(struct akai_sample900_s))<0){
		fprintf(stderr,"cannot write sample header\n");
		goto akai_sample900_noncompr2compr_exit;
	}

	/* write compressed sample */
	if (akai_write_file(0,sbufcompr,&tmpfile,sizeof(struct akai_sample900_s),sizeof(struct akai_sample900_s)+samplesizecompr)<0){
		fprintf(stderr,"cannot write sample\n");
		goto akai_sample900_noncompr2compr_exit;
	}

	ret=0; /* success */

akai_sample900_noncompr2compr_exit:
	if (sbufcompr!=NULL){
		free(sbufcompr);
	}
	if (sbufnoncompr!=NULL){
		free(sbufnoncompr);
	}
	if (wavbuf!=NULL){
		free(wavbuf);
	}
	akai_sample900compr_wav2sample(NULL,NULL,0); /* NULL,NULL: free buffers if still allocated */
	return ret;
}



int
akai_sample2wav(struct file_s *fp,int wavfd,u_int *sizep,char **wavnamep,int what)
{
	/* Note: static for multiple calls with different what */
	static struct akai_sample3000_s s3000hdr;
	static struct akai_sample900_s *s900hdrp;
	static u_int hdrsize;
	static u_int samplecount;
	static u_int samplecountpart;
	static u_int samplesize;
	static u_int samplerate;
	static u_char *sbuf;
	static u_int wavsamplesize;
	static u_char *wavbuf;
	static int ret;
	static char wavname[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */
	static int nlen;
	static u_int i;

	if (fp==NULL){
		return -1;
	}

	if (what&SAMPLE2WAV_CHECK){

		/* file type */
		if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){
			/* S900 sample */
			hdrsize=sizeof(struct akai_sample900_s);
		}else if (fp->type==(u_char)AKAI_SAMPLE1000_FTYPE){
			/* S1000 sample */
			hdrsize=sizeof(struct akai_sample1000_s);
		}else if (fp->type==(u_char)AKAI_SAMPLE3000_FTYPE){
			/* S3000 sample */
			hdrsize=sizeof(struct akai_sample3000_s);
		}else{
			/* unknown or unsupported */
			return 1; /* no error */
		}

		sbuf=NULL; /* no sample so far */
		wavbuf=NULL; /* no sample so far */
		ret=-1; /* no success so far */

		/* read header to memory */
		/* Note: use S3000 header as buffer for S900 header */
		if (akai_read_file(0,(u_char *)&s3000hdr,fp,0,hdrsize)<0){
			fprintf(stderr,"cannot read sample\n");
			goto akai_sample2wav_exit;
		}

		/* parse header */
		if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){
			/* S900 sample */
			s900hdrp=(struct akai_sample900_s *)&s3000hdr;
			/* number of samples */
			/* XXX should be an even number */
			samplecount=(s900hdrp->slen[3]<<24)
				+(s900hdrp->slen[2]<<16)
				+(s900hdrp->slen[1]<<8)
				+s900hdrp->slen[0];
			/* number of samples per part  */
			samplecountpart=(samplecount+1)/2; /* round up */
			samplecount=2*samplecountpart; /* XXX correct samplecount */
			if (fp->osver==0){
				/* S900 non-compressed sample format */
				/* size in bytes */
				samplesize=3*samplecountpart;
			}else{
				/* S900 compressed sample format */
				/* size in bytes */
				if (fp->size<hdrsize){
					fprintf(stderr,"invalid sample size\n");
					goto akai_sample2wav_exit;
				}
				samplesize=fp->size-hdrsize;
			}
			samplerate=(s900hdrp->srate[1]<<8)
				+s900hdrp->srate[0];
		}else{
			/* S1000/S3000 sample */
			/* Note: S1000 header is contained within S3000 header */
			/* number of samples */
			samplecount=(s3000hdr.s1000.slen[3]<<24)
				+(s3000hdr.s1000.slen[2]<<16)
				+(s3000hdr.s1000.slen[1]<<8)
				+s3000hdr.s1000.slen[0];
			/* size in bytes */
			samplesize=samplecount*2; /* *2 for 16bit per sample word */
			samplecountpart=0;
			samplerate=(s3000hdr.s1000.srate[1]<<8)
				+s3000hdr.s1000.srate[0];
		}
		/* size in bytes */
		wavsamplesize=samplecount*2; /* *2 for 16bit per WAV sample word */

#ifdef DEBUG
		printf("type:        %15i\n",fp->type);
		printf("samplecount: %15u\n",samplecount);
		printf("samplesize:  %15u bytes\n",samplesize);
		printf("samplerate:  %15u Hz\n",samplerate);
#endif

		/* check size */
		if (hdrsize+samplesize>fp->size){
			fprintf(stderr,"invalid sample size\n");
			goto akai_sample2wav_exit;
		}

		if (sizep!=NULL){
			/* WAV file size */
			*sizep=WAV_HEAD_SIZE+wavsamplesize;
#ifndef WAV_AKAIHEAD_DISABLE
			*sizep+=sizeof(struct wav_chunkhead_s)+hdrsize; /* sample header chunk (see below) */
#endif
		}

		/* create WAV name */
		if ((fp->volp!=NULL)&&(fp->index<fp->volp->fimax)){
			akai2ascii_name(fp->volp->file[fp->index].name,wavname,fp->volp->type==AKAI_VOL_TYPE_S900);
			nlen=(int)strlen(wavname);
		}else{
			nlen=0;
		}
		bcopy(".wav",wavname+nlen,5);

		if (wavnamep!=NULL){
			/* pointer to name (wavname must be static!) */
			*wavnamep=wavname;
		}
	}

	if (what&SAMPLE2WAV_EXPORT){

		/* allocate WAV sample buffer */
		wavbuf=(u_char *)malloc(wavsamplesize);
		if (wavbuf==NULL){
			perror("malloc");
			goto akai_sample2wav_exit;
		}

		if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
			/* allocate sample buffer */
			sbuf=(u_char *)malloc(samplesize);
			if (sbuf==NULL){
				perror("malloc");
				goto akai_sample2wav_exit;
			}
		}else{
			/* Note: no sample format conversion necessary for S1000/S3000 */
			sbuf=wavbuf;
		}

		/* read sample to memory */
		if (akai_read_file(0,sbuf,fp,hdrsize,hdrsize+samplesize)<0){
			fprintf(stderr,"cannot read sample\n");
			goto akai_sample2wav_exit;
		}

		if (what&SAMPLE2WAV_CREATE){
			/* create WAV file */
			if ((wavfd=OPEN(wavname,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
				perror("create WAV");
				goto akai_sample2wav_exit;
			}
		}

		if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
			if (fp->osver==0){
				/* S900 non-compressed sample format */
				/* convert S900 non-compressed sample format into 16bit WAV sample format */
				akai_sample900noncompr_sample2wav(sbuf,wavbuf,samplecountpart);
			}else{
				/* S900 compressed sample format */
				/* convert S900 compressed sample format into 16bit WAV sample format */
				i=akai_sample900compr_sample2wav(sbuf,wavbuf,samplesize,wavsamplesize);
				if (i<wavsamplesize){
					fprintf(stderr,"warning: incomplete sample data\n");
					/* zero padding */
					bzero(wavbuf+i,wavsamplesize-i);
				}
			}
		}
		/* Note: no sample format conversion necessary for S1000/S3000 */

		/* write WAV header */
		if (wav_write_head(wavfd,
						   wavsamplesize,1,samplerate,16, /* 1: mono, 16: 16bit */
#ifndef WAV_AKAIHEAD_DISABLE
						   sizeof(struct wav_chunkhead_s)+hdrsize /* sample header chunk (see below) */
#else
						   0
#endif
						   )<0){
			fprintf(stderr,"cannot write WAV header\n");
			goto akai_sample2wav_exit;
		}

		/* write WAV sample to WAV file */
		if (WRITE(wavfd,wavbuf,wavsamplesize)!=(int)wavsamplesize){
			perror("write WAV samples");
			goto akai_sample2wav_exit;
		}

#ifndef WAV_AKAIHEAD_DISABLE
		{
			struct wav_chunkhead_s wavchunkhead;

			/* create sample header chunk */
			if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){
				/* S900 sample */
				bcopy(WAV_CHUNKHEAD_AKAIS900SAMPLEHEADSTR,wavchunkhead.typestr,4);
			}else if (fp->type==(u_char)AKAI_SAMPLE1000_FTYPE){
				/* S1000 sample */
				bcopy(WAV_CHUNKHEAD_AKAIS1000SAMPLEHEADSTR,wavchunkhead.typestr,4);
			}else{
				/* S3000 sample */
				bcopy(WAV_CHUNKHEAD_AKAIS3000SAMPLEHEADSTR,wavchunkhead.typestr,4);
			}
			wavchunkhead.csize[0]=0xff&hdrsize;
			wavchunkhead.csize[1]=0xff&(hdrsize>>8);
			wavchunkhead.csize[2]=0xff&(hdrsize>>16);
			wavchunkhead.csize[3]=0xff&(hdrsize>>24);
			/* write WAV chunk header */
			if (WRITE(wavfd,(u_char *)&wavchunkhead,sizeof(struct wav_chunkhead_s))!=(int)sizeof(struct wav_chunkhead_s)){
				perror("write WAV chunk");
				goto akai_sample2wav_exit;
			}
			/* write sample header */
			/* Note: use S3000 header as buffer for S900 header */
			/* Note: S1000 header is contained within S3000 header */
			if (WRITE(wavfd,(u_char *)&s3000hdr,hdrsize)!=(int)hdrsize){
				perror("write WAV chunk");
				goto akai_sample2wav_exit;
			}
		}
#endif
#if 1
		printf("sample exported to WAV\n");
#endif
	}

	ret=0; /* success */

akai_sample2wav_exit:
	if (wavbuf!=NULL){
		free(wavbuf);
	}
	if (fp->type==(u_char)AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
		if (sbuf!=NULL){
			free(sbuf);
		}
	}
	if (what&SAMPLE2WAV_CREATE){
		if (wavfd>=0){
			CLOSE(wavfd);
		}
	}
	return ret;
}



int
akai_wav2sample(int wavfd,char *wavname,struct vol_s *volp,u_int findex,
				u_int type,int s9cflag,u_int osver,u_char *tagp,
				u_int *bcountp,int what)
{
	/* Note: static for multiple calls with different what */
	struct file_s tmpfile;
	struct akai_sample900_s s900hdr;
	struct akai_sample3000_s s3000hdr;
	static u_int hdrsize;
	static u_int chnr;
	static u_int samplerate;
	static u_int bitnr;
	static u_int samplecount;
	static u_int samplecountpart;
	static u_int samplesize;
	static u_char *sbuf;
	static u_int wavsamplesize;
	static u_int wavsamplesizealloc;
	static u_int wavsamplecount;
	static u_char *wavbuf, *tempbuf;
	static char *errstrp;
	static int nlen;
	static char *tname;
	static char fname[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */
	static char sname[AKAI_NAME_LEN+1]; /* +1 for '\0' */
	static int r;
	static u_int bcount;
#ifndef WAV_AKAIHEAD_DISABLE
	static u_int extrasize;
	static int wavakaiheadfound;
#endif
	static int ret;
	static u_int ch, ltype = -1, lstart, lend;

	if (wavname==NULL){
		return -1;
	}
	if (volp==NULL){
		return -1;
	}

	/* sample file type */
	if (type==AKAI_FTYPE_FREE){ /* invalid file type? */
		/* derive sample file type from volume type/file osver */
		if (volp->type==AKAI_VOL_TYPE_S900){
			/* S900 sample */
			type=AKAI_SAMPLE900_FTYPE;
			/* Note: ignore given osver, osver will be overwritten below */
		}else{
#if 1
			if (osver==AKAI_OSVER_S900VOL){
				/* S900 sample */
				type=AKAI_SAMPLE900_FTYPE;
			}else if (osver<=AKAI_OSVER_S1100MAX){
				/* S1000 sample */
				type=AKAI_SAMPLE1000_FTYPE;
			}else{
				/* S3000 sample */
				type=AKAI_SAMPLE3000_FTYPE;
			}
#else
			if (volp->type==AKAI_VOL_TYPE_S1000){
				/* S1000 sample */
				type=AKAI_SAMPLE1000_FTYPE;
			}else if ((volp->type==AKAI_VOL_TYPE_S3000)||(volp->type==AKAI_VOL_TYPE_CD3000)){
				/* S3000 sample */
				type=AKAI_SAMPLE3000_FTYPE;
			}else{
				return -1;
			}
#endif
		}
	}
	if (type==AKAI_SAMPLE900_FTYPE){
		/* S900 sample */
		hdrsize=sizeof(struct akai_sample900_s);
		if (s9cflag){
			/* S900 compressed sample format */
			tname=".S9C";
		}else{
			/* S900 non-compressed sample format */
			tname=".S9";
		}
	}else if (type==AKAI_SAMPLE1000_FTYPE){
		/* S1000 sample */
		hdrsize=sizeof(struct akai_sample1000_s);
		tname=".S1";
	}else if (type==AKAI_SAMPLE3000_FTYPE){
		/* S3000 sample */
		hdrsize=sizeof(struct akai_sample3000_s);
		tname=".S3";
	}else{
		return -1;
	}

	sbuf=NULL; /* no sample so far */
	wavbuf=NULL; /* no sample so far */
	ret=-1; /* no success so far */
	bcount=0; /* no bytes read yet */

	/* check WAV name */
	nlen=(int)strlen(wavname);
	if ((nlen<4)||(strncasecmp(wavname+nlen-4,".wav",4)!=0)){
		/* unknown or unsupported */
		ret=1; /* no error */
		goto akai_wav2sample_exit;
	}
	nlen-=4; /* remove suffix */

	if (what&WAV2SAMPLE_OPEN){
		/* open external WAV file */
		if ((wavfd=OPEN(wavname,O_RDONLY|O_BINARY,0))<0){
			perror("open WAV");
			goto akai_wav2sample_exit;
		}
	}

	/* read and parse WAV header */
	if (wav_read_head(wavfd,&bcount,
							&wavsamplesize,&chnr,&samplerate,&bitnr,
#ifndef WAV_AKAIHEAD_DISABLE
							&extrasize,
#else
							NULL,
#endif
							&errstrp,
							&ltype, &lstart, &lend)<0){
		if (errstrp!=NULL){
			fprintf(stderr,"%s\n",errstrp);
		}
		/* error, don't know how many bytes read */
		goto akai_wav2sample_exit;
	}

	printf("bcount: %d, wavsamplesize: %d, chnr: %d, samplerate: %d, bitnr: %d, extrasize: %d\n",
		bcount, wavsamplesize, chnr, samplerate, bitnr, extrasize);

	/* check parameters */
	if (type == AKAI_SAMPLE900_FTYPE) {
		if (chnr!=1){
			fprintf(stderr,"WAV must be mono on S900\n");
			/* unknown or unsupported */
			ret=1; /* no error */
			goto akai_wav2sample_exit;
		}
	} else {
		if(chnr == 2) {
			tempbuf = (u_char *)malloc(wavsamplesize);
			if (READ(wavfd,tempbuf,wavsamplesize)!=(int)wavsamplesize){
				fprintf(stderr,"cannot read stereo sample\n");
				goto akai_wav2sample_exit;
			}
			wavsamplesize /= 2;
		}
	}
	if (bitnr!=16){
		fprintf(stderr,"WAV must be 16bit\n");
		/* unknown or unsupported */
		ret=1; /* no error */
		goto akai_wav2sample_exit;
	}
	

	for(ch = 0; ch < chnr; ch++) {
		printf("writing channel %d (wavsamplesize = %d)\n", ch, wavsamplesize);

		/* number of samples in WAV file */
		/* Note: can be an odd number */
		wavsamplecount=wavsamplesize/2; /* /2 for 16bit per WAV sample word */

		/* allocate WAV sample buffer */
		if (type==AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
			/* Note: allocate WAV buffer for an rounded up even number of samples */
			wavsamplesizealloc=(0xfffffffe&(wavsamplecount+1))*2; /* *2 for 16bit per WAV sample word */
		}else{
			wavsamplesizealloc=wavsamplesize;
		}
		wavbuf=(u_char *)malloc(wavsamplesizealloc);
		if (wavbuf==NULL){
			perror("malloc");
			goto akai_wav2sample_exit;
		}

		if(chnr==2) {
			u_int i, j;
			u_short * d = (u_short*)wavbuf;
			u_short * s = (u_short*)tempbuf;

			for(i = ch, j = 0; i < wavsamplesize; i+=2, j++) {
				d[j] = s[i];
			}
		} else {
			/* read WAV sample to memory */
			if (READ(wavfd,wavbuf,wavsamplesize)!=(int)wavsamplesize){
				fprintf(stderr,"cannot read sample\n");
				goto akai_wav2sample_exit;
			}
		}

		bcount+=wavsamplesize;
		if (wavsamplesize<wavsamplesizealloc){
			/* zero padding */
			bzero(wavbuf+wavsamplesize,wavsamplesizealloc-wavsamplesize);
		}

		/* sample size */
		if (type==AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
			/* S900 sample */
			/* number of samples per part  */
			samplecountpart=(wavsamplecount+1)/2; /* round up */
			samplecount=2*samplecountpart; /* samplecount must be an even number */
			if (s9cflag){
				/* S900 compressed sample format */
				/* first pass to convert 16bit WAV sample format into S900 compressed sample format */
				/* Note: first pass of akai_sample900compr_wav2sample() requires WAV sample to be loaded to wavbuf */
				r=akai_sample900compr_wav2sample(NULL,wavbuf,samplecountpart); /* NULL: first pass */
				if (r<0){
					goto akai_wav2sample_exit;
				}
				/* size in bytes */
				samplesize=(u_int)r;
			}else{
				/* S900 non-compressed sample format */
				/* size in bytes */
				samplesize=3*samplecountpart;
			}
		}else{
			/* S1000/S3000 sample */
			samplecountpart=0;
			samplecount=wavsamplecount;
			/* size in bytes */
			samplesize=samplecount*2; /* *2 for 16bit per sample word */
		}
		if (hdrsize+samplesize>AKAI_FILE_SIZEMAX){
			fprintf(stderr,"WAV too large\n");
			/* unknown or unsupported */
			ret=1; /* no error */
			goto akai_wav2sample_exit;
		}

		if (type==AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
			/* allocate sample buffer */
			sbuf=(u_char *)malloc(samplesize);
			if (sbuf==NULL){
				perror("malloc");
				goto akai_wav2sample_exit;
			}
		}else{
			/* Note: no sample format conversion necessary for S1000/S3000 */
			sbuf=wavbuf;
		}

		/* sample name */
		if ((type==AKAI_SAMPLE900_FTYPE)||(volp->type==AKAI_VOL_TYPE_S900)){ /* S900 sample or S900 volume? */
			if (nlen>AKAI_NAME_LEN_S900){
				nlen=AKAI_NAME_LEN_S900;
			}
		}else{
			if (nlen>AKAI_NAME_LEN){
				nlen=AKAI_NAME_LEN;
			}
		}
		bcopy(wavname,sname,nlen);
		sname[nlen]='\0';

		if(chnr == 1) {
			sprintf(fname,"%12s%s",sname,tname);
		} else {
			if(ch == 0) {
				sprintf(fname,"%-10s-L%s",sname,tname);
				sprintf(sname,"%-10s-L",sname);
			} else {
				sprintf(fname,"%-10s-R%s",sname,tname);
				sprintf(sname,"%-10s-R",sname);
			}
		}
		
		printf("fname=%s, sname=%s\n", fname, sname);

	#ifdef WAV2SAMPLE_OVERWRITE
		if ((findex==AKAI_CREATE_FILE_NOINDEX)
			/* check if destination file already exists */
			&&(akai_find_file(volp,&tmpfile,fname)==0)){
			/* exists */
			if (what&WAV2SAMPLE_OVERWRITE){
				/* delete file */
				printf("overwriting\n");
				if (akai_delete_file(&tmpfile)<0){
					fprintf(stderr,"cannot overwrite existing file\n");
					goto akai_wav2sample_exit;
				}
			}else{
				fprintf(stderr,"file name already used\n");
				goto akai_wav2sample_exit;
			}
		}
	#endif

		/* correct osver if necessary */
		if (type==AKAI_SAMPLE900_FTYPE){
			/* S900 sample */
			if (s9cflag){
				/* S900 compressed sample format */
				/* non-compressed sample size in bytes */
				osver=3*samplecountpart;
				/* number of un-compressed floppy blocks */
				/* Note: without sample header */
				osver=(osver+AKAI_FL_BLOCKSIZE-1)/AKAI_FL_BLOCKSIZE; /* round up */
				if (osver==0){ /* unsuitable osver? */
					osver=1; /* XXX non zero */
				}
			}else{
				/* S900 non-compressed sample format */
				osver=0;
			}
		}else if (type==AKAI_SAMPLE1000_FTYPE){
			/* S1000 sample */
			if ((osver==AKAI_OSVER_S900VOL)||(osver>AKAI_OSVER_S1100MAX)){
				osver=AKAI_OSVER_S1000MAX; /* XXX */
			}
		}else{
			/* S3000 sample */
			if ((osver==AKAI_OSVER_S900VOL)||(osver>AKAI_OSVER_S3000MAX)){
				osver=AKAI_OSVER_S3000MAX; /* XXX */
			}
		}

		/* create file */
		/* Note: akai_create_file() will correct osver if necessary */
		if (akai_create_file(volp,&tmpfile,
							 hdrsize+samplesize,
							 findex,
							 fname,
							 osver,
							 tagp)<0){
			fprintf(stderr,"cannot create file\n");
			goto akai_wav2sample_exit;
		}

#ifndef WAV_AKAIHEAD_DISABLE
		/* check for sample header chunk in WAV file */
		{
			u_int wavakaiheadsearchtype;
			int wavakaiheadtype;
			u_int wavakaiheadsize;
			u_int bc;

			/* matching type */
			if (type==AKAI_SAMPLE900_FTYPE){
				wavakaiheadsearchtype=WAV_AKAIHEADTYPE_SAMPLE900;
			}else if (type==AKAI_SAMPLE1000_FTYPE){
				wavakaiheadsearchtype=WAV_AKAIHEADTYPE_SAMPLE1000;
			}else{
				wavakaiheadsearchtype=WAV_AKAIHEADTYPE_SAMPLE3000;
			}

			wavakaiheadtype=wav_find_akaihead(wavfd,&bc,&wavakaiheadsize,extrasize,wavakaiheadsearchtype);
			if (wavakaiheadtype<0){
				goto akai_wav2sample_exit;
			}
			bcount+=bc;

			if ((wavakaiheadtype==(int)wavakaiheadsearchtype)&&(wavakaiheadsize==hdrsize)){
				/* found matching sample header chunk */
				wavakaiheadfound=1;
			}else{
				wavakaiheadfound=0;
			}
		}
#endif

		if (type==AKAI_SAMPLE900_FTYPE){
#ifndef WAV_AKAIHEAD_DISABLE
			if (wavakaiheadfound){
				/* read S900 sample header */
				if (READ(wavfd,(u_char *)&s900hdr,hdrsize)!=(int)hdrsize){
					fprintf(stderr,"cannot read sample header\n");
					goto akai_wav2sample_exit;
				}
				bcount+=hdrsize;
#if 1
				printf("S900 sample header imported from WAV\n");
#endif
			}else
#endif
			{
				/* create S900 sample header */
				bzero(&s900hdr,sizeof(struct akai_sample900_s));

				s900hdr.srate[1]=0xff&(samplerate>>8);
				s900hdr.srate[0]=0xff&samplerate;

				s900hdr.npitch[1]=0xff&(SAMPLE900_NPITCH_DEF>>8); /* XXX */
				s900hdr.npitch[0]=0xff&SAMPLE900_NPITCH_DEF; /* XXX */

				s900hdr.pmode=SAMPLE900_PMODE_ONESHOT; /* XXX */

				/* Note: use wavsamplecount for end */
				s900hdr.end[3]=0xff&(wavsamplecount>>24);
				s900hdr.end[2]=0xff&(wavsamplecount>>16);
				s900hdr.end[1]=0xff&(wavsamplecount>>8);
				s900hdr.end[0]=0xff&wavsamplecount;

				/* Note: use wavsamplecount for llen */
				s900hdr.llen[3]=0xff&(wavsamplecount>>24);
				s900hdr.llen[2]=0xff&(wavsamplecount>>16);
				s900hdr.llen[1]=0xff&(wavsamplecount>>8);
				s900hdr.llen[0]=0xff&wavsamplecount;

				s900hdr.dir=SAMPLE900_DIR_NORM; /* XXX */
			}

			/* set correct slen */
			s900hdr.slen[3]=0xff&(samplecount>>24);
			s900hdr.slen[2]=0xff&(samplecount>>16);
			s900hdr.slen[1]=0xff&(samplecount>>8);
			s900hdr.slen[0]=0xff&samplecount;

			/* set RAM name of sample */
			/* Note: akai_fixramname() not needed afterwards */
			ascii2akai_name(sname,(u_char *)s900hdr.name,1); /* 1: S900 */

			/* write sample header */
			if (akai_write_file(0,(u_char *)&s900hdr,&tmpfile,0,hdrsize)<0){
				fprintf(stderr,"cannot write sample header\n");
				goto akai_wav2sample_exit;
			}

			if (s9cflag){
				/* second pass to convert 16bit WAV sample format into S900 compressed sample format */
				if (akai_sample900compr_wav2sample(sbuf,wavbuf,samplecountpart)<0){ /* sbuf!=NULL: second pass */
					goto akai_wav2sample_exit;
				}
			}else{
				/* convert 16bit WAV sample format into S900 non-compressed sample format */
				akai_sample900noncompr_wav2sample(sbuf,wavbuf,samplecountpart);
			}
		}else{
#ifndef WAV_AKAIHEAD_DISABLE
			if (wavakaiheadfound){
				/* read S1000/S3000 sample header */
				/* Note: S1000 header is contained within S3000 header */
				if (READ(wavfd,(u_char *)&s3000hdr,hdrsize)!=(int)hdrsize){
					fprintf(stderr,"cannot read sample header\n");
					goto akai_wav2sample_exit;
				}
				bcount+=hdrsize;
#if 1
				if (type==AKAI_SAMPLE1000_FTYPE){
					printf("S1000 sample header imported from WAV\n");
				}else{
					printf("S3000 sample header imported from WAV\n");
				}
#endif
			}else
#endif
			{
				/* create S3000 sample header */
				/* Note: S1000 header is contained within S3000 header */
				bzero(&s3000hdr,sizeof(struct akai_sample3000_s));

				s3000hdr.s1000.blockid=SAMPLE1000_BLOCKID;
				s3000hdr.s1000.bandw=SAMPLE1000_BANDW_20KHZ; /* XXX */
				s3000hdr.s1000.rkey=60; /* XXX */
				s3000hdr.s1000.dummy1=0x80; /* XXX */

				/* Note: use samplelen-1 for end */
				s3000hdr.s1000.end[3]=0xff&((samplecount-1)>>24);
				s3000hdr.s1000.end[2]=0xff&((samplecount-1)>>16);
				s3000hdr.s1000.end[1]=0xff&((samplecount-1)>>8);
				s3000hdr.s1000.end[0]=0xff&(samplecount-1);

				s3000hdr.s1000.stpaira[1]=0xff&(AKAI_SAMPLE1000_STPAIRA_NONE>>8);
				s3000hdr.s1000.stpaira[0]=0xff&AKAI_SAMPLE1000_STPAIRA_NONE;

				s3000hdr.s1000.srate[1]=0xff&(samplerate>>8);
				s3000hdr.s1000.srate[0]=0xff&samplerate;
			}

			/* set correct slen */
			s3000hdr.s1000.slen[3]=0xff&(samplecount>>24);
			s3000hdr.s1000.slen[2]=0xff&(samplecount>>16);
			s3000hdr.s1000.slen[1]=0xff&(samplecount>>8);
			s3000hdr.s1000.slen[0]=0xff&samplecount;

			/* set loop data */
			if(ltype == 0) {
				u_int llen = lend - lstart;

				s3000hdr.s1000.lnum = 1;
				s3000hdr.s1000.lfirst = 0;
				
				s3000hdr.s1000.loop[0].at[3]=0xff&(lend>>24);
				s3000hdr.s1000.loop[0].at[2]=0xff&(lend>>16);
				s3000hdr.s1000.loop[0].at[1]=0xff&(lend>>8);
				s3000hdr.s1000.loop[0].at[0]=0xff&lend;

				s3000hdr.s1000.loop[0].flen[1]=0;
				s3000hdr.s1000.loop[0].flen[0]=0;

				s3000hdr.s1000.loop[0].len[3]=0xff&(llen>>24);
				s3000hdr.s1000.loop[0].len[2]=0xff&(llen>>16);
				s3000hdr.s1000.loop[0].len[1]=0xff&(llen>>8);
				s3000hdr.s1000.loop[0].len[0]=0xff&llen;

				s3000hdr.s1000.loop[0].time[1]=0xff&(SAMPLE1000LOOP_TIME_HOLD>>8);
				s3000hdr.s1000.loop[0].time[0]=0xff&SAMPLE1000LOOP_TIME_HOLD;

				printf("Created loop at %d with length %d\n", lend, llen);
			} else {
				printf("Unsupported loop type %d\n", ltype);
			}

			/* set RAM name of sample */
			/* Note: akai_fixramname() not needed afterwards */
			ascii2akai_name(sname,s3000hdr.s1000.name,0); /* 0: not S900 */

			/* write sample header */
			if (akai_write_file(0,(u_char *)&s3000hdr,&tmpfile,0,hdrsize)<0){
				fprintf(stderr,"cannot write sample header\n");
				goto akai_wav2sample_exit;
			}

			/* Note: no sample format conversion necessary for S1000/S3000 */
		}

		/* write sample */
		if (akai_write_file(0,sbuf,&tmpfile,hdrsize,hdrsize+samplesize)<0){
			fprintf(stderr,"cannot write sample\n");
			goto akai_wav2sample_exit;
		}
	}

	printf("sample imported from WAV\n");

	ret=0; /* success */

akai_wav2sample_exit:
	if (tempbuf!=NULL){
		free(tempbuf);
	}
	if (wavbuf!=NULL){
		free(wavbuf);
	}
	if (type==AKAI_SAMPLE900_FTYPE){ /* S900 sample? */
		if (sbuf!=NULL){
			free(sbuf);
		}
		if (s9cflag){
			akai_sample900compr_wav2sample(NULL,NULL,0); /* NULL,NULL: free buffers if still allocated */
		}
	}
	if (what&WAV2SAMPLE_OPEN){
		if (wavfd>=0){
			CLOSE(wavfd);
		}
	}
	if (bcountp!=NULL){
		*bcountp=bcount;
	}
	return ret;
}



/* EOF */
