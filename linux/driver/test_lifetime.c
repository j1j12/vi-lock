/* No RPMsg writes or motion commands. Run only against lifetime-v2. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

static long elapsed(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec - start->tv_sec;
}

int main(void)
{
    uint32_t msg[2], state;
    struct timespec start;
    int fd, result = 1;
    setvbuf(stdout, NULL, _IONBF, 0);
    fd = open("/dev/access_control", O_RDONLY | O_NONBLOCK);
    if (fd < 0) { perror("open"); return 1; }
    clock_gettime(CLOCK_MONOTONIC, &start);
    puts("READY: old fd held; unbind driver in second terminal within 120 seconds.");
    while (elapsed(&start) < 120) {
        struct pollfd p = { fd, POLLIN, 0 };
        int n = poll(&p, 1, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("poll"); break;
        }
        if (p.revents & (POLLHUP | POLLERR)) {
            ssize_t r;
            int read_errno, ioctl_ret, ioctl_errno;
            errno = 0;
            r = read(fd, msg, sizeof(msg));
            read_errno = errno;
            errno = 0;
            ioctl_ret = ioctl(fd, _IOR('A', 1, uint32_t), &state);
            ioctl_errno = errno;
            printf("offline: revents=0x%x read=%ld errno=%d ioctl=%d errno=%d\n",
                   p.revents, (long)r, read_errno, ioctl_ret, ioctl_errno);
            if ((p.revents & (POLLHUP | POLLERR)) == (POLLHUP | POLLERR) &&
                r == -1 && read_errno == ENODEV &&
                ioctl_ret == -1 && ioctl_errno == ENODEV) result = 0;
            break;
        }
        if (p.revents & POLLNVAL) { puts("FAIL: invalid fd"); break; }
        if (p.revents & POLLIN) {
            ssize_t r = read(fd, msg, sizeof(msg));
            if (r < 0 && errno != EAGAIN && errno != EINTR && errno != ENODEV) {
                perror("read"); break;
            }
        }
    }
    close(fd);
    puts(result ? "FAIL/TIMEOUT: old-fd disconnect not verified" :
                  "PASS: poll HUP+ERR, read/ioctl ENODEV, old fd closed");
    return result;
}
