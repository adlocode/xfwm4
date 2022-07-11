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
#include "hopalong-server.h"
#include "hopalong-environment.h"
#include <sys/socket.h>
#include <sys/wait.h>
#include "os-compatibility.h"
#include "util/helpers.h"

static struct wl_list child_process_list;

struct weston_process;
typedef void (*weston_process_cleanup_func_t)(struct weston_process *process,
					    int status);

struct weston_process {
	pid_t pid;
	weston_process_cleanup_func_t cleanup;
	struct wl_list link;
};

struct process_info {
	struct weston_process proc;
	char *path;
};

static inline void *
zalloc(size_t size)
{
	return calloc(1, size);
}


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

static const struct xfwm_shell_interface xfwm_desktop_shell_implementation =
{
  .window_focus = xfwm_shell_send_focus_signal,
  .window_raise = xfwm_shell_send_raise_signal,
  .set_tabwin = xfwm_shell_handle_set_tabwin,
};

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	struct hopalong_server *shell = wl_resource_get_user_data(resource);

	shell->child.desktop_shell = NULL;
}

static void
bind_desktop_shell(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct hopalong_server *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &xfwm_shell_interface,
				      1, id);

  wlr_log (WLR_INFO, "\nbind desktop shell\n");

	if (client == shell->child.client) {
		wl_resource_set_implementation(resource,
					       &xfwm_desktop_shell_implementation,
					       shell, unbind_desktop_shell);
		shell->child.desktop_shell = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
}

WL_EXPORT size_t
weston_module_path_from_env(const char *name, char *path, size_t path_len)
{
	const char *mapping = getenv("WESTON_MODULE_MAP");
	const char *end;
	const int name_len = strlen(name);

	if (!mapping)
		return 0;

	end = mapping + strlen(mapping);
	while (mapping < end && *mapping) {
		const char *filename, *next;

		/* early out: impossibly short string */
		if (end - mapping < name_len + 1)
			return 0;

		filename = &mapping[name_len + 1];
		next = strchrnul(mapping, ';');

		if (strncmp(mapping, name, name_len) == 0 &&
		    mapping[name_len] == '=') {
			size_t file_len = next - filename; /* no trailing NUL */
			if (file_len >= path_len)
				return 0;
			strncpy(path, filename, file_len);
			path[file_len] = '\0';
			return file_len;
		}

		mapping = next + 1;
	}

	return 0;
}

WL_EXPORT char *
wet_get_binary_path(const char *name)
{
	char path[PATH_MAX];
	size_t len;
  
  wlr_log (WLR_INFO, "get binary path");
  wlr_log (WLR_INFO, BINDIR);

	len = weston_module_path_from_env(name, path, sizeof path);
	if (len > 0)
		return strdup(path);

	len = snprintf(path, sizeof path, "%s/%s", BINDIR, name);
	if (len >= sizeof path)
		return NULL;
  
  wlr_log (WLR_INFO, "exit get binary path");

	return strdup(path);
}

static void
child_client_exec(int sockfd, const char *path)
{
	int clientfd;
	char s[32];
	sigset_t allsigs;

	/* do not give our signal mask to the new process */
	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

	/* Launch clients as the user. Do not lauch clients with wrong euid.*/
	if (seteuid(getuid()) == -1) {
		wlr_log(WLR_ERROR, "compositor: failed seteuid\n");
		return;
	}

	/* SOCK_CLOEXEC closes both ends, so we dup the fd to get a
	 * non-CLOEXEC fd to pass through exec. */
	clientfd = dup(sockfd);
	if (clientfd == -1) {
		wlr_log(WLR_ERROR, "compositor: dup failed: %m\n");
		return;
	}

	snprintf(s, sizeof s, "%d", clientfd);
	setenv("WAYLAND_SOCKET", s, 1);

	if (execl(path, path, NULL) < 0)
		wlr_log(WLR_ERROR, "compositor: executing '%s' failed: %m\n",
			path);
}

