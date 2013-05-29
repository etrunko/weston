MAINTAINERCLEANFILES = Makefile.in

EDJE_CC = @EDJE_CC_PATH@
EDJE_FLAGS_VERBOSE_ =
EDJE_FLAGS_VERBOSE_0 =
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_VERBOSE_$(V))

filesdir = $(pkglibexecdir)
files_DATA = iclone.edj

ICLONE_FILES = iclone/iclone.edc \
	       iclone/specialkeys.txt \
	       iclone/images/keyboard-background.png \
	       iclone/images/backspace.png \
	       iclone/images/key-special.png \
	       iclone/images/key-default-selected.png \
	       iclone/images/key-special-selected.png \
	       iclone/images/backspace-selected.png \
	       iclone/images/shift.png \
	       iclone/images/key-default.png \
	       iclone/images/shift-selected.png \
	       iclone/images/background.png

EXTRA_DIST = $(ICLONE_FILES)

iclone.edj: Makefile $(ICLONE_FILES)
	$(EDJE_CC) $(EDJE_FLAGS) \
	    -id $(top_srcdir)/data/vkbd-efl/iclone/images \
	    $(top_srcdir)/data/vkbd-efl/iclone/iclone.edc \
	    $(top_builddir)/data/vkbd-efl/iclone.edj

clean-local:
	rm -f *.edj
