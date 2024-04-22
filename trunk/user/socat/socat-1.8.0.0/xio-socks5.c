/* source: xio-socks5.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* This file contains the source for opening addresses of socks5 type */

/*
* At the moment UDP ASSOCIATE is not supported, but CONNECT and BIND are.
* At the moment no authentication methods are supported (i.e only NO AUTH),
* which is technically not compliant with RFC1928.
*/

#include "xiosysincludes.h"

#if WITH_SOCKS5

#include "xioopen.h"
#include "xio-ascii.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ipapp.h"

#include "xio-socks5.h"


#define SOCKS5_VERSION 5

#define SOCKS5_MAX_REPLY_SIZE	(6 + 256)

#define SOCKS5_AUTH_NONE		0
#define SOCKS5_AUTH_FAIL		0xff

#define SOCKS5_COMMAND_CONNECT		1
#define SOCKS5_COMMAND_BIND		2
#define SOCKS5_COMMAND_UDP_ASSOCIATE	3

#define SOCKS5_ATYPE_IPv4		1
#define SOCKS5_ATYPE_DOMAINNAME		3
#define SOCKS5_ATYPE_IPv6		4

#define SOCKS5_STATUS_SUCCESS				0
#define SOCKS5_STATUS_GENERAL_FAILURE			1
#define SOCKS5_STATUS_CONNECTION_NOT_ALLOWED		2
#define SOCKS5_STATUS_NETWORK_UNREACHABLE		3
#define SOCKS5_STATUS_HOST_UNREACHABLE			4
#define SOCKS5_STATUS_CONNECTION_REFUSED		5
#define SOCKS5_STATUS_TTL_EXPIRED			6
#define SOCKS5_STATUS_COMMAND_NOT_SUPPORTED		7
#define SOCKS5_STATUS_ADDRESS_TYPE_NOT_SUPPORTED	8

