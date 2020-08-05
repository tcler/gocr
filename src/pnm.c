/*
This is a Optical-Character-Recognition program
Copyright (C) 2000-2019  Joerg Schulenburg

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 see README for EMAIL-address

    v0.1.0 initial version (stdin added)
    v0.2.0 popen added
    v0.2.7 review by Bruno Barberi Gnecco
    v0.39  autoconf
    v0.41  fix integer and heap overflow, change color output
    v0.46  fix blank spaces problem in filenames
    v0.52  2019-04 add handling of pam-format
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
/* #include <unistd.h> */
#endif

/* Windows needs extra code to work fine, ^Z in BMP's will stop input else.
 * I do not have any idea when this text mode will be an advantage
 * but the MS community seems to like to do simple things in a complex way. */
#if defined(O_BINARY) && (defined(__WIN32) || defined(__WIN32__)\
 || defined(__WIN64) || defined(__WIN64__) || defined(__MSDOS__))
# include <fcntl.h>
# define SET_BINARY(_f) do {if (!isatty(_f)) setmode (_f, O_BINARY);} while (0)
#else
# define SET_BINARY(f) (void)0
#endif

#include "pnm.h"
#ifdef HAVE_PAM_H
# include <pam.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#else
# include <ctype.h>
#endif

#define EE()         fprintf(stderr,"\nERROR "__FILE__" L%d: ",__LINE__)
#define E0(x0)       {EE();fprintf(stderr,x0 "\n");      }
#define F0(x0)       {EE();fprintf(stderr,x0 "\n");      exit(1);}
#define F1(x0,x1)    {EE();fprintf(stderr,x0 "\n",x1);   exit(1);}
#define F2(x0,x1,x2) {EE();fprintf(stderr,x0 "\n",x1,x2);exit(1);}

/*
 * Weights to use for the different colours when converting a ppm
 * to greyscale.  These weights should sum to 1.0
 * 
 * The below values have been chosen to reflect the fact that paper
 * goes a reddish-yellow as it ages.
 *
 * v0.41: for better performance, we use integer instead of double
 *        this integer value divided by 1024 (2^10) gives the factor 
 */
#define PPM_RED_WEIGHT   511  /* .499 */
#define PPM_GREEN_WEIGHT 396  /* .387 */
#define PPM_BLUE_WEIGHT  117  /* .114 */

/*
    feel free to expand this list of usable converting programs
    Note 1: the last field must be NULL.
    Note 2: "smaller" extensions must come later: ".pnm.gz" must come
       before ".pnm".
    calling external programs is a security risk
    ToDo: for better security replace gzip by /usr/bin/gzip or ENV?!
    ToDo: option -z to trigger decompression?
 */
