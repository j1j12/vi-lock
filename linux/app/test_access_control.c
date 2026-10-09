/*
 * test_access_control.c - user-space test for /dev/access_control
 *
 * Build (A7 target, in VM):
 *   arm-none-linux-gnueabihf-gcc -o test_access_control test_access_control.c
 *
 * Run on the board:
 *   ./test_access_control
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include "access_control.h"

static const char *msg_name(uint32_t id)
{
	switch (id) {
	case AC_MSG_PING:            return "PING";
	case AC_MSG_PONG:            return "PONG";
	case AC_MSG_PERSON_DETECTED: return "PERSON_DETECTED";
	case AC_MSG_AUTH_SUCCESS:    return "AUTH_SUCCESS";
	case AC_MSG_AUTH_FAILED:     return "AUTH_FAILED";
	case AC_MSG_UNLOCK:          return "UNLOCK";
	case AC_MSG_UNLOCK_DONE:     return "UNLOCK_DONE";
	case AC_MSG_DOOR_STATE:      return "DOOR_STATE";
	default:                     return "UNKNOWN";
	}
}

int main(int argc, char **argv)
{
	int fd;
	uint32_t state;

	fd = open("/dev/access_control", O_RDWR);
	if (fd < 0) {
		perror("open /dev/access_control");
		return 1;
	}

	if (ioctl(fd, AC_IOCTL_GET_DOOR_STATE, &state) == 0) {
		printf("door state: %s\n",
		       state == AC_DOOR_STATE_UNLOCKED ? "UNLOCKED" : "LOCKED");
	} else {
		perror("ioctl GET_DOOR_STATE");
	}

	/* send PING, expect PONG from M4 */
	{
		struct ac_msg m = { .msg_id = AC_MSG_PING, .value = 0 };
		if (write(fd, &m, sizeof(m)) < 0)
			perror("write PING");
		else
			printf("sent PING\n");
	}

	printf("listening for messages (Ctrl-C to quit)...\n");
	for (;;) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		struct ac_msg m;
		int ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			perror("poll");
			break;
		}
		if (pfd.revents & POLLIN) {
			ssize_t n = read(fd, &m, sizeof(m));
			if (n == (ssize_t)sizeof(m)) {
				printf("recv %s value=%u\n",
				       msg_name(m.msg_id), m.value);
			} else if (n < 0) {
				perror("read");
				break;
			}
		}
	}

	close(fd);
	return 0;
}
