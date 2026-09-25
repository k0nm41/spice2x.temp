#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <string>

#include "hooks/netredirect.h"
#include "util/detour.h"
#include "util/logging.h"

namespace {
    decltype(connect) *connect_orig = nullptr;
    decltype(WSAConnect) *WSAConnect_orig = nullptr;

    bool g_enabled = false;
    in_addr g_from_addr {};
    u_short g_from_port = 0;
    in_addr g_to_addr {};
    u_short g_to_port = 0;

    bool parse_endpoint(const std::string &text, in_addr &addr, u_short &port) {
        const auto colon = text.rfind(':');
        if (colon == std::string::npos) {
            return false;
        }
        unsigned value = 0;
        char trail = 0;
        if (sscanf(text.substr(colon + 1).c_str(), "%u%c", &value, &trail) != 1 || value == 0 || value > 65535) {
            return false;
        }
        const auto parsed = inet_addr(text.substr(0, colon).c_str());
        if (parsed == INADDR_NONE) {
            return false;
        }
        addr.s_addr = parsed;
        port = htons(static_cast<u_short>(value));
        return true;
    }

    const sockaddr *redirected(const sockaddr *name, int namelen, sockaddr_in &copy) {
        if (!g_enabled || name == nullptr || namelen < static_cast<int>(sizeof(sockaddr_in)) || name->sa_family != AF_INET) {
            return name;
        }
        const auto *target = reinterpret_cast<const sockaddr_in *>(name);
        if (target->sin_addr.s_addr != g_from_addr.s_addr || target->sin_port != g_from_port) {
            return name;
        }
        copy = *target;
        copy.sin_addr = g_to_addr;
        copy.sin_port = g_to_port;
        return reinterpret_cast<const sockaddr *>(&copy);
    }

    int WSAAPI connect_hook(SOCKET s, const sockaddr *name, int namelen) {
        sockaddr_in copy {};
        return connect_orig(s, redirected(name, namelen, copy), namelen);
    }

    int WSAAPI WSAConnect_hook(SOCKET s, const sockaddr *name, int namelen, LPWSABUF caller_data, LPWSABUF callee_data, LPQOS sqos, LPQOS gqos) {
        sockaddr_in copy {};
        return WSAConnect_orig(s, redirected(name, namelen, copy), namelen, caller_data, callee_data, sqos, gqos);
    }
}

bool netredirect_configure(const std::string &rule) {
    const auto separator = rule.find('=');
    if (separator == std::string::npos
            || !parse_endpoint(rule.substr(0, separator), g_from_addr, g_from_port)
            || !parse_endpoint(rule.substr(separator + 1), g_to_addr, g_to_port)) {
        log_warning("network", "invalid -netredirect '{}', expected ip:port=ip:port", rule);
        return false;
    }
    g_enabled = true;
    return true;
}

void netredirect_init() {
    if (!g_enabled) {
        return;
    }
    bool ok = true;
    ok &= detour::trampoline_try("ws2_32.dll", "connect", (void *) connect_hook, (void **) &connect_orig);
    ok &= detour::trampoline_try("ws2_32.dll", "WSAConnect", (void *) WSAConnect_hook, (void **) &WSAConnect_orig);
    const std::string from = inet_ntoa(g_from_addr);
    const std::string to = inet_ntoa(g_to_addr);
    if (ok) {
        log_info("network", "TCP connects to {}:{} are redirected to {}:{}", from, ntohs(g_from_port), to, ntohs(g_to_port));
    } else {
        log_warning("network", "-netredirect hooks were not all installed");
    }
}
