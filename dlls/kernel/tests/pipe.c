/*
 * Unit tests for named pipe functions in Wine
 *
 * Copyright (c) 2002 Dan Kegel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifndef STANDALONE
#include "wine/test.h"
#else
#include <assert.h>
#define START_TEST(name) main(int argc, char **argv)
#define ok(condition, msg) assert(condition)
#define todo_wine
#endif

#include <wtypes.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>

#define PIPENAME "\\\\.\\PiPe\\tests_" __FILE__

void test_CreateNamedPipeA(void)
{
    HANDLE hnp;
    HANDLE hFile;
    const char obuf[] = "Bit Bucket";
    char ibuf[32];
    DWORD written;
    DWORD gelesen;

    /* Bad parameter checks */
    hnp = CreateNamedPipeA("not a named pipe", 
        PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE|PIPE_WAIT, 
        /* nMaxInstances */ 1,
        /* nOutBufSize */ 1024,
        /* nInBufSize */ 1024,
        /* nDefaultWait */ NMPWAIT_USE_DEFAULT_WAIT,
        /* lpSecurityAttrib */ NULL);

    if (hnp == INVALID_HANDLE_VALUE && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        /* Is this the right way to notify user of skipped tests? */
        ok(hnp == INVALID_HANDLE_VALUE && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED, 
	    "CreateNamedPipe not supported on this platform, skipping tests.");
        return;
    }
    ok(hnp == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_NAME,
        "CreateNamedPipe should fail if name doesn't start with \\\\.\\pipe");

    hnp = CreateNamedPipeA(NULL, 
        PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE|PIPE_WAIT, 
        1, 1024, 1024, NMPWAIT_USE_DEFAULT_WAIT, NULL);
    ok(hnp == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND,
        "CreateNamedPipe should fail if name is NULL");

    hFile = CreateFileA(PIPENAME, GENERIC_READ|GENERIC_WRITE, 0, 
	NULL, OPEN_EXISTING, 0, 0);
    ok(hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "connecting to nonexistent named pipe should fail with ERROR_FILE_NOT_FOUND");

    /* Functional checks */

    hnp = CreateNamedPipeA(PIPENAME,
        PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE|PIPE_WAIT, 
        /* nMaxInstances */ 1,
        /* nOutBufSize */ 1024,
        /* nInBufSize */ 1024,
        /* nDefaultWait */ NMPWAIT_USE_DEFAULT_WAIT,
        /* lpSecurityAttrib */ NULL);
    ok(hnp != INVALID_HANDLE_VALUE, "CreateNamedPipe failed");

    hFile = CreateFileA(PIPENAME, GENERIC_READ|GENERIC_WRITE, 0, 
            NULL, OPEN_EXISTING, 0, 0);
    todo_wine
    {
        ok(hFile != INVALID_HANDLE_VALUE, "CreateFile failed");
    }

    /* don't try to do i/o if one side couldn't be opened, as it hangs */
    if (hFile != INVALID_HANDLE_VALUE) {
	HANDLE hFile2;

	/* Make sure we can read and write a few bytes in both directions*/
	memset(ibuf, 0, sizeof(ibuf));
	ok(WriteFile(hnp, obuf, sizeof(obuf), &written, NULL), "WriteFile");
	ok(written == sizeof(obuf), "write file len");
	ok(ReadFile(hFile, ibuf, sizeof(obuf), &gelesen, NULL), "ReadFile");
	ok(gelesen == sizeof(obuf), "read file len");
	ok(memcmp(obuf, ibuf, written) == 0, "content check");

	memset(ibuf, 0, sizeof(ibuf));
	ok(WriteFile(hFile, obuf, sizeof(obuf), &written, NULL), "WriteFile");
	ok(written == sizeof(obuf), "write file len");
	ok(ReadFile(hnp, ibuf, sizeof(obuf), &gelesen, NULL), "ReadFile");
	ok(gelesen == sizeof(obuf), "read file len");
	ok(memcmp(obuf, ibuf, written) == 0, "content check");

	/* Picky conformance tests */

	/* Verify that you can't connect to pipe again
	 * until server calls DisconnectNamedPipe+ConnectNamedPipe
	 * or creates a new pipe
	 * case 1: other client not yet closed
	 */
	hFile2 = CreateFileA(PIPENAME, GENERIC_READ|GENERIC_WRITE, 0, 
	    NULL, OPEN_EXISTING, 0, 0);
	ok(hFile2 == INVALID_HANDLE_VALUE, "connecting to named pipe after other client closes but before DisconnectNamedPipe should fail");
	ok(GetLastError() == ERROR_PIPE_BUSY, "connecting to named pipe before other client closes should fail with ERROR_PIPE_BUSY");

	ok(CloseHandle(hFile), "CloseHandle");

	/* case 2: other client already closed */
	hFile = CreateFileA(PIPENAME, GENERIC_READ|GENERIC_WRITE, 0, 
	    NULL, OPEN_EXISTING, 0, 0);
	ok(hFile == INVALID_HANDLE_VALUE, "connecting to named pipe after other client closes but before DisconnectNamedPipe should fail");
	ok(GetLastError() == ERROR_PIPE_BUSY, "connecting to named pipe after other client closes but before DisconnectNamedPipe should fail with ERROR_PIPE_BUSY");

	ok(DisconnectNamedPipe(hnp), "DisconnectNamedPipe");

	/* case 3: server has called DisconnectNamedPipe but not ConnectNamed Pipe */
	hFile = CreateFileA(PIPENAME, GENERIC_READ|GENERIC_WRITE, 0, 
	    NULL, OPEN_EXISTING, 0, 0);
	ok(hFile == INVALID_HANDLE_VALUE, "connecting to named pipe after other client closes but before DisconnectNamedPipe should fail");
	ok(GetLastError() == ERROR_PIPE_BUSY, "connecting to named pipe after other client closes but before ConnectNamedPipe should fail with ERROR_PIPE_BUSY");

	/* to be complete, we'd call ConnectNamedPipe here and loop,
	 * but by default that's blocking, so we'd either have
	 * to turn on the uncommon nonblocking mode, or
	 * use another thread.
	 */
    }

    ok(CloseHandle(hnp), "CloseHandle");
}

START_TEST(pipe)
{
    test_CreateNamedPipeA();
}
