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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hopalong-environment.h"

bool
hopalong_environment_put(char **env[], const char *str)
{
	/* create an initial envp: {"foo=bar", NULL} */
	if (*env == NULL)
	{
		*env = calloc(2, sizeof (char *));
		if (*env == NULL)
			return false;

		(*env)[0] = strdup(str);
		if ((*env)[0] == NULL)
			return false;

		(*env)[1] = NULL;

		return true;
	}

	size_t nelems;
	for (nelems = 0; (*env)[nelems] != NULL; nelems++)
		;

	/* NULL at end, plus next env var */
	size_t allocelems = nelems + 2;
	*env = realloc(*env, ((allocelems + 2) * sizeof (char *)));
	if (*env == NULL)
		return false;

	(*env)[nelems] = strdup(str);
	if ((*env)[nelems] == NULL)
		return false;

	(*env)[nelems + 1] = NULL;

	return true;
}

bool
hopalong_environment_copy(char **env[], const char **src)
{
	const char **envp;

	for (envp = src; *envp != NULL; envp++)
	{
		if (!hopalong_environment_put(env, *envp))
			return false;
	}

	return true;
}

bool
hopalong_environment_push(char **env[], const char *name, const char *val)
{
	char buf[4096];

	snprintf(buf, sizeof buf, "%s=%s", name, val);

	return hopalong_environment_put(env, buf);
}

void
hopalong_environment_free(char **env[])
{
	size_t nelems;

	for (nelems = 0; (*env)[nelems] != NULL; nelems++)
		free((*env)[nelems]);

	free(*env);
}
