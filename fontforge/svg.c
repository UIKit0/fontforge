/* Copyright (C) 2003-2005 by George Williams */
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
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <utype.h>
#include <chardata.h>
#include <ustring.h>
#include <unistd.h>
#include "sd.h"

/* ************************************************************************** */
/* ****************************    SVG Output    **************************** */
/* ************************************************************************** */

static void latin1ToUtf8Out(FILE *file,char *str) {
    /* beware of characters above 0x80, also &, <, > (things that are magic for xml) */
    while ( *str ) {
	if ( *str=='&' || *str=='<' || *str=='>' || (*str&0x80) )
	    fprintf( file, "&#%d;", (uint8) *str);
	else
	    putc(*str,file);
	++str;
    }
}

static int svg_outfontheader(FILE *file, SplineFont *sf) {
    int defwid = SFFigureDefWidth(sf,NULL);
    struct pfminfo info;
    static const char *condexp[] = { "squinchy", "ultra-condensed", "extra-condensed",
	"condensed", "semi-condensed", "normal", "semi-expanded", "expanded",
	"extra-expanded", "ultra-expanded", "broad" };
    DBounds bb;
    BlueData bd;
    char *hash, *hasv, ch;
    int minu, maxu, i;
    time_t now;
    const char *author = GetAuthor();
    extern char *source_version_str;

    SFDefaultOS2Info(&info,sf,sf->fontname);
    SplineFontFindBounds(sf,&bb);
    QuickBlues(sf,&bd);

    fprintf( file, "<?xml version=\"1.0\" standalone=\"no\"?>\n" );
    fprintf( file, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\" >\n" );
    if ( sf->comments!=NULL ) {
	fprintf( file, "<!--\n" );
	latin1ToUtf8Out(file,sf->comments);
	fprintf( file, "\n-->\n" );
    }
    fprintf( file, "<svg>\n" );
    time(&now);
    fprintf( file, "<metadata>\nCreated by FontForge %s at %s",
	    source_version_str, ctime(&now) );
    if ( author!=NULL )
	fprintf(file," By %s\n", author);
    else
	fprintf(file,"\n" );
    if ( sf->copyright!=NULL ) {
	latin1ToUtf8Out(file,sf->copyright);
	putc('\n',file);
    }
    fprintf( file, "</metadata>\n" );
    fprintf( file, "<defs>\n" );
    fprintf( file, "<font id=\"%s\" horiz-adv-x=\"%d\" ", sf->fontname, defwid );
    if ( sf->hasvmetrics )
	fprintf( file, "vert-adv-y=\"%d\" ", sf->ascent+sf->descent );
    putc('>',file); putc('\n',file);
    fprintf( file, "  <font-face \n" );
    fprintf( file, "    font-family=\"%s\"\n", sf->familyname );
    fprintf( file, "    font-weight=\"%d\"\n", info.weight );
    if ( strstrmatch(sf->fontname,"obli") || strstrmatch(sf->fontname,"slanted") )
	fprintf( file, "    font-style=\"oblique\"\n" );
    else if ( MacStyleCode(sf,NULL)&sf_italic )
	fprintf( file, "    font-style=\"italic\"\n" );
    if ( strstrmatch(sf->fontname,"small") || strstrmatch(sf->fontname,"cap") )
	fprintf( file, "    font-variant=small-caps\n" );
    fprintf( file, "    font-stretch=\"%s\"\n", condexp[info.width]);
    fprintf( file, "    units-per-em=\"%d\"\n", sf->ascent+sf->descent );
    fprintf( file, "    panose-1=\"%d %d %d %d %d %d %d %d %d %d\"\n", info.panose[0],
	info.panose[1], info.panose[2], info.panose[3], info.panose[4], info.panose[5],
	info.panose[6], info.panose[7], info.panose[8], info.panose[9]);
    fprintf( file, "    ascent=\"%d\"\n", sf->ascent );
    fprintf( file, "    descent=\"%d\"\n", -sf->descent );
    fprintf( file, "    x-height=\"%g\"\n", bd.xheight );
    fprintf( file, "    cap-height=\"%g\"\n", bd.caph );
    fprintf( file, "    bbox=\"%g %g %g %g\"\n", bb.minx, bb.miny, bb.maxx, bb.maxy );
    fprintf( file, "    underline-thickness=\"%g\"\n", sf->uwidth );
    fprintf( file, "    underline-position=\"%g\"\n", sf->upos );
    if ( sf->italicangle!=0 )
	fprintf(file, "    slope=\"%g\"\n", sf->italicangle );
    hash = PSDictHasEntry(sf->private,"StdHW");
    hasv = PSDictHasEntry(sf->private,"StdVW");
    if ( hash!=NULL ) {
	if ( *hash=='[' ) ++hash;
	ch = hash[strlen(hash)-1];
	if ( ch==']' ) hash[strlen(hash)-1] = '\0';
	fprintf(file, "    stemh=\"%s\"\n", hash );
	if ( ch==']' ) hash[strlen(hash)] = ch;
    }
    if ( hasv!=NULL ) {
	if ( *hasv=='[' ) ++hasv;
	ch = hasv[strlen(hasv)-1];
	if ( ch==']' ) hasv[strlen(hasv)-1] = '\0';
	fprintf(file, "    stemv=\"%s\"\n", hasv );
	if ( ch==']' ) hasv[strlen(hasv)] = ch;
    }
    minu = 0x7fffff; maxu = 0;
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && sf->chars[i]->unicodeenc>0 ) {
	if ( sf->chars[i]->unicodeenc<minu ) minu = sf->chars[i]->unicodeenc;
	if ( sf->chars[i]->unicodeenc>maxu ) maxu = sf->chars[i]->unicodeenc;
    }
    if ( maxu!=0 )
	fprintf(file, "    unicode-range=\"U+%04X-U+%04X\"\n", minu, maxu );
    fprintf( file, "  />\n" );
return( defwid );
}

static int svg_pathdump(FILE *file, SplineSet *spl, int lineout) {
    BasePoint last;
    char buffer[60];
    int closed=false;
    Spline *sp, *first;
    /* as I see it there is nothing to be gained by optimizing out the */
    /* command characters, since they just have to be replaced by spaces */
    /* so I don't bother to */

    last.x = last.y = 0;
    while ( spl!=NULL ) {
	sprintf( buffer, "M%g %g", spl->first->me.x, spl->first->me.y );
	if ( lineout+strlen(buffer)>=255 ) { putc('\n',file); lineout = 0; }
	fputs( buffer,file );
	lineout += strlen( buffer );
	last = spl->first->me;
	closed = false;

	first = NULL;
	for ( sp = spl->first->next; sp!=NULL && sp!=first; sp = sp->to->next ) {
	    if ( first==NULL ) first=sp;
	    if ( sp->knownlinear ) {
		if ( sp->to->me.x==sp->from->me.x )
		    sprintf( buffer,"v%g", sp->to->me.y-last.y );
		else if ( sp->to->me.y==sp->from->me.y )
		    sprintf( buffer,"h%g", sp->to->me.x-last.x );
		else if ( sp->to->next==first ) {
		    strcpy( buffer, "z");
		    closed = true;
		} else
		    sprintf( buffer,"l%g %g", sp->to->me.x-last.x, sp->to->me.y-last.y );
	    } else if ( sp->order2 ) {
		if ( sp->from->prev!=NULL && sp->from!=spl->first &&
			sp->from->me.x-sp->from->prevcp.x == sp->from->nextcp.x-sp->from->me.x &&
			sp->from->me.y-sp->from->prevcp.y == sp->from->nextcp.y-sp->from->me.y )
		    sprintf( buffer,"t%g %g", sp->to->me.x-last.x, sp->to->me.y-last.y );
		else
		    sprintf( buffer,"q%g %g %g %g",
			    sp->to->prevcp.x-last.x, sp->to->prevcp.y-last.y,
			    sp->to->me.x-last.x,sp->to->me.y-last.y);
	    } else {
		if ( sp->from->prev!=NULL && sp->from!=spl->first &&
			sp->from->me.x-sp->from->prevcp.x == sp->from->nextcp.x-sp->from->me.x &&
			sp->from->me.y-sp->from->prevcp.y == sp->from->nextcp.y-sp->from->me.y )
		    sprintf( buffer,"s%g %g %g %g",
			    sp->to->prevcp.x-last.x, sp->to->prevcp.y-last.y,
			    sp->to->me.x-last.x,sp->to->me.y-last.y);
		else
		    sprintf( buffer,"c%g %g %g %g %g %g",
			    sp->from->nextcp.x-last.x, sp->from->nextcp.y-last.y,
			    sp->to->prevcp.x-last.x, sp->to->prevcp.y-last.y,
			    sp->to->me.x-last.x,sp->to->me.y-last.y);
	    }
	    if ( lineout+strlen(buffer)>=255 ) { putc('\n',file); lineout = 0; }
	    fputs( buffer,file );
	    lineout += strlen( buffer );
	    last = sp->to->me;
	}
	if ( !closed ) {
	    if ( lineout>=254 ) { putc('\n',file); lineout=0; }
	    putc('z',file);
	    ++lineout;
	}
	spl = spl->next;
    }
return( lineout );
}

#ifdef FONTFORGE_CONFIG_TYPE3
static void svg_dumpstroke(FILE *file, struct pen *cpen, struct pen *fallback) {
    static char *joins[] = { "miter", "round", "bevel", "inherit", NULL };
    static char *caps[] = { "butt", "round", "square", "inherit", NULL };
    struct pen pen;

    pen = *cpen;
    if ( fallback!=NULL ) {
	if ( pen.brush.col == COLOR_INHERITED ) pen.brush.col = fallback->brush.col;
	if ( pen.brush.opacity <0 ) pen.brush.opacity = fallback->brush.opacity;
	if ( pen.width == WIDTH_INHERITED ) pen.width = fallback->width;
	if ( pen.linecap == lc_inherited ) pen.linecap = fallback->linecap;
	if ( pen.linejoin == lj_inherited ) pen.linejoin = fallback->linejoin;
	if ( pen.dashes[0]==0 && pen.dashes[1]==DASH_INHERITED )
	    memcpy(pen.dashes,fallback->dashes,sizeof(pen.dashes));
    }

    if ( pen.brush.col!=COLOR_INHERITED )
	fprintf( file, "stroke=\"#%02x%02x%02x\" ",
		COLOR_RED(pen.brush.col), COLOR_GREEN(pen.brush.col), COLOR_BLUE(pen.brush.col));
    else
	fprintf( file, "stroke=\"currentColor\" " );
    if ( pen.brush.opacity>=0 )
	fprintf( file, "stroke-opacity=\"%g\" ", pen.brush.opacity);
    if ( pen.width!=WIDTH_INHERITED )
	fprintf( file, "stroke-width=\"%g\" ", pen.width );
    if ( pen.linecap!=lc_inherited )
	fprintf( file, "stroke-linecap=\"%s\" ", caps[pen.linecap] );
    if ( pen.linejoin!=lc_inherited )
	fprintf( file, "stroke-linejoin=\"%s\" ", joins[pen.linejoin] );
/* the current transformation matrix will not affect the fill, but it will */
/*  affect the way stroke looks. So we must include it here. BUT the spline */
/*  set has already been transformed, so we must apply the inverse transform */
/*  to the splineset before outputting it, so that applying the transform */
/*  will give us the splines we desire. */
    if ( pen.trans[0]!=1.0 || pen.trans[3]!=1.0 || pen.trans[1]!=0 || pen.trans[2]!=0 )
	fprintf( file, "transform=\"matrix(%g, %g, %g, %g, 0, 0)\" ",
		pen.trans[0], pen.trans[1], pen.trans[2], pen.trans[3] );
    if ( pen.dashes[0]==0 && pen.dashes[1]==DASH_INHERITED ) {
	fprintf( file, "stroke-dasharray=\"inherit\" " );
    } else if ( pen.dashes[0]!=0 ) {
	int i;
	fprintf( file, "stroke-dasharray=\"" );
	for ( i=0; i<DASH_MAX && pen.dashes[i]!=0; ++i )
	    fprintf( file, "%d ", pen.dashes[i]);
	fprintf( file,"\" ");
    } else
	/* fprintf( file, "stroke-dasharray=\"none\" " )*/;	/* That's the default, don't need to say it */
}

static void svg_dumpfill(FILE *file, struct brush *cbrush, struct brush *fallback,
	int dofill) {
    struct brush brush;

    if ( !dofill ) {
	fprintf( file, "fill=\"none\" " );
return;
    }
    
    brush = *cbrush;
    if ( fallback!=NULL ) {
	if ( brush.col==COLOR_INHERITED ) brush.col = fallback->col;
	if ( brush.opacity<0 ) brush.opacity = fallback->opacity;
    }

    if ( brush.col!=COLOR_INHERITED )
	fprintf( file, "fill=\"#%02x%02x%02x\" ",
		COLOR_RED(brush.col), COLOR_GREEN(brush.col), COLOR_BLUE(brush.col));
    else
	fprintf( file, "fill=\"currentColor\" " );
    if ( brush.opacity>=0 )
	fprintf( file, "fill-opacity=\"%g\" ", brush.opacity);
}

static SplineSet *TransBy(SplineSet *ss, real trans[4] ) {
    real inversetrans[6], transform[6];

    if ( trans[0]==1.0 && trans[3]==1.0 && trans[1]==0 && trans[2]==0 )
return( ss );
    memcpy(transform,trans,4*sizeof(real));
    transform[4] = transform[5] = 0;
    MatInverse(inversetrans,transform);
return( SplinePointListTransform(SplinePointListCopy(
		    ss),inversetrans,true));
}
#endif

