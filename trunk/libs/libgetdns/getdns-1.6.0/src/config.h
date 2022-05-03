/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define this enable printing of anchor debugging messages. */
/* #undef ANCHOR_DEBUG */

/* Define this to enable printing of daemon debugging messages. */
/* #undef DAEMON_DEBUG */

/* Define this to disable recursing resolution type. */
#define DISABLE_RESOLUTION_RECURSING 1

/* Define this to enable the experimental dnssec roadblock avoidance. */
#define DNSSEC_ROADBLOCK_AVOIDANCE 1

/* Define this to enable all rrtypes in gldns. */
#define DRAFT_RRTYPES 1

/* Define this to enable the experimental edns cookies. */
#define EDNS_COOKIES 1

/* The edns cookie option code. */
#define EDNS_COOKIE_OPCODE 10

/* How often the edns client cookie is refreshed. */
#define EDNS_COOKIE_ROLLOVER_TIME (24 * 60 * 60)

/* The edns padding option code. */
#define EDNS_PADDING_OPCODE 12

/* Alternate value for the FD_SETSIZE */
/* #undef FD_SETSIZE */

/* Path to static table lookup for hostnames */
#define GETDNS_FN_HOSTS "/etc/hosts"

/* Path to resolver configuration file */
#define GETDNS_FN_RESOLVCONF "/etc/resolv.conf"

/* Define this to enable Windows build. */
/* #undef GETDNS_ON_WINDOWS */

/* Define to 1 if you have the `arc4random' function. */
/* #undef HAVE_ARC4RANDOM */

/* Define to 1 if you have the `arc4random_uniform' function. */
/* #undef HAVE_ARC4RANDOM_UNIFORM */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Whether the C compiler accepts the "format" attribute */
#define HAVE_ATTR_FORMAT 1

/* Whether the C compiler accepts the "unused" attribute */
#define HAVE_ATTR_UNUSED 1

/* Define to 1 if you have the declaration of `arc4random', and to 0 if you
   don't. */
#define HAVE_DECL_ARC4RANDOM 0

/* Define to 1 if you have the declaration of `arc4random_uniform', and to 0
   if you don't. */
#define HAVE_DECL_ARC4RANDOM_UNIFORM 0

/* Define to 1 if you have the declaration of `inet_ntop', and to 0 if you
   don't. */
#define HAVE_DECL_INET_NTOP 0

/* Define to 1 if you have the declaration of `inet_pton', and to 0 if you
   don't. */
#define HAVE_DECL_INET_PTON 0

/* Define to 1 if you have the declaration of `MSG_FASTOPEN', and to 0 if you
   don't. */
#define HAVE_DECL_MSG_FASTOPEN 1

/* Define to 1 if you have the declaration of `NID_ED25519', and to 0 if you
   don't. */
#define HAVE_DECL_NID_ED25519 1

/* Define to 1 if you have the declaration of `NID_ED448', and to 0 if you
   don't. */
#define HAVE_DECL_NID_ED448 1

/* Define to 1 if you have the declaration of `NID_secp384r1', and to 0 if you
   don't. */
#define HAVE_DECL_NID_SECP384R1 1

/* Define to 1 if you have the declaration of `NID_X9_62_prime256v1', and to 0
   if you don't. */
#define HAVE_DECL_NID_X9_62_PRIME256V1 1

/* Define to 1 if you have the declaration of `sk_SSL_COMP_pop_free', and to 0
   if you don't. */
#define HAVE_DECL_SK_SSL_COMP_POP_FREE 1

/* Define to 1 if you have the declaration of
   `SSL_COMP_get_compression_methods', and to 0 if you don't. */
#define HAVE_DECL_SSL_COMP_GET_COMPRESSION_METHODS 1

/* Define to 1 if you have the declaration of `SSL_CTX_set1_curves_list', and
   to 0 if you don't. */
#define HAVE_DECL_SSL_CTX_SET1_CURVES_LIST 1

/* Define to 1 if you have the declaration of `SSL_CTX_set_ecdh_auto', and to
   0 if you don't. */
#define HAVE_DECL_SSL_CTX_SET_ECDH_AUTO 1

/* Define to 1 if you have the declaration of `SSL_get_min_proto_version', and
   to 0 if you don't. */
#define HAVE_DECL_SSL_GET_MIN_PROTO_VERSION 1

/* Define to 1 if you have the declaration of `SSL_set1_curves_list', and to 0
   if you don't. */
