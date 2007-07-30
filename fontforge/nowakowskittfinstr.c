/* Copyright (C) 2000-2007 by 
   George Williams, Michal Nowakowski & Alexey Kryukov */

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "pfaeditui.h"
#include <math.h>
#include <utype.h>

#include "ttf.h"
#include "splinefont.h"

/* define some often used instructions */
#define SVTCA_y                 (0x00)
#define SVTCA_x                 (0x01)
#define SRP0                    (0x10)
#define SRP1                    (0x11)
#define SRP2                    (0x12)
#define SZP0                    (0x13)
#define RTG                     (0x18)
#define CALL			(0x2b)
#define MDAP                    (0x2e)
#define MDAP_rnd                (0x2f)
#define IUP_y                   (0x30)
#define IUP_x                   (0x31)
#define SHP_rp2                 (0x32)
#define SHP_rp1                 (0x33)
#define MIAP                    (0x3e)
#define MIAP_rnd                (0x3f)
#define RDTG                    (0x7d)
#define MDRP_rnd_grey           (0xc4)
#define MDRP_min_rnd_black      (0xcd)
#define MDRP_min_rnd_white      (0xce)
#define MDRP_rp0_rnd_white      (0xd6)
#define MDRP_rp0_min_rnd_black  (0xdd)
#define MDRP_rp0_min_rnd_white  (0xde)
#define MIRP_rnd_grey           (0xe4)
#define MIRP_min_rnd_black      (0xed)
#define MIRP_rp0_rnd_grey       (0xf4)
#define MIRP_rp0_min_rnd_black  (0xfd)

struct glyphinstrs {
    SplineFont *sf;
    BlueData *bd;
    int fudge;
};

typedef struct instrct {
    SplineChar *sc;
    SplineSet *ss;
    struct glyphinstrs *gi;  /* finally, I'll bring its members here */
    int ptcnt;
    int *contourends;
    BasePoint *bp;
    uint8 *touched;       /* these points got instructed */
    uint8 *affected;      /* almost touched, but optimized out */
    uint8 *instrs;        /* the beginning of the instructions */
    uint8 *pt;            /* the current position in the instructions */

  /* stuff for hinting edges of stems and blues: */
    int xdir;             /* direction flag: x=true, y=false */
    int cdir;             /* is current contour clockwise (outer)? */
    struct __edge {
	real base;        /* where the edge is */
	real end;         /* the second edge - currently only for blue snapping */
	int refpt;        /* best ref. point for this base, ttf index, -1 if none */
	int refscore;     /* quality of basept, for searching better one, 0 if none */
	int othercnt;     /* count of other points worth instructing for this edge */
	int *others;      /* ttf indexes of these points */
    } edge;
} InstrCt;

extern int autohint_before_generate;

#if 0		/* in getttfinstrs.c */
struct ttf_table *SFFindTable(SplineFont *sf,uint32 tag) {
    struct ttf_table *tab;

    for ( tab=sf->ttf_tables; tab!=NULL && tab->tag!=tag; tab=tab->next );
return( tab );
}
#endif

/* We'll need at least three stack levels and a twilight point (and thus also
 * a twilight zone). We also currently define one function in fpgm. We must
 * ensure this is indicated in 'maxp' table. Note: we'll surely need more
 * stack levels in the future. Twilight point count may vary depending on
 * hinting method; for now, one is enough.
 *
 * TODO! I don't know why, but FF sometimes won't set the stack depth.
 */
static void init_maxp(InstrCt *ct) {
    struct ttf_table *tab = SFFindTable(ct->sc->parent,CHR('m','a','x','p'));

    if ( tab==NULL ) {
	tab = chunkalloc(sizeof(struct ttf_table));
	tab->next = ct->sc->parent->ttf_tables;
	ct->sc->parent->ttf_tables = tab;
	tab->tag = CHR('m','a','x','p');
    }

    if ( tab->len<32 ) {
	tab->data = grealloc(tab->data,32);
	memset(tab->data+tab->len,0,32-tab->len);
	tab->len = tab->maxlen = 32;
    }

    uint16 zones = memushort(tab->data, 32,  7*sizeof(uint16));
    uint16 twpts = memushort(tab->data, 32,  8*sizeof(uint16));
    uint16 fdefs = memushort(tab->data, 32, 10*sizeof(uint16));
    uint16 stack = memushort(tab->data, 32, 12*sizeof(uint16));

    if (zones<2) zones=2;
    if (twpts<1) twpts=1;
    if (fdefs<1) fdefs=1;
    /* A TEMPORARY fix */
    if (stack<256) stack=256; /* we'll surely need more in future */

    memputshort(tab->data, 7*sizeof(uint16), zones);
    memputshort(tab->data, 8*sizeof(uint16), twpts);
    memputshort(tab->data,10*sizeof(uint16), fdefs);
    memputshort(tab->data,12*sizeof(uint16), stack);
}

/* Turning dropout control on will dramatically improve mono rendering, even
 * without further hinting, especcialy for light typefaces. And turning hinting
 * off at veeery small pixel sizes is required, because hints tend to visually
 * tear outlines apart when not having enough workspace.
 */
static void init_prep(InstrCt *ct) {
    uint8 new_prep[] =
    {
	0x4b, // MPPEM
	0xb0, // PUSHB_1
	0x07, //   7 - hinting threshold - should be configurable
	0x50, // LT
	0x58, // IF
	0xb1, //   PUSHB_2
	0x01, //     1
	0x01, //     1
	0x8e, //   INSTCTRL
	0x59, // EIF
	0xb8, // PUSHW_1
	0x01, //   511
	0xff, //   ...still that 511
	0x85, // SCANCTRL
	0xb0, // PUSHB_1
	0x01, //   70/64 = about 1.094 pixel
	0x1d, // SCVTCI
	0x4b, // MPPEM
	0xb0, // PUSHB_1
	0x32, //   50 PPEM - a threshold below which we'll use larger CVT cut-in
	0x52, // GT
	0x58, // IF
	0xb0, // PUSHB_1
	0x01, //   128/64 = 2 pixels
	0x1d, // SCVTCI
	0x59  // EIF
    };

    int alert;
    struct ttf_table *tab = SFFindTable(ct->sc->parent,CHR('p','r','e','p'));

    if ( tab==NULL || (tab->len==0) ) {
	tab = chunkalloc(sizeof(struct ttf_table));
	tab->next = ct->sc->parent->ttf_tables;
	ct->sc->parent->ttf_tables = tab;
	tab->tag = CHR('p','r','e','p');

	tab->len = tab->maxlen = sizeof(new_prep);
	tab->data = grealloc(tab->data, sizeof(new_prep));
	memmove(tab->data, new_prep, sizeof(new_prep));
    }
    else {
	/* there already is a font program. */
	alert = 1;
	if (tab->len >= sizeof(new_prep))
	    if (!memcmp(tab->data, new_prep, sizeof(new_prep)))
		alert = 0;  /* it's ours, perhaps with user's extensions. */

	/* Log warning message. */
	if (alert) {
	    LogError(_("Can't insert 'prep'"),
		_("There exists a 'prep' code incompatible with FontForge's. "
		  "It can't be guaranteed it will work well. It is suggested "
		  "to allow FontForge to insert its code and then append user's"
		  "own."
		 ));
	}

    }
}

/* Other hinting software puts certain actions in FPGM to ease developer's life
 * and compress the code. I feel that having a 'standard' library of functions
 * could also help FF users.
 *
 * Caution! This code is heavily relied by autohinting. Any other code should
 * be placed below it. It's good to first clear font's hinting tables, then
 * autohint it, and then insert user's own code and do the manual hinting of
 * glyphs that do need it.
 */
static void init_fpgm(InstrCt *ct) {
    uint8 new_fpgm[] = 
    {
	/* Function 0: position a point within a blue zone (given via cvt).
	 * Syntax: PUSHB_3 point cvt_of_blue 0 CALL
	 */
	0xb0, // PUSHB_1
	0x00, //   0
	0x2c, // FDEF
	0xb0, //   PUSHB_1
	0x00, //     0
	0x13, //   SZP0
	0xb0, //   PUSHB_1
	0x00, //     0
	0x23, //   SWAP
	0x7c, //   RUTG
	0x3f, //   MIAP[rnd]
	0x7d, //   RDTG
	0x20, //   DUP
	0xd4, //   MDRP[rp0,rnd,grey]
	0xb0, //   PUSHB_1
	0x01, //     1
	0x13, //   SZP0
	0x2e, //   MDAP[no-rnd]
	0x18, //   RTG
	0x2d  // ENDF
    };

    int alert;
    struct ttf_table *tab = SFFindTable(ct->sc->parent,CHR('f','p','g','m'));

    if ( tab==NULL || (tab->len==0) ) {
	/* We can safely update font program. */
	tab = chunkalloc(sizeof(struct ttf_table));
	tab->next = ct->sc->parent->ttf_tables;
	ct->sc->parent->ttf_tables = tab;
	tab->tag = CHR('f','p','g','m');

	tab->len = tab->maxlen = sizeof(new_fpgm);
	tab->data = grealloc(tab->data, sizeof(new_fpgm));
	memmove(tab->data, new_fpgm, sizeof(new_fpgm));
    }
    else {
	/* there already is a font program. */
	alert = 1;
	if (tab->len >= sizeof(new_fpgm))
	    if (!memcmp(tab->data, new_fpgm, sizeof(new_fpgm)))
		alert = 0;  /* it's ours, perhaps with user's extensions. */

	/* Log warning message. */
	if (alert) {
	    ff_post_notice(_("Can't insert 'fpgm'"),
		_("There exists an 'fpgm' that seems incompatible with FontForge's. "
		  "Instructions generated will behave wrong. Please clear the `fpgm` "
		  "so that FontForge could insert his code there on next instructing."
		  "It will be then possible to append user's code to FontForge's, "
		  "but using high function numbers is extremely advised due to "
		  "possible updates.\n"
		 ));
	}
    }
}

