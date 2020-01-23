#include "networkHelpers.h"

/**
 * getaddrinfo_example
 * https://gist.github.com/jirihnidek/bf7a2363e480491da72301b228b35d5d
 * "Example of the getaddrinfo() program"
 * 
 * Modified slightly -> changed function type to void, pass in serv_IP,
 *                      copy the IPv4 address into it to be used later
 */
void lookup_host (const char *host, char *serv_IP) {
	struct addrinfo hints, *res;
	int errcode;
	char addrstr[100];
	void *ptr;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;

	errcode = getaddrinfo (host, NULL, &hints, &res);
	if (errcode != 0) {
		perror ("getaddrinfo");
		exit(-1);
	}

	printf ("Host: %s\n", host);
	while (res)
	{
		inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

		switch (res->ai_family)
		{
		case AF_INET:
			ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
			break;
		case AF_INET6:
			ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
			break;
		}
		inet_ntop (res->ai_family, ptr, addrstr, 100);

		if (res->ai_family != PF_INET6) {
			strncpy(serv_IP, addrstr, 99);
		}

		printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
				addrstr, res->ai_canonname);
		res = res->ai_next;
	}
}