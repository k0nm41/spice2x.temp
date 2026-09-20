#include "mouse.h"

#include <functional>

#include "external/rapidjson/document.h"
#include "misc/mouseoverride.h"

using namespace std::placeholders;
using namespace rapidjson;

namespace api::modules {

    Mouse::Mouse() : Module("mouse") {
        functions["read"] = std::bind(&Mouse::read, this, _1, _2);
        functions["write"] = std::bind(&Mouse::write, this, _1, _2);
        functions["write_reset"] = std::bind(&Mouse::write_reset, this, _1, _2);
    }

    void Mouse::read(Request &req, Response &res) {
        auto &alloc = res.doc()->GetAllocator();

        SIZE canvas {};
        if (!mouseoverride::canvas(&canvas)) {
            return;
        }

        Value state(kObjectType);
        state.AddMember("width", (int64_t) canvas.cx, alloc);
        state.AddMember("height", (int64_t) canvas.cy, alloc);

        POINT position {};
        bool pressed = false;
        bool active = mouseoverride::read(&position, &pressed);
        state.AddMember("active", active, alloc);
        if (active) {
            state.AddMember("x", (int64_t) position.x, alloc);
            state.AddMember("y", (int64_t) position.y, alloc);
            state.AddMember("pressed", pressed, alloc);
        }

        res.add_data(state);
    }

    void Mouse::write(Request &req, Response &res) {
        if (req.params.Size() < 3) {
            return error_params_insufficient(res);
        }
        if (!req.params[0].IsInt()) {
            return error_type(res, "x", "int");
        }
        if (!req.params[1].IsInt()) {
            return error_type(res, "y", "int");
        }
        if (!req.params[2].IsBool()) {
            return error_type(res, "pressed", "bool");
        }
        if (!mouseoverride::available()) {
            return error(res, "no cursor to override; this game does not read one");
        }

        mouseoverride::write(
                req.params[0].GetInt(),
                req.params[1].GetInt(),
                req.params[2].GetBool());
    }

    void Mouse::write_reset(Request &req, Response &res) {
        mouseoverride::write_reset();
    }
}
