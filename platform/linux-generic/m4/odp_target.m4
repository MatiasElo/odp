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

AS_CASE([$ODP_TARGET],
	[default], [],
	[neoverse-n2], [],
	[neoverse-n3], [],
	[neoverse-v1], [],
	[neoverse-v2], [],
	[neoverse-v3], [],
	[AC_MSG_ERROR([unsupported --with-target value '$ODP_TARGET'. Supported values: ]_ODP_TARGET_LIST)]
)

])
