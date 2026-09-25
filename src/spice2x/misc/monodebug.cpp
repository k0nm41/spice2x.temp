#include "monodebug.h"

#include <windows.h>

#include <cstring>
#include <fstream>
#include <string>

#include "util/execexe.h"
#include "util/libutils.h"
#include "util/logging.h"
#include "util/utils.h"

namespace monodebug {

    static std::string agent_options;
    static bool armed = false;

    static const wchar_t *SETTING_FILE = L"monodebug.txt";

    void init() {
        const auto setting = libutils::module_file_name(nullptr).parent_path() / SETTING_FILE;
        std::ifstream stream(setting);
        std::string address;
        log_info("monodebug", "settings {}: {}", setting.string(), stream ? "found" : "absent");
        if (!stream) {
            return;
        }
        if (!std::getline(stream, address)) {
            log_warning("monodebug", "{} is empty", setting.string());
            return;
        }

        while (!address.empty() && !isgraph(static_cast<unsigned char>(address.back()))) {
            address.pop_back();
        }
        if (address.empty()) {
            log_warning("monodebug", "{} has no address", setting.string());
            return;
        }

        agent_options = fmt::format(
                "transport=dt_socket,address={},server=y,suspend=n",
                address);
        log_info("monodebug", "waiting to arm {}", agent_options);
    }

    void poll() {
        if (agent_options.empty() || armed) {
            return;
        }
        const auto runtime = GetModuleHandleA("mono-2.0-bdwgc.dll") != nullptr
                ? GetModuleHandleA("mono-2.0-bdwgc.dll")
                : (execexe::is_initialized() ? execexe::get_module("mono-2.0-bdwgc.dll", false) : nullptr);
        if (runtime == nullptr) {
            return;
        }

        armed = true;
        const auto debug_init = reinterpret_cast<void (*)(int)>(
                execexe::get_proc(runtime, "mono_debug_init", false));
        const auto parse_options = reinterpret_cast<void (*)(const char *)>(
                execexe::get_proc(runtime, "mono_debugger_agent_parse_options", false));
        log_info("monodebug", "resolved debug_init={} parse_options={}", fmt::ptr(debug_init), fmt::ptr(parse_options));
        if (parse_options == nullptr) {
            return;
        }

        if (debug_init != nullptr) {
            debug_init(1);
            log_info("monodebug", "debug info enabled");
        }

        parse_options(agent_options.c_str());
        log_info("monodebug", "agent enabled, runtime init will start the transport");

    }
}
