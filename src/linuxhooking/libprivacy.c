/* libprivacy.c:    block network connections
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
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
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "libcommon.h"
#include "list.h"

#define PFX "[privacy] "
#define OLD_PFX "[privacy] "
#define HOOK_SOCKET 1
#define HOOK_SOCKETPAIR 1
#define HOOK_GETADDRINFO 1
#define HOOK_CONNECT 1
#define HOOK_BIND 1

#define IP_LEN 4

struct node {
	struct list_head list;
	char *name;
};

struct port {
	struct list_head list;
	u16 num;
	u8 *ips;
	size_t ips_len;
};

static struct list_head node_list;
static struct list_head port_list;

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

static bool allow_sockets = false;
static bool allow_bind = false;


/* int socket(int domain, int type, int protocol); */
/* int socketpair(int domain, int type, int protocol, int sv[2]); */
/* int getaddrinfo(const char *node, const char *service,
       const struct addrinfo *hints, struct addrinfo **res); */
/* int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen); */
/* int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen); */


static inline i32 list_add_port (u16 port_num, u8 ip[IP_LEN])
{
	bool found = false;
	struct port *port, *tmp;

	list_for_each_entry_safe (port, tmp, &port_list, list) {
		if (port->num == port_num) {
			port->ips = (u8 *) realloc(port->ips,
				port->ips_len + IP_LEN);
			if (!port->ips) {
				list_del(&port->list);
				free(port);
				goto err;
			}
			memcpy(port->ips + port->ips_len, ip, IP_LEN);
			port->ips_len += IP_LEN;
			found = true;
			break;
		}
	}
	if (!found) {
		port = calloc(1, sizeof(*port));
		if (!port)
			goto err;
		port->num = port_num;
		port->ips = (u8 *) malloc(IP_LEN);
		if (!port->ips) {
			free(port);
			goto err;
		}
		memcpy(port->ips, ip, IP_LEN);
		port->ips_len = IP_LEN;
		list_add_tail(&port->list, &port_list);
	}
	return 0;
err:
	return -1;
}

static inline void sa_to_ip (const struct sockaddr *sa, u8 ip[IP_LEN])
{
	int i;

	for (i = 0; i < IP_LEN; i++)
		ip[i] = (u8) sa->sa_data[i + 2];
}

static inline u16 sa_to_port (const struct sockaddr *sa)
{
	u16 port_num = (u8) sa->sa_data[0] << 8;
	port_num |= (u8) sa->sa_data[1];
	return port_num;
}


/* prepare memory discovery upon library load */
void __attribute ((constructor)) privacy_init (void)
{
	const char *cfg_path = "/tmp/libprivacy.conf";
	char buf[PIPE_BUF] = { 0 };
	char *start, *pos;
	int ifd = -1;
	ssize_t rbytes;
	bool end_parsing = false;
	struct node *node;
	struct port *port;

	INIT_LIST_HEAD(&node_list);
	INIT_LIST_HEAD(&port_list);
#if USE_DEBUG_LOG
	if (!DBG_FILE_VAR) {
		DBG_FILE_VAR = fopen(DBG_FILE_NAME, "a+");
		if (!DBG_FILE_VAR) {
			perror(PFX "fopen debug log");
			exit(1);
		}
	}
#endif
#if DEBUG
	{
		int num_domains = sizeof(domain_strings) /
			sizeof(const char *);
		int num_types = sizeof(type_strings) /
			sizeof(const char *);
		pr_dbg("Knowing %d AF_* and %d SOCK_*.\n",
			num_domains, num_types);
	}
#endif

	/* read config */
	pr_out("reading config from %s ..\n", cfg_path);
	ifd = open(cfg_path, O_RDONLY);
	if (ifd < 0) {
		perror(PFX "open config file");
		goto out;
	}
	rbytes = read(ifd, buf, sizeof(buf));
	if (rbytes < 0) {
		perror(PFX "read");
		goto close_out;
	} else if (rbytes == 0 || rbytes >= sizeof(buf)) {
		pr_err("Wrong input size - ignoring config!\n");
		goto close_out;
	} else {
		buf[rbytes] = '\0';
	}
	close(ifd);

	/* parse config */
	start = pos = buf;
	for (;;) {
		pos = strchr(start, '\n');
		if (!pos) {
			pos = buf + rbytes - 1;
			end_parsing = true;
		} else {
			*pos = '\0';
			if (pos == buf + rbytes - 1)
				end_parsing = true;
		}
		if (strncmp(start, "sockets", buf + rbytes - start) == 0) {
			allow_sockets = true;
			pr_dbg("Allowing sockets!\n");
		} else if (strncmp(start, "bind", buf + rbytes - start) == 0) {
			allow_bind = true;
			pr_dbg("Allowing bind!\n");
		} else if (isdigit(*start)) {
			u16 port_num;
			int i;
			u8 ip[IP_LEN];
			char *sep_pos = strchr(start, ':');
			pr_dbg("Port config detected!\n");
			if (!sep_pos || sep_pos == pos)
				goto parse_err;
			port_num = atoi(start);
			for (i = 0; i < IP_LEN; i++) {
				start = sep_pos + 1;
				sep_pos = strchr(start, '.');
				if ((!sep_pos && i != IP_LEN - 1) ||
				    sep_pos == pos)
					goto parse_err;
				ip[i] = atoi(start);
			}
			if (list_add_port(port_num, ip) < 0)
				goto parse_err;
		} else if (isalpha(*start)) {
			node = calloc(1, sizeof(*node));
			if (!node)
				goto parse_err;
			node->name = malloc(pos - start + 1);
			if (!node->name) {
				free(node);
				goto parse_err;
			}
			memcpy(node->name, start, pos - start);
			node->name[pos - start] = 0;
			list_add_tail(&node->list, &node_list);
		} else {
			goto parse_err;
		}
		if (end_parsing)
			break;
		start = pos + 1;
	}
	list_for_each_entry (node, &node_list, list)
		pr_dbg("%s\n", node->name);
	list_for_each_entry (port, &port_list, list) {
		int i;
		pr_dbg("%u:%u.%u.%u.%u", port->num, port->ips[0],
			port->ips[1], port->ips[2], port->ips[3]);
#undef PFX
#define PFX ""
		for (i = IP_LEN; i < port->ips_len; i += IP_LEN)
			pr_dbg(",%u.%u.%u.%u", port->ips[i + 0],
				port->ips[i + 1], port->ips[i + 2],
				port->ips[i + 3]);
		pr_dbg("\n");
#undef PFX
#define PFX OLD_PFX
	}
out:
	return;
close_out:
	close(ifd);
	return;
parse_err:
	pr_err("parsing error - ignoring rest of the config!\n");
	return;
}

static inline void process_socket_blocking (int domain, int type, int protocol,
					    bool block)
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

	if (block) {
		pr_out("Blocking %s,%s,%s socket for \"%s\"!\n", domain_str,
			type_str, protocol_str, __progname);
	} else if (domain == AF_INET) {
		pr_out("Allowing %s,%s,%s socket for \"%s\"!\n", domain_str,
			type_str, protocol_str, __progname);
	} else {
		pr_dbg("Allowing %s,%s,%s socket for \"%s\"!\n", domain_str,
			type_str, protocol_str, __progname);
	}
}

#ifdef HOOK_SOCKET
int socket (int domain, int type, int protocol)
{
	int sfd = -1;
	static int (*orig_socket)(int domain, int type, int protocol) = NULL;

	if (!allow_sockets && domain != AF_LOCAL) {
		process_socket_blocking(domain, type, protocol, true);
		errno = EACCES;
	} else {
		if (!orig_socket)
			*(void **) (&orig_socket) = dlsym(RTLD_NEXT, "socket");
		process_socket_blocking(domain, type, protocol, false);
		sfd = orig_socket(domain, type, protocol);
	}
	return sfd;
}
#endif

#ifdef HOOK_SOCKETPAIR
int socketpair (int domain, int type, int protocol, int sv[2])
{
	int ret = -1;
	static int (*orig_socketpair)(int domain, int type,
		int protocol, int sv[2]) = NULL;

	if (!allow_sockets && domain != AF_LOCAL) {
		process_socket_blocking(domain, type, protocol, true);
		errno = EOPNOTSUPP;
	} else {
		if (!orig_socketpair)
			*(void **) (&orig_socketpair) =
				dlsym(RTLD_NEXT, "socketpair");
		process_socket_blocking(domain, type, protocol, false);
		ret = orig_socketpair(domain, type, protocol, sv);
	}
	return ret;
}
#endif

static inline void output_sock_addr (const struct sockaddr *sa)
{
	u16 port_num = sa_to_port(sa);
	pr_out("addr info: %u %u %u %u %u %u %u %u %u %u %u "
	       "%u %u\n", port_num, (u8) sa->sa_data[2], (u8) sa->sa_data[3],
	       (u8) sa->sa_data[4], (u8) sa->sa_data[5], (u8) sa->sa_data[6],
	       (u8) sa->sa_data[7], (u8) sa->sa_data[8], (u8) sa->sa_data[9],
	       (u8) sa->sa_data[10], (u8) sa->sa_data[11],
	       (u8) sa->sa_data[12], (u8) sa->sa_data[13]);
}

