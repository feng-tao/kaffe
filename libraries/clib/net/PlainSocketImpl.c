/*
 * java.net.PlainSocketImpl.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#include "config-net.h"
#include <native.h>
#include "java_io_FileDescriptor.h"
#include "java_lang_Integer.h"
#include "java_net_SocketImpl.h"
#include "java_net_InetAddress.h"
#include "java_net_PlainSocketImpl.h"
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
#ifdef SO_LINGER
    { java_net_SocketOptions_SO_LINGER,		SOL_SOCKET,	SO_LINGER },
#endif
#ifdef SO_REUSEADDR
    { java_net_SocketOptions_SO_REUSEADDR,	SOL_SOCKET,	SO_REUSEADDR },
#endif
#ifdef TCP_NODELAY
    { java_net_SocketOptions_TCP_NODELAY,	IPPROTO_TCP,	TCP_NODELAY },
#endif
  };

/*
 * Create a stream or datagram socket.
 */
void
java_net_PlainSocketImpl_socketCreate(struct Hjava_net_PlainSocketImpl* this, jbool stream)
{
	int fd;
	int type;
	int rc;

	if (stream == 0) {
		type = SOCK_DGRAM;
	}
	else {
		type = SOCK_STREAM;
	}

	rc = KSOCKET(AF_INET, type, 0, &fd);
	if (rc) {
		unhand(unhand(this)->fd)->fd = -1;
		SignalError("java.io.IOException", SYS_ERROR(rc));
	}
	unhand(unhand(this)->fd)->fd = fd;
}

/*
 * Connect the socket to someone.
 */
void
java_net_PlainSocketImpl_socketConnect(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_InetAddress* daddr, jint dport)
{
	int fd;
	int r;
	struct sockaddr_in addr;
	size_t alen;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(dport);
	addr.sin_addr.s_addr = htonl(unhand(daddr)->address);

	fd = unhand(unhand(this)->fd)->fd;
	r = KCONNECT(fd, (struct sockaddr*)&addr, sizeof(addr), unhand(this)->timeout);
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}

	/* Enter information into socket object */
	alen = sizeof(addr);
	r = KGETSOCKNAME(fd, (struct sockaddr*)&addr, &alen);
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}

	unhand(this)->address = daddr;
	unhand(this)->port = dport;
	unhand(this)->localport = ntohs(addr.sin_port);
}

/*
 * Bind this socket to an address.
 */
void
java_net_PlainSocketImpl_socketBind(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_InetAddress* laddr, jint lport)
{
	int r;
	struct sockaddr_in addr;
	int fd;
	int on = 1;
	size_t alen;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(lport);
	addr.sin_addr.s_addr = htonl(unhand(laddr)->address);

	fd = unhand(unhand(this)->fd)->fd;

	/* Allow rebinding to socket - ignore errors */
	(void)KSETSOCKOPT(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	r = KBIND(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}

	/* Enter information into socket object */
	unhand(this)->address = laddr;
	if (lport == 0) {
		alen = sizeof(addr);
		r = KGETSOCKNAME(fd, (struct sockaddr*)&addr, &alen);
		if (r) {
			SignalError("java.io.IOException", SYS_ERROR(r));
		}
		lport = ntohs(addr.sin_port);
	}
	unhand(this)->localport = lport;
}

/*
 * Turn this socket into a listener.
 */
void
java_net_PlainSocketImpl_socketListen(struct Hjava_net_PlainSocketImpl* this, jint count)
{
	int r;

	r = KLISTEN(unhand(unhand(this)->fd)->fd, count);
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}
}

/*
 * Accept a connection.
 */
void
java_net_PlainSocketImpl_socketAccept(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_SocketImpl* sock)
{
	int r;
	int rc;
	size_t alen;
	struct sockaddr_in addr;

	alen = sizeof(addr);
#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(unhand(sock)->localport);
	addr.sin_addr.s_addr = htonl(unhand(unhand(sock)->address)->address);

	rc = KACCEPT(unhand(unhand(this)->fd)->fd, (struct sockaddr*)&addr, &alen, unhand(this)->timeout, &r);
	if (rc == EINTR) {
		SignalError("java.io.InterruptedIOException", 
			    "Accept timed out or was interrupted");
	}
	if (rc) {
		SignalError("java.io.IOException", SYS_ERROR(rc));
	}
	unhand(unhand(sock)->fd)->fd = r;

	/* Enter information into socket object */
	alen = sizeof(addr);
	r = KGETPEERNAME(r, (struct sockaddr*)&addr, &alen);
	if (r) {
		SignalError("java.io.IOException", SYS_ERROR(r));
	}

	unhand(unhand(sock)->address)->address = ntohl(addr.sin_addr.s_addr);
	unhand(sock)->port = ntohs(addr.sin_port);
}