#if 0		/* in getttfinstrs.c */
int TTF__getcvtval(SplineFont *sf,int val) {
    int i;
    struct ttf_table *cvt_tab = SFFindTable(sf,CHR('c','v','t',' '));

    if ( cvt_tab==NULL ) {
	cvt_tab = chunkalloc(sizeof(struct ttf_table));
	cvt_tab->tag = CHR('c','v','t',' ');
	cvt_tab->maxlen = 200;
	cvt_tab->data = galloc(100*sizeof(short));
	cvt_tab->next = sf->ttf_tables;
	sf->ttf_tables = cvt_tab;
    }
    for ( i=0; sizeof(uint16)*i<cvt_tab->len; ++i ) {
	int tval = (int16) memushort(cvt_tab->data,cvt_tab->len, sizeof(uint16)*i);
	if ( val>=tval-1 && val<=tval+1 )
return( i );
    }
    if ( sizeof(uint16)*i>=cvt_tab->maxlen ) {
	if ( cvt_tab->maxlen==0 ) cvt_tab->maxlen = cvt_tab->len;
	cvt_tab->maxlen += 200;
	cvt_tab->data = grealloc(cvt_tab->data,cvt_tab->maxlen);
    }
    memputshort(cvt_tab->data,sizeof(uint16)*i,val);
    cvt_tab->len += sizeof(uint16);
return( i );
}

int TTF_getcvtval(SplineFont *sf,int val) {

    /* by default sign is unimportant in the cvt */
    /* For some instructions anyway, but not for MIAP so this routine has */
    /*  been broken in two. */
    if ( val<0 ) val = -val;
return( TTF__getcvtval(sf,val));
}
#endif

static int _CVT_SeekInPrivateString(SplineFont *sf, char *str, double value, double fudge) {
    char *end;
    double d;

    if ( str==NULL )
return -1;
    while ( *str ) {
	while ( !isdigit(*str) && *str!='-' && *str!='+' && *str!='.' && *str!='\0' )
	    ++str;
	if ( *str=='\0' )
    break;
	d = strtod(str,&end);
	if ( d>=-32768 && d<=32767 ) {
	    int v = rint(d);

	    if (fabs(d - value) <= fudge)
		return TTF__getcvtval(sf,v);
	}
	str = end;
    }

    return -1;
}

#if 0		/* in getttfinstrs.c */
static void _CVT_ImportPrivateString(SplineFont *sf,char *str) {
    char *end;
    double d;

    if ( str==NULL )
return;
    while ( *str ) {
	while ( !isdigit(*str) && *str!='-' && *str!='+' && *str!='.' && *str!='\0' )
	    ++str;
	if ( *str=='\0' )
    break;
	d = strtod(str,&end);
	if ( d>=-32768 && d<=32767 ) {
	    int v = rint(d);
	    TTF__getcvtval(sf,v);
	}
	str = end;
    }
}

void CVT_ImportPrivate(SplineFont *sf) {
    if ( sf->private==NULL )
return;
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"StdHW"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"StdVW"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"StemSnapH"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"StemSnapV"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"BlueValues"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"OtherBlues"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"FamilyBlues"));
    _CVT_ImportPrivateString(sf,PSDictHasEntry(sf->private,"FamilyOtherBlues"));
}
#endif

static uint8 *pushheader(uint8 *instrs, int isword, int tot) {
    if ( isword ) {
	if ( tot>8 ) {
	    *instrs++ = 0x41;		/* N(next byte) Push words */
	    *instrs++ = tot;
	} else
	    *instrs++ = 0xb8+(tot-1);	/* Push Words */
    } else {
	if ( tot>8 ) {
	    *instrs++ = 0x40;		/* N(next byte) Push bytes */
	    *instrs++ = tot;
	} else
	    *instrs++ = 0xb0+(tot-1);	/* Push bytes */
    }
return( instrs );
}

static uint8 *addpoint(uint8 *instrs,int isword,int pt) {
    if ( !isword ) {
	*instrs++ = pt;
    } else {
	*instrs++ = pt>>8;
	*instrs++ = pt&0xff;
    }
return( instrs );
}

static uint8 *pushpoint(uint8 *instrs,int pt) {
    instrs = pushheader(instrs,pt>255,1);
return( addpoint(instrs,pt>255,pt));
}

static uint8 *pushpointstem(uint8 *instrs,int pt, int stem) {
    int isword = pt>255 || stem>255;
    instrs = pushheader(instrs,isword,2);
    instrs = addpoint(instrs,isword,pt);
return( addpoint(instrs,isword,stem));
}

static uint8 *pushpoints(uint8 *instrs, int ptcnt, const int *pts) {
    int i, isword = 0;
    for (i=0; i<ptcnt; i++) if (pts[i]>255) isword=1;
    if (ptcnt > 255) isword = 1; /* or use several NPUSHB if all are bytes */
    instrs = pushheader(instrs,isword,ptcnt);
    for (i=0; i<ptcnt; i++) instrs = addpoint(instrs, isword, pts[i]);
return( instrs );
}

/* Find previous point index on the contour. */
static int PrevOnContour(int *contourends, BasePoint *bp, int p) {
    int i;

    if (p == 0) return contourends[0];
    else {
	for (i=0; contourends[i+1]; i++)
	    if (contourends[i]+1 == p)
		return contourends[i+1];

	return p-1;
    }
}

/* Find next point index on the contour. */
static int NextOnContour(int *contourends, BasePoint *bp, int p) {
    int i;

    if (p == 0) return 1;
    else {
	for (i=0; contourends[i]; i++) {
	    if (contourends[i] == p) {
		if (i==0) return 0;
		else return contourends[i-1]+1;
	    }
	}
	return p+1;
    }
}

/* For hinting stems, I found it needed to check if candidate point for
 * instructing is pararell with a hint to avoid snapping wrong points.
 * I splitted the routine into two, as sometimes it may be needed to check
 * the angle to be strictly almost the same, not just pararell.
 */
static int __same_angle(int *contourends, BasePoint *bp, int p, double angle) {
    int PrevPoint, NextPoint;
    double PrevTangent, NextTangent;

    PrevPoint = PrevOnContour(contourends, bp, p);
    NextPoint = NextOnContour(contourends, bp, p);

    PrevTangent = atan2(bp[p].y - bp[PrevPoint].y, bp[p].x - bp[PrevPoint].x);
    NextTangent = atan2(bp[NextPoint].y - bp[p].y, bp[NextPoint].x - bp[p].x);

    /* If at least one of the tangents is close to the given angle, return true. */
    /* 'Close' means about 5 deg, i.e. about 0.087 rad. */
    PrevTangent = fabs(PrevTangent-angle);
    NextTangent = fabs(NextTangent-angle);
    while (PrevTangent > M_PI) PrevTangent -= 2*M_PI;
    while (NextTangent > M_PI) NextTangent -= 2*M_PI;
return (fabs(PrevTangent) <= 0.087) || (fabs(NextTangent) <= 0.087);
}

static int same_angle(int *contourends, BasePoint *bp, int p, double angle) {
return __same_angle(contourends, bp, p, angle) || __same_angle(contourends, bp, p, angle+M_PI);
}

/* I found it needed to write some simple functions to determine whether
 * a spline point is a vertical/horizontal extremum. I looked at the code
 * responsible for displaying a charview and found that FF also treats an
 * inflection point of a curve as an extremum. I'm not sure if this is OK,
 * but I retain this for consistency.
 */
static int IsXExtremum(SplinePoint *sp) {
return (!sp->nonextcp && !sp->noprevcp && sp->nextcp.x==sp->me.x && sp->prevcp.x==sp->me.x);
}

static int IsYExtremum(SplinePoint *sp) {
return (!sp->nonextcp && !sp->noprevcp && sp->nextcp.y==sp->me.y && sp->prevcp.y==sp->me.y);
}

/* Other interesting points are corners. I'll name two types of them.
 * An acute corner is one with angle between its tangents less than 90 deg.
 * I won't distinct X and Y tight corners, as they usually need hinting in
 * both directions.
 * An obtuse corner is one with angle between its tangents close to 180 deg.
 * I'll distinct X and Y flat corners, as they usually need hinting only
 * in one direction.
 */
static int IsAcuteCorner(int *contourends, BasePoint *bp, int p) {
return 0;
}

static int IsXObtuseCorner(int *contourends, BasePoint *bp, int p) {
    int PrevPoint = PrevOnContour(contourends, bp, p);
    int NextPoint = NextOnContour(contourends, bp, p);

return ((bp[PrevPoint].x > bp[p].x) && (bp[NextPoint].x > bp[p].x)) ||
       ((bp[PrevPoint].x < bp[p].x) && (bp[NextPoint].x < bp[p].x));
}

static int IsYObtuseCorner(int *contourends, BasePoint *bp, int p) {
    int PrevPoint = PrevOnContour(contourends, bp, p);
    int NextPoint = NextOnContour(contourends, bp, p);

return ((bp[PrevPoint].y > bp[p].y) && (bp[NextPoint].y > bp[p].y)) ||
       ((bp[PrevPoint].y < bp[p].y) && (bp[NextPoint].y < bp[p].y));
}

/* I found it easier to write an iterator that calls given function for each
 * point worth instructing than repeating the same loops all the time. Each
 * function passed to the iterator may need different set of arguments. So I
 * decided to make a container for them to leave argument lists reasonably short.
 * The control points are normally skipped, as instructing them seems to cause
 * more damages than profits. An exception is made for interpolated extrema:
 * as they can't be directly instructed, their control points are used.
 * The iterator won't run a function twice on the same point in any case.
 *
 * The contour_direction option is for blues - snapping internal contour to a
 * blue zone is plain wrong.
 *
 * TODO! If there is an edge consisting of on-curve points interlaced with off-
 * curve points, only on-curve points will be added to the edge, and thus
 * optimize_edge won't be able to do the job well. It seems that introduction
 * of edge optimizer made skipping on-curve points obsolete. I'll turn this
 * off, but this may require a slight rewrite of other functions that don't
 * rely on edge optimizer. Also, the optimizer should be modified not to leave
 * off-curve points whenever possible.
 */