char *xlist[]={
  ".pnm.gz",	"gzip -cd",  /* compressed pnm-files, gzip package */
  ".pbm.gz",	"gzip -cd",
  ".pgm.gz",	"gzip -cd",
  ".ppm.gz",	"gzip -cd",
  ".pam.gz",	"gzip -cd",  /* pam added JS1904 */
  ".pnm.bz2",	"bzip2 -cd",
  ".pbm.bz2",	"bzip2 -cd",
  ".pgm.bz2",	"bzip2 -cd",
  ".ppm.bz2",	"bzip2 -cd",
  ".pam.bz2",	"bzip2 -cd",
  ".jpg", 	"djpeg -gray -pnm",  /* JPG/JPEG, jpeg package */
  ".jpeg",	"djpeg -gray -pnm",
  ".gif",	"giftopnm -image=all",  /* GIF, netpbm package */
  ".bmp",	"bmptoppm",
  ".tiff",	"tifftopnm",
  ".png",	"pngtopnm", /* Portable Network Graphics (PNG) format */
  /* pstopnm.default: -xmax 612, -xsize ? -ymax 792 -ysize ? */
  ".ps",	"pstopnm -stdout -portrait -pgm", /* postscript */
  ".eps",	"pstopnm -stdout -portrait -pgm", /* encapsulated postscript */
   /* gs -sDEVICE=pgmraw -sOutputFile=- -g609x235 -r141x141 -q -dNOPAUSE */
  ".fig",	"fig2dev -L ppm -m 3", /* xfig files, transfig package */
  /* 2019-04 int_overflows_WDietz15.pdf 27 pages 271KB 
   *  150dpi=55MB  default = bad 
   *  200dpi=97MB  = bad, some words ok 
   *  300dpi=217MB=8.5MBgz=4.1MB.bz2=3.6MB.xz best=3.1MB.xz  2.5Kx3.3K
   *              png=16MB monopng=3.9MB=worse_ocr gray.page1=773KB
   *              pdf2ppm=21MB/60MB,40s gocr=16MB/23MB,14.7m partly readable
   *              ImageMagick-6.7.7_2018.display=1.2GB++ crash
   *  600dpi=867MB pdftoppm=69MB,150s 5.1Kx6.6K=34MB gocr=40MB,30.5m
   *    Xfce.ristretto 1:1 unsharp 126MB v433MB
   *    feh    ram=132MB   1:1 sharp zoom+shift page1-only
   *    mirage ram=135MB/465MB sharp 1:1, zoom, scroll
   *    xzgv   ram=105MB/221MB 1:1  1st page only, scroll
   *    sxiv   ram=130MB/201MB zoom 1st page only, scroll
   *    pnmsplit multi_img.pnm 'image%d' # to split multi-image PNM/PAM 
   *    pampick 0 multi_img.pnm    # pick 1st page
   *   -r 150(dflt_dpi) -png -f 2(first_page) -l 2(last_page)
   *    but renders web-link-boxes (better color?)
   *   ToDo: try multi-img-png + bad-spacing of 2nd-multi-img-page
   */
  ".pdf",	"pdftoppm -r 300 -gray", /* -r 150 (dpi) -mono -png JS1904 */
  NULL
};

/* return a pointer to command converting file to pnm or NULL */
char *testsuffix(char *name){
  int i; char *rr;

  for(i = 0; xlist[i] != NULL; i += 2 ) {
    // ToDo: new version = no case sensitivity, .JPG = .jpg
    //  xlen=length(xlist[i]);
    //  rlen=length(rr);
    //  if (xlen>rlen && strncasecmp(rr+rlen-xlen,xlist[i])==0) ...
    // old version = case sensitivity!
    if((rr=strstr(name, xlist[i])) != NULL)
      if(strlen(rr)==strlen(xlist[i])) /* handle *.eps.pbm correct */
        return xlist[i+1];
  }
  return NULL;
}


char read_char(FILE *f1){ // filter #-comments transparently
  char c;                 // JS19 add vvv and output comments to stderr?
  int  m;
  static int nEOF=0;
  if (nEOF > 99) exit(1); /* JS1904 */
  for(m=0;;){
    c=fgetc(f1);
    if( feof(f1)   ) { E0("read feof"); m=0; nEOF++; } /* 2019-04 JS m=0 */
    if( ferror(f1) ) F0("read ferror");   /* exit */
    if( c == '#'  )  { m = 1; continue; } /* start comment */
    if( m ==  0   )  return c;            /* no comment, return */
    if( c == '\n' )  m = 0;          
  } // endless loop
} // read_char


/*
   for simplicity only PAM of netpbm is used, the older formats
   PBM, PGM and PPM can be handled implicitly by PAM routines (js05)
   v0.43: return 1 if multiple file (hold it open), 0 otherwise
 */
