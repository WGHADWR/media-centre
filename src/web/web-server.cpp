//
// Created by cyy on 2022/12/14.
//

#include "web-server.h"

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


class WebRequestInterceptor : public oatpp::web::server::interceptor::RequestInterceptor {
public:
    std::shared_ptr<OutgoingResponse> intercept(const std::shared_ptr<IncomingRequest>& request) override {
        auto routes = request->getStartingLine().path.std_str();

        auto userAgent = request->getHeader("User-Agent");
        auto &context = request->getConnection()->getInputStreamContext();
        auto address_format = context.getProperties().get("peer_address_format");
        auto address = context.getProperties().get("peer_address");
        auto port = context.getProperties().get("peer_port");

        OATPP_LOGV("Interceptor", "User-Agent: %s", userAgent == nullptr ? "N/A" : userAgent->c_str());
        OATPP_LOGV("Interceptor", "Client IP: %s - %s port: %s, route: %s",
                   (address_format != nullptr ? address_format->c_str() : "N/A"),
                   (address != nullptr ? address->c_str() : "N/A"),
                   (port != nullptr ? port->c_str() : "N/A"),
                   routes.c_str()
                   );
        return nullptr;
    }
};


int WebServer::start() {
    /* Init oatpp Environment */
    oatpp::base::Environment::init();

    auto *schedule = new Schedule;
    schedule->start();

    WebRequestInterceptor requestInterceptor;
    WebComponent webComponent(this->config->port, &requestInterceptor);
    webComponent.staticFilesManager.getObject()->set_cache_status(false);
    auto connectionProvider = webComponent.serverConnectionProvider;//.getObject();
    // auto connectionHandler = webComponent.serverConnectionHandler.getObject();

    /* Get router component */
//    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    auto router = webComponent.httpRouter.getObject();

    auto hlsController = HlsController::createShared();
    hlsController->setServerConnectionProvider(connectionProvider);
    hlsController->setConfig(this->config);
    hlsController->setSchedule(schedule);

    router->addController(MediaController::createShared());
    router->addController(hlsController);

    router->route("GET", "/about", std::make_shared<AboutHandler>());

    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    oatpp::network::Server server(webComponent.serverConnectionProvider,
                                  webComponent.serverConnectionHandler.getObject());

    OATPP_LOGI("MediaServer", "Server running on port %s", connectionProvider->getProperty("port").getData());

    server.run();
    oatpp::base::Environment::destroy();
    return 0;
}

[[maybe_unused]] int WebServer::test_run() {
    /* Create Router for HTTP requests routing */
    auto router = oatpp::web::server::HttpRouter::createShared();

    router->route("GET", "/about", std::make_shared<AboutHandler>());

    /* Create HTTP connection handler with router */
    auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);

    /* Create TCP connection provider */
    auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});

    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    oatpp::network::Server server(connectionProvider, connectionHandler);

    /* Print info about server port */
    OATPP_LOGI("MyApp", "Server running on port %s", connectionProvider->getProperty("port").getData());

    /* Run server */
    server.run();

    return 0;
}
