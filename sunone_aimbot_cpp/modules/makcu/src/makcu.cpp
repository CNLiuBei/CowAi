#include "../include/makcu.h"
#include <iostream>

namespace makcu {

bool Device::connect(const std::string& port) {
    std::cerr << "[makcu stub] connect() called - real makcu library not available" << std::endl;
    return false;
}

void Device::disconnect() {
    connected_ = false;
}

bool Device::isConnected() const {
    return connected_;
}

bool Device::setBaudRate(unsigned int baud_rate, bool apply) {
    return false;
}

void Device::mouseMove(int x, int y) {}
void Device::click(MouseButton button) {}
void Device::mouseDown(MouseButton button) {}
void Device::mouseUp(MouseButton button) {}

void Device::enableButtonMonitoring(bool enable) {}

void Device::setMouseButtonCallback(std::function<void(MouseButton, bool)> callback) {
    button_callback_ = callback;
}

} // namespace makcu
