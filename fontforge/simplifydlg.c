/* Copyright (C) 2002-2005 by George Williams */
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
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
#include "ustring.h"

#define CID_Extrema	1000
#define CID_Slopes	1001
#define CID_Error	1002
#define CID_Smooth	1003
#define CID_SmoothTan	1004
#define CID_SmoothHV	1005
#define CID_FlattenBumps	1006
#define CID_FlattenBound	1007
#define CID_LineLenMax		1008

static double olderr_rat = 1/1000., oldsmooth_tan=.2,
	oldlinefixup_rat = 10./1000., oldlinelenmax_rat = 1/100.;
static int oldextrema = false;
static int oldslopes = false;
static int oldsmooth = true;
static int oldsmoothhv = true;
static int oldlinefix = false;

typedef struct simplifydlg {
    int flags;
    double err;
    double tan_bounds;
    double linefixup;
    double linelenmax;
    int done;
    int cancelled;
    int em_size;
} Simple;

static int Sim_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	Simple *sim = GDrawGetUserData(GGadgetGetWindow(g));
	int badparse=false;
	sim->flags = 0;
	if ( GGadgetIsChecked(GWidgetGetControl(GGadgetGetWindow(g),CID_Extrema)) )
	    sim->flags = sf_ignoreextremum;
	if ( GGadgetIsChecked(GWidgetGetControl(GGadgetGetWindow(g),CID_Slopes)) )
	    sim->flags |= sf_ignoreslopes;
	if ( GGadgetIsChecked(GWidgetGetControl(GGadgetGetWindow(g),CID_Smooth)) )
	    sim->flags |= sf_smoothcurves;
	if ( GGadgetIsChecked(GWidgetGetControl(GGadgetGetWindow(g),CID_SmoothHV)) )
	    sim->flags |= sf_choosehv;
	if ( GGadgetIsChecked(GWidgetGetControl(GGadgetGetWindow(g),CID_FlattenBumps)) )
	    sim->flags |= sf_forcelines;
	sim->err = GetRealR(GGadgetGetWindow(g),CID_Error,_STR_ErrorLimit,&badparse);
	if ( sim->flags&sf_smoothcurves )
	    sim->tan_bounds= GetRealR(GGadgetGetWindow(g),CID_SmoothTan,_STR_Tangent,&badparse);
	if ( sim->flags&sf_forcelines )
	    sim->linefixup= GetRealR(GGadgetGetWindow(g),CID_FlattenBound,_STR_BumpSize,&badparse);
	sim->linelenmax = GetRealR(GGadgetGetWindow(g),CID_LineLenMax,_STR_LineLenMax,&badparse);
	if ( badparse )
return( true );
	olderr_rat = sim->err/sim->em_size;
	oldextrema = (sim->flags&sf_ignoreextremum);
	oldslopes = (sim->flags&sf_ignoreslopes);
	oldsmooth = (sim->flags&sf_smoothcurves);
	oldlinefix = (sim->flags&sf_forcelines);
	if ( oldsmooth ) {
	    oldsmooth_tan = sim->tan_bounds;
	    oldsmoothhv = (sim->flags&sf_choosehv);
	}
	if ( oldlinefix )
	    oldlinefixup_rat = sim->linefixup/sim->em_size;
	oldlinelenmax_rat = sim->linelenmax/sim->em_size;
	
	sim->done = true;
    }
return( true );
}

static int Sim_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	Simple *sim = GDrawGetUserData(GGadgetGetWindow(g));
	sim->done = sim->cancelled = true;
    }
return( true );
}