#define EXTERNAL_CONTOURS 0
#define ALL_CONTOURS 1
#define INTERNAL_CONTOURS 2
static void RunOnPoints(InstrCt *ct, int contour_direction,
    void (*runme)(int p, SplinePoint *sp, InstrCt *ct))
{
    SplineSet *ss = ct->ss;
    SplinePoint *sp;
    uint8 *done;
    int p;

    done = (uint8 *)gcalloc(ct->ptcnt, sizeof(uint8));

    for ( ; ss!=NULL; ss=ss->next ) {
	ct->cdir = SplinePointListIsClockwise(ss);

	if (((contour_direction == EXTERNAL_CONTOURS) && !ct->cdir) ||
	    ((contour_direction == INTERNAL_CONTOURS) && ct->cdir)) continue;

	for ( sp=ss->first; ; ) {
	    if (sp->ttfindex == 0xffff) {
		if (ct->xdir?IsXExtremum(sp):IsYExtremum(sp)) {
		    if (!done[p = PrevOnContour(ct->contourends, ct->bp, sp->nextcpindex)]) {
			runme(p, sp, ct);
			done[p] = true;
		    }
		    /* this section seems unneeded - one point is enough */
		    //if (!done[p = sp->nextcpindex]) {
		    //    runme(p, sp, ct);
		    //    done[p] = true;
		    //}
		}
	    }
	    else if (!done[p = sp->ttfindex]) {
		runme(p, sp, ct);
		done[p] = true;
	    }

	    if ( sp->next==NULL ) break;
	    sp = sp->next->to;
	    if ( sp==ss->first ) break;
	}
    }

    free(done);
}

/******************************************************************************
 *
 * Hinting is mostly aligning 'edges' (in FreeType's sense). Each stem hint
 * consists of two edges (or one, for ghost hints). And each blue zone can be
 * represented as an edge with extended fudge (overshoot).
 *
 * Hinting a stem edge is broken in two steps. First: init_edge() seeks for
 * points to snap and chooses one that will be used as a reference point - it
 * should be then instructed elsewhere (a general method of edge positioning).
 * Finally, finish_edge() instructs the rest of points found with given command:
 * SHP, SHPIX, IP, FLIPPT or ALIGNRP (in future the function may use SLOOP to
 * minimize the size of generated ttf code, so other opcodes mustn't be used).
 * It also tries not to instruct unneeded points on an edge, relying on IUP[].
 *
 * The contour_direction option of init_edge() is for hinting blues - snapping
 * internal contour to a bluezone seems just plainly wrong.
 *
 ******************************************************************************/

/* search for points to be snapped to an edge - to be used in RunOnPoints() */
static void search_edge(int p, SplinePoint *sp, InstrCt *ct) {
    int tmp, score = 0;
    real coord = ct->xdir?ct->bp[p].x:ct->bp[p].y;
    real fudge = ct->gi->fudge;
    uint8 touchflag = ct->xdir?tf_x:tf_y;

    if (fabs(coord - ct->edge.base) <= fudge) {
	if (same_angle(ct->contourends, ct->bp, p, ct->xdir?0.5*M_PI:0.0)) score++;
	if (ct->xdir?IsXExtremum(sp):IsYExtremum(sp)) score+=4;
	if (ct->xdir?IsXObtuseCorner(ct->contourends, ct->bp, p):
	             IsYObtuseCorner(ct->contourends, ct->bp, p)) score+=4;
	if (score && (sp->ttfindex != 0xffff)) score+=2;

	if (!score)
return;
	else if (ct->touched[p]) score+=8;

	if (score > ct->edge.refscore) {
	    tmp = ct->edge.refpt;
	    ct->edge.refpt = p;
	    ct->edge.refscore = score;
	    p = tmp;
	}

	if ((p!=-1) && !(ct->touched[p] & touchflag)) {
	    ct->edge.othercnt++;

	    if (ct->edge.othercnt==1) ct->edge.others=(int *)gcalloc(1, sizeof(int));
	    else ct->edge.others=(int *)grealloc(ct->edge.others, ct->edge.othercnt*sizeof(int));

	    ct->edge.others[ct->edge.othercnt-1] = p;
	}
    }
}

/* Initialize the InstrCt for instructing given edge.
 */
static void init_edge(InstrCt *ct, real base, int contour_direction) {
    ct->edge.base = base;
    ct->edge.refpt = -1;
    ct->edge.refscore = 0;
    ct->edge.othercnt = 0;
    ct->edge.others = NULL;
    RunOnPoints(ct, contour_direction, &search_edge);
}

/* An apparatus to optimize edge hinting. For given 'others' in ct, it detects
 * 'segments' (in FreeType's sense) and leaves only one point per segment.
 * A segment to which refpt belong is completely removed (refpt is enough).
 *
 * Optimizer is deliberately turned off if diagonal stems are present.
 *
 * TODO! This function won't remove obvious neighbors if there is a control
 * (off-curve) point between them, because RunOnPoints skips off-curve points
 * if possible. And if it is rewritten not to skip them (quite simple in fact),
 * the optimizer itself should be taught to do so itself if possible
 * (and that's somewhat trickier).
 
 * TODO! #2 If diagonal stems are present, optimizer should remove off-curve
 * points and nothing more. Or it can work as usual but preserving points
 * that will be used by diagonal stem hinter. We'll see.
 */
static int sortbynum(const void *a, const void *b) { return *(int *)a > *(int *)b; }

static void optimize_edge(InstrCt *ct) {
#define NextP(_p) NextOnContour(ct->contourends, ct->bp, _p)

    if ((ct->edge.othercnt == 0) || (ct->sc->dstem))
return;

    int i, next, leading_unneeded=0, final_unneeded=0;
    int *others = ct->edge.others;
    int othercnt = ct->edge.othercnt;
    int refpt = ct->edge.refpt;
    uint8 *affected = ct->affected;
    int touchflag = (ct->xdir)?tf_x:tf_y;

    qsort(others, othercnt, sizeof(int), sortbynum);

    if (((others[0] < refpt) && (NextP(others[0]) == others[othercnt-1])) ||
        (NextP(refpt) == others[0]))
	leading_unneeded = 1;

    if (NextP(others[othercnt-1]) == refpt) final_unneeded = 1;

    for (i=0; (i < othercnt-1) && (others[i] < refpt); i++) {
	next = NextP(others[i]);
	if ((next == others[i+1]) || (next == refpt))
	    affected[others[i]] |= touchflag;
    }

    for (i=othercnt-1; (i > 0) && (others[i] > refpt); i--)
	if ((NextP(others[i-1]) == others[i]) || (NextP(refpt) == others[i]))
	    affected[others[i]] |= touchflag;

    if (leading_unneeded) {
	for(i=0; (i < othercnt) && (others[i] == -1); i++);
	if (i < othercnt) affected[others[i]] |= touchflag;
    }

    if (final_unneeded) {
	for(i=othercnt-1; (i >= 0) && (others[i] == -1); i--);
	if (i >= 0) affected[others[i]] |= touchflag;
    }

    for (i=next=0; i<othercnt; i++)
	if (!(affected[others[i]] & touchflag))
	    others[next++] = others[i];

    ct->edge.othercnt = next;

    if (ct->edge.othercnt == 0) {
	free(ct->edge.others);
	ct->edge.others = NULL;
    }

#undef NextP
}

/* Finish instructing the edge. The 'command' must be
 * either of: SHP, SHPIX, IP, FLIPPT or ALIGNRP.

 * We try to hint only those points on edge that are necessary.
 * IUP will do the rest.

 * TODO! 
 * Possible strategies:
 *   - do point by point (currently used, poor space efficiency)
 *   - push all the stock at once (better, but has
 *     poor space efficiency in case of a word among several bytes).
 *   - push bytes and words separately
 *   - when the stuff is pushed, try to use SLOOP.
 */
static void finish_edge(InstrCt *ct, uint8 command) {

    optimize_edge(ct);
    if (ct->edge.othercnt==0)
return;

    int i;
    int othercnt = ct->edge.othercnt;
    int *others = ct->edge.others;

    /* This will need to signal maximum stack depth. */
//    ct->pt = pushpoints(ct->pt, othercnt, others);

    for(i=0; i<othercnt; i++) {
	ct->pt = pushpoint(ct->pt, others[i]); /* to be gone */
	*(ct->pt)++ = command;
	ct->touched[others[i]] |= (ct->xdir?tf_x:tf_y);
    }

    free(ct->edge.others);
    ct->edge.others=NULL;
    ct->edge.othercnt = 0;
}

/******************************************************************************
 * I decided to do snapping to blues at the very beginning of the instructing.
 *
 * Blues are processed in certain (important) order: baseline, descenders
 * (from deeper to shorter), ascenders (from taller to shorter).
 *
 * For each blue, one of the edges is put into CVT: lower if is't > zero,
 * the upper otherwise. A twilight point 0 is established at this height. All
 * the glyph's points decided to be worth snapping are then moved relative to
 * this twilight point, being subject to rounding 'down-to-int'. Space taken
 * is at most 8*ptcnt.
 *
 * For each blue, all yet unprocessed HStems affected are instructed. Ghost
 * hints are reckognised. Hinting other points is disabled here, as they should
 * be marked with ghost stems explicitly to be snapped to the bluezone.
 *
 * TODO! We have to check whether we have to instruct hints that overlaps with
 * those affected by blues.
 *
 * Important notes:
 *
 * The zone count must be set to 2, the twilight point count must be nonzero.
 * This is done automagically in init_maxp(), otherwise this method wouldn't
 * work at all. Currently there is only one twilight point used, but there
 * may be needed one or even two points per blue zone if some advanced snapping
 * and counter managing is to be done.
 *
 * Snapping relies on function 0 in FPGM, see init_fpgm().
 *
 * Using MIAP (single cvt, relying on cut-in) instead of twilight points
 * causes overshoots to appear/disappear inconsistently at small pixel sizes.
 * This flickering is disastrous to soft, wavy horizontal lines. We could use
 * any glyph's point at needed height, but we'r not certain we'll find any.
 *
 * The inner (leftwards) contours aren't snapped to the blue zone.
 * This could have created weird artifacts. Of course this will fail for
 * glyphs with wrong direction, but I won't handle it for now.
 *
 * TODO! Remind the user to correct direction or do it for him.
 * TODO! Try to do this with a single push and looped MDRP.
 *
 * If we didn't snapped any point to a blue zone, we shouldn't mark any HStem
 * edges done. This could made some important points on inner contours missed.
 *
 ******************************************************************************/

