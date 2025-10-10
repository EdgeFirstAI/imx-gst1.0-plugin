/*
 * Copyright 2025 NXP
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your pconfigion) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include "stdlib.h"
#include <fcntl.h>
#include "remote_input.h"
#include <getopt.h>
#include <string.h>
#include <stdarg.h>

void
usage (char *app)
{
  printf ("%s program.\n", app);
  printf
      ("Usage: %s [-h] [-r remote input type ] [-o server IP address] [-d print debug log]\n",
      app);
  printf
      ("\t-r\t  remote input type: 0->client that sends data, 1->server that receives data\n");
  printf ("\t-o\t  IP address of the server\n");
  printf ("\t-d\t  enable debug log\n");
  printf
      ("\t  \t  client example: /usr/bin/remote_input-1.0 -r 0 -o 192.168.1.1 \n");
  printf
      ("\t  \t  server example: /usr/bin/remote_input-1.0 -r 1 -o 192.168.1.1 \n");
}

int
main (int argc, char **argv)
{
  int rt = 0;
  char host_ip[32];
  RINPUT_TYPE type = RINPUT_NULL;
  void *rinput_handle = NULL;
  RINPUT_RESULT ret = RINPUT_OK;

  printf ("%s\n", REMOTE_INPUT_VERSION_STR);
  if (argc < 3) {
    usage (argv[0]);
    return 0;
  }

  while ((rt = getopt (argc, argv, "r:o:d:h")) >= 0) {
    switch (rt) {
      case 'h':
        usage (argv[0]);
        return 0;
      case 'r':
        type = (RINPUT_TYPE) (atoi (optarg));
        break;
      case 'o':
        if (optarg && strlen (optarg) < 32)
          strcpy (host_ip, optarg);
        break;
      case 'd':
        remote_input_enable_log (atoi (optarg));
        break;
      default:
        usage (argv[0]);
        return 0;
    }
  }

  ret = remote_input_init (&rinput_handle, type);
  if (ret != RINPUT_OK) {
    printf ("Failed to initialize the rinput, ret: %d\n", ret);
    goto done;
  }

reconnect:
  ret = remote_input_connect (rinput_handle, host_ip);
  if (ret != RINPUT_OK) {
    printf ("Failed to connect the target\n");
    goto done;
  }
  printf ("Connected\n");

  while (1) {
    ret = remote_input_dispatch (rinput_handle);

    if (ret != RINPUT_OK) {
      if (type == RINPUT_SERVER && ret == RINPUT_READ_ERROR) {
        printf ("Disconnect from the client, reconnecting\n");
        goto reconnect;
      }

      printf ("Failed to dispatch\n");
      break;
    }
  }

done:
  printf ("Free remote input\n");
  remote_input_free (rinput_handle);
  return 0;
}
