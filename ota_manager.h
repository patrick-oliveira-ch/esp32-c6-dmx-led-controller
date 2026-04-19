#pragma once
#include <functional>

void otaManagerBegin(const char* hostname,
                     std::function<void()> onStart,
                     std::function<void()> onEnd);
void otaManagerLoop();