static void snap_to_blues(InstrCt *ct) {
    int i, cvt, cvt2;
    StemInfo *hint;          /* for HStems affected wit blues */
    real base, advance, tmp; /* for the hint */
    int callargs[4] = { 0/*pt*/, 0/*cvt*/, 0 };

    /* Create an array of blues ordered as said above. */
    int baseline, next;
    int bluecnt=ct->gi->bd->bluecnt;
    real blues [12][2];

    if (bluecnt == 0)
return;

    for (i=0; (i < bluecnt) && (ct->gi->bd->blues[i][1] < 0); i++);
    baseline = i;
    next = 0;

    if (baseline < bluecnt) {
	blues[next][0] = ct->gi->bd->blues[baseline][0];
	blues[next++][1] = ct->gi->bd->blues[baseline][1];
    }

    for (i=0; i<baseline; i++) {
	blues[next][0] = ct->gi->bd->blues[i][0];
	blues[next++][1] = ct->gi->bd->blues[i][1];
    }

    for (i=bluecnt-1; i>baseline; i--) {
	blues[next][0] = ct->gi->bd->blues[i][0];
	blues[next++][1] = ct->gi->bd->blues[i][1];
    }

#if 1
    /* For debugging purposes only */
    if (next != bluecnt) {
	IError("We have a stupid but serious bug in snapping to blues. Please tell the author.");
return;
    }
#endif

    for (i=0; i<bluecnt; i++) {
	/* decide the cvt index */
	if ((cvt = rint(blues[i][0])) < 0) cvt = rint(blues[i][1]);
	cvt = callargs[1] = TTF__getcvtval(ct->gi->sf,cvt);

	/* Process all hints with edges within ccurrent blue zone. */
	for ( hint=ct->sc->hstem; hint!=NULL; hint=hint->next ) {
	    if (hint->startdone || hint->enddone) continue;
	    
	    /* Wchich edge to start at? */
	    /*Starting at the other would usually be wrong. */
	    if ((blues[i][0] < 0) || ((hint->ghost) && (hint->width == 21))) {
		base = hint->start;
		advance = hint->start + hint->width;
	    }
	    else {
		base = hint->start + hint->width;
		advance = hint->start;
	    }

	    /* This is intended as a fallback if the base edge wasn't within
	     * this bluezone, and advance was. This seems a bit controversial.
	     * For now, I keep this turned on.
	     */
	    if ((base + ct->gi->fudge < blues[i][0]) ||
		(base - ct->gi->fudge > blues[i][1]))
	    {
		if (hint->ghost && ((hint->width == 20) || (hint->width == 21)))
		    continue;

		tmp = base;
		base = advance;
		advance = tmp;

		if ((base + ct->gi->fudge < blues[i][0]) ||
		    (base - ct->gi->fudge > blues[i][1])) continue;
	    }

	    /* instruct the stem */
	    init_edge(ct, base, EXTERNAL_CONTOURS);
	    if (ct->edge.refpt == -1) continue;
	    callargs[0] = ct->edge.refpt;
	    ct->pt = pushpoints(ct->pt, 3, callargs);
	    *(ct->pt)++ = CALL;
	    //ct->pt = pushpointstem(ct->pt, ct->edge.refpt, cvt);
	    //*(ct->pt)++ = MIAP_rnd;
	    ct->touched[ct->edge.refpt] |= tf_y;

	    finish_edge(ct, SHP_rp1);
	    if (hint->start == base) hint->startdone = true;
	    else hint->enddone = true;

	    if (hint->ghost && ((hint->width == 20) || (hint->width == 21))) {
		hint->startdone = hint->enddone = true;
		continue;
	    }

	    /* TODO! It might be worth to instruct at least one edge of each */
	    /* hint overlapping with currently processed. This would preserve */
	    /* relative hint placement in some difficult areas. */

	    cvt2 = _CVT_SeekInPrivateString(ct->gi->sf,
		PSDictHasEntry(ct->gi->sf->private, "StemSnapH"), hint->width,
		ct->gi->fudge);

	    init_edge(ct, advance, ALL_CONTOURS);
	    if (ct->edge.refpt == -1) continue;
	    if (cvt2 == -1) {
		ct->pt = pushpoint(ct->pt, ct->edge.refpt);
		*(ct->pt)++ = MDRP_min_rnd_black;
	    }
	    else {
		ct->pt = pushpointstem(ct->pt, ct->edge.refpt, cvt2);
		*(ct->pt)++ = MIRP_min_rnd_black;
	    }
	    ct->touched[ct->edge.refpt] |= tf_y;

	    finish_edge(ct, SHP_rp2);
	    if (hint->start == advance) hint->startdone = true;
	    else hint->enddone = true;
	}

#if 0
	/* Now I'll try to find points not snapped by any previous stem hint. */
	/* This will ensure correct glyph heights even if there are no hints. */
	/* NOTE! Seemingly, that was a bad idea */

	base = (blues[i][0] + blues[i][1]) / 2.0;
	real fudge = ct->gi->fudge;
	ct->gi->fudge = fabs(base - blues[i][0]);
	init_edge(ct, base, EXTERNAL_CONTOURS);
	ct->gi->fudge = fudge;
	optimize_edge(ct);

	if (ct->edge.refpt == -1) continue;

	if (!(ct->touched[ct->edge.refpt] || ct->affected[ct->edge.refpt])) {
	    callargs[0] = ct->edge.refpt;
	    ct->pt = pushpoints(ct->pt, 3, callargs);
	    *(ct->pt)++ = CALL;
	    ct->touched[ct->edge.refpt] |= tf_y;
	}

	int j;
	for (j=0; j<ct->edge.othercnt; j++) {
	    callargs[0] = ct->edge.others[j];
	    ct->pt = pushpoints(ct->pt, 3, callargs);
	    *(ct->pt)++ = CALL;
	    ct->touched[ct->edge.others[j]] |= tf_y;
	}

	if (ct->edge.othercnt > 0) {
	    free(ct->edge.others);
	    ct->edge.others = NULL;
	    ct->edge.othercnt = 0;
	}
#endif
    }
}

/******************************************************************************

 Find points that should be snapped to this hint's edges.
 The searching routine will return two types of points per edge:
 a 'chosen one' that should be used as a reference for this hint, and
 'others' to position after him with SHP[rp2].

 If the hint's width is in the PS dictionary, or at least it is close within
 fudge margin to one of the values, place the value in the cvt and use MIRP.
 Otherwise don't pollute the cvt and use MDRP. In both cases, the distance
 should be black, rounded and kept not under reasonable minimum.

 If one of the edges is already positioned, set a RP0 there, then MIRP or
 MDRP a reference point at the second edge, setting it as rp0 if this is
 the end vertical edge, and do its others.

 If none of the edges is positioned:
   If this hint is the first, previously overlapped, or simply horizontal,
   position the reference point at the base where it is using MDAP; otherwise
   position the hint's base rp0 relatively to the previous hint's end using
   MDRP with white minimum distance. Then do its others; MIRP or MDRP rp0
   at the hint's end and do its others.

 Mark startdones and enddones.

 ******************************************************************************/

static void geninstrs(InstrCt *ct, StemInfo *hint) {
    real hbase, base, width, hend;
    StemInfo *h;
    real fudge = ct->gi->fudge;
    StemInfo *firsthint, *lasthint=NULL, *testhint;
    int first;
    int cvtindex=-1;

    /* if this hint has conflicts don't try to establish a minimum distance */
    /* between it and the last stem, there might not be one */        
    if (ct->xdir) firsthint = ct->sc->vstem;
    else firsthint = ct->sc->hstem;
    for ( testhint = firsthint; testhint!=NULL && testhint!=hint; testhint = testhint->next ) {
	if ( HIoverlap(testhint->where,hint->where)!=0 )
	    lasthint = testhint;
    }
    first = lasthint==NULL;
    if ( hint->hasconflicts ) first = true;		

    /* Check whether to use CVT value or shift the stuff directly */
    hbase = base = rint(hint->start);
    width = rint(hint->width);
    hend = hbase + width;

    if (ct->xdir) cvtindex = _CVT_SeekInPrivateString(ct->gi->sf, PSDictHasEntry(ct->gi->sf->private, "StemSnapV"), width, fudge);
    else cvtindex = _CVT_SeekInPrivateString(ct->gi->sf, PSDictHasEntry(ct->gi->sf->private, "StemSnapH"), width, fudge);

    /* flip the hint if needed */
    if (hint->enddone) {
	hbase = (base + width);
	width = -width;
	hend = base;
    }

    if (hint->startdone || hint->enddone) {
	/* Set a reference point on the base edge.
	 * Ghost hints can get skipped, that's a bug.
	 */
	init_edge(ct, hbase, ALL_CONTOURS);
	if (ct->edge.refpt == -1) goto done;
	ct->pt = pushpoint(ct->pt, ct->edge.refpt);
	*ct->pt++ = MDAP; /* sets rp0 and rp1 */

	/* Still unhinted points on that edge? */
	if (ct->edge.othercnt != 0) finish_edge(ct, SHP_rp1);

	/* set a reference point on the other edge */
	init_edge(ct, hend, ALL_CONTOURS);
	if (ct->edge.refpt == -1) goto done;

	if (cvtindex == -1) {
	    ct->pt = pushpoint(ct->pt, ct->edge.refpt);
	    *ct->pt++ = MDRP_min_rnd_black;
	}
	else {
	    ct->pt = pushpointstem(ct->pt, ct->edge.refpt, cvtindex);
	    *ct->pt++ = MIRP_min_rnd_black;
	}

	ct->touched[ct->edge.refpt] |= (ct->xdir?tf_x:tf_y);

	/* finish the edge and prepare for the next stem hint */
	finish_edge(ct, SHP_rp2);

	if (hint->startdone) {
	    ct->pt = pushpoint(ct->pt, ct->edge.refpt);
	    *ct->pt++ = SRP0;
	}
    }
    else {
	init_edge(ct, hbase, ALL_CONTOURS);
	if (ct->edge.refpt == -1) goto done;

        /* Now I must place the stem's origin in respect to others... */
	/* The steps here are extremely simple and often do not work. */
	/* What's really needed here is an iterative procedure that would */
	/* preserve counters and widths, like in freetype2. */
	/* For horizontal stems, interpolating between blues MUST be done. */
	/* rp0 and rp1 are set to refpt after its positioning */
	if (!ct->xdir || first) {
	    ct->pt = pushpoint(ct->pt, ct->edge.refpt);
	    *ct->pt++ = MDAP_rnd;
	}
	else {
	  ct->pt = pushpointstem(ct->pt, ct->edge.refpt, ct->edge.refpt);
	  *ct->pt++ = MDRP_rp0_min_rnd_white;
	  *ct->pt++ = MDAP;
	}

	ct->touched[ct->edge.refpt] |= (ct->xdir?tf_x:tf_y);
	finish_edge(ct, SHP_rp1);

	/* Start the second edge */
	init_edge(ct, hend, ALL_CONTOURS);
	if (ct->edge.refpt == -1) goto done;

	if (cvtindex == -1) {
	  ct->pt = pushpoint(ct->pt, ct->edge.refpt);
	  *ct->pt++ = MDRP_rp0_min_rnd_black; //CAUTION!
	}
	else {
	    ct->pt = pushpointstem(ct->pt, ct->edge.refpt, cvtindex);
	    *ct->pt++ = MIRP_rp0_min_rnd_black; //CAUTION!
	}

	ct->touched[ct->edge.refpt] |= (ct->xdir?tf_x:tf_y);
	finish_edge(ct, SHP_rp2);
    }

    done:

    for ( h=hint->next; h!=NULL && h->start<=hint->start+hint->width; h=h->next ) {
	if ( (h->start>=hint->start-ct->gi->fudge && h->start<=hint->start+ct->gi->fudge) ||
		(h->start>=hint->start+hint->width-ct->gi->fudge && h->start<=hint->start+hint->width+ct->gi->fudge) )
	    h->startdone = true;
	if ( (h->start+h->width>=hint->start-ct->gi->fudge && h->start+h->width<=hint->start+ct->gi->fudge) ||
		(h->start+h->width>=hint->start+hint->width-ct->gi->fudge && h->start+h->width<=hint->start+hint->width+ct->gi->fudge) )
	    h->enddone = true;
    }
}

