/* Copyright (C) 2002-2007 by George Williams */
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
#include <gkeysym.h>
#include <math.h>


#include "sftextfieldP.h"
#include <ustring.h>
#include <utype.h>
#include <chardata.h>

static GBox sftextarea_box = { /* Don't initialize here */ 0 };
static int sftextarea_inited = false;
static FontInstance *sftextarea_font;

static unichar_t nullstr[] = { 0 }, 
	newlinestr[] = { '\n', 0 }, tabstr[] = { '\t', 0 };

static int SFTextArea_Show(SFTextArea *st, int pos);
static void GTPositionGIC(SFTextArea *st);

static uint32 *TagsCopy(uint32 *tags) {
    int i;
    uint32 *ret;

    if ( tags==NULL )
return( NULL );
    for ( i=0; tags[i]!=0; ++i );
    ret = galloc((i+1)*sizeof(uint32));
    for ( i=0; tags[i]!=0; ++i )
	ret[i] = tags[i];
    ret[i] = 0;
return( ret );
}

static int TagsSame(uint32 *tags1, uint32 *tags2) {
    int i;

    if ( tags1==NULL || tags2==NULL )
return( tags1==NULL && tags2==NULL );

    for ( i=0; tags1[i]!=0 && tags1[i]==tags2[i]; ++i );
return( tags1[i]==tags2[i] );
return( false );
}

static int _FDMap(FontData *fd,int uenc) {
    /* given a unicode code point, find the encoding in this font */
    int gid;

    if ( uenc>=fd->sfmap->map->enccount )
return( -1 );
    gid = fd->sfmap->map->map[uenc];
    if ( gid==-1 || fd->sf->glyphs[gid]==NULL )
return( -1 );

return( gid );
}

static SplineChar *FDMap(FontData *fd,int uenc) {
    /* given a unicode code point, find the encoding in this font */
    /* We've already converted arabic to its forms... but we did that to */
    /*  the deprecated unicode code points. If those don't work see if we */
    /*  can find a substitution lookup. */
    int gid;

    gid = _FDMap(fd,uenc);
    if ( gid!=-1 && fd->sf->glyphs[gid]!=NULL )
return( fd->sf->glyphs[gid] );
    gid = fd->sfmap->notdef_gid;
    if ( gid!=-1 && fd->sf->glyphs[gid]!=NULL )
return( fd->sf->glyphs[gid] );

return( fd->sfmap->fake_notdef );
}

static void GImageDrawRect(GImage *img,GRect *r,Color col) {
    struct _GImage *base;
    int i;

    base = img->u.image;
    if ( r->y>=base->height || r->x>=base->width )
return;

    for ( i=0; i<r->width; ++i ) {
	if ( i+r->x>=base->width )
    break;
	base->data[r->y*base->bytes_per_line + i + r->x] = col;
	if ( r->y+r->height-1<base->height )
	    base->data[(r->y+r->height-1)*base->bytes_per_line + i + r->x] = col;
    }
    for ( i=0; i<r->height; ++i ) {
	if ( i+r->y>=base->height )
    break;
	base->data[(r->y+i)*base->bytes_per_line + r->x] = col;
	if ( r->x+r->width-1<base->width )
	    base->data[(r->y+i)*base->bytes_per_line + r->x+r->width-1] = col;
    }
}

static void GImageDrawImage(GImage *dest,GImage *src,int x, int y) {
    struct _GImage *sbase, *dbase;
    int i,j, di, sbi, dbi, val;

    dbase = dest->u.image;
    sbase =  src->u.image;

    for ( i=0; i<sbase->height; ++i ) {
	di = y + i;
	if ( di<0 || di>=dbase->height )
    continue;
	sbi = i*sbase->bytes_per_line;
	dbi = di*dbase->bytes_per_line;
	for ( j=0; j<sbase->width; ++j ) {
	    if ( x+j<0 || x+j>=dbase->width )
	continue;
	    val = dbase->data[dbi+x+j] + sbase->data[sbi+j];
	    if ( val>255 ) val = 255;
	    dbase->data[dbi+x+j] = val;
	}
    }
}

static int FDDrawChar(GWindow pixmap,GImage *img,struct opentype_str *osc,int x,int y,Color col) {
    BDFChar *bdfc;
    int gid;
    FontData *fd;
    SplineChar *sc;

    if ( osc==NULL )
return( x );
    sc = osc->sc;
    fd = ((struct fontlist *) (osc->fl))->fd;

    x += osc->vr.xoff;
    y += osc->vr.yoff;

    gid = sc->orig_pos;
    if ( gid!=-1 && fd->bdf->glyphs[gid]==NULL )
	BDFPieceMeal(fd->bdf,gid);
    if ( gid==-1 || (bdfc=fd->bdf->glyphs[gid])==NULL ) {
	if ( col!=-1 ) {
	    GRect r;
	    r.x = x+1; r.width= osc->advance_width-2;
	    r.height = (2*fd->bdf->ascent/3); r.y = y-r.height;
	    if ( img!=NULL )
		GImageDrawRect(img,&r,col);
	    else
		GDrawDrawRect(pixmap,&r,col);
	}
	x += fd->bdf->ascent/2;
    } else {
	if ( col!=-1 ) {
	    if ( !fd->antialias )
		fd->clut.clut[1] = col;		/* Only works for bitmaps */
	    if ( fd->base.clut!=NULL )
		fd->base.clut->trans_index = 0;
	    else
		fd->base.trans = 0;
	    fd->base.data = bdfc->bitmap;
	    fd->base.bytes_per_line = bdfc->bytes_per_line;
	    fd->base.width = bdfc->xmax-bdfc->xmin+1;
	    fd->base.height = bdfc->ymax-bdfc->ymin+1;
	    if ( img!=NULL )
		GImageDrawImage(img,&fd->gi,x+bdfc->xmin, y-bdfc->ymax);
	    else
		GDrawDrawImage(pixmap,&fd->gi,NULL,x+bdfc->xmin, y-bdfc->ymax);
	    fd->base.clut->trans_index = -1;
	}
	x += bdfc->width;
    }
return( x );
}

static int LinesInPara(SFTextArea *st, struct opentype_str **paratext) {
    int start, end, break_pos, cnt;
    int len, pos;

    if ( paratext==NULL )
return( 1 );
    if ( !st->wrap ) {
	for ( start=0; paratext[start]!=NULL; ++start);
	paratext[start-1]->line_break_after = true;
return( 1 );
    }
    cnt = 0;
    for ( start=0; paratext[start]!=NULL; ) {
	break_pos = start;
	len = paratext[start]->advance_width + paratext[start]->vr.h_adv_off;
	for ( end=start+1 ; paratext[end]!=NULL; ++end ) {
	    len += paratext[end]->advance_width + paratext[end]->vr.h_adv_off;
	    if ( len>st->g.inner.width && break_pos!=start ) {
		paratext[break_pos]->line_break_after = true;
		start = break_pos+1;
	break;
	    }
	    pos = paratext[end]->orig_index +
		    ((struct fontlist *) (paratext[end]->fl))->start;
	    if ( ((st->text[pos+1]<0x10000 && st->text[pos]<0x10000 &&
			isbreakbetweenok(st->text[pos],st->text[pos+1])) ||
		    (st->text[pos]==' ' && st->text[pos+1]>=0x10000 )))
		break_pos = end;
	}
	if ( paratext[end]==NULL && end!=0 ) {
	    paratext[end-1]->line_break_after = true;
	    start = end;
	}
	++cnt;
    }
    if ( cnt==0 ) cnt=1;
return( cnt );
}

static struct opentype_str **LineFromPara(struct opentype_str **str, int *_pos) {
    int len;
    struct opentype_str **ret;

    for ( len=0; str[len]!=NULL && !str[len]->line_break_after ; ++len );
    if ( str[len]!=NULL ) ++len;
    *_pos += len;
    ret = galloc((len+1)*sizeof(struct opentype_str *));
    for ( len=0; str[len]!=NULL && !str[len]->line_break_after ; ++len )
	ret[len] = str[len];
    if ( str[len]!=NULL ) {
	ret[len] = str[len];
	++len;
    }
    ret[len] = NULL;
return( ret );
}

static void SFFigureLineHeight(SFTextArea *st,int l,int p) {
    int i;
    struct opentype_str **line = st->lines[l];
    int as=0, ds=0;
    int width=0;

    for ( i=0; line[i]!=NULL; ++i ) {
	FontData *fd = ((struct fontlist *) (line[i]->fl))->fd;
	BDFFont *bdf = fd->bdf;
	if ( bdf!=NULL ) {
	    if ( as<bdf->ascent ) as = bdf->ascent;
	    if ( ds<bdf->descent ) ds = bdf->descent;
	} else {
	    double scale = fd->pointsize*st->dpi/(72.0*(fd->sf->ascent+fd->sf->descent));
	    if ( as<scale*fd->sf->ascent ) as = scale*fd->sf->ascent;
	    if ( ds<scale*fd->sf->descent ) ds = scale*fd->sf->descent;
	}
	width += line[i]->advance_width + line[i]->vr.h_adv_off;
    }
    if ( as+ds==0 ) {
	struct fontlist *fl, *last = st->fontlist;
	for ( fl=st->fontlist; fl!=NULL && st->paras[p].start_pos>=fl->start; fl=fl->next )
	    last = fl;
	if ( last!=NULL ) {
	    FontData *fd = last->fd;
	    double scale = fd->pointsize*st->dpi/(72.0*(fd->sf->ascent+fd->sf->descent));
	    if ( as<scale*fd->sf->ascent ) as = scale*fd->sf->ascent;
	    if ( ds<scale*fd->sf->descent ) ds = scale*fd->sf->descent;
	}
    }
    st->lineheights[l].fh = as+ds;
    st->lineheights[l].as = as;
    st->lineheights[l].linelen = width;
    if ( l==0 )
	st->lineheights[l].y = 0;
    else
	st->lineheights[l].y = st->lineheights[l-1].y+st->lineheights[l-1].fh;
    st->lineheights[l].p = p;
    if ( line[0]==NULL )			/* Before bidir text */
	st->lineheights[l].start_pos = st->paras[p].start_pos;
    else
	st->lineheights[l].start_pos = line[0]->orig_index +
		((struct fontlist *) (line[0]->fl))->start;
}

static void SFDoBiText(struct opentype_str **line) {
    int i, j, start, end, inr;
    /* I'm going to make a huge simplification. Instead of doing the unicode */
    /*  algorithem to determine whether a glyph should be r2l or l2r, I'm */
    /*  just going to assume that the script tells us that. Each glyph is */
    /*  tagged with a script because each fontlist is. So things are easy */

    inr = 0;
    for ( i=0; line[i]!=NULL; ++i ) {
	if ( ScriptIsRightToLeft( ((struct fontlist *) (line[i]->fl))->script )) {
	    if ( !inr ) {
		start = i;
		inr = true;
	    }
	} else {
	    if ( inr ) {
		end = i;
		inr = false;
		for ( j=(end-start)/2; j>0; --j ) {
		    struct opentype_str *chr = line[start+j-1];
		    line[start+j-1] = line[end-j];
		    line[end-j] = chr;
		}
		for ( j=start; j<end; ++j ) line[j]->r2l = true;
	    }
	}
    }
    if ( inr ) {
	end = i;
	inr = false;
	for ( j=(end-start)/2; j>0; --j ) {
	    struct opentype_str *chr = line[start+j-1];
	    line[start+j-1] = line[end-j];
	    line[end-j] = chr;
	}
	for ( j=start; j<end; ++j ) line[j]->r2l = true;
    }
}

static int ot_strlen(struct opentype_str *str) {
    int i;
    for ( i=0; str[i].sc!=NULL; ++i );
return( i );
}

