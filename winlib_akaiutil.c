/*
* unless noted otherwise: Copyright (C) 2003-2010 K.M.Indlekofer. All rights reserved.
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



#include "winlib_akaiutil.h"



/* bcopy, bzero */
/* (c) 2003, 2004 by K. M. Indlekofer */
void
bcopy(const void *src,void *dst,size_t len)
{
	
	/* XXX no check for NULL pointer */
	while (len-->0){
		*(((char *)dst)++)=*(((char *)src)++);
	}
}

void
bzero(void *b,size_t len)
{
	
	/* XXX no check for NULL pointer */
	while (len-->0){
		*(((char *)b)++)=0;
	}
}



/* strcasecmp */
/*
* Copyright (c) 1987, 1993
*    The Regents of the University of California.  All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
*    must display the following acknowledgement:
* 	This product includes software developed by the University of
* 	California, Berkeley and its contributors.
* 4. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

/*
* This array is designed for mapping upper and lower case letter
* together for a case independent comparison.  The mappings are
* based upon ascii character sequences.
*/
static const u_char charmap[] = {
	0000, 0001, 0002, 0003, 0004, 0005, 0006, 0007,
		0010, 0011, 0012, 0013, 0014, 0015, 0016, 0017,
		0020, 0021, 0022, 0023, 0024, 0025, 0026, 0027,
		0030, 0031, 0032, 0033, 0034, 0035, 0036, 0037,
		0040, 0041, 0042, 0043, 0044, 0045, 0046, 0047,
		0050, 0051, 0052, 0053, 0054, 0055, 0056, 0057,
		0060, 0061, 0062, 0063, 0064, 0065, 0066, 0067,
		0070, 0071, 0072, 0073, 0074, 0075, 0076, 0077,
		0100, 0141, 0142, 0143, 0144, 0145, 0146, 0147,
		0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
		0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
		0170, 0171, 0172, 0133, 0134, 0135, 0136, 0137,
		0140, 0141, 0142, 0143, 0144, 0145, 0146, 0147,
		0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
		0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
		0170, 0171, 0172, 0173, 0174, 0175, 0176, 0177,
		0200, 0201, 0202, 0203, 0204, 0205, 0206, 0207,
		0210, 0211, 0212, 0213, 0214, 0215, 0216, 0217,
		0220, 0221, 0222, 0223, 0224, 0225, 0226, 0227,
		0230, 0231, 0232, 0233, 0234, 0235, 0236, 0237,
		0240, 0241, 0242, 0243, 0244, 0245, 0246, 0247,
		0250, 0251, 0252, 0253, 0254, 0255, 0256, 0257,
		0260, 0261, 0262, 0263, 0264, 0265, 0266, 0267,
		0270, 0271, 0272, 0273, 0274, 0275, 0276, 0277,
		0300, 0301, 0302, 0303, 0304, 0305, 0306, 0307,
		0310, 0311, 0312, 0313, 0314, 0315, 0316, 0317,
		0320, 0321, 0322, 0323, 0324, 0325, 0326, 0327,
		0330, 0331, 0332, 0333, 0334, 0335, 0336, 0337,
		0340, 0341, 0342, 0343, 0344, 0345, 0346, 0347,
		0350, 0351, 0352, 0353, 0354, 0355, 0356, 0357,
		0360, 0361, 0362, 0363, 0364, 0365, 0366, 0367,
		0370, 0371, 0372, 0373, 0374, 0375, 0376, 0377
};

int
strcasecmp(const char *s1, const char *s2) {
	const u_char *cm = charmap,
		*us1 = (const u_char *)s1,
		*us2 = (const u_char *)s2;
	
	while (cm[*us1] == cm[*us2++])
		if (*us1++ == '\0')
			return (0);
		return (cm[*us1] - cm[*--us2]);
}

int
strncasecmp(const char *s1, const char *s2, size_t n) {
	if (n != 0) {
		const u_char *cm = charmap,
			*us1 = (const u_char *)s1,
			*us2 = (const u_char *)s2;
		
		do {
			if (cm[*us1] != cm[*us2++])
				return (cm[*us1] - cm[*--us2]);
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}



/* getopt */
/*
* getopt.c --
*
*      Standard UNIX getopt function.  Code is from BSD.
*
* Copyright (c) 1987-2002 The Regents of the University of California.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* A. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
* B. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
* C. Neither the names of the copyright holders nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
* IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/* #if !defined(lint)
* static char sccsid[] = "@(#)getopt.c 8.2 (Berkeley) 4/2/94";
* #endif
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* declarations to provide consistent linkage */
extern char *optarg;
extern int optind;
extern int opterr;

int     opterr = 1,             /* if error message should be printed */
optind = 1,             /* index into parent argv vector */
optopt,                 /* character checked for validity */
optreset;               /* reset getopt */
char    *optarg;                /* argument associated with option */

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

								/*
								* getopt --
								*      Parse argc/argv argument vector.
*/
int
getopt(nargc, nargv, ostr)
int nargc;
char * const *nargv;
const char *ostr;
{
	static char *place = EMSG;              /* option letter processing */
	char *oli;                              /* option letter list index */
	
	if (optreset || !*place) {              /* update scanning pointer */
		optreset = 0;
		if (optind >= nargc || *(place = nargv[optind]) != '-') {
			place = EMSG;
			return (EOF);
		}
		if (place[1] && *++place == '-') {      /* found "--" */
			++optind;
			place = EMSG;
			return (EOF);
		}
	}                                       /* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' ||
		!(oli = strchr(ostr, optopt))) {
		/*
		* if the user didn't specify '-' as an option,
		* assume it means EOF.
		*/
		if (optopt == (int)'-')
			return (EOF);
		if (!*place)
			++optind;
		if (opterr && *ostr != ':')
			(void)fprintf(stderr,
			"%s: illegal option -- %c\n", __FILE__, optopt);
		return (BADCH);
	}
	if (*++oli != ':') {                    /* don't need argument */
		optarg = NULL;
		if (!*place)
			++optind;
	}
	else {                                  /* need an argument */
		if (*place)                     /* no white space */
			optarg = place;
		else if (nargc <= ++optind) {   /* no arg */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (opterr)
				(void)fprintf(stderr,
				"%s: option requires an argument -- %c\n",
				__FILE__, optopt);
			return (BADCH);
		}
		else                            /* white space */
			optarg = nargv[optind];
		place = EMSG;
		++optind;
	}
	return (optopt);                        /* dump back option letter */
}



/* EOF */