#define HAVE_DECL_SSL_SET1_CURVES_LIST 1

/* Define to 1 if you have the declaration of `SSL_set_min_proto_version', and
   to 0 if you don't. */
#define HAVE_DECL_SSL_SET_MIN_PROTO_VERSION 1

/* Define to 1 if you have the declaration of `strlcpy', and to 0 if you
   don't. */
#define HAVE_DECL_STRLCPY 1

/* Define to 1 if you have the declaration of `TCP_FASTOPEN', and to 0 if you
   don't. */
#define HAVE_DECL_TCP_FASTOPEN 1

/* Define to 1 if you have the declaration of `TCP_FASTOPEN_CONNECT', and to 0
   if you don't. */
#define HAVE_DECL_TCP_FASTOPEN_CONNECT 0

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `DSA_set0_key' function. */
#define HAVE_DSA_SET0_KEY 1

/* Define to 1 if you have the `DSA_set0_pqg' function. */
#define HAVE_DSA_SET0_PQG 1

/* Define to 1 if you have the `DSA_SIG_set0' function. */
#define HAVE_DSA_SIG_SET0 1

/* Define to 1 if you have the `ECDSA_SIG_get0' function. */
#define HAVE_ECDSA_SIG_GET0 1

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define to 1 if you have the `ENGINE_load_cryptodev' function. */
/* #undef HAVE_ENGINE_LOAD_CRYPTODEV */

/* Define to 1 if you have the <event2/event.h> header file. */
/* #undef HAVE_EVENT2_EVENT_H */

/* Define to 1 if you have the `event_base_free' function. */
/* #undef HAVE_EVENT_BASE_FREE */

/* Define to 1 if you have the `event_base_new' function. */
/* #undef HAVE_EVENT_BASE_NEW */

/* Define to 1 if you have the <event.h> header file. */
/* #undef HAVE_EVENT_H */

/* Define to 1 if you have the `EVP_DigestVerify' function. */
#define HAVE_EVP_DIGESTVERIFY 1

/* Define to 1 if you have the `EVP_dss1' function. */
/* #undef HAVE_EVP_DSS1 */

/* Define to 1 if you have the `EVP_md5' function. */
#define HAVE_EVP_MD5 1

/* Define to 1 if you have the `EVP_MD_CTX_new' function. */
#define HAVE_EVP_MD_CTX_NEW 1

/* Define to 1 if you have the `EVP_PKEY_base_id' function. */
#define HAVE_EVP_PKEY_BASE_ID 1

/* Define to 1 if you have the `EVP_PKEY_keygen' function. */
#define HAVE_EVP_PKEY_KEYGEN 1

/* Define to 1 if you have the `EVP_sha1' function. */
#define HAVE_EVP_SHA1 1

/* Define to 1 if you have the `EVP_sha224' function. */
#define HAVE_EVP_SHA224 1

/* Define to 1 if you have the `EVP_sha256' function. */
#define HAVE_EVP_SHA256 1

/* Define to 1 if you have the `EVP_sha384' function. */
#define HAVE_EVP_SHA384 1

/* Define to 1 if you have the `EVP_sha512' function. */
#define HAVE_EVP_SHA512 1

/* Define to 1 if you have the <ev.h> header file. */
/* #undef HAVE_EV_H */

/* Define to 1 if you have the `fcntl' function. */
#define HAVE_FCNTL 1

/* Define to 1 if you have the `FIPS_mode' function. */
#define HAVE_FIPS_MODE 1

/* Whether getaddrinfo is available */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getauxval' function. */
/* #undef HAVE_GETAUXVAL */

/* Define this to enable getdns_yaml2dict function. */
/* #undef HAVE_GETDNS_YAML2DICT */

/* Define to 1 if you have the `getentropy' function. */
/* #undef HAVE_GETENTROPY */

/* Define to 1 if you have the `HMAC_CTX_free' function. */
#define HAVE_HMAC_CTX_FREE 1

/* Define to 1 if you have the `HMAC_CTX_new' function. */
#define HAVE_HMAC_CTX_NEW 1

/* If you have HMAC_Update */
#define HAVE_HMAC_UPDATE 1

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have the `inet_pton' function. */
#define HAVE_INET_PTON 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* if the function 'ioctlsocket' is available */
/* #undef HAVE_IOCTLSOCKET */

/* Define to 1 if you have the `crypto' library (-lcrypto). */
/* #undef HAVE_LIBCRYPTO */