#if 0
static int MapSP2Index(SplineSet *ttfss,SplinePoint *csp, int ptcnt) {
    SplineSet *ss;
    SplinePoint *sp;

    if ( csp==NULL )
return( ptcnt+1 );		/* ptcnt+1 is the phantom point for the width */
    for ( ss=ttfss; ss!=NULL; ss=ss->next ) {
	for ( sp=ss->first;; ) {
	    if ( sp->me.x==csp->me.x && sp->me.y==csp->me.y )
return( sp->ttfindex );
	    if ( sp->next==NULL )
	break;
	    sp = sp->next->to;
	    if ( sp==ss->first )
	break;
	}
    }
return( -1 );
}

/* Order of the md hints is important hence the double loop. */
/* We only do a hint if one edge has been fixed (or if we have no choice) */
static uint8 *gen_md_instrs(struct glyphinstrs *gi, uint8 *instrs,MinimumDistance *_md,
	SplineSet *ttfss, BasePoint *bp, int ptcnt, int xdir, uint8 *touched) {
    int mask = xdir ? 1 : 2;
    int pt1, pt2;
    int any, graspatstraws=false, undone;
    MinimumDistance *md;

    do {
	any = false; undone = false;
	for ( md=_md; md!=NULL; md=md->next ) {
	    if ( md->x==xdir && !md->done ) {
		pt1 = MapSP2Index(ttfss,md->sp1,ptcnt);
		pt2 = MapSP2Index(ttfss,md->sp2,ptcnt);
		if ( pt1==ptcnt+1 ) {
		    pt1 = pt2;
		    pt2 = ptcnt+1;
		}
		if ( pt1==0xffff || pt2==0xffff )
		    fprintf(stderr, "Internal Error: Failed to find point in minimum distance check\n" );
		else if ( pt1!=ptcnt+1 && (touched[pt1]&mask) &&
			pt2!=ptcnt+1 && (touched[pt2]&mask) )
		    md->done = true;	/* somebody else did it, might not be right for us, but... */
		else if ( !graspatstraws &&
			!(touched[pt1]&mask) &&
			 (pt2==ptcnt+1 || !(touched[pt2]&mask)) )
		     /* If neither edge has been touched, then don't process */
		     /*  it now. hope that by filling in some other mds we will*/
		     /*  establish one edge or the other, and then come back to*/
		     /*  it */
		    undone = true;
		else if ( pt2==ptcnt+1 || !(touched[pt2]&mask)) {
		    md->done = true;
		    instrs = pushpointstem(instrs,pt2,pt1);	/* Misnomer, I'm pushing two points */
		    if ( !(touched[pt1]&mask))
			*instrs++ = 0x2f;			/* MDAP[rnd] */
		    else
			*instrs++ = 0x10;			/* SRP0 */
		    *instrs++ = 0xcc;			/* MDRP[01100] min, rnd, grey */
		    touched[pt1] |= mask;
		    if ( pt2!=ptcnt+1 )
			touched[pt2]|= mask;
		    any = true;
		} else {
		    md->done = true;
		    instrs = pushpointstem(instrs,pt1,pt2);	/* Misnomer, I'm pushing two points */
		    *instrs++ = 0x10;			/* SRP0 */
		    *instrs++ = 0xcc;			/* MDRP[01100] min, rnd, grey */
		    touched[pt1] |= mask;
		    any = true;
		}
	    }
	}
	graspatstraws = undone && !any;
    } while ( undone );
return(instrs);
}

/* Rounding extremae to grid is generally a good idea, so I do this by default.
 * Yet it is possible that rounding an extremum is wrong, and that some other
 * points might also be rounded. There once was a 'round to x/y grid' flag for
 * each point with an UI to set it. It's gone now, but could probably be helpful
 * in future. If an interpolated point is to be rounded, it's control points
 * are rounded instead.
 */

static void do_extrema(int ttfindex, SplinePoint *sp, InstrCt *ct) {
    if (!(ct->touched[ttfindex] & (ct->xdir?tf_x:tf_y)) &&
	!(ct->affected[ttfindex] & (ct->xdir?tf_x:tf_y)) &&
	 (ct->xdir?IsXExtremum(sp):IsYExtremum(sp)) &&
	 ((sp->ttfindex != 0xffff) || (ttfindex != sp->nextcpindex)))
    {
	ct->pt = pushpoint(ct->pt,ttfindex);
	*(ct->pt)++ = MDAP_rnd;
	ct->touched[ttfindex] |= ct->xdir?tf_x:tf_y;
    }
}

static void do_rounded(int ttfindex, SplinePoint *sp, InstrCt *ct) {
    if (!(ct->touched[ttfindex] & (ct->xdir?tf_x:tf_y)) &&
         ((sp->roundx && ct->xdir) || (sp->roundy && !ct->xdir)))
    {
	ct->pt = pushpoint(ct->pt,ttfindex);
	*(ct->pt)++ = MDAP_rnd;
	ct->touched[ttfindex] |= ct->xdir?tf_x:tf_y;
    }
}
#endif

/* Everything related with diagonal hinting goes here */

/* This structure is used to keep a point number together with
   its coordinates */
typedef struct numberedpoint {
    int num;
    struct basepoint *pt;
} NumberedPoint;

/* A line, described by two points */
typedef struct pointvector {
    struct numberedpoint *pt1, *pt2;
    int done;
} PointVector;

/* In this structure we store information about diagonales,
   relatively to which the given point should be positioned */
typedef struct diagpointinfo {
    struct pointvector *line[2];
    int count;
} DiagPointInfo;

/* Diagonal stem hints. This structure is a bit similar to DStemInfo FF
   uses in other cases, but additionally stores point numbers and hint width */
typedef struct dstem {
    struct dstem *next;
    struct numberedpoint pts[4];
    int done;
    real width;
} DStem;

/* Takes a line defined by two points and returns a vector decribed as a 
   pair of x and y values, such that the value (x2 + y2) is equal to 1.
   Note that the BasePoint structure is used to store the vector, although
   it is not a point itself. This is just because that structure has "x"
   and "y" fields which can be used for our purpose. */
static BasePoint *GetVector ( BasePoint *top,BasePoint *bottom,int orth ) {
    real catx, caty, hip, retsin, retcos;
    BasePoint *ret;
    
    catx=top->x-bottom->x; caty=top->y-bottom->y;
    hip=sqrt(( catx*catx )+( caty*caty ));
    retsin=caty/hip; retcos=catx/hip;
    
    ret=chunkalloc(sizeof(struct basepoint));
    
    if( !orth ) {
        ret->x=retcos; ret->y=retsin;
    } else {
        ret->x=-retsin; ret->y=retcos;
    }
    
    return ret;
}

/* Checks if a point is positioned on a diagonal line described by
   two other points. Note that this function will return (false) in case
   the test point is exactly coincident with the line's start or end points. */
static int IsPointOnLine( BasePoint *top, BasePoint *bottom, BasePoint *test ) {
    real slope, testslope;

    if (!(top->y > test->y) && (test->y > bottom->y))
        return (false);
    
    if (!(((top->x > test->x) && (test->x > bottom->x)) || 
        ((top->x < test->x) && (test->x < bottom->x))))
        return (false);
    
    slope = (top->y-bottom->y)/(top->x-bottom->x);
    testslope = (top->y-test->y)/(top->x-test->x);
    
return( RealApprox( slope, testslope ) );
}

/* find the orthogonal distance from the left stem edge to the right. 
   Make it positive (just a dot product with the unit vector orthog to 
   the left edge) */
