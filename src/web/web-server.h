//
// Created by cyy on 2022/12/14.
//

#ifndef VIDEOPLAYER_WEBSERVER_H
#define VIDEOPLAYER_WEBSERVER_H

#include <iostream>

#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"

#include "web_component.h"
#include "media_controller.h"
#include "hls/HlsController.h"

#include "../util/encrypt.h"

class WebServer {

private:
    int port;

public:
    int start(uint16_t port);

};

#endif //VIDEOPLAYER_WEBSERVER_H