/* Define to 1 if you have the <libev/ev.h> header file. */
/* #undef HAVE_LIBEV_EV_H */

/* Define to 1 if you have the `idn' library (-lidn). */
/* #undef HAVE_LIBIDN */

/* Define to 1 if you have the `idn2' library (-lidn). */
/* #undef HAVE_LIBIDN2 */

/* Define to 1 if you have the `unbound' library (-lunbound). */
/* #undef HAVE_LIBUNBOUND */

/* Define to 1 if you have the `yaml' library (-lyaml). */
/* #undef HAVE_LIBYAML */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define this to enable the draft mdns client support. */
/* #undef HAVE_MDNS_SUPPORT */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Use libnettle for crypto */
/* #undef HAVE_NETTLE */

/* Define to 1 if you have the <nettle/dsa-compat.h> header file. */
/* #undef HAVE_NETTLE_DSA_COMPAT_H */

/* Define to 1 if you have the <nettle/eddsa.h> header file. */
/* #undef HAVE_NETTLE_EDDSA_H */

/* Define to 1 if you have the <nettle/nettle-meta.h> header file. */
/* #undef HAVE_NETTLE_NETTLE_META_H */

/* Does libuv have the new uv_time_cb signature */
/* #undef HAVE_NEW_UV_TIMER_CB */

/* Define to 1 if you have the <openssl/bn.h> header file. */
#define HAVE_OPENSSL_BN_H 1

/* Define to 1 if you have the `OPENSSL_config' function. */
#define HAVE_OPENSSL_CONFIG 1

/* Define to 1 if you have the <openssl/conf.h> header file. */
#define HAVE_OPENSSL_CONF_H 1

/* Define to 1 if you have the <openssl/dsa.h> header file. */
#define HAVE_OPENSSL_DSA_H 1

/* Define to 1 if you have the <openssl/engine.h> header file. */
#define HAVE_OPENSSL_ENGINE_H 1

/* Define to 1 if you have the <openssl/err.h> header file. */
#define HAVE_OPENSSL_ERR_H 1

/* Define to 1 if you have the `OPENSSL_init_crypto' function. */
#define HAVE_OPENSSL_INIT_CRYPTO 1

/* Define to 1 if you have the <openssl/rand.h> header file. */
#define HAVE_OPENSSL_RAND_H 1

/* Define to 1 if you have the <openssl/rsa.h> header file. */
#define HAVE_OPENSSL_RSA_H 1

/* Define to 1 if you have the <openssl/ssl.h> header file. */
#define HAVE_OPENSSL_SSL_H 1

/* Define to 1 if you have the `OpenSSL_version' function. */
#define HAVE_OPENSSL_VERSION 1

/* Define to 1 if you have the `OpenSSL_version_num' function. */
#define HAVE_OPENSSL_VERSION_NUM 1

/* Define to 1 if you have the <openssl/x509.h> header file. */
/* #undef HAVE_OPENSSL_X509_H */

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Have pthreads library */
#define HAVE_PTHREAD 1

/* Define to 1 if you have the `RSA_set0_key' function. */
#define HAVE_RSA_SET0_KEY 1

/* Define to 1 if you have the `SHA512_Update' function. */
/* #undef HAVE_SHA512_UPDATE */

/* Define to 1 if you have the `sigaddset' function. */
#define HAVE_SIGADDSET 1

/* Define to 1 if you have the `sigemptyset' function. */
#define HAVE_SIGEMPTYSET 1

/* Define to 1 if you have the `sigfillset' function. */
#define HAVE_SIGFILLSET 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if the system has the type `sigset_t'. */
#define HAVE_SIGSET_T 1

/* Define if you have the SSL libraries installed. */
#define HAVE_SSL /**/

/* Define to 1 if you have the `SSL_CTX_dane_enable' function. */
#define HAVE_SSL_CTX_DANE_ENABLE 1

/* Define to 1 if you have the `SSL_CTX_set_ciphersuites' function. */
#define HAVE_SSL_CTX_SET_CIPHERSUITES 1

/* Define to 1 if you have the `SSL_dane_enable' function. */
#define HAVE_SSL_DANE_ENABLE 1

/* Define to 1 if you have the `SSL_dane_tlsa_add' function. */
#define HAVE_SSL_DANE_TLSA_ADD 1