#ifdef HAVE_PAM_H
int readpgm(char *name, pix * p, int vvv) {
  static FILE *fp=NULL;
  static char *pip; 
  char magic1, magic2;
  int i, j, sample, minv = 0, maxv = 0, eofP=0;
  struct pam inpam;
  tuple *tuplerow;

  assert(p);

  if (!fp) { // fp!=0 for multi-pnm and idx>0
    /* open file; test if conversion is needed. */
    if (name[0] == '-' && name[1] == '\0') { /* - means /dev/stdin */
      fp = stdin;
      SET_BINARY (fileno(fp)); /* Windows-OS needs it for correct work */
    }
    else {
      pip = testsuffix(name);
      if (!pip) {
        fp = fopen(name, "rb");
        if (!fp)
          F1("opening file %s", name);
      }
      else { /* ToDo: test bad names containing ;|"$() utf8 etc., safety */ 
        char *buf = (char *)malloc((strlen(pip)+strlen(name)+4));
        sprintf(buf, "%s \"%s\"", pip, name); /* allow spaces in filename */
        if (vvv) {
          fprintf(stderr, "# popen( %s )\n", buf);
        }
#ifdef HAVE_POPEN
        /* potential security vulnerability, if name contains tricks */
        /* example: gunzip -c dummy | rm -rf * */
        /* windows needs "rb" for correct work, linux not, cygwin? */
        /* ToDo: do you have better code to go arround this? */
#if defined(__WIN32) || defined(__WIN32__) || defined(__WIN64) || defined(__WIN64__)
	fp = popen(buf, "rb");  /* ToDo: may fail, please report */
	if (!fp) fp = popen(buf, "r"); /* 2nd try, the gnu way */
#else
        fp = popen(buf, "r");
#endif
#else
        F0("sorry, compile with HAVE_POPEN to use pipes");
#endif
        if (!fp)
          F1("opening pipe %s", buf);
        free(buf);
      }
    }
  }

  /* netpbm 0.10.36 tries to write a comment to nonzero char** comment_p */
  /* patch by C.P.Schmidt 21Nov06 */
  memset (&inpam, 0, sizeof(inpam));

  /* read pgm-header */
  /* struct pam may change between netpbm-versions, causing problems? */
#ifdef PAM_STRUCT_SIZE  /* ok for netpbm-10.35 */
  /* new-and-better? but PAM_STRUCT_SIZE is not defined in netpbm-10.18 */
  pnm_readpaminit(fp, &inpam, PAM_STRUCT_SIZE(tuple_type));
#else /* ok for netpbm-10.18 old-and-bad for new netpbms */
  pnm_readpaminit(fp, &inpam, sizeof(inpam));
#endif

  p->x = inpam.width;
  p->y = inpam.height;
  magic1=(inpam.format >> 8) & 255; /* 'P' for PNM,PAM */
  magic2=(inpam.format     ) & 255; /* '7' for PAM */
  minv=inpam.maxval;
  if (vvv) {
      fprintf(stderr, "# readpam: format=0x%04x=%c%c h*w(d*b)=%d*%d(%d*%d)\n",
        inpam.format, /* magic1*256+magic2 */
        ((magic1>31 && magic1<127)?magic1:'.'),
        ((magic2>31 && magic2<127)?magic2:'.'),
        inpam.height,
        inpam.width,
        inpam.depth,
        inpam.bytes_per_sample);
  }
  if ( (1.*(p->x*p->y))!=((1.*p->x)*p->y) )
    F0("Error integer overflow");
  if ( !(p->p = (unsigned char *)malloc(p->x*p->y)) )
    F1("Error at malloc: p->p: %d bytes", p->x*p->y);
  tuplerow = pnm_allocpamrow(&inpam);
  for ( i=0; i < inpam.height; i++ ) {
    pnm_readpamrow(&inpam, tuplerow);   /* exit on error */
    for ( j = 0; j < inpam.width; j++ ) {
      if (inpam.depth>=3)
        /* tuplerow is unsigned long (see pam.h sample) */
        /* we expect 8bit or 16bit integers,
           no overflow up to 32-10-2=20 bits */
        sample
          = ((PPM_RED_WEIGHT   * tuplerow[j][0] + 511)>>10)
          + ((PPM_GREEN_WEIGHT * tuplerow[j][1] + 511)>>10)
          + ((PPM_BLUE_WEIGHT  * tuplerow[j][2] + 511)>>10);
      else
        sample = tuplerow[j][0];
      sample = 255 * sample / inpam.maxval; /* normalize to 8 bit */
      p->p[i*inpam.width+j] = sample;
      if (maxv<sample) maxv=sample;
      if (minv>sample) minv=sample;
    }
  }
  pnm_freepamrow(tuplerow);
  pnm_nextimage(fp,&eofP);
  if (vvv)
    fprintf(stderr,"# readpam: min=%d max=%d eof=%d\n", minv, maxv, eofP);
  p->bpp = 1;
  if (eofP) {
    if (!pip) fclose(fp);
#ifdef HAVE_POPEN
    else      pclose(fp);	/* close pipe (v0.43) */
#endif
    fp=NULL; return 0; 
  }
  return 1; /* multiple image = concatenated pnm */
}