static int DStemWidth( BasePoint *tl, BasePoint *bl, 
    BasePoint *tr, BasePoint *br ) {
    
    double tempx, tempy, len, stemwidth;

    tempx = tl->x-bl->x;
    tempy = tl->y-bl->y;
    len = sqrt(tempx*tempx+tempy*tempy);
    stemwidth = ((tr->x-tl->x)*tempy -
	    (tr->y-tl->y)*tempx)/len;
    if ( stemwidth<0 ) stemwidth = -stemwidth;
return( rint( stemwidth ));
}

static int BpIndex(BasePoint *search,BasePoint *bp,int ptcnt) {
    int i;

    for ( i=0; i<ptcnt; ++i )
	if ( rint(search->x) == bp[i].x && rint(search->y)==bp[i].y )
return( i );

return( -1 );
}

/* Order the given diagonal stems by the X coordinate of the left edge top,
   and by Y if X is the same. The order is arbtrary, but may be essential for
   things like "W", so we should be sure that we are doing diagonals from
   left to right. At the same time find some additional data which should
   be associated with each stem but aren't initially stored in DStemInfo,
   and put them into DStem structures */
static DStem  *DStemSort(DStemInfo *d, BasePoint *bp, int ptcnt, uint8 *touched) {
    DStemInfo *di, *di2, *cur=NULL;
    DStem *head, *newhead;
    real xmaxcur, xmaxtest, ymaxcur, ymaxtest;
    
    head = newhead = NULL;
    for ( di=d; di!=NULL; di=di->next ) di->used = false;
    
    for ( di=d; di!=NULL; di=di->next ) { 
        for ( di2=d; di2!=NULL; di2=di2->next ) { 
            if ( !di2->used ) {
                if ( cur==NULL || cur->used) 
                    cur = di2;

                else {
                    xmaxcur = cur->leftedgetop.x >= cur->leftedgebottom.x ?
                        cur->leftedgetop.x:cur->leftedgebottom.x;
                    xmaxtest = di2->leftedgetop.x >= di2->leftedgebottom.x ?
                        di2->leftedgetop.x:di2->leftedgebottom.x;

                    if (xmaxtest > xmaxcur)
                        cur = di2;
                    else if (xmaxtest == xmaxcur) {
                        ymaxcur = cur->leftedgetop.y >= cur->leftedgebottom.y ?
                            cur->leftedgetop.y:cur->leftedgebottom.y;
                        ymaxtest = di2->leftedgetop.y >= di2->leftedgebottom.y ?
                            di2->leftedgetop.y:di2->leftedgebottom.y;

                        if (ymaxtest < ymaxcur)
                            cur = di2;
                    }
                }
            }
        }
        cur->used = true;

        newhead = chunkalloc( sizeof( DStem ) );
	newhead->pts[0].pt = &(di->leftedgetop);
	newhead->pts[0].num = BpIndex( &di->leftedgetop,bp,ptcnt );
	newhead->pts[1].pt = &(di->leftedgebottom);
	newhead->pts[1].num = BpIndex( &di->leftedgebottom,bp,ptcnt );
	newhead->pts[2].pt = &(di->rightedgetop);
	newhead->pts[2].num = BpIndex( &di->rightedgetop,bp,ptcnt );
	newhead->pts[3].pt = &(di->rightedgebottom);
	newhead->pts[3].num = BpIndex( &di->rightedgebottom,bp,ptcnt );
        newhead->width = DStemWidth( &di->leftedgetop,&di->leftedgebottom,
            &di->rightedgetop,&di->rightedgebottom );
        newhead->done = false;
        newhead->next = head;

        head = newhead;
    }

return( head );
}

/* Run on points and check each points agains each diagonale. In case the
   point is the diagonale's starting or ending point, or is positioned on 
   that line, associate the information about the that line with the given
   point and put it into an array. Note that we have to do this on a relatively
   early stage, as it may be important to know, if the given point is subject
   to the subsequent diagonale hinting, before any actual processing of
   diagonal stems is started.*/
static DiagPointInfo *GetDiagPoints ( InstrCt *ct, DStem *ds ) {
    DStem *curds;
    DiagPointInfo *diagpts;
    int i, j, ptcnt, num;
    
    ptcnt = ct->ptcnt;
    diagpts = gcalloc( ptcnt, sizeof ( struct diagpointinfo ) );
    
    for ( i=0; i<ptcnt; i++ ) {
        diagpts[i].count = 0;
        
        for ( curds=ds; curds!=NULL; curds=curds->next ) {
            for ( j=0; j<3; j=j+2) {
                if ((i == curds->pts[j].num || i == curds->pts[j+1].num) ||
                    IsPointOnLine( curds->pts[j].pt, curds->pts[j+1].pt, &(ct->bp[i]) )) {
                    
                    if ( diagpts[i].count <= 2 ) {
                        num = diagpts[i].count;
                        diagpts[i].line[num] = 
                            chunkalloc(sizeof(struct pointvector));
                        diagpts[i].line[num]->pt1 = &(curds->pts[j]);
                        diagpts[i].line[num]->pt2 = &(curds->pts[j+1]);
                        diagpts[i].line[num]->done = false;

                        diagpts[i].count++;
                    }
                }
            }
        }
    }
    
return ( diagpts );
}

/* Usually we have to start doing each diagonal stem from the point which
   is most touched in any directions. */
static int FindDiagStartPoint( DStem *ds, uint8 *touched ) {
    int i;
    
    for (i=0; i<4; ++i) {
        if (touched[ds->pts[i].num] & (tf_x | tf_y))
return i;
    }

    for (i=0; i<4; ++i) {
        if (touched[ds->pts[i].num] & tf_y)
return i;
    }

    for (i=0; i<4; ++i) {
        if (touched[ds->pts[i].num] & tf_x)
return i;
    }
return 0;
}

/* Check the directions at which the given point still can be moved
   (i. e. has not yet been touched) and set freedom vector to that
   direction in case it has not already been set */
static int SetFreedomVector( uint8 **instrs,int pnum,int ptcnt,
    uint8 *touched,DiagPointInfo *diagpts,
    NumberedPoint *lp1,NumberedPoint *lp2,BasePoint **fv) {
    
    int i;
    NumberedPoint *start, *end;
    BasePoint *newfv;
    
    if ( (touched[pnum] & tf_d) && !(touched[pnum] & tf_x) && !(touched[pnum] & tf_y)) {
        for( i=0 ; i<diagpts[pnum].count ; i++) {
            if (diagpts[pnum].line[i]->done) {
                start = diagpts[pnum].line[i]->pt1;
                end = diagpts[pnum].line[i]->pt2;
            }
        }
        /* This should never happen */
        if (start == NULL || end == NULL)
return( false );
        
        newfv = GetVector ( start->pt,end->pt,false );
        if (!RealApprox((*fv)->x, newfv->x) || !RealApprox((*fv)->y, newfv->y)) {
            (*fv)->x = newfv->x; (*fv)->y = newfv->y;
	
            *instrs = pushheader( *instrs,ptcnt>255,2 );
            *instrs = addpoint( *instrs,ptcnt>255,start->num );
            *instrs = addpoint( *instrs,ptcnt>255,end->num );

            *(*instrs)++ = 0x08;       /*SFVTL[parallel]*/
        }
        chunkfree (newfv, sizeof( BasePoint ) );
        
return ( true );
    } else if ( (touched[pnum] & tf_x) && !(touched[pnum] & tf_d) && !(touched[pnum] & tf_y)) {
        if (!((*fv)->x == 0 && (*fv)->y == 1)) {
            (*fv)->x = 0; (*fv)->y = 1;
            *(*instrs)++ = 0x04;   /*SFVTCA[y]*/
        }
return ( true );

    } else if ( (touched[pnum] & tf_y) && !(touched[pnum] & tf_d) && !(touched[pnum] & tf_x)) {
        if (!((*fv)->x == 1 && (*fv)->y == 0)) {
            *(*instrs)++ = 0x05;   /*SFVTCA[x]*/
            (*fv)->x = 1; (*fv)->y = 0;
        }
return ( true );
    } else if ( !(touched[pnum] & (tf_x|tf_y|tf_d))) {
        newfv = GetVector( lp1->pt,lp2->pt,true );
        if ( !RealApprox((*fv)->x, newfv->x) || !RealApprox((*fv)->y, newfv->y) ) {
            (*fv)->x = newfv->x; (*fv)->y = newfv->y;
        
            *(*instrs)++ = 0x0E;       /*SFVTPV*/
        }
        chunkfree (newfv, sizeof( BasePoint ) );
        
return ( true );
    }
    
return ( false );
}

static int MarkLineFinished ( int pnum,int startnum,int endnum,
    DiagPointInfo *diagpts ) {
    
    int i;
    
    for( i=0 ; i<diagpts[pnum].count ; i++) {
        if ((diagpts[pnum].line[i]->pt1->num == startnum) &&
            (diagpts[pnum].line[i]->pt2->num == endnum)) {
            
            diagpts[pnum].line[i]->done = true;
return( true );
        }
    }
return( false );
}