static int svg_sc_any(SplineChar *sc) {
    int i,j;
    int any;
    RefChar *ref;

    any = false;
    for ( i=ly_fore; i<sc->layer_cnt && !any; ++i ) {
	any = sc->layers[i].splines!=NULL;
	for ( ref=sc->layers[i].refs ; ref!=NULL && !any; ref = ref->next )
	    for ( j=0; j<ref->layer_cnt && !any; ++j )
		any = ref->layers[j].splines!=NULL;
    }
return( any );
}

static void svg_scpathdump(FILE *file, SplineChar *sc,char *endpath) {
    RefChar *ref;
    int lineout;
#ifdef FONTFORGE_CONFIG_TYPE3
    int i,j;
    SplineSet *transed;
#endif

    if ( !svg_sc_any(sc) ) {
	/* I think a space is represented by leaving out the d (path) entirely*/
	/*  rather than having d="" */
	fputs(" />\n",file);
    } else if ( !sc->parent->multilayer ) {
	fprintf( file,"d=\"");
	lineout = svg_pathdump(file,sc->layers[ly_fore].splines,3);
	for ( ref= sc->layers[ly_fore].refs; ref!=NULL; ref=ref->next )
	    lineout = svg_pathdump(file,ref->layers[0].splines,lineout);
	if ( lineout>=255-4 ) putc('\n',file );
	putc('"',file);
	fputs(" />\n",file);
    } else {
	putc('>',file);
#ifdef FONTFORGE_CONFIG_TYPE3
	for ( i=ly_fore; i<sc->layer_cnt ; ++i ) {
	    if ( sc->layers[i].splines!=NULL ) {
		fprintf(file, "  <g " );
		transed = sc->layers[i].splines;
		if ( sc->layers[i].dostroke ) {
		    svg_dumpstroke(file,&sc->layers[i].stroke_pen,NULL);
		    transed = TransBy(transed,sc->layers[i].stroke_pen.trans);
		}
		svg_dumpfill(file,&sc->layers[i].fill_brush,NULL,sc->layers[i].dofill);
		fprintf( file, ">\n" );
		fprintf(file, "  <path d=\"\n");
		svg_pathdump(file,transed,12);
		fprintf(file, "\"/>\n" );
		if ( transed!=sc->layers[i].splines )
		    SplinePointListsFree(transed);
		fprintf(file, "  </g>\n" );
	    }
	    for ( ref=sc->layers[i].refs ; ref!=NULL; ref = ref->next ) {
		for ( j=0; j<ref->layer_cnt; ++j ) if ( ref->layers[j].splines!=NULL ) {
		    fprintf(file, "   <g " );
		    transed = ref->layers[j].splines;
		    if ( ref->layers[j].dostroke ) {
			svg_dumpstroke(file,&ref->layers[j].stroke_pen,&sc->layers[i].stroke_pen);
			transed = TransBy(transed,ref->layers[j].stroke_pen.trans);
		    }
		    svg_dumpfill(file,&ref->layers[j].fill_brush,&sc->layers[i].fill_brush,ref->layers[j].dofill);
		    fprintf( file, ">\n" );
		    fprintf(file, "  <path d=\"\n");
		    svg_pathdump(file,transed,12);
		    fprintf(file, "\"/>\n" );
		    if ( transed!=ref->layers[j].splines )
			SplinePointListsFree(transed);
		    fprintf(file, "   </g>\n" );
		}
	    }
	}
#endif
	fputs(endpath,file);
    }
}

static int LigCnt(SplineFont *sf,PST *lig,int32 *univals,int max) {
    char *pt, *end;
    int c=0;
    SplineChar *sc;

    if ( lig->type!=pst_ligature )
return( 0 );
    else if ( lig->tag!=CHR('l','i','g','a') && lig->tag!=CHR('r','l','i','g'))
return( 0 );
    pt = lig->u.lig.components;
    forever {
	end = strchr(pt,' ');
	if ( end!=NULL ) *end='\0';
	sc = SFGetCharDup(sf,-1,pt);
	if ( end!=NULL ) *end=' ';
	if ( sc==NULL || sc->unicodeenc==-1 )
return( 0 );
	if ( c>=max )
return( 0 );
	univals[c++] = sc->unicodeenc;
	if ( end==NULL )
return( c );
	pt = end+1;
	while ( *pt==' ' ) ++pt;
    }
}

static PST *HasLigature(SplineChar *sc) {
    PST *pst, *best=NULL;
    int bestc=0,c;
    int32 univals[50];

    for ( pst=sc->possub; pst!=NULL; pst=pst->next ) {
	if ( pst->type==pst_ligature ) {
	    c = LigCnt(sc->parent,pst,univals,sizeof(univals)/sizeof(univals[0]));
	    if ( c>1 && c>bestc ) {
		c = bestc;
		best = pst;
	    }
	}
    }
return( best );
}

static void svg_scdump(FILE *file, SplineChar *sc,int defwid) {
    PST *best=NULL;
    const unichar_t *alt;
    int32 univals[50];
    int i, c, uni;

    best = HasLigature(sc);
    if ( sc->comment!=NULL ) {
	char *temp = u2utf8_copy(sc->comment);
	fprintf( file, "\n<!--\n%s\n-->\n",temp );
	free(temp);
    }
    fprintf(file,"    <glyph glyph-name=\"%s\" ",sc->name );
    if ( best!=NULL ) {
	c = LigCnt(sc->parent,best,univals,sizeof(univals)/sizeof(univals[0]));
	fputs("unicode=\"",file);
	for ( i=0; i<c; ++i )
	    if ( univals[i]>='A' && univals[i]<'z' )
		putc(univals[i],file);
	    else
		fprintf(file,"&#x%x;",univals[i]);
	fputs("\" ",file);
    } else if ( sc->unicodeenc!=-1 && sc->unicodeenc<0x110000 ) {
	if ( sc->unicodeenc!=0x9 &&
		sc->unicodeenc!=0xa &&
		sc->unicodeenc!=0xd &&
		!(sc->unicodeenc>=0x20 && sc->unicodeenc<=0xd7ff) &&
		!(sc->unicodeenc>=0xe000 && sc->unicodeenc<=0xfffd) &&
		!(sc->unicodeenc>=0x10000 && sc->unicodeenc<=0x10ffff) )
	    /* Not allowed in XML */;
	else if ( (sc->unicodeenc>=0x7f && sc->unicodeenc<=0x84) ||
		  (sc->unicodeenc>=0x86 && sc->unicodeenc<=0x9f) ||
		  (sc->unicodeenc>=0xfdd0 && sc->unicodeenc<=0xfddf) ||
		  (sc->unicodeenc&0xffff)==0xfffe ||
		  (sc->unicodeenc&0xffff)==0xffff )
	    /* Not recommended in XML */;
	else if ( sc->unicodeenc>=32 && sc->unicodeenc<127 &&
		sc->unicodeenc!='"' && sc->unicodeenc!='&' &&
		sc->unicodeenc!='<' && sc->unicodeenc!='>' )
	    fprintf( file, "unicode=\"%c\" ", sc->unicodeenc);
	else if ( sc->unicodeenc<0x10000 &&
		( isarabisolated(sc->unicodeenc) || isarabinitial(sc->unicodeenc) || isarabmedial(sc->unicodeenc) || isarabfinal(sc->unicodeenc) ) &&
		unicode_alternates[sc->unicodeenc>>8]!=NULL &&
		(alt = unicode_alternates[sc->unicodeenc>>8][uni&0xff])!=NULL &&
		alt[1]=='\0' )
	    /* For arabic forms use the base representation in the 0600 block */
	    fprintf( file, "unicode=\"&#x%x;\" ", alt[0]);
	else
	    fprintf( file, "unicode=\"&#x%x;\" ", sc->unicodeenc);
    }
    if ( sc->width!=defwid )
	fprintf( file, "horiz-adv-x=\"%d\" ", sc->width );
    if ( sc->parent->hasvmetrics && sc->vwidth!=sc->parent->ascent+sc->parent->descent )
	fprintf( file, "vert-adv-y=\"%d\" ", sc->vwidth );
    if ( strstr(sc->name,".vert")!=NULL || strstr(sc->name,".vrt2")!=NULL )
	fprintf( file, "orientation=\"v\" " );
    if ( sc->unicodeenc!=-1 && sc->unicodeenc<0x10000 ) {
	if ( isarabinitial(sc->unicodeenc))
	    fprintf( file,"arabic-form=initial " );
	else if ( isarabmedial(sc->unicodeenc))
	    fprintf( file,"arabic-form=medial ");
	else if ( isarabfinal(sc->unicodeenc))
	    fprintf( file,"arabic-form=final ");
	else if ( isarabisolated(sc->unicodeenc))
	    fprintf( file,"arabic-form=isolated ");
    }
    putc('\n',file);
    svg_scpathdump(file,sc," </glyph>\n");
}

static void svg_notdefdump(FILE *file, SplineFont *sf,int defwid) {
    
    if ( SCWorthOutputting(sf->chars[0]) && SCIsNotdef(sf->chars[0],-1 )) {
	SplineChar *sc = sf->chars[0];

	fprintf(file, "<missing-glyph ");
	if ( sc->width!=defwid )
	    fprintf( file, "horiz-adv-x=\"%d\" ", sc->width );
	if ( sc->parent->hasvmetrics && sc->vwidth!=sc->parent->ascent+sc->parent->descent )
	    fprintf( file, "vert-adv-y=\"%d\" ", sc->vwidth );
	putc('\n',file);
	svg_scpathdump(file,sc," </glyph>\n");
    } else {
	/* We'll let both the horiz and vert advances default to the values */
	/*  specified by the font, and I think a space is done by omitting */
	/*  d (the path) altogether */
	fprintf(file,"    <missing-glyph />\n");	/* Is this a blank space? */
    }
}

static void fputkerns( FILE *file, char *names) {
    while ( *names ) {
	if ( *names==' ' ) {
	    putc(',',file);
	    while ( names[1]==' ' ) ++names;
	} else
	    putc(*names,file);
	++names;
    }
}

static void svg_dumpkerns(FILE *file,SplineFont *sf,int isv) {
    int i,j;
    KernPair *kp;
    KernClass *kc;

    for ( i=0; i<sf->charcnt; ++i ) if ( SCWorthOutputting(sf->chars[i]) ) {
	for ( kp = isv ? sf->chars[i]->vkerns : sf->chars[i]->kerns;
		kp!=NULL; kp = kp->next )
	    if ( kp->off!=0 && SCWorthOutputting(kp->sc)) {
		fprintf( file, isv ? "    <vkern " : "    <hkern " );
		if ( sf->chars[i]->unicodeenc==-1 || HasLigature(sf->chars[i]))
		    fprintf( file, "g1=\"%s\" ", sf->chars[i]->name );
		else if ( sf->chars[i]->unicodeenc>='A' && sf->chars[i]->unicodeenc<='z' )
		    fprintf( file, "u1=\"%c\" ", sf->chars[i]->unicodeenc );
		else
		    fprintf( file, "u1=\"&#x%x;\" ", sf->chars[i]->unicodeenc );
		if ( kp->sc->unicodeenc==-1 || HasLigature(kp->sc))
		    fprintf( file, "g2=\"%s\" ", kp->sc->name );
		else if ( kp->sc->unicodeenc>='A' && kp->sc->unicodeenc<='z' )
		    fprintf( file, "u2=\"%c\" ", kp->sc->unicodeenc );
		else
		    fprintf( file, "u2=\"&#x%x;\" ", kp->sc->unicodeenc );
		fprintf( file, "k=\"%d\" />\n", -kp->off );
	    }
    }

    for ( kc=isv ? sf->vkerns : sf->kerns; kc!=NULL; kc=kc->next ) {
	for ( i=1; i<kc->first_cnt; ++i ) for ( j=1; j<kc->second_cnt; ++j ) {
	    if ( kc->offsets[i*kc->first_cnt+j]!=0 ) {
		fprintf( file, isv ? "    <vkern g1=\"" : "    <hkern g1=\"" );
		fputkerns( file, kc->firsts[i]);
		fprintf( file, "\"\n\tg2=\"" );
		fputkerns( file, kc->seconds[j]);
		fprintf( file, "\"\n\tk=\"%d\" />\n",
			-kc->offsets[i*kc->first_cnt+j]);
	    }
	}
    }
}

static void svg_outfonttrailer(FILE *file,SplineFont *sf) {
    fprintf(file,"  </font>\n");
    fprintf(file,"</defs></svg>\n" );
}

static void svg_sfdump(FILE *file,SplineFont *sf) {
    int defwid, i;
    char *oldloc;

    oldloc = setlocale(LC_NUMERIC,"C");

    defwid = svg_outfontheader(file,sf);
    svg_notdefdump(file,sf,defwid);

    /* Ligatures must be output before their initial components */
    for ( i=0; i<sf->charcnt; ++i ) {
	if ( SCWorthOutputting(sf->chars[i]) && HasLigature(sf->chars[i]))
	    svg_scdump(file, sf->chars[i],defwid);
    }
    /* And formed arabic before unformed */
    for ( i=0; i<sf->charcnt; ++i ) {
	if ( SCWorthOutputting(sf->chars[i]) && !HasLigature(sf->chars[i]) &&
		sf->chars[i]->unicodeenc!=-1 && sf->chars[i]->unicodeenc<0x10000 &&
		(isarabinitial(sf->chars[i]->unicodeenc) ||
		 isarabmedial(sf->chars[i]->unicodeenc) ||
		 isarabfinal(sf->chars[i]->unicodeenc) ||
		 isarabisolated(sf->chars[i]->unicodeenc)))
	    svg_scdump(file, sf->chars[i],defwid);
    }
    for ( i=0; i<sf->charcnt; ++i ) {
	if ( i==0 && sf->chars[i]!=NULL && strcmp(sf->chars[i]->name,".notdef")==0 )
    continue;
	if ( SCWorthOutputting(sf->chars[i]) && !HasLigature(sf->chars[i]) &&
		(sf->chars[i]->unicodeenc==-1 || sf->chars[i]->unicodeenc>=0x10000 ||
		!(isarabinitial(sf->chars[i]->unicodeenc) ||
		 isarabmedial(sf->chars[i]->unicodeenc) ||
		 isarabfinal(sf->chars[i]->unicodeenc) ||
		 isarabisolated(sf->chars[i]->unicodeenc))))
	    svg_scdump(file, sf->chars[i],defwid);
    }
    svg_dumpkerns(file,sf,false);
    svg_dumpkerns(file,sf,true);
    svg_outfonttrailer(file,sf);
    setlocale(LC_NUMERIC,oldloc);
}

