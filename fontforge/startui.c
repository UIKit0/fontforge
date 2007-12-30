/* Copyright (C) 2000-2007 by George Williams */
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
#include <gfile.h>
#include <gresource.h>
#include <ustring.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
#include <unistd.h>
#include <dynamic.h>
#ifdef __Mac
# include <stdlib.h>		/* getenv,setenv */
#endif

int splash = 1;

static void _dousage(void) {
    printf( "fontforge [options] [fontfiles]\n" );
    printf( "\t-new\t\t\t (creates a new font)\n" );
    printf( "\t-last\t\t\t (loads the last sfd file closed)\n" );
#if HANYANG
    printf( "\t-newkorean\t\t (creates a new korean font)\n" );
#endif
    printf( "\t-recover none|auto|inquire|clean (control error recovery)\n" );
    printf( "\t-nosplash\t\t (no splash screen)\n" );
    printf( "\t-display display-name\t (sets the X display)\n" );
    printf( "\t-depth val\t\t (sets the display depth if possible)\n" );
    printf( "\t-vc val\t\t\t (sets the visual class if possible)\n" );
    printf( "\t-cmap current|copy|private\t (sets the type of colormap)\n" );
    printf( "\t-dontopenxdevices\t (in case that fails)\n" );
    printf( "\t-sync\t\t\t (syncs the display, debugging)\n" );
    printf( "\t-keyboard ibm|mac|sun|ppc  (generates appropriate hotkeys in menus)\n" );
#if MyMemory
    printf( "\t-memory\t\t\t (turns on memory checks, debugging)\n" );
#endif
    printf( "\t-usage\t\t\t (displays this message, and exits)\n" );
    printf( "\t-help\t\t\t (displays this message, invokes a browser)\n\t\t\t\t  (Using the BROWSER environment variable)\n" );
    printf( "\t-version\t\t (prints the version of fontforge and exits)\n" );
    printf( "\t-library-status\t (prints information about optional libraries\n\t\t\t\t and exits)\n" );
#ifndef _NO_PYTHON
    printf( "\t-lang=py\t\t use python for scripts (may precede -script)\n" );
#endif
#ifndef _NO_FFSCRIPT
    printf( "\t-lang=ff\t\t use fontforge's legacy scripting language\n" );
#endif
    printf( "\t-script scriptfile\t (executes scriptfile)\n" );
    printf( "\t\tmust be the first option (or follow -lang).\n" );
    printf( "\t\tAll others passed to scriptfile.\n" );
    printf( "\t-dry scriptfile\t (syntax checks scriptfile)\n" );
    printf( "\t\tmust be the first option. All others passed to scriptfile.\n" );
    printf( "\t-c script-string\t (executes argument as scripting cmds)\n" );
    printf( "\t\tmust be the first option. All others passed to the script.\n" );
    printf( "\n" );
    printf( "FontForge will read postscript (pfa, pfb, ps, cid), opentype (otf),\n" );
    printf( "\ttruetype (ttf,ttc), macintosh resource fonts (dfont,bin,hqx),\n" );
    printf( "\tand bdf and pcf fonts. It will also read it's own format --\n" );
    printf( "\tsfd files.\n" );
    printf( "If no fontfiles are specified (and -new is not either and there's nothing\n" );
    printf( "\tto recover) then fontforge will produce an open font dlg.\n" );
    printf( "If a scriptfile is specified then FontForge will not open the X display\n" );
    printf( "\tnor will it process any additional arguments. It will execute the\n" );
    printf( "\tscriptfile and give it any remaining arguments\n" );
    printf( "If the first argument is an executable filename, and that file's first\n" );
    printf( "\tline contains \"fontforge\" then it will be treated as a scriptfile.\n\n" );
    printf( "For more information see:\n\thttp://fontforge.sourceforge.net/\n" );
    printf( "Send bug reports to:\tfontforge-devel@lists.sourceforge.net\n" );
}

static void dousage(void) {
    _dousage();
exit(0);
}

static void dohelp(void) {
    _dousage();
    help("overview.html");
exit(0);
}

static struct library_descriptor {
    char *libname;
    char *entry_point;
    char *description;
    char *url;
    int usable;
    struct library_descriptor *depends_on;
} libs[] = {
    {
#ifdef PYTHON_LIB_NAME
	#PYTHON_LIB_NAME,
#else
	"libpython-",		/* a bad name */
#endif
	dlsymmod("Py_Main"),
	"This allows users to write python scripts in fontforge",
	"http://www.python.org/",
#ifdef _NO_PYTHON
	0
#else
	1
#endif
    },
    { "libspiro", dlsymmod("TaggedSpiroCPsToBezier"), "This allows you to edit with Raph Levien's spiros.", "http://libspiro.sf.net/",
#ifdef _NO_LIBSPIRO
	0
#else
	1
#endif
    },
    { "libz", dlsymmod("deflateEnd"), "This is a prerequisite for reading png files,\n\t and is used for some pdf files.", "http://www.gzip.org/zlib/",
#ifdef _NO_LIBPNG
	0
#else
	1
#endif
    },
    { "libpng", dlsymmod("png_create_read_struct"), "This is one way to read png files.", "http://www.libpng.org/pub/png/libpng.html",
#ifdef _NO_LIBPNG
	0,
#else
	1,
#endif
	&libs[1] },
    { "libpng12", dlsymmod("png_create_read_struct"), "This is another way to read png files.", "http://www.libpng.org/pub/png/libpng.html",
#ifdef _NO_LIBPNG
	0,
#else
	1,
#endif
	&libs[1] },
    { "libjpeg", dlsymmod("jpeg_CreateDecompress"), "This allows fontforge to load jpeg images.", "http://www.ijg.org/",
#ifdef _NO_LIBPNG
	0
#else
	1
#endif
    },
    { "libtiff", dlsymmod("TIFFOpen"), "This allows fontforge to open tiff images.", "http://www.libtiff.org/",
#ifdef _NO_LIBTIFF
	0
#else
	1
#endif
    },
    { "libgif", dlsymmod("DGifOpenFileName"), "This allows fontforge to open gif images.", "http://gnuwin32.sf.net/packages/libungif.htm",
#ifdef _NO_LIBUNGIF
	0
#else
	1
#endif
    },
    { "libungif", dlsymmod("DGifOpenFileName"), "This allows fontforge to open gif images.", "http://gnuwin32.sf.net/packages/libungif.htm",
#ifdef _NO_LIBUNGIF
	0
#else
	1
#endif
    },
    { "libxml2", dlsymmod("xmlParseFile"), "This allows fontforge to load svg files and fonts and ufo fonts.", "http://xmlsoft.org/",
#ifdef _NO_LIBXML
	0
#else
	1
#endif
    },
    { "libuninameslist", dlsymmod("UnicodeNameAnnot"), "This provides fontforge with the names of all (named) unicode characters", "http://libuninameslist.sf.net/",
#ifdef _NO_LIBUNINAMESLIST
	0
#else
	1
#endif
    },
    { "libfreetype", dlsymmod("FT_New_Memory_Face"), "This provides a better rasterizer than the one built in to fontforge", "http://freetype.sf.net/",
#if _NO_FREETYPE || _NO_MMAP
	0
#else
	1
#endif
    },
    { NULL }
};

static void _dolibrary(void) {
    int i;
    char buffer[300];
    int fail, isfreetype, hasdebugger;
    DL_CONST void *lib_handle;

    fprintf( stderr, "\n" );
    for ( i=0; libs[i].libname!=NULL; ++i ) {
	fail = false;
	if ( libs[i].depends_on!=NULL ) {
	    sprintf( buffer, "%s%s", libs[i].depends_on->libname, SO_EXT );
	    lib_handle = dlopen(buffer,RTLD_LAZY);
	    if ( lib_handle==NULL )
		fail = 3;
	    else {
		if ( dlsymbare(lib_handle,libs[i].depends_on->entry_point)==NULL )
		    fail = 4;
	    }
	}
	if ( !fail ) {
	    sprintf( buffer, "%s%s", libs[i].libname, SO_EXT );
	    lib_handle = dlopen(buffer,RTLD_LAZY);
	    if ( lib_handle==NULL )
		fail = true;
	    else {
		if ( dlsymbare(lib_handle,libs[i].entry_point)==NULL )
		    fail = 2;
	    }
	}
	isfreetype = strcmp(libs[i].libname,"libfreetype")==0;
	hasdebugger = false;
	if ( !fail && isfreetype && dlsym(lib_handle,"TT_RunIns")!=NULL )
	    hasdebugger = true;
	fprintf( stderr, "%-15s - %s\n", libs[i].libname,
		fail==0 ? "is present and appears functional on your system." :
		fail==1 ? "is not present on your system." :
		fail==2 ? "is present on your system but is not functional." :
		fail==3 ? "a prerequisite library is missing." :
			"a prerequisite library is not functional." );
	fprintf( stderr, "\t%s\n", libs[i].description );
	if ( isfreetype ) {
	    if ( hasdebugger )
		fprintf( stderr, "\tThis version of freetype includes the byte code interpreter\n\t which means you can use fontforge as a truetype debugger.\n" );
	    else
		fprintf( stderr, "\tThis version of freetype does notinclude the byte code interpreter\n\t which means you cannot use fontforge as a truetype debugger.\n\t If you want the debugger you must download freetype source,\n\t enable the bytecode interpreter, and then build it.\n" );
	}
	if ( fail || (isfreetype && !hasdebugger))
	    fprintf( stderr, "\tYou may download %s from %s .\n", libs[i].libname, libs[i].url );
	if ( !libs[i].usable )
	    fprintf( stderr, "\tUnfortunately this version of fontforge is not configured to use this\n\t library.  You must rebuild from source.\n" );
    }
}

static void dolibrary(void) {
    _dolibrary();
exit(0);
}

struct delayed_event {
    void *data;
    void (*func)(void *);
};

static void BuildCharHook(GDisplay *gd) {
    GWidgetCreateInsChar();
}

static void InsCharHook(GDisplay *gd,unichar_t ch) {
    GInsCharSetChar(ch);
}

extern GImage splashimage;
static GWindow splashw;
static GTimer *autosave_timer, *splasht;
static GFont *splash_font, *splash_italic;
static int as,fh, linecnt;
static unichar_t msg[450];
static unichar_t *lines[20], *is, *ie;

void ShowAboutScreen(void) {
    static int first=1;

    if ( first ) {
	GDrawResize(splashw,splashimage.u.image->width,splashimage.u.image->height+linecnt*fh);
	first = false;
    }
    if ( splasht!=NULL )
	GDrawCancelTimer(splasht);
    splasht=NULL;
    GDrawSetVisible(splashw,true);
}

static void SplashLayout() {
    unichar_t *start, *pt, *lastspace;
    extern const char *source_modtime_str;
    extern const char *source_version_str;

    uc_strcpy(msg, "When my father finished his book on Renaissance printing (The Craft of Printing and the Publication of Shakespeare's Works) he told me that I would have to write the chapter on computer typography. This is my attempt to do so.");

    GDrawSetFont(splashw,splash_font);
    linecnt = 0;
    lines[linecnt++] = msg-1;
    for ( start = msg; *start!='\0'; start = pt ) {
	lastspace = NULL;
	for ( pt=start; ; ++pt ) {
	    if ( *pt==' ' || *pt=='\0' ) {
		if ( GDrawGetTextWidth(splashw,start,pt-start,NULL)<splashimage.u.image->width-10 )
		    lastspace = pt;
		else
	break;
		if ( *pt=='\0' )
	break;
	    }
	}
	if ( lastspace!=NULL )
	    pt = lastspace;
	lines[linecnt++] = pt;
	if ( *pt ) ++pt;
    }
    uc_strcpy(pt, " FontForge used to be named PfaEdit.");
    pt += u_strlen(pt);
    lines[linecnt++] = pt;
    uc_strcpy(pt,"  Version: ");;
    uc_strcat(pt,source_modtime_str);
    uc_strcat(pt," (");
    uc_strcat(pt,source_version_str);
#ifdef FONTFORGE_CONFIG_TYPE3
    uc_strcat(pt,"-ML");
#endif
    uc_strcat(pt,")");
    pt += u_strlen(pt);
    lines[linecnt++] = pt;
    uc_strcpy(pt,"  Library Version: ");
    uc_strcat(pt,library_version_configuration.library_source_modtime_string);
    lines[linecnt++] = pt+u_strlen(pt);
    lines[linecnt] = NULL;
    is = u_strchr(msg,'(');
    ie = u_strchr(msg,')');
}

void DelayEvent(void (*func)(void *), void *data) {
    struct delayed_event *info = gcalloc(1,sizeof(struct delayed_event));

    info->data = data;
    info->func = func;
    GDrawRequestTimer(splashw,100,0,info);
}

static void DoDelayedEvents(GEvent *event) {
    GTimer *t = event->u.timer.timer;
    struct delayed_event *info = (struct delayed_event *) (event->u.timer.userdata);

    if ( info!=NULL ) {
	(info->func)(info->data);
	free(info);
    }
    GDrawCancelTimer(t);
}

static int splash_e_h(GWindow gw, GEvent *event) {
    static int splash_cnt;
    GRect old;
    int i, y, x;
    static char *foolishness[] = {
/* GT: These strings are for fun. If they are offensive of incomprehensible */
/* GT: simply translate them as something dull like: "FontForge" */
	    N_("A free press discriminates\nagainst the illiterate."),
	    N_("A free press discriminates\nagainst the illiterate."),
	    N_("Gaudeamus Ligature!"),
	    N_("Gaudeamus Ligature!"),
	    N_("In the beginning was the letter..."),
	    N_("fontology recapitulates file-ogeny")
    };

    if ( event->type == et_expose ) {
	GDrawPushClip(gw,&event->u.expose.rect,&old);
	GDrawDrawImage(gw,&splashimage,NULL,0,0);
	GDrawSetFont(gw,splash_font);
	y = splashimage.u.image->height + as + fh/2;
	for ( i=1; i<linecnt; ++i ) {
	    if ( is>=lines[i-1]+1 && is<lines[i] ) {
		x = 8+GDrawDrawText(gw,8,y,lines[i-1]+1,is-lines[i-1]-1,NULL,0x000000);
		GDrawSetFont(gw,splash_italic);
		GDrawDrawText(gw,x,y,is,lines[i]-is,NULL,0x000000);
	    } else if ( ie>=lines[i-1]+1 && ie<lines[i] ) {
		x = 8+GDrawDrawText(gw,8,y,lines[i-1]+1,ie-lines[i-1]-1,NULL,0x000000);
		GDrawSetFont(gw,splash_font);
		GDrawDrawText(gw,x,y,ie,lines[i]-ie,NULL,0x000000);
	    } else
		GDrawDrawText(gw,8,y,lines[i-1]+1,lines[i]-lines[i-1]-1,NULL,0x000000);
	    y += fh;
	}
	GDrawPopClip(gw,&old);
    } else if ( event->type == et_map ) {
	splash_cnt = 0;
    } else if ( event->type == et_timer && event->u.timer.timer==autosave_timer ) {
	DoAutoSaves();
    } else if ( event->type == et_timer && event->u.timer.timer==splasht ) {
	if ( ++splash_cnt==1 )
	    GDrawResize(gw,splashimage.u.image->width,splashimage.u.image->height-24);
	else if ( splash_cnt==2 )
	    GDrawResize(gw,splashimage.u.image->width,splashimage.u.image->height);
	else if ( splash_cnt>=7 ) {
	    GGadgetEndPopup();
	    GDrawSetVisible(gw,false);
	    GDrawCancelTimer(splasht);
	    splasht = NULL;
	}
    } else if ( event->type == et_timer ) {
	DoDelayedEvents(event);
    } else if ( event->type==et_char || event->type==et_mousedown ||
	    event->type==et_close ) {
	GGadgetEndPopup();
	GDrawSetVisible(gw,false);
    } else if ( event->type==et_mousemove ) {
	GGadgetPreparePopup8(gw,_(foolishness[rand()%(sizeof(foolishness)/sizeof(foolishness[0]))]) );
    }
return( true );
}

static void AddR(char *prog, char *name, char *val ) {
    char *full = galloc(strlen(name)+strlen(val)+4);
    strcpy(full,name);
    strcat(full,": ");
    strcat(full,val);
    GResourceAddResourceString(full,prog);
}

#if defined(__Mac)
static int uses_local_x(int argc,char **argv) {
    int i;
    char *arg;

    for ( i=1; i<argc; ++i ) {
	arg = argv[i];
	if ( *arg=='-' ) {
	    if ( arg[0]=='-' && arg[1]=='-' )
		++arg;
	    if ( strcmp(arg,"-display")==0 )
return( false );		/* we use a different display */
	    if ( strcmp(arg,"-c")==0 )
return( false );		/* we use a script string, no x display at all */
	    if ( strcmp(arg,"-script")==0 )
return( false );		/* we use a script, no x display at all */
	    if ( strcmp(arg,"-")==0 )
return( false );		/* script on stdin */
	} else {
	    /* Is this argument a script file ? */
	    FILE *temp = fopen(argv[i],"r");
	    char buffer[200];
	    if ( temp==NULL )
return( true );			/* not a script file, so need local local X */
	    buffer[0] = '\0';
	    fgets(buffer,sizeof(buffer),temp);
	    fclose(temp);
	    if ( buffer[0]=='#' && buffer[1]=='!' &&
		    (strstr(buffer,"pfaedit")!=NULL || strstr(buffer,"fontforge")!=NULL )) {
return( false );		/* is a script file, so no need for X */

return( true );			/* not a script, so needs X */
	    }
	}
    }
return( true );
}
#endif

static char *getLocaleDir(void) {
    static char *sharedir=NULL;
    static int set=false;
    char *pt;
    int len;

    if ( set )
return( sharedir );

    set = true;
    pt = strstr(GResourceProgramDir,"/bin");
    if ( pt==NULL ) {
#ifdef PREFIX
return( PREFIX "/share/locale" );
#else
	pt = GResourceProgramDir + strlen(GResourceProgramDir);
#endif
    }
    len = (pt-GResourceProgramDir)+strlen("/share/locale")+1;
    sharedir = galloc(len);
    strncpy(sharedir,GResourceProgramDir,pt-GResourceProgramDir);
    strcpy(sharedir+(pt-GResourceProgramDir),"/share/locale");
return( sharedir );
}

int main( int argc, char **argv ) {
    extern const char *source_modtime_str;
    extern const char *source_version_str;
    const char *load_prefs = getenv("FONTFORGE_LOADPREFS");
    int i;
    int recover=2;
    int any;
    int next_recent=0;
    GRect pos;
    GWindowAttrs wattrs;
    char *display = NULL;
    FontRequest rq;
    static unichar_t times[] = { 't', 'i', 'm', 'e', 's',',','c','l','e','a','r','l','y','u',',','u','n','i','f','o','n','t', '\0' };
    int ds, ld;

#ifdef FONTFORGE_CONFIG_TYPE3
    fprintf( stderr, "Copyright (c) 2000-2007 by George Williams.\n Executable based on sources from %s-ML.\n",
	    source_modtime_str );
#else
    fprintf( stderr, "Copyright (c) 2000-2007 by George Williams.\n Executable based on sources from %s.\n",
	    source_modtime_str );
#endif
    fprintf( stderr, " Library based on sources from %s.\n", library_version_configuration.library_source_modtime_string );

#if defined(__Mac)
    /* Start X if they haven't already done so. Well... try anyway */
    /* Must be before we change DYLD_LIBRARY_PATH or X won't start */
    if ( uses_local_x(argc,argv) && getenv("DISPLAY")==NULL ) {
	system( "open /Applications/Utilities/X11.app/" );
	setenv("DISPLAY",":0.0",0);
    }
#endif

    FF_SetUiInterface(&gdraw_ui_interface);
    FF_SetPrefsInterface(&gdraw_prefs_interface);
    FF_SetSCInterface(&gdraw_sc_interface);
    FF_SetCVInterface(&gdraw_cv_interface);
    FF_SetBCInterface(&gdraw_bc_interface);
    FF_SetFVInterface(&gdraw_fv_interface);
    FF_SetFIInterface(&gdraw_fi_interface);
    FF_SetMVInterface(&gdraw_mv_interface);
    FF_SetClipInterface(&gdraw_clip_interface);
#ifndef _NO_PYTHON
    PythonUI_Init();
#endif

    InitSimpleStuff();

    GResourceSetProg(argv[0]);

    GMenuSetShortcutDomain("FontForge-MenuShortCuts");
    bind_textdomain_codeset("FontForge-MenuShortCuts","UTF-8");
    bindtextdomain("FontForge-MenuShortCuts", getLocaleDir());
    bind_textdomain_codeset("FontForge","UTF-8");
    bindtextdomain("FontForge", getLocaleDir());
    textdomain("FontForge");
    GResourceUseGetText();

    if ( load_prefs!=NULL && strcasecmp(load_prefs,"Always")==0 )
	LoadPrefs();
    if ( default_encoding==NULL )
	default_encoding=FindOrMakeEncoding("ISO8859-1");
    if ( default_encoding==NULL )
	default_encoding=&custom;	/* In case iconv is broken */
    CheckIsScript(argc,argv);		/* Will run the script and exit if it is a script */
					/* If there is no UI, there is always a script */
			                /*  and we will never return from the above */
    if ( load_prefs==NULL ||
	    (strcasecmp(load_prefs,"Always")!=0 &&	/* Already loaded */
	     strcasecmp(load_prefs,"Never")!=0 ))
	LoadPrefs();
    for ( i=1; i<argc; ++i ) {
	char *pt = argv[i];
	if ( pt[0]=='-' && pt[1]=='-' )
	    ++pt;
	if ( strcmp(pt,"-sync")==0 )
	    GResourceAddResourceString("Gdraw.Synchronize: true",argv[0]);
	else if ( strcmp(pt,"-depth")==0 && i<argc-1 )
	    AddR(argv[0],"Gdraw.Depth", argv[++i]);
	else if ( strcmp(pt,"-vc")==0 && i<argc-1 )
	    AddR(argv[0],"Gdraw.VisualClass", argv[++i]);
	else if ( (strcmp(pt,"-cmap")==0 || strcmp(pt,"-colormap")==0) && i<argc-1 )
	    AddR(argv[0],"Gdraw.Colormap", argv[++i]);
	else if ( (strcmp(pt,"-dontopenxdevices")==0) )
	    AddR(argv[0],"Gdraw.DontOpenXDevices", "true");
	else if ( strcmp(pt,"-keyboard")==0 && i<argc-1 )
	    AddR(argv[0],"Gdraw.Keyboard", argv[++i]);
	else if ( strcmp(pt,"-display")==0 && i<argc-1 )
	    display = argv[++i];
# if MyMemory
	else if ( strcmp(pt,"-memory")==0 )
	    __malloc_debug(5);
# endif
	else if ( strcmp(pt,"-nosplash")==0 )
	    splash = 0;
	else if ( strcmp(pt,"-recover")==0 && i<argc-1 ) {
	    ++i;
	    if ( strcmp(argv[i],"none")==0 )
		recover=0;
	    else if ( strcmp(argv[i],"clean")==0 )
		recover= -1;
	    else if ( strcmp(argv[i],"auto")==0 )
		recover= 1;
	    else if ( strcmp(argv[i],"inquire")==0 )
		recover= 2;
	    else {
		fprintf( stderr, "Invalid argument to -recover, must be none, auto, inquire or clean\n" );
		dousage();
	    }
	} else if ( strcmp(pt,"-recover=none")==0 ) {
	    recover = 0;
	} else if ( strcmp(pt,"-recover=clean")==0 ) {
	    recover = -1;
	} else if ( strcmp(pt,"-recover=auto")==0 ) {
	    recover = 1;
	} else if ( strcmp(pt,"-recover=inquire")==0 ) {
	    recover = 2;
	} else if ( strcmp(pt,"-help")==0 )
	    dohelp();
	else if ( strcmp(pt,"-usage")==0 )
	    dousage();
	else if ( strcmp(pt,"-version")==0 )
	    doversion(source_version_str);
	else if ( strcmp(pt,"-library-status")==0 )
	    dolibrary();
    }

    GDrawCreateDisplays(display,argv[0]);
    default_background = GDrawGetDefaultBackground(screen_display);
    InitToolIconClut(default_background);
    InitCursors();
#ifndef _NO_PYTHON
    PyFF_ProcessInitFiles();
#endif

    /* Wait until the UI has started, otherwise people who don't have consoles*/
    /*  open won't get our error messages, and it's an important one */
    /* Scripting doesn't care about a mismatch, because scripting interpretation */
    /*  all lives in the library */
    check_library_version(&exe_library_version_configuration,true,false);

    /* the splash screen used not to have a title bar (wam_nodecor) */
    /*  but I found I needed to know how much the window manager moved */
    /*  the window around, which I can determine if I have a positioned */
    /*  decorated window created at the begining */
    /* Actually I don't care any more */
    wattrs.mask = wam_events|wam_cursor|wam_bordwidth|wam_backcol|wam_positioned|wam_utf8_wtitle|wam_isdlg;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.positioned = 1;
    wattrs.cursor = ct_pointer;
    wattrs.utf8_window_title = "FontForge";
    wattrs.border_width = 2;
    wattrs.background_color = 0xffffff;
    wattrs.is_dlg = true;
    pos.x = pos.y = 200;
    pos.width = splashimage.u.image->width;
    pos.height = splashimage.u.image->height-56;		/* 54 */
    splashw = GDrawCreateTopWindow(NULL,&pos,splash_e_h,NULL,&wattrs);
	memset(&rq,0,sizeof(rq));
	rq.family_name = times;
	rq.point_size = 12;
	rq.weight = 400;
	splash_font = GDrawInstanciateFont(NULL,&rq);
	rq.style = fs_italic;
	splash_italic = GDrawInstanciateFont(NULL,&rq);
	GDrawSetFont(splashw,splash_font);
	GDrawFontMetrics(splash_font,&as,&ds,&ld);
	fh = as+ds+ld;
	SplashLayout();
    if ( splash ) {
	GDrawSetVisible(splashw,true);
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
	GDrawProcessPendingEvents(NULL);
	splasht = GDrawRequestTimer(splashw,1000,1000,NULL);
    }
    autosave_timer=GDrawRequestTimer(splashw,60*1000,30*1000,NULL);

    GDrawProcessPendingEvents(NULL);
    GDrawSetBuildCharHooks(BuildCharHook,InsCharHook);

    any = 0;
    if ( recover==-1 )
	CleanAutoRecovery();
    else if ( recover )
	any = DoAutoRecovery(recover-1);

    for ( i=1; i<argc; ++i ) {
	char buffer[1025];
	char *pt = argv[i];

	GDrawProcessPendingEvents(NULL);
	if ( pt[0]=='-' && pt[1]=='-' )
	    ++pt;
	if ( strcmp(pt,"-new")==0 ) {
	    FontNew();
	    any = 1;
#  if HANYANG
	} else if ( strcmp(pt,"-newkorean")==0 ) {
	    MenuNewComposition(NULL,NULL,NULL);
	    any = 1;
#  endif
	} else if ( strcmp(pt,"-last")==0 ) {
	    if ( next_recent<RECENT_MAX && RecentFiles[next_recent]!=NULL )
		if ( ViewPostscriptFont(RecentFiles[next_recent++]))
		    any = 1;
	} else if ( strcmp(pt,"-sync")==0 || strcmp(pt,"-memory")==0 ||
		strcmp(pt,"-nosplash")==0 || strcmp(pt,"-recover=none")==0 ||
		strcmp(pt,"-recover=clean")==0 || strcmp(pt,"-recover=auto")==0 ||
		strcmp(pt,"-dontopenxdevices")==0 )
	    /* Already done, needed to be before display opened */;
	else if ( (strcmp(pt,"-depth")==0 || strcmp(pt,"-vc")==0 ||
		    strcmp(pt,"-cmap")==0 || strcmp(pt,"-colormap")==0 || 
		    strcmp(pt,"-keyboard")==0 || 
		    strcmp(pt,"-display")==0 || strcmp(pt,"-recover")==0 ) &&
		i<argc-1 )
	    ++i; /* Already done, needed to be before display opened */
	else {
	    GFileGetAbsoluteName(argv[i],buffer,sizeof(buffer));
	    if ( GFileIsDir(buffer)) {
		char *fname;
		fname = galloc(strlen(buffer)+strlen("/glyphs/contents.plist")+1);
		strcpy(fname,buffer); strcat(fname,"/glyphs/contents.plist");
		if ( GFileExists(fname)) {
		    /* It's probably a Unified Font Object directory */
		    free(fname);
		    if ( ViewPostscriptFont(buffer) )
			any = 1;
		} else {
		    strcpy(fname,buffer); strcat(fname,"/font.props");
		    if ( GFileExists(fname)) {
			/* It's probably a sf dir collection */
			free(fname);
			if ( ViewPostscriptFont(buffer) )
			    any = 1;
		    } else {
			free(fname);
			if ( buffer[strlen(buffer)-1]!='/' ) {
			    /* If dirname doesn't end in "/" we'll be looking in parent dir */
			    buffer[strlen(buffer)+1]='\0';
			    buffer[strlen(buffer)] = '/';
			}
			fname = GetPostscriptFontName(buffer,false);
			if ( fname!=NULL )
			    ViewPostscriptFont(fname);
			any = 1;	/* Even if we didn't get a font, don't bring up dlg again */
			free(fname);
		    }
		}
	    } else if ( ViewPostscriptFont(buffer)!=0 )
		any = 1;
	}
    }
    if ( !any )
	MenuOpen(NULL,NULL,NULL);
    GDrawEventLoop(NULL);
return( 0 );
}