/* Define if you have libssl with host name verification */
#define HAVE_SSL_HN_AUTH 1

/* Define to 1 if you have the `SSL_set_ciphersuites' function. */
#define HAVE_SSL_SET_CIPHERSUITES 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strptime' function. */
#define HAVE_STRPTIME 1

/* Define to 1 if you have the <sys/limits.h> header file. */
/* #undef HAVE_SYS_LIMITS_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/sha2.h> header file. */
/* #undef HAVE_SYS_SHA2_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the `TLS_client_method' function. */
#define HAVE_TLS_CLIENT_METHOD 1

/* Define if you have libssl with tls 1.2 */
#define HAVE_TLS_v1_2 1

/* Define to 1 if you have the `ub_ctx_set_stub' function. */
/* #undef HAVE_UB_CTX_SET_STUB */

/* Define this when libunbound is compiled with the --enable-event-api option.
   */
/* #undef HAVE_UNBOUND_EVENT_API */

/* Define to 1 if you have the <unbound-event.h> header file. */
/* #undef HAVE_UNBOUND_EVENT_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <uv.h> header file. */
/* #undef HAVE_UV_H */

/* Define to 1 if the system has the type `u_char'. */
#define HAVE_U_CHAR 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define to 1 if you have the <winsock2.h> header file. */
/* #undef HAVE_WINSOCK2_H */

/* Define to 1 if you have the <winsock.h> header file. */
/* #undef HAVE_WINSOCK_H */

/* Define to 1 if you have the <ws2tcpip.h> header file. */
/* #undef HAVE_WS2TCPIP_H */

/* Define to 1 if you have the `X509_check_host' function. */
#define HAVE_X509_CHECK_HOST 1

/* Define to 1 if you have the `X509_get0_notAfter' function. */
#define HAVE_X509_GET0_NOTAFTER 1

/* Define to 1 if you have the `X509_get_notAfter' function. */
/* #undef HAVE_X509_GET_NOTAFTER */

/* Define to 1 if the system has the type `_sigset_t'. */
/* #undef HAVE__SIGSET_T */

/* Whether the C compiler support the __func__ variable */
#define HAVE___FUNC__ 1

/* Do not set this */
/* #undef KEEP_CONNECTIONS_OPEN_DEBUG */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* limit for dynamically-generated DNS options */
#define MAXIMUM_UPSTREAM_OPTION_SPACE 3000

/* The maximum number of cname referrals. */
#define MAX_CNAME_REFERRALS 100

/* Algorithm AES in nettle library */
/* #undef NETTLE_WITH_AES */

/* Algorithm ARCTWO in nettle library */
/* #undef NETTLE_WITH_ARCTWO */

/* Algorithm BLOWFISH in nettle library */
/* #undef NETTLE_WITH_BLOWFISH */

/* Algorithm CAST128 in nettle library */
/* #undef NETTLE_WITH_CAST128 */

/* Algorithm DES in nettle library */
/* #undef NETTLE_WITH_DES */

/* Algorithm DES3 in nettle library */
/* #undef NETTLE_WITH_DES3 */

/* Algorithm MD2 in nettle library */
/* #undef NETTLE_WITH_MD2 */

/* Algorithm MD4 in nettle library */
/* #undef NETTLE_WITH_MD4 */

/* Algorithm MD5 in nettle library */
/* #undef NETTLE_WITH_MD5 */

/* Algorithm SERPENT in nettle library */
/* #undef NETTLE_WITH_SERPENT */

/* Algorithm SHA1 in nettle library */
/* #undef NETTLE_WITH_SHA1 */

/* Algorithm SHA256 in nettle library */
/* #undef NETTLE_WITH_SHA256 */

/* Algorithm TWOFISH in nettle library */
/* #undef NETTLE_WITH_TWOFISH */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "team@getdnsapi.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "getdns"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "getdns 1.6.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "getdns"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://getdnsapi.net"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.6.0"

/* Define this to enable printing of request debugging messages. */
/* #undef REQ_DEBUG */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define this to enable printing of scheduling debugging messages. */
/* #undef SCHED_DEBUG */

/* Define this to enable printing of dnssec debugging messages. */
/* #undef SEC_DEBUG */

/* Define this enable printing of server debugging messages. */
/* #undef SERVER_DEBUG */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* use default strptime. */
#define STRPTIME_WORKS 1

/* Stubby package */
#define STUBBY_PACKAGE "stubby"

