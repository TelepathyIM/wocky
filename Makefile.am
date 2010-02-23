ACLOCAL_AMFLAGS = -I m4

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
