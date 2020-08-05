/*
This is a Optical-Character-Recognition program
Copyright (C) 2000-2017 Joerg Schulenburg License: GPL2

*/
#ifndef _OCR0_DBG_H
#define _OCR0_DBG_H

//#include "gocr.h"
//#include "unicode_defs.h"

#define IFV if(OCR_JOB->cfg.verbose&4)
#define MM {IFV fprintf(stderr,"\nDBG %c L%04d (%d,%d): ",\
            (char)c_ask,__LINE__,box1->x0,box1->y0);}

// new debug mode (0.41) explains why char is declined or accepted as ABC...
//   the output can be filtered by external scripts
//   ToDo: we could reduce output to filter string, +move to ocr0dbg.h
#ifndef DO_DEBUG       /* can be defined outside (configure --with-debug) */
#define DO_DEBUG 0     /* 0 is the default, 1 is for debug+developping */
#endif

/* this macro is for debugging output: "if char is declined, why?" */
#if DO_DEBUG   /* 0=Work mode, 1=debugging mode */
// Setac: output, that char is choosen with a probability
// Break: output, why the char is not choosen
// MSG: debugging functions for char C_ASK, mostly messages
// DBG: definitions usefull only for debugging
#define Setac(box1,ac,ad)  { MM;IFV fprintf(stderr,"setac %c %d",ac,ad);\
                             setac(box1,ac,ad); }
#define Break              { MM;IFV fprintf(stderr,"break"); break; }
#define MSG(x)             { MM;IFV x }
#define DBG(x) x
#else
#define Setac(box1,ac,ad)  setac(box1,ac,ad)
#define Break break
#define MSG(x)
#define DBG(x)
#endif

#endif