#else
/*
   if PAM not installed, here is the fallback routine,
   which is not so powerful but needs no dependencies from other libs
   bps: bytes per sample, values=0...((256^bps)-1)
 */
static int fread_num(char *buf, int bps, FILE *f1) { /* read sample */
  int mode, j2, j3; char c1;
  for (j2=0;j2<bps;j2++) buf[j2]=0; // initialize value to zero
  for(mode=0;!feof(f1);){ // mod=0: skip leading spaces, 1: scan digits
    c1=read_char(f1);     // filter out #-comments inclusive 1st \n
    if (isspace(c1)) { if (mode==0) continue; else break; }
    mode=1; // digits scan mode
    if( !isdigit(c1) ) F0("unexpected char");
    for (j3=j2=0;j2<bps;j2++) {  /* multiply bps*bytes by 10 */
      j3 = buf[j2]*10 + j3;  /* j3 is used as result and byte-carry */
      buf[j2]=j3 & 255; j3>>=8; 
    } /* bps loop */
    buf[0] += c1-'0';  /* add digit c1 = 0..9 */
  } /* loop chars, mode 0,1 */
  return 0;
}  /* fread_num */

/*
 * read image file, used to read the OCR-image and database images,
 * image file can be PBM/PGM/PPM in RAW or TEXT
 *   name: filename of image (input)
 *   p:    pointer where to store the loaded image (input)
 *   vvv:  verbose mode (input)
 *   return: 0=ok, 1=further image follows (multiple image), -1 on error
 * this is the fall back routine if libpnm cant be used
P1
# ASCII-bitmap 1bit with or without spaces, comments at any line
6 2
000001 0 1 0 0 1 1 
P16 2 000001 010011 # also valid
 */