int WriteSVGFont(char *fontname,SplineFont *sf,enum fontformat format,int flags) {
    FILE *file;
    int ret;

    if (( file=fopen(fontname,"w+"))==NULL )
return( 0 );
    svg_sfdump(file,sf);
    ret = true;
    if ( ferror(file))
	ret = false;
    if ( fclose(file)==-1 )
return( 0 );
return( ret );
}

int _ExportSVG(FILE *svg,SplineChar *sc) {
    char *oldloc, *end;
    int em_size;
    DBounds b;

    SplineCharFindBounds(sc,&b);
    em_size = sc->parent->ascent+sc->parent->descent;
    if ( b.minx>0 ) b.minx=0;
    if ( b.maxx<em_size ) b.maxx = em_size;
    if ( b.miny>-sc->parent->descent ) b.miny = -sc->parent->descent;
    if ( b.maxy<em_size ) b.maxy = em_size;

    oldloc = setlocale(LC_NUMERIC,"C");
    fprintf(svg, "<?xml version=\"1.0\" standalone=\"no\"?>\n" );
    fprintf(svg, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\" >\n" ); 
    fprintf(svg, "<svg viewBox=\"%d %d %d %d\">\n",
	    (int) floor(b.minx), (int) floor(b.miny),
	    (int) ceil(b.maxx), (int) ceil(b.maxy));
    fprintf(svg, "  <g transform=\"matrix(1 0 0 -1 0 %d)\">\n",
	    sc->parent->ascent );
#if 0		/* Used to show the advance width, but as I don't in eps, probably should be consistent */
    fprintf(svg, "   <g stroke=\"green\" stroke-width=\"1\">\n" );
    fprintf(svg, "     <line x1=\"0\" y1=\"0\" x2=\"%d\" y2=\"0\" />\n", sc->width );
    fprintf(svg, "     <line x1=\"0\" y1=\"10\" x2=\"0\" y2=\"-10\" />\n" );
    fprintf(svg, "     <line x1=\"%d\" y1=\"10\" x2=\"%d\" y2=\"-10\" />\n",
	    sc->width, sc->width );
    fprintf(svg, "   </g>\n\n" );
#endif
    if ( !sc->parent->multilayer || !svg_sc_any(sc)) {
	fprintf(svg, "   <path fill=\"currentColor\"\n");
	end = "   </path>\n";
    } else {
	fprintf(svg, "   <g ");
	end = "   </g>\n";
    }
    svg_scpathdump(svg,sc,end);
    fprintf(svg, "  </g>\n\n" );
    fprintf(svg, "</svg>\n" );

    setlocale(LC_NUMERIC,oldloc);
return( !ferror(svg));
}

/* ************************************************************************** */
/* *****************************    SVG Input    **************************** */
/* ************************************************************************** */

#if _NO_LIBXML
SplineFont *SFReadSVG(char *filename, int flags) {
return( NULL );
}

char **NamesReadSVG(char *filename) {
return( NULL );
}

SplineSet *SplinePointListInterpretSVG(char *filename,int em_size,int ascent) {
return( NULL );
}
#else

#ifndef HAVE_ICONV_H
# undef iconv
# undef iconv_t
# undef iconv_open
# undef iconv_close
#endif

#include <libxml/parser.h>

/* Ok, this complication is here because:				    */
/*	1) I want to be able to deal with static libraries on some systems  */
/*	2) If I've got a dynamic library and I compile up an executable     */
/*		I want it to run on systems without libxml2		    */
/* So one case boils down to linking against the standard names, while the  */
/*  other does the link at run time if it's possible */

# if defined(_STATIC_LIBXML) || defined(NODYNAMIC)

#define _xmlParseFile		xmlParseFile
#define _xmlDocGetRootElement	xmlDocGetRootElement
#define _xmlFreeDoc		xmlFreeDoc
#ifdef __CygWin
# define _xmlFree		free
/* Nasty kludge, but xmlFree doesn't work on cygwin (or I can't get it to) */
#else
# define _xmlFree		xmlFree
#endif
#define _xmlStrcmp		xmlStrcmp
#define _xmlGetProp		xmlGetProp

static int libxml_init_base() {
return( true );
}
# else
#  include <dynamic.h>

static DL_CONST void *libxml;
static xmlDocPtr (*_xmlParseFile)(const char *filename);
static xmlNodePtr (*_xmlDocGetRootElement)(xmlDocPtr doc);
static void (*_xmlFreeDoc)(xmlDocPtr doc);
static void (*_xmlFree)(void *);
static int (*_xmlStrcmp)(const xmlChar *,const xmlChar *);
static xmlChar *(*_xmlGetProp)(xmlNodePtr,const xmlChar *);

static int libxml_init_base() {
    static int xmltested = false;

    if ( xmltested )
return( libxml!=NULL );

    libxml = dlopen("libxml2" SO_EXT,RTLD_LAZY);
    xmltested = true;
    if ( libxml==NULL )
return( false );

    _xmlParseFile = (xmlDocPtr (*)(const char *)) dlsym(libxml,"xmlParseFile");
    _xmlDocGetRootElement = (xmlNodePtr (*)(xmlDocPtr )) dlsym(libxml,"xmlDocGetRootElement");
    _xmlFreeDoc = (void (*)(xmlDocPtr)) dlsym(libxml,"xmlFreeDoc");
    /* xmlFree is done differently for threaded and non-threaded libraries. */
    /*  I hope this gets both right... */
    if ( dlsym(libxml,"__xmlFree")) {
	xmlFreeFunc *(*foo)(void) = (xmlFreeFunc *(*)(void)) dlsym(libxml,"__xmlFree");
	_xmlFree = *(*foo)();
    } else {
	xmlFreeFunc *foo = dlsym(libxml,"xmlFree");
	_xmlFree = *foo;
    }
    _xmlStrcmp = (int (*)(const xmlChar *,const xmlChar *)) dlsym(libxml,"xmlStrcmp");
    _xmlGetProp = (xmlChar *(*)(xmlNodePtr,const xmlChar *)) dlsym(libxml,"xmlGetProp");
    if ( _xmlParseFile==NULL || _xmlDocGetRootElement==NULL || _xmlFree==NULL ) {
	libxml = NULL;
return( false );
    }

return( true );
}
# endif

/* Find a node with the given id */
static xmlNodePtr XmlFindID(xmlNodePtr xml, char *name) {
    xmlChar *id;
    xmlNodePtr child, ret;

    id = _xmlGetProp(xml,(xmlChar *) "id");
    if ( id!=NULL && _xmlStrcmp(id,(xmlChar *) name)==0 ) {
	_xmlFree(id);
return( xml );
    }
    if ( id!=NULL )
	_xmlFree(id);

    for ( child = xml->children; child!=NULL; child=child->next ) {
	ret = XmlFindID(child,name);
	if ( ret!=NULL )
return( ret );
    }
return( NULL );
}

/* We want to look for "font" nodes within "svg" nodes. Since "svg" nodes may */
/*  be embedded within another xml/html document there may be several of them */
/*  and there may be several fonts within each */
static int _FindSVGFontNodes(xmlNodePtr node,xmlNodePtr *fonts,int cnt, int max,
	char *nodename) {
    if ( _xmlStrcmp(node->name,(const xmlChar *) nodename)==0 ) {
	if ( strcmp(nodename,"svg")==0 )
	    nodename = "font";
	else {
	    fonts[cnt++] = node;
	    if ( cnt>=max )
return( cnt );
	}
    }

    for ( node=node->children; node!=NULL; node=node->next ) {
	cnt = _FindSVGFontNodes(node,fonts,cnt,max,nodename);
	if ( cnt>=max )
return( cnt );
    }
return( cnt );
}

static xmlNodePtr *FindSVGFontNodes(xmlDocPtr doc) {
    xmlNodePtr *fonts=NULL;
    int cnt;

    fonts = gcalloc(100,sizeof(xmlNodePtr));	/* If the file has more than 100 fonts in it then it's foolish to expect the user to pick out one, so let's limit ourselves to 100 */
    cnt = _FindSVGFontNodes(_xmlDocGetRootElement(doc),fonts,0,100,"svg");
    if ( cnt==0 ) {
	free(fonts);
return( NULL );
    }
return( fonts );
}

static xmlNodePtr SVGPickFont(xmlNodePtr *fonts,char *filename) {
    int cnt;
    unichar_t **names;
    xmlChar *name;
    char *pt, *lparen;
    int choice;

    for ( cnt=0; fonts[cnt]!=NULL; ++cnt);
    names = galloc((cnt+1)*sizeof(unichar_t *));
    for ( cnt=0; fonts[cnt]!=NULL; ++cnt) {
	name = _xmlGetProp(fonts[cnt],(xmlChar *) "id");
	if ( name==NULL ) {
	    names[cnt] = uc_copy("nameless-font");
	} else {
	    names[cnt] = utf82u_copy((char *) name);
	    _xmlFree(name);
	}
    }
    names[cnt] = NULL;

    choice = -1;
    pt = strrchr(filename,'/');
    if ( pt==NULL ) pt = filename;
    if ( (lparen = strchr(pt,'('))!=NULL && strchr(lparen,')')!=NULL ) {
	char *find = copy(lparen+1);
	pt = strchr(find,')');
	if ( pt!=NULL ) *pt='\0';
	for ( choice=cnt-1; choice>=0; --choice )
	    if ( uc_strcmp(names[choice],find)==0 )
	break;
	if ( choice==-1 ) {
	    char *fn = copy(filename);
	    fn[lparen-filename] = '\0';
#if defined(FONTFORGE_CONFIG_GTK)
	    gwwv_post_error(_("Not in Collection"),_("%s is not in %.100s"),find,fn);
#else
	    GWidgetErrorR(_STR_NotInCollection,_STR_FontNotInCollection,find,fn);
#endif
	    free(fn);
	}
	free(find);
#if defined(FONTFORGE_CONFIG_GDRAW)
    } else if ( no_windowing_ui )
	choice = 0;
    else
	choice = GWidgetChoicesR(_STR_PickFont,(const unichar_t **) names,cnt,0,_STR_MultipleFontsPick);
#elif defined(FONTFORGE_CONFIG_GTK)
    } else if ( no_windowing_ui )
	choice = 0;
    else
	choice = gwwv_choose(_("Pick a font, any font..."),(const unichar_t **) names,cnt,0,_("There are multiple fonts in this file, pick one"));
#elif defined(FONTFORGE_CONFIG_NO_WINDOWING_UI)
    } else
	choice = 0;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    for ( cnt=0; names[cnt]!=NULL; ++cnt )
	free(names[cnt]);
    free(names);
    if ( choice!=-1 )
return( fonts[choice] );

return( NULL );
}

#define PI	3.1415926535897932

/* I don't see where the spec says that the seperator between numbers is */
/*  comma or whitespace (both is ok too) */
/* But the style sheet spec says it, so I probably just missed it */
static char *skipcomma(char *pt) {
    while ( isspace(*pt))++pt;
    if ( *pt==',' ) ++pt;
return( pt );
}

