# Description:
#   curl is a tool for talking to web servers.

licenses(["notice"])  # MIT/X derivative license

exports_files(["COPYING"])

CURL_WIN_COPTS = [
    "/Iexternal/curl/lib",
    "/DBUILDING_LIBCURL",
    "/DHAVE_CONFIG_H",
    "/DCURL_DISABLE_FTP",
    "/DCURL_DISABLE_NTLM",
    "/DCURL_DISABLE_PROXY",
    "/DHAVE_LIBZ",
    "/DHAVE_ZLIB_H",
    # Defining _USING_V110_SDK71_ is hackery to defeat curl's incorrect
    # detection of what OS releases we can build on with VC 2012. This
    # may not be needed (or may have to change) if the WINVER setting
    # changes in //third_party/msvc/vc_12_0/CROSSTOOL.
    "/D_USING_V110_SDK71_",
]

CURL_WIN_SRCS = [
    "lib/asyn-thread.c",
    "lib/inet_ntop.c",
    "lib/system_win32.c",
    "lib/vtls/schannel.c",
    "lib/idn_win32.c",
]

cc_library(
    name = "curl",
    srcs = [
        "include/curl_config.h",
        "lib/amigaos.h",
        "lib/arpa_telnet.h",
        "lib/asyn.h",
        "lib/asyn-ares.c",
        "lib/base64.c",
        "lib/config-win32.h",
        "lib/conncache.c",
        "lib/conncache.h",
        "lib/connect.c",
        "lib/connect.h",
        "lib/content_encoding.c",
        "lib/content_encoding.h",
        "lib/cookie.c",
        "lib/cookie.h",
        "lib/curl_addrinfo.c",
        "lib/curl_addrinfo.h",
        "lib/curl_base64.h",
        "lib/curl_ctype.c",
        "lib/curl_ctype.h",
        "lib/curl_des.h",
        "lib/curl_endian.h",
        "lib/curl_fnmatch.c",
        "lib/curl_fnmatch.h",
        "lib/curl_gethostname.c",
        "lib/curl_gethostname.h",
        "lib/curl_gssapi.h",
        "lib/curl_hmac.h",
        "lib/curl_ldap.h",
        "lib/curl_md4.h",
        "lib/curl_md5.h",
        "lib/curl_memory.h",
        "lib/curl_memrchr.c",
        "lib/curl_memrchr.h",
        "lib/curl_multibyte.c",
        "lib/curl_multibyte.h",
        "lib/curl_ntlm_core.h",
        "lib/curl_ntlm_wb.h",
        "lib/curl_printf.h",
        "lib/curl_rtmp.c",
        "lib/curl_rtmp.h",
        "lib/curl_sasl.c",
        "lib/curl_sasl.h",
        "lib/curl_sec.h",
        "lib/curl_setup.h",
        "lib/curl_setup_once.h",
        "lib/curl_sha256.h",
        "lib/curl_sspi.c",
        "lib/curl_sspi.h",
        "lib/curl_threads.c",
        "lib/curl_threads.h",
        "lib/curlx.h",
        "lib/dict.h",
        "lib/dotdot.c",
        "lib/dotdot.h",
        "lib/easy.c",
        "lib/easyif.h",
        "lib/escape.c",
        "lib/escape.h",
        "lib/file.h",
        "lib/fileinfo.c",
        "lib/fileinfo.h",
        "lib/formdata.c",
        "lib/formdata.h",
        "lib/ftp.h",
        "lib/ftplistparser.h",
        "lib/getenv.c",
        "lib/getinfo.c",
        "lib/getinfo.h",
        "lib/gopher.h",
        "lib/hash.c",
        "lib/hash.h",
        "lib/hmac.c",
        "lib/hostasyn.c",
        "lib/hostcheck.c",
        "lib/hostcheck.h",
        "lib/hostip.c",
        "lib/hostip.h",
        "lib/hostip4.c",
        "lib/hostip6.c",
        "lib/hostsyn.c",
        "lib/http.c",
        "lib/http.h",
        "lib/http2.c",
        "lib/http2.h",
        "lib/http_chunks.c",
        "lib/http_chunks.h",
        "lib/http_digest.c",
        "lib/http_digest.h",
        "lib/http_negotiate.h",
        "lib/http_ntlm.h",
        "lib/http_proxy.c",
        "lib/http_proxy.h",
        "lib/if2ip.c",
        "lib/if2ip.h",
        "lib/imap.h",
        "lib/inet_ntop.h",
        "lib/inet_pton.c",
        "lib/inet_pton.h",
        "lib/krb5.c",
        "lib/llist.c",
        "lib/llist.h",
        "lib/md4.c",
        "lib/md5.c",
        "lib/memdebug.c",
        "lib/memdebug.h",
        "lib/mime.c",
        "lib/mime.h",
        "lib/mprintf.c",
        "lib/multi.c",
        "lib/multihandle.h",
        "lib/multiif.h",
        "lib/netrc.c",
        "lib/netrc.h",
        "lib/non-ascii.h",
        "lib/nonblock.c",
        "lib/nonblock.h",
        "lib/nwlib.c",
        "lib/nwos.c",
        "lib/parsedate.c",
        "lib/parsedate.h",
        "lib/pingpong.h",
        "lib/pipeline.c",
        "lib/pipeline.h",
        "lib/pop3.h",
        "lib/progress.c",
        "lib/progress.h",
        "lib/rand.c",
        "lib/rand.h",
        "lib/rtsp.c",
        "lib/rtsp.h",
        "lib/security.c",
        "lib/select.c",
        "lib/select.h",
        "lib/sendf.c",
        "lib/sendf.h",
        "lib/setopt.c",
        "lib/setopt.h",
        "lib/setup-os400.h",
        "lib/setup-vms.h",
        "lib/sha256.c",
        "lib/share.c",
        "lib/share.h",
        "lib/sigpipe.h",
        "lib/slist.c",
        "lib/slist.h",
        "lib/smb.h",
        "lib/smtp.h",
        "lib/sockaddr.h",
        "lib/socks.c",
        "lib/socks.h",
        "lib/speedcheck.c",
        "lib/speedcheck.h",
        "lib/splay.c",
        "lib/splay.h",
        "lib/ssh.h",
        "lib/strcase.c",
        "lib/strcase.h",
        "lib/strdup.c",
        "lib/strdup.h",
        "lib/strerror.c",
        "lib/strerror.h",
        "lib/strtok.c",
        "lib/strtok.h",
        "lib/strtoofft.c",
        "lib/strtoofft.h",
        "lib/system_win32.h",
        "lib/telnet.h",
        "lib/tftp.h",
        "lib/timeval.c",
        "lib/timeval.h",
        "lib/transfer.c",
        "lib/transfer.h",
        "lib/url.c",
        "lib/url.h",
        "lib/urldata.h",
        "lib/vauth/cleartext.c",
        "lib/vauth/cram.c",
        "lib/vauth/digest.c",
        "lib/vauth/digest.h",
        "lib/vauth/ntlm.h",
        "lib/vauth/oauth2.c",
        "lib/vauth/vauth.c",
        "lib/vauth/vauth.h",
        "lib/version.c",
        "lib/vtls/axtls.h",
        "lib/vtls/cyassl.h",
        "lib/vtls/darwinssl.h",
        "lib/vtls/gskit.h",
        "lib/vtls/gtls.h",
        "lib/vtls/mbedtls.h",
        "lib/vtls/nssg.h",
        "lib/vtls/openssl.h",
        "lib/vtls/polarssl.h",
        "lib/vtls/polarssl_threadlock.h",
        "lib/vtls/schannel.h",
        "lib/vtls/vtls.c",
        "lib/vtls/vtls.h",
        "lib/warnless.c",
        "lib/warnless.h",
        "lib/wildcard.c",
        "lib/wildcard.h",
        "lib/x509asn1.h",
        "lib/vtls/openssl.c",
    ],
    hdrs = [
        "include/curl/curl.h",
        "include/curl/curlver.h",
        "include/curl/easy.h",
        "include/curl/mprintf.h",
        "include/curl/multi.h",
        "include/curl/stdcheaders.h",
        "include/curl/system.h",
        "include/curl/typecheck-gcc.h",
    ],
    copts = [
            "-Iexternal/curl/lib",
            "-D_GNU_SOURCE",
            "-DBUILDING_LIBCURL",
            "-DHAVE_CONFIG_H",
            "-DCURL_DISABLE_FTP",
            "-DCURL_DISABLE_NTLM",  # turning it off in configure is not enough
            "-DHAVE_LIBZ",
            "-DHAVE_ZLIB_H",
            "-Wno-string-plus-int",
            "-DCURL_MAX_WRITE_SIZE=65536",
    ],
    defines = ["CURL_STATICLIB"],
    includes = ["include"],
    linkopts = [
        "-lrt",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@zlib_archive//:zlib",
        "//external:libssl",
    ],
)