/*
 * Return how many bytes can be read without blocking.
 */
jint
java_net_PlainSocketImpl_socketAvailable(struct Hjava_net_PlainSocketImpl* this)
{
	int r;
	jint len;

#if defined(HAVE_IOCTL) && defined(FIONREAD)
	/* XXX make part of system call interface to protect errno */
	r = ioctl(unhand(unhand(this)->fd)->fd, FIONREAD, &len);
	if (r < 0) {
		SignalError("java.io.IOException", SYS_ERROR(errno));
	}
#else
	/* This uses KSELECT() to work out if we can read - but what
	 * happens at the end of file?
	 */
	static struct timeval tm = { 0, 0 };
	int fd;
	fd_set rd;

	fd = unhand(unhand(this)->fd)->fd;
	FD_ZERO(&rd);
	FD_SET(fd, &rd);
	KSELECT(fd+1, &rd, NULL, NULL, &tm, &r);	/* XXX ignore ret code*/
	if (r == 1) {
		len = 1;
	}
	else {
		len = 0;
	}
#endif
	return (len);
}

/*
 * Close this socket.
 */
void
java_net_PlainSocketImpl_socketClose(struct Hjava_net_PlainSocketImpl* this)
{
	int r;

	if (unhand(unhand(this)->fd)->fd != -1) {
		r = KSOCKCLOSE(unhand(unhand(this)->fd)->fd);
		unhand(unhand(this)->fd)->fd = -1;
		if (r) {
			SignalError("java.io.IOException", SYS_ERROR(r));
		}
	}
}

void
java_net_PlainSocketImpl_socketSetOption(struct Hjava_net_PlainSocketImpl* this, jint opt, struct Hjava_lang_Object* arg)
{
	int k, r, v;

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

	/* Do harder cases */
	switch(opt) {
	case java_net_SocketOptions_SO_BINDADDR:
		SignalError("java.net.SocketException", "Read-only socket option");    
		break;
	case java_net_SocketOptions_SO_TIMEOUT: /* java takes care */
	case java_net_SocketOptions_IP_MULTICAST_IF:
	default:
		SignalError("java.net.SocketException", "Unimplemented socket option");    
		break;
	} 
}

jint
java_net_PlainSocketImpl_socketGetOption(struct Hjava_net_PlainSocketImpl* this, jint opt)
{
	struct sockaddr_in addr;
	int alen = sizeof(addr);
	int k, r, v;
	int vsize = sizeof(v);

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

	/* Do harder cases */
	switch(opt) {
	case java_net_SocketOptions_SO_BINDADDR:
		r = KGETSOCKNAME(unhand(unhand(this)->fd)->fd,
			(struct sockaddr*)&addr, &alen);
		if (r) {
			SignalError("java.net.SocketException", SYS_ERROR(r));
		}
		r = htonl(addr.sin_addr.s_addr);
		break;
	case java_net_SocketOptions_IP_MULTICAST_IF:
	case java_net_SocketOptions_SO_TIMEOUT: /* java takes care */
	default:
		SignalError("java.net.SocketException", "Unimplemented socket option");    
	} 
	return (r);
}

jint
java_net_PlainSocketImpl_read(struct Hjava_net_PlainSocketImpl* this, HArrayOfByte* buf, jint offset, jint len)
{
        ssize_t r;
	int rc;
	int fd;

	fd = unhand(unhand(this)->fd)->fd;
	if (fd < 0) {
		SignalError("java.io.IOException", "fd invalid"); 
	}

        rc = KSOCKREAD(fd, &unhand_array(buf)->body[offset], len, unhand(this)->timeout, &r);
	if (rc == EINTR) {
		SignalError("java.io.InterruptedIOException", 
			    "Read timed out or was interrupted");
	}
        if (rc) {
                SignalError("java.io.IOException", SYS_ERROR(rc));
        }
        else if (r == 0 && len > 0) {
                return (-1);    /* EOF */
        }
        else {
                return (r);
        }
}

void
java_net_PlainSocketImpl_write(struct Hjava_net_PlainSocketImpl* this, HArrayOfByte* buf, jint offset, jint len)
{
        int r;
	int fd;

	fd = unhand(unhand(this)->fd)->fd;
        if (fd >= 0) {
		int rc = KSOCKWRITE(fd, &unhand_array(buf)->body[offset], len, &r);
		if (rc) {
			SignalError("java.io.IOException", SYS_ERROR(rc));
		}
	} else {
		SignalError("java.io.IOException", "fd invalid"); 
	}
}
