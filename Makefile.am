ACLOCAL_AMFLAGS = -I m4

SUBDIRS = wocky tools m4 examples tests docs

dist-hook:
	chmod u+w ${distdir}/ChangeLog
	if test -d ${top_srcdir}/.git; then \
		git log --stat > ${distdir}/ChangeLog || \
		git log > ${distdir}/ChangeLog; \
	fi

include $(top_srcdir)/rules/lcov.mak

