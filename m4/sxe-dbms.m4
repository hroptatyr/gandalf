## database connectors

AC_DEFUN([SXE_CHECK_MYSQL], [dnl
AC_ARG_WITH([mysql], [
	AS_HELP_STRING([--with-mysql], [Which mysql to use])], [], [])

	if test "${with_mysql}" != "no"; then
		AC_PATH_PROG([MYSQL_CONFIG], [mysql_config], [:])
		MYSQL_CPPFLAGS=$(${MYSQL_CONFIG} --include)
		MYSQL_LDFLAGS=$(${MYSQL_CONFIG} --libs)
		AC_SUBST([MYSQL_CPPFLAGS])
		AC_SUBST([MYSQL_LDFLAGS])
		if test "${MYSQL_CONFIG}" != ":"; then
			AC_DEFINE([HAVE_MYSQL], [1],
				[Defined when mysql could be found])
		fi
	
		SXE_DUMP_LIBS
		CPPFLAGS="${CPPFLAGS} ${MYSQL_CPPFLAGS}"
		AC_CHECK_HEADERS([mysql/mysql.h mysql.h])
		SXE_RESTORE_LIBS
	fi

	pushdef([use_mysql_p],
		[test "${with_mysql}" != "no" -a "${MYSQL_CONFIG}" != ":"])
	AM_CONDITIONAL([USE_MYSQL], []use_mysql_p[])

	if []use_mysql_p[]; then
		AC_DEFINE([WITH_MYSQL], [1], [Defined when mysql was desired])
		DBMS="${DBMS} mysql"
	fi

	popdef([use_mysql_p])
])dnl SXE_CHECK_MYSQL

AC_DEFUN([SXE_CHECK_SQLITE], [dnl
	## database stuff ... does not belong here
	AC_ARG_WITH([sqlite], [
	AS_HELP_STRING([--with-sqlite], [Which sqlite to use])], [], [])

	if test "${with_sqlite}" != "no"; then
		PKG_CHECK_MODULES([sqlite], [sqlite3 >= 3.6.0])
	fi

	pushdef([use_sqlite_p],
		[test "${with_sqlite}" != "no" -a -n "${sqlite_LIBS}"])
	AM_CONDITIONAL([USE_SQLITE], []use_sqlite_p[])

	if []use_sqlite_p[]; then
		DBMS="${DBMS} sqlite"
		AC_DEFINE([WITH_SQLITE], [1], [Defined when sqlite was desired])
	fi

	popdef([use_sqlite_p])
])dnl SXE_CHECK_SQLITE

