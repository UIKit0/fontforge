CFLAGS=/nowarn/incl=([-.inc],[-.fontforge])/name=(as_is,short)/define=(\
	"_STATIC_LIBFREETYPE=1","_STATIC_LIBPNG=1","HAVE_LIBINTL_H=1",\
	"_STATIC_LIBUNINAMESLIST=1","_STATIC_LIBXML=1","_NO_XINPUT=1",\
	"_STATIC_LIBUNGIF=1","_STATIC_LIBJPEG=1","_STATIC_LIBTIFF=1",\
        "FONTFORGE_CONFIG_DEVICETABLES=1","_NO_LIBSPIRO=1")

gb12345.exe : gb12345.obj [-.fontforge]lff.opt
	@ WRITE_ SYS$OUTPUT "  generating gb12345.opt"
	@ OPEN_/WRITE FILE  gb12345.opt
	@ WRITE_ FILE "!"
	@ WRITE_ FILE "! gb12345.opt generated by DESCRIP.$(MMS_EXT)" 
	@ WRITE_ FILE "!"
	@ WRITE_ FILE "IDENTIFICATION=""gb12345"""
	@ WRITE_ FILE "GSMATCH=LEQUAL,1,0
	@ WRITE_ FILE "gb12345.obj"
	@ CLOSE_ FILE
	@ $(MMS)$(MMSQUALIFIERS)/ignore=warning gb12345_vms
	@ WRITE_ SYS$OUTPUT "  linking gb12345.exe ..."
	@ LINK_/NODEB/SHARE=gb12345.exe/MAP=gb12345.map/FULL gb12345.opt/opt,\
	gb12345_vms.opt/opt,[-.fontforge]lff.opt/opt

gb12345_vms :
	@ WRITE_ SYS$OUTPUT "  generating gb12345.map ..."
	@ LINK_/NODEB/NOSHARE/NOEXE/MAP=gb12345.map/FULL gb12345.opt/OPT
	@ WRITE_ SYS$OUTPUT "  analyzing gb12345.map ..."
	@ @ANALYZE_MAP.COM gb12345.map gb12345_vms.opt

gb12345.obj : gb12345.c
