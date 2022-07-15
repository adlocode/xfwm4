/*
 * Hopalong - a friendly Wayland compositor
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Copyright (C) 2018 - 2022 adlo
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <spawn.h>
#include "../../../protocol/xfway-shell-protocol.h"
#include "xfwm-shell.h"
#include "hopalong-server.h"
#include "hopalong-environment.h"
#include <sys/socket.h>
#include <sys/wait.h>
#include "os-compatibility.h"
#include "util/helpers.h"

static void
launch_session_leader(const char **envp, const char *socket, const char *display, char *program)
{
	char **sockenvp = NULL;

	if (!hopalong_environment_copy(&sockenvp, envp))
	{
		wlr_log(WLR_ERROR, "hopalong_environment_copy failed while launching session leader");
		return;
	}

	if (!hopalong_environment_push(&sockenvp, "WAYLAND_DISPLAY", socket))
	{
		wlr_log(WLR_ERROR, "hopalong_environment_push failed while launching session leader");
		return;
	}

	if (display != NULL && !hopalong_environment_push(&sockenvp, "DISPLAY", display))
	{
		wlr_log(WLR_ERROR, "hopalong_environment_push failed while launching session leader");
		return;
	}

	if (!hopalong_environment_push(&sockenvp, "XDG_SESSION_TYPE", "wayland"))
	{
		wlr_log(WLR_ERROR, "hopalong_environment_push failed while launching session leader");
		return;
	}

	char *shellargs[] = { "/bin/sh", "-i", "-c", program, NULL };

	wlr_log(WLR_INFO, "Launching session leader: %s", program);

	pid_t child;
	if (posix_spawn(&child, "/bin/sh", NULL, NULL, shellargs, sockenvp) != 0)
		wlr_log(WLR_ERROR, "Failed to launch session leader (%s): %s", program, strerror(errno));

	wlr_log(WLR_INFO, "Session leader running as PID %u", child);

	hopalong_environment_free(&sockenvp);
}

static void
version(void)
{
	printf("hopalong 0.1\n");
	exit(EXIT_SUCCESS);
}

static void
usage(int code)
{
	printf("usage: hopalong [options] [program]\n"
	       "\n"
	       "If [program] is specified, it will be launched to start a session.\n");
	exit(code);
}

static void xfwm_shell_send_focus_signal (struct wl_client   *client,
                                            struct wl_resource *shell_resource,
                                            struct wl_resource *handle_resource,
                                            struct wl_resource *seat_resource)
{
    
}
  
static void xfwm_shell_send_raise_signal (struct wl_client   *client,
                                            struct wl_resource *shell_resource,
                                            struct wl_resource *handle_resource,
                                            struct wl_resource *seat_resource)
{
    
}


static void xfwm_shell_handle_set_tabwin (struct wl_client   *client,
                                          struct wl_resource *resource,
                                          struct wl_resource *surface_resource,
                                          struct wl_resource *output_resource)
{
  
}

void xfwm_server_shell_init (struct hopalong_server *server, int argc, char *argv[])
{
  int ret;
  struct weston_client *client;
  struct wl_event_loop *loop;
  
  server->shell = xfwm_shell_create (server->display);
  
  wlr_log (WLR_INFO, "\ninit shell\n");

     
  
}

int
main(int argc, char *argv[], const char *envp[])
{
	enum wlr_log_importance log_level = WLR_INFO;

	static struct option long_options[] = {
		{"version",	no_argument, 0, 'V'},
		{"help",	no_argument, 0, 'h'},
		{"debug",	no_argument, 0, 'd'},
		{"style-name",	required_argument, 0, 's'},
		{NULL,		0,	     0, 0 },
	};

	static struct hopalong_server_options opts = {};

	for (;;)
	{
		int c = getopt_long(argc, argv, "Vhds:", long_options, NULL);

		if (c == -1)
			break;

		switch (c)
		{
		case 'V':
			version();
			break;

		case 'h':
			usage(EXIT_SUCCESS);
			break;

		case 'd':
			log_level = WLR_DEBUG;
			break;

		case 's':
			opts.style_name = optarg;
			break;

		default:
			usage(EXIT_FAILURE);
			break;
		}
	}

	wlr_log_init(log_level, NULL);
	struct hopalong_server *server = hopalong_server_new(&opts);

	if (server == NULL)
	{
		fprintf(stderr, "Failed to initialize Hopalong.\n");
		return EXIT_FAILURE;
	}

	const char *socket = hopalong_server_add_socket(server);
	if (socket == NULL)
	{
		fprintf(stderr, "Failed to open socket for Wayland clients.\n");
		return EXIT_FAILURE;
	}

	wlr_log(WLR_INFO, "Listening for Wayland clients at: %s", socket);

	if (optind < argc)
		launch_session_leader(envp, socket, server->wlr_xwayland->display_name, argv[optind]);
	else
	{
		wlr_log(WLR_INFO, "Using $HOME/.hopalong_init for session leader");
		launch_session_leader(envp, socket, server->wlr_xwayland->display_name, "sh ~/.hopalong_init");
	}  
  
  xfwm_server_shell_init (server, argc, argv);

	if (!hopalong_server_run(server))
	{
		hopalong_server_destroy(server);

		fprintf(stderr, "Hopalong server terminating uncleanly.\n");
		return EXIT_FAILURE;
	}

	hopalong_server_destroy(server);
	return EXIT_SUCCESS;
}