static int sim_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	Simple *sim = GDrawGetUserData(gw);
	sim->done = sim->cancelled = true;
    } else if ( event->type == et_char ) {
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

int SimplifyDlg(SplineFont *sf, struct simplifyinfo *smpl) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[22];
    GTextInfo label[22];
    Simple sim;
    char buffer[12], buffer2[12], buffer3[12], buffer4[12];

    memset(&sim,0,sizeof(sim));
    sim.em_size = sf->ascent+sf->descent;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_isdlg|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
#if defined(FONTFORGE_CONFIG_GDRAW)
    wattrs.window_title = GStringGetResource(_STR_Simplify,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    wattrs.window_title = _("Simplify");
#endif
    wattrs.is_dlg = true;
    pos.x = pos.y = 0;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,180));
    pos.height = GDrawPointsToPixels(NULL,255);
    gw = GDrawCreateTopWindow(NULL,&pos,sim_e_h,&sim,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    label[0].text = (unichar_t *) _STR_ErrorLimit;
    label[0].text_in_resource = true;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.pos.x = 10; gcd[0].gd.pos.y = 12;
    gcd[0].gd.flags = gg_enabled|gg_visible;
    gcd[0].creator = GLabelCreate;

    sprintf( buffer, "%.3g", olderr_rat*sim.em_size );
    label[1].text = (unichar_t *) buffer;
    label[1].text_is_1byte = true;
    gcd[1].gd.label = &label[1];
    gcd[1].gd.pos.x = 70; gcd[1].gd.pos.y = gcd[0].gd.pos.y-6;
    gcd[1].gd.pos.width = 40;
    gcd[1].gd.flags = gg_enabled|gg_visible;
    gcd[1].gd.cid = CID_Error;
    gcd[1].creator = GTextFieldCreate;

    gcd[2].gd.pos.x = gcd[1].gd.pos.x+gcd[1].gd.pos.width+3;
    gcd[2].gd.pos.y = gcd[0].gd.pos.y;
    gcd[2].gd.flags = gg_visible | gg_enabled ;
    label[2].text = (unichar_t *) _STR_EmUnits;
    label[2].text_in_resource = true;
    gcd[2].gd.label = &label[2];
    gcd[2].creator = GLabelCreate;

    label[3].text = (unichar_t *) _STR_RemoveExtrema;
    label[3].text_in_resource = true;
    gcd[3].gd.label = &label[3];
    gcd[3].gd.pos.x = 8; gcd[3].gd.pos.y = gcd[1].gd.pos.y+24;
    gcd[3].gd.flags = gg_enabled|gg_visible;
    if ( oldextrema )
	gcd[3].gd.flags |= gg_cb_on;
#if defined(FONTFORGE_CONFIG_GDRAW)
    gcd[3].gd.popup_msg = GStringGetResource(_STR_RemoveExtremaPopup,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    gcd[3].gd.popup_msg = _("Normally simplify will not remove points at the extrema of curves\n(both PostScript and TrueType suggest you retain these points)");
#endif
    gcd[3].gd.cid = CID_Extrema;
    gcd[3].creator = GCheckBoxCreate;

    label[4].text = (unichar_t *) _STR_ChangeSlopes;
    label[4].text_in_resource = true;
    gcd[4].gd.label = &label[4];
    gcd[4].gd.pos.x = 8; gcd[4].gd.pos.y = gcd[3].gd.pos.y+14;
    gcd[4].gd.flags = gg_enabled|gg_visible;
    if ( oldslopes )
	gcd[4].gd.flags |= gg_cb_on;
    gcd[4].gd.cid = CID_Slopes;
#if defined(FONTFORGE_CONFIG_GDRAW)
    gcd[4].gd.popup_msg = GStringGetResource(_STR_ChangeSlopesPopup,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    gcd[4].gd.popup_msg = _("Normally simplify will not change the slope of the contour at the points.");
#endif
    gcd[4].creator = GCheckBoxCreate;

    gcd[5].gd.pos.x = 15; gcd[5].gd.pos.y = gcd[4].gd.pos.y + 20;
    gcd[5].gd.pos.width = 150;
    gcd[5].gd.flags = gg_enabled|gg_visible;
    gcd[5].creator = GLineCreate;

    label[6].text = (unichar_t *) _STR_CurveSmoothing;
    label[6].text_in_resource = true;
    gcd[6].gd.label = &label[6];
    gcd[6].gd.pos.x = 8; gcd[6].gd.pos.y = gcd[5].gd.pos.y+4;
    if ( sf->order2 )
	gcd[6].gd.flags = gg_visible;
    else {
	gcd[6].gd.flags = gg_enabled|gg_visible;
	if ( oldsmooth )
	    gcd[6].gd.flags |= gg_cb_on;
    }
#if defined(FONTFORGE_CONFIG_GDRAW)
    gcd[6].gd.popup_msg = GStringGetResource(_STR_CurveSmoothingPopup,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    gcd[6].gd.popup_msg = _("Simplify will examine corner points whose control points are almost\ncolinear and smooth them into curve points");
#endif
    gcd[6].gd.cid = CID_Smooth;
    gcd[6].creator = GCheckBoxCreate;

    label[7].text = (unichar_t *) _STR_IfTan;
    label[7].text_in_resource = true;
    gcd[7].gd.label = &label[7];
    gcd[7].gd.pos.x = 20; gcd[7].gd.pos.y = gcd[6].gd.pos.y+24;
    gcd[7].gd.flags = gg_enabled|gg_visible;
    if ( sf->order2 ) gcd[7].gd.flags = gg_visible;
    gcd[7].creator = GLabelCreate;

    sprintf( buffer2, "%.3g", oldsmooth_tan );
    label[8].text = (unichar_t *) buffer2;
    label[8].text_is_1byte = true;
    gcd[8].gd.label = &label[8];
    gcd[8].gd.pos.x = 94; gcd[8].gd.pos.y = gcd[7].gd.pos.y-6;
    gcd[8].gd.pos.width = 40;
    gcd[8].gd.flags = gg_enabled|gg_visible;
    if ( sf->order2 ) gcd[8].gd.flags = gg_visible;
    gcd[8].gd.cid = CID_SmoothTan;
    gcd[8].creator = GTextFieldCreate;

    label[9].text = (unichar_t *) _STR_SnapToHV;
    label[9].text_in_resource = true;
    gcd[9].gd.label = &label[9];
    gcd[9].gd.pos.x = 17; gcd[9].gd.pos.y = gcd[8].gd.pos.y+24; 
    if ( sf->order2 )
	gcd[9].gd.flags = gg_visible;
    else {
	gcd[9].gd.flags = gg_enabled|gg_visible;
	if ( oldsmoothhv )
	    gcd[9].gd.flags |= gg_cb_on;
    }
#if defined(FONTFORGE_CONFIG_GDRAW)
    gcd[9].gd.popup_msg = GStringGetResource(_STR_SnapToHVPopup,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    gcd[9].gd.popup_msg = _("If the slope of an adjusted point is near horizontal or vertical\nsnap to that");
#endif
    gcd[9].gd.cid = CID_SmoothHV;
    gcd[9].creator = GCheckBoxCreate;

    label[10].text = (unichar_t *) _STR_FlattenBumps;
    label[10].text_in_resource = true;
    gcd[10].gd.label = &label[10];
    gcd[10].gd.pos.x = 8; gcd[10].gd.pos.y = gcd[9].gd.pos.y+14; 
    if ( sf->order2 )
	gcd[10].gd.flags = gg_visible;
    else {
	gcd[10].gd.flags = gg_enabled|gg_visible;
	if ( oldlinefix )
	    gcd[10].gd.flags |= gg_cb_on;
    }
#if defined(FONTFORGE_CONFIG_GDRAW)
    gcd[10].gd.popup_msg = GStringGetResource(_STR_FlattenBumpsPopup,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    gcd[10].gd.popup_msg = _("If a line has a bump on it then flatten out that bump");
#endif
    gcd[10].gd.cid = CID_FlattenBumps;
    gcd[10].creator = GCheckBoxCreate;

    label[11].text = (unichar_t *) _STR_IfSmallerThan;
    label[11].text_in_resource = true;
    gcd[11].gd.label = &label[11];
    gcd[11].gd.pos.x = 20; gcd[11].gd.pos.y = gcd[10].gd.pos.y+24;
    gcd[11].gd.flags = gg_enabled|gg_visible;
    if ( sf->order2 ) gcd[11].gd.flags = gg_visible;
    gcd[11].creator = GLabelCreate;

    sprintf( buffer3, "%.3g", oldlinefixup_rat*sim.em_size );
    label[12].text = (unichar_t *) buffer3;
    label[12].text_is_1byte = true;
    gcd[12].gd.label = &label[12];
    gcd[12].gd.pos.x = 90; gcd[12].gd.pos.y = gcd[11].gd.pos.y-6;
    gcd[12].gd.pos.width = 40;
    gcd[12].gd.flags = gg_enabled|gg_visible;
    if ( sf->order2 ) gcd[12].gd.flags = gg_visible;
    gcd[12].gd.cid = CID_FlattenBound;
    gcd[12].creator = GTextFieldCreate;

    gcd[13].gd.pos.x = gcd[12].gd.pos.x+gcd[12].gd.pos.width+3;
    gcd[13].gd.pos.y = gcd[11].gd.pos.y;
    gcd[13].gd.flags = gg_visible | gg_enabled ;
    if ( sf->order2 ) gcd[13].gd.flags = gg_visible;
    label[13].text = (unichar_t *) _STR_EmUnits;
    label[13].text_in_resource = true;
    gcd[13].gd.label = &label[13];
    gcd[13].creator = GLabelCreate;


    label[14].text = (unichar_t *) _STR_DontSmoothLines;
    label[14].text_in_resource = true;
    gcd[14].gd.label = &label[14];
    gcd[14].gd.pos.x = 8; gcd[14].gd.pos.y = gcd[13].gd.pos.y+14; 
    gcd[14].gd.flags = gg_enabled|gg_visible;
    gcd[14].creator = GLabelCreate;

    label[15].text = (unichar_t *) _STR_LongerThan;
    label[15].text_in_resource = true;
    gcd[15].gd.label = &label[15];
    gcd[15].gd.pos.x = 20; gcd[15].gd.pos.y = gcd[14].gd.pos.y+24;
    gcd[15].gd.flags = gg_enabled|gg_visible;
    gcd[15].creator = GLabelCreate;

    sprintf( buffer4, "%.3g", oldlinelenmax_rat*sim.em_size );
    label[16].text = (unichar_t *) buffer4;
    label[16].text_is_1byte = true;
    gcd[16].gd.label = &label[16];
    gcd[16].gd.pos.x = 90; gcd[16].gd.pos.y = gcd[15].gd.pos.y-6;
    gcd[16].gd.pos.width = 40;
    gcd[16].gd.flags = gg_enabled|gg_visible;
    gcd[16].gd.cid = CID_LineLenMax;
    gcd[16].creator = GTextFieldCreate;

    gcd[17].gd.pos.x = gcd[16].gd.pos.x+gcd[16].gd.pos.width+3;
    gcd[17].gd.pos.y = gcd[15].gd.pos.y;
    gcd[17].gd.flags = gg_visible | gg_enabled ;
    label[17].text = (unichar_t *) _STR_EmUnits;
    label[17].text_in_resource = true;
    gcd[17].gd.label = &label[17];
    gcd[17].creator = GLabelCreate;



    gcd[18].gd.pos.x = 20-3; gcd[18].gd.pos.y = gcd[17].gd.pos.y+30;
    gcd[18].gd.pos.width = -1; gcd[18].gd.pos.height = 0;
    gcd[18].gd.flags = gg_visible | gg_enabled | gg_but_default;
    label[18].text = (unichar_t *) _STR_OK;
    label[18].text_in_resource = true;
    gcd[18].gd.mnemonic = 'O';
    gcd[18].gd.label = &label[18];
    gcd[18].gd.handle_controlevent = Sim_OK;
    gcd[18].creator = GButtonCreate;

    gcd[19].gd.pos.x = -20; gcd[19].gd.pos.y = gcd[18].gd.pos.y+3;
    gcd[19].gd.pos.width = -1; gcd[19].gd.pos.height = 0;
    gcd[19].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[19].text = (unichar_t *) _STR_Cancel;
    label[19].text_in_resource = true;
    gcd[19].gd.label = &label[19];
    gcd[19].gd.mnemonic = 'C';
    gcd[19].gd.handle_controlevent = Sim_Cancel;
    gcd[19].creator = GButtonCreate;

    gcd[20].gd.pos.x = 2; gcd[20].gd.pos.y = 2;
    gcd[20].gd.pos.width = pos.width-4; gcd[20].gd.pos.height = pos.height-4;
    gcd[20].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
    gcd[20].creator = GGroupCreate;

    GGadgetsCreate(gw,gcd);
    GWidgetIndicateFocusGadget(GWidgetGetControl(gw,CID_Error));
    GTextFieldSelect(GWidgetGetControl(gw,CID_Error),0,-1);

    GWidgetHidePalettes();
    GDrawSetVisible(gw,true);
    while ( !sim.done )
	GDrawProcessOneEvent(NULL);
    GDrawSetVisible(gw,false);
    if ( sim.cancelled )
return( false );

    smpl->flags = sim.flags;
    smpl->err = sim.err;
    smpl->tan_bounds = sim.tan_bounds;
    smpl->linefixup = sim.linefixup;
    smpl->linelenmax = sim.linelenmax;
return( true );
}
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
