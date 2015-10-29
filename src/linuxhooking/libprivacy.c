/* libprivacy.c:    block network connections
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#define _GNU_SOURCE
#include <dlfcn.h>        /* dlsym */
#include <errno.h>        /* errno*/
#include <sys/types.h>    /* socket */
#include <sys/socket.h>   /* socket */

/* local includes */
#include <lib/types.h>


/* int socket(int domain, int type, int protocol); */
/* int socketpair(int domain, int type, int protocol, int sv[2]); */

int socket (int domain, int type, int protocol)
{
	int sfd = -1;
	static int (*orig_socket)(int domain, int type, int protocol) = NULL;

	if (domain == AF_LOCAL)
		goto allow;

	errno = EACCES;
out:
	return sfd;
allow:
	if (!orig_socket)
		*(void **) (&orig_socket) = dlsym(RTLD_NEXT, "socket");
	sfd = orig_socket(domain, type, protocol);
	goto out;
}

int socketpair (int domain, int type, int protocol, int sv[2])
{
	int ret = -1;
	static int (*orig_socketpair)(int domain, int type,
		int protocol, int sv[2]) = NULL;

	if (domain == AF_LOCAL)
		goto allow;

	errno = EOPNOTSUPP;
out:
	return ret;
allow:
	if (!orig_socketpair)
		*(void **) (&orig_socketpair) =	dlsym(RTLD_NEXT, "socketpair");
	ret = orig_socketpair(domain, type, protocol, sv);
	goto out;
}