static void SFTextAreaRefigureLines(SFTextArea *st, int start_of_change,
	int end_of_change) {
    int i,j, p,ps,pe, l,ls,le, pdiff, ldiff;
    int len, start, pcnt, lcnt;
    struct fontlist *fl, *oldstart, *oldend, *curp;
    double scale;

    if ( st->lines==NULL ) {
	st->lines = galloc(10*sizeof(struct opentype_str *));
	st->lineheights = galloc(10*sizeof(struct lineheights));
	st->lines[0] = NULL;
	st->lmax = 10;
	st->lcnt = 0;
	if ( st->vsb!=NULL )
	    GScrollBarSetBounds(&st->vsb->g,0,0,st->g.inner.height);
    }

    if ( end_of_change==-1 )
	end_of_change = start_of_change + u_strlen(st->text+start_of_change);
    if ( st->ps==-1 ) {
	ps = 0; pe = st->pcnt;
	ls = 0; le = st->lcnt;
	oldstart = st->fontlist;
	oldend = NULL;
    } else {
	ps = st->ps; pe = st->pe;
	ls = st->ls; le = st->le;
	oldstart = st->oldstart;
	oldend = st->oldend;
    }

    /* Do transformations dictated by features on the changed region */
    /* while we're at it find the beginning of the changed paragraph, the */
    /* number of paragraphs (newlines) within the change, and the last paragraph */
    /*  of the change */
    pcnt = 0;
    if ( oldstart!=NULL ) {
	for ( fl=oldstart, start = start_of_change-fl->start;
		fl!=NULL && fl!=oldend;
		fl=fl->next, start = 0 ) {
	    if ( start<0 ) start = 0;
	    if ( fl->end - fl->start >= fl->scmax )
		fl->sctext = grealloc(fl->sctext,((fl->scmax = fl->end-fl->start+4)+1)*sizeof(SplineChar *));
	    for ( i=j=0; i<fl->end-fl->start; ++i ) {
		SplineChar *sc = FDMap(fl->fd,st->text[fl->start+i]);
		if ( sc!=NULL && sc!=(SplineChar *) -1 )
		    fl->sctext[j++] = sc;
	    }
	    fl->sctext[j] = NULL;

	    free( fl->ottext );
	    fl->ottext = ApplyTickedFeatures(fl->fd->sf,fl->feats,
		    fl->script, fl->lang,
		    rint( (fl->fd->pointsize*st->dpi)/72 ),
		    fl->sctext);
	    scale = fl->fd->pointsize*st->dpi / (72.0*(fl->fd->sf->ascent+fl->fd->sf->descent));
	    for ( i=0; fl->ottext[i].sc!=NULL; ++i ) {
		fl->ottext[i].fl = fl;
		fl->ottext[i].advance_width = rint( fl->ottext[i].sc->width * scale );
	    }
	    if ( st->text[fl->end]=='\n' || st->text[fl->end]=='\0' )
		++pcnt;
	}
    }

    if ( st->pmax <= st->pcnt+pcnt - (pe-ps+1) )
	st->paras = grealloc(st->paras,(st->pmax = st->pcnt+30+pcnt-(pe-ps+1))*sizeof(struct paras));
    /* move any old paragraphs around */
    pdiff = pcnt-(pe-ps);
    for ( p=ps; p<pe; ++p )
	free(st->paras[p].para);
    if ( pdiff<0 ) {
	for ( p=pe; p<st->pcnt; ++p )
	    st->paras[p+pdiff] = st->paras[p];
    } else if ( pdiff>0 ) {
	for ( p=st->pcnt-1; p>=pe; --p )
	    st->paras[p+pdiff] = st->paras[p];
    }
    /* Figure out the changed paragraphs */
    /* And the number of lines in each */
    lcnt = 0;
    for ( p=ps, curp=oldstart; p<ps+pcnt && curp!=NULL; ++p ) {
	len = 0;
	/* Each para may be composed of several font segments */
	for ( fl=curp; fl!=NULL; fl=fl->next ) {
	    len += ot_strlen( fl->ottext );
	    if ( st->text[fl->end]=='\n' )
	break;		/* End of paragraph */
	}
	st->paras[p].para = galloc((len+1)*sizeof( struct paras));
	st->paras[p].start_pos = curp->start;
	len = 0;
	for ( fl=curp; fl!=NULL; fl=fl->next ) {
	    for ( i=0; fl->ottext[i].sc!=NULL; ++i )
		st->paras[p].para[len+i] = &fl->ottext[i];
	    len += i;
	    if ( st->text[fl->end]=='\n' ) {
		fl = fl->next;
	break;		/* End of paragraph */
	    }
	}
	st->paras[p].para[len] = NULL;
	lcnt += LinesInPara(st,st->paras[p].para);
	curp = fl;
    }
    st->pcnt += pdiff;

    if ( st->lmax <= st->lcnt+lcnt - (le-ls) + 1 ) {
	st->lines = grealloc(st->lines,(st->lmax = st->lcnt+30+lcnt-(le-ls+1))*sizeof(struct openfont_str **));
	st->lineheights = grealloc(st->lineheights,st->lmax*sizeof(struct lineheights));
    }
    /* move any old lines around */
    ldiff = lcnt-(le-ls);
    for ( l=ls; l<le; ++l )
	free(st->lines[l]);
    if ( ldiff<0 ) {
	for ( l=le; l<=st->lcnt; ++l ) {
	    st->lines[l+ldiff] = st->lines[l];
	    st->lineheights[l+ldiff] = st->lineheights[l];
	}
    } else if ( ldiff>0 ) {
	for ( l=st->lcnt-1; l>=le; --l ) {
	    st->lines[l+ldiff] = st->lines[l];
	    st->lineheights[l+ldiff] = st->lineheights[l];
	}
    }
    for ( l=ls, p=ps; l<ls+lcnt ; ++p ) {
	int eol=0;
	do {
	    st->lines[l] = LineFromPara(&st->paras[p].para[eol],&eol);
	    SFFigureLineHeight(st,l,p);	/* Must preceed BiText */
	    SFDoBiText(st->lines[l++]);
	} while ( st->paras[p].para[eol]!=NULL );
    }
    st->lcnt += ldiff;

    st->xmax = 0;
    for ( l=0; l<st->lcnt; ++l ) {
	if ( st->lineheights[l].linelen>st->xmax )
	    st->xmax = st->lineheights[l].linelen;
    }
    if ( ls+lcnt==0 )
	lcnt = 1;	/* line 0 always starts at 0 */
    for ( l=ls+lcnt; l<st->lcnt; ++l )
	st->lineheights[l].y = st->lineheights[l-1].y + st->lineheights[l-1].fh;
    if ( st->hsb!=NULL ) {
	GScrollBarSetBounds(&st->hsb->g,0,st->xmax,st->g.inner.width);
    }
    if ( st->vsb!=NULL && st->lcnt>0 ) {
	GScrollBarSetBounds(&st->vsb->g,0,st->lineheights[st->lcnt-1].y,st->g.inner.height);
    }
    st->ps = -1;
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void fontlistfree(struct fontlist *fl ) {
    struct fontlist *nfl;

    for ( ; fl!=NULL; fl=nfl ) {
	nfl = fl->next;
	free(fl->feats);
	free(fl->sctext);
	free(fl->ottext);
	chunkfree(fl,sizeof(struct fontlist));
    }
}

static void SFTextAreaChanged(SFTextArea *st,int src) {
    GEvent e;

    e.type = et_controlevent;
    e.w = st->g.base;
    e.u.control.subtype = et_textchanged;
    e.u.control.g = &st->g;
    e.u.control.u.tf_changed.from_pulldown = src;
    if ( st->g.handle_controlevent != NULL )
	(st->g.handle_controlevent)(&st->g,&e);
    else
	GDrawPostEvent(&e);
}

static void SFTextAreaFocusChanged(SFTextArea *st,int gained) {
    GEvent e;

    e.type = et_controlevent;
    e.w = st->g.base;
    e.u.control.subtype = et_textfocuschanged;
    e.u.control.g = &st->g;
    e.u.control.u.tf_focus.gained_focus = gained;
    if ( st->g.handle_controlevent != NULL )
	(st->g.handle_controlevent)(&st->g,&e);
    else
	GDrawPostEvent(&e);
}

static struct fontlist *fontlistcopy(struct fontlist *fl ) {
    struct fontlist *nfl, *nhead=NULL, *last=NULL;

    for ( ; fl!=NULL; fl=fl->next ) {
	nfl = chunkalloc(sizeof(struct fontlist));
	*nfl = *fl;
	nfl->feats = TagsCopy(fl->feats);
	nfl->scmax = 0; nfl->sctext = NULL; nfl->ottext = NULL;
	if ( nhead == NULL )
	    nhead = nfl;
	else
	    last->next = nfl;
	last = nfl;
    }
return( nhead );
}

static void fontlistcheck(SFTextArea *st) {
    struct fontlist *fl, *next;

    if ( st->fontlist==NULL )
return;
    for ( fl = st->fontlist; fl!=NULL; fl=next ) {
	next = fl->next;
	if ( next==NULL )
    break;
	/* fontlists should either be consecutive or allow for a line break to */
	/*  be between entries */
	if ( fl->start>fl->end || (fl->end!=next->start && fl->end!=next->start-1) ||
		next==fl || next->next==fl ) {
	    IError("FontList is corrupted" );
	    fl->next = NULL;
return;
	}
    }
}

static void fontlistmergecheck(SFTextArea *st) {
    struct fontlist *fl, *next;
    unichar_t *pt;

    if ( st->fontlist==NULL )
return;
    fontlistcheck(st);
    /* Make sure there is a new fontlist for each paragraph -- omit the newline */
    /*  char from the set of glyphs to be displayed */
    for ( pt=st->text, fl = st->fontlist; *pt; ++pt ) {
	if ( *pt=='\n' ) {
	    while ( fl!=NULL && fl->end<=pt-st->text ) fl=fl->next;
	    if ( fl==NULL )
    break;
	    if ( fl->start<=pt-st->text ) {
		if ( fl->next!=NULL && fl->next->start == pt+1-st->text )
		    fl->end = pt-st->text;
		else {
		    next = chunkalloc(sizeof(struct fontlist));
		    *next = *fl;
		    fl->next = next;
		    fl->end = pt-st->text;
		    next->scmax = 0; next->sctext = NULL; next->ottext = NULL;
		    next->feats = TagsCopy(fl->feats);
		    next->start = pt+1-st->text;
		}
	    }
	}
    }
    fontlistcheck(st);
    /* Now join adjacent fontlists with the same properties (except don't merge*/
    /*  over line breaks */
    for ( fl = st->fontlist; fl!=NULL; fl=next ) {
	for ( next=fl->next; next!=NULL && next->fd==fl->fd &&
		st->text[fl->end]!='\n' &&
		next->lang==fl->lang && next->script==fl->script &&
		TagsSame(next->feats,fl->feats); next = fl->next ) {
	    if ( st->oldstart==next )
		st->oldstart = fl;
	    if ( st->oldend == next )
		st->oldend = next->next;
	    fl->next = next->next;
	    fl->end = next->end;
	    free(next->feats);
	    free(next->ottext); free(next->sctext);
	    chunkfree(next,sizeof(struct fontlist));
	}
    }
    fontlistcheck(st);
}

static void SFTextAreaChangeFontList(SFTextArea *st,int rpllen) {
    /* we are removing a chunk starting at st->sel_start going to st->sel_end */
    /*  and replacing it with a chunk that is rpllen long */
    /* So we remove any chunks wholy within sel_start,sel_end and extend the */
    /*  chunk at sel_start by rpllen */
    struct fontlist *fl, *next, *test;
    int diff;
    int ps,pe,ls,le, p,l;
    struct fontlist *oldstart, *oldend;

    fontlistfree(st->oldfontlist);
    st->oldfontlist = fontlistcopy(st->fontlist);

    diff = rpllen - (st->sel_end-st->sel_start);

    ps = 0; pe = st->pcnt;
    ls = 0; le = st->lcnt;
    p = l = 0;
    oldstart = st->fontlist;
    for ( fl = st->fontlist; fl!=NULL && fl->start<=st->sel_start; fl=fl->next ) {
	if ( fl->next!=NULL && fl->end!=fl->next->start && fl->next->start<=st->sel_start ) {
	    oldstart = fl->next;
	}
    }
    if ( st->paras!=NULL && oldstart!=NULL ) {
	while ( p<st->pcnt && st->paras[p].start_pos!=oldstart->start )
	    ++p;
	if ( p<st->pcnt ) {
	    ps = p;
	    while ( l<st->lcnt && st->lineheights[l].start_pos != oldstart->start )
		++l;
	    if ( l<st->lcnt )
		ls = l;
	}
    }

    for ( fl = oldstart; fl!=NULL && st->sel_end>=fl->start ; fl=fl->next );
    oldend = fl;
    while ( oldend!=NULL && st->text[oldend->start-1]!='\n' )
	oldend = oldend->next;
    if ( oldend!=NULL && st->paras!=NULL ) {
	while ( p<st->pcnt && st->paras[p].start_pos!=oldend->start )
	    ++p;
	if ( p<st->pcnt ) {
	    pe = p;
	    while ( l<st->lcnt && st->lineheights[l].start_pos != oldend->start )
		++l;
	    if ( l<st->lcnt )
		le = l;
	    for ( ; p<st->pcnt; ++p )
		st->paras[p].start_pos += diff;
	    for ( ; l<st->lcnt; ++l )
		st->lineheights[l].start_pos += diff;
	}
    }
    st->ps = ps; st->pe = pe;
    st->ls = ls; st->le = le;
    st->oldstart = oldstart;
    st->oldend = oldend;

    for ( fl=st->fontlist; fl!=NULL && st->sel_start>fl->end; fl=fl->next );
    if ( fl==NULL )
return;
    if ( fl->end>=st->sel_end ) {
	fl->end += diff;
	fl = fl->next;
    } else {
	fl->end = st->sel_start + rpllen;
	for ( test=fl->next; test!=NULL && st->sel_end>=test->end; test=next ) {
	    next = test->next;
	    free(test->feats);
	    free(test->sctext); free(test->ottext);
	    chunkfree(test,sizeof(struct fontlist));
	}
	fl->next = test;
	if ( test!=NULL ) {
	    test->start = fl->end;
	    test->end += diff;
	    fl = test->next;
	} else
	    fl = NULL;
    }
    while ( fl!=NULL ) {
	fl->start += diff;
	fl->end += diff;
	fl = fl->next;
    }
}

static void _SFTextAreaReplace(SFTextArea *st, const unichar_t *str) {
    unichar_t *old = st->oldtext;
    int rpllen = u_strlen(str);
    unichar_t *new = galloc((u_strlen(st->text)-(st->sel_end-st->sel_start) + rpllen+1)*sizeof(unichar_t));

    st->oldtext = st->text;
    st->sel_oldstart = st->sel_start;
    st->sel_oldend = st->sel_end;
    st->sel_oldbase = st->sel_base;
    SFTextAreaChangeFontList(st,rpllen);

    u_strncpy(new,st->text,st->sel_start);
    u_strcpy(new+st->sel_start,str);
    st->sel_start += rpllen;
    u_strcpy(new+st->sel_start,st->text+st->sel_end);
    st->text = new;
    st->sel_end = st->sel_base = st->sel_start;
    free(old);

    fontlistmergecheck(st);
    SFTextAreaRefigureLines(st,st->sel_oldstart,st->sel_oldstart+rpllen);
}

static void SFTextArea_Replace(SFTextArea *st, const unichar_t *str) {
    _SFTextAreaReplace(st,str);
    SFTextArea_Show(st,st->sel_start);
}

static int SFTextAreaFindLine(SFTextArea *st, int pos) {
    int i;

    for ( i=0; i+1<st->lcnt; ++i )
	if ( pos<st->lineheights[i+1].start_pos )
    break;

return( i );
}

static int PSTComponentCount(PST *pst) {
    int cnt=0;
    char *pt = pst->u.lig.components;

    forever {
	while ( *pt==' ' ) ++pt;
	if ( *pt=='\0' )
return( cnt );
	while ( *pt!=' ' && *pt!='\0' ) ++pt;
	++cnt;
    }
}

static int PSTLigComponentCount(SplineChar *sc) {
    PST *pst;
    int lcnt, ltemp;

    lcnt = 0;
    for ( pst=sc->possub; pst!=NULL ; pst=pst->next ) {
	/* Find out the number of components. Note that ffi might be f+f+i or ff+i */
	/*  so find the max */
	if ( pst->type==pst_ligature ) {
	    ltemp = PSTComponentCount(pst);
	    if ( ltemp>lcnt )
		lcnt = ltemp;
	}
    }
return( lcnt );
}

static int SFTextAreaGetOffsetFromXPos(SFTextArea *st,int i,int xpos) {
    int p, x, xend, pos, j, l, r2l, lcnt;
    struct opentype_str **line;
    PST *pst;

    if ( i<0 )
return( 0 );
    if ( i>=st->lcnt )
return( u_strlen(st->text));

    p = st->lineheights[i].p;
    if ( st->paras[p].para[0]!=NULL &&
	    ScriptIsRightToLeft( ((struct fontlist *) (st->paras[p].para[0]->fl))->script )) {
	x = st->xmax - st->lineheights[i].linelen;
	r2l = true;
    } else {
	x = 0;
	r2l = false;
    }

    line = st->lines[i];
    if ( line[0]==NULL ) {
	pos = st->lineheights[i].start_pos;
    } else {
	for ( j=0; line[j]!=NULL; ++j ) {
	    xend = x + line[j]->advance_width + line[j]->vr.h_adv_off;
	    if ( xpos<xend ) {
		double scale;
		xpos -= x; xend -= x;
		/* Check for ligature carets */
		for ( pst=line[j]->sc->possub; pst!=NULL && pst->type!=pst_lcaret; pst=pst->next );
		if ( pst!=NULL && pst->u.lcaret.cnt==0 )
		    pst = NULL;
		lcnt = 0;
		if ( pst==NULL ) {
		    lcnt = PSTLigComponentCount(line[j]->sc);
		    if ( lcnt<=1 ) {
			if ( xpos>xend/2 )
			    ++j;
		    } else {
			if ( line[j+1]!=NULL && xpos>(2*lcnt-1)*xend/lcnt ) {
			    ++j;
			    lcnt = 0;
			}
		    }
		} else {
		    FontData *fd = ((struct fontlist *) (line[j]->fl))->fd;
		    scale = fd->pointsize*st->dpi / (72.0*(fd->sf->ascent+fd->sf->descent));
		    if ( xpos-pst->u.lcaret.carets[ pst->u.lcaret.cnt-1 ]*scale > xend-xpos ) {
			++j;
			pst = NULL;
		    }
		}
		if ( line[j]==NULL ) {
		    pos = line[j-1]->orig_index + ((struct fontlist *) (line[j-1]->fl))->start +1;
		} else
		    pos = line[j]->orig_index + ((struct fontlist *) (line[j]->fl))->start;
		if ( pst!=NULL ) {
		    for ( l=0; l<pst->u.lcaret.cnt; ++l )
			if (( l==0 && xpos < scale*pst->u.lcaret.carets[0]-xpos ) ||
				(l!=0 && xpos-scale*pst->u.lcaret.carets[l-1] < scale*pst->u.lcaret.carets[0]-xpos ))
		    break;
		    if ( r2l )
			l = pst->u.lcaret.cnt-l;
		    pos += l;
		} else if ( lcnt>=2 ) {
		    /* Ok it's a ligature with lcnt components. Assume each has the */
		    /*  length */
		    l = (xpos+xend/(2*lcnt))/(xend/lcnt);
		    if ( r2l )
			l = lcnt-1-l;
		    pos += l;
		}
	break;
	    }
	    x = xend;
	}
	if ( line[j]==NULL )
	    pos = line[j-1]->orig_index + ((struct fontlist *) (line[j-1]->fl))->start +1;
    }
return( pos );
}

static int SFTextAreaGetXPosFromOffset(SFTextArea *st,int l,int pos) {
    int j, scpos, lcnt, x;
    struct opentype_str **line;
    PST *pst;

    if ( l<0 || l>= st->lcnt )
return( 0 );
    if ( st->lines[0]==NULL || pos < st->lineheights[l].start_pos )
return( 0 );

    line = st->lines[l];
    x = 0;
    for ( j=0; line[j]!=NULL; ++j ) {
	scpos = line[j]->orig_index + ((struct fontlist *) (line[j]->fl))->start;
	if ( scpos==pos )
return( x );
	for ( pst=line[j]->sc->possub; pst!=NULL && pst->type!=pst_lcaret; pst=pst->next );
	if ( pst!=NULL && pst->u.lcaret.cnt==0 )
	    pst = NULL;
	if ( pst!=NULL && pos>scpos && pos<=scpos+pst->u.lcaret.cnt ) {
	    FontData *fd = ((struct fontlist *) (line[j]->fl))->fd;
	    double scale = fd->pointsize*st->dpi / (72.0*(fd->sf->ascent+fd->sf->descent));
return( x + rint(scale*pst->u.lcaret.carets[pos-scpos-1]) );
	}
	x += line[j]->advance_width + line[j]->vr.h_adv_off;
    }
    /* Ok, maybe they didn't specify lig carets. Check if we are within a ligature */
    x=0;
    for ( j=0; line[j]!=NULL; ++j ) {
	scpos = line[j]->orig_index + ((struct fontlist *) (line[j]->fl))->start;
	if ( scpos==pos )
return( x );
	lcnt = PSTLigComponentCount(line[j]->sc);
	if ( pos>scpos && pos<scpos+lcnt ) {
	    int wid = line[j]->advance_width + line[j]->vr.h_adv_off;
return( x + (pos-scpos)*wid/lcnt );
	}
	x += line[j]->advance_width + line[j]->vr.h_adv_off;
    }
return( x );
}

static int SFTextArea_EndPage(SFTextArea *st) {
    int endpage;

    for ( endpage=1; st->lcnt-endpage>=0 && st->lineheights[st->lcnt-1].y-st->lineheights[st->lcnt-endpage].y<=st->g.inner.height;
	    ++endpage );
    if ( (endpage-=2) < 1 ) endpage = 1;
return( endpage );
}

static int SFTextArea_Show(SFTextArea *st, int pos) {
    int i, xoff, loff, x, xlen;
    int refresh=false, endpage, page;

    if ( pos < 0 ) pos = 0;
    if ( pos > u_strlen(st->text)) pos = u_strlen(st->text);
    i = SFTextAreaFindLine(st,pos);

    loff = st->loff_top;
    for ( page=1; st->loff_top+page<st->lcnt && st->lineheights[st->loff_top+page].y-st->lineheights[st->loff_top].y<=st->g.inner.height;
	    ++page );
    if ( --page < 1 ) page = 1;
    /* a page starting at loff_top may have a different number of lines than */
    /*  a page ending at lcnt */
    endpage = SFTextArea_EndPage(st);
    if ( i<loff || i>=st->loff_top+page)
	loff = i-page/4;
    if ( loff > st->lcnt-endpage )
	loff = st->lcnt-endpage;
    if ( loff<0 ) loff = 0;
    if ( st->lcnt==0 || st->lineheights[st->lcnt-1].y<st->g.inner.height )
	loff = 0;

    xoff = st->xoff_left;
    x = 0;
    if ( i<st->lcnt ) {
	x = SFTextAreaGetXPosFromOffset(st,i,pos);
	xlen = st->lineheights[i].linelen;
	if ( xlen< st->g.inner.width )
	    xoff = 0;
	else if ( x<xoff+4 || x>=xoff+st->g.inner.width-4 ) {
	    xoff = x - xlen/4;
	    if ( xoff<0 ) xoff = 0;
	}
    }

    if ( xoff!=st->xoff_left ) {
	st->xoff_left = xoff;
	if ( st->hsb!=NULL )
	    GScrollBarSetPos(&st->hsb->g,xoff);
	refresh = true;
    }
    if ( loff!=st->loff_top ) {
	st->loff_top = loff;
	if ( st->vsb!=NULL )
	    GScrollBarSetPos(&st->vsb->g,st->lineheights[loff].y);
	refresh = true;
    }
    GTPositionGIC(st);
return( refresh );
}

static void *genunicodedata(void *_gt,int32 *len) {
    SFTextArea *st = _gt;
    unichar_t *temp;
    *len = st->sel_end-st->sel_start + 1;
    temp = galloc((*len+2)*sizeof(unichar_t));
    temp[0] = 0xfeff;		/* KDE expects a byte order flag */
    u_strncpy(temp+1,st->text+st->sel_start,st->sel_end-st->sel_start);
    temp[*len+1] = 0;
return( temp );
}

static void *genutf8data(void *_gt,int32 *len) {
    SFTextArea *st = _gt;
    unichar_t *temp =u_copyn(st->text+st->sel_start,st->sel_end-st->sel_start);
    char *ret = u2utf8_copy(temp);
    free(temp);
    *len = strlen(ret);
return( ret );
}

static void *ddgenunicodedata(void *_gt,int32 *len) {
    void *temp = genunicodedata(_gt,len);
    SFTextArea *st = _gt;
    _SFTextAreaReplace(st,nullstr);
    _ggadget_redraw(&st->g);
return( temp );
}

static void *genlocaldata(void *_gt,int32 *len) {
    SFTextArea *st = _gt;
    unichar_t *temp =u_copyn(st->text+st->sel_start,st->sel_end-st->sel_start);
    char *ret = u2def_copy(temp);
    free(temp);
    *len = strlen(ret);
return( ret );
}

static void *ddgenlocaldata(void *_gt,int32 *len) {
    void *temp = genlocaldata(_gt,len);
    SFTextArea *st = _gt;
    _SFTextAreaReplace(st,nullstr);
    _ggadget_redraw(&st->g);
return( temp );
}

static void noop(void *_st) {
}

static void SFTextAreaGrabPrimarySelection(SFTextArea *st) {
    int ss = st->sel_start, se = st->sel_end;

    GDrawGrabSelection(st->g.base,sn_primary);
    st->sel_start = ss; st->sel_end = se;
#ifdef UNICHAR_16
    GDrawAddSelectionType(st->g.base,sn_primary,"text/plain;charset=ISO-10646-UCS-2",gt,st->sel_end-st->sel_start,
	    sizeof(unichar_t),
	    genunicodedata,noop);
#else
    GDrawAddSelectionType(st->g.base,sn_primary,"text/plain;charset=ISO-10646-UCS-4",st,st->sel_end-st->sel_start,
	    sizeof(unichar_t),
	    genunicodedata,noop);
#endif
    GDrawAddSelectionType(st->g.base,sn_primary,"UTF8_STRING",st,3*(st->sel_end-st->sel_start),
	    sizeof(unichar_t),
	    genutf8data,noop);
    GDrawAddSelectionType(st->g.base,sn_primary,"STRING",st,st->sel_end-st->sel_start,sizeof(char),
	    genlocaldata,noop);
}

static void SFTextAreaGrabDDSelection(SFTextArea *st) {

    GDrawGrabSelection(st->g.base,sn_drag_and_drop);
#ifdef UNICHAR_16
    GDrawAddSelectionType(st->g.base,sn_drag_and_drop,"text/plain;charset=ISO-10646-UCS-2",st,st->sel_end-st->sel_start,
	    sizeof(unichar_t),
	    ddgenunicodedata,noop);
#else
    GDrawAddSelectionType(st->g.base,sn_drag_and_drop,"text/plain;charset=ISO-10646-UCS-4",st,st->sel_end-st->sel_start,
	    sizeof(unichar_t),
	    ddgenunicodedata,noop);
#endif
    GDrawAddSelectionType(st->g.base,sn_drag_and_drop,"STRING",st,st->sel_end-st->sel_start,sizeof(char),
	    ddgenlocaldata,noop);
}

static void SFTextAreaGrabSelection(SFTextArea *st, enum selnames sel ) {

    if ( st->sel_start!=st->sel_end ) {
	unichar_t *temp;
	char *ctemp;
#ifndef UNICHAR_16
	int i;
	uint16 *u2temp;
#endif

	GDrawGrabSelection(st->g.base,sel);
	temp = galloc((st->sel_end-st->sel_start + 2)*sizeof(unichar_t));
	temp[0] = 0xfeff;		/* KDE expects a byte order flag */
	u_strncpy(temp+1,st->text+st->sel_start,st->sel_end-st->sel_start);
	ctemp = u2utf8_copy(temp);
#ifdef UNICHAR_16
	GDrawAddSelectionType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2",temp,u_strlen(temp),
		sizeof(unichar_t),
		NULL,NULL);
#else
	GDrawAddSelectionType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-4",temp,u_strlen(temp),
		sizeof(unichar_t),
		NULL,NULL);
	u2temp = galloc((st->sel_end-st->sel_start + 2)*sizeof(uint16));
	for ( i=0; temp[i]!=0; ++i )
	    u2temp[i] = temp[i];
	u2temp[i] = 0;
	GDrawAddSelectionType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2",u2temp,u_strlen(temp),
		2,
		NULL,NULL);