/* A basic algorith for hinting diagonal stems:
   -- iterate through diagonal stems, ordered from left to right;
   -- for each stem, find the most touched point, to start from,
      and fix that point. TODO: the positioning should be done
      relatively to points already touched by x or y;
   -- position the second point on the same edge, using dual projection
      vector;
   -- link to the second edge and repeat the same operation.
   
   For each point we first determine a direction at which it still can
   be moved. If a point has already been positioned relatively to another
   diagonal line, then we move it along that diagonale. Thus this algorithm
   can handle things like "V" where one line's ending point is another
   line's starting point without special exceptions.
*/
static uint8 *FixDstem( InstrCt *ct, DStem **ds, DiagPointInfo *diagpts, BasePoint *fv) {
    int startnum, a1, a2, b1, b2, ptcnt;
    NumberedPoint *v1, *v2;
    real distance;
    uint8 *instrs, *touched;
    
    if ((*ds)->done)
return (ct->pt);
    
    ptcnt = ct->ptcnt;
    touched = ct->touched;
    instrs = ct->pt;
    
    startnum = FindDiagStartPoint( (*ds), touched );
    a1 = (*ds)->pts[startnum].num;
    if ((startnum == 0) || (startnum == 1)) {
        v1 = &((*ds)->pts[0]); v2 = &((*ds)->pts[1]);
        a2 = (startnum == 1)?(*ds)->pts[0].num:(*ds)->pts[1].num;
        b1 = (*ds)->pts[2].num; b2 = (*ds)->pts[3].num;
    } else {
        v1 = &((*ds)->pts[2]); v2 = &((*ds)->pts[3]);
        a2 = (startnum == 3)?(*ds)->pts[2].num:(*ds)->pts[3].num;
        b1 = (*ds)->pts[0].num; b2 = (*ds)->pts[1].num;
    }
    instrs = pushheader( instrs,ptcnt>255,2 );
    instrs = addpoint( instrs,ptcnt>255,v1->num );
    instrs = addpoint( instrs,ptcnt>255,v2->num );

    *instrs++ = 0x87;       /*SDPVTL [orthogonal] */
    if (SetFreedomVector( &instrs,a1,ptcnt,touched,diagpts,v1,v2,&fv )) {
        instrs = pushpoint(instrs,a1);
        *instrs++ = MDAP;
        touched[a1] |= tf_d;
        
        /* Mark the point as already positioned relatively to the given
           diagonale. As the point may be associated either with the current 
           vector, or with another edge of the stem (which, of course, both point
           in the same direction), we have to check it agains both vectors. */
        if (!MarkLineFinished( a1,(*ds)->pts[0].num,(*ds)->pts[1].num,diagpts ))
            MarkLineFinished( a1,(*ds)->pts[2].num,(*ds)->pts[3].num,diagpts );
    }
    
    if ( SetFreedomVector( &instrs,a2,ptcnt,touched,diagpts,v1,v2,&fv )) {
        instrs = pushpoint(instrs,a2);
        *instrs++ = 0x3c;	    /* ALIGNRP */
        touched[a2] |= tf_d; 
        if (!MarkLineFinished( a2,(*ds)->pts[0].num,(*ds)->pts[1].num,diagpts ))
            MarkLineFinished( a2,(*ds)->pts[2].num,(*ds)->pts[3].num,diagpts );
    }
    
    /* Always put the calculated stem width into the CVT table, unless it is
       already there. This approach would be wrong for vertical or horizontal
       stems, but for diagonales it is just unlikely that we can find an 
       acceptable predefined value in StemSnapH or StemSnapW */
    distance = TTF_getcvtval(ct->gi->sf,(*ds)->width);
    
    if ( SetFreedomVector( &instrs,b1,ptcnt,touched,diagpts,v1,v2,&fv )) {
        instrs = pushpointstem(instrs,b1,distance);
        *instrs++ = 0xe0+0x19;  /* MIRP, srp0, minimum, black */
        touched[b1] |= tf_d; 
        if (!MarkLineFinished( b1,(*ds)->pts[0].num,(*ds)->pts[1].num,diagpts ))
            MarkLineFinished( b1,(*ds)->pts[2].num,(*ds)->pts[3].num,diagpts );
    }
    
    if ( SetFreedomVector( &instrs,b2,ptcnt,touched,diagpts,v1,v2,&fv )) {
        instrs = pushpoint(instrs,b2);
        *instrs++ = 0x3c;	    /* ALIGNRP */
        touched[b2] |= tf_d; 
        if (!MarkLineFinished( b2,(*ds)->pts[0].num,(*ds)->pts[1].num,diagpts ))
            MarkLineFinished( b2,(*ds)->pts[2].num,(*ds)->pts[3].num,diagpts );
    }
    
    (*ds)->done = true;
return instrs;
}

static uint8 *FixPointOnLine (DiagPointInfo *diagpts,PointVector *line,
    NumberedPoint *pt,InstrCt *ct,BasePoint **fv,BasePoint **pv,
    int *rp0,int *rp1,int *rp2) {
    
    uint8 *instrs, *touched;
    BasePoint *newpv;
    int ptcnt;

    touched = ct->touched;
    instrs = ct->pt;
    ptcnt = ct->ptcnt;

    newpv = GetVector( line->pt1->pt,line->pt2->pt,true );

    if (!RealApprox((*pv)->x, newpv->x) || !RealApprox((*pv)->y, newpv->y)) {

        (*pv)->x = newpv->x; (*pv)->y = newpv->y;

        instrs = pushheader( instrs,ptcnt>255,2 );
        instrs = addpoint( instrs,ptcnt>255,line->pt1->num );
        instrs = addpoint( instrs,ptcnt>255,line->pt2->num );

        *instrs++ = 0x07;         /*SPVTL[orthogonal]*/
    }

    if ( SetFreedomVector( &instrs,pt->num,ptcnt,touched,diagpts,line->pt1,line->pt2,&(*fv) ) ) {
        if ( *rp0!=line->pt1->num ) {
            *rp0=line->pt1->num;
            instrs = pushheader( instrs,ptcnt>255,2 );
            instrs = addpoint( instrs,ptcnt>255,pt->num );
            instrs = addpoint( instrs,ptcnt>255,line->pt1->num );
            *instrs++ = 0x10;	    /* Set RP0, SRP0 */
        } else {
            instrs = pushpoint( instrs,pt->num );
        }
        *instrs++ = 0x3c;	    /* ALIGNRP */

        /* If a point has to be positioned just relatively to the diagonal
           line (no intersections, no need to maintain other directions),
           then we can interpolate it along that line. This usually produces
           better results for things like Danish slashed "O". */
        if ( !(touched[pt->num] & (tf_x|tf_y|tf_d)) &&
            !(diagpts[pt->num].count > 1) ) {
            if ( *rp1!=line->pt1->num || *rp2!=line->pt2->num) {
                *rp1=line->pt1->num;
                *rp2=line->pt2->num;
                instrs = pushheader( instrs,ptcnt>255,3 );
                instrs = addpoint( instrs,ptcnt>255,pt->num );
                instrs = addpoint( instrs,ptcnt>255,line->pt2->num );
                instrs = addpoint( instrs,ptcnt>255,line->pt1->num );
                *instrs++ = 0x11;	    /* Set RP1, SRP1 */
                *instrs++ = 0x12;	    /* Set RP2, SRP2 */
            } else {
                instrs = pushpoint( instrs,pt->num );
            }
            *instrs++ = 0x39;	    /* Interpolate points, IP */
        }
    }

return ( instrs );
}

/* When all stem edges have already been positioned, run through other
   points which are known to be related with some diagonales and position
   them too. This may include both intersections and point which just
   lie on a diagonal line. This function does not care about starting/ending
   points of stems, unless they should be additinally positioned relatively
   to another stem. Thus is can handle things like "X" or "K". */
static uint8 *MovePointsToIntersections( InstrCt *ct,DiagPointInfo *diagpts,
    BasePoint *fv ) {
    
    int i, j, ptcnt, rp0=-1, rp1=-1, rp2=-1;
    uint8 *touched;
    BasePoint *pv;
    NumberedPoint *curpt;
    
    touched = ct->touched;
    ptcnt = ct->ptcnt;
    pv = chunkalloc( sizeof(struct basepoint) );
    pv->x = 1; pv->y = 0;
    
    for ( i=0; i<ptcnt; i++ ) {
        if ( diagpts[i].count > 0 ) {
            for ( j=0 ; j<diagpts[i].count ; j++) {
                if (!diagpts[i].line[j]->done) {
                    curpt = chunkalloc(sizeof(struct numberedpoint));
                    curpt->num = i;
                    curpt->pt = &(ct->bp[i]);

                    ct->pt = FixPointOnLine( diagpts,diagpts[i].line[j],
                        curpt,ct,&fv,&pv,&rp0,&rp1,&rp2 );

                    touched[i] |= tf_d;
                    diagpts[i].line[j]->done = ( true );
                    chunkfree( curpt,sizeof(struct numberedpoint) );
                }
            }
        }
    }

    chunkfree( pv,sizeof(struct basepoint) );
return ( ct->pt );
}

/* Finally explicitly touch all affected points by X and Y (uless they
   have already been), so that subsequent YUP's can't distort our
   stems. */
static uint8 *TouchDStemPoints( InstrCt *ct,DiagPointInfo *diagpts, 
    BasePoint *fv ) {
    
    int i, ptcnt, numx=0, numy=0, numpushes=0;
    uint8 *instrs, *touched, *tobefixedy, *tobefixedx;

    touched = ct->touched;
    instrs = ct->pt;
    ptcnt = ct->ptcnt;
    
    tobefixedy = gcalloc( ptcnt,1 );
    tobefixedx = gcalloc( ptcnt,1 );

    for ( i=0; i<ptcnt; i++ ) {
        if ( diagpts[i].count > 0 ) {
            if (!(touched[i] & tf_y)) {
                tobefixedy[numy++]=i;
                touched[i] |= tf_y;
            }

            if (!(touched[i] & tf_x)) {
                tobefixedx[numx++]=i;
                touched[i] |= tf_x;
            }
        }
    }

    numpushes = numy + numx;
    
    instrs = pushheader( instrs,ptcnt>255,numpushes );
    for ( i=0 ; i<numx ; i++ )
        instrs = addpoint( instrs,ptcnt>255,tobefixedx[i] );
    
    for ( i=0 ; i<numy ; i++ )
        instrs = addpoint( instrs,ptcnt>255,tobefixedy[i] );
    
    if ( numy>0 ) {
        if ( !(fv->x == 0 && fv->y == 1) ) *instrs++ = SVTCA_y;
        
        fv->x=0; fv->y=1;
        for ( i=0 ; i<numy ; i++ )
            *instrs++ = MDAP;
    }

    if ( numx>0 ) {
        if ( !(fv->x == 1 && fv->y == 0) ) *instrs++ = SVTCA_x;

        fv->x=1; fv->y=0;
        for ( i=0 ; i<numx ; i++ )
            *instrs++ = MDAP;
    }

    chunkfree( tobefixedy,ptcnt );
    chunkfree( tobefixedx,ptcnt );
return instrs;
}