/* Stubby package string */
#define STUBBY_PACKAGE_STRING "0.2.6"

/* Define this to enable printing of stub debugging messages. */
/* #undef STUB_DEBUG */

/* Define this to enable native stub DNSSEC support. */
#define STUB_NATIVE_DNSSEC 1

/* System configuration dir */
#define SYSCONFDIR sysconfdir

/* Default trust anchor file */
#define TRUST_ANCHOR_FILE "/etc/unbound/getdns-root.key"

/* Maximum number of queries an failed UDP upstream passes before it will
   retry */
#define UDP_MAX_BACKOFF 1000

/* Define this to use DANE functions from the ssl_dane/danessl library. */
/* #undef USE_DANESSL */

/* Define this to enable DSA support. */
#define USE_DSA 1

/* Define this to enable ECDSA support. */
#define USE_ECDSA 1

/* Define this to enable an EVP workaround for older openssl */
/* #undef USE_ECDSA_EVP_WORKAROUND */

/* Define this to enable ED25519 support. */
#define USE_ED25519 1

/* Define this to enable ED448 support. */
#define USE_ED448 1

/* Use the GnuTLS library */
/* #undef USE_GNUTLS */

/* Define this to enable GOST support. */
#define USE_GOST 1

/* Define this to enable TCP fast open. */
/* #undef USE_OSX_TCP_FASTOPEN */

/* Define this to enable a default eventloop based on poll(). */
#define USE_POLL_DEFAULT_EVENTLOOP 1

/* Define this to enable SHA1 support. */
#define USE_SHA1 1

/* Define this to enable SHA256 and SHA512 support. */
#define USE_SHA2 1

/* Define this to enable TCP fast open. */
#define USE_TCP_FASTOPEN 1

/* Whether the windows socket API is used */
/* #undef USE_WINSOCK */

/* Define this to enable YAML config support. */
/* #undef USE_YAML_CONFIG */

/* Define for Solaris 2.5.1 so the uint32_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT32_T */

/* Define for Solaris 2.5.1 so the uint64_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT64_T */

/* Define for Solaris 2.5.1 so the uint8_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT8_T */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint16_t */

/* Define to the type of an unsigned integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint32_t */

/* Define to the type of an unsigned integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint64_t */

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint8_t */



#ifdef HAVE___FUNC__
#define __FUNC__ __func__
#else
#define __FUNC__ __FUNCTION__
#endif

#ifdef GETDNS_ON_WINDOWS
 /* On windows it is allowed to increase the FD_SETSIZE
  * (and necessary to make our custom eventloop work)
  * See: https://support.microsoft.com/en-us/kb/111855
  */
# ifndef FD_SETSIZE
#  define FD_SETSIZE 1024
# endif

/* the version of the windows API enabled */
# ifndef WINVER
#  define WINVER 0x0600 // 0x0502
# endif
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600 // 0x0502
# endif
# ifdef HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
# endif

# ifdef _MSC_VER
#  if _MSC_VER >= 1800
#   define PRIsz "zu"
#  else
#   define PRIsz "Iu"
#  endif
# else
#  define PRIsz "Iu"
# endif

# ifdef HAVE_WINSOCK2_H
#  include <winsock2.h>
# endif

/* detect if we need to cast to unsigned int for FD_SET to avoid warnings */
# ifdef HAVE_WINSOCK2_H
#  define FD_SET_T (u_int)
# else
#  define FD_SET_T 
# endif

 /* Windows wants us to use _strdup instead of strdup */
# ifndef strdup
#  define strdup _strdup
# endif
#else
# define PRIsz "zu"
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

#if !defined(HAVE_STRLCPY) || !HAVE_DECL_STRLCPY || !defined(strlcpy)
size_t strlcpy(char *dst, const char *src, size_t siz);
#else
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#endif
#if !defined(HAVE_ARC4RANDOM) || !HAVE_DECL_ARC4RANDOM
uint32_t arc4random(void);
#endif
#if !defined(HAVE_ARC4RANDOM_UNIFORM) || !HAVE_DECL_ARC4RANDOM_UNIFORM 
uint32_t arc4random_uniform(uint32_t upper_bound);
#endif
#ifndef HAVE_ARC4RANDOM
void explicit_bzero(void* buf, size_t len);
int getentropy(void* buf, size_t len);
void arc4random_buf(void* buf, size_t n);
void _ARC4_LOCK(void);
void _ARC4_UNLOCK(void);
#endif
#ifdef COMPAT_SHA512
#ifndef SHA512_DIGEST_LENGTH
#define SHA512_BLOCK_LENGTH             128
#define SHA512_DIGEST_LENGTH            64
#define SHA512_DIGEST_STRING_LENGTH     (SHA512_DIGEST_LENGTH * 2 + 1)
typedef struct _SHA512_CTX {
        uint64_t        state[8];
        uint64_t        bitcount[2];
        uint8_t buffer[SHA512_BLOCK_LENGTH];
} SHA512_CTX;
#endif /* SHA512_DIGEST_LENGTH */
void SHA512_Init(SHA512_CTX*);
void SHA512_Update(SHA512_CTX*, void*, size_t);
void SHA512_Final(uint8_t[SHA512_DIGEST_LENGTH], SHA512_CTX*);
unsigned char *SHA512(void* data, unsigned int data_len, unsigned char *digest);
#endif /* COMPAT_SHA512 */

#ifndef HAVE_DECL_INET_PTON
int inet_pton(int af, const char* src, void* dst);
#endif /* HAVE_INET_PTON */

#ifndef HAVE_DECL_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif

#ifdef USE_WINSOCK
# ifndef  _CUSTOM_VSNPRINTF
#  define _CUSTOM_VSNPRINTF
static inline int _gldns_custom_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{ int r = vsnprintf(str, size, format, ap); return r == -1 ? _vscprintf(format, ap) : r; }
#  define vsnprintf _gldns_custom_vsnprintf
# endif
#endif

#ifdef __cplusplus
}
#endif

/** Use on-board gldns */
#define USE_GLDNS 1
#ifdef HAVE_SSL
#  define GLDNS_BUILD_CONFIG_HAVE_SSL 1
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <errno.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_LIMITS_H
#include <sys/limits.h>
#endif

#ifdef PATH_MAX
#define _GETDNS_PATH_MAX PATH_MAX
#else
#define _GETDNS_PATH_MAX 2048
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifdef HAVE_ATTR_FORMAT
#  define ATTR_FORMAT(archetype, string_index, first_to_check) \
    __attribute__ ((format (archetype, string_index, first_to_check)))
#else /* !HAVE_ATTR_FORMAT */
#  define ATTR_FORMAT(archetype, string_index, first_to_check) /* empty */
#endif /* !HAVE_ATTR_FORMAT */

#if defined(DOXYGEN)
#  define ATTR_UNUSED(x)  x
#elif defined(__cplusplus)
#  define ATTR_UNUSED(x)
#elif defined(HAVE_ATTR_UNUSED)
#  define ATTR_UNUSED(x)  x __attribute__((unused))
#else /* !HAVE_ATTR_UNUSED */
#  define ATTR_UNUSED(x)  x
#endif /* !HAVE_ATTR_UNUSED */

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(HAVE_STRPTIME) || !defined(STRPTIME_WORKS)
#define strptime unbound_strptime
struct tm;
char *strptime(const char *s, const char *format, struct tm *tm);
#endif

#if !defined(HAVE_SIGSET_T) && defined(HAVE__SIGSET_T)
typedef _sigset_t sigset_t;
#endif
#if !defined(HAVE_SIGEMPTYSET)
#  define sigemptyset(pset)    (*(pset) = 0)
#endif
#if !defined(HAVE_SIGFILLSET)
#  define sigfillset(pset)     (*(pset) = (sigset_t)-1)
#endif
#if !defined(HAVE_SIGADDSET)
#  define sigaddset(pset, num) (*(pset) |= (1L<<(num)))
#endif

#ifdef HAVE_LIBUNBOUND
# include <unbound.h>
# ifdef HAVE_UNBOUND_EVENT_H
#  include <unbound-event.h>
# else
#  ifdef HAVE_UNBOUND_EVENT_API
#   ifndef _UB_EVENT_PRIMITIVES
#    define _UB_EVENT_PRIMITIVES
struct ub_event_base;
struct ub_ctx* ub_ctx_create_ub_event(struct ub_event_base* base);
typedef void (*ub_event_callback_t)(void*, int, void*, int, int, char*);
int ub_resolve_event(struct ub_ctx* ctx, const char* name, int rrtype, 
        int rrclass, void* mydata, ub_event_callback_t callback, int* async_id);
#   endif
#  endif
# endif
#endif

#ifdef __cplusplus
}
#endif