#endif
	GDrawAddSelectionType(st->g.base,sel,"UTF8_STRING",ctemp,strlen(ctemp),sizeof(char),
		NULL,NULL);
	GDrawAddSelectionType(st->g.base,sel,"STRING",u2def_copy(temp),u_strlen(temp),sizeof(char),
		NULL,NULL);
    }
}

static int SFTextAreaSelBackword(unichar_t *text,int start) {
    unichar_t ch;

    if ( start==0 )
return( 0 ); /* Can't go back */;

    ch = text[start-1];
    if ( isalnum(ch) || ch=='_' ) {
	int i;
	for ( i=start-1; i>=0 && (isalnum(text[i]) || text[i]=='_') ; --i );
	start = i+1;
    } else {
	int i;
	for ( i=start-1; i>=0 && !isalnum(text[i]) && text[i]!='_' ; --i );
	start = i+1;
    }
return( start );
}

static int SFTextAreaSelForeword(unichar_t *text,int end) {
    unichar_t ch = text[end];

    if ( ch=='\0' )
	/* Nothing */;
    else if ( isalnum(ch) || ch=='_' ) {
	int i;
	for ( i=end; isalnum(text[i]) || text[i]=='_' ; ++i );
	end = i;
    } else {
	int i;
	for ( i=end; !isalnum(text[i]) && text[i]!='_' && text[i]!='\0' ; ++i );
	end = i;
    }
return( end );
}

static void SFTextAreaSelectWord(SFTextArea *st,int mid, int16 *start, int16 *end) {
    unichar_t *text = st->text;
    unichar_t ch = text[mid];

    if ( ch=='\0' )
	*start = *end = mid;
    else if ( isspace(ch) ) {
	int i;
	for ( i=mid; isspace(text[i]); ++i );
	*end = i;
	for ( i=mid-1; i>=0 && isspace(text[i]) ; --i );
	*start = i+1;
    } else if ( isalnum(ch) || ch=='_' ) {
	int i;
	for ( i=mid; isalnum(text[i]) || text[i]=='_' ; ++i );
	*end = i;
	for ( i=mid-1; i>=0 && (isalnum(text[i]) || text[i]=='_') ; --i );
	*start = i+1;
    } else {
	int i;
	for ( i=mid; !isalnum(text[i]) && text[i]!='_' && text[i]!='\0' ; ++i );
	*end = i;
	for ( i=mid-1; i>=0 && !isalnum(text[i]) && text[i]!='_' ; --i );
	*start = i+1;
    }
}

static void SFTextAreaSelectWords(SFTextArea *st,int last) {
    int16 ss, se;
    SFTextAreaSelectWord(st,st->sel_base,&st->sel_start,&st->sel_end);
    if ( last!=st->sel_base ) {
	SFTextAreaSelectWord(st,last,&ss,&se);
	if ( ss<st->sel_start ) st->sel_start = ss;
	if ( se>st->sel_end ) st->sel_end = se;
    }
}

static void SFTextAreaPaste(SFTextArea *st,enum selnames sel) {
#ifdef UNICHAR_16
    if ( GDrawSelectionHasType(st->g.base,sel,"Unicode") ||
	    GDrawSelectionHasType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2")) {
	unichar_t *temp;
	int32 len;
	temp = GDrawRequestSelection(st->g.base,sel,"Unicode",&len);
	if ( temp==NULL || len==0 )
	    temp = GDrawRequestSelection(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2",&len);
	/* Bug! I don't handle byte reversed selections. But I don't think there should be any anyway... */
	if ( temp!=NULL )
	    GTextField_Replace(gt,temp[0]==0xfeff?temp+1:temp);
	free(temp);
#else
    if ( GDrawSelectionHasType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-4")) {
	unichar_t *temp;
	int32 len;
	temp = GDrawRequestSelection(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-4",&len);
	/* Bug! I don't handle byte reversed selections. But I don't think there should be any anyway... */
	if ( temp!=NULL )
	    SFTextArea_Replace(st,temp[0]==0xfeff?temp+1:temp);
	free(temp);
    } else if ( GDrawSelectionHasType(st->g.base,sel,"Unicode") ||
	    GDrawSelectionHasType(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2")) {
	unichar_t *temp;
	uint16 *temp2;
	int32 len;
	temp2 = GDrawRequestSelection(st->g.base,sel,"text/plain;charset=ISO-10646-UCS-2",&len);
	if ( temp2==NULL || len==0 )
	    temp2 = GDrawRequestSelection(st->g.base,sel,"Unicode",&len);
	if ( temp2!=NULL ) {
	    int i;
	    temp = galloc((len/2+1)*sizeof(unichar_t));
	    for ( i=0; temp2[i]!=0; ++i )
		temp[i] = temp2[i];
	    temp[i] = 0;
	    SFTextArea_Replace(st,temp[0]==0xfeff?temp+1:temp);
	    free(temp);
	}
	free(temp2);
#endif
    } else if ( GDrawSelectionHasType(st->g.base,sel,"UTF8_STRING") ||
	    GDrawSelectionHasType(st->g.base,sel,"text/plain;charset=UTF-8")) {
	unichar_t *temp; char *ctemp;
	int32 len;
	if ( GDrawSelectionHasType(st->g.base,sel,"UTF8_STRING") )
	    ctemp = GDrawRequestSelection(st->g.base,sel,"UTF8_STRING",&len);
	else
	    ctemp = GDrawRequestSelection(st->g.base,sel,"text/plain;charset=UTF-8",&len);
	if ( ctemp!=NULL ) {
	    temp = utf82u_copyn(ctemp,strlen(ctemp));
	    SFTextArea_Replace(st,temp);
	    free(ctemp); free(temp);
	}
    } else if ( GDrawSelectionHasType(st->g.base,sel,"STRING")) {
	unichar_t *temp; char *ctemp;
	int32 len;
	ctemp = GDrawRequestSelection(st->g.base,sel,"STRING",&len);
	if ( ctemp!=NULL ) {
	    temp = def2u_copy(ctemp);
	    SFTextArea_Replace(st,temp);
	    free(ctemp); free(temp);
	}
    }
}

static int sftextarea_editcmd(GGadget *g,enum editor_commands cmd) {
    SFTextArea *st = (SFTextArea *) g;
    int i;

    switch ( cmd ) {
      case ec_selectall:
	st->sel_start = 0;
	st->sel_end = u_strlen(st->text);
return( true );
      case ec_clear:
	SFTextArea_Replace(st,nullstr);
return( true );
      case ec_cut:
	SFTextAreaGrabSelection(st,sn_clipboard);
	SFTextArea_Replace(st,nullstr);
return( true );
      case ec_copy:
	SFTextAreaGrabSelection(st,sn_clipboard);
return( true );
      case ec_paste:
	SFTextAreaPaste(st,sn_clipboard);
	SFTextArea_Show(st,st->sel_start);
return( true );
      case ec_undo:
	if ( st->oldtext!=NULL ) {
	    unichar_t *temp = st->text;
	    struct fontlist *ofl = st->fontlist;
	    int16 s;
	    st->text = st->oldtext; st->oldtext = temp;
	    st->fontlist = st->oldfontlist; st->oldfontlist = ofl;
	    s = st->sel_start; st->sel_start = st->sel_oldstart; st->sel_oldstart = s;
	    s = st->sel_end; st->sel_end = st->sel_oldend; st->sel_oldend = s;
	    s = st->sel_base; st->sel_base = st->sel_oldbase; st->sel_oldbase = s;
	    for ( i=0; i<st->pcnt; ++i )
		free( st->paras[i].para);
	    free(st->paras); st->paras = NULL; st->pcnt = 0;
	    for ( i=0; i<st->lcnt; ++i )
		free( st->lines[i]);
	    free( st->lines );
	    free( st->lineheights );
	    st->lines = NULL; st->lineheights = NULL; st->lcnt = 0;
	    SFTextAreaRefigureLines(st, 0, -1);
	    SFTextArea_Show(st,st->sel_end);
	}
return( true );
      case ec_redo:		/* Hmm. not sure */ /* we don't do anything */
return( true );			/* but probably best to return success */
      case ec_backword:
        if ( st->sel_start==st->sel_end && st->sel_start!=0 ) {
	    st->sel_start = SFTextAreaSelBackword(st->text,st->sel_start);
	}
	SFTextArea_Replace(st,nullstr);
return( true );
      case ec_deleteword:
        if ( st->sel_start==st->sel_end && st->sel_start!=0 )
	    SFTextAreaSelectWord(st,st->sel_start,&st->sel_start,&st->sel_end);
	SFTextArea_Replace(st,nullstr);
return( true );
    }
return( false );
}

static int _sftextarea_editcmd(GGadget *g,enum editor_commands cmd) {
    if ( sftextarea_editcmd(g,cmd)) {
	_ggadget_redraw(g);
	GTPositionGIC((SFTextArea *) g);
return( true );
    }
return( false );
}

static int GTBackPos(SFTextArea *st,int pos, int ismeta) {
    int newpos/*, xloc, l*/;

    if ( ismeta )
	newpos = SFTextAreaSelBackword(st->text,pos);
    else
	newpos = pos-1;
#if 0		/* Why did I think this mattered? */
    /* Best just to step over each ligature component even if the cursor */
    /*  stays put. Or that's what I think today */

    /* There are some cases (a ligature of a ligature perhaps) where we can't */
    /*  show a difference between pos and pos-1, so keep subtracting until we */
    /*  can */
    l = SFTextAreaFindLine(st,pos);
    xloc = SFTextAreaGetXPosFromOffset(st,l,pos);
    while ( newpos!=-1 &&
	    SFTextAreaFindLine(st,newpos) == l &&
	    xloc == SFTextAreaGetXPosFromOffset(st,l,newpos) )
	--newpos;
#endif
    if ( newpos==-1 ) newpos = 0;
return( newpos );
}

static int GTForePos(SFTextArea *st,int pos, int ismeta) {
    int newpos=pos/*, xloc, l*/;

    if ( ismeta )
	newpos = SFTextAreaSelForeword(st->text,pos);
    else {
	if ( st->text[pos]!=0 )
	    newpos = pos+1;
    }
#if 0		/* Why did I think this mattered? */
    /* Best just to step over each ligature component even if the cursor */
    /*  stays put. Or that's what I think today */

    /* There are some cases (a ligature of a ligature perhaps) where we can't */
    /*  show a difference between pos and pos-1, so keep subtracting until we */
    /*  can */
    l = SFTextAreaFindLine(st,pos);
    xloc = SFTextAreaGetXPosFromOffset(st,l,pos);
    while ( st->text[newpos]!=0 &&
	    SFTextAreaFindLine(st,newpos) == l &&
	    xloc == SFTextAreaGetXPosFromOffset(st,l,newpos) )
	++newpos;
#endif
return( newpos );
}

static void SFTextAreaImport(SFTextArea *st) {
    char *cret = gwwv_open_filename(_("Open"),NULL,
	    "*.txt",NULL);
    unichar_t *str;

    if ( cret==NULL )
return;
    str = _GGadgetFileToUString(cret,65536);
    if ( str==NULL ) {
	gwwv_post_error(_("Could not open"),_("Could not open %.100s"),cret);
	free(cret);
return;
    }
    free(cret);
    SFTextArea_Replace(st,str);
    free(str);
}

static void SFTextAreaSave(SFTextArea *st) {
    char *cret = gwwv_save_filename(_("Save"),NULL, "*.txt");
    FILE *file;
    unichar_t *pt;

    if ( cret==NULL )
return;
    file = fopen(cret,"w");
    if ( file==NULL ) {
	gwwv_post_error(_("Could not open"),_("Could not open %.100s"),cret);
	free(cret);
return;
    }
    free(cret);

	putc(0xef,file);		/* Zero width something or other. Marks this as unicode, utf8 */
	putc(0xbb,file);
	putc(0xbf,file);
	for ( pt = st->text ; *pt; ++pt ) {
	    if ( *pt<0x80 )
		putc(*pt,file);
	    else if ( *pt<0x800 ) {
		putc(0xc0 | (*pt>>6), file);
		putc(0x80 | (*pt&0x3f), file);
	    } else if ( *pt>=0xd800 && *pt<0xdc00 && pt[1]>=0xdc00 && pt[1]<0xe000 ) {
		int u = ((*pt>>6)&0xf)+1, y = ((*pt&3)<<4) | ((pt[1]>>6)&0xf);
		putc( 0xf0 | (u>>2),file );
		putc( 0x80 | ((u&3)<<4) | ((*pt>>2)&0xf),file );
		putc( 0x80 | y,file );
		putc( 0x80 | (pt[1]&0x3f),file );
	    } else {
		putc( 0xe0 | (*pt>>12),file );
		putc( 0x80 | ((*pt>>6)&0x3f),file );
		putc( 0x80 | (*pt&0x3f),file );
	    }
	}
    fclose(file);
}

static void SFTextAreaSaveImage(SFTextArea *st) {
    char *cret;
    GImage *image;
    struct _GImage *base;
    char *basename;
    int i,ret, p, x, j;
    struct opentype_str **line;

    if ( st->lcnt==0 )
return;

    basename = NULL;
    if ( st->fontlist!=NULL ) {
	basename = galloc(strlen(st->fontlist->fd->sf->fontname)+8);
	strcpy(basename, st->fontlist->fd->sf->fontname);
#ifdef _NO_LIBPNG
	strcat(basename,".bmp");
#else
	strcat(basename,".png");
#endif
    }
#ifdef _NO_LIBPNG
    cret = gwwv_save_filename(_("Save Image"),basename, "*.bmp");
#else
    cret = gwwv_save_filename(_("Save Image"),basename, "*.{bmp,png}");
#endif
    free(basename);
    if ( cret==NULL )
return;

    image = GImageCreate(it_index,st->g.inner.width+2,
	    st->lineheights[st->lcnt-1].y+st->lineheights[st->lcnt-1].fh+2);
    base = image->u.image;
    memset(base->data,0,base->bytes_per_line*base->height);
    for ( i=0; i<256; ++i )
	base->clut->clut[i] = (255-i)*0x010101;
    base->clut->is_grey = true;
    base->clut->clut_len = 256;

    for ( i=0; i<st->lcnt; ++i ) {
	/* Does this para start out r2l or l2r? */
	p = st->lineheights[i].p;
	if ( st->paras[p].para[0]!=NULL &&
		ScriptIsRightToLeft( ((struct fontlist *) (st->paras[p].para[0]->fl))->script ))
	    x = st->xmax - st->lineheights[i].linelen;
	else
	    x = 0;
	line = st->lines[i];
	for ( j=0; line[j]!=NULL; ++j ) {
	    FDDrawChar(NULL,image,line[j],x,st->lineheights[i].y,0x000000);
	    x += line[j]->advance_width + line[j]->vr.h_adv_off;
	}
    }
#ifndef _NO_LIBPNG
    if ( strstrmatch(cret,".png")!=NULL )
	ret = GImageWritePng(image,cret,false);
    else
#endif
    if ( strstrmatch(cret,".bmp")!=NULL )
	ret = GImageWriteBmp(image,cret);
    else
	gwwv_post_error(_("Unsupported image format"),
#ifndef _NO_LIBPNG
		_("Unsupported image format must be bmp or png")
#else
		_("Unsupported image format must be bmp")
#endif
	    );
    if ( !ret )
	gwwv_post_error(_("Could not write"),_("Could not write %.100s"),cret);
    free( cret );
    GImageDestroy(image);
}

#define MID_Cut		1
#define MID_Copy	2
#define MID_Paste	3

#define MID_SelectAll	4

#define MID_Save	5
#define MID_Import	6

#define MID_Undo	7

#define MID_SaveImage	8

static SFTextArea *popup_kludge;

static void SFTFPopupInvoked(GWindow v, GMenuItem *mi,GEvent *e) {
    SFTextArea *st;
    if ( popup_kludge==NULL )
return;
    st = popup_kludge;
    popup_kludge = NULL;
    switch ( mi->mid ) {
      case MID_Undo:
	sftextarea_editcmd(&st->g,ec_undo);
      break;
      case MID_Cut:
	sftextarea_editcmd(&st->g,ec_cut);
      break;
      case MID_Copy:
	sftextarea_editcmd(&st->g,ec_copy);
      break;
      case MID_Paste:
	sftextarea_editcmd(&st->g,ec_paste);
      break;
      case MID_SelectAll:
	sftextarea_editcmd(&st->g,ec_selectall);
      break;
      case MID_Save:
	SFTextAreaSave(st);
      break;
      case MID_Import:
	SFTextAreaImport(st);
      break;
      case MID_SaveImage:
	SFTextAreaSaveImage(st);
      break;
    }
}

static GMenuItem sftf_popuplist[] = {
    { { (unichar_t *) N_("_Undo"), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'U' }, 'Z', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Undo },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) N_("Cu_t"), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 't' }, 'X', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Cut },
    { { (unichar_t *) N_("_Copy"), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'C' }, 'C', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Copy },
    { { (unichar_t *) N_("_Paste"), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'P' }, 'V', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Paste },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) N_("_Save As..."), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'S' }, 'S', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Save },
    { { (unichar_t *) N_("_Import..."), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'I' }, 'I', ksm_control, NULL, NULL, SFTFPopupInvoked, MID_Import },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) N_("Save As _Image..."), NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 1, 0, 'S' }, 'S', ksm_control|ksm_shift, NULL, NULL, SFTFPopupInvoked, MID_SaveImage },
    { NULL }
};

static void SFTFPopupMenu(SFTextArea *st, GEvent *event) {
    int no_sel = st->sel_start==st->sel_end;
    static int done = false;

    if ( !done ) {
	int i;
	for ( i=0; sftf_popuplist[i].ti.text!=NULL || sftf_popuplist[i].ti.line; ++i )
	    if ( sftf_popuplist[i].ti.text!=NULL )
		sftf_popuplist[i].ti.text = (unichar_t *) _( (char *) sftf_popuplist[i].ti.text);
	done = true;
    }

    sftf_popuplist[0].ti.disabled = st->oldtext==NULL;	/* Undo */
    sftf_popuplist[2].ti.disabled = no_sel;		/* Cut */
    sftf_popuplist[3].ti.disabled = no_sel;		/* Copy */
    sftf_popuplist[4].ti.disabled = !GDrawSelectionHasType(st->g.base,sn_clipboard,"text/plain;charset=ISO-10646-UCS-2") &&
	    !GDrawSelectionHasType(st->g.base,sn_clipboard,"UTF8_STRING") &&
	    !GDrawSelectionHasType(st->g.base,sn_clipboard,"STRING");
    sftf_popuplist[9].ti.disabled = (st->lcnt<=0);
    popup_kludge = st;
    GMenuCreatePopupMenu(st->g.base,event, sftf_popuplist);
}

static int SFTextAreaDoChange(SFTextArea *st, GEvent *event) {
    int ss = st->sel_start, se = st->sel_end;
    int pos, l, xpos;
    unichar_t *upt;

    if ( ( event->u.chr.state&(ksm_control|ksm_meta)) ||
	    event->u.chr.chars[0]<' ' || event->u.chr.chars[0]==0x7f ) {
	switch ( event->u.chr.keysym ) {
	  case GK_BackSpace:
	    if ( st->sel_start==st->sel_end ) {
		if ( st->sel_start==0 )
return( 2 );
		--st->sel_start;
	    }
	    SFTextArea_Replace(st,nullstr);
return( true );
	  break;
	  case GK_Delete:
	    if ( st->sel_start==st->sel_end ) {
		if ( st->text[st->sel_start]==0 )
return( 2 );
		++st->sel_end;
	    }
	    SFTextArea_Replace(st,nullstr);
return( true );
	  break;
	  case GK_Left: case GK_KP_Left:
	    if ( st->sel_start==st->sel_end ) {
		st->sel_start = GTBackPos(st,st->sel_start,event->u.chr.state&ksm_meta);
		if ( !(event->u.chr.state&ksm_shift ))
		    st->sel_end = st->sel_start;
	    } else if ( event->u.chr.state&ksm_shift ) {
		if ( st->sel_end==st->sel_base ) {
		    st->sel_start = GTBackPos(st,st->sel_start,event->u.chr.state&ksm_meta);
		} else {
		    st->sel_end = GTBackPos(st,st->sel_end,event->u.chr.state&ksm_meta);
		}
	    } else {
		st->sel_end = st->sel_base = st->sel_start;
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case GK_Right: case GK_KP_Right:
	    if ( st->sel_start==st->sel_end ) {
		st->sel_end = GTForePos(st,st->sel_start,event->u.chr.state&ksm_meta);
		if ( !(event->u.chr.state&ksm_shift ))
		    st->sel_start = st->sel_end;
	    } else if ( event->u.chr.state&ksm_shift ) {
		if ( st->sel_end==st->sel_base ) {
		    st->sel_start = GTForePos(st,st->sel_start,event->u.chr.state&ksm_meta);
		} else {
		    st->sel_end = GTForePos(st,st->sel_end,event->u.chr.state&ksm_meta);
		}
	    } else {
		st->sel_start = st->sel_base = st->sel_end;
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case GK_Up: case GK_KP_Up:
	    if ( !st->multi_line )
	  break;
	    if ( !( event->u.chr.state&ksm_shift ) && st->sel_start!=st->sel_end )
		st->sel_end = st->sel_base = st->sel_start;
	    else {
		pos = st->sel_start;
		if ( ( event->u.chr.state&ksm_shift ) && st->sel_start==st->sel_base )
		    pos = st->sel_end;
		l = SFTextAreaFindLine(st,st->sel_start);
		xpos = SFTextAreaGetXPosFromOffset(st,l,st->sel_start);
		if ( l!=0 )
		    pos = SFTextAreaGetOffsetFromXPos(st,l-1,xpos);
		if ( event->u.chr.state&ksm_shift ) {
		    if ( pos<st->sel_base ) {
			st->sel_start = pos;
			st->sel_end = st->sel_base;
		    } else {
			st->sel_start = st->sel_base;
			st->sel_end = pos;
		    }
		} else {
		    st->sel_start = st->sel_end = st->sel_base = pos;
		}
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case GK_Down: case GK_KP_Down:
	    if ( !st->multi_line )
	  break;
	    if ( !( event->u.chr.state&ksm_shift ) && st->sel_start!=st->sel_end )
		st->sel_end = st->sel_base = st->sel_end;
	    else {
		pos = st->sel_start;
		if ( ( event->u.chr.state&ksm_shift ) && st->sel_start==st->sel_base )
		    pos = st->sel_end;
		l = SFTextAreaFindLine(st,st->sel_start);
		xpos = SFTextAreaGetXPosFromOffset(st,l,st->sel_start);
		if ( l<st->lcnt-1 )
		    pos = SFTextAreaGetOffsetFromXPos(st,l+1,xpos);
		if ( event->u.chr.state&ksm_shift ) {
		    if ( pos<st->sel_base ) {
			st->sel_start = pos;
			st->sel_end = st->sel_base;
		    } else {
			st->sel_start = st->sel_base;
			st->sel_end = pos;
		    }
		} else {
		    st->sel_start = st->sel_end = st->sel_base = pos;
		}
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case GK_Home: case GK_Begin: case GK_KP_Home: case GK_KP_Begin:
	    if ( !(event->u.chr.state&ksm_shift) ) {
		st->sel_start = st->sel_base = st->sel_end = 0;
	    } else {
		st->sel_start = 0; st->sel_end = st->sel_base;
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  /* Move to eol. (if already at eol, move to next eol) */
	  case 'E': case 'e':
	    if ( !( event->u.chr.state&ksm_control ) )
return( false );
	    upt = st->text+st->sel_base;
	    if ( *upt=='\n' )
		++upt;
	    upt = u_strchr(upt,'\n');
	    if ( upt==NULL ) upt=st->text+u_strlen(st->text);
	    if ( !(event->u.chr.state&ksm_shift) ) {
		st->sel_start = st->sel_base = st->sel_end =upt-st->text;
	    } else {
		st->sel_start = st->sel_base; st->sel_end = upt-st->text;
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case GK_End: case GK_KP_End:
	    if ( !(event->u.chr.state&ksm_shift) ) {
		st->sel_start = st->sel_base = st->sel_end = u_strlen(st->text);
	    } else {
		st->sel_start = st->sel_base; st->sel_end = u_strlen(st->text);
	    }
	    SFTextArea_Show(st,st->sel_start);
return( 2 );
	  break;
	  case 'A': case 'a':
	    if ( event->u.chr.state&ksm_control ) {	/* Select All */
		sftextarea_editcmd(&st->g,ec_selectall);
return( 2 );
	    }
	  break;
	  case 'C': case 'c':
	    if ( event->u.chr.state&ksm_control ) {	/* Copy */
		sftextarea_editcmd(&st->g,ec_copy);
	    }
	  break;
	  case 'V': case 'v':
	    if ( event->u.chr.state&ksm_control ) {	/* Paste */
		sftextarea_editcmd(&st->g,ec_paste);
		SFTextArea_Show(st,st->sel_start);
return( true );
	    }
	  break;
	  case 'X': case 'x':
	    if ( event->u.chr.state&ksm_control ) {	/* Cut */
		sftextarea_editcmd(&st->g,ec_cut);
		SFTextArea_Show(st,st->sel_start);
return( true );
	    }
	  break;
	  case 'Z': case 'z':				/* Undo */
	    if ( event->u.chr.state&ksm_control ) {
		sftextarea_editcmd(&st->g,ec_undo);
		SFTextArea_Show(st,st->sel_start);
return( true );
	    }
	  break;
	  case 'D': case 'd':
	    if ( event->u.chr.state&ksm_control ) {	/* delete word */
		sftextarea_editcmd(&st->g,ec_deleteword);
		SFTextArea_Show(st,st->sel_start);
return( true );
	    }
	  break;
	  case 'W': case 'w':
	    if ( event->u.chr.state&ksm_control ) {	/* backword */
		sftextarea_editcmd(&st->g,ec_backword);
		SFTextArea_Show(st,st->sel_start);
return( true );
	    }
	  break;
	  case 'M': case 'm': case 'J': case 'j':
	    if ( !( event->u.chr.state&ksm_control ) )
return( false );
	    /* fall through into return case */
	  case GK_Return: case GK_Linefeed:
	    if ( st->accepts_returns ) {
		SFTextArea_Replace(st,newlinestr);
return( true );
	    }
	  break;
	  case GK_Tab:
	    if ( st->accepts_tabs ) {
		SFTextArea_Replace(st,tabstr);
return( true );
	    }
	  break;
	  case 's': case 'S':
	    if ( !( event->u.chr.state&ksm_control ) )
return( false );
	    SFTextAreaSave(st);
return( 2 );
	  break;
	  case 'I': case 'i':
	    if ( !( event->u.chr.state&ksm_control ) )
return( false );
	    SFTextAreaImport(st);
return( true );
	}
    } else {
	SFTextArea_Replace(st,event->u.chr.chars);
return( true );
    }

    if ( st->sel_start == st->sel_end )
	st->sel_base = st->sel_start;
    if ( ss!=st->sel_start || se!=st->sel_end )
	SFTextAreaGrabPrimarySelection(st);
return( false );
}

static void gt_cursor_pos(SFTextArea *st, int *x, int *y, int *fh) {
    int l, ty;

    *x = 0; *y= 0; *fh = 20;
    if ( st->fontlist!=NULL )
	*fh = st->fontlist->fd->pointsize*st->dpi/72;
    l = SFTextAreaFindLine(st,st->sel_start);
    if ( l<0 || l>=st->lcnt )
return;
    ty = st->lineheights[l].y - st->lineheights[st->loff_top].y;
    if ( ty<0 || ty>st->g.inner.height ) {
	*x = *y = -1;
return;
    }
    *y = ty;
    *fh = st->lineheights[l].fh;
    *x = SFTextAreaGetXPosFromOffset(st,l,st->sel_start);
}

static void GTPositionGIC(SFTextArea *st) {
    int x,y,fh;

    if ( !st->g.has_focus || st->gic==NULL )
return;
    gt_cursor_pos(st,&x,&y,&fh);
    if ( x<0 )
return;
    GDrawSetGIC(st->g.base,st->gic,st->g.inner.x+x,st->g.inner.y+y+st->as);
}

static void gt_draw_cursor(GWindow pixmap, SFTextArea *st) {
    GRect old;
    int x, y, fh;

    if ( !st->cursor_on || st->sel_start != st->sel_end )
return;
    gt_cursor_pos(st,&x,&y,&fh);

    if ( x<0 || x>=st->g.inner.width )
return;
    GDrawPushClip(pixmap,&st->g.inner,&old);
    GDrawSetXORMode(pixmap);
    GDrawSetXORBase(pixmap,st->g.box->main_background!=COLOR_DEFAULT?st->g.box->main_background:
	    GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(pixmap)) );
    GDrawSetFont(pixmap,st->font);
    GDrawSetLineWidth(pixmap,0);
    GDrawDrawLine(pixmap,st->g.inner.x+x,st->g.inner.y+y,
	    st->g.inner.x+x,st->g.inner.y+y+fh,
	    st->g.box->main_foreground!=COLOR_DEFAULT?st->g.box->main_foreground:
	    GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(pixmap)) );
    GDrawSetCopyMode(pixmap);
    GDrawPopClip(pixmap,&old);
}

static void SFTextAreaDrawDDCursor(SFTextArea *st, int pos) {
    GRect old;
    int x, y, l;

    l = SFTextAreaFindLine(st,pos);
    y = st->lineheights[l].y - st->lineheights[st->loff_top].y;
    if ( y<0 || y>st->g.inner.height )
return;
    x = SFTextAreaGetXPosFromOffset(st,l,pos);
    if ( x<0 || x>=st->g.inner.width )
return;

    GDrawPushClip(st->g.base,&st->g.inner,&old);
    GDrawSetXORMode(st->g.base);
    GDrawSetXORBase(st->g.base,st->g.box->main_background!=COLOR_DEFAULT?st->g.box->main_background:
	    GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(st->g.base)) );
    GDrawSetFont(st->g.base,st->font);
    GDrawSetLineWidth(st->g.base,0);
    GDrawSetDashedLine(st->g.base,2,2,0);
    GDrawDrawLine(st->g.base,st->g.inner.x+x,st->g.inner.y+y,
	    st->g.inner.x+x,st->g.inner.y+y+st->lineheights[l].fh,
	    st->g.box->main_foreground!=COLOR_DEFAULT?st->g.box->main_foreground:
	    GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(st->g.base)) );
    GDrawSetCopyMode(st->g.base);
    GDrawPopClip(st->g.base,&old);
    GDrawSetDashedLine(st->g.base,0,0,0);
    st->has_dd_cursor = !st->has_dd_cursor;
    st->dd_cursor_pos = pos;
}

static int sftextarea_expose(GWindow pixmap, GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;
    GRect old1, old2, *r = &g->r, selr;
    Color fg,sel;
    int y,x,p,i,dotext,j,xend;
    struct opentype_str **line;

    if ( g->state == gs_invisible || st->dontdraw )
return( false );

    GDrawPushClip(pixmap,r,&old1);

    GBoxDrawBackground(pixmap,r,g->box,
	    g->state==gs_enabled? gs_pressedactive: g->state,false);
    GBoxDrawBorder(pixmap,r,g->box,g->state,false);

    GDrawPushClip(pixmap,&g->inner,&old2);
    GDrawSetFont(pixmap,st->font);
    GDrawSetDither(NULL, false);	/* on 8 bit displays we don't want any dithering */
    GDrawSetLineWidth(pixmap,0);

    fg = g->state==gs_disabled?g->box->disabled_foreground:
		    g->box->main_foreground==COLOR_DEFAULT?GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(pixmap)):
		    g->box->main_foreground;
    sel = g->box->active_border;
    for ( i=st->loff_top; i<st->lcnt; ++i ) {
	int selstartx, selendx;
	/* First draw the selection, then draw the text */
	y = g->inner.y+ st->lineheights[i].y-st->lineheights[st->loff_top].y+
		st->lineheights[i].as;
	if ( y>g->inner.y+g->inner.height || y>event->u.expose.rect.y+event->u.expose.rect.height )
    break;
	if ( y+st->lineheights[i].fh<=event->u.expose.rect.y )
    continue;
	selstartx = selendx = -1;
	for ( dotext=0; dotext<2; ++dotext ) {
	    /* Does this para start out r2l or l2r? */
	    p = st->lineheights[i].p;
	    if ( st->paras[p].para[0]!=NULL &&
		    ScriptIsRightToLeft( ((struct fontlist *) (st->paras[p].para[0]->fl))->script ))
		x = st->xmax - st->lineheights[i].linelen;
	    else
		x = 0;
	    line = st->lines[i];
	    for ( j=0; line[j]!=NULL; ++j ) {
		xend = x + line[j]->advance_width + line[j]->vr.h_adv_off;
		if ( dotext ) {
		    FDDrawChar(pixmap,NULL,line[j],g->inner.x+x-st->xoff_left,y,fg);
		} else {
		    int pos = line[j]->orig_index +
			    ((struct fontlist *) (line[j]->fl))->start;
		    if ( pos>=st->sel_start && pos<st->sel_end ) {
			if ( selstartx==-1 )
			    selstartx = x;
			selendx = xend;
		    }
		    if ( !(pos>=st->sel_start && pos<st->sel_end) || line[j+1]==NULL ) {
			if ( selstartx!=-1 ) {
			    selr.x = selstartx+g->inner.x-st->xoff_left;
			    selr.width = selendx-selstartx;
			    selr.y = y-st->lineheights[i].as;
			    selr.height = st->lineheights[i].fh;
			    GDrawFillRect(pixmap,&selr,sel);
			    selstartx = selendx = -1;
			}
		    }
		}
		x = xend;
	    }
	}
    }

    GDrawSetDither(NULL, true);
    GDrawPopClip(pixmap,&old2);
    GDrawPopClip(pixmap,&old1);
    gt_draw_cursor(pixmap, st);
return( true );
}

static int SFTextAreaDoDrop(SFTextArea *st,GEvent *event,int endpos) {

    if ( st->has_dd_cursor )
	SFTextAreaDrawDDCursor(st,st->dd_cursor_pos);

    if ( event->type == et_mousemove ) {
	if ( GGadgetInnerWithin(&st->g,event->u.mouse.x,event->u.mouse.y) ) {
	    if ( endpos<st->sel_start || endpos>=st->sel_end )
		SFTextAreaDrawDDCursor(st,endpos);
	} else if ( !GGadgetWithin(&st->g,event->u.mouse.x,event->u.mouse.y) ) {
	    GDrawPostDragEvent(st->g.base,event,et_drag);
	}
    } else {
	if ( GGadgetInnerWithin(&st->g,event->u.mouse.x,event->u.mouse.y) ) {
	    if ( endpos>=st->sel_start && endpos<st->sel_end ) {
		st->sel_start = st->sel_end = endpos;
	    } else {
		unichar_t *old=st->oldtext, *temp;
		int pos=0;
		if ( event->u.mouse.state&ksm_control ) {
		    temp = galloc((u_strlen(st->text)+st->sel_end-st->sel_start+1)*sizeof(unichar_t));
		    memcpy(temp,st->text,endpos*sizeof(unichar_t));
		    memcpy(temp+endpos,st->text+st->sel_start,
			    (st->sel_end-st->sel_start)*sizeof(unichar_t));
		    u_strcpy(temp+endpos+st->sel_end-st->sel_start,st->text+endpos);
		} else if ( endpos>=st->sel_end ) {
		    temp = u_copy(st->text);
		    memcpy(temp+st->sel_start,temp+st->sel_end,
			    (endpos-st->sel_end)*sizeof(unichar_t));
		    memcpy(temp+endpos-(st->sel_end-st->sel_start),
			    st->text+st->sel_start,(st->sel_end-st->sel_start)*sizeof(unichar_t));
		    pos = endpos;
		} else /*if ( endpos<st->sel_start )*/ {
		    temp = u_copy(st->text);
		    memcpy(temp+endpos,st->text+st->sel_start,
			    (st->sel_end-st->sel_start)*sizeof(unichar_t));
		    memcpy(temp+endpos+st->sel_end-st->sel_start,st->text+endpos,
			    (st->sel_start-endpos)*sizeof(unichar_t));
		    pos = endpos+st->sel_end-st->sel_start;
		}
		st->oldtext = st->text;
		st->sel_oldstart = st->sel_start;
		st->sel_oldend = st->sel_end;
		st->sel_oldbase = st->sel_base;
		st->sel_start = st->sel_end = st->sel_end = pos;
		st->text = temp;
		free(old);
		SFTextAreaRefigureLines(st, endpos<st->sel_oldstart?endpos:st->sel_oldstart,-1);
	    }
	} else if ( !GGadgetWithin(&st->g,event->u.mouse.x,event->u.mouse.y) ) {
	    /* Don't delete the selection until someone actually accepts the drop */
	    /* Don't delete at all (copy not move) if control key is down */
	    if ( ( event->u.mouse.state&ksm_control ) )
		SFTextAreaGrabSelection(st,sn_drag_and_drop);
	    else
		SFTextAreaGrabDDSelection(st);
	    GDrawPostDragEvent(st->g.base,event,et_drop);
	}
	st->drag_and_drop = false;
	GDrawSetCursor(st->g.base,st->old_cursor);
	_ggadget_redraw(&st->g);
    }
return( false );
}

static void STChangeCheck(SFTextArea *st) {
    struct fontlist *fl;

    for ( fl=st->fontlist; fl!=NULL && fl->end<st->sel_end; fl=fl->next );
    if ( fl==NULL || /* fl->fd==st->last_fd ||*/ st->changefontcallback==NULL )
return;
    (st->changefontcallback)(st->cbcontext,fl->fd->sf,fl->fd->fonttype,
	    fl->fd->pointsize,fl->fd->antialias,fl->script,fl->lang,fl->feats);
}
    
static int sftextarea_mouse(GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;
    int end=-1;
    int i=0;

    if ( st->hidden_cursor ) {
	GDrawSetCursor(st->g.base,st->old_cursor);
	st->hidden_cursor = false;
	_GWidget_ClearGrabGadget(g);
    }
    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active && g->state!=gs_focused ))
return( false );
    if ( event->type == et_crossing )
return( false );
    if (( event->type==et_mouseup || event->type==et_mousedown ) &&
	    (event->u.mouse.button==4 || event->u.mouse.button==5) &&
	    st->vsb!=NULL )
return( GGadgetDispatchEvent(&st->vsb->g,event));

    if ( st->pressed==NULL && event->type == et_mousemove && g->popup_msg!=NULL &&
	    GGadgetWithin(g,event->u.mouse.x,event->u.mouse.y))
	GGadgetPreparePopup(g->base,g->popup_msg);

    if ( event->type == et_mousedown && event->u.mouse.button==3 &&
	    GGadgetWithin(g,event->u.mouse.x,event->u.mouse.y)) {
	SFTFPopupMenu(st,event);
return( true );
    }

    if ( event->type == et_mousedown || st->pressed ) {
	for ( i=st->loff_top; i<st->lcnt-1 &&
		event->u.mouse.y-g->inner.y>=st->lineheights[i+1].y-st->lineheights[st->loff_top].y;
		++i );
	if ( i<0 ) i = 0;
	if ( !st->multi_line ) i = 0;
	end = SFTextAreaGetOffsetFromXPos(st,i,event->u.mouse.x - st->g.inner.x - st->xoff_left);
    }

    if ( event->type == et_mousedown ) {
	st->wordsel = st->linesel = false;
	if ( event->u.mouse.button==1 && event->u.mouse.clicks>=3 ) {
	    if ( i<st->lcnt )
		st->sel_start = st->lineheights[i].start_pos;
	    else
		st->sel_start = end;
	    if ( i+1<st->lcnt )
		st->sel_end = st->lineheights[i+1].start_pos;
	    else
		st->sel_end = u_strlen(st->text);
	    st->wordsel = false; st->linesel = true;
	} else if ( event->u.mouse.button==1 && event->u.mouse.clicks==2 ) {
	    st->sel_start = st->sel_end = st->sel_base = end;
	    st->wordsel = true;
	    SFTextAreaSelectWords(st,st->sel_base);
	} else if ( end>=st->sel_start && end<st->sel_end &&
		st->sel_start!=st->sel_end &&
		event->u.mouse.button==1 ) {
	    st->drag_and_drop = true;
	    if ( !st->hidden_cursor )
		st->old_cursor = GDrawGetCursor(st->g.base);
	    GDrawSetCursor(st->g.base,ct_draganddrop);
	} else if ( event->u.mouse.button!=3 && !(event->u.mouse.state&ksm_shift) ) {
	    if ( event->u.mouse.button==1 )
		SFTextAreaGrabPrimarySelection(st);
	    st->sel_start = st->sel_end = st->sel_base = end;
	} else if ( end>st->sel_base ) {
	    st->sel_start = st->sel_base;
	    st->sel_end = end;
	} else {
	    st->sel_start = end;
	    st->sel_end = st->sel_base;
	}
	if ( st->pressed==NULL )
	    st->pressed = GDrawRequestTimer(st->g.base,200,100,NULL);
	if ( st->sel_start > u_strlen( st->text ))	/* Ok to have selection at end, but beyond is an error */
	    fprintf( stderr, "About to crash\n" );
	_ggadget_redraw(g);
	if ( st->changefontcallback )
	    STChangeCheck(st);
return( true );
    } else if ( st->pressed && (event->type == et_mousemove || event->type == et_mouseup )) {
	int refresh = true;

	if ( st->drag_and_drop ) {
	    refresh = SFTextAreaDoDrop(st,event,end);
	} else if ( st->linesel ) {
	    int basel, l, spos;
	    basel = SFTextAreaFindLine(st,st->sel_base);
	    l = basel<i ? basel : i;
	    if ( l<st->lcnt )
		spos = st->lineheights[l].start_pos;
	    else
		spos = basel<i ? st->sel_base : end;
	    st->sel_start = spos;
	    l = basel>i ? basel : i;
	    if ( l+1<st->lcnt )
		spos = st->lineheights[l+1].start_pos;
	    else
		spos = u_strlen(st->text);
	    st->sel_end = spos;
	} else if ( st->wordsel )
	    SFTextAreaSelectWords(st,end);
	else if ( event->u.mouse.button!=2 ) {
	    int e = end;
	    if ( e>st->sel_base ) {
		st->sel_start = st->sel_base; st->sel_end = e;
	    } else {
		st->sel_start = e; st->sel_end = st->sel_base;
	    }
	}
	if ( event->type==et_mouseup ) {
	    GDrawCancelTimer(st->pressed); st->pressed = NULL;
	    if ( event->u.mouse.button==2 )
		SFTextAreaPaste(st,sn_primary);
	    if ( st->sel_start==st->sel_end )
		SFTextArea_Show(st,st->sel_start);
	}
	if ( st->sel_end > u_strlen( st->text ))
	    fprintf( stderr, "About to crash\n" );
	if ( refresh )
	    _ggadget_redraw(g);
	if ( event->type==et_mouseup && st->changefontcallback )
	    STChangeCheck(st);
return( true );
    }
return( false );
}

static int sftextarea_key(GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;
    int ret;

    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active && g->state!=gs_focused ))
return( false );

    if ( event->type == et_charup )
return( false );
    if ( event->u.chr.keysym == GK_F1 || event->u.chr.keysym == GK_Help ||
	    (event->u.chr.keysym == GK_Return && !st->accepts_returns ) ||
	    ( event->u.chr.keysym == GK_Tab && !st->accepts_tabs ) ||
	    event->u.chr.keysym == GK_BackTab || event->u.chr.keysym == GK_Escape )
return( false );

    if ( !st->hidden_cursor ) {	/* hide the mouse pointer */
	if ( !st->drag_and_drop )
	    st->old_cursor = GDrawGetCursor(st->g.base);
	GDrawSetCursor(g->base,ct_invisible);
	st->hidden_cursor = true;
	_GWidget_SetGrabGadget(g);	/* so that we get the next mouse movement to turn the cursor on */
    }
    if( st->cursor_on ) {	/* undraw the blinky text cursor if it is drawn */
	gt_draw_cursor(g->base, st);
	st->cursor_on = false;
    }

    ret = SFTextAreaDoChange(st,event);
    if ( st->changefontcallback )
	STChangeCheck(st);
    switch ( ret ) {
      case 2:
      break;
      case true:
	SFTextAreaChanged(st,-1);
      break;
      case false:
return( false );
    }
    _ggadget_redraw(g);
return( true );
}

static int sftextarea_focus(GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;
    if ( st->cursor!=NULL ) {
	GDrawCancelTimer(st->cursor);
	st->cursor = NULL;
	st->cursor_on = false;
    }
    if ( st->hidden_cursor && !event->u.focus.gained_focus ) {
	GDrawSetCursor(st->g.base,st->old_cursor);
	st->hidden_cursor = false;
    }
    st->g.has_focus = event->u.focus.gained_focus;
    if ( event->u.focus.gained_focus ) {
	st->cursor = GDrawRequestTimer(st->g.base,400,400,NULL);
	st->cursor_on = true;
	if ( event->u.focus.mnemonic_focus != mf_normal )
	    SFTextAreaSelect(&st->g,0,-1);
	if ( st->gic!=NULL )
	    GTPositionGIC(st);
    }
    _ggadget_redraw(g);
    SFTextAreaFocusChanged(st,event->u.focus.gained_focus);
return( true );
}

static int sftextarea_timer(GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;

    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active && g->state!=gs_focused ))
return(false);
    if ( st->cursor == event->u.timer.timer ) {
	if ( st->cursor_on ) {
	    gt_draw_cursor(g->base, st);
	    st->cursor_on = false;
	} else {
	    st->cursor_on = true;
	    gt_draw_cursor(g->base, st);
	}
return( true );
    }
    if ( st->pressed == event->u.timer.timer ) {
	GEvent e;
	GDrawSetFont(g->base,st->font);
	GDrawGetPointerPosition(g->base,&e);
	if ( (e.u.mouse.x<g->r.x && st->xoff_left>0 ) ||
		(st->multi_line && e.u.mouse.y<g->r.y && st->loff_top>0 ) ||
		( e.u.mouse.x >= g->r.x + g->r.width &&
			st->xmax-st->xoff_left>g->inner.width ) ||
		( e.u.mouse.y >= g->r.y + g->r.height &&
			st->lineheights[st->lcnt-1].y-st->lineheights[st->loff_top].y >= g->inner.height )) {
	    int l;
	    int xpos, end;

	    for ( l=st->loff_top; l<st->lcnt-1 && e.u.mouse.y-g->inner.y>st->lineheights[l+1].y-st->lineheights[st->loff_top].y;
		    ++l );
	    if ( e.u.mouse.y<g->r.y && st->loff_top>0 )
		l = --st->loff_top;
	    else if ( e.u.mouse.y >= g->r.y + g->r.height &&
			    st->lineheights[st->lcnt-1].y-st->lineheights[st->loff_top].y > g->inner.height ) {
		++st->loff_top;
		++l;
	    } else if ( l<st->loff_top )
		l = st->loff_top; 
	    else if ( st->lineheights[l].y>=st->lineheights[st->loff_top].y + g->inner.height ) {
		for ( l = st->loff_top+1; st->lineheights[l].y<st->lineheights[st->loff_top].y+g->inner.height; ++l );
		--l;
		if ( l==st->loff_top ) ++l;
	    }
	    if ( l>=st->lcnt ) l = st->lcnt-1;

	    xpos = e.u.mouse.x+st->xoff_left;
	    if ( e.u.mouse.x<g->r.x && st->xoff_left>0 ) {
		st->xoff_left -= st->nw;
		xpos = g->inner.x + st->xoff_left;
	    } else if ( e.u.mouse.x >= g->r.x + g->r.width &&
			    st->xmax-st->xoff_left>g->inner.width ) {
		st->xoff_left += st->nw;
		xpos = g->inner.x + st->xoff_left + g->inner.width;
	    }

	    end = SFTextAreaGetOffsetFromXPos(st,l,xpos - st->g.inner.x - st->xoff_left);
	    if ( end > st->sel_base ) {
		st->sel_start = st->sel_base;
		st->sel_end = end;
	    } else {
		st->sel_start = end;
		st->sel_end = st->sel_base;
	    }
	    _ggadget_redraw(g);
	    if ( st->vsb!=NULL )
		GScrollBarSetPos(&st->vsb->g,st->lineheights[st->loff_top].y);
	    if ( st->hsb!=NULL )
		GScrollBarSetPos(&st->hsb->g,st->xoff_left);
	}
return( true );
    }
return( false );
}

static int sftextarea_sel(GGadget *g, GEvent *event) {
    SFTextArea *st = (SFTextArea *) g;
    int end;
    int i;

    if ( event->type == et_selclear ) {
	if ( event->u.selclear.sel==sn_primary && st->sel_start!=st->sel_end ) {
#if 0		/* Retain the drawn selection even if X says we don't own */
		/*  the selection property. Otherwise we can't change the */
		/*  fontsize (ie. must select the fontsize field) */
	    st->sel_start = st->sel_end = st->sel_base;
	    _ggadget_redraw(g);
#endif
return( true );
	}
return( false );
    }

    if ( st->has_dd_cursor )
	SFTextAreaDrawDDCursor(st,st->dd_cursor_pos);
    GDrawSetFont(g->base,st->font);
    for ( i=st->loff_top ; i<st->lcnt-1 && st->lineheights[i+1].y-st->lineheights[st->loff_top].y<
	    event->u.drag_drop.y-g->inner.y; ++i );
    if ( !st->multi_line ) i = 0;
    if ( i>=st->lcnt )
	end = u_strlen(st->text);
    else
	end = SFTextAreaGetOffsetFromXPos(st,i,event->u.drag_drop.x - st->g.inner.x - st->xoff_left);
    if ( event->type == et_drag ) {
	SFTextAreaDrawDDCursor(st,end);
    } else if ( event->type == et_dragout ) {
	/* this event exists simply to clear the dd cursor line. We've done */
	/*  that already */ 
    } else if ( event->type == et_drop ) {
	st->sel_start = st->sel_end = st->sel_base = end;
	SFTextAreaPaste(st,sn_drag_and_drop);
	SFTextArea_Show(st,st->sel_start);
	_ggadget_redraw(&st->g);
    } else
return( false );

return( true );
}
#endif

static void sftextarea_destroy(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
    struct sfmaps *m, *n;
    FontData *fd, *nfd;

    if ( st==NULL )
return;

    if ( st->vsb!=NULL )
	(st->vsb->g.funcs->destroy)(&st->vsb->g);
    if ( st->hsb!=NULL )
	(st->hsb->g.funcs->destroy)(&st->hsb->g);
    GDrawCancelTimer(st->pressed);
    GDrawCancelTimer(st->cursor);
    free(st->paras);
    free(st->lines);
    free(st->oldtext);
    free(st->text);
    fontlistfree(st->fontlist);
    fontlistfree(st->oldfontlist);
    for ( m=st->sfmaps; m!=NULL; m=n ) {
	n = m->next;
	SplineCharFree(m->fake_notdef);
	EncMapFree(m->map);
	chunkfree(m,sizeof(struct sfmaps));
    }
    for ( fd=st->generated ; fd!=NULL; fd = nfd ) {
	nfd = fd->next;
	if ( fd->depends_on )
	    fd->bdf->freetype_context = NULL;
	if ( fd->fonttype!=sftf_bitmap )	/* If it's a bitmap font, we didn't create it (lives in sf) so we can't destroy it */
	    BDFFontFree(fd->bdf);
	free(fd);
    }
    _ggadget_destroy(g);
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void SFTextAreaSetTitle(GGadget *g,const unichar_t *tit) {
    SFTextArea *st = (SFTextArea *) g;
    unichar_t *old = st->oldtext;
    if ( u_strcmp(tit,st->text)==0 )	/* If it doesn't change anything, then don't trash undoes or selection */
return;
    st->oldtext = st->text;
    st->sel_oldstart = st->sel_start; st->sel_oldend = st->sel_end; st->sel_oldbase = st->sel_base;
    st->text = u_copy(tit);		/* tit might be oldtext, so must copy before freeing */
    free(old);
    st->sel_start = st->sel_end = st->sel_base = u_strlen(tit);
    fontlistmergecheck(st);
    SFTextAreaRefigureLines(st,0,-1);
    SFTextArea_Show(st,st->sel_start);
    _ggadget_redraw(g);
}

static const unichar_t *_SFTextAreaGetTitle(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
return( st->text );
}

static void SFTextAreaSetFont(GGadget *g,FontInstance *new) {
    SFTextArea *st = (SFTextArea *) g;
    st->font = new;
    /* Irrelevant */;
}

static FontInstance *SFTextAreaGetFont(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
return( st->font );
}

void SFTextAreaShow(GGadget *g,int pos) {
    SFTextArea *st = (SFTextArea *) g;

    SFTextArea_Show(st,pos);
    _ggadget_redraw(g);
}

void SFTextAreaSelect(GGadget *g,int start, int end) {
    SFTextArea *st = (SFTextArea *) g;

    SFTextAreaGrabPrimarySelection(st);
    if ( end<0 ) {
	end = u_strlen(st->text);
	if ( start<0 ) start = end;
    }
    if ( start>end ) { int temp = start; start = end; end = temp; }
    if ( end>u_strlen(st->text)) end = u_strlen(st->text);
    if ( start>u_strlen(st->text)) start = end;
    else if ( start<0 ) start=0;
    st->sel_start = st->sel_base = start;
    st->sel_end = end;
    _ggadget_redraw(g);			/* Should be safe just to draw the textfield gadget, sbs won't have changed */
}

void SFTextAreaReplace(GGadget *g,const unichar_t *txt) {
    SFTextArea *st = (SFTextArea *) g;

    SFTextArea_Replace(st,txt);
    _ggadget_redraw(g);
}

static void sftextarea_redraw(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
    if ( st->vsb!=NULL )
	_ggadget_redraw((GGadget *) (st->vsb));
    if ( st->hsb!=NULL )
	_ggadget_redraw((GGadget *) (st->hsb));
    _ggadget_redraw(g);
}

static void sftextarea_move(GGadget *g, int32 x, int32 y ) {
    SFTextArea *st = (SFTextArea *) g;
    if ( st->vsb!=NULL )
	_ggadget_move((GGadget *) (st->vsb),x+(st->vsb->g.r.x-g->r.x),y);
    if ( st->hsb!=NULL )
	_ggadget_move((GGadget *) (st->hsb),x,y+(st->hsb->g.r.y-g->r.y));
    _ggadget_move(g,x,y);
}

static void sftextarea_resize(GGadget *g, int32 width, int32 height ) {
    SFTextArea *st = (SFTextArea *) g;
    int gtwidth=width, gtheight=height, oldheight=0;
    int l;

    if ( st->hsb!=NULL ) {
	oldheight = st->hsb->g.r.y+st->hsb->g.r.height-g->r.y;
	gtheight = height - (oldheight-g->r.height);
    }
    if ( st->vsb!=NULL ) {
	int oldwidth = st->vsb->g.r.x+st->vsb->g.r.width-g->r.x;
	gtwidth = width - (oldwidth-g->r.width);
	_ggadget_move((GGadget *) (st->vsb),st->vsb->g.r.x+width-oldwidth,st->vsb->g.r.y);
	_ggadget_resize((GGadget *) (st->vsb),st->vsb->g.r.width,gtheight);
    }
    if ( st->hsb!=NULL ) {
	_ggadget_move((GGadget *) (st->hsb),st->hsb->g.r.y,st->hsb->g.r.y+height-oldheight);
	_ggadget_resize((GGadget *) (st->hsb),gtwidth,st->hsb->g.r.height);
    }
    _ggadget_resize(g,gtwidth, gtheight);
    SFTextAreaRefigureLines(st,0,-1);
    if ( st->vsb!=NULL ) {
	GScrollBarSetBounds(&st->vsb->g,0,st->lineheights[st->lcnt-1].y,st->g.inner.height);
	if ( st->loff_top>=st->lcnt )
	    st->loff_top = st->lcnt-1;
	l = st->lcnt - SFTextArea_EndPage(st);
	if ( l<0 ) l = 0;
	if ( l!=st->loff_top ) {
	    st->loff_top = l;
	    GScrollBarSetPos(&st->vsb->g,st->lineheights[l].y);
	    _ggadget_redraw(&st->g);
	}
    }
    SFTextAreaShow(&st->g,st->sel_start);
}

static GRect *sftextarea_getsize(GGadget *g, GRect *r ) {
    SFTextArea *st = (SFTextArea *) g;
    _ggadget_getsize(g,r);
    if ( st->vsb!=NULL )
	r->width =  st->vsb->g.r.x+st->vsb->g.r.width-g->r.x;
    if ( st->hsb!=NULL )
	r->height =  st->hsb->g.r.y+st->hsb->g.r.height-g->r.y;
return( r );
}

static void sftextarea_setvisible(GGadget *g, int visible ) {
    SFTextArea *st = (SFTextArea *) g;
    if ( st->vsb!=NULL ) _ggadget_setvisible(&st->vsb->g,visible);
    if ( st->hsb!=NULL ) _ggadget_setvisible(&st->hsb->g,visible);
    _ggadget_setvisible(g,visible);
}

static void sftextarea_setenabled(GGadget *g, int enabled ) {
    SFTextArea *st = (SFTextArea *) g;
    if ( st->vsb!=NULL ) _ggadget_setenabled(&st->vsb->g,enabled);
    if ( st->hsb!=NULL ) _ggadget_setenabled(&st->hsb->g,enabled);
    _ggadget_setenabled(g,enabled);
}

static int sftextarea_vscroll(GGadget *g, GEvent *event) {
    enum sb sbt = event->u.control.u.sb.type;
    SFTextArea *st = (SFTextArea *) (g->data);
    int loff = st->loff_top;
    int page;

    g = (GGadget *) st;

    if ( sbt==et_sb_top )
	loff = 0;
    else if ( sbt==et_sb_bottom ) {
	loff = st->lcnt;
    } else if ( sbt==et_sb_up ) {
	if ( st->loff_top!=0 ) loff = st->loff_top-1; else loff = 0;
    } else if ( sbt==et_sb_down ) {
	++loff;
    } else if ( sbt==et_sb_uppage ) {
	for ( page=0; st->loff_top-page>=0 && st->lineheights[st->loff_top].y-st->lineheights[st->loff_top-page].y<=g->inner.height;
		++page );
	if ( --page < 1 ) page = 1;
	else if ( page>2 ) page-=1;
	loff = st->loff_top - page;
    } else if ( sbt==et_sb_downpage ) {
	for ( page=0; st->loff_top+page<st->lcnt && st->lineheights[st->loff_top+page].y-st->lineheights[st->loff_top].y<=g->inner.height;
		++page );
	if ( --page < 1 ) page = 1;
	else if ( page>2 ) page-=1;
	loff = st->loff_top + page;
    } else /* if ( sbt==et_sb_thumb || sbt==et_sb_thumbrelease ) */ {
	for ( loff = 0; loff<st->lcnt && st->lineheights[loff].y<event->u.control.u.sb.pos; ++loff );
    }
    for ( page=1; st->lcnt-page>=0 && st->lineheights[st->lcnt-1].y-st->lineheights[st->lcnt-page].y<=g->inner.height;
	    ++page );
    --page;
    if ( loff > st->lcnt-page )
	loff = st->lcnt - page;
    if ( loff<0 ) loff = 0;
    if ( loff!=st->loff_top ) {
	st->loff_top = loff;
	GScrollBarSetPos(&st->vsb->g,st->lineheights[loff].y);
	_ggadget_redraw(&st->g);
    }
return( true );
}

static int sftextarea_hscroll(GGadget *g, GEvent *event) {
    enum sb sbt = event->u.control.u.sb.type;
    SFTextArea *st = (SFTextArea *) (g->data);
    int xoff = st->xoff_left;

    g = (GGadget *) st;

    if ( sbt==et_sb_top )
	xoff = 0;
    else if ( sbt==et_sb_bottom ) {
	xoff = st->xmax - st->g.inner.width;
	if ( xoff<0 ) xoff = 0;
    } else if ( sbt==et_sb_up ) {
	if ( st->xoff_left>st->nw ) xoff = st->xoff_left-st->nw; else xoff = 0;
    } else if ( sbt==et_sb_down ) {
	if ( st->xoff_left + st->nw + st->g.inner.width >= st->xmax )
	    xoff = st->xmax - st->g.inner.width;
	else
	    xoff += st->nw;
    } else if ( sbt==et_sb_uppage ) {
	int page = (3*g->inner.width)/4;
	xoff = st->xoff_left - page;
	if ( xoff<0 ) xoff=0;
    } else if ( sbt==et_sb_downpage ) {
	int page = (3*g->inner.width)/4;
	xoff = st->xoff_left + page;
	if ( xoff + st->g.inner.width >= st->xmax )
	    xoff = st->xmax - st->g.inner.width;
    } else /* if ( sbt==et_sb_thumb || sbt==et_sb_thumbrelease ) */ {
	xoff = event->u.control.u.sb.pos;
    }
    if ( xoff + st->g.inner.width >= st->xmax )
	xoff = st->xmax - st->g.inner.width;
    if ( xoff<0 ) xoff = 0;
    if ( st->xoff_left!=xoff ) {
	st->xoff_left = xoff;
	GScrollBarSetPos(&st->hsb->g,xoff);
	_ggadget_redraw(&st->g);
    }
return( true );
}

struct gfuncs sftextarea_funcs = {
    0,
    sizeof(struct gfuncs),

    sftextarea_expose,
    sftextarea_mouse,
    sftextarea_key,
    _sftextarea_editcmd,
    sftextarea_focus,
    sftextarea_timer,
    sftextarea_sel,

    sftextarea_redraw,
    sftextarea_move,
    sftextarea_resize,
    sftextarea_setvisible,
    sftextarea_setenabled,
    sftextarea_getsize,
    _ggadget_getinnersize,

    sftextarea_destroy,

    SFTextAreaSetTitle,
    _SFTextAreaGetTitle,
    NULL,
    NULL,
    NULL,
    SFTextAreaSetFont,
    SFTextAreaGetFont
};

static void SFTextAreaInit() {
    static unichar_t courier[] = { 'c', 'o', 'u', 'r', 'i', 'e', 'r', ',', 'm','o','n','o','s','p','a','c','e',',','c','l','e','a','r','l','y','u',',', 'u','n','i','f','o','n','t', '\0' };
    FontRequest rq;

    GGadgetInit();
    GDrawDecomposeFont(_ggadget_default_font,&rq);
    rq.family_name = courier;
    sftextarea_font = GDrawInstanciateFont(screen_display,&rq);
    _GGadgetCopyDefaultBox(&sftextarea_box);
    sftextarea_box.padding = 3;
    sftextarea_box.flags = box_active_border_inner;
    sftextarea_font = _GGadgetInitDefaultBox("SFTextArea.",&sftextarea_box,sftextarea_font);
    sftextarea_inited = true;
}

static void SFTextAreaAddVSb(SFTextArea *st) {
    GGadgetData gd;

    memset(&gd,'\0',sizeof(gd));
    gd.pos.y = st->g.r.y; gd.pos.height = st->g.r.height;
    gd.pos.width = GDrawPointsToPixels(st->g.base,_GScrollBar_Width);
    gd.pos.x = st->g.r.x+st->g.r.width - gd.pos.width;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_sb_vert;
    gd.handle_controlevent = sftextarea_vscroll;
    st->vsb = (GScrollBar *) GScrollBarCreate(st->g.base,&gd,st);
    st->vsb->g.contained = true;

    gd.pos.width += GDrawPointsToPixels(st->g.base,1);
    st->g.r.width -= gd.pos.width;
    st->g.inner.width -= gd.pos.width;
}

static void SFTextAreaAddHSb(SFTextArea *st) {
    GGadgetData gd;

    memset(&gd,'\0',sizeof(gd));
    gd.pos.x = st->g.r.x; gd.pos.width = st->g.r.width;
    gd.pos.height = GDrawPointsToPixels(st->g.base,_GScrollBar_Width);
    gd.pos.y = st->g.r.y+st->g.r.height - gd.pos.height;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
    gd.handle_controlevent = sftextarea_hscroll;
    st->hsb = (GScrollBar *) GScrollBarCreate(st->g.base,&gd,st);
    st->hsb->g.contained = true;

    gd.pos.height += GDrawPointsToPixels(st->g.base,1);
    st->g.r.height -= gd.pos.height;
    st->g.inner.height -= gd.pos.height;
    if ( st->vsb!=NULL ) {
	st->vsb->g.r.height -= gd.pos.height;
	st->vsb->g.inner.height -= gd.pos.height;
    }
}

static void SFTextAreaFit(SFTextArea *st) {
    GTextBounds bounds;
    int as=0, ds, ld, fh=0, temp;
    GRect needed;
    int extra=0;

    needed.x = needed.y = 0;
    needed.width = needed.height = 1;

    {
	FontInstance *old = GDrawSetFont(st->g.base,st->font);
	(void) GDrawGetTextBounds(st->g.base,st->text, -1, NULL, &bounds);
	GDrawFontMetrics(st->font,&as, &ds, &ld);
	if ( as<bounds.as ) as = bounds.as;
	if ( ds<bounds.ds ) ds = bounds.ds;
	st->fh = fh = as+ds;
	st->as = as;
	st->nw = 6;
	GDrawSetFont(st->g.base,old);
    }

    temp = GGadgetScale(GDrawPointsToPixels(st->g.base,80))+extra;

    if ( st->g.r.width==0 || st->g.r.height==0 ) {
	int bp = GBoxBorderWidth(st->g.base,st->g.box);
	needed.x = needed.y = 0;
	needed.width = temp;
	needed.height = st->multi_line? 4*fh:fh;
	_ggadgetFigureSize(st->g.base,st->g.box,&needed,false);
	if ( st->g.r.width==0 ) {
	    st->g.r.width = needed.width;
	    st->g.inner.width = temp-extra;
	    st->g.inner.x = st->g.r.x + (needed.width-temp)/2;
	} else {
	    st->g.inner.x = st->g.r.x + bp;
	    st->g.inner.width = st->g.r.width - 2*bp;
	}
	if ( st->g.r.height==0 ) {
	    st->g.r.height = needed.height;
	    st->g.inner.height = st->multi_line? 4*fh:fh;
	    st->g.inner.y = st->g.r.y + (needed.height-st->g.inner.height)/2;
	} else {
	    st->g.inner.y = st->g.r.y + bp;
	    st->g.inner.height = st->g.r.height - 2*bp;
	}
	if ( st->multi_line ) {
	    int sbadd = GDrawPointsToPixels(st->g.base,_GScrollBar_Width) +
		    GDrawPointsToPixels(st->g.base,1);
	    {
		st->g.r.width += sbadd;
		st->g.inner.width += sbadd;
	    }
	    if ( !st->wrap ) {
		st->g.r.height += sbadd;
		st->g.inner.height += sbadd;
	    }
	}
    } else {
	int bp = GBoxBorderWidth(st->g.base,st->g.box);
	st->g.inner = st->g.r;
	st->g.inner.x += bp; st->g.inner.y += bp;
	st->g.inner.width -= 2*bp-extra; st->g.inner.height -= 2*bp;
    }
    if ( st->multi_line ) {
	SFTextAreaAddVSb(st);
	if ( !st->wrap )
	    SFTextAreaAddHSb(st);
    }
}

static SFTextArea *_SFTextAreaCreate(SFTextArea *st, struct gwindow *base, GGadgetData *gd,void *data, GBox *def) {

    if ( !sftextarea_inited )
	SFTextAreaInit();
    st->g.funcs = &sftextarea_funcs;
    _GGadget_Create(&st->g,base,gd,data,def);

    st->g.takes_input = true; st->g.takes_keyboard = true; st->g.focusable = true;
    if ( gd->label!=NULL ) {
	if ( gd->label->text_in_resource )	/* This one use of GStringGetResource is ligit */
	    st->text = u_copy((unichar_t *) GStringGetResource((intpt) gd->label->text,&st->g.mnemonic));
	else if ( gd->label->text_is_1byte )
	    st->text = utf82u_copy((char *) gd->label->text);
	else
	    st->text = u_copy(gd->label->text);
	st->sel_start = st->sel_end = st->sel_base = u_strlen(st->text);
    }
    if ( st->text==NULL )
	st->text = gcalloc(1,sizeof(unichar_t));
    st->font = sftextarea_font;
    if ( gd->label!=NULL && gd->label->font!=NULL )
	st->font = gd->label->font;
    SFTextAreaFit(st);
    _GGadget_FinalPosition(&st->g,base,gd);
    SFTextAreaRefigureLines(st,0,-1);

    if ( gd->flags & gg_group_end )
	_GGadgetCloseGroup(&st->g);
    GWidgetIndicateFocusGadget(&st->g);
    if ( gd->flags & gg_text_xim )
	st->gic = GDrawCreateInputContext(base,gic_overspot|gic_orlesser);
return( st );
}

GGadget *SFTextAreaCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    SFTextArea *st = gcalloc(1,sizeof(SFTextArea));
    st->multi_line = true;
    st->accepts_returns = true;
    st->wrap = true;
    _SFTextAreaCreate(st,base,gd,data,&sftextarea_box);
    st->dpi = 100;

return( &st->g );
}
#endif

static void SFMapFill(struct sfmaps *sfmaps,SplineFont *sf) {
    sfmaps->map = EncMapFromEncoding(sf,FindOrMakeEncoding("UnicodeFull"));
    sfmaps->notdef_gid = SFFindGID(sf,-1,".notdef");
    if ( sfmaps->notdef_gid==-1 ) {
	SplineChar *notdef = SplineCharCreate();
	sfmaps->fake_notdef = notdef;
	notdef->name = copy(".notdef");
	notdef->parent = sf;
	notdef->width = (sf->ascent+sf->descent);
	if ( sf->cidmaster==NULL )
	    notdef->width = 6*notdef->width/10;
	notdef->searcherdummy = true;
	notdef->orig_pos = -1;
    }
}

struct sfmaps *SFMapOfSF(SFTextArea *st,SplineFont *sf) {
    struct sfmaps *sfmaps;

    for ( sfmaps=st->sfmaps; sfmaps!=NULL; sfmaps=sfmaps->next )
	if ( sfmaps->sf==sf )
return( sfmaps );

    sfmaps = chunkalloc(sizeof(struct sfmaps));
    sfmaps->sf = sf;
    sfmaps->next = st->sfmaps;
    st->sfmaps = sfmaps;
    SFMapFill(sfmaps,sf);
return( sfmaps );
}

static FontData *RegenFontData(SFTextArea *st, FontData *ret) {
    FontData *test;
    BDFFont *bdf, *ok, *old;
    void *ftc;
    int pixelsize;
    int freeold = ret->fonttype != sftf_bitmap;

    pixelsize = rint((ret->pointsize * st->dpi)/72.0 );
    old = ret->bdf;
    ret->bdf = NULL;

    if ( ret->fonttype==sftf_bitmap ) {
	ok = NULL;
	for ( bdf= ret->sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	    if ( bdf->pixelsize==pixelsize ) {
		if (( !ret->antialias && bdf->clut==NULL ) ||
			(ret->antialias && bdf->clut!=NULL && bdf->clut->clut_len==256) ) {
		    ok = bdf;
	break;
		}
		if ( ret->antialias && bdf->clut!=NULL &&
			(ok==NULL || bdf->clut->clut_len>ok->clut->clut_len))
		    ok = bdf;
	    }
	}
	if ( ok==NULL )
	    ret->fonttype = sftf_pfaedit;
	else
	    ret->bdf = ok;
    } else if ( !hasFreeType() && ret->fonttype!=sftf_pfaedit )
	ret->fonttype = sftf_pfaedit;
    else if (( ret->sf->multilayer || ret->sf->strokedfont ) && ret->fonttype!=sftf_nohints )
	ret->fonttype = sftf_pfaedit;

    if ( ret->bdf!=NULL )
	/* Already done */;
    else if ( ret->fonttype==sftf_pfaedit )
	ret->bdf = SplineFontPieceMeal(ret->sf,pixelsize,ret->antialias?pf_antialias:0,NULL);
    else if ( ret->fonttype==sftf_nohints )
	ret->bdf = SplineFontPieceMeal(ret->sf,pixelsize,
		(ret->antialias?pf_antialias:0)|pf_ft_nohints,NULL);
    else {
	for ( test=st->generated; test!=NULL; test=test->next )
	    if ( test!=ret && test->bdf!=NULL && test->sf == ret->sf &&
		    test->fonttype == ret->fonttype )
	break;
	ret->depends_on = test;
	ftc = NULL;
	if ( test && test->bdf )
	    ftc = test->bdf->freetype_context;
	if ( ftc==NULL ) {
	    int flags = 0;
	    int ff = ret->fonttype==sftf_pfb ? ff_pfb :
		     ret->fonttype==sftf_ttf ? ff_ttf :
		     ff_otf;
	    ftc = _FreeTypeFontContext(ret->sf,NULL,NULL,ff,flags,NULL);
	}
	if ( ftc==NULL ) {
	    if ( old!=NULL )
		ret->bdf = old;
	    else {
		free(ret);
		ret = NULL;
	    }
return( ret );
	}
	ret->bdf = SplineFontPieceMeal(ret->sf,pixelsize,ret->antialias,ftc);
    }
    if ( freeold )
	BDFFontFree(old);

    if ( ret->bdf->clut ) {
	ret->gi.u.image = &ret->base;
	ret->base.image_type = it_index;
	ret->base.clut = ret->bdf->clut;
	ret->base.trans = 0;
    } else {
	memset(&ret->clut,'\0',sizeof(ret->clut));
	ret->gi.u.image = &ret->base;
	ret->base.image_type = it_mono;
	ret->base.clut = &ret->clut;
	ret->clut.clut_len = 2;
	ret->clut.clut[0] = GDrawGetDefaultBackground(NULL);
	ret->base.trans = 0;
    }
return( ret );
}

