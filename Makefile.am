@SET_MAKE@

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =	\
	panel-plugin \
	po \
	icons

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles =                                          \
        find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST = \
	meson.build \
	po/meson.build \
	xfce-revision.h.in \
	$(NULL)