static int xioopen_socks5(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xxfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_socks5_connect = { "SOCKS5-CONNECT", 1+XIO_RDWR, xioopen_socks5, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_CHILD|GROUP_RETRY, SOCKS5_COMMAND_CONNECT, 0, 0 HELP(":<socks-server>:<socks-port>:<target-host>:<target-port>") };

const struct addrdesc xioaddr_socks5_listen  = { "SOCKS5-LISTEN",  1+XIO_RDWR, xioopen_socks5, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_CHILD|GROUP_RETRY, SOCKS5_COMMAND_BIND,    0, 0 HELP(":<socks-server>:<socks-port>:<listen-host>:<listen-port>") };

static const char * _xioopen_socks5_strerror(uint8_t r)
{
	switch(r) {
	case SOCKS5_STATUS_SUCCESS:
		return "succeeded";
	case SOCKS5_STATUS_GENERAL_FAILURE:
		return "general SOCKS server failure";
	case SOCKS5_STATUS_CONNECTION_NOT_ALLOWED:
		return "connection not allowed by ruleset";
	case SOCKS5_STATUS_NETWORK_UNREACHABLE:
		return "network unreachable";
	case SOCKS5_STATUS_HOST_UNREACHABLE:
		return "host unreachable";
	case SOCKS5_STATUS_CONNECTION_REFUSED:
		return "connection refused";
	case SOCKS5_STATUS_TTL_EXPIRED:
		return "TTL expired";
	case SOCKS5_STATUS_COMMAND_NOT_SUPPORTED:
		return "command not supported";
	case SOCKS5_STATUS_ADDRESS_TYPE_NOT_SUPPORTED:
		return "address type not supported";
	default:
		return "unknown error";
	}
}

/*
* Performs the SOCKS5 handshake, i.e sends client hello and receives server
* hello back.
* If successful the connection is now ready for sending a SOCKS5 request.
*
* The code is unnecessarily complex right now, for what is essentially
* send(0x050100) followed by "return read() == 0x0500", but will be easier to
* extend for other auth mode support.
*/
static int _xioopen_socks5_handshake(struct single *sfd, int level)
{
	int result;
	ssize_t bytes;
	struct socks5_server_hello server_hello;
	int nmethods = 1;	/* support only 1 auth method - no auth */
	int client_hello_size =
		sizeof(struct socks5_client_hello) +
		(sizeof(uint8_t) * nmethods);

	struct socks5_client_hello *client_hello = Malloc(client_hello_size);
	if (client_hello == NULL) {
		Msg2(level, "malloc(%d): %s",
			client_hello_size, strerror(errno));
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}

		/* malloc failed - could succeed later, so retry then */
		return STAT_RETRYLATER;
	}

	unsigned char *server_hello_ptr = (unsigned char *)&server_hello;

	/* SOCKS5 Hello with 1 authentication mechanism -
	   0x00 NO AUTHENTICATION */
	client_hello->version	= SOCKS5_VERSION;
	client_hello->nmethods	= 1;
	client_hello->methods[0]= SOCKS5_AUTH_NONE;

	/* Send SOCKS5 Client Hello */
	Info2("sending socks5 client hello version=%d nmethods=%d",
		client_hello->version,
		client_hello->nmethods);
#if WITH_MSGLEVEL <= E_DEBUG
	{
		char *msgbuf;
		if ((msgbuf = Malloc(3 * client_hello_size)) != NULL) {
			xiohexdump((unsigned char *)client_hello,
				   client_hello_size, msgbuf);
			Debug1("sending socks5 client hello %s", msgbuf);
			free(msgbuf);
		}
	}
#endif

	if (writefull(sfd->fd, client_hello, client_hello_size) < 0) {
		Msg4(level, "write(%d, %p, %d): %s",
		     sfd->fd, client_hello, client_hello_size,
		     strerror(errno));
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		free(client_hello);

		/* writefull() failed, but might succeed later, so RETRYLATER */
		return STAT_RETRYLATER;
	}
	free(client_hello);

	bytes = 0;
	Info("waiting for socks5 reply");
	while (bytes >= 0) {
		do {
			result = Read(sfd->fd, server_hello_ptr + bytes,
				      sizeof(struct socks5_server_hello)-bytes);
		} while (result < 0 && errno == EINTR);
		if (result < 0) {
			Msg4(level, "read(%d, %p, "F_Zu"): %s",
			     sfd->fd, server_hello_ptr + bytes,
			     sizeof(struct socks5_server_hello)-bytes,
			     strerror(errno));
			if (Close(sfd->fd) < 0) {
				Info2("close(%d): %s", sfd->fd, strerror(errno));
			}
		}
		if (result == 0) {
			Msg(level, "read(): EOF during read of SOCKS5 server hello, peer might not be a SOCKS5 server");
			if (Close(sfd->fd) < 0) {
				Info2("close(%d): %s", sfd->fd,
				      strerror(errno));
			}

			return STAT_RETRYLATER;
		}

		bytes += result;
		if (bytes == sizeof(struct socks5_server_hello)) {
			Debug1("received all "F_Zd" bytes", bytes);
			break;
		}
		Debug2("received %d bytes, waiting for "F_Zu" more bytes",
			result, sizeof(struct socks5_server_hello)-bytes);
	}
	if (result <= 0) {
		return STAT_RETRYLATER;
	}

	Info2("received SOCKS5 server hello version=%d method=%d",
		server_hello.version,
		server_hello.method);

	if (server_hello.version != SOCKS5_VERSION) {
		Msg2(level, "SOCKS5 Server Hello version was %d, not the expected %d, peer might not be a SOCKS5 server",
			server_hello.version, SOCKS5_VERSION);
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		return STAT_RETRYLATER;
	}

	if (server_hello.method == SOCKS5_AUTH_FAIL) {
		Msg(level, "SOCKS5 authentication negotiation failed - client & server have no common supported methods");
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		return STAT_RETRYLATER;
	}

	if (server_hello.method != SOCKS5_AUTH_NONE) {
		Msg1(level, "SOCKS5 server requested unsupported auth method (%d)",
		     server_hello.method);
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		return STAT_RETRYLATER;
	}

	/* Server accepted using no auth */
	return STAT_OK;
}