static FontData *FindFontData(SFTextArea *st, SplineFont *sf,
	enum sftf_fonttype fonttype, int size, int antialias) {
    FontData *test, *ret;

    for ( test=st->generated; test!=NULL; test=test->next )
	if ( test->sf == sf && test->fonttype == fonttype &&
		test->pointsize==size && test->antialias==antialias )
return( test );

    ret = gcalloc(1,sizeof(FontData));
    ret->sf = sf;
    ret->fonttype = fonttype;
    ret->pointsize = size;
    ret->antialias = antialias;
    ret = RegenFontData(st,ret);
    if ( ret==NULL )
return( NULL );

    ret->sfmap = SFMapOfSF(st,sf);
    ret->next = st->generated;
    st->generated = ret;
return( ret );
}

static FontData *FontDataCopyNoBDF(SFTextArea *print, FontData *source) {
    FontData *head=NULL, *last=NULL, *cur;

    while ( source ) {
	cur = gcalloc(1,sizeof(FontData));
	cur->sf = source->sf;
	cur->fonttype = source->fonttype;
	cur->pointsize = source->pointsize;

	cur->sfmap = SFMapOfSF(print,source->sf);
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
	source = source->next;
    }
return( head );
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static int SFTF_NormalizeStartEnd(SFTextArea *st, int start, int *_end) {
    int end = *_end;
    int len = u_strlen(st->text);

    if ( st->generated==NULL ) {
	start = 0;
	end = len;
    } else if ( start==-1 ) {
	start = st->sel_start;
	end = st->sel_end;
    } else if ( end==-1 )
	end = len;
    if ( end>len ) end = len;
    if ( start<0 ) start = 0;
    if ( start>end ) start = end;
    *_end = end;
return( start );
}

static struct fontlist *SFTFBreakFontList(SFTextArea *st,int start,int end) {
    /* We are going to change some item in the fontlist between start and end */
    /* Make sure that after this call there will be an entry which starts at */
    /*  start and (perhaps) another which ends at end */
    struct fontlist *new, *fl, *prev, *next, *first;

    if ( st->fontlist==NULL ) {
	new = chunkalloc(sizeof(struct fontlist));
	new->start = start;
	new->end = end;
	st->fontlist = new;
return( new );
    }

    prev = next = NULL;
    for ( fl=st->fontlist; fl!=NULL && fl->end<start; fl=fl->next )
	prev = fl;
    if ( fl==NULL ) {
	fl = chunkalloc(sizeof(struct fontlist));
	*fl = *prev;
	fl->feats = TagsCopy(prev->feats);
	fl->start = prev->end;
	fl->end = end;
	fl->scmax = 0; fl->sctext = NULL; fl->ottext = NULL;
    }
    if ( fl->start == start )
	first = fl;
    else {
	new = chunkalloc(sizeof(struct fontlist));
	*new = *fl;
	new->feats = TagsCopy(fl->feats);
	new->start = start;
	fl->end = start;
	fl->next = new;
	new->scmax = 0; new->sctext = NULL; new->ottext = NULL;
	first = new;
    }
    prev = first;
    for ( fl=first; fl!=NULL && fl->start<end ; fl=fl->next )
	prev = fl;
    if ( fl==NULL && prev->end<end )
	prev->end = end;
    if ( prev->end>end ) {
	new = chunkalloc(sizeof(struct fontlist));
	*new = *prev;
	new->feats = TagsCopy(prev->feats);
	new->start = end;
	new->scmax = 0; new->sctext = NULL; new->ottext = NULL;
	prev->end = end;
	prev->next = new;
    }
return( first );
}

static void SFTFMetaChangeCleanup(SFTextArea *st,int start, int end) {
    fontlistmergecheck(st);
    SFTextAreaRefigureLines(st, start,end);
    GDrawRequestExpose(st->g.base,&st->g.inner,false);
    if ( st->changefontcallback != NULL )
	STChangeCheck(st);
}

int SFTFSetFontData(GGadget *g, int start, int end, SplineFont *sf,
	enum sftf_fonttype fonttype, int size, int antialias) {
    /* Sets the font for the region between start and end. If start==-1 it */
    /*  means use the current selection (and ignore end). If end==-1 it means */
    /*  strlen(g->text) */
    /* I'm not going to mess with making this undoable. Nor am I going to clear*/
    /*  out the undoes. So if someone does an undo after this it will undo */
    /*  two things. Tough. */
    SFTextArea *st = (SFTextArea *) g;
    FontData *cur;
    struct fontlist *fl;

    cur = FindFontData(st, sf, fonttype, size, antialias);
    if ( cur==NULL )
return( false );

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	fl->fd = cur;
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetFont(GGadget *g, int start, int end, SplineFont *sf) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *cur;
    struct fontlist *fl;

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	if ( fl->fd->sf!=sf ) {
	    cur = FindFontData(st, sf, fl->fd->fonttype, fl->fd->pointsize, fl->fd->antialias);
	    if ( cur!=NULL )
		fl->fd = cur;
	}
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetFontType(GGadget *g, int start, int end, enum sftf_fonttype fonttype) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *cur;
    struct fontlist *fl;

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	if ( fl->fd->fonttype!=fonttype ) {
	    cur = FindFontData(st, fl->fd->sf, fonttype, fl->fd->pointsize, fl->fd->antialias);
	    if ( cur!=NULL )
		fl->fd = cur;
	}
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetSize(GGadget *g, int start, int end, int pointsize) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *cur;
    struct fontlist *fl;

    if ( st->generated==NULL )
return( false );
    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	if ( fl->fd->pointsize!=pointsize ) {
	    cur = FindFontData(st, fl->fd->sf, fl->fd->fonttype, pointsize, fl->fd->antialias);
	    if ( cur!=NULL )
		fl->fd = cur;
	}
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetAntiAlias(GGadget *g, int start, int end, int antialias) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *cur;
    struct fontlist *fl;

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	if ( fl->fd->antialias!=antialias ) {
	    cur = FindFontData(st, fl->fd->sf, fl->fd->fonttype, fl->fd->pointsize, antialias);
	    if ( cur!=NULL )
		fl->fd = cur;
	}
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetScriptLang(GGadget *g, int start, int end, uint32 script, uint32 lang) {
    SFTextArea *st = (SFTextArea *) g;
    struct fontlist *fl;

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	if ( fl->script != script ) {
	    free(fl->feats);
	    fl->feats = TagsCopy(StdFeaturesOfScript(script));
	}
	fl->script = script;
	fl->lang = lang;
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

int SFTFSetFeatures(GGadget *g, int start, int end, uint32 *features) {
    SFTextArea *st = (SFTextArea *) g;
    struct fontlist *fl;

    start = SFTF_NormalizeStartEnd(st, start, &end);
    fl = SFTFBreakFontList(st,start,end);
    while ( fl!=NULL && fl->end<=end ) {
	free(fl->feats);
	fl->feats = TagsCopy(features);
	fl = fl->next;
    }

    SFTFMetaChangeCleanup(st,start,end);
return( true );
}

void SFTFRegisterCallback(GGadget *g, void *cbcontext,
	void (*changefontcallback)(void *,SplineFont *,enum sftf_fonttype,int size,int aa, uint32 script, uint32 lang, uint32 *feats)) {
    SFTextArea *st = (SFTextArea *) g;

    st->cbcontext = cbcontext;
    st->changefontcallback = changefontcallback;
}

void SFTFProvokeCallback(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
    STChangeCheck(st);
}

void SFTFInitLangSys(GGadget *g, int end, uint32 script, uint32 lang) {
    SFTextArea *st = (SFTextArea *) g;
    struct fontlist *prev, *next;

    if ( (st->text!=NULL && st->text[0]!='\0') || st->fontlist==NULL ) {
	IError( "SFTFInitLangSys can only be called during initialization" );
return;
    }
    if ( st->fontlist!=NULL && st->fontlist->script==0 ) {
	next = st->fontlist;
    } else {
	for ( prev = st->fontlist; prev->next!=NULL; prev=prev->next );
	next = chunkalloc(sizeof(struct fontlist));
	*next = *prev;
	next->scmax = 0; next->sctext = NULL; next->ottext = NULL;
	next->feats = TagsCopy(prev->feats);
	prev->next = next;
	next->start = prev->end;
    }
    next->script = script;
    next->lang = lang;
    next->end = end;
    next->feats = TagsCopy(StdFeaturesOfScript(script));
}

void SFTFSetDPI(GGadget *g, float dpi) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *fd;

    if ( st->dpi == dpi )
return;
    st->dpi = dpi;
    for ( fd = st->generated; fd!=NULL; fd=fd->next ) {
	RegenFontData(st,fd);
    }
    SFTextAreaRefigureLines(st,0,-1);
    SFTextAreaShow(&st->g,st->sel_start);	/* Refigure scrollbars for new size */
	    /* And force an expose event */
}

float SFTFGetDPI(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;

return( st->dpi );
}

void SFTFRefreshFonts(GGadget *g) {
    SFTextArea *st = (SFTextArea *) g;
    FontData *fd;
    struct sfmaps *sfmaps;

    /* First regenerate the EncMaps. Glyphs might have been added or removed */
    for ( sfmaps = st->sfmaps; sfmaps!=NULL; sfmaps = sfmaps->next ) {
	EncMapFree(sfmaps->map);
	SplineCharFree(sfmaps->fake_notdef);
	sfmaps->fake_notdef = NULL;
	SFMapFill(sfmaps,sfmaps->sf);
    }

    /* Then free all old generated bitmaps */
    /* need to do this first because otherwise we might reuse a freetype context */
    for ( fd = st->generated; fd!=NULL; fd=fd->next ) {
	if ( fd->fonttype!=sftf_bitmap ) {
	    BDFFontFree(fd->bdf);
	    fd->bdf = NULL;
	}
    }
    for ( fd = st->generated; fd!=NULL; fd=fd->next ) {
	RegenFontData(st,fd);
    }
    SFTextAreaRefigureLines(st,0,-1);
    SFTextAreaShow(&st->g,st->sel_start);	/* Refigure scrollbars for new size */
	    /* And force an expose event */
}
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

SFTextArea *SFTFConvertToPrint(GGadget *g, int width, int height, int dpi) {
    SFTextArea *st = (SFTextArea *) g;
    SFTextArea *print = gcalloc(1,sizeof(SFTextArea));
    struct fontlist *fl;
    struct fontdata *fd1, *fd2;

    print->multi_line = true;
    print->accepts_returns = true;
    print->wrap = true;
    print->dpi = dpi;
    print->g.inner.width = width;
    print->g.inner.height = height;
    print->g.funcs = &sftextarea_funcs;

    print->text = u_copy(st->text);
    print->generated = FontDataCopyNoBDF(print,st->generated);
    print->fontlist = fontlistcopy(st->fontlist);
    for ( fl = print->fontlist; fl!=NULL; fl=fl->next ) {
	for ( fd1=st->generated, fd2=print->generated; fd1!=NULL && fd1!=fl->fd;
		fd1=fd1->next, fd2=fd2->next );
	fl->fd = fd2;
    }
    print->ps = -1;
    SFTextAreaRefigureLines(print,0,-1);
return( print );
}

#include "scripting.h"
void FontImage(SplineFont *sf,char *filename,Array *arr,int width,int height) {
    SFTextArea *st = gcalloc(1,sizeof(SFTextArea));
    int cnt, len, i,j, ret, p, x;
    struct fontlist *last;
    enum sftf_fonttype type = sf->order2 ? sftf_ttf : sftf_otf;
    GImage *image;
    struct _GImage *base;
    unichar_t *upt;
    uint32 script;
    struct opentype_str **line;

    if ( !hasFreeType())
	type = sftf_pfaedit;
    if ( sf->onlybitmaps && sf->bitmaps!=NULL )
	type = sftf_bitmap;

    st->multi_line = true;
    st->accepts_returns = true;
    st->wrap = true;
    st->dpi = 72;
    st->ps = -1;
    st->g.funcs = &sftextarea_funcs;
    SFMapOfSF(st,sf);

    cnt = arr->argc/2;
    len = 1;
    for ( i=0; i<cnt; ++i )
	len += utf8_strlen( arr->vals[2*i+1].u.sval )+1;
    
    st->text = galloc(len*sizeof(unichar_t));
    len = 0;
    last = NULL;
    for ( i=0; i<cnt; ++i ) {
	if ( last==NULL )
	    last = st->fontlist = gcalloc(1,sizeof(struct fontlist));
	else {
	    last->next = gcalloc(1,sizeof(struct fontlist));
	    last = last->next;
	}
	last->fd = FindFontData(st,sf,type,arr->vals[2*i].u.ival,true);
	last->start = len;

	utf82u_strcpy(st->text+len,arr->vals[2*i+1].u.sval);
	script = DEFAULT_SCRIPT;
	for ( upt = st->text+len; *upt && script!=DEFAULT_SCRIPT; ++upt )
	    script = ScriptFromUnicode(*upt,NULL);
	len += utf8_strlen( arr->vals[2*i+1].u.sval );
	st->text[len++] = '\n';

	last->end = len-1;

	last->script = script; last->lang = DEFAULT_LANG;
	last->feats = TagsCopy(StdFeaturesOfScript(script));
    }
    st->text[len++] = '\0';
    
    st->g.inner.width = st->g.r.width = width==-1 ? 0xff00 : width;
    st->g.inner.height = st->g.r.height = 1000;	/* should not matter */
    SFTextAreaRefigureLines(st,0,-1);
    if ( width==-1 )
	width = st->xmax+2;
    if ( height==-1 && st->lcnt!=0 )
	height = st->lineheights[st->lcnt-1].y + st->lineheights[st->lcnt-1].fh + 2;

    image = GImageCreate(it_index,width,height);
    base = image->u.image;
    memset(base->data,0,base->bytes_per_line*base->height);
    for ( i=0; i<256; ++i )
	base->clut->clut[i] = (255-i)*0x010101;
    base->clut->is_grey = true;
    base->clut->clut_len = 256;

    for ( i=0; i<st->lcnt; ++i ) {
	/* Does this para start out r2l or l2r? */
	p = st->lineheights[i].p;
	if ( st->paras[p].para[0]!=NULL &&
		ScriptIsRightToLeft( ((struct fontlist *) (st->paras[p].para[0]->fl))->script ))
	    x = st->xmax - st->lineheights[i].linelen;
	else
	    x = 0;
	line = st->lines[i];
	for ( j=0; line[j]!=NULL; ++j ) {
	    FDDrawChar(NULL,image,line[j],x,st->lineheights[i].y,0x000000);
	    x += line[j]->advance_width + line[j]->vr.h_adv_off;
	}
    }
#ifndef _NO_LIBPNG
    if ( strstrmatch(filename,".png")!=NULL )
	ret = GImageWritePng(image,filename,false);
    else
#endif
    if ( strstrmatch(filename,".bmp")!=NULL )
	ret = GImageWriteBmp(image,filename);
    else
	gwwv_post_error(_("Unsupported image format"),
#ifndef _NO_LIBPNG
		_("Unsupported image format must be bmp or png")
#else
		_("Unsupported image format must be bmp")
#endif
	    );
    if ( !ret )
	gwwv_post_error(_("Could not write"),_("Could not write %.100s"),filename);
    GImageDestroy(image);

    sftextarea_destroy(&st->g);
}