CURL_BIN_WIN_COPTS = [
    "/Iexternal/curl/lib",
    "/DHAVE_CONFIG_H",
    "/DCURL_DISABLE_LIBCURL_OPTION",
]

cc_binary(
    name = "curl_bin",
    srcs = [
        "lib/config-win32.h",
        "src/slist_wc.c",
        "src/slist_wc.h",
        "src/tool_binmode.c",
        "src/tool_binmode.h",
        "src/tool_bname.c",
        "src/tool_bname.h",
        "src/tool_cb_dbg.c",
        "src/tool_cb_dbg.h",
        "src/tool_cb_hdr.c",
        "src/tool_cb_hdr.h",
        "src/tool_cb_prg.c",
        "src/tool_cb_prg.h",
        "src/tool_cb_rea.c",
        "src/tool_cb_rea.h",
        "src/tool_cb_see.c",
        "src/tool_cb_see.h",
        "src/tool_cb_wrt.c",
        "src/tool_cb_wrt.h",
        "src/tool_cfgable.c",
        "src/tool_cfgable.h",
        "src/tool_convert.c",
        "src/tool_convert.h",
        "src/tool_dirhie.c",
        "src/tool_dirhie.h",
        "src/tool_doswin.c",
        "src/tool_doswin.h",
        "src/tool_easysrc.c",
        "src/tool_easysrc.h",
        "src/tool_formparse.c",
        "src/tool_formparse.h",
        "src/tool_getparam.c",
        "src/tool_getparam.h",
        "src/tool_getpass.c",
        "src/tool_getpass.h",
        "src/tool_help.c",
        "src/tool_help.h",
        "src/tool_helpers.c",
        "src/tool_helpers.h",
        "src/tool_homedir.c",
        "src/tool_homedir.h",
        "src/tool_hugehelp.c",
        "src/tool_hugehelp.h",
        "src/tool_libinfo.c",
        "src/tool_libinfo.h",
        "src/tool_main.c",
        "src/tool_main.h",
        "src/tool_metalink.c",
        "src/tool_metalink.h",
        "src/tool_mfiles.c",
        "src/tool_mfiles.h",
        "src/tool_msgs.c",
        "src/tool_msgs.h",
        "src/tool_operate.c",
        "src/tool_operate.h",
        "src/tool_operhlp.c",
        "src/tool_operhlp.h",
        "src/tool_panykey.c",
        "src/tool_panykey.h",
        "src/tool_paramhlp.c",
        "src/tool_paramhlp.h",
        "src/tool_parsecfg.c",
        "src/tool_parsecfg.h",
        "src/tool_sdecls.h",
        "src/tool_setopt.c",
        "src/tool_setopt.h",
        "src/tool_setup.h",
        "src/tool_sleep.c",
        "src/tool_sleep.h",
        "src/tool_strdup.c",
        "src/tool_strdup.h",
        "src/tool_urlglob.c",
        "src/tool_urlglob.h",
        "src/tool_util.c",
        "src/tool_util.h",
        "src/tool_version.h",
        "src/tool_vms.c",
        "src/tool_vms.h",
        "src/tool_writeenv.c",
        "src/tool_writeenv.h",
        "src/tool_writeout.c",
        "src/tool_writeout.h",
        "src/tool_xattr.c",
        "src/tool_xattr.h",
    ],
    copts = [
            "-Iexternal/curl/lib",
            "-D_GNU_SOURCE",
            "-DHAVE_CONFIG_H",
            "-DCURL_DISABLE_LIBCURL_OPTION",
            "-Wno-string-plus-int",
    ],
    deps = [":curl"],
)