static void SVGTraceArc(SplineSet *cur,BasePoint *current,
	double x,double y,double rx,double ry,double axisrot,
	int large_arc,int sweep) {
    double cosr, sinr;
    double x1p, y1p;
    double lambda, factor;
    double cxp, cyp, cx, cy;
    double tmpx, tmpy, t2x, t2y;
    double startangle, delta, a;
    SplinePoint *final, *sp;
    BasePoint arcp[4], prevcp[4], nextcp[4], firstcp[2];
    int i, j, ia, firstia;
    static double sines[] = { 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1 };
    static double cosines[]={ 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1, 0 };

    final = SplinePointCreate(x,y);
    if ( rx < 0 ) rx = -rx;
    if ( ry < 0 ) ry = -ry;
    if ( rx!=0 && ry!=0 ) {
	/* Page 647 in the SVG 1.1 spec describes how to do this */
	/* This is Appendix F (Implementation notes) section 6.5 */
	cosr = cos(axisrot); sinr = sin(axisrot);
	x1p = cosr*(current->x-x)/2 + sinr*(current->y-y)/2;
	y1p =-sinr*(current->x-x)/2 + cosr*(current->y-y)/2;
	/* Correct for bad radii */
	lambda = x1p*x1p/(rx*rx) + y1p*y1p/(ry*ry);
	if ( lambda>1 ) {
	   lambda = sqrt(lambda);
	   rx *= lambda;
	   ry *= lambda;
	}
	factor = rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p;
	if ( RealNear(factor,0))
	    factor = 0;		/* Avoid rounding errors that lead to small negative values */
	else
	    factor = sqrt(factor/(rx*rx*y1p*y1p+ry*ry*x1p*x1p));
	if ( large_arc==sweep )
	    factor = -factor;
	cxp = factor*(rx*y1p)/ry;
	cyp =-factor*(ry*x1p)/rx;
	cx = cosr*cxp - sinr*cyp + (current->x+x)/2;
	cy = sinr*cxp + cosr*cyp + (current->y+y)/2;

	tmpx = (x1p-cxp)/rx; tmpy = (y1p-cyp)/ry;
	startangle = acos(tmpx/sqrt(tmpx*tmpx+tmpy*tmpy));
	if ( tmpy<0 )
	    startangle = -startangle;
	t2x = (-x1p-cxp)/rx; t2y = (-y1p-cyp)/ry;
	delta = acos((tmpx*t2x+tmpy*t2y)/
		sqrt((tmpx*tmpx+tmpy*tmpy)*(t2x*t2x+t2y*t2y)));
	if ( tmpx*t2y-tmpy*t2x<0 )
	    delta = -delta;
	if ( sweep==0 && delta>0 )
	    delta -= 2*PI;
	if ( sweep && delta<0 )
	    delta += 2*PI;

	if ( delta>0 ) {
	    i = 0;
	    ia = firstia = floor(startangle/(PI/2))+1;
	    for ( a=ia*(PI/2), ia+=4; a<startangle+delta && !RealNear(a,startangle+delta); a += PI/2, ++i, ++ia ) {
		t2x = rx*cosines[ia]; t2y = ry*sines[ia];
		arcp[i].x = cosr*t2x - sinr*t2y + cx;
		arcp[i].y = sinr*t2x + cosr*t2y + cy;
		if ( t2x==0 ) {
		    t2x = rx*cosines[ia+1]; t2y = 0;
		} else {
		    t2x = 0; t2y = ry*sines[ia+1];
		}
		prevcp[i].x = arcp[i].x - .552*(cosr*t2x - sinr*t2y);
		prevcp[i].y = arcp[i].y - .552*(sinr*t2x + cosr*t2y);
		nextcp[i].x = arcp[i].x + .552*(cosr*t2x - sinr*t2y);
		nextcp[i].y = arcp[i].y + .552*(sinr*t2x + cosr*t2y);
	    }
	} else {
	    i = 0;
	    ia = firstia = ceil(startangle/(PI/2))-1;
	    for ( a=ia*(PI/2), ia += 8; a>startangle+delta && !RealNear(a,startangle+delta); a -= PI/2, ++i, --ia ) {
		t2x = rx*cosines[ia]; t2y = ry*sines[ia];
		arcp[i].x = cosr*t2x - sinr*t2y + cx;
		arcp[i].y = sinr*t2x + cosr*t2y + cy;
		if ( t2x==0 ) {
		    t2x = rx*cosines[ia+1]; t2y = 0;
		} else {
		    t2x = 0; t2y = ry*sines[ia+1];
		}
		prevcp[i].x = arcp[i].x + .552*(cosr*t2x - sinr*t2y);
		prevcp[i].y = arcp[i].y + .552*(sinr*t2x + cosr*t2y);
		nextcp[i].x = arcp[i].x - .552*(cosr*t2x - sinr*t2y);
		nextcp[i].y = arcp[i].y - .552*(sinr*t2x + cosr*t2y);
	    }
	}
	if ( i!=0 ) {
	    double firsta=firstia*PI/2;
	    double d = (firsta-startangle)/2;
	    double th = startangle+d;
	    double hypot = 1/cos(d);
	    BasePoint temp;
	    t2x = rx*cos(th)*hypot; t2y = ry*sin(th)*hypot;
	    temp.x = cosr*t2x - sinr*t2y + cx;
	    temp.y = sinr*t2x + cosr*t2y + cy;
	    firstcp[0].x = cur->last->me.x + .552*(temp.x-cur->last->me.x);
	    firstcp[0].y = cur->last->me.y + .552*(temp.y-cur->last->me.y);
	    firstcp[1].x = arcp[0].x + .552*(temp.x-arcp[0].x);
	    firstcp[1].y = arcp[0].y + .552*(temp.y-arcp[0].y);
	}
	for ( j=0; j<i; ++j ) {
	    sp = SplinePointCreate(arcp[j].x,arcp[j].y);
	    if ( j!=0 ) {
		sp->prevcp = prevcp[j];
		cur->last->nextcp = nextcp[j-1];
	    } else {
		sp->prevcp = firstcp[1];
		cur->last->nextcp = firstcp[0];
	    }
	    sp->noprevcp = cur->last->nonextcp = false;
	    SplineMake(cur->last,sp,false);
	    cur->last = sp;
	}
	{ double hypot, c, s;
	BasePoint temp;
	if ( i==0 ) {
	    double th = startangle+delta/2;
	    hypot = 1.0/cos(delta/2);
	    c = cos(th); s=sin(th);
	} else {
	    double lasta = delta<0 ? a+PI/2 : a-PI/2;
	    double d = (startangle+delta-lasta);
	    double th = lasta+d/2;
	    hypot = 1.0/cos(d/2);
	    c = cos(th); s=sin(th);
	}
	t2x = rx*c*hypot; t2y = ry*s*hypot;
	temp.x = cosr*t2x - sinr*t2y + cx;
	temp.y = sinr*t2x + cosr*t2y + cy;
	cur->last->nextcp.x = cur->last->me.x + .552*(temp.x-cur->last->me.x);
	cur->last->nextcp.y = cur->last->me.y + .552*(temp.y-cur->last->me.y);
	final->prevcp.x = final->me.x + .552*(temp.x-final->me.x);
	final->prevcp.y = final->me.y + .552*(temp.y-final->me.y);
	cur->last->nonextcp = final->noprevcp = false;
	}
    }
    *current = final->me;
    SplineMake(cur->last,final,false);
    cur->last = final;
}

static SplineSet *SVGParsePath(xmlChar *path) {
    BasePoint current;
    SplineSet *head=NULL, *last=NULL, *cur=NULL;
    SplinePoint *sp;
    int type = 'M';
    double x1,x2,x,y1,y2,y,rx,ry,axisrot;
    int large_arc,sweep;
    int order2 = 0;
    char *end;

    current.x = current.y = 0;

    while ( *path ) {
	while ( *path==' ' ) ++path;
	while ( isalpha(*path))
	    type = *path++;
	if ( *path=='\0' && type!='z' && type!='Z' )
    break;
	if ( type=='m' || type=='M' ) {
	    if ( cur!=NULL && cur->last!=cur->first ) {
		if ( RealNear(cur->last->me.x,cur->first->me.x) &&
			RealNear(cur->last->me.y,cur->first->me.y) ) {
		    cur->first->prevcp = cur->last->prevcp;
		    cur->first->noprevcp = cur->last->noprevcp;
		    cur->first->prev = cur->last->prev;
		    cur->first->prev->to = cur->first;
		    SplinePointFree(cur->last);
		} else
		    SplineMake(cur->last,cur->first,order2);
		cur->last = cur->first;
	    }
	    x = strtod((char *) path,&end);
	    end = skipcomma(end);
	    y = strtod(end,&end);
	    if ( type=='m' ) {
		x += current.x; y += current.y;
	    }
	    sp = SplinePointCreate(x,y);
	    current = sp->me;
	    cur = chunkalloc(sizeof(SplineSet));
	    if ( head==NULL )
		head = cur;
	    else
		last->next = cur;
	    last = cur;
	    cur->first = cur->last = sp;
	} else if ( type=='z' || type=='Z' ) {
	    if ( cur!=NULL && cur->last!=cur->first ) {
		if ( RealNear(cur->last->me.x,cur->first->me.x) &&
			RealNear(cur->last->me.y,cur->first->me.y) ) {
		    cur->first->prevcp = cur->last->prevcp;
		    cur->first->noprevcp = cur->last->noprevcp;
		    cur->first->prev = cur->last->prev;
		    cur->first->prev->to = cur->first;
		    SplinePointFree(cur->last);
		} else
		    SplineMake(cur->last,cur->first,order2);
		cur->last = cur->first;
		current = cur->first->me;
	    }
	    type = ' ';
	    end = (char *) path;
	} else {
	    if ( cur==NULL ) {
		sp = SplinePointCreate(current.x,current.y);
		cur = chunkalloc(sizeof(SplineSet));
		if ( head==NULL )
		    head = cur;
		else
		    last->next = cur;
		last = cur;
		cur->first = cur->last = sp;
	    }
	    switch ( type ) {
	      case 'l': case'L':
		x = strtod((char *) path,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='l' ) {
		    x += current.x; y += current.y;
		}
		sp = SplinePointCreate(x,y);
		current = sp->me;
		SplineMake(cur->last,sp,order2);
		cur->last = sp;
	      break;
	      case 'h': case'H':
		x = strtod((char *) path,&end);
		y = current.y;
		if ( type=='h' ) {
		    x += current.x;
		}
		sp = SplinePointCreate(x,y);
		current = sp->me;
		SplineMake(cur->last,sp,order2);
		cur->last = sp;
	      break;
	      case 'v': case 'V':
		x = current.x;
		y = strtod((char *) path,&end);
		if ( type=='v' ) {
		    y += current.y;
		}
		sp = SplinePointCreate(x,y);
		current = sp->me;
		SplineMake(cur->last,sp,order2);
		cur->last = sp;
	      break;
	      case 'c': case 'C':
		x1 = strtod((char *) path,&end);
		end = skipcomma(end);
		y1 = strtod(end,&end);
		end = skipcomma(end);
		x2 = strtod(end,&end);
		end = skipcomma(end);
		y2 = strtod(end,&end);
		end = skipcomma(end);
		x = strtod(end,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='c' ) {
		    x1 += current.x; y1 += current.y;
		    x2 += current.x; y2 += current.y;
		    x += current.x; y += current.y;
		}
		sp = SplinePointCreate(x,y);
		sp->prevcp.x = x2; sp->prevcp.y = y2; sp->noprevcp = false;
		cur->last->nextcp.x = x1; cur->last->nextcp.y = y1; cur->last->nonextcp = false;
		current = sp->me;
		SplineMake(cur->last,sp,false);
		cur->last = sp;
	      break;
	      case 's': case 'S':
		x1 = 2*cur->last->me.x - cur->last->prevcp.x;
		y1 = 2*cur->last->me.y - cur->last->prevcp.y;
		x2 = strtod((char *) path,&end);
		end = skipcomma(end);
		y2 = strtod(end,&end);
		end = skipcomma(end);
		x = strtod(end,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='s' ) {
		    x2 += current.x; y2 += current.y;
		    x += current.x; y += current.y;
		}
		sp = SplinePointCreate(x,y);
		sp->prevcp.x = x2; sp->prevcp.y = y2; sp->noprevcp = false;
		cur->last->nextcp.x = x1; cur->last->nextcp.y = y1; cur->last->nonextcp = false;
		current = sp->me;
		SplineMake(cur->last,sp,false);
		cur->last = sp;
	      break;
	      case 'Q': case 'q':
		x1 = strtod((char *) path,&end);
		end = skipcomma(end);
		y1 = strtod(end,&end);
		end = skipcomma(end);
		x = strtod(end,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='q' ) {
		    x1 += current.x; y1 += current.y;
		    x += current.x; y += current.y;
		}
		sp = SplinePointCreate(x,y);
		sp->prevcp.x = x1; sp->prevcp.y = y1; sp->noprevcp = false;
		cur->last->nextcp.x = x1; cur->last->nextcp.y = y1; cur->last->nonextcp = false;
		current = sp->me;
		SplineMake(cur->last,sp,true);
		cur->last = sp;
		order2 = true;
	      break;
	      case 'T': case 't':
		x = strtod((char *) path,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='t' ) {
		    x += current.x; y += current.y;
		}
		x1 = 2*cur->last->me.x - cur->last->prevcp.x;
		y1 = 2*cur->last->me.y - cur->last->prevcp.y;
		sp = SplinePointCreate(x,y);
		sp->prevcp.x = x1; sp->prevcp.y = y1; sp->noprevcp = false;
		cur->last->nextcp.x = x1; cur->last->nextcp.y = y1; cur->last->nonextcp = false;
		current = sp->me;
		SplineMake(cur->last,sp,true);
		cur->last = sp;
		order2 = true;
	      break;
	      case 'A': case 'a':
		rx = strtod((char *) path,&end);
		end = skipcomma(end);
		ry = strtod(end,&end);
		end = skipcomma(end);
		axisrot = strtod(end,&end)*3.1415926535897932/180;
		end = skipcomma(end);
		large_arc = strtol(end,&end,10);
		end = skipcomma(end);
		sweep = strtol(end,&end,10);
		end = skipcomma(end);
		x = strtod(end,&end);
		end = skipcomma(end);
		y = strtod(end,&end);
		if ( type=='a' ) {
		    x += current.x; y += current.y;
		}
		if ( x!=current.x || y!=current.y )
		    SVGTraceArc(cur,&current,x,y,rx,ry,axisrot,large_arc,sweep);
	      break;
	      default:
		fprintf(stderr,"Unknown type '%c' found in path specification\n", type );
	      break;
	    }
	}
	path = (xmlChar *) skipcomma(end);
    }
return( head );
}

