//
// Created by smzhkj on 23-3-2.
//

#include <yaml-cpp/yaml.h>
#include "../util/sm_log.h"

#include "web-server.h"

static YAML::Node config;

void parse_configs() {
    try {
        config = YAML::LoadFile("media-server.yaml");
    } catch (std::exception& e) {
        sm_error("Load config failed; {}", e.what());
    }
}

int main(int argc, char* args[]) {
//    auto str = "123456";
//    printf("%s\n", Encrypt::md5(str).c_str());

    parse_configs();

    auto port = config["server"]["port"].as<int>();
    auto hlsDest = config["hls"]["m3u"]["dest"].as<std::string>();

    // std::cout << "Port: " << port << std::endl;

    // auto tcpServer = new net::TcpServer(port + 1);
    // tcpServer->start();

    auto webConfig = new WebConfig {
            .port = port,
            .hlsDst = hlsDest.c_str(),
    };

    try {
        auto *webServer = new WebServer(webConfig);
        webServer->start();
    } catch (const std::runtime_error &e) {
        sm_error("Start web server failed. {}", e.what());
    } catch (std::exception& e) {
        sm_error("Start web server failed. {}", e.what());
    } catch (...) {
        sm_error("Start web server failed. Unknown failure occurred");
    }
    return 0;
}