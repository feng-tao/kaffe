/*
 * java.net.PlainDatagramSocketImpl.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-net.h"
#include "config-io.h"
#include "config-hacks.h"
#include <native.h>
#include "java_lang_Integer.h"
#include "java_io_FileDescriptor.h"
#include "java_net_DatagramPacket.h"
#include "java_net_PlainDatagramSocketImpl.h"
#include "java_net_InetAddress.h"
#include "java_net_SocketOptions.h"
#include "nets.h"
#include <jsyscall.h>

/*
 * Supported socket options
 */
static const struct {
    int jopt;
    int level;
    int copt;
} socketOptions[] = {
#ifdef SO_SNDBUF
    { java_net_SocketOptions_SO_SNDBUF,		SOL_SOCKET,	SO_SNDBUF },
#endif
#ifdef SO_RCVBUF
    { java_net_SocketOptions_SO_RCVBUF,		SOL_SOCKET,	SO_RCVBUF },
#endif
#ifdef SO_REUSEADDR
    { java_net_SocketOptions_SO_REUSEADDR,	SOL_SOCKET,	SO_REUSEADDR },
#endif
  };

/*
 * Create a datagram socket.
 */
void
java_net_PlainDatagramSocketImpl_datagramSocketCreate(struct Hjava_net_PlainDatagramSocketImpl* this)
{
	int fd;
	int rc;

	rc = KSOCKET(AF_INET, SOCK_DGRAM, 0, &fd);
	if (rc) {
		unhand(unhand(this)->fd)->fd = -1;
		SignalError("java.net.SocketException", SYS_ERROR(rc));
	}
	unhand(unhand(this)->fd)->fd = fd;
#if defined(SOL_SOCKET) && defined(SO_BROADCAST)
	/* On some systems broadcasting is off by default - enable it here */
	{
		int brd = 1;
		KSETSOCKOPT(fd, SOL_SOCKET, SO_BROADCAST, &brd, sizeof(brd));
		/* ignore return code */
	}
#endif
}

/*
 * Bind a port to the socket.
 */
void
java_net_PlainDatagramSocketImpl_bind(struct Hjava_net_PlainDatagramSocketImpl* this, jint port, struct Hjava_net_InetAddress* laddr)
{
	int r;
	struct sockaddr_in addr;
	size_t alen;
	int fd;

	fd = unhand(unhand(this)->fd)->fd;
#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(unhand(laddr)->address);

	r = KBIND(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (r) {
		SignalError("java.net.SocketException", SYS_ERROR(r));
	}

	if (port == 0) {
		alen = sizeof(addr);
		r = KGETSOCKNAME(fd, (struct sockaddr*)&addr, &alen);
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
		}
		port = ntohs(addr.sin_port);
	}
	unhand(this)->localPort = port;
}

void
java_net_PlainDatagramSocketImpl_send(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_DatagramPacket* pkt)
{
	int rc;
	ssize_t bsent;
	struct sockaddr_in addr;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(unhand(pkt)->port);
	addr.sin_addr.s_addr = htonl(unhand(unhand(pkt)->address)->address);

	rc = KSENDTO(unhand(unhand(this)->fd)->fd, unhand_array(unhand(pkt)->buf)->body, unhand(pkt)->length, 0, (struct sockaddr*)&addr, sizeof(addr), &bsent);
	if (rc) {
		SignalError("java.net.SocketException", SYS_ERROR(rc));
	}
}

jint
java_net_PlainDatagramSocketImpl_peek(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* addr)
{
	ssize_t r;
	int rc;
	struct sockaddr_in saddr;
	size_t alen = sizeof(saddr);

	rc = KRECVFROM(unhand(unhand(this)->fd)->fd, 0, 0, MSG_PEEK, (struct sockaddr*)&saddr, &alen, 0 /* timeout */, &r);
	if (rc) {
		SignalError("java.net.SocketException", SYS_ERROR(rc));
	}

	unhand(addr)->address = ntohl(saddr.sin_addr.s_addr);

	return ((jint)r);
}

void
java_net_PlainDatagramSocketImpl_receive(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_DatagramPacket* pkt)
{
	ssize_t r;
	int rc;
	struct sockaddr_in addr;
	size_t alen = sizeof(addr);

	/* Which port am I receiving from */
	addr.sin_port = htons(unhand(this)->localPort);

	rc = KRECVFROM(unhand(unhand(this)->fd)->fd, unhand_array(unhand(pkt)->buf)->body, unhand(pkt)->length, 0, (struct sockaddr*)&addr, &alen, unhand(this)->timeout, &r);
	if (rc) {
		SignalError("java.net.SocketException", SYS_ERROR(rc));
	}

	unhand(pkt)->length = r;
	unhand(pkt)->port = ntohs(addr.sin_port);
	unhand(unhand(pkt)->address)->address = ntohl(addr.sin_addr.s_addr);
}

/*
 * Close the socket.
 */
void
java_net_PlainDatagramSocketImpl_datagramSocketClose(struct Hjava_net_PlainDatagramSocketImpl* this)
{
	int r;

	if (unhand(unhand(this)->fd)->fd != -1) {
		r = KSOCKCLOSE(unhand(unhand(this)->fd)->fd);
		unhand(unhand(this)->fd)->fd = -1;
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
		}
	}
}


