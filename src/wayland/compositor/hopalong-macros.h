/*
 * Hopalong - a friendly Wayland compositor
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef HOPALONG_COMPOSITOR_MACROS_H
#define HOPALONG_COMPOSITOR_MACROS_H

#define return_if_fail(cond)		\
	if (!(cond)) {			\
		wlr_log(WLR_ERROR, #cond ": Assertion failed."); \
		return;			\
	}

#define return_val_if_fail(cond, val)	\
	if (!(cond)) {			\
		wlr_log(WLR_ERROR, #cond ": Assertion failed."); \
		return (val);		\
	}

#endif
