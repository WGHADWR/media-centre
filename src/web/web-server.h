//
// Created by cyy on 2022/12/14.
//

#ifndef VIDEOPLAYER_WEBSERVER_H
#define VIDEOPLAYER_WEBSERVER_H

#include <iostream>

#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"

#include "yaml-cpp/yaml.h"

#include "./schedule.h"

#include "./web_config.h"
#include "web_component.h"
#include "media_controller.h"
#include "hls/HlsController.h"

#include "../util/encrypt.h"

class WebServer {

private:
    const WebConfig* config;

public:
    WebServer(const WebConfig* config): config(config) {};

    int start();

};

#endif //VIDEOPLAYER_WEBSERVER_H