/*
* Generates the SOCKS5 request for a given command, host and port
*/
static struct socks5_request *_xioopen_socks5_prepare_request(
	int *bytes, const char *target_name, const char *target_port,
	uint8_t socks_command, int level)
{
	struct socks5_request *req;
	char ipaddr[16];
	uint16_t *dstport;

	*bytes = 0;

	if (inet_pton(AF_INET, target_name, ipaddr)){ /* if(valid_ipv4) */
		*bytes = sizeof(struct socks5_request) + 4 + sizeof(uint16_t);
		req = (struct socks5_request *)Malloc(*bytes);
		if (req == NULL){
			Info2("Malloc(%d): %s", *bytes, strerror(errno));
			return NULL;
		}

		req->address_type = SOCKS5_ATYPE_IPv4;
		memcpy(req->dstdata, ipaddr, 4);

		dstport = (uint16_t *) &req->dstdata[4];
		*dstport = parseport(target_port, IPPROTO_TCP);
	} else if (inet_pton(AF_INET6, target_name, ipaddr)) { /* else if(valid_ipv6) */
		*bytes = sizeof(struct socks5_request) + 16 + sizeof(uint16_t);
		req = (struct socks5_request *)Malloc(*bytes);
		if (req == NULL){
			Info2("Malloc(%d): %s", *bytes, strerror(errno));
			return NULL;
		}

		req->address_type = SOCKS5_ATYPE_IPv6;
		memcpy(req->dstdata, ipaddr, 16);

		dstport = (uint16_t *) &req->dstdata[16];
		*dstport = parseport(target_port, IPPROTO_TCP);
	} else { /* invalid IP, assume hostname */
		int hlen = strlen(target_name);
		if (hlen > 255) {
			Msg(level, "target hostname too long (>255 bytes), aborting");
			return NULL;
		}

		*bytes = sizeof(struct socks5_request) + 1 + hlen +
			sizeof(uint16_t);
		req = (struct socks5_request *)Malloc(*bytes);
		if (req == NULL ){
			Info2("malloc(%d): %s", *bytes, strerror(errno));
			return NULL;
		}

		req->address_type = SOCKS5_ATYPE_DOMAINNAME;
		req->dstdata[0] = (unsigned char) hlen;
		memcpy(&req->dstdata[1], target_name, hlen);

		dstport = (uint16_t *) &req->dstdata[hlen + 1];
		*dstport = parseport(target_port, IPPROTO_TCP);
	}


	if (*dstport == 0){
		free(req);
		return NULL;
	}

	req->version = SOCKS5_VERSION;
	req->command = socks_command;
	req->reserved = 0;

	return req;
}

/*
* Reads a server reply after a request has been sent
*/
static int _xioopen_socks5_read_reply(
	struct single *sfd, struct socks5_reply *reply, int level)
{
	int result = 0;
	int bytes_read = 0;
	int bytes_to_read = 5;
	bool typechecked = false;

	while (bytes_to_read >= 0) {
		Info("reading SOCKS5 reply");
		do {
			result = Read(sfd->fd,
				      ((unsigned char *)reply) + bytes_read,
				      bytes_to_read-bytes_read);
		} while (result < 0 && errno == EINTR);
		if (result < 0) {
			Msg4(level, "read(%d, %p, %d): %s",
			     sfd->fd, ((unsigned char *)reply) + bytes_read,
			     bytes_to_read-bytes_read, strerror(errno));
			if (Close(sfd->fd) < 0) {
				Info2("close(%d): %s", sfd->fd, strerror(errno));
			}
			return STAT_RETRYLATER;
		}
		if (result == 0) {
			Msg(level, "read(): EOF during read of SOCKS5 reply");
			if (Close(sfd->fd) < 0) {
				Info2("close(%d): %s",
				      sfd->fd, strerror(errno));
			}
			return STAT_RETRYLATER;
		}
		bytes_read += result;

		/* Once we've read 5 bytes, figure out total message length and
		*  update bytes_to_read accordingly. */
		if (!typechecked && bytes_read <= 5) {
			switch(reply->address_type) {
			case SOCKS5_ATYPE_IPv4:
				/* 6 fixed bytes, and 4 bytes for v4 address */
				bytes_to_read = 10;
				break;
			case SOCKS5_ATYPE_IPv6:
				/* 6 fixed bytes, and 16 bytes for v6 address */
				bytes_to_read = 22;
				break;
			case SOCKS5_ATYPE_DOMAINNAME:
				/* 6 fixed bytes, 1 byte for strlen,
				   and 0-255 bytes for domain name */
				bytes_to_read = 7 + reply->dstdata[0];
				break;
			default:
				Msg1(level, "invalid SOCKS5 reply address type (%d)",
				     reply->address_type);
				if (Close(sfd->fd) < 0) {
					Info2("close(%d): %s",
					      sfd->fd, strerror(errno));
				}
				return STAT_RETRYLATER;
			}
			typechecked = true;
			continue;
		}

		if (bytes_to_read == bytes_read) {
			Debug1("received all %d bytes", bytes_read);
			break;
		}

		Debug2("received %d of %d bytes, waiting",
		       bytes_read, bytes_to_read);
	}

	if (result <= 0) {
		return STAT_RETRYLATER;
	}

	return STAT_OK;
}


/*
* Sends a request and receives the reply.
* If command is BIND we receive two replies.
*/
static int _xioopen_socks5_request(
	struct single *sfd, const char *target_name, const char *target_port,
	uint8_t socks_command, int level)
{
	struct socks5_request *req;
	int bytes, result = 0;

	req = _xioopen_socks5_prepare_request(&bytes, target_name, target_port,
					      socks_command, level);
	if (req == NULL) {
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}

		/* Prepare_request could fail due to malloc, but most likely
		the destination is invalid, e.g too long hostname, so NORETRY */
		return STAT_NORETRY;
	}

	Info4("sending socks5 request version=%d command=%d reserved=%d address_type=%d",
		req->version, req->command, req->reserved, req->address_type);

#if WITH_MSGLEVEL <= E_DEBUG
	{
		char *msgbuf;
		if ((msgbuf = Malloc(3 * bytes)) != NULL) {
			xiohexdump((const unsigned char *)req, bytes, msgbuf);
			Debug1("sending socks5 request %s", msgbuf);
			free(msgbuf);
		}
	}
#endif

	if (writefull(sfd->fd, req, bytes) < 0) {
		Msg4(level, "write(%d, %p, %d): %s",
			sfd->fd, req, bytes, strerror(errno));
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		free(req);
		return STAT_RETRYLATER;
	}
	free(req);
	req = NULL;

	struct socks5_reply *reply = Malloc(SOCKS5_MAX_REPLY_SIZE);
	if (reply == NULL) {
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}

		return STAT_RETRYLATER;
	}

	result = _xioopen_socks5_read_reply(sfd, reply, level);
	if (result != STAT_OK) {
		free(reply);
		return result;
	}

	/* TODO: maybe output nicer debug, like including address */
	Info3("received SOCKS5 reply version=%d reply=%d address_type=%d",
	      reply->version, reply->reply, reply->address_type);

	if (reply->version != SOCKS5_VERSION) {
		Msg2(level, "SOCKS5 reply version was %d, not the expected %d, peer might not be a SOCKS5 server",
			reply->version, SOCKS5_VERSION);
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		free(reply);
		return STAT_RETRYLATER;
	}

	if (reply->reply == SOCKS5_STATUS_SUCCESS &&
	    socks_command == SOCKS5_COMMAND_BIND) {
		Notice("listening on remote host, waiting for connection");
			/* TODO: nicer debug output */
		/* For BIND, we read two replies */
		result = _xioopen_socks5_read_reply(sfd, reply, level);
		if (result != STAT_OK) {
			free(reply);
			return result;
		}
		Notice("received connection on remote host");
		    /* TODO: maybe output nicer debug, like including address */
		Info3("received second SOCKS5 reply version=%d reply=%d address_type=%d",
		      reply->version, reply->reply, reply->address_type);
	}

	switch (reply->reply) {
	case SOCKS5_STATUS_SUCCESS:
		break;
	default:
		Msg2(level, "SOCKS5 server error %d: %s",
		     reply->reply,
		     _xioopen_socks5_strerror(reply->reply));
		if (Close(sfd->fd) < 0) {
			Info2("close(%d): %s", sfd->fd, strerror(errno));
		}
		free(reply);
		return STAT_RETRYLATER;
	}

	free(reply);
	return STAT_OK;
}