int readpgm( char *name, pix *p, int vvv){
  static char c1, c2;           /* magic bytes, file type */
  static char *pip;             // static to survive multiple calls
  int  number=0, nx=0,ny=0,nc=0, mod=0,
       i, j; // nc=num_color, mod=read_modus
  static FILE *f1=NULL;         // trigger read new file or multi image file
  unsigned char *pic;
  char buf[512];
  int lx, ly, dx;
  int bps=1; /* bps: bytes per sample, values=0...((256^bps)-1) */
  int depth=1; /* number of planes or channels, gray=1 RGB=3 JS1904 */
  char pam_token[8+1], tupletype[9+1];  /* PAM format: P7\n[# comments\n] */ 

  if (!f1) {  /* first of multiple image, on MultipleImageFiles c1 was read */
    pip=NULL;
    if (name[0]=='-' && name[1]==0) {
      f1=stdin;  /* is this correct ??? */
      SET_BINARY (fileno(f1)); // Windows needs it for correct work
    } else {
      pip=testsuffix(name);
      if (!pip) {
        f1=fopen(name,"rb"); if (!f1) F1("opening file %s",name);
      } else {  // sprintf(buf,"%s \"%s\"",pip,name); /* snprintf simu */
        for(i=0;i<sizeof(buf)-4 && pip[i];i++) buf[i]=pip[i];
        buf[i++]=' '; buf[i++]='"';
        for(j=0;i<sizeof(buf)-3 && name[j];i++,j++) buf[i]=name[j];
        buf[i++]='"'; buf[i++]=0;
        // sprintf(buf,"%s \"%s\"",pip,name); /* ToDo: how to prevent OVL ? */
        if (vvv) { fprintf(stderr,"# popen( %s )\n",buf); }
#ifdef HAVE_POPEN
#if defined(__WIN32) || defined(__WIN32__) || defined(__WIN64) || defined(__WIN64__)
	f1 = popen(buf, "rb");  /* ToDo: may fail, please report */
	if (!f1) f1 = popen(buf, "r"); /* 2nd try, the gnu way */
#else
        f1=popen(buf,"r");
#endif
#else
        F0("only PNM files supported (compiled without HAVE_POPEN)");
#endif
        if (!f1) F1("opening pipe %s",buf);
      } /* file/pipe */
    } /* stdin or file/pipe */
    c1=fgetc(f1); if (feof(f1)) { E0("unexpected EOF"); return -1; }
  } /* if closed, open file and read 1st char, if open c1 was read  */
  c2=fgetc(f1);   if (feof(f1)) { E0("unexpected EOF"); return -1; }
  // check the first two bytes of the PNM file 
  //         PBM   PGM   PPM  PAM
  //    TXT   P1    P2    P3   - 
  //    RAW   P4    P5    P6   P7      (2019-04 add P7)
  if (c1!='P' || c2 <'1' || c2 >'7') { 
    fprintf(stderr,"\nread-PNM-error: file number is %2d,"
                   " position %ld", fileno(f1), ftell(f1));
    fprintf(stderr,"\nread-PNM-error: bad magic bytes, expect 0x50 0x3[1-7]"
                   " but got 0x%02x 0x%02x", 255&c1, 255&c2);
    if (f1) fclose(f1); f1=NULL; return(-1);
  }
  /* use pnmtoplainpnm to convert from regular PBM to Plain PBM = ASCII */
  pam_token[0]=0; tupletype[0]=0; /* ini P7 string buffers */
  nx=ny=nc=0;i=0; if (c2=='4' || c2=='1') nc=1; /* PBM nc=num_colors=1 */
  for(mod=0;((c2=='5' || c2=='2') && (mod&7)<6) /* PGM 3spaces+3args */
        ||  ((c2=='6' || c2=='3') && (mod&7)<6) /* PPM 3spaces+3args */
        ||  ((c2=='4' || c2=='1') && (mod&7)<4) /* PBM 2spaces+2args */
        ||   (c2=='7');)                        /* PAM header strings */
  {						// mode: 0,2,4=[ \t\r\n] 
  						//   1=nx 3=ny 5=nc 8-13=#rem
    c1=read_char(f1); // filter out #-comments inclusive 1st \n
    // en.wikipedia.org/wiki/Netpbm_format, 2019-04-06 add comments '#'
    //  isspace = [\ \f\n\r\t\v]
// if (vvv) fprintf(stderr,"#DBG c1=%c mod=%d numb=%d i=%d\n",c1,mod,number,i);
    if( (mod & 1)==0 )				// scan whitespaces
      if( !isspace(c1) ) { mod++; number=0; i=0; }  //  1st non-space-char
    if( (mod & 1)==1 ) {                        // scan numbers or P7 token
      if(c2=='7' && c1>='A' && c1<='Z' && i<9 && (mod&2)){ // scan tupletype
        tupletype[i]=c1; i++;
        tupletype[i]=0;
      } else
      if(c2=='7' && c1>='A' && c1<='Z' && i<8){       // scan PAM-token
        pam_token[i]=c1; i++;
        pam_token[i]=0;
      } else  /* scan decimal number */
      if( !isdigit(c1) ) {           //  1st space after diggit or token 
        if(!isspace(c1) )F2("unexpected character P%c mode=%d", c2, mod);
        if (c2=='7' &&  /* 2019-04 JS added P7 header */
          strcmp(pam_token,"ENDHDR"  )==0) break; /* the only way out */
        if (c2=='7' && (mod&2)==2) {  /* 2019-04 JS added P7 header */
          if(vvv)
            fprintf(stderr,"#DBG.PAM %s = %d  mod=%d\n",
              pam_token, number, mod);
          if (strcmp(pam_token,"ENDHDR"  )==0) break; /* the only way out */
          if (strcmp(pam_token,"WIDTH"   )==0) nx=number;
          if (strcmp(pam_token,"HEIGHT"  )==0) ny=number;
          if (strcmp(pam_token,"DEPTH"   )==0) depth=number; /* planes or channels 1,3 */
          if (strcmp(pam_token,"MAXVAL"  )==0) nc=number; /* max65536 */
          if (strcmp(pam_token,"TUPLTYPE")==0) { /* GRAYSCALE,RGB */
            if (vvv) fprintf(stderr,"#DBG.PAM %s = %s\n",pam_token,tupletype);
            if (strcmp(tupletype,"GRAYSCALE")==0) ; // ToDo like PGM? depth=1?
            if (strcmp(tupletype,"RGB")==0) ;       // ToDo like PPM? depth=3?
          }
        }      // PAM
        else { // PNM
          if(mod==1) nx=number;
          if(mod==3) ny=number;
          if(mod==5) nc=number;
        }
        mod++;
      } //  1st non-diggit
      else { /* add next digit tu number */
        if (((unsigned)(number*10))/10 != number) F0("int overflow");
        number= number*10+c1-'0';
      }
    } /* number read modus */
  }
  if(vvv)
   fprintf(stderr,"# PNM P%c h*w=%d*%d c=%d head=%ld",c2,ny,nx,nc,ftell(f1));
  if( c2=='4' && (nx&7)!=0 ){
    /* nx=(nx+7)&~7;*/ if(vvv)fprintf(stderr," PBM2PGM nx %d",(nx+7)&~7);
  }
  if (nc>> 8) bps=2; // bytes per color and pixel
  if (nc>>16) bps=3; // 24bit per sample (single color value R or G or B)
  if (nc>>24) bps=4; // 32bit per sample
  if ( c2=='4' || c2=='1' ) depth=1; /* Black on White = 1 channel */
  if ( c2=='5' || c2=='2' ) depth=1; /* GRAY = 1 channel */
  if ( c2=='6' || c2=='3' ) depth=3; /* (R + G + B) * bps */
  if (depth*bps > sizeof(buf)) F0("Byte/pixel overflow");
  fflush(stdout);
  if ( (nx*ny)!=((1.*nx)*ny) || nx<=0 || ny<=0) F0("integer overflow");
  pic=(unsigned char *)malloc( nx*ny );
  if(pic==NULL)F0("memory failed");			// no memory
  for (i=0;i<nx*ny;i++) pic[i]=255; // init to white if reading fails 
  /* this is a slow but short routine for P1 to P6 formats */
  // we want to normalize brightness to 0..255
  // JS1904 simplified PGM/PPM/PAM code
  if (c2=='2' || c2=='3' || (c2>='5' && c2<='7')) { // PGM/PPM/PAM-RAW/ASC
    for (i=0;i<nx*ny;i++) {  // read single pixels (slow IO)
      if (c2>='5' && c2<='7') {
        // 2018-10 Endianess.NetPBM=MSB=H,L (see PBM-PLAIN) changed from L,H
        if (depth*bps!=(int)fread(buf,1,depth*bps,f1)){  // PPM-RAW
         fprintf(stderr," ERROR reading byte %d*%d*%d\n", depth, bps, i);
         exit(1); /* break;? */ } } // for i
      else for (j=0;j<depth;j++) fread_num(buf+j*bps, bps, f1); // PPM-PLAIN
      if (depth<3) pic[i]=buf[0]; /* JS1903 PPM+PAM */
      else pic[i]
          = ((PPM_RED_WEIGHT   * (unsigned char)buf[  bps-1] + 511)>>10)
          + ((PPM_GREEN_WEIGHT * (unsigned char)buf[2*bps-1] + 511)>>10)
          + ((PPM_BLUE_WEIGHT  * (unsigned char)buf[3*bps-1] + 511)>>10);
      /* normalized to 0..255 */
    }
  }
  if( c2=='1' )  // PBM-PLAIN
    for(mod=j=i=0,nc=255;i<nx*ny && !feof(f1);){ // PBM-ASCII 0001100
    c1=read_char(f1);
    if( isdigit(c1) ) { pic[i]=((c1=='0')?255:0); i++; }
    else if( !isspace(c1) )F0("unexpected char");
  }
  if( c2=='4' ){ // PBM-RAW
    dx=(nx+7)&~7;				// dx
    if(ny!=(int)fread(pic,dx>>3,ny,f1))F0("read");	// read all bytes
    for(ly=ny-1;ly>=0;ly--)
    for(lx=nx-1;lx>=0;lx--)
    pic[lx+ly*nx]=( (128 & (pic[(lx+ly*dx)>>3]<<(lx & 7))) ? 0 : 255 );
    nc=255;
  }
  {
    int minc=255, maxc=0;
    for (i=0;i<nx*ny;i++) {
      if (pic[i]>maxc) maxc=pic[i];
      if (pic[i]<minc) minc=pic[i];
    }
    if (vvv) fprintf(stderr," min=%d max=%d", minc, maxc);
  }
  p->p=pic;  p->x=nx;  p->y=ny; p->bpp=1;
  if (vvv) fprintf(stderr,"\n");  
  c1=0; c1=fgetc(f1); /* needed to trigger feof() */
  while ((!feof(f1)) && (c1==' ' || c1=='\n' || c1=='\r' || c1=='\t'))
    c1=fgetc(f1); /* JS 2019-04 allow spaces until next PNM P[1-7] */
  /* reference: pamfile -allimages -comments jocr/examples/test02.pbm */
  if (feof(f1) || c1!='P') {  /* EOF ^Z or not 'P' -> single image */
    if (vvv && feof(f1)) fprintf(stderr,"# PNM EOF\n");
    if (vvv && (!feof(f1)) && c1!='P' )
       fprintf(stderr,"# PNM unexpected char 0x%02x\n",(int)c1);
    if(name[0]!='-' || name[1]!=0){ /* do not close stdin */
      if(!pip) fclose(f1);
#ifdef HAVE_POPEN
      else     pclose(f1);	/* close pipe (Jul00) */
#endif
    } // stdin
    f1=NULL;  /* set file is closed flag */
    return 0;
  }
  if (!feof(f1) && c1=='P' && vvv) fprintf(stderr,"# PNM multi-image\n"); 
  return 1; /* multiple image = concatenated pnm's */
}
#endif /* HAVE_PAM_H */

