#pragma once

#include <string>
#include <functional>
#include <stdexcept>

namespace makcu {

enum class MouseButton {
    LEFT,
    RIGHT,
    MIDDLE,
    SIDE1,
    SIDE2
};

class MakcuException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Device {
public:
    Device() = default;
    ~Device() = default;

    bool connect(const std::string& port);
    void disconnect();
    bool isConnected() const;

    bool setBaudRate(unsigned int baud_rate, bool apply = false);

    void mouseMove(int x, int y);
    void click(MouseButton button);
    void mouseDown(MouseButton button);
    void mouseUp(MouseButton button);

    void enableButtonMonitoring(bool enable);
    void setMouseButtonCallback(std::function<void(MouseButton, bool)> callback);

private:
    bool connected_ = false;
    std::function<void(MouseButton, bool)> button_callback_;
};

} // namespace makcu