/* Same function for all socks5-modes, determined by argv[0] */
static int xioopen_socks5(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
	int socks_command = addrdesc->arg1;
	bool dofork = false;
	int socktype = SOCK_STREAM;
	int pf = PF_UNSPEC;
	int ipproto = IPPROTO_TCP;
	int level, result;
	struct opt *opts0 = NULL;
	struct single *sfd = &xxfd->stream;
	const char *socks_server, *target_name, *target_port, *socks_port;
	union sockaddr_union us_sa, *us = &us_sa;
	socklen_t uslen = sizeof(us_sa);
	struct addrinfo *themlist, *themp;
	bool needbind = false;
	bool lowport = false;
	char infobuff[256];

	if (!xioparms.experimental) {
		Error1("%s: use option --experimental to acknowledge unmature state", argv[0]);
		return STAT_NORETRY;
	}
	if (argc != 5) {
		xio_syntax(argv[0], 4, argc-1, addrdesc->syntax);
		return STAT_NORETRY;
	}

	socks_server = argv[1];
	socks_port = argv[2];
	target_name = argv[3];
	target_port = argv[4];

	if (sfd->howtoend == END_UNSPEC)
		sfd->howtoend = END_SHUTDOWN;
	if (applyopts_single(sfd, opts, PH_INIT) < 0)	return -1;
	applyopts(sfd, -1, opts, PH_INIT);

	retropt_int(opts, OPT_SO_TYPE, &socktype);
	retropt_bool(opts, OPT_FORK, &dofork);

	result = _xioopen_ipapp_prepare(opts, &opts0, socks_server, socks_port,
					&pf, ipproto,
					sfd->para.socket.ip.ai_flags,
					&themlist, us, &uslen,
					&needbind, &lowport, socktype);

	Notice2("connecting to socks5 server %s:%s",
		socks_server, socks_port);

	do {
#if WITH_RETRY
		if (sfd->forever || sfd->retry) {
			level = E_INFO;
		} else {
			level = E_ERROR;
		}
#endif

		/* loop over themlist */
		themp = themlist;
		while (themp != NULL) {
			Notice1("opening connection to %s",
				sockaddr_info(themp->ai_addr, themp->ai_addrlen,
					      infobuff, sizeof(infobuff)));
			result = _xioopen_connect(sfd, needbind?us:NULL, sizeof(*us),
						  themp->ai_addr, themp->ai_addrlen,
						  opts, pf?pf:themp->ai_family, socktype,
						  IPPROTO_TCP, lowport, level);
			if (result == STAT_OK)
				break;
			themp = themp->ai_next;
			if (themp == NULL)
				result = STAT_RETRYLATER;

			switch(result){
				break;
#if WITH_RETRY
			case STAT_RETRYLATER:
			case STAT_RETRYNOW:
				if (sfd->forever || sfd->retry-- ) {
					if (result == STAT_RETRYLATER)	Nanosleep(&sfd->intervall, NULL);
					continue;
				}
#endif
			default:
				xiofreeaddrinfo(themlist);
				return result;
			}
		}
		xiofreeaddrinfo(themlist);
		applyopts(sfd, -1, opts, PH_ALL);

		if ((result = _xio_openlate(sfd, opts)) < 0)
			return result;

		if ((result = _xioopen_socks5_handshake(sfd, level)) != STAT_OK) {
			return result;
		}

		result = _xioopen_socks5_request(sfd, target_name, target_port, socks_command, level);
		switch (result) {
		case STAT_OK:
			break;
#if WITH_RETRY
		case STAT_RETRYLATER:
		case STAT_RETRYNOW:
			if ( sfd->forever || sfd->retry-- ) {
				if (result == STAT_RETRYLATER)	Nanosleep(&sfd->intervall, NULL);
				continue;
			}
#endif
		default:
			return result;
		}

		if (dofork) {
			xiosetchilddied();
		}

#if WITH_RETRY
		if (dofork) {
			pid_t pid;
			int level = E_ERROR;
			if (sfd->forever || sfd->retry) {
				level = E_WARN;
			}
			while ((pid = xio_fork(false, level, sfd->shutup)) < 0) {
				if (sfd->forever || --sfd->retry) {
					Nanosleep(&sfd->intervall, NULL);
					continue;
				}
				return STAT_RETRYLATER;
			}
			if ( pid == 0 ) {
				sfd->forever = false;
				sfd->retry = 0;
				break;
			}

			Close(sfd->fd);
			Nanosleep(&sfd->intervall, NULL);
			dropopts(opts, PH_ALL);
			opts = copyopts(opts0, GROUP_ALL);
			continue;
		} else
#endif
		{
			break;
		}
	} while (true);

	return 0;
}

#endif /* WITH_SOCKS5 */
