AC_DEFUN([SXE_CHECK_ONION], [dnl
	### GRRRRR this is fucking broken
	dnl CMAKE_FIND_PACKAGE([onion], [C], [GNU])

	AC_ARG_VAR([onion_CFLAGS], [C compiler flags for onion.])dnl
	AC_ARG_VAR([onion_LIBS], [linker flags for onion.])dnl

	save_CPPFLAGS="${CPPFLAGS}"
	CPPFLAGS="${onion_CFLAGS} ${CPPFLAGS}"
	AC_CHECK_HEADERS([onion/onion.h])
	CPPFLAGS="${save_CPPFLAGS}"

	save_LDFLAGS="${LDFLAGS}"
	LDFLAGS="${onion_LIBS} ${LDFLAGS}"
	AC_CHECK_LIB([onion], [onion_listen])
	AC_CHECK_LIB([onion_static], [onion_listen])
	LDFLAGS="${save_LDFLAGS}"

	AC_MSG_CHECKING([for onion])

	sxe_cv_feat_onion="no"
	if test "${ac_cv_header_onion_onion_h}" != "yes"; then
		:
	elif test "${ac_cv_lib_onion_onion_listen}" != "yes"; then
		:
	elif test "${ac_cv_lib_onion_static_onion_listen}" != "yes"; then
		:
	else
		## finally
		sxe_cv_feat_onion="yes"
	fi

	AC_MSG_RESULT([${sxe_cv_feat_onion}])
])dnl SXE_CHECK_ONION

dnl sxe-onion.m4 ends here