int writepgm(char *nam,pix *p){// P5 raw-pgm
  FILE *f1;int a,x,y;
  f1=fopen(nam,"wb");if(!f1)F0("open");		// open-error
  fprintf(f1,"P5\n%d %d\n255\n",p->x,p->y);
  if(p->bpp==3)
  for(y=0;y<p->y;y++)
  for(x=0;x<p->x;x++){	// set bit
    a=x+y*p->x;
    p->p[a]=(p->p[3*a+0]+p->p[3*a+1]+p->p[3*a+2])/3;
  }
  if(p->y!=(int)fwrite(p->p,p->x,p->y,f1))F0("write");	// write all lines
  fclose(f1);
  return 0;
}

/* adding colours sr=red, sg=green, sb=blue, care about range */
void addrgb(unsigned char rgb[3], int sr, int sg, int sb) {
 int add[3], i;
 /* add colour on dark pixels 0+.., subtract on white pixels 255-.. */ 
 add[0]=2*sr; add[1]=2*sg; add[2]=2*sb;
 if (((int)rgb[0])+((int)rgb[1])+((int)rgb[2])>=3*160)
    { add[0]=(-sg-sb); add[1]=(-sr-sb); add[2]=(-sr-sg); } // rgb/2?
 /* care about colour range */
 for (i=0;i<3;i++) /* avoid overflow */
   if (add[i]<0) rgb[i]=((rgb[i]+add[i]<0  )?  0:rgb[i]+add[i]);
   else          rgb[i]=((rgb[i]+add[i]>255)?255:rgb[i]+add[i]);
}
/*
 * pgmtoppm or pnmtopng, use last 3 bits for farbcoding 
 * replaces old writebmp variant
 *  2018-10 add $opt to control coloring
 */