static void DStemFree( DStem *ds,DiagPointInfo *diagpts,int cnt ) {
    DStem *next;
    int i,j;

    while ( ds!=NULL ) {
	next = ds->next;
	chunkfree( ds,sizeof( struct dstem ));
	ds = next;
    }
    
    for ( i=0; i<cnt ; i++ ) {
        if (diagpts[i].count > 0) {
            for ( j=0 ; j<diagpts[i].count ; j++ ) {
                chunkfree ( diagpts[i].line[j],sizeof( struct pointvector ) );
            }
        }
    }
}

static void DStemInfoGeninst( InstrCt *ct ) {
    DStemInfo *di;
    DStem *ds, *curds;
    BasePoint *fv;
    DiagPointInfo *diagpts;
    
    di = ct->sc->dstem;
    if (di == NULL)
return;

    ds = DStemSort( di,ct->bp,ct->ptcnt,ct->touched );
    diagpts = GetDiagPoints( ct,ds );

    fv=chunkalloc( sizeof( struct basepoint ) );
    fv->x=1; fv->y=0;

    for ( curds=ds; curds!=NULL; curds=curds->next )
        ct->pt = FixDstem ( ct,&curds,diagpts,fv );
    
    ct->pt = MovePointsToIntersections( ct,diagpts,fv );
    ct->pt = TouchDStemPoints ( ct,diagpts,fv);

    chunkfree( fv,sizeof(struct basepoint ));
    DStemFree( ds,diagpts,ct->ptcnt );
    free( diagpts );
}

/* End of the code related with diagonal stems */

static uint8 *dogeninstructions(InstrCt *ct) {
    StemInfo *hint;
    int max;
    DStemInfo *dstem;

    /* very basic preparations for default hinting */
    /* TODO! Move init_maxp to the end, to use some collected statistics. */
    init_maxp(ct);
    init_prep(ct);
    init_fpgm(ct);

    /* Maximum instruction length is 6 bytes for each point in each dimension */
    /*  2 extra bytes to finish up. And one byte to switch from x to y axis */
    /* Diagonal take more space because we need to set the orientation on */
    /*  each stem, and worry about intersections, etc. */
    /*  That should be an over-estimate */
    max=2;
    if ( ct->sc->vstem!=NULL ) max += ct->ptcnt*6;
    if ( ct->sc->hstem!=NULL ) max += ct->ptcnt*6+1;
    for ( dstem=ct->sc->dstem; dstem!=NULL; max+=7+4*6+100, dstem=dstem->next );
    if ( ct->sc->md!=NULL ) max += ct->ptcnt*12;
    max += ct->ptcnt*6;			/* in case there are any rounds */
    max += ct->ptcnt*6;			/* paranoia */
    ct->instrs = ct->pt = galloc(max);

    /* Initially no stem hints are done */
    for ( hint=ct->sc->vstem; hint!=NULL; hint=hint->next )
	hint->enddone = hint->startdone = false;
    for ( hint=ct->sc->hstem; hint!=NULL; hint=hint->next )
	hint->enddone = hint->startdone = false;

    /* We start from instructing horizontal features (=> movement in y) */
    /*  Do this first so that the diagonal hinter will have everything moved */
    /*  properly when it sets the projection vector */
    /*  Even if we aren't doing the diagonals, we do the blues. */
    ct->xdir = false;
    *(ct->pt)++ = SVTCA_y;

    /* First of all, snap key points to the blue zones. This gives consistent */
    /* glyph heights and usually improves the text look very much. The blues */
    /* will be placed in CVT automagically, but the user may want to tweak'em */
    /* in PREP on his own. */
    snap_to_blues(ct);

    /* instruct horizontal stems (=> movement in y) */
    ct->pt = pushpoint(ct->pt, ct->ptcnt);
    *(ct->pt)++ = SRP0;
    for ( hint=ct->sc->hstem; hint!=NULL; hint=hint->next )
	if ( !hint->startdone || !hint->enddone )
	    geninstrs(ct,hint);

    /* TODO! Instruct top and bottom bearings for fonts which have them. */

    /* Extremae and others should take shifts by stems into account. */
    //RunOnPoints(ct, ALL_CONTOURS, &do_extrema);
    //RunOnPoints(ct, ALL_CONTOURS, &do_rounded);
    //ct->pt = gen_md_instrs(ct->gi,ct->pt,ct->sc->md,ct->ss,ct->bp,ct->ptcnt,false,ct->touched);

    /* next instruct vertical features (=> movement in x) */
    ct->xdir = true;
    if ( ct->pt != ct->instrs ) *(ct->pt)++ = SVTCA_x;

    /* instruct vertical stems (=> movement in x) */
    ct->pt = pushpoint(ct->pt, ct->ptcnt);
    *(ct->pt)++ = SRP0;
    for ( hint=ct->sc->vstem; hint!=NULL; hint=hint->next )
	if ( !hint->startdone || !hint->enddone )
	    geninstrs(ct,hint);

    /* instruct right sidebearing */
    if (ct->sc->width != 0) {
	ct->pt = pushpoint(ct->pt, ct->ptcnt+1);
	*(ct->pt)++ = MDRP_min_rnd_white;
    }

    /* Extremae and others should take shifts by stems into account. */
    //RunOnPoints(ct, ALL_CONTOURS, &do_extrema);
    //RunOnPoints(ct, ALL_CONTOURS, &do_rounded);

    /* finally instruct diagonal stems (=> movement in x) */
    /*  This is done after vertical stems because it involves */
    /*  moving some points out-of their vertical stems. */
    DStemInfoGeninst(ct);
    //ct->pt = gen_md_instrs(ct->gi,ct->pt,ct->sc->md,ct->ss,ct->bp,ct->ptcnt,true,ct->touched);

    /* Interpolate untouched points */
    *(ct->pt)++ = IUP_y;
    *(ct->pt)++ = IUP_x;

    if ((ct->pt)-(ct->instrs) > max) IError(
	"We're about to crash.\n"
	"We miscalculated the glyph's instruction set length\n"
	"When processing TTF instructions (hinting) of %s", ct->sc->name
    );

    ct->sc->ttf_instrs_len = (ct->pt)-(ct->instrs);
return ct->sc->ttf_instrs = grealloc(ct->instrs,(ct->pt)-(ct->instrs));
}

void NowakowskiSCAutoInstr(SplineChar *sc, BlueData *bd) {
    BlueData _bd;
    struct glyphinstrs gi;
    int cnt, contourcnt;
    BasePoint *bp;
    int *contourends;
    uint8 *touched;
    uint8 *affected;
    SplineSet *ss;
    RefChar *ref;
    InstrCt ct;

    if ( !sc->parent->order2 )
return;

    if ( sc->layers[ly_fore].refs!=NULL && sc->layers[ly_fore].splines!=NULL ) {
	gwwv_post_error(_("Can't instruct this glyph"),
		_("TrueType does not support mixed references and contours.\nIf you want instructions for %.30s you should either:\n * Unlink the reference(s)\n * Copy the inline contours into their own (unencoded\n    glyph) and make a reference to that."),
		sc->name );
return;
    }
    for ( ref = sc->layers[ly_fore].refs; ref!=NULL; ref=ref->next ) {
	if ( ref->transform[0]>=2 || ref->transform[0]<-2 ||
		ref->transform[1]>=2 || ref->transform[1]<-2 ||
		ref->transform[2]>=2 || ref->transform[2]<-2 ||
		ref->transform[3]>=2 || ref->transform[3]<-2 )
    break;
    }
    if ( ref!=NULL ) {
	gwwv_post_error(_("Can't instruct this glyph"),
		_("TrueType does not support references which\nare scaled by more than 200%%.  But %1$.30s\nhas been in %2$.30s. Any instructions\nadded would be meaningless."),
		ref->sc->name, sc->name );
return;
    }

    if ( sc->ttf_instrs ) {
	free(sc->ttf_instrs);
	sc->ttf_instrs = NULL;
	sc->ttf_instrs_len = 0;
    }
    SCNumberPoints(sc);
    if ( autohint_before_generate && sc->changedsincelasthinted &&
	    !sc->manualhints )
	SplineCharAutoHint(sc,NULL);

    if ( bd==NULL ) {
	QuickBlues(sc->parent,&_bd);
	bd = &_bd;
    }

    if ( sc->vstem==NULL && sc->hstem==NULL && sc->dstem==NULL && sc->md==NULL && !bd->bluecnt)
return;

    /* TODO!
     *
     * I'm having problems with references that are rotated or flipped
     * horizontally. Basically, such glyphs can get negative width. Such widths
     * are treated very differently under Freetype (OK) and Windows (terribly
     * shifted), and I suppose other rasterizers can also complain.
     */
    if ( sc->layers[ly_fore].splines==NULL )
return;

    gi.sf = sc->parent;
    gi.bd = bd;
    gi.fudge = (sc->parent->ascent+sc->parent->descent)/500;

    contourcnt = 0;
    for ( ss=sc->layers[ly_fore].splines; ss!=NULL; ss=ss->next, ++contourcnt );
    cnt = SSTtfNumberPoints(sc->layers[ly_fore].splines);

    contourends = galloc((contourcnt+1)*sizeof(int));
    bp = galloc(cnt*sizeof(BasePoint));
    touched = gcalloc(cnt,1);
    affected = gcalloc(cnt,1);
    contourcnt = cnt = 0;
    for ( ss=sc->layers[ly_fore].splines; ss!=NULL; ss=ss->next ) {
	touched[cnt] |= tf_startcontour;
	cnt = SSAddPoints(ss,cnt,bp,NULL);
	touched[cnt-1] |= tf_endcontour;
	contourends[contourcnt++] = cnt-1;
    }
    contourends[contourcnt] = 0;

    ct.sc = sc;
    ct.ss = sc->layers[ly_fore].splines;
    ct.gi = &gi;
    ct.instrs = NULL;
    ct.pt = NULL;
    ct.ptcnt = cnt;
    ct.contourends = contourends;
    ct.bp = bp;
    ct.touched = touched;
    ct.affected = affected;

    dogeninstructions(&ct);

    free(touched);
    free(affected);
    free(bp);
    free(contourends);

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    SCMarkInstrDlgAsChanged(sc);
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
}