void
java_net_PlainDatagramSocketImpl_socketSetOption(struct Hjava_net_PlainDatagramSocketImpl* this, jint opt, struct Hjava_lang_Object* arg)
{
	struct Hjava_net_InetAddress* addrp;
	struct sockaddr_in addr;
	int k, v, r;

	/* Do easy cases */
	for (k = 0; k < sizeof(socketOptions) / sizeof(*socketOptions); k++) {
		if (opt == socketOptions[k].jopt) {
			v = unhand((struct Hjava_lang_Integer*)arg)->value;
			r = KSETSOCKOPT(unhand(unhand(this)->fd)->fd,
				socketOptions[k].level, socketOptions[k].copt,
				&v, sizeof(v));
			if (r) {
				SignalError("java.net.SocketException", SYS_ERROR(r));    
			}
			return;
		}
	}

	switch(opt) {
	case java_net_SocketOptions_IP_MULTICAST_IF:
#if defined(IP_MULTICAST_IF)
		addrp = (struct Hjava_net_InetAddress*)arg;
#if defined(BSD44)
		addr.sin_len = sizeof(addr);
#endif
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(unhand(addrp)->address);
		r = KSETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr));
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
		}
#else
		SignalError("java.net.SocketException", "IP_MULTICAST_IF is not supported");
#endif
		break;

	case java_net_SocketOptions_SO_BINDADDR:
		SignalError("java.net.SocketException", "Read-only socket option");    
		break;
	case java_net_SocketOptions_SO_TIMEOUT: /* JAVA takes care */
	default:
		SignalError("java.net.SocketException", "Unimplemented socket option");   
	} 
}

jint
java_net_PlainDatagramSocketImpl_socketGetOption(struct Hjava_net_PlainDatagramSocketImpl* this, jint opt)
{
	int k, r, v;
	int vsize = sizeof(v);
	struct sockaddr_in addr;
	int alen = sizeof(addr);

	/* Do easy cases */
	for (k = 0; k < sizeof(socketOptions) / sizeof(*socketOptions); k++) {
		if (opt == socketOptions[k].jopt) {
			r = KGETSOCKOPT(unhand(unhand(this)->fd)->fd,
				socketOptions[k].level, socketOptions[k].copt,
				&v, &vsize);
			if (r) {
				SignalError("java.net.SocketException", SYS_ERROR(r));
			}
			return v;
		}
	}

	switch(opt) {
	case java_net_SocketOptions_SO_BINDADDR:
		r = KGETSOCKNAME(unhand(unhand(this)->fd)->fd,
			(struct sockaddr*)&addr, &alen);
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
		}
		r = htonl(addr.sin_addr.s_addr);
		break;
#if defined(IP_MULTICAST_IF)
	case java_net_SocketOptions_IP_MULTICAST_IF:
		r = KGETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_IF, &addr, &alen);
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
			return (0);	/* NOT REACHED, avoid warning */
		}
		r = ntohl(addr.sin_addr.s_addr);    
		break;
#endif
	case java_net_SocketOptions_SO_TIMEOUT: /* JAVA takes care */
	default:
		SignalError("java.net.SocketException", "Unimplemented socket option");   
	} 
	return r;
}

/*
 * Join multicast group
 */
void
java_net_PlainDatagramSocketImpl_join(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* laddr)
{
#if defined(IP_ADD_MEMBERSHIP)
	int r;
	struct ip_mreq ipm;

	ipm.imr_multiaddr.s_addr = htonl(unhand(laddr)->address);
	ipm.imr_interface.s_addr = htonl(INADDR_ANY);

	r = KSETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ipm, sizeof(ipm));
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}
#else
	SignalError("java.net.SocketException", "IP_ADD_MEMBERSHIP not supported");
#endif
}

/*
 * leave multicast group
 */
void
java_net_PlainDatagramSocketImpl_leave(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* laddr)
{
#if defined(IP_DROP_MEMBERSHIP)
	int r;
	struct ip_mreq ipm;

	ipm.imr_multiaddr.s_addr = htonl(unhand(laddr)->address);
	ipm.imr_interface.s_addr = htonl(INADDR_ANY);

	r = KSETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &ipm, sizeof(ipm));
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}
#else
	SignalError("java.net.SocketException", "IP_DROP_MEMBERSHIP not supported");
#endif
}

/*
 * set multicast-TTL
 */
void
java_net_PlainDatagramSocketImpl_setTTL(struct Hjava_net_PlainDatagramSocketImpl* this, jbool ttl)
{
#if defined(IP_MULTICAST_TTL)
	int r;
	unsigned char v = (unsigned char)ttl;

	r = KSETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_TTL, &v, sizeof(v));
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}
#else
	SignalError("java.net.SocketException", "IP_MULTICAST_TTL not supported");
#endif
}

/*
 * get multicast-TTL
 */
jbyte
java_net_PlainDatagramSocketImpl_getTTL(struct Hjava_net_PlainDatagramSocketImpl* this)
{
#if defined(IP_MULTICAST_TTL)
	unsigned char v;
	int s;
	int r;

	r = KGETSOCKOPT(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_TTL, &v, &s);
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}
	return (jbyte)v;
#else
	SignalError("java.net.SocketException", "IP_MULTICAST_TTL not supported");
#endif
}
