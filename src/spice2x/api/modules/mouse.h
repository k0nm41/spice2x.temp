#pragma once

#include "api/module.h"
#include "api/request.h"

namespace api::modules {

    class Mouse : public Module {
    public:
        Mouse();

    private:
        void read(Request &req, Response &res);
        void write(Request &req, Response &res);
        void write_reset(Request &req, Response &res);
    };
}