static void
process_handle_sigchld(struct weston_process *process, int status)
{
	struct process_info *pinfo =
		container_of(process, struct process_info, proc);

	/*
	 * There are no guarantees whether this runs before or after
	 * the wl_client destructor.
	 */

	if (WIFEXITED(status)) {
		wlr_log (WLR_INFO, "%s exited with status %d\n", pinfo->path,
			   WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		wlr_log (WLR_INFO, "%s died on signal %d\n", pinfo->path,
			   WTERMSIG(status));
	} else {
		wlr_log (WLR_INFO, "%s disappeared\n", pinfo->path);
	}

	free(pinfo->path);
	free(pinfo);
}

WL_EXPORT void
weston_watch_process(struct weston_process *process)
{
	wl_list_insert(&child_process_list, &process->link);
}

WL_EXPORT struct wl_client *
weston_client_launch(struct hopalong_server *server,
		     struct weston_process *proc,
		     const char *path,
		     weston_process_cleanup_func_t cleanup)
{
	int sv[2];
	pid_t pid;
	struct wl_client *client;

	wlr_log (WLR_INFO, "launching '%s'\n", path);

	if (os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"socketpair failed while launching '%s': %m\n",
			path);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"fork failed while launching '%s': %m\n", path);
		return NULL;
	}

	if (pid == 0) {
		child_client_exec(sv[1], path);
		_exit(-1);
	}

	close(sv[1]);

	client = wl_client_create(server->display, sv[0]);
	if (!client) {
		close(sv[0]);
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"wl_client_create failed while launching '%s'.\n",
			path);
		return NULL;
	}

	proc->pid = pid;
	proc->cleanup = cleanup;
	weston_watch_process(proc);

	return client;
}

WL_EXPORT struct wl_client *
weston_client_start(struct hopalong_server *server, const char *path)
{
	struct process_info *pinfo;
	struct wl_client *client;

	pinfo = zalloc(sizeof *pinfo);
	if (!pinfo)
		return NULL;

	pinfo->path = strdup(path);
	if (!pinfo->path)
		goto out_free;

	client = weston_client_launch(server, &pinfo->proc, path,
				      process_handle_sigchld);
	if (!client)
		goto out_str;

	return client;

out_str:
	free(pinfo->path);

out_free:
	free(pinfo);

	return NULL;
}

static void
desktop_shell_client_destroy(struct wl_listener *listener, void *data)
{
	struct hopalong_server *shell;

	shell = container_of(listener, struct hopalong_server,
			     child.client_destroy_listener);

	wl_list_remove(&shell->child.client_destroy_listener.link);
	shell->child.client = NULL;
	/*
	 * unbind_desktop_shell() will reset shell->child.desktop_shell
	 * before the respawned process has a chance to create a new
	 * desktop_shell object, because we are being called from the
	 * wl_client destructor which destroys all wl_resources before
	 * returning.
	 */

	//if (!check_desktop_shell_crash_too_early(shell))
		//respawn_desktop_shell_process(shell);

	//shell_fade_startup(shell);
}

static void
launch_desktop_shell_process(void *data)
{
	struct hopalong_server *server = data;
  char *client;  
  
  wlr_log (WLR_INFO, "\nlaunch desktop shell\n");

  client = wet_get_binary_path ("xfwm4-wayland-shell");

  server->child.client = weston_client_start (server, client);

	if (!server->child.client) {
		wlr_log(WLR_ERROR, "not able to start client");
		return;
	}

	server->child.client_destroy_listener.notify =
		desktop_shell_client_destroy;
	wl_client_add_destroy_listener(server->child.client,
				       &server->child.client_destroy_listener);
}

void xfwm_server_shell_init (struct hopalong_server *server, int argc, char *argv[])
{
  int ret;
  struct weston_client *client;
  struct wl_event_loop *loop;  

  wl_global_create (server->display,
                    &xfwm_shell_interface, 1,
                    server, bind_desktop_shell);
  
  wlr_log (WLR_INFO, "\ninit shell\n");

  loop = wl_display_get_event_loop(server->display);
	wl_event_loop_add_idle(loop, launch_desktop_shell_process, server);   
  
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
  
  wl_list_init(&child_process_list);
  
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