int writeppm(char *nam, pix *p, int opt){ /* P6 raw-ppm */
  FILE *f1=NULL; int x,y,f1t=0; unsigned char rgb[3], gray, bits;
  char buf[128];
  if (strchr(nam,'|')) return -1;  /* no nasty code */
  if (strstr(nam,".ppm")) { f1=fopen(nam,"wb"); }
#ifdef HAVE_POPEN
  /* be sure that nam contains hacker code like "dummy | rm -rf *" */
  if (!f1) {
    strncpy(buf,"pnmtopng > ",12);  /* no spaces within filenames allowed! */
    strncpy(buf+11,nam,111); buf[123]=0;
    strncpy(buf+strlen(buf),".png",5);
    /* we dont care about win "wb" here, never debug on win systems */
    f1 = popen(buf, "w"); if(f1) f1t=1; else E0("popen pnmtopng");
  }
  if (!f1) {
    strncpy(buf,"gzip -c > ",11);
    strncpy(buf+10,nam,109); buf[120]=0;
    strncpy(buf+strlen(buf),".ppm.gz",8); 
    /* we dont care about win "wb" here, never debug on win systems */
    f1 = popen(buf, "w"); if(f1) f1t=1; else E0("popen gzip -c");
  }
#endif
  if (!f1) {
    strncpy(buf,nam,113); buf[114]=0;
    strncpy(buf+strlen(buf),".ppm",5);
    f1=fopen(buf,"wb");
  }
  if (!f1) F0("open"); /* open-error */
  fprintf(f1,"P6\n%d %d\n255\n",p->x,p->y);
  if ( p->bpp==1 )
  for (y=0;y<p->y;y++)
  for (x=0;x<p->x;x++){
    gray=p->p[x+y*p->x];
    bits=(gray&0x0F);  /* save 4 marker bits */
    /* replace used bits to get max. contrast, 160=0xA0 */
    if (opt&7) gray = ((gray<160) ? (gray&~0x0F)>>1 : 0xC3|(gray>>1) );
    rgb[0] = rgb[1] = rgb[2] = gray; /* add red,grn,blue */
    if (opt&7) {
      if ((bits &  1)==1) { addrgb(rgb,0,0,8+8*((x+y)&1)); } /* dark blue */
      if ((bits &  8)==8) { addrgb(rgb,0,0, 16); } /* blue (low priority) */
      if ((bits &  6)==6) { addrgb(rgb,0,0, 32); } /* blue */
      if ((bits &  6)==4) { addrgb(rgb,0,48,0); } /* green */
      if ((bits &  6)==2) { addrgb(rgb,32,0,0); } /* red */
    }
    if ( 1!=(int)fwrite(rgb,3,1,f1) ) { E0("write"); y=p->y; break; }
  }
  if ( p->bpp==3 )
  if ( p->y!=(int)fwrite(p->p,3*p->x,p->y,f1) ) E0("write");
#ifdef HAVE_POPEN
  if (f1t) { pclose (f1); f1=NULL; }
#endif
  if (f1) fclose(f1);
  return 0;
}

// high bit = first, 
int writepbm(char *nam,pix *p){// P4 raw-pbm
  FILE *f1;int x,y,a,b,dx,i;
  dx=(p->x+7)&~7;	// enlarge to a factor of 8
  for(y=0;y<p->y;y++)
  for(x=0;x<p->x;x++){	// set bit
    a=(x+y*dx)>>3;b=7-(x&7);	// adress an bitisnumber
    i=x+y*p->x;
    if(p->bpp==3) i=(p->p[3*i+0]+p->p[3*i+1]+p->p[3*i+2])/3;
    else          i= p->p[  i  ];
    i=((i>127)?0:1);
    p->p[a]=(p->p[a] & (~1<<b)) | (i<<b);
  }
  f1=fopen(nam,"wb");if(!f1)F0("open");		// open-error
  fprintf(f1,"P4\n%d %d\n",p->x,p->y);
  if(p->y!=(int)fwrite(p->p,dx>>3,p->y,f1))F0("write");	// write all lines
  fclose(f1);
  return 0;
}
// ------------------------------------------------------------------------
