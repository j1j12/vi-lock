#ifndef RPMSG_CLIENT_H
#define RPMSG_CLIENT_H

#include <string>
#include <stdint.h>

/*
 * Client for /dev/access_control (kernel RPMsg driver).
 * Sends/receives messages to/from the M4 (LD2401 radar + SG90 door lock).
 */
class RPMsgClient {
public:
    RPMsgClient();
    ~RPMsgClient();

    bool open(const std::string& device = "/dev/access_control");
    void close();
    bool is_open() const { return fd_ >= 0; }

    // Send a message (msg_id, value) to the M4.
    bool send(uint32_t msg_id, uint32_t value);

    // Receive a message, blocking up to timeout_ms. Returns false on timeout.
    bool recv(uint32_t& msg_id, uint32_t& value, int timeout_ms);

    // Poll for an incoming message without consuming it.
    bool poll(int timeout_ms);

    // Query the door state.
    bool get_door_state(uint32_t& state);

private:
    int fd_;
};

#endif // RPMSG_CLIENT_H
