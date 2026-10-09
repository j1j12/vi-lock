#include "rpmsg_client.h"
#include "access_control.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cerrno>

RPMsgClient::RPMsgClient() : fd_(-1) {}

RPMsgClient::~RPMsgClient() { close(); }

bool RPMsgClient::open(const std::string& device)
{
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        perror("open /dev/access_control");
        return false;
    }
    return true;
}

void RPMsgClient::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool RPMsgClient::send(uint32_t msg_id, uint32_t value)
{
    struct ac_msg m;
    m.msg_id = msg_id;
    m.value = value;
    ssize_t n = write(fd_, &m, sizeof(m));
    return (n == (ssize_t)sizeof(m));
}

bool RPMsgClient::recv(uint32_t& msg_id, uint32_t& value, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    int r = ::poll(&pfd, 1, timeout_ms);
    if (r <= 0)
        return false;
    if (!(pfd.revents & POLLIN))
        return false;

    struct ac_msg m;
    ssize_t n = read(fd_, &m, sizeof(m));
    if (n != (ssize_t)sizeof(m))
        return false;
    msg_id = m.msg_id;
    value = m.value;
    return true;
}

bool RPMsgClient::poll(int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    int r = ::poll(&pfd, 1, timeout_ms);
    return (r > 0) && (pfd.revents & POLLIN);
}

bool RPMsgClient::get_door_state(uint32_t& state)
{
    if (ioctl(fd_, AC_IOCTL_GET_DOOR_STATE, &state) < 0)
        return false;
    return true;
}
