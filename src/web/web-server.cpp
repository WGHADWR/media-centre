//
// Created by cyy on 2022/12/14.
//

#include "web-server.h"

/***
 * httplib
 * @param serv
 */
/*
void set_server_handler(httplib::Server *serv) {
    serv->Get("/about", [](const httplib::Request &, httplib::Response &resp) {
        resp.set_content("v0.0.1", "text/plain");
    });

}

int WebServer::start(int port) {
    this->port = port;
    httplib::Server serv;

    set_server_handler(&serv);

    serv.set_error_handler([](const httplib::Request& req, httplib::Response &res) {
        const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    std::cout << "Start http server on :" << this->port << std::endl;
    bool ret = serv.listen("0.0.0.0", this->port);
    if (!ret) {
        std::cout << "Start http server on port " << this->port << " failed" << std::endl;
    }
    return ret ? 0 : -1;
}

int main(int argc, char* args[]) {
    auto *webServer = new WebServer;
    webServer->start(8080);

    return 0;
}

*/

class AboutHandler : public oatpp::web::server::HttpRequestHandler {
public:
    std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
        return ResponseFactory::createResponse(Status::CODE_200, "About media server");
    }
};

static YAML::Node config;

void parse_configs() {
    try {
        config = YAML::LoadFile("media-server.yaml");
    } catch (std::exception& e) {
        std::cout << "Load config failed; " << e.what() << std::endl;
    }
}

int WebServer::start() {
    /* Init oatpp Environment */
    oatpp::base::Environment::init();
    /* Run App */
    auto *schedule = new Schedule;
    schedule->start();

    /* Create Router for HTTP requests routing */
    // auto router = oatpp::web::server::HttpRouter::createShared();

    WebComponent webComponent(this->config->port);
    webComponent.staticFilesManager.getObject()->set_cache_status(false);
    auto connectionProvider = webComponent.serverConnectionProvider;//.getObject();

    auto router = webComponent.httpRouter.getObject();

    //router->route("GET", "/about", std::make_shared<AboutHandler>());

    auto hlsController = HlsController::createShared();
    hlsController->setServerConnectionProvider(connectionProvider);
    hlsController->setConfig(this->config);
    hlsController->setSchedule(schedule);

    router->addController(MediaController::createShared());
    router->addController(hlsController);

    /* Create HTTP connection handler with router */
    //auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);

    /* Create TCP connection provider */
    //auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", port, oatpp::network::Address::IP_4});

    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    //oatpp::network::Server server(connectionProvider, connectionHandler);

    oatpp::network::Server server(webComponent.serverConnectionProvider,
                                  webComponent.serverConnectionHandler.getObject());

    /* Print info about server port */
    OATPP_LOGI("MediaServer", "Server running on port %s", connectionProvider->getProperty("port").getData());

    /* Run server */
    server.run();

    /* Destroy oatpp Environment */
    oatpp::base::Environment::destroy();
    return 0;
}

int main(int argc, char* args[]) {
//    auto str = "123456";
//    printf("%s\n", Encrypt::md5(str).c_str());

    parse_configs();

    auto port = config["server"]["port"].as<int>();
    auto hlsDest = config["hls"]["m3u"]["dest"].as<std::string>();

    std::cout << "Port: " << port << std::endl;

    auto tcpServer = new net::TcpServer(port + 1);
    tcpServer->start();

    auto webConfig = new WebConfig {
        .port = port,
        .hlsDst = hlsDest.c_str(),
    };

    auto *webServer = new WebServer(webConfig);
    webServer->start();

    return 0;
}