ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = autogen.sh

SUBDIRS = wocky tools m4 examples tests docs

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

dist-hook:
	chmod u+w ${distdir}/ChangeLog
	if test -d ${top_srcdir}/.git; then \
		git log --stat > ${distdir}/ChangeLog || \
		git log > ${distdir}/ChangeLog; \
	fi

include $(top_srcdir)/rules/lcov.mak

CLEANFILES = FIXME.out

check-local::
	egrep -A 5 '[F]IXME|[T]ODO|[X]XX' \
		$(srcdir)/wocky/*.[ch] \
		$(srcdir)/tests/*.[ch] \
		$(srcdir)/examples/*.[ch] \
		> FIXME.out || true

# one day we'll have a wocky.freedesktop.org but today is not that day
maintainer-upload-docs:
	rsync -rvzPp --chmod=Dg+s,ug+rwX,o=rX $(srcdir)/docs/reference/html/ \
	telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/wocky/
