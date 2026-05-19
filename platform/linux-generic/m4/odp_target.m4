# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 Nokia
#

m4_define([_ODP_TARGET_LIST], [[default, neoverse-n2, neoverse-n3, neoverse-v1, neoverse-v2, neoverse-v3]])

# ODP_TARGET_OPTIONS
# ------------------
# Configure target-specific options
AC_DEFUN([ODP_TARGET_OPTIONS],
[
AC_ARG_WITH([target],
	    [AS_HELP_STRING([--with-target=TARGET],
		    [select target system [default=default]. Supported values: ]_ODP_TARGET_LIST)],
	    [ODP_TARGET=$with_target],
	    [ODP_TARGET=default
	     with_target=default])

feat_ecv=no
time_freq_1ghz=no

AS_CASE([$ODP_TARGET],
	[default], [],
	[neoverse-n2], [time_freq_1ghz=yes],
	[neoverse-n3], [feat_ecv=yes
			time_freq_1ghz=yes],
	[neoverse-v1], [],
	[neoverse-v2], [time_freq_1ghz=yes],
	[neoverse-v3], [feat_ecv=yes
			time_freq_1ghz=yes],
	[AC_MSG_ERROR([unsupported --with-target value '$ODP_TARGET'. Supported values: ]_ODP_TARGET_LIST)]
)

if test "x$feat_ecv" = "xyes"; then
	AC_DEFINE([_ODP_FEAT_ECV], [1],
		  [Define to 1 when FEAT_ECV is available])
fi
if test "x$time_freq_1ghz" = "xyes"; then
	AC_DEFINE([_ODP_TIME_FREQ_1GHZ], [1],
		  [Define to 1 when target has 1 GHz time counter frequency])
fi

])
