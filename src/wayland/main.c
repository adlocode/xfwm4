/* Copyright (c) 2018 - 2023 adlo
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "server.h"

bool
show_help(char *name)
{
	printf(_("Syntax: %s [options]\n"), name);
	printf(_("\nOptions:\n"));
	printf(_("  --help              Display this help and exit\n"));
	printf(_("  --version           Display the version and exit\n"));
	printf(_("  --startup CMD       Run CMD after starting\n"));
	printf(_("  --debug             Display debugging output\n"));

	return true;
}

xfwmServer server = {0};

void
signal_handler(int sig)
{
	switch (sig) {
		case SIGINT:
		case SIGTERM:
			wl_display_terminate(server.wl_display);
			break;
	}
}

int
main (int argc, char **argv)
{
#ifdef USE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);
#endif

    char *startup_cmd = NULL;
    enum wlr_log_importance debuglevel = WLR_ERROR;
    if (argc > 1) {
      int i;
      for (i = 0; i < argc; i++) {
      if (strcmp("--debug", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
          debuglevel = WLR_INFO;
      } else if (strcmp("--startup", argv[i]) == 0 || strcmp("-s", argv[i]) == 0) {
          if (i < argc - 1) {
          startup_cmd = argv[i + 1];
          } else {
          fprintf(stderr, _("%s requires an argument\n"), argv[i]);
          }
        } else if (strcmp("--version", argv[i]) == 0 || strcmp("-V", argv[i]) == 0) {
            printf(PACKAGE_NAME " " PACKAGE_VERSION "\n");
            return 0;
          } else if (strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0) {
          show_help(argv[0]);
          return 0;
      } else if (argv[i][0] == '-') {
        show_help(argv[0]);
        return 1;
      }
    }
  }

    wlr_log_init(debuglevel, NULL);

  if (serverInit(&server))
  {
    wlr_log(WLR_INFO, "%s", _("Successfully started server"));
  }
  else
  {
    wlr_log(WLR_ERROR, "%s", _("Failed to start server"));
    terminate(&server);
    exit(EXIT_FAILURE);
  }

  if (startup_cmd) {
    if (fork() == 0) {
      execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (char *) NULL);
    }
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = signal_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);

  wl_display_run(server.wl_display);

  terminate (&server);

  return 0;
}
