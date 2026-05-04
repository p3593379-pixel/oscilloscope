// FILE: oscilloscope_backend/src/main.cpp
#include "buf_connect_server/server.hpp"
#include "buf_connect_server/config/config_loader.hpp"
#include "services/oscilloscope_service_impl.hpp"
#include "services/archive_service_impl.hpp"
#include <spdlog/spdlog.h>
#include <cstdlib>

int main(int argc, char** argv) {
    const char* cfg_path = argc > 1 ? argv[1] : "config.json";
    auto config = buf_connect_server::ConfigLoader::LoadFromFile(cfg_path);
    spdlog::set_level(spdlog::level::debug); // Log everything (debug and up)
    spdlog::flush_on(spdlog::level::debug);

    // JWT secret must come from the environment — never from the config file
    auto js = std::getenv("JWT_SECRET");
    std::string jwt_secret = std::string(js ? js : "buba");

    // BufConnectServer owns the UserStore; db path comes from config
    buf_connect_server::BufConnectServer server(config);

    // Pass jwt_secret so the data plane can validate stream tokens
    auto osc_service = std::make_shared<OscilloscopeServiceImpl>(jwt_secret);

    server.RegisterService(osc_service);
    osc_service->RegisterRoutes(server);

//    server.RegisterService(
//            std::make_shared<ArchiveServiceImpl>(jwt_secret));

    spdlog::info("oscilloscope_backend starting");
    server.Start(jwt_secret);
    spdlog::info("oscilloscope_backend shut down cleanly");
    return 0;
}
