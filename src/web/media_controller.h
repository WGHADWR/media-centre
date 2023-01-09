//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_MEDIA_CONTROLLER_H
#define VIDEOPLAYER_MEDIA_CONTROLLER_H

#include <unordered_map>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/protocol/http/outgoing/StreamingBody.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "static_files_manager.h"

class MediaController : public oatpp::web::server::api::ApiController {
private:
    OATPP_COMPONENT(std::shared_ptr<StaticFilesManager>, staticFileManager);
private:
    std::shared_ptr<OutgoingResponse> getStaticFileResponse(const oatpp::String& filename, const oatpp::String& rangeHeader) const;
    std::shared_ptr<OutgoingResponse> getFullFileResponse(const oatpp::String& file) const;
    std::shared_ptr<OutgoingResponse> getRangeResponse(const oatpp::String& rangeStr, const oatpp::String& file) const;

public:
    typedef MediaController __ControllerType;

    MediaController(const std::shared_ptr<ObjectMapper>& objectMapper)
            : oatpp::web::server::api::ApiController(objectMapper){}

public:

    /**
     *  Inject @objectMapper component here as default parameter
     */
    static std::shared_ptr<MediaController> createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)){
        return std::shared_ptr<MediaController>(std::make_shared<MediaController>(objectMapper));
    }

#include OATPP_CODEGEN_BEGIN(ApiController)"oatpp/codegen/ApiController_define.hpp"

    ENDPOINT_ASYNC("GET", "/about", About) {
        ENDPOINT_ASYNC_INIT(About)
        Action act() override {
            auto response = controller->createResponse(Status::CODE_200, "About media server");
            response->putOrReplaceHeader("Content-Type", "text/html");
            return _return(response);
        }
    };

    ENDPOINT_ASYNC("GET", "/static/*", Static) {
        ENDPOINT_ASYNC_INIT(Static)
        Action act() override {
            auto filename = request->getPathTail();
            OATPP_ASSERT_HTTP(filename, Status::CODE_400, "Filename is empty");
            auto range = request->getHeader(Header::RANGE);
            return _return(controller->getStaticFileResponse(filename, range));
        }
    };

#include OATPP_CODEGEN_END(ApiController)"oatpp/codegen/ApiController_undef.hpp"
};

#endif //VIDEOPLAYER_MEDIA_CONTROLLER_H
