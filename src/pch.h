#pragma once

#pragma once

#define NOMINMAX  // Prevent Windows headers from defining min/max macros
// Windows headers (if needed)
#if defined (_WIN32)
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

// Standard library headers
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