genrule(
    name = "configure",
    outs = ["include/curl_config.h"],
    cmd = "\n".join([
        "cat <<'EOF' >$@",
        "#ifndef EXTERNAL_CURL_INCLUDE_CURL_CONFIG_H_",
        "#define EXTERNAL_CURL_INCLUDE_CURL_CONFIG_H_",
        "",
        "#if !defined(_WIN32) && !defined(__APPLE__)",
        "#  include <openssl/opensslv.h>",
        "#  if defined(OPENSSL_IS_BORINGSSL)",
        "#    define HAVE_BORINGSSL 1",
        "#  endif",
        "#endif",
        "",
        "#if defined(_WIN32)",
        "#  include \"lib/config-win32.h\"",
        "#  define BUILDING_LIBCURL 1",
        "#  define CURL_DISABLE_CRYPTO_AUTH 1",
        "#  define CURL_DISABLE_DICT 1",
        "#  define CURL_DISABLE_FILE 1",
        "#  define CURL_DISABLE_GOPHER 1",
        "#  define CURL_DISABLE_IMAP 1",
        "#  define CURL_DISABLE_LDAP 1",
        "#  define CURL_DISABLE_LDAPS 1",
        "#  define CURL_DISABLE_POP3 1",
        "#  define CURL_PULL_WS2TCPIP_H 1",
        "#  define CURL_DISABLE_SMTP 1",
        "#  define CURL_DISABLE_TELNET 1",
        "#  define CURL_DISABLE_TFTP 1",
        "#  define CURL_PULL_WS2TCPIP_H 1",
        "#  define USE_WINDOWS_SSPI 1",
        "#  define USE_WIN32_IDN 1",
        "#  define USE_SCHANNEL 1",
        "#  define WANT_IDN_PROTOTYPES 1",
        "#elif defined(__APPLE__)",
        "#  define HAVE_FSETXATTR_6 1",
        "#  define HAVE_SETMODE 1",
        "#  define HAVE_SYS_FILIO_H 1",
        "#  define HAVE_SYS_SOCKIO_H 1",
        "#  define OS \"x86_64-apple-darwin15.5.0\"",
        "#  define USE_DARWINSSL 1",
        "#else",
        "#  define CURL_CA_BUNDLE \"/etc/ssl/certs/ca-certificates.crt\"",
        "#  define GETSERVBYPORT_R_ARGS 6",
        "#  define GETSERVBYPORT_R_BUFSIZE 4096",
        "#  define HAVE_BORINGSSL 1",
        "#  define HAVE_CLOCK_GETTIME_MONOTONIC 1",
        "#  define HAVE_CRYPTO_CLEANUP_ALL_EX_DATA 1",
        "#  define HAVE_FSETXATTR_5 1",
        "#  define HAVE_GETHOSTBYADDR_R 1",
        "#  define HAVE_GETHOSTBYADDR_R_8 1",
        "#  define HAVE_GETHOSTBYNAME_R 1",
        "#  define HAVE_GETHOSTBYNAME_R_6 1",
        "#  define HAVE_GETSERVBYPORT_R 1",
        "#  define HAVE_LIBSSL 1",
        "#  define HAVE_MALLOC_H 1",
        "#  define HAVE_MSG_NOSIGNAL 1",
        "#  define HAVE_OPENSSL_CRYPTO_H 1",
        "#  define HAVE_OPENSSL_ERR_H 1",
        "#  define HAVE_OPENSSL_PEM_H 1",
        "#  define HAVE_OPENSSL_PKCS12_H 1",
        "#  define HAVE_OPENSSL_RSA_H 1",
        "#  define HAVE_OPENSSL_SSL_H 1",
        "#  define HAVE_OPENSSL_X509_H 1",
        "#  define HAVE_RAND_EGD 1",
        "#  define HAVE_RAND_STATUS 1",
        "#  define HAVE_SSL_GET_SHUTDOWN 1",
        "#  define HAVE_TERMIOS_H 1",
        "#  define OS \"x86_64-pc-linux-gnu\"",
        "#  define RANDOM_FILE \"/dev/urandom\"",
        "#  define USE_OPENSSL 1",
        "#endif",
        "",
        "#if !defined(_WIN32)",
        "#  define CURL_DISABLE_DICT 1",
        "#  define CURL_DISABLE_FILE 1",
        "#  define CURL_DISABLE_GOPHER 1",
        "#  define CURL_DISABLE_IMAP 1",
        "#  define CURL_DISABLE_LDAP 1",
        "#  define CURL_DISABLE_LDAPS 1",
        "#  define CURL_DISABLE_POP3 1",
        "#  define CURL_DISABLE_SMTP 1",
        "#  define CURL_DISABLE_TELNET 1",
        "#  define CURL_DISABLE_TFTP 1",
        "#  define CURL_EXTERN_SYMBOL __attribute__ ((__visibility__ (\"default\")))",
        "#  define ENABLE_IPV6 1",
        "#  define GETHOSTNAME_TYPE_ARG2 size_t",
        "#  define GETNAMEINFO_QUAL_ARG1 const",
        "#  define GETNAMEINFO_TYPE_ARG1 struct sockaddr *",
        "#  define GETNAMEINFO_TYPE_ARG2 socklen_t",
        "#  define GETNAMEINFO_TYPE_ARG46 socklen_t",
        "#  define GETNAMEINFO_TYPE_ARG7 int",
        "#  define HAVE_ALARM 1",
        "#  define HAVE_ALLOCA_H 1",
        "#  define HAVE_ARPA_INET_H 1",
        "#  define HAVE_ARPA_TFTP_H 1",
        "#  define HAVE_ASSERT_H 1",
        "#  define HAVE_BASENAME 1",
        "#  define HAVE_BOOL_T 1",
        "#  define HAVE_CONNECT 1",
        "#  define HAVE_DLFCN_H 1",
        "#  define HAVE_ERRNO_H 1",
        "#  define HAVE_FCNTL 1",
        "#  define HAVE_FCNTL_H 1",
        "#  define HAVE_FCNTL_O_NONBLOCK 1",
        "#  define HAVE_FDOPEN 1",
        "#  define HAVE_FORK 1",
        "#  define HAVE_FREEADDRINFO 1",
        "#  define HAVE_FREEIFADDRS 1",
        "#  if !defined(__ANDROID__)",
        "#    define HAVE_FSETXATTR 1",
        "#  endif",
        "#  define HAVE_FTRUNCATE 1",
        "#  define HAVE_GAI_STRERROR 1",
        "#  define HAVE_GETADDRINFO 1",
        "#  define HAVE_GETADDRINFO_THREADSAFE 1",
        "#  define HAVE_GETEUID 1",
        "#  define HAVE_GETHOSTBYADDR 1",
        "#  define HAVE_GETHOSTBYNAME 1",
        "#  define HAVE_GETHOSTNAME 1",
        "#  if !defined(__ANDROID__)",
        "#    define HAVE_GETIFADDRS 1",
        "#  endif",
        "#  define HAVE_GETNAMEINFO 1",
        "#  define HAVE_GETPPID 1",
        "#  define HAVE_GETPROTOBYNAME 1",
        "#  define HAVE_GETPWUID 1",
        "#  if !defined(__ANDROID__)",
        "#    define HAVE_GETPWUID_R 1",
        "#  endif",
        "#  define HAVE_GETRLIMIT 1",
        "#  define HAVE_GETTIMEOFDAY 1",
        "#  define HAVE_GMTIME_R 1",
        "#  if !defined(__ANDROID__)",
        "#    define HAVE_IFADDRS_H 1",
        "#  endif",
        "#  define HAVE_IF_NAMETOINDEX 1",
        "#  define HAVE_INET_ADDR 1",
        "#  define HAVE_INET_NTOP 1",
        "#  define HAVE_INET_PTON 1",
        "#  define HAVE_INTTYPES_H 1",
        "#  define HAVE_IOCTL 1",
        "#  define HAVE_IOCTL_FIONBIO 1",
        "#  define HAVE_IOCTL_SIOCGIFADDR 1",
        "#  define HAVE_LIBGEN_H 1",
        "#  define HAVE_LIBZ 1",
        "#  define HAVE_LIMITS_H 1",
        "#  define HAVE_LL 1",
        "#  define HAVE_LOCALE_H 1",
        "#  define HAVE_LOCALTIME_R 1",
        "#  define HAVE_LONGLONG 1",
        "#  define HAVE_MEMORY_H 1",
        "#  define HAVE_NETDB_H 1",
        "#  define HAVE_NETINET_IN_H 1",
        "#  define HAVE_NETINET_TCP_H 1",
        "#  define HAVE_NET_IF_H 1",
        "#  define HAVE_PERROR 1",
        "#  define HAVE_PIPE 1",
        "#  define HAVE_POLL 1",
        "#  define HAVE_POLL_FINE 1",
        "#  define HAVE_POLL_H 1",
        "#  define HAVE_POSIX_STRERROR_R 1",
        "#  define HAVE_PWD_H 1",
        "#  define HAVE_RECV 1",
        "#  define HAVE_SELECT 1",
        "#  define HAVE_SEND 1",
        "#  define HAVE_SETJMP_H 1",
        "#  define HAVE_SETLOCALE 1",
        "#  define HAVE_SETRLIMIT 1",
        "#  define HAVE_SETSOCKOPT 1",
        "#  define HAVE_SGTTY_H 1",
        "#  define HAVE_SIGACTION 1",
        "#  define HAVE_SIGINTERRUPT 1",
        "#  define HAVE_SIGNAL 1",
        "#  define HAVE_SIGNAL_H 1",
        "#  define HAVE_SIGSETJMP 1",
        "#  define HAVE_SIG_ATOMIC_T 1",
        "#  define HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID 1",
        "#  define HAVE_SOCKET 1",
        "#  define HAVE_SOCKETPAIR 1",
        "#  define HAVE_STDBOOL_H 1",
        "#  define HAVE_STDINT_H 1",
        "#  define HAVE_STDIO_H 1",
        "#  define HAVE_STDLIB_H 1",
        "#  define HAVE_STRCASECMP 1",
        "#  define HAVE_STRDUP 1",
        "#  define HAVE_STRERROR_R 1",
        "#  define HAVE_STRINGS_H 1",
        "#  define HAVE_STRING_H 1",
        "#  define HAVE_STRNCASECMP 1",
        "#  define HAVE_STRSTR 1",
        "#  define HAVE_STRTOK_R 1",
        "#  define HAVE_STRTOLL 1",
        "#  define HAVE_STRUCT_SOCKADDR_STORAGE 1",
        "#  define HAVE_STRUCT_TIMEVAL 1",
        "#  define HAVE_SYS_IOCTL_H 1",
        "#  define HAVE_SYS_PARAM_H 1",
        "#  define HAVE_SYS_POLL_H 1",
        "#  define HAVE_SYS_RESOURCE_H 1",
        "#  define HAVE_SYS_SELECT_H 1",
        "#  define HAVE_SYS_SOCKET_H 1",
        "#  define HAVE_SYS_STAT_H 1",
        "#  define HAVE_SYS_TIME_H 1",
        "#  define HAVE_SYS_TYPES_H 1",
        "#  define HAVE_SYS_UIO_H 1",
        "#  define HAVE_SYS_UN_H 1",
        "#  define HAVE_SYS_WAIT_H 1",
        "#  define HAVE_SYS_XATTR_H 1",
        "#  define HAVE_TIME_H 1",
        "#  define HAVE_UNAME 1",
        "#  define HAVE_UNISTD_H 1",
        "#  define HAVE_UTIME 1",
        "#  define HAVE_UTIME_H 1",
        "#  define HAVE_VARIADIC_MACROS_C99 1",
        "#  define HAVE_VARIADIC_MACROS_GCC 1",
        "#  define HAVE_WRITABLE_ARGV 1",
        "#  define HAVE_WRITEV 1",
        "#  define HAVE_ZLIB_H 1",
        "#  define LT_OBJDIR \".libs/\"",
        "#  define PACKAGE \"curl\"",
        "#  define PACKAGE_BUGREPORT \"a suitable curl mailing list: https://curl.haxx.se/mail/\"",
        "#  define PACKAGE_NAME \"curl\"",
        "#  define PACKAGE_STRING \"curl -\"",
        "#  define PACKAGE_TARNAME \"curl\"",
        "#  define PACKAGE_URL \"\"",
        "#  define PACKAGE_VERSION \"-\"",
        "#  define RECV_TYPE_ARG1 int",
        "#  define RECV_TYPE_ARG2 void *",
        "#  define RECV_TYPE_ARG3 size_t",
        "#  define RECV_TYPE_ARG4 int",
        "#  define RECV_TYPE_RETV ssize_t",
        "#  define RETSIGTYPE void",
        "#  define SELECT_QUAL_ARG5",
        "#  define SELECT_TYPE_ARG1 int",
        "#  define SELECT_TYPE_ARG234 fd_set *",
        "#  define SELECT_TYPE_ARG5 struct timeval *",
        "#  define SELECT_TYPE_RETV int",
        "#  define SEND_QUAL_ARG2 const",
        "#  define SEND_TYPE_ARG1 int",
        "#  define SEND_TYPE_ARG2 void *",
        "#  define SEND_TYPE_ARG3 size_t",
        "#  define SEND_TYPE_ARG4 int",
        "#  define SEND_TYPE_RETV ssize_t",
        "#  define SIZEOF_INT 4",
        "#  define SIZEOF_LONG 8",
        "#  define SIZEOF_OFF_T 8",
        "#  define SIZEOF_CURL_OFF_T 8",
        "#  define SIZEOF_SHORT 2",
        "#  define SIZEOF_SIZE_T 8",
        "#  define SIZEOF_TIME_T 8",
        "#  define SIZEOF_VOIDP 8",
        "#  define STDC_HEADERS 1",
        "#  define STRERROR_R_TYPE_ARG3 size_t",
        "#  define TIME_WITH_SYS_TIME 1",
        "#  define VERSION \"-\"",
        "#  ifndef _DARWIN_USE_64_BIT_INODE",
        "#    define _DARWIN_USE_64_BIT_INODE 1",
        "#  endif",
        "#endif",
        "",
        "#endif  // EXTERNAL_CURL_INCLUDE_CURL_CONFIG_H_",
        "EOF",
    ]),
)
