#include "rpmsg_worker.h"
#include "../app/access_control.h"
#include <QElapsedTimer>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

static_assert(sizeof(ac_msg) == 8, "RPMsg wire message must be 8 bytes");

void RPMsgWorker::run()
{
    const bool manual = qgetenv("ACCESS_CONTROL_SERVO_MANUAL") == "1";
    const bool oneShot = manual && qgetenv("ACCESS_CONTROL_ONESHOT") == "1";
    while (!isInterruptionRequested()) {
        const int fd = ::open("/dev/access_control", O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            emit linkStatus(QString("未连接：%1").arg(QString::fromLocal8Bit(std::strerror(errno))));
            for (int i=0; i<10 && !isInterruptionRequested(); ++i) msleep(100);
            continue;
        }
        emit linkStatus("已打开，等待 PONG");
        QElapsedTimer clock;
        clock.start();
        qint64 nextPing = 0, sentAt = 0;
        qint64 lastPong = -1;
        bool awaiting = false;
        bool moving = false, accepted = false, servoFault = false;
        qint64 servoSent = 0, nextQuery = 0, lastStatus = -1, cooldown = 0;
        quint32 targetPulse = 0;
        bool queryPending = false;
        qint64 querySent = 0;
        servoReady_.store(false);
        servoPending_.store(-1);
        emit servoReady(false);
        if (manual) emit servoStatus(oneShot ? "等待 v13 单次联动能力确认" : "等待 v11 状态确认");
        unsigned int pongCount = 0;
        while (!isInterruptionRequested()) {
            const qint64 now = clock.elapsed();
            if (manual) {
                if (!servoFault && ((moving && now-servoSent > (targetPulse == 2000 ? 6500 : 4000)) ||
                    (queryPending && now-querySent > 2000))) {
                    servoFault = true;
                    servoReady_.store(false);
                    emit servoReady(false);
                    emit servoStatus("状态超时：停止操作，位置未知");
                }
                int pulse = servoPending_.exchange(-1);
                if (pulse >= 0) {
                    if (servoFault || moving || lastStatus < 0 || now-lastStatus > 2000 ||
                        lastPong < 0 || now-lastPong >= 5000) {
                        servoFault = true;
                        emit servoStatus("状态失效，未发送动作");
                    } else {
                        const ac_msg command = {
                            pulse == 2000 ? 0x53525634U : 0x53525630U,
                            pulse == 2000 ? 0U : static_cast<quint32>(pulse)};
                        if (::write(fd, &command, sizeof(command)) != static_cast<ssize_t>(sizeof(command))) break;
                        moving = true;
                        accepted = false;
                        servoSent = clock.elapsed();
                        targetPulse = static_cast<quint32>(pulse);
                        cooldown = servoSent + (pulse == 2000 ? 7000 : 3500);
                        emit servoStatus("等待 M4 接受命令");
                    }
                }
                if (!servoFault && !queryPending && now >= nextQuery) {
                    const ac_msg query = {0x53525632U, oneShot ? 1U : 0U};
                    if (::write(fd, &query, sizeof(query)) != static_cast<ssize_t>(sizeof(query))) break;
                    queryPending = true;
                    querySent = clock.elapsed();
                    nextQuery = querySent + 300;
                }
            }
            if (awaiting && now - sentAt >= 5000) {
                emit linkStatus("心跳超时");
                emit doorStateReceived(0xffffffffU);
                break;
            }
            if (!awaiting && now >= nextPing) {
                const ac_msg ping = {AC_MSG_PING, 0};
                const ssize_t n = ::write(fd, &ping, sizeof(ping));
                if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                    msleep(100);
                    continue;
                }
                if (n != static_cast<ssize_t>(sizeof(ping))) {
                    emit linkStatus("RPMsg 发送失败");
                    break;
                }
                awaiting = true;
                sentAt = clock.elapsed();
                nextPing = sentAt + 2000;
            }
            struct pollfd pfd = {fd, POLLIN, 0};
            const int ready = ::poll(&pfd, 1, 100);
            if (ready < 0 && errno == EINTR) continue;
            if (ready < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                emit linkStatus("RPMsg 连接异常");
                break;
            }
            if (!(pfd.revents & POLLIN)) continue;
            ac_msg msg;
            const ssize_t n = ::read(fd, &msg, sizeof(msg));
            if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
            if (n != static_cast<ssize_t>(sizeof(msg))) {
                emit linkStatus("RPMsg 接收异常");
                break;
            }
            if (manual && msg.msg_id == 0x53525631U && moving && !accepted) {
                if (msg.value == 1) {
                    accepted = true;
                    emit servoStatus("命令已接受，PWM 执行中");
                } else {
                    servoFault = true;
                    moving = false;
                    servoReady_.store(false);
                    emit servoReady(false);
                    emit servoStatus(QString("M4 拒绝动作：%1").arg(msg.value));
                }
            } else if (manual && msg.msg_id == 0x53525633U && queryPending &&
                       (oneShot ? (msg.value == 0x100 || msg.value == 0x101) : msg.value <= 1)) {
                queryPending = false;
                lastStatus = clock.elapsed();
                bool idle = (msg.value & 1U) == 0;
                if (moving && accepted && idle) {
                    moving = false;
                    if(targetPulse == 2000) emit cycleFinished();
                    emit servoStatus(targetPulse == 2000 ? "单次开关周期已结束；未验证到位" :
                        QString("PWM 已停止，目标 %1us；未验证到位").arg(targetPulse));
                }
                bool ready = !servoFault && !moving && idle &&
                             lastStatus >= cooldown && servoPending_.load() < 0 &&
                             lastPong >= 0 && lastStatus-lastPong < 5000;
                servoReady_.store(ready);
                emit servoReady(ready);
            } else if (msg.msg_id == AC_MSG_PONG && msg.value == 0 && awaiting) {
                awaiting = false;
                lastPong = clock.elapsed();
                emit linkStatus(QString("在线 · PONG %1").arg(++pongCount));
            } else if (msg.msg_id == AC_MSG_PERSON_DETECTED) {
                emit presenceDetected();
            } else if (msg.msg_id == AC_MSG_DOOR_STATE) {
                emit doorStateReceived(msg.value);
            }
        }
        ::close(fd);
        servoReady_.store(false);
        servoPending_.store(-1);
        emit servoReady(false);
        if (manual) emit servoStatus("连接已关闭，位置未知");
        emit doorStateReceived(0xffffffffU);
        for (int i=0; i<10 && !isInterruptionRequested(); ++i) msleep(100);
    }
}