#ifdef HOOK_GETADDRINFO
int getaddrinfo (const char *node, const char *service,
		 const struct addrinfo *hints, struct addrinfo **res)
{
	int ret;
	bool found = false;
	struct node *node_en;
	struct addrinfo *rp;
	static int (*orig_getaddrinfo)(const char *node, const char *service,
		const struct addrinfo *hints, struct addrinfo **res) = NULL;

	if (!orig_getaddrinfo)
		*(void **) (&orig_getaddrinfo) =
			dlsym(RTLD_NEXT, "getaddrinfo");

	pr_out("%s: node = %s, service = %s\n", __func__, node, service);

	list_for_each_entry (node_en, &node_list, list) {
		if (strcmp(node_en->name, node) == 0) {
			found = true;
			break;
		}
	}
	if (!found)
		node = "127.0.0.1";
	pr_out("new node = %s\n", node);
	ret = orig_getaddrinfo(node, service, hints, res);
	if (ret != 0)
		goto out;
	for (rp = *res; rp != NULL; rp = rp->ai_next) {
		output_sock_addr(rp->ai_addr);
		if (found && rp->ai_addr->sa_family == AF_INET) {
			u16 port_num = sa_to_port(rp->ai_addr);
			u8 ip[IP_LEN];
			sa_to_ip(rp->ai_addr, ip);
			port_num = 0;
			list_add_port(port_num, ip);
		}
	}
out:
	return ret;
}
#endif

#ifdef HOOK_CONNECT
int connect (int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int i, ret = -1;
	struct port *port;
	u16 port_num = 0;
	u8 ip[IP_LEN] = { 0 };
	static int (*orig_connect)(int sockfd, const struct sockaddr *addr,
		socklen_t addrlen) = NULL;

	/* allow connecting to local sockets */
	if (addr->sa_family == AF_LOCAL)
		goto allow_silent;
	if (addr->sa_family != AF_INET) {
#if DEBUG
		int num_domains = sizeof(domain_strings) / sizeof(const char *);
		const char *domain_str = (addr->sa_family >= num_domains) ?
			"AF_UNKNOWN" : domain_strings[addr->sa_family];
		pr_dbg("Blocking %s connect for \"%s\"!\n", domain_str,
			__progname);
		output_sock_addr(addr);
#endif
		errno = EACCES;
		goto out;
	}

	port_num = sa_to_port(addr);
	sa_to_ip(addr, ip);

	list_for_each_entry (port, &port_list, list) {
		if (addr->sa_family != AF_INET)
			continue;
		if (port->num != 0 && port->num != port_num)
			continue;
		for (i = 0; i < port->ips_len; i += IP_LEN) {
			pr_dbg("connect to: %u.%u.%u.%u:%u, allowed: "
				"%u.%u.%u.%u:%u\n", ip[0], ip[1], ip[2], ip[3],
				port_num, port->ips[i + 0], port->ips[i + 1],
				port->ips[i + 2], port->ips[i + 3], port_num);
			if (memcmp(&port->ips[i], &ip, IP_LEN) == 0)
				goto allow;
		}
	}
	/* allow DNS */
	if (port_num != 53) {
		pr_out("Blocking connect to %u.%u.%u.%u:%u for \"%s\"!\n",
			ip[0], ip[1], ip[2], ip[3], port_num, __progname);
		errno = EACCES;
		goto out;
	}
allow:
	pr_out("Allowing connect to %u.%u.%u.%u:%u for \"%s\"!\n",
		ip[0], ip[1], ip[2], ip[3], port_num, __progname);
allow_silent:
	if (!orig_connect)
		*(void **) (&orig_connect) =
			dlsym(RTLD_NEXT, "connect");
	ret = orig_connect(sockfd, addr, addrlen);
out:
	return ret;
}
#endif

#ifdef HOOK_BIND
int bind (int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int ret = -1;
	int port_num = sa_to_port(addr);
	static int (*orig_bind)(int sockfd, const struct sockaddr *addr,
		socklen_t addrlen) = NULL;

	if (addr->sa_family == AF_LOCAL)
		goto allow;
	/* allow local binds and those without a particular port */
	if (!allow_bind && !(addr->sa_family == AF_INET && port_num == 0)) {
		pr_out("Blocking bind for \"%s\"!\n", __progname);
		output_sock_addr(addr);
		errno = EACCES;
		goto out;
	}
allow:
	if (!orig_bind)
		*(void **) (&orig_bind) = dlsym(RTLD_NEXT, "bind");
	pr_dbg("Allowing bind for \"%s\"!\n", __progname);
	ret = orig_bind(sockfd, addr, addrlen);
out:
	return ret;
}
#endif
