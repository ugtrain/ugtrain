/* libprivacy.c:    block network connections
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 2, or any later version
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
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "libcommon.h"

#define PFX "[privacy] "
#define HOOK_SOCKET 1
#define HOOK_SOCKETPAIR 1


/* ask libc for our process name as 'argv[0]' and 'getenv("_")'
   don't work here */
extern char *__progname;

/* from /usr/include/bits/socket.h */
static const char *domain_strings[] = { "AF_UNSPEC", "AF_LOCAL", "AF_INET",
"AF_AX25", "AF_IPX", "AF_APPLETALK", "AF_NETROM", "AF_BRIDGE",
"AF_ATMPVC", "AF_X25", "AF_INET6", "AF_ROSE", "AF_DECnet", "AF_NETBEUI",
"AF_SECURITY", "AF_KEY", "AF_NETLINK", "AF_PACKET", "AF_ASH", "AF_ECONET",
"AF_ATMSVC", "AF_RDS", "AF_SNA", "AF_IRDA", "AF_PPPOX", "AF_WANPIPE",
"AF_LLC", "AF_UNKNOWN", "AF_UNKNOWN", "AF_CAN", "AF_TIPC", "AF_BLUETOOTH",
"AF_IUCV", "AF_RXRPC", "AF_ISDN", "AF_PHONET", "AF_IEEE802154", "AF_CAIF",
"AF_ALG", "AF_NFC", "AF_MAX" };
static const char *type_strings[] = { "SOCK_UNKNOWN", "SOCK_STREAM",
"SOCK_DGRAM", "SOCK_RAW", "SOCK_RDM", "SOCK_SEQPACKET", "SOCK_DCCP",
"SOCK_UNKNOWN", "SOCK_UNKNOWN", "SOCK_UNKNOWN", "SOCK_PACKET" };


/* int socket(int domain, int type, int protocol); */
/* int socketpair(int domain, int type, int protocol, int sv[2]); */

/* prepare memory discovery upon library load */
void __attribute ((constructor)) privacy_init (void)
{
	int num_domains = sizeof(domain_strings) / sizeof(const char *);
	int num_types = sizeof(type_strings) / sizeof(const char *);

	pr_out("Knowing %d AF_* and %d SOCK_*.\n", num_domains, num_types);
}

static inline void process_socket_blocking (int domain, int type, int protocol)
{
	int num_domains = sizeof(domain_strings) / sizeof(const char *);
	int num_types = sizeof(type_strings) / sizeof(const char *);
	int type_id = type & 0xff;
	const char *domain_str = (domain >= num_domains) ? "AF_UNKNOWN" :
		domain_strings[domain];
	const char *type_str = (type >= num_types) ? "SOCK_UNKNOWN" :
		type_strings[type_id];
	struct protoent *proto = getprotobynumber(protocol);
	char *protocol_str = (!proto) ? (char *) "unknown" : proto->p_name;

	pr_out("Blocking %s,%s,%s socket for \"%s\"!\n", domain_str,
		type_str, protocol_str, __progname);
}

#ifdef HOOK_SOCKET
int socket (int domain, int type, int protocol)
{
	int sfd = -1;
	static int (*orig_socket)(int domain, int type, int protocol) = NULL;

	if (domain != AF_LOCAL) {
		process_socket_blocking(domain, type, protocol);
		errno = EACCES;
	} else {
		if (!orig_socket)
			*(void **) (&orig_socket) = dlsym(RTLD_NEXT, "socket");
		sfd = orig_socket(domain, type, protocol);
	}
	return sfd;
}
#endif

#ifdef HOOK_SOCKETPAIR
int socketpair(int domain, int type, int protocol, int sv[2])
{
	int ret = -1;
	static int (*orig_socketpair)(int domain, int type,
		int protocol, int sv[2]) = NULL;

	if (domain != AF_LOCAL) {
		process_socket_blocking(domain, type, protocol);
		errno = EOPNOTSUPP;
	} else {
		if (!orig_socketpair)
			*(void **) (&orig_socketpair) =
				dlsym(RTLD_NEXT, "socketpair");
		ret = orig_socketpair(domain, type, protocol, sv);
	}
	return ret;
}
#endif
