#ifndef	SPID_VERSION_INCLUDED
#define SPID_VERSION_INCLUDED

#define MAKE_STR_HELPER(a_str) #a_str
#define MAKE_STR(a_str) MAKE_STR_HELPER(a_str)

#define SPID_VERSION_MAJOR	4
#define SPID_VERSION_MINOR	0
#define SPID_VERSION_PATCH	0
#define SPID_VERSION_BETA	0
#define SPID_VERSION_VERSTRING	MAKE_STR(SPID_VERSION_MAJOR) "." MAKE_STR(SPID_VERSION_MINOR) "." MAKE_STR(SPID_VERSION_PATCH) "." MAKE_STR(SPID_VERSION_BETA)

#endif
