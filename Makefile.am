## Process this file with automake to produce Makefile.in
## aclocal && automake --foreign && autoconf && ./configure

AM_YFLAGS = -d


if ENABLE_NEXPM
bin_PROGRAMS = vtwm nexpm
else
bin_PROGRAMS = vtwm
endif

rcdir = ${datadir}/X11/vtwm
dist_rc_DATA = system.vtwmrc

pre_vtwm_CFLAGS = $(VWM_CFLAGS) -D_BSD_SOURCE -fno-strict-aliasing
pre_vtwm_CFLAGS += -DSYSTEM_VTWMRC=\"${rcdir}/system.vtwmrc\" -DSYSTEM_TWMRC=\"${rcdir}/system.twmrc\"
pre_vtwm_CFLAGS += @SOUND_SUPPORT@ @xpm_CFLAGS@ @regex_CFLAGS@ @xft_CFLAGS@ @xft_AUX_CFLAGS@ @xrandr_CFLAGS@ @xrandr_AUX_CFLAGS@
pre_vtwm_CFLAGS += @TWM_USE_SPACING@ @TWM_USE_OPACITY@ @TWM_USE_SLOPPYFOCUS@ @xinerama_CFLAGS@ @xinerama_AUX_CFLAGS@
pre_vtwm_CFLAGS += @DDEBUG@
pre_vtwm_CFLAGS += @NO_BUILD_INFO@ @NEED_SELECT_H@ @NEED_PROCESS_H@ @NEED_PUTENV_F@ @NO_M4_SUPPORT@ @png_CFLAGS@
if MAINTAINER_MODE
pre_vtwm_CFLAGS += -Wall -Werror -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes
endif

doc_cflags=-DDOC_CFLAGS="\"$(subst \,\\\,$(pre_vtwm_CFLAGS))\""

vtwm_CFLAGS = $(pre_vtwm_CFLAGS) $(doc_cflags)

vtwm_LDADD = $(VTWM_LIBS) $(xpm_LIBS) $(png_LIBS) $(regex_LIBS) $(xft_LIBS) $(xrandr_LIBS) $(xinerama_LIBS)

nexpm_CFLAGS = $(vtwm_CFLAGS) -Icontrib/nexpm
nexpm_LDADD = $(vtwm_LDADD)

vtwm_SOURCES =   \
	add_window.c \
	add_window.h \
	applets.c \
	config.h \
	cursor.c \
	deftwmrc.c \
	desktop.c \
	desktop.h \
	doors.c \
	doors.h \
	events.c \
	events.h \
	gc.c \
	gc.h \
	iconmgr.c \
	iconmgr.h \
	icons.c \
	image_formats.c \
	image_formats.h \
	lastmake.c \
	list.c \
	list.h \
	menus.c \
	menus.h \
	parse.c \
	parse.h \
	regions.c \
	regions.h \
	resize.c \
	resize.h \
	screen.h \
	sound.c \
	sound.h \
	twm.c \
	twm.h \
	util.c \
	util.h \
	version-tmp.c \
	version.h \
	gram.y \
	lex.l

nexpm_SOURCES = contrib/nexpm/nexpm.c

BUILT_SOURCES = gram.h gram.c lex.c deftwmrc.c lastmake.c version-tmp.c
CLEANFILES = gram.h gram.c lex.c deftwmrc.c lastmake.c version-tmp.c
MAINTAINERCLEANFILES =
DISTCLEANFILES=$(BUILT_SOURCES) system.vtwmrc vtwm.man vtwm.1 nexpm
DISTCLEANDIRS=autom4te.cache


deftwmrc.c: system.vtwmrc
	-$(RM) $@
	echo '/* ' >>$@
	echo ' * This file is generated automatically from the default' >>$@
	echo ' * VTWM bindings file system.vtwmrc.'$(SYS_VTWMRC_LOOK)' by the VTWM Makefile.' >>$@
	echo ' */' >>$@
	echo '' >>$@
	echo 'char *defTwmrc[] = {' >>deftwmrc.c
	sed -e '/^$$/d' -e '/^#/d' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/",/' -e 's/[	]\{1,\}/ /g' -e 's/^" /"/' system.vtwmrc >>$@
	echo '(char *)0 };' >>$@

.INTERMEDIATE: version-tmp.c
version-tmp.c: version.c
	if [ -d .git ]; then sed "s/SUBST/`git describe --tags`/" < version.c > version-tmp.c; else cp version.c version-tmp.c; fi

releasetar: maintainer-clean
	@echo "Please use make -f Makefile.release"; exit 1

FORCE:
lastmake.c: FORCE
	-$(RM) $@
	echo '/* ' >>$@
	echo ' * This file is generated automatically by the VTWM Makefile.' >>$@
	echo ' */' >>$@
	echo '' >>$@
	echo 'char *lastmake[] = {' >>lastmake.c
	echo '    "Platform:  '`uname -r -s`'",' >>$@
	echo '    "Build:  '`date`'",' >>$@
	echo '    "" };' >>$@

gram.h: gram.c

#nexpm: contrib/nexpm/nexpm.c
#	$(vtwm_LINK) -o $@ -Icontrib/nexpm $^ $(vtwm_LDADD) $(LIBS)

system.vtwmrc:
	-$(RM) $@
	$(CP) $@.$(SYS_VTWMRC_LOOK) $@

man1_MANS = vtwm.man
EXRTA_DIST = $(man1_MANS)

vtwm.man: doc/vtwm.man
	-$(RM) $@
	$(LN) doc/$@ $@

clean-local:
	-rm -rf .release
