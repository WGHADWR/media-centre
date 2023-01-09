//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_HLSCONTROLLER_H
#define VIDEOPLAYER_HLSCONTROLLER_H

#include <string_view>

#include <unistd.h>
#include <cstdlib>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/protocol/http/outgoing/StreamingBody.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "../http.h"
#include "../../util/util.h"
#include "../web_utils.h"

using namespace std::string_view_literals;

class HlsController : public oatpp::web::server::api::ApiController {

private:
    std::shared_ptr<oatpp::network::ServerConnectionProvider> serverConnectionProvider;

private:
    std::shared_ptr<OutgoingResponse> list();
    std::shared_ptr<OutgoingResponse> open(std::shared_ptr<IncomingRequest> request);

    std::shared_ptr<OutgoingResponse> heartbeat(std::shared_ptr<IncomingRequest> request);

    std::shared_ptr<OutgoingResponse> muxerStatus(std::shared_ptr<IncomingRequest> request);

public:
    typedef HlsController __ControllerType;

    HlsController(const std::shared_ptr<ObjectMapper> &objectMapper) :
        oatpp::web::server::api::ApiController(objectMapper) {
    }

    static std::shared_ptr<HlsController> createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)){
        return std::shared_ptr<HlsController>(std::make_shared<HlsController>(objectMapper));
    }

    void setServerConnectionProvider(std::shared_ptr<oatpp::network::ServerConnectionProvider>& connectionProvider) {
        this->serverConnectionProvider = connectionProvider;
    }

#include OATPP_CODEGEN_BEGIN(ApiController)"oatpp/codegen/ApiController_define.hpp"

    ENDPOINT_ASYNC("GET", "/hls/list", HlsList) {
        ENDPOINT_ASYNC_INIT(HlsList)
        Action act() override {
            return _return(controller->list());
        }
    };

    ENDPOINT_ASYNC("POST", "/hls/open", Open) {
    ENDPOINT_ASYNC_INIT(Open)
        Action act() override {
            return _return(controller->open(request));
        }
    };

    ENDPOINT_ASYNC("GET", "/hls/heartbeat", Heartbeat) {
    ENDPOINT_ASYNC_INIT(Heartbeat)
        Action act() override {
            return _return(controller->heartbeat(request));
        }
    };

    ENDPOINT_ASYNC("POST", "/hls/muxer/status", MuxerStatus) {
    ENDPOINT_ASYNC_INIT(MuxerStatus)
        Action act() override {
            return _return(controller->muxerStatus(request));
        }
    };

#include OATPP_CODEGEN_END(ApiController)"oatpp/codegen/ApiController_undef.hpp"
};


#endif //VIDEOPLAYER_HLSCONTROLLER_H
