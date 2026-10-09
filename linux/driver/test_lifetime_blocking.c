/* Read-only fault test for lifetime-v2. SIGALRM bounds a stuck read to 120s.
 * Confirm /proc/PID/stack shows the waiting ac_read before unbinding.
 * No RPMsg write, ioctl command, module removal or firmware stop. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    uint32_t msg[2];
    int fd;
    setvbuf(stdout, NULL, _IONBF, 0);
    fd = open("/dev/access_control", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    alarm(120);
    printf("READY: blocking read test pid=%ld; deadline 120s; no motion commands\n",
           (long)getpid());
    for (;;) {
        ssize_t n = read(fd, msg, sizeof(msg));
        int saved = errno;
        if (n == (ssize_t)sizeof(msg)) continue;
        if (n < 0 && saved == ENODEV) {
            struct pollfd p = { fd, POLLIN, 0 };
            int rc;
            alarm(0);
            rc = poll(&p, 1, 0);
            printf("offline: read=-1 errno=%d poll=%d revents=0x%x\n",
                   saved, rc, p.revents);
            close(fd);
            if (rc == 1 && (p.revents & (POLLHUP | POLLERR)) == (POLLHUP | POLLERR)) {
                puts("PASS: blocking read returned ENODEV; poll HUP+ERR; fd closed");
                return 0;
            }
            puts("FAIL: unexpected offline poll result");
            return 1;
        }
        if (n < 0 && saved == EINTR) continue;
        fprintf(stderr, "FAIL: read=%ld errno=%d\n", (long)n, n < 0 ? saved : 0);
        close(fd);
        return 1;
    }
}
