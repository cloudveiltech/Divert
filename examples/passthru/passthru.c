/*
 * passthru.c
 * (C) 2019, all rights reserved,
 *
 * This file is part of WinDivert.
 *
 * WinDivert is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WinDivert is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * DESCRIPTION:
 * This program does nothing except divert packets and re-inject them.  This is
 * useful for performance testing.
 *
 * usage: passthru.exe [windivert-filter] [num-threads] [batch-size] [priority]
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "windivert.h"

#define MTU 1500

typedef struct
{
    HANDLE handle;
    int batch;
} CONFIG, *PCONFIG;

static DWORD passthru(LPVOID arg);

/*
 * Entry.
 */
int __cdecl main(int argc, char **argv)
{
    const char *filter = "outbound and (tcp.DstPort == 80 or tcp.DstPort == 443 or tcp.DstPort == 8080 or tcp.DstPort == 8443 or udp.DstPort == 80 or udp.DstPort == 443)";
    int threads = 1, batch = 1, priority = 0;
    int i;
    HANDLE handle, thread;
    CONFIG config;

    if (argc > 5)
    {
        fprintf(stderr, "usage: %s [filter] [num-threads] [batch-size] "
            "[priority]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc >= 2)
    {
        filter = argv[1];
    }
    if (argc >= 3)
    {
        threads = atoi(argv[2]);
        if (threads < 1 || threads > 64)
        {
            fprintf(stderr, "error: invalid number of threads\n");
            exit(EXIT_FAILURE);
        }
    }
    if (argc >= 4)
    {
        batch = atoi(argv[3]);
        if (batch <= 0 || batch > WINDIVERT_BATCH_MAX)
        {
            fprintf(stderr, "error: invalid batch size\n");
            exit(EXIT_FAILURE);
        }
    }
    if (argc >= 5)
    {
        priority = atoi(argv[4]);
        if (priority < WINDIVERT_PRIORITY_LOWEST ||
            priority > WINDIVERT_PRIORITY_HIGHEST)
        {
            fprintf(stderr, "error: invalid priority value\n");
            exit(EXIT_FAILURE);
        }
    }

    // Divert traffic matching the filter:
    handle = WinDivertOpen(filter, WINDIVERT_LAYER_REDIRECT, (INT16)priority,
        0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_INVALID_PARAMETER)
        {
            fprintf(stderr, "error: filter syntax error\n");
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "error: failed to open the WinDivert device (%d)\n",
            GetLastError());
        exit(EXIT_FAILURE);
    }

    WinDivertSetParam(handle, WINDIVERT_PARAM_PROXY_PORT, 14501);
    WinDivertSetParam(handle, WINDIVERT_PARAM_PROXY_PID, 12948);
    WinDivertAddWhitelistedApp(handle, "fiRefox.exe");

    // Start the threads
    config.handle = handle;
    config.batch = batch;
    for (i = 1; i < threads; i++)
    {
        thread = CreateThread(NULL, 1, (LPTHREAD_START_ROUTINE)passthru,
            (LPVOID)&config, 0, NULL);
        if (thread == NULL)
        {
            fprintf(stderr, "error: failed to start passthru thread (%d)\n",
                GetLastError());
            exit(EXIT_FAILURE);
        }
    }

    // Main thread:
    passthru((LPVOID)&config);

    return 0;
}

// Passthru thread.
static DWORD passthru(LPVOID arg)
{
    PCONFIG config = (PCONFIG)arg;
    WINDIVERT_ADDRESS addr;
    HANDLE handle;
    handle = config->handle;


    // Main loop:
    while (TRUE)
    {
        // Read a matching packet.
        Sleep(1);

        fprintf(stderr, "Waiting\n");
        if (!WinDivertRecv(handle, NULL, 0, NULL, &addr))
        {
            fprintf(stderr, "failed to read packet (%d)\n", GetLastError());
            continue;
        }
        
        fprintf(stderr, "Received an event local %d, remote %d\n", addr.Flow.LocalPort, addr.Flow.RemotePort);
    }
}