static SplineSet *SVGParseRect(xmlNodePtr rect) {
	/* x,y,width,height,rx,ry */
    double x,y,width,height,rx,ry;
    char *num;
    SplinePoint *sp;
    SplineSet *cur;

    num = (char *) _xmlGetProp(rect,(xmlChar *) "x");
    if ( num!=NULL ) {
	x = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	x = 0;
    num = (char *) _xmlGetProp(rect,(xmlChar *) "width");
    if ( num!=NULL ) {
	width = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
return( NULL );
    num = (char *) _xmlGetProp(rect,(xmlChar *) "y");
    if ( num!=NULL ) {
	y = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	y = 0;
    num = (char *) _xmlGetProp(rect,(xmlChar *) "height");
    if ( num!=NULL ) {
	height = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
return( NULL );

    rx = ry = 0;
    num = (char *) _xmlGetProp(rect,(xmlChar *) "rx");
    if ( num!=NULL ) {
	ry = rx = strtod((char *) num,NULL);
	_xmlFree(num);
    }
    num = (char *) _xmlGetProp(rect,(xmlChar *) "ry");
    if ( num!=NULL ) {
	ry = strtod((char *) num,NULL);
	if ( rx==0 ) ry = rx;
	_xmlFree(num);
    }

    if ( 2*rx>width ) rx = width/2;
    if ( 2*ry>height ) ry = height/2;

    cur = chunkalloc(sizeof(SplineSet));
    if ( rx==0 ) {
	cur->first = SplinePointCreate(x,y+height);
	cur->last = SplinePointCreate(x+width,y+height);
	SplineMake(cur->first,cur->last,true);
	sp = SplinePointCreate(x+width,y);
	SplineMake(cur->last,sp,true);
	cur->last = sp;
	sp = SplinePointCreate(x,y);
	SplineMake(cur->last,sp,true);
	SplineMake(sp,cur->first,true);
	cur->last = cur->first;
return( cur );
    } else {
	cur->first = SplinePointCreate(x,y+height-ry);
	cur->last = SplinePointCreate(x+rx,y+height);
	cur->first->nextcp.x = x; cur->first->nextcp.y = y+height;
	cur->last->prevcp = cur->first->nextcp;
	cur->first->nonextcp = cur->last->noprevcp = false;
	SplineMake(cur->first,cur->last,false);
	if ( rx<2*width ) {
	    sp = SplinePointCreate(x+width-rx,y+height);
	    SplineMake(cur->last,sp,true);
	    cur->last = sp;
	}
	sp = SplinePointCreate(x+width,y+height-ry);
	sp->prevcp.x = x+width; sp->prevcp.y = y+height;
	cur->last->nextcp = sp->prevcp;
	cur->last->nonextcp = sp->noprevcp = false;
	SplineMake(cur->last,sp,false);
	cur->last = sp;
	if ( ry<2*height ) {
	    sp = SplinePointCreate(x+width,y+ry);
	    SplineMake(cur->last,sp,false);
	    cur->last = sp;
	}
	sp = SplinePointCreate(x+width-rx,y);
	sp->prevcp.x = x+width; sp->prevcp.y = y;
	cur->last->nextcp = sp->prevcp;
	cur->last->nonextcp = sp->noprevcp = false;
	SplineMake(cur->last,sp,false);
	cur->last = sp;
	if ( rx<2*width ) {
	    sp = SplinePointCreate(x+rx,y);
	    SplineMake(cur->last,sp,false);
	    cur->last = sp;
	}
	cur->last->nextcp.x = x; cur->last->nextcp.y = y;
	cur->last->nonextcp = false;
	if ( ry>=2*height ) {
	    cur->first->prevcp = cur->last->nextcp;
	    cur->first->noprevcp = false;
	} else {
	    sp = SplinePointCreate(x,y+ry);
	    sp->prevcp = cur->last->nextcp;
	    sp->noprevcp = false;
	    SplineMake(cur->last,sp,false);
	    cur->last = sp;
	}
	SplineMake(cur->last,cur->first,false);
	cur->first = cur->last;
return( cur );
    }
}

static SplineSet *SVGParseLine(xmlNodePtr line) {
	/* x1,y1, x2,y2 */
    double x,y, x2,y2;
    char *num;
    SplinePoint *sp1, *sp2;
    SplineSet *cur;

    num = (char *) _xmlGetProp(line,(xmlChar *) "x1");
    if ( num!=NULL ) {
	x = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	x = 0;
    num = (char *) _xmlGetProp(line,(xmlChar *) "x2");
    if ( num!=NULL ) {
	x2 = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	x2 = 0;
    num = (char *) _xmlGetProp(line,(xmlChar *) "y1");
    if ( num!=NULL ) {
	y = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	y = 0;
    num = (char *) _xmlGetProp(line,(xmlChar *) "y2");
    if ( num!=NULL ) {
	y2 = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	y2 = 0;

    sp1 = SplinePointCreate(x,y);
    sp2 = SplinePointCreate(x2,y2);
    SplineMake(sp1,sp2,false);
    cur = chunkalloc(sizeof(SplineSet));
    cur->first = sp1;
    cur->last = sp2;
return( cur );
}

static SplineSet *SVGParseEllipse(xmlNodePtr ellipse, int iscircle) {
	/* cx,cy,rx,ry */
	/* cx,cy,r */
    double cx,cy,rx,ry;
    char *num;
    SplinePoint *sp;
    SplineSet *cur;

    num = (char *) _xmlGetProp(ellipse,(xmlChar *) "cx");
    if ( num!=NULL ) {
	cx = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	cx = 0;
    num = (char *) _xmlGetProp(ellipse,(xmlChar *) "cy");
    if ( num!=NULL ) {
	cy = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	cy = 0;
    if ( iscircle ) {
	num = (char *) _xmlGetProp(ellipse,(xmlChar *) "r");
	if ( num!=NULL ) {
	    rx = ry = strtod((char *) num,NULL);
	    _xmlFree(num);
	} else
return( NULL );
    } else {
	num = (char *) _xmlGetProp(ellipse,(xmlChar *) "rx");
	if ( num!=NULL ) {
	    rx = strtod((char *) num,NULL);
	    _xmlFree(num);
	} else
return( NULL );
	num = (char *) _xmlGetProp(ellipse,(xmlChar *) "ry");
	if ( num!=NULL ) {
	    ry = strtod((char *) num,NULL);
	    _xmlFree(num);
	} else
return( NULL );
    }
    if ( rx<0 ) rx = -rx;
    if ( ry<0 ) ry = -ry;

    cur = chunkalloc(sizeof(SplineSet));
    cur->first = SplinePointCreate(cx-rx,cy);
    cur->last = SplinePointCreate(cx,cy+ry);
    cur->first->nextcp.x = cx-rx; cur->first->nextcp.y = cy+ry;
    cur->last->prevcp = cur->first->nextcp;
    cur->first->noprevcp = cur->first->nonextcp = false;
    cur->last->noprevcp = cur->last->nonextcp = false;
    SplineMake(cur->first,cur->last,true);
    sp = SplinePointCreate(cx+rx,cy);
    sp->prevcp.x = cx+rx; sp->prevcp.y = cy+ry;
    sp->nextcp.x = cx+rx; sp->nextcp.y = cy-ry;
    sp->nonextcp = sp->noprevcp = false;
    cur->last->nextcp = sp->prevcp;
    SplineMake(cur->last,sp,true);
    cur->last = sp;
    sp = SplinePointCreate(cx,cy-ry);
    sp->prevcp = cur->last->nextcp;
    sp->nextcp.x = cx-rx; sp->nextcp.y = cy-ry;
    sp->nonextcp = sp->noprevcp = false;
    cur->first->prevcp = sp->nextcp;
    SplineMake(cur->last,sp,true);
    SplineMake(sp,cur->first,true);
    cur->last = cur->first;
return( cur );
}

static SplineSet *SVGParsePoly(xmlNodePtr poly, int isgon) {
	/* points */
    double x,y;
    char *pts, *end;
    SplinePoint *sp;
    SplineSet *cur;

    pts = (char *) _xmlGetProp(poly,(xmlChar *) "points");
    if ( pts==NULL )
return( NULL );

    x = strtod(pts,&end);
    if ( *end!=',' ) {
	_xmlFree(pts);
return( NULL );
    }
    y = strtod(end+1,&end);
    while ( isspace(*end)) ++end;

    cur = chunkalloc(sizeof(SplineSet));
    cur->first = cur->last = SplinePointCreate(x,y);
    while ( *end ) {
	x = strtod(end,&end);
	if ( *end!=',' ) {
	    _xmlFree(pts);
	    SplinePointListFree(cur);
return( NULL );
	}
	y = strtod(end+1,&end);
	while ( isspace(*end)) ++end;
	sp = SplinePointCreate(x,y);
	SplineMake(cur->last,sp,false);
	cur->last = sp;
    }
    if ( isgon ) {
	if ( RealNear(cur->last->me.x,cur->first->me.x) &&
		RealNear(cur->last->me.y,cur->first->me.y) ) {
	    cur->first->prev = cur->last->prev;
	    cur->first->prev->to = cur->first;
	    SplinePointFree(cur->last);
	} else
	    SplineMake(cur->last,cur->first,false);
	cur->last = cur->first;
    }
return( cur );
}

struct svg_state {
    double linewidth;
    int dofill, dostroke;
    uint32 fillcol, strokecol;
    float fillopacity, strokeopacity;
    int isvisible;
    enum linecap lc;
    enum linejoin lj;
    real transform[6];
    DashType dashes[DASH_MAX];
};

static void SVGFigureTransform(struct svg_state *st,char *name) {
    real trans[6], res[6];
    double a, cx,cy;
    char *pt, *paren, *end;
	/* matrix(a,b,c,d,e,f)
	   rotate(theta[,cx,cy])
	   scale(sx[,sy])
	   translate(x,y)
	   skewX(theta)
	   skewY(theta)
	  */

    for ( pt = (char *)name; isspace(*pt); ++pt );
    while ( *pt ) {
	paren = strchr(pt,'(');
	if ( paren==NULL )
    break;
	if ( strncmp(pt,"matrix",paren-pt)==0 ) {
	    trans[0] = strtod(paren+1,&end);
	    trans[1] = strtod(skipcomma(end),&end);
	    trans[2] = strtod(skipcomma(end),&end);
	    trans[3] = strtod(skipcomma(end),&end);
	    trans[4] = strtod(skipcomma(end),&end);
	    trans[5] = strtod(skipcomma(end),&end);
	} else if ( strncmp(pt,"rotate",paren-pt)==0 ) {
	    trans[4] = trans[5] = 0;
	    a = strtod(paren+1,&end)*3.1415926535897932/180;
	    trans[0] = trans[3] = cos(a);
	    trans[1] = sin(a);
	    trans[2] = -trans[1];
	    while ( isspace(*end)) ++end;
	    if ( *end!=')' ) {
		cx = strtod(skipcomma(end),&end);
		cy = strtod(skipcomma(end),&end);
		res[0] = res[3] = 1;
		res[1] = res[2] = 0;
		res[4] = cx; res[5] = cy;
		MatMultiply(res,trans,res);
		trans[0] = trans[3] = 1;
		trans[1] = trans[2] = 0;
		trans[4] = -cx; trans[5] = -cy;
		MatMultiply(res,trans,res);
		memcpy(trans,res,sizeof(res));
	    }
	} else if ( strncmp(pt,"scale",paren-pt)==0 ) {
	    trans[1] = trans[2] = trans[4] = trans[5] = 0;
	    trans[0] = trans[3] = strtod(paren+1,&end);
	    while ( isspace(*end)) ++end;
	    if ( *end!=')' )
		trans[3] = strtod(skipcomma(end),&end);
	} else if ( strncmp(pt,"translate",paren-pt)==0 ) {
	    trans[0] = trans[3] = 1;
	    trans[1] = trans[2] = trans[5] = 0;
	    trans[4] = strtod(paren+1,&end);
	    while ( isspace(*end)) ++end;
	    if ( *end!=')' )
		trans[5] = strtod(skipcomma(end),&end);
	} else if ( strncmp(pt,"skewX",paren-pt)==0 ) {
	    trans[0] = trans[3] = 1;
	    trans[1] = trans[2] = trans[4] = trans[5] = 0;
	    trans[2] = tan(strtod(paren+1,&end)*3.1415926535897932/180);
	} else if ( strncmp(pt,"skewY",paren-pt)==0 ) {
	    trans[0] = trans[3] = 1;
	    trans[1] = trans[2] = trans[4] = trans[5] = 0;
	    trans[1] = tan(strtod(paren+1,&end)*3.1415926535897932/180);
	} else
    break;
	while ( isspace(*end)) ++end;
	if ( *end!=')')
    break;
	MatMultiply(trans,st->transform,st->transform);
	pt = end+1;
	while ( isspace(*pt)) ++pt;
    }
}

static void SVGuseTransform(struct svg_state *st,xmlNodePtr use, xmlNodePtr symbol) {
    double x,y,uwid,uheight,swid,sheight;
    char *num, *end;
    real trans[6];

    num = (char *) _xmlGetProp(use,(xmlChar *) "x");
    if ( num!=NULL ) {
	x = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	x = 0;
    num = (char *) _xmlGetProp(use,(xmlChar *) "y");
    if ( num!=NULL ) {
	y = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	y = 0;
    if ( x!=0 || y!=0 ) {
	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = 0;
	trans[4] = x; trans[5] = y;
	MatMultiply(trans,st->transform,st->transform);
    }
    num = (char *) _xmlGetProp(use,(xmlChar *) "width");
    if ( num!=NULL ) {
	uwid = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	uwid = 0;
    num = (char *) _xmlGetProp(use,(xmlChar *) "height");
    if ( num!=NULL ) {
	uheight = strtod((char *) num,NULL);
	_xmlFree(num);
    } else
	uheight = 0;
    num = (char *) _xmlGetProp(symbol,(xmlChar *) "viewBox");
    if ( num!=NULL ) {
	x = strtod((char *) num,&end);
	y = strtod((char *) end+1,&end);
	swid = strtod((char *) end+1,&end);
	sheight = strtod((char *) end+1,&end);
	_xmlFree(num);
    } else
return;
    if ( uwid != 0 || uheight != 0 ) {
	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = trans[4] = trans[5] = 0;
	if ( uwid != 0 && swid!=0 ) trans[0] = uwid/swid;
	if ( uheight != 0 && sheight!=0 ) trans[3] = uheight/sheight;
	MatMultiply(trans,st->transform,st->transform);
    }
}

static int xmlParseColor(xmlChar *name,uint32 *color) {
    int doit, i;
    static struct { char *name; uint32 col; } stdcols[] = {
	{ "red", 0xff0000 },
	{ "green", 0x008000 },
	{ "blue", 0x0000ff },
	{ "cyan", 0x00ffff },
	{ "magenta", 0xff00ff },
	{ "yellow", 0xffff00 },
	{ "black", 0x000000 },
	{ "darkgray", 0x404040 },
	{ "darkgrey", 0x404040 },
	{ "gray", 0x808080 },
	{ "grey", 0x808080 },
	{ "lightgray", 0xc0c0c0 },
	{ "lightgrey", 0xc0c0c0 },
	{ "white", 0xffffff },
	{ "maroon", 0x800000 },
	{ "olive", 0x808000 },
	{ "navy", 0x000080 },
	{ "purple", 0x800080 },
	{ "lime", 0x00ff00 },
	{ "aqua", 0x00ffff },
	{ "teal", 0x008080 },
	{ "fuchsia", 0xff0080 },
	{ "silver", 0xc0c0c0 },
	{ NULL }};

    doit = _xmlStrcmp(name,(xmlChar *) "none")!=0;
    if ( doit ) {
	for ( i=0; stdcols[i].name!=NULL; ++i )
	    if ( _xmlStrcmp(name,(xmlChar *) stdcols[i].name)==0 )
	break;
	if ( stdcols[i].name!=NULL )
	    *color = stdcols[i].col;
	else if ( _xmlStrcmp(name,(xmlChar *) "currentColor")==0 )
	    *color = COLOR_INHERITED;
	else if ( name[0]=='#' ) {
	    unsigned int temp;
	    sscanf( (char *) name, "#%x", &temp );
	    if ( strlen( (char *) name)==4 ) {
		*color = (((temp&0xf00)*0x11)<<8) |
			 (((temp&0x0f0)*0x11)<<4) |
			 (((temp&0x00f)*0x11)   );
	    } else if ( strlen( (char *) name)==7 ) {
		*color = temp;
	    } else
		*color = COLOR_INHERITED;
	} else if ( strncmp( (char *) name, "rgb(",4)==0 ) {
	    float r,g,b;
	    sscanf((char *)name + 4, "%g,%g,%g", &r, &g, &b );
	    if ( strchr((char *) name,'.')!=NULL ) {
		if ( r>=1 ) r = 1; else if ( r<0 ) r=0;
		if ( g>=1 ) g = 1; else if ( g<0 ) g=0;
		if ( b>=1 ) b = 1; else if ( b<0 ) b=0;
		*color = ( ((int) rint(r*255))<<16 ) |
			 ( ((int) rint(g*255))<<8  ) |
			 ( ((int) rint(b*255))     );
	    } else {
		if ( r>=255 ) r = 255; else if ( r<0 ) r=0;
		if ( g>=255 ) g = 255; else if ( g<0 ) g=0;
		if ( b>=255 ) b = 255; else if ( b<0 ) b=0;
		*color = ( ((int) r)<<16 ) |
			 ( ((int) g)<<8  ) |
			 ( ((int) b)     );
	    }
	} else {
	    fprintf( stderr, "Failed to parse color %s\n", (char *) name );
	    *color = COLOR_INHERITED;
	}
    }
return( doit );
}

static Entity *EntityCreate(SplinePointList *head,struct svg_state *state) {
    Entity *ent = gcalloc(1,sizeof(Entity));
    ent->type = et_splines;
    ent->u.splines.splines = head;
    ent->u.splines.cap = state->lc;
    ent->u.splines.join = state->lj;
    ent->u.splines.stroke_width = state->linewidth;
    ent->u.splines.fill.col = state->dofill ? state->fillcol : state->dostroke ? 0xffffffff : COLOR_INHERITED;
    ent->u.splines.stroke.col = state->dostroke ? state->strokecol : 0xffffffff;
    ent->u.splines.fill.opacity = state->fillopacity;
    ent->u.splines.stroke.opacity = state->strokeopacity;
    memcpy(ent->u.splines.transform,state->transform,6*sizeof(real));
return( ent );
}

static Entity *_SVGParseSVG(xmlNodePtr svg, xmlNodePtr top,
	struct svg_state *inherit) {
    struct svg_state st;
    xmlChar *name;
    xmlNodePtr kid;
    Entity *ehead, *elast, *eret;
    SplineSet *head;
    int treat_symbol_as_g = false;

    if ( svg==NULL )
return( NULL );

    st = *inherit;
  tail_recurse:
    name = _xmlGetProp(svg,(xmlChar *) "display");
    if ( name!=NULL ) {
	int hide = _xmlStrcmp(name,(xmlChar *) "none")==0;
	_xmlFree(name);
	if ( hide )
return( NULL );
    }
    name = _xmlGetProp(svg,(xmlChar *) "visibility");
    if ( name!=NULL ) {
	st.isvisible = _xmlStrcmp(name,(xmlChar *) "hidden")!=0 &&
		_xmlStrcmp(name,(xmlChar *) "colapse")!=0;
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "fill");
    if ( name!=NULL ) {
	st.dofill = xmlParseColor(name,&st.fillcol);
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "fill-opacity");
    if ( name!=NULL ) {
	st.fillopacity = strtod((char *)name,NULL);
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke");
    if ( name!=NULL ) {
	st.dostroke = xmlParseColor(name,&st.strokecol);
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke-opacity");
    if ( name!=NULL ) {
	st.strokeopacity = strtod((char *)name,NULL);
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke-width");
    if ( name!=NULL ) {
	st.linewidth = strtod((char *)name,NULL);
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke-linecap");
    if ( name!=NULL ) {
	st.lc = _xmlStrcmp(name,(xmlChar *) "butt") ? lc_butt :
		     _xmlStrcmp(name,(xmlChar *) "round") ? lc_round :
		     lc_square;
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke-linejoin");
    if ( name!=NULL ) {
	st.lj = _xmlStrcmp(name,(xmlChar *) "miter") ? lj_miter :
		     _xmlStrcmp(name,(xmlChar *) "round") ? lj_round :
		     lj_bevel;
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "stroke-dasharray");
    if ( name!=NULL ) {
	if ( _xmlStrcmp(name,(xmlChar *) "inherit") ) {
	    st.dashes[0] = 0; st.dashes[1] = DASH_INHERITED;
	} else if ( _xmlStrcmp(name,(xmlChar *) "none") ) {
	    st.dashes[0] = 0; st.dashes[1] = 0;
	} else {
	    int i;
	    xmlChar *pt, *end;
	    pt = name;
	    for ( i=0; i<DASH_MAX && *pt!='\0'; ++i ) {
		st.dashes[i] = strtol((char *) pt,(char **) &end,10);
		pt = end;
	    }
	    if ( i<DASH_MAX ) st.dashes[i] = 0;
	}
	_xmlFree(name);
    }
    name = _xmlGetProp(svg,(xmlChar *) "transform");
    if ( name!=NULL ) {
	SVGFigureTransform(&st,(char *) name);
	_xmlFree(name);
    }

    if ( (treat_symbol_as_g && _xmlStrcmp(svg->name,(xmlChar *) "symbol")==0) ||
	    _xmlStrcmp(svg->name,(xmlChar *) "svg")==0 ||
	    _xmlStrcmp(svg->name,(xmlChar *) "g")==0 ) {
	ehead = elast = NULL;
	for ( kid = svg->children; kid!=NULL; kid=kid->next ) {
	    eret = _SVGParseSVG(kid,top,&st);
	    if ( eret!=NULL ) {
		if ( elast==NULL )
		    ehead = eret;
		else
		    elast->next = eret;
		while ( eret->next!=NULL ) eret = eret->next;
		elast = eret;
	    }
	}
return( ehead );
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "use")==0 ) {
	name = _xmlGetProp(svg,(xmlChar *) "href");
	kid = NULL;
	if ( name!=NULL && *name=='#' ) {	/* Within this file */
	    kid = XmlFindID(top,(char *) name+1);
	    treat_symbol_as_g = true;
	}
	SVGuseTransform(&st,svg,kid);
	svg = kid;
	if ( name!=NULL )
	    _xmlFree(name);
	if ( svg!=NULL )
  goto tail_recurse;
return( NULL );
    }

    if ( !st.isvisible )
return( NULL );

    /* basic shapes */
    head = NULL;
    if ( _xmlStrcmp(svg->name,(xmlChar *) "path")==0 ) {
	name = _xmlGetProp(svg,(xmlChar *) "d");
	if ( name!=NULL ) {
	    head = SVGParsePath(name);
	    _xmlFree(name);
	}
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "rect")==0 ) {
	head = SVGParseRect(svg);		/* x,y,width,height,rx,ry */
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "circle")==0 ) {
	head = SVGParseEllipse(svg,true);	/* cx,cy, r */
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "ellipse")==0 ) {
	head = SVGParseEllipse(svg,false);	/* cx,cy, rx,ry */
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "line")==0 ) {
	head = SVGParseLine(svg);		/* x1,y1, x2,y2 */
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "polyline")==0 ) {
	head = SVGParsePoly(svg,0);		/* points */
    } else if ( _xmlStrcmp(svg->name,(xmlChar *) "polygon")==0 ) {
	head = SVGParsePoly(svg,1);		/* points */
    } else
return( NULL );
    if ( head==NULL )
return( NULL );

    SPLCatagorizePoints(head);

return( EntityCreate(SplinePointListTransform(head,st.transform,true),
	    &st));
}

static Entity *SVGParseSVG(xmlNodePtr svg,int em_size,int ascent) {
    struct svg_state st;
    char *num, *end;
    double x,y,swidth,sheight,width=1,height=1;

    memset(&st,0,sizeof(st));
    st.lc = lc_inherited;
    st.lj = lj_inherited;
    st.linewidth = WIDTH_INHERITED;
    st.fillcol = COLOR_INHERITED;
    st.strokecol = COLOR_INHERITED;
    st.isvisible = true;
    st.transform[0] = 1;
    st.transform[3] = -1;	/* The SVG coord system has y increasing down */
    				/*  Font coords have y increasing up */
    st.transform[5] = ascent;
    st.strokeopacity = st.fillopacity = 1.0;

    num = (char *) _xmlGetProp(svg,(xmlChar *) "width");
    if ( num!=NULL ) {
	width = strtod(num,NULL);
	_xmlFree(num);
    }
    num = (char *) _xmlGetProp(svg,(xmlChar *) "height");
    if ( num!=NULL ) {
	height = strtod(num,NULL);
	_xmlFree(num);
    }
    if ( height<=0 ) height = 1;
    if ( width<=0 ) width = 1;
    num = (char *) _xmlGetProp(svg,(xmlChar *) "viewBox");
    if ( num!=NULL ) {
	x = strtod((char *) num,&end);
	y = strtod((char *) end+1,&end);
	swidth = strtod((char *) end+1,&end);
	sheight = strtod((char *) end+1,&end);
	_xmlFree(num);
	if ( width>height ) {
	    if ( swidth!=0 ) st.transform[0] *= em_size/swidth;
	    if ( sheight!=0 ) st.transform[3] *= em_size/(sheight*width/height);
	} else {
	    if ( swidth!=0 ) st.transform[0] *= em_size/(swidth*height/width);
	    if ( sheight!=0 ) st.transform[3] *= em_size/sheight;
	}
    }
return( _SVGParseSVG(svg,svg,&st));
}

static void SVGParseGlyphBody(SplineChar *sc, xmlNodePtr glyph,int *flags) {
    xmlChar *path;

    path = _xmlGetProp(glyph,(xmlChar *) "d");
    if ( path!=NULL ) {
	sc->layers[ly_fore].splines = SVGParsePath(path);
	_xmlFree(path);
    } else {
	Entity *ent = SVGParseSVG(glyph,sc->parent->ascent+sc->parent->descent,
		sc->parent->ascent);
#ifdef FONTFORGE_CONFIG_TYPE3
	sc->layer_cnt = 1;
	SCAppendEntityLayers(sc,ent);
	if ( sc->layer_cnt==1 ) ++sc->layer_cnt;
	sc->parent->multilayer = true;
#else
	sc->layers[ly_fore].splines = SplinesFromEntities(ent,flags);
#endif
    }
}

static SplineChar *SVGParseGlyphArgs(xmlNodePtr glyph,int defh, int defv) {
    SplineChar *sc = SplineCharCreate();
    xmlChar *name, *form, *glyphname, *unicode, *orientation;
    int32 *u;
    char buffer[100];

    name = _xmlGetProp(glyph,(xmlChar *) "horiz-adv-x");
    if ( name!=NULL ) {
	sc->width = strtod((char *) name,NULL);
	_xmlFree(name);
    } else
	sc->width = defh;
    name = _xmlGetProp(glyph,(xmlChar *) "vert-adv-y");
    if ( name!=NULL ) {
	sc->vwidth = strtod((char *) name,NULL);
	_xmlFree(name);
    } else
	sc->vwidth = defv;
    name = _xmlGetProp(glyph,(xmlChar *) "vert-adv-y");
    if ( name!=NULL ) {
	sc->vwidth = strtod((char *) name,NULL);
	_xmlFree(name);
    } else
	sc->vwidth = defv;

    form = _xmlGetProp(glyph,(xmlChar *) "arabic-form");
    unicode = _xmlGetProp(glyph,(xmlChar *) "unicode");
    glyphname = _xmlGetProp(glyph,(xmlChar *) "glyph-name");
    orientation = _xmlGetProp(glyph,(xmlChar *) "orientation");
    if ( unicode!=NULL ) {
	u = utf82u32_copy((char *) unicode);
	_xmlFree(unicode);
	if ( u[1]=='\0' ) {
	    sc->unicodeenc = u[0];
	    if ( form!=NULL && u[0]>=0x600 && u[0]<=0x6ff ) {
		if ( _xmlStrcmp(form,(xmlChar *) "initial")==0 )
		    sc->unicodeenc = ArabicForms[u[0]-0x600].initial;
		else if ( _xmlStrcmp(form,(xmlChar *) "medial")==0 )
		    sc->unicodeenc = ArabicForms[u[0]-0x600].medial;
		else if ( _xmlStrcmp(form,(xmlChar *) "final")==0 )
		    sc->unicodeenc = ArabicForms[u[0]-0x600].final;
		else if ( _xmlStrcmp(form,(xmlChar *) "isolated")==0 )
		    sc->unicodeenc = ArabicForms[u[0]-0x600].isolated;
	    }
	}
	free(u);
    }
    if ( glyphname!=NULL ) {
	if ( sc->unicodeenc==-1 )
	    sc->unicodeenc = UniFromName((char *) glyphname,ui_none,&custom);
	sc->name = copy((char *) glyphname);
	_xmlFree(glyphname);
    } else if ( orientation!=NULL && *orientation=='v' && sc->unicodeenc!=-1 ) {
	if ( sc->unicodeenc<0x10000 )
	    sprintf( buffer, "uni%04X.vert", sc->unicodeenc );
	else
	    sprintf( buffer, "u%04X.vert", sc->unicodeenc );
	sc->name = copy( buffer );
    }
    /* we finish off defaulting the glyph name in the parseglyph routine */
    if ( form!=NULL )
	_xmlFree(form);
    if ( orientation!=NULL )
	_xmlFree(orientation);
return( sc );
}

static SplineChar *SVGParseMissing(SplineFont *sf,xmlNodePtr notdef,int defh, int defv, int enc, int *flags) {
    SplineChar *sc = SVGParseGlyphArgs(notdef,defh,defv);
    sc->parent = sf; sc->enc = enc;
    sc->name = copy(".notdef");
    sc->unicodeenc = 0;
    SVGParseGlyphBody(sc,notdef,flags);
return( sc );
}

static SplineChar *SVGParseGlyph(SplineFont *sf,xmlNodePtr glyph,int defh, int defv, int enc, int *flags) {
    char buffer[40];
    SplineChar *sc = SVGParseGlyphArgs(glyph,defh,defv);
    sc->parent = sf; sc->enc = enc;
    if ( sc->name==NULL ) {
	if ( sc->unicodeenc==-1 )
	    sprintf( buffer, "glyph%d", enc);
	else if ( sc->unicodeenc>=0x10000 )
	    sprintf( buffer, "u%04X", sc->unicodeenc );
	else if ( psunicodenames[sc->unicodeenc]!=NULL )
	    strcpy(buffer,psunicodenames[sc->unicodeenc]);
	else
	    sprintf( buffer, "uni%04X", sc->unicodeenc );
	sc->name = copy(buffer);
    }
    SVGParseGlyphBody(sc,glyph,flags);
return( sc );
}

static PST *AddLig(PST *last,uint32 tag,char *components,SplineChar *first) {
    PST *lig = chunkalloc(sizeof(PST));
    lig->tag = tag;
    lig->flags = PSTDefaultFlags(pst_ligature,first);
    lig->type = pst_ligature;
    lig->script_lang_index = SFAddScriptLangIndex(first->parent,
			SCScriptFromUnicode(first),DEFAULT_LANG);
    lig->next = last;
    lig->u.lig.components = copy(components);
return( lig );
}

static void SVGLigatureFixupCheck(SplineChar *sc,xmlNodePtr glyph) {
    xmlChar *unicode;
    int32 *u;
    int len, len2;
    SplineChar **chars, *any = NULL;
    char *comp, *pt;

    unicode = _xmlGetProp(glyph,(xmlChar *) "unicode");
    if ( unicode!=NULL ) {
	u = utf82u32_copy((char *) unicode);
	_xmlFree(unicode);
	if ( u[1]!='\0' ) {
	    for ( len=0; u[len]!=0; ++len );
	    chars = galloc(len*sizeof(SplineChar *));
	    for ( len=len2=0; u[len]!=0; ++len ) {
		chars[len] = SFGetChar(sc->parent,u[len],NULL);
		if ( chars[len]==NULL )
		    len2 += 9;
		else {
		    len2 += strlen(chars[len]->name);
		    if ( any==NULL ) any = chars[len];
		}
	    }
	    if ( any==NULL ) any=sc;
	    comp = pt = galloc(len2+1);
	    *pt = '\0';
	    for ( len=0; u[len]!=0; ++len ) {
		if ( chars[len]!=NULL )
		    strcpy(pt,chars[len]->name);
		else if ( u[len]<0x10000 )
		    sprintf(pt,"uni%04X", u[len]);
		else
		    sprintf(pt,"u%04X", u[len]);
		pt += strlen(pt);
		if ( u[len+1]!='\0' )
		    *pt++ = ' ';
	    }
	    sc->possub = AddLig(sc->possub,CHR('l','i','g','a'),comp,any);
		/* Understand the unicode latin ligatures. There are too many */
		/*  arabic ones */
	    if ( u[0]=='f' ) {
		if ( u[1]=='f' && u[2]==0 )
		    sc->unicodeenc = 0xfb00;
		else if ( u[1]=='i' && u[2]==0 )
		    sc->unicodeenc = 0xfb01;
		else if ( u[1]=='l' && u[2]==0 )
		    sc->unicodeenc = 0xfb02;
		else if ( u[1]=='f' && u[2]=='i' && u[3]==0 )
		    sc->unicodeenc = 0xfb03;
		else if ( u[1]=='f' && u[2]=='l' && u[3]==0 )
		    sc->unicodeenc = 0xfb04;
		else if ( u[1]=='t' && u[2]==0 )
		    sc->unicodeenc = 0xfb05;
	    } else if ( u[0]=='s' && u[1]=='t' && u[2]==0 )
		sc->unicodeenc = 0xfb06;
	    if ( strncmp(sc->name,"glyph",5)==0 && isdigit(sc->name[5])) {
		/* It's a default name, we can do better */
		free(sc->name);
		sc->name = copy(comp);
		for ( pt = sc->name; *pt; ++pt )
		    if ( *pt==' ' ) *pt = '_';
	    }
	}
    }
}

static char *SVGGetNames(SplineFont *sf,xmlChar *g,xmlChar *utf8,SplineChar **sc) {
    int32 *u=NULL;
    char *names;
    int len, i, ch;
    SplineChar *temp;
    char *pt, *gpt;

    *sc = NULL;
    len = 0;
    if ( utf8!=NULL ) {
	u = utf82u32_copy((char *) utf8);
	for ( i=0; u[i]!=0; ++i ) {
	    temp = SFGetChar(sf,u[i],NULL);
	    if ( temp!=NULL ) {
		if ( *sc==NULL ) *sc = temp;
		len = strlen(temp->name)+1;
	    }
	}
    }
    names = pt = galloc(len+(g!=NULL?strlen((char *)g):0)+1);
    if ( utf8!=NULL ) {
	for ( i=0; u[i]!=0; ++i ) {
	    temp = SFGetChar(sf,u[i],NULL);
	    if ( temp!=NULL ) {
		strcpy(pt,temp->name);
		pt += strlen( pt );
		*pt++ = ' ';
	    }
	}
	free(u);
    }
    if ( g!=NULL ) {
	for ( gpt=(char *) g; *gpt; ) {
	    if ( *gpt==',' || isspace(*gpt)) {
		while ( *gpt==',' || isspace(*gpt)) ++gpt;
		*pt++ = ' ';
	    } else {
		*pt++ = *gpt++;
	    }
	}
	if ( *sc==NULL ) {
	    for ( gpt = (char *) g; *gpt!='\0' && *gpt!=',' && !isspace(*gpt); ++gpt );
	    ch = *gpt; *gpt = '\0';
	    *sc = SFGetChar(sf,-1,(char *) g);
	    *gpt = ch;
	}
    }
    if ( pt>names && pt[-1]==' ' ) --pt;
    *pt = '\0';
return( names );
}

static void SVGParseKern(SplineFont *sf,xmlNodePtr kern,int isv) {
    xmlChar *k, *g1, *u1, *g2, *u2;
    double off;
    char *c1, *c2;
    SplineChar *sc1, *sc2;

    k = _xmlGetProp(kern,(xmlChar *) "k");
    if ( k==NULL )
return;
    off = -strtod((char *)k, NULL);
    _xmlFree(k);
    if ( off==0 )
return;

    g1 = _xmlGetProp(kern,(xmlChar *) "g1");
    u1 = _xmlGetProp(kern,(xmlChar *) "u1");
    if ( g1==NULL && u1==NULL )
return;
    c1 = SVGGetNames(sf,g1,u1,&sc1);
    if ( g1!=NULL ) _xmlFree(g1);
    if ( u1!=NULL ) _xmlFree(u1);

    g2 = _xmlGetProp(kern,(xmlChar *) "g2");
    u2 = _xmlGetProp(kern,(xmlChar *) "u2");
    if ( g2==NULL && u2==NULL ) {
	free(c1);
return;
    }
    c2 = SVGGetNames(sf,g2,u2,&sc2);
    if ( g2!=NULL ) _xmlFree(g2);
    if ( u2!=NULL ) _xmlFree(u2);

    if ( strchr(c1,' ')==NULL && strchr(c2,' ')==NULL ) {
	KernPair *kp = chunkalloc(sizeof(KernPair));
	kp->sc = sc2;
	kp->off = off;
	if ( isv ) {
	    kp->next = sc1->vkerns;
	    sc1->vkerns = kp;
	} else {
	    kp->next = sc1->kerns;
	    sc1->kerns = kp;
	}
	free(c1); free(c2);
    } else {
	KernClass *kc = chunkalloc(sizeof(KernClass));
	if ( isv ) {
	    kc->next = sf->vkerns;
	    sf->vkerns = kc;
	} else {
	    kc->next = sf->kerns;
	    sf->kerns = kc;
	}
	kc->first_cnt = kc->second_cnt = 2;
	kc->firsts = gcalloc(2,sizeof(char *));
	kc->firsts[1] = c1;
	kc->seconds = gcalloc(2,sizeof(char *));
	kc->seconds[1] = c2;
	kc->offsets = gcalloc(4,sizeof(int16));
	kc->offsets[3] = off;
	kc->flags = ((sc1!=NULL && SCRightToLeft(sc1)) ||
			(sc1==NULL && sc2!=NULL && SCRightToLeft(sc2)))? pst_r2l : 0;
	if ( sc1!=NULL || sc2!=NULL )
	    kc->sli = SCDefaultSLI(sf,sc1!=NULL ? sc1 : sc2 );
    }
}

static SplineFont *SVGParseFont(xmlNodePtr font) {
    int cnt, flags = -1;
    xmlNodePtr kids;
    int defh=0, defv=0;
    xmlChar *name;
    SplineFont *sf;

    sf = SplineFontEmpty();
    name = _xmlGetProp(font,(xmlChar *) "horiz-adv-x");
    if ( name!=NULL ) {
	defh = strtod((char *) name,NULL);
	_xmlFree(name);
    }
    name = _xmlGetProp(font,(xmlChar *) "vert-adv-y");
    if ( name!=NULL ) {
	defv = strtod((char *) name,NULL);
	_xmlFree(name);
	sf->hasvmetrics = true;
    }
    name = _xmlGetProp(font,(xmlChar *) "id");
    if ( name!=NULL ) {
	sf->fontname = copy( (char *) name);
	_xmlFree(name);
    }

    cnt = 0;
    for ( kids = font->children; kids!=NULL; kids=kids->next ) {
	int ascent=0, descent=0;
	if ( _xmlStrcmp(kids->name,(const xmlChar *) "font-face")==0 ) {
	    name = _xmlGetProp(kids,(xmlChar *) "units-per-em");
	    if ( name!=NULL ) {
		int val = rint(strtod((char *) name,NULL));
		_xmlFree(name);
		if ( val<0 ) val = 0;
		sf->ascent = val*800/1000;
		sf->descent = val - sf->ascent;
		if ( defv==0 ) defv = val;
		if ( defh==0 ) defh = val;
		SFDefaultOS2Simple(&sf->pfminfo,sf);
	    } else {
		fprintf( stderr, "This font does not specify units-per-em\n" );
		SplineFontFree(sf);
return( NULL );
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "font-family");
	    if ( name!=NULL ) {
		if ( strchr((char *) name,',')!=NULL )
		    *strchr((char *) name,',') ='\0';
		sf->familyname = copy( (char *) name);
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "font-weight");
	    if ( name!=NULL ) {
		if ( strnmatch((char *) name,"normal",6)==0 ) {
		    sf->pfminfo.weight = 400;
		    sf->weight = copy("Regular");
		    sf->pfminfo.panose[2] = 5;
		} else if ( strnmatch((char *) name,"bold",4)==0 ) {
		    sf->pfminfo.weight = 700;
		    sf->weight = copy("Bold");
		    sf->pfminfo.panose[2] = 8;
		} else {
		    sf->pfminfo.weight = strtod((char *) name,NULL);
		    if ( sf->pfminfo.weight <= 100 ) {
			sf->weight = copy("Thin");
			sf->pfminfo.panose[2] = 2;
		    } else if ( sf->pfminfo.weight <= 200 ) {
			sf->weight = copy("Extra-Light");
			sf->pfminfo.panose[2] = 3;
		    } else if ( sf->pfminfo.weight <= 300 ) {
			sf->weight = copy("Light");
			sf->pfminfo.panose[2] = 4;
		    } else if ( sf->pfminfo.weight <= 400 ) {
			sf->weight = copy("Regular");
			sf->pfminfo.panose[2] = 5;
		    } else if ( sf->pfminfo.weight <= 500 ) {
			sf->weight = copy("Medium");
			sf->pfminfo.panose[2] = 6;
		    } else if ( sf->pfminfo.weight <= 600 ) {
			sf->weight = copy("DemiBold");
			sf->pfminfo.panose[2] = 7;
		    } else if ( sf->pfminfo.weight <= 700 ) {
			sf->weight = copy("Bold");
			sf->pfminfo.panose[2] = 8;
		    } else if ( sf->pfminfo.weight <= 800 ) {
			sf->weight = copy("Heavy");
			sf->pfminfo.panose[2] = 9;
		    } else {
			sf->weight = copy("Black");
			sf->pfminfo.panose[2] = 10;
		    }
		}
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "font-stretch");
	    if ( name!=NULL ) {
		if ( strnmatch((char *) name,"normal",6)==0 ) {
		    sf->pfminfo.panose[3] = 3;
		    sf->pfminfo.width = 5;
		} else if ( strmatch((char *) name,"ultra-condensed")==0 ) {
		    sf->pfminfo.panose[3] = 8;
		    sf->pfminfo.width = 1;
		} else if ( strmatch((char *) name,"extra-condensed")==0 ) {
		    sf->pfminfo.panose[3] = 8;
		    sf->pfminfo.width = 2;
		} else if ( strmatch((char *) name,"condensed")==0 ) {
		    sf->pfminfo.panose[3] = 6;
		    sf->pfminfo.width = 3;
		} else if ( strmatch((char *) name,"semi-condensed")==0 ) {
		    sf->pfminfo.panose[3] = 6;
		    sf->pfminfo.width = 4;
		} else if ( strmatch((char *) name,"ultra-expanded")==0 ) {
		    sf->pfminfo.panose[3] = 7;
		    sf->pfminfo.width = 9;
		} else if ( strmatch((char *) name,"extra-expanded")==0 ) {
		    sf->pfminfo.panose[3] = 7;
		    sf->pfminfo.width = 8;
		} else if ( strmatch((char *) name,"expanded")==0 ) {
		    sf->pfminfo.panose[3] = 5;
		    sf->pfminfo.width = 7;
		} else if ( strmatch((char *) name,"semi-expanded")==0 ) {
		    sf->pfminfo.panose[3] = 5;
		    sf->pfminfo.width = 6;
		}
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "panose-1");
	    if ( name!=NULL ) {
		char *pt, *end;
		int i;
		for ( i=0, pt=(char *) name; i<10 && *pt; pt = end, ++i ) {
		    sf->pfminfo.panose[i] = strtol(pt,&end,10);
		}
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "slope");
	    if ( name!=NULL ) {
		sf->italicangle = strtod((char *) name,NULL);
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "underline-position");
	    if ( name!=NULL ) {
		sf->upos = strtod((char *) name,NULL);
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "underline-thickness");
	    if ( name!=NULL ) {
		sf->uwidth = strtod((char *) name,NULL);
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "ascent");
	    if ( name!=NULL ) {
		ascent = strtod((char *) name,NULL);
		_xmlFree(name);
	    }
	    name = _xmlGetProp(kids,(xmlChar *) "descent");
	    if ( name!=NULL ) {
		descent = strtod((char *) name,NULL);
		_xmlFree(name);
	    }
	    if ( ascent-descent==sf->ascent+sf->descent ) {
		sf->ascent = ascent;
		sf->descent = -descent;
	    }
	    sf->pfminfo.pfmset = true;
	} else if ( _xmlStrcmp(kids->name,(const xmlChar *) "glyph")==0 ||
		_xmlStrcmp(kids->name,(const xmlChar *) "missing-glyph")==0 )
	    ++cnt;
    }
    if ( sf->descent==0 ) {
	fprintf( stderr, "This font does not specify font-face\n" );
	SplineFontFree(sf);
return( NULL );
    }
    if ( sf->weight==NULL )
	sf->weight = copy("Regular");
    if ( sf->fontname==NULL && sf->familyname==NULL )
	sf->fontname = GetNextUntitledName();
    if ( sf->familyname==NULL )
	sf->familyname = copy(sf->fontname);
    if ( sf->fontname==NULL )
	sf->fontname = EnforcePostScriptName(sf->familyname);
    sf->fullname = copy(sf->fontname);

    /* Give ourselves an xuid, just in case they want to convert to PostScript*/
    if ( xuid!=NULL ) {
	sf->xuid = galloc(strlen(xuid)+20);
	sprintf(sf->xuid,"[%s %d]", xuid, (rand()&0xffffff));
    }

#if defined(FONTFORGE_CONFIG_GDRAW)
    GProgressChangeTotal(cnt);
#elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_change_total(cnt);
#endif
    sf->charcnt = cnt;
    sf->chars = galloc(cnt*sizeof(SplineChar *));
    sf->encoding_name = FindOrMakeEncoding("Original");

    cnt = 0;
    for ( kids = font->children; kids!=NULL; kids=kids->next ) {
	if ( _xmlStrcmp(kids->name,(const xmlChar *) "missing-glyph")==0 ) {
	    sf->chars[cnt] = SVGParseMissing(sf,kids,defh,defv,cnt,&flags);
	    cnt++;
#if defined(FONTFORGE_CONFIG_GDRAW)
	    GProgressNext();
#elif defined(FONTFORGE_CONFIG_GTK)
	    gwwv_progress_next();
#endif
	} else if ( _xmlStrcmp(kids->name,(const xmlChar *) "glyph")==0 ) {
	    sf->chars[cnt] = SVGParseGlyph(sf,kids,defh,defv,cnt,&flags);
	    cnt++;
#if defined(FONTFORGE_CONFIG_GDRAW)
	    GProgressNext();
#elif defined(FONTFORGE_CONFIG_GTK)
	    gwwv_progress_next();
#endif
	}
    }
    cnt = 0;
    for ( kids = font->children; kids!=NULL; kids=kids->next ) {
	if ( _xmlStrcmp(kids->name,(const xmlChar *) "hkern")==0 ) {
	    SVGParseKern(sf,kids,false);
	} else if ( _xmlStrcmp(kids->name,(const xmlChar *) "vkern")==0 ) {
	    SVGParseKern(sf,kids,true);
	} else if ( _xmlStrcmp(kids->name,(const xmlChar *) "glyph")==0 ) {
	    SVGLigatureFixupCheck(sf->chars[cnt++],kids);
	} else if ( _xmlStrcmp(kids->name,(const xmlChar *) "missing-glyph")==0 ) {
	    ++cnt;
	}
    }
    
return( sf );
}

static int SPLFindOrder(SplineSet *ss) {
    Spline *s, *first;

    while ( ss!=NULL ) {
	first = NULL;
	for ( s = ss->first->next; s!=NULL && s!=first ; s = s->to->next ) {
	    if ( first==NULL ) first = s;
	    if ( !s->knownlinear )
return( s->order2 );
	}
	ss = ss->next;
    }
return( -1 );    
}

static int EntFindOrder(Entity *ent) {
    int ret;

    while ( ent!=NULL ) {
	ret = SPLFindOrder(ent->u.splines.splines);
	if ( ret!=-1 )
return( ret );
	ent = ent->next;
    }
return( -1 );    
}

static int SFFindOrder(SplineFont *sf) {
    int i, ret;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	ret = SPLFindOrder(sf->chars[i]->layers[ly_fore].splines);
	if ( ret!=-1 )
return( ret );
    }
return( 0 );
}

static void SPLSetOrder(SplineSet *ss,int order2) {
    Spline *s, *first;
    SplinePoint *from, *to;

    while ( ss!=NULL ) {
	first = NULL;
	for ( s = ss->first->next; s!=NULL && s!=first ; s = s->to->next ) {
	    if ( first==NULL ) first = s;
	    if ( s->order2!=order2 ) {
		if ( s->knownlinear ) {
		    s->from->nextcp = s->from->me;
		    s->to->prevcp = s->to->me;
		    s->from->nonextcp = s->to->noprevcp = true;
		    s->order2 = order2;
		} else if ( order2 ) {
		    from = SplineTtfApprox(s);
		    s->from->nextcp = from->nextcp;
		    s->from->nonextcp = from->nonextcp;
		    s->from->next = from->next;
		    from->next->from = s->from;
		    SplinePointFree(from);
		    for ( to = s->from->next->to; to->next!=NULL; to=to->next->to );
		    s->to->prevcp = to->prevcp;
		    s->to->noprevcp = to->noprevcp;
		    s->to->prev = to->prev;
		    to->prev->to = s->to;
		    SplinePointFree(to);
		    to = s->to;
		    from = s->from;
		    SplineFree(s);
		    if ( first==s ) first = from->next;
		    s = to->prev;
		} else {
		    s->from->nextcp.x = s->splines[0].c/3 + s->from->me.x;
		    s->from->nextcp.y = s->splines[1].c/3 + s->from->me.y;
		    s->to->prevcp.x = s->from->nextcp.x+ (s->splines[0].b+s->splines[0].c)/3;
		    s->to->prevcp.y = s->from->nextcp.y+ (s->splines[1].b+s->splines[1].c)/3;
		    s->order2 = false;
		    SplineRefigure(s);
		}
	    }
	}
	ss = ss->next;
    }
}

static void EntSetOrder(Entity *ent,int order2) {
    while ( ent!=NULL ) {
	SPLSetOrder(ent->u.splines.splines,order2);
	ent = ent->next;
    }
}

static void SFSetOrder(SplineFont *sf,int order2) {
    int i,j;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	for ( j=ly_fore; j<sf->chars[i]->layer_cnt; ++j )
	    SPLSetOrder(sf->chars[i]->layers[j].splines,order2);
    }
}

SplineFont *SFReadSVG(char *filename, int flags) {
    xmlNodePtr *fonts, font;
    xmlDocPtr doc;
    SplineFont *sf;
    char *temp=filename, *pt, *lparen;
    char *oldloc;
    char *chosenname = NULL;

    if ( !libxml_init_base()) {
	fprintf( stderr, "Can't find libxml2.\n" );
return( NULL );
    }

    pt = strrchr(filename,'/');
    if ( pt==NULL ) pt = filename;
    if ( (lparen=strchr(pt,'('))!=NULL && strchr(lparen,')')!=NULL ) {
	temp = copy(filename);
	pt = temp + (lparen-filename);
	*pt = '\0';
    }

    doc = _xmlParseFile(temp);
    if ( temp!=filename ) free(temp);
    if ( doc==NULL ) {
	/* Can I get an error message from libxml? */
return( NULL );
    }

    fonts = FindSVGFontNodes(doc);
    if ( fonts==NULL || fonts[0]==NULL ) {
	fprintf( stderr, "This file contains no SVG fonts.\n" );
	_xmlFreeDoc(doc);
return( NULL );
    }
    font = fonts[0];
    if ( fonts[1]!=NULL ) {
	xmlChar *name;
	font = SVGPickFont(fonts,filename);
	name = _xmlGetProp(font,(xmlChar *) "id");
	if ( name!=NULL ) {
	    chosenname = cu_copy(utf82u_copy((char *) name));
	    _xmlFree(name);
	}
    }
    free(fonts);
    oldloc = setlocale(LC_NUMERIC,"C");
    sf = SVGParseFont(font);
    setlocale(LC_NUMERIC,oldloc);
    _xmlFreeDoc(doc);

    if ( sf!=NULL ) {
	sf->order2 = SFFindOrder(sf);
	SFSetOrder(sf,sf->order2);
	sf->chosenname = chosenname;
    }
return( sf );
}

char **NamesReadSVG(char *filename) {
    xmlNodePtr *fonts;
    xmlDocPtr doc;
    char **ret=NULL;
    unichar_t *utemp;
    int cnt;
    xmlChar *name;

    if ( !libxml_init_base()) {
	fprintf( stderr, "Can't find libxml2.\n" );
return( NULL );
    }

    doc = _xmlParseFile(filename);
    if ( doc==NULL ) {
	/* Can I get an error message from libxml? */
return( NULL );
    }

    fonts = FindSVGFontNodes(doc);
    if ( fonts==NULL || fonts[0]==NULL ) {
	_xmlFreeDoc(doc);
return( NULL );
    }

    for ( cnt=0; fonts[cnt]!=NULL; ++cnt);
    ret = galloc((cnt+1)*sizeof(char *));
    for ( cnt=0; fonts[cnt]!=NULL; ++cnt) {
	name = _xmlGetProp(fonts[cnt],(xmlChar *) "id");
	if ( name==NULL ) {
	    ret[cnt] = copy("nameless-font");
	} else {
	    utemp = utf82u_copy((char *) name);
	    ret[cnt] = cu_copy(utemp);
	    free(utemp);
	    _xmlFree(name);
	}
    }
    ret[cnt] = NULL;

    free(fonts);
    _xmlFreeDoc(doc);

return( ret );
}

Entity *EntityInterpretSVG(char *filename,int em_size,int ascent) {
    xmlDocPtr doc;
    xmlNodePtr top;
    char *oldloc;
    Entity *ret;
    int order2;

    if ( !libxml_init_base()) {
	fprintf( stderr, "Can't find libxml2.\n" );
return( NULL );
    }
    doc = _xmlParseFile(filename);
    if ( doc==NULL ) {
	/* Can I get an error message from libxml???? */
return( NULL );
    }

    top = _xmlDocGetRootElement(doc);
    if ( _xmlStrcmp(top->name,(xmlChar *) "svg")!=0 ) {
	fprintf( stderr, "%s does not contain an <svg> element at the top\n", filename);
	_xmlFreeDoc(doc);
return( NULL );
    }

    oldloc = setlocale(LC_NUMERIC,"C");
    ret = SVGParseSVG(top,em_size,ascent);
    setlocale(LC_NUMERIC,oldloc);
    _xmlFreeDoc(doc);

    if ( loaded_fonts_same_as_new )
	order2 = new_fonts_are_order2;
    else
	order2 = EntFindOrder(ret);
    if ( order2==-1 ) order2 = 0;
    EntSetOrder(ret,order2);

return( ret );
}

SplineSet *SplinePointListInterpretSVG(char *filename,int em_size,int ascent) {
    Entity *ret = EntityInterpretSVG(filename, em_size, ascent);
    int flags = -1;
return( SplinesFromEntities(ret,&flags));
}
#endif
