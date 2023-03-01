//
// Created by cyy on 2023/1/5.
//

#include "HlsController.h"

std::string HlsController::new_hls_command(std::string& source) {
    std::string hlsCmd = HLS_EXEC_PROGRAM;

#if defined(WIN32) || defined(__WIN32) || defined(__WIN32__)
    hlsCmd = std::string(HLS_EXEC_PROGRAM).append(".exe");
#endif

    nlohmann::json extArgs = {
            { "url", "/hls/muxer/status" }
    };
    extArgs["serverPort"] = std::to_string(this->webConfig->port);

    hlsCmd.append(" -s ").append(source);
    hlsCmd.append(" -d ").append(this->webConfig->hlsDst);
    hlsCmd.append(" -x ").append(extArgs.dump());

    return hlsCmd;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::list() {
    auto data = this->schedule->getAllocatorsJsonStr();
    auto response = createResponse(Status::CODE_200, data);
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);
    return response;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::open(std::shared_ptr<HlsController::IncomingRequest> request) {
    auto body = request->readBodyToString().operator std::string();
    nlojson data = parse(body);
    auto url = data["url"];
    if (url.is_null()) {
        auto response = createResponse(Status::CODE_400, "Parameter url cannot be null");
        return response;
    }
    auto source = url.get<std::string>();
    auto streamId = md5(source);

    if (this->schedule->running(streamId)) {
        auto allocator = this->schedule->hlsProcessors[streamId];
        this->schedule->updateLastAccessTime(streamId);
        auto response = createResponse(Status::CODE_200, allocator->toJson().dump());
        response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);
        return response;
    }

    std::string hlsCmd = this->new_hls_command(source);
    std::cout << "Start hls muxer, command: " << hlsCmd << std::endl;
    int ret = std::system(hlsCmd.c_str());
    if (ret != 0) {
        auto response = createResponse(Status::CODE_500, "Open hls process failed");
        return response;
    }

    auto allocator = new HlsAllocator(streamId, source);
    this->schedule->add(allocator);

    auto response = createResponse(Status::CODE_200, allocator->toJson().dump());
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);

    return response;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::heartbeat(std::shared_ptr<HlsController::IncomingRequest> request) {
    auto id = request->getQueryParameter("id").operator std::string();
    if (id.empty()) {
        auto response = createResponse(Status::CODE_400, "Parameter id cannot be null");
        return response;
    }
    this->schedule->updateLastAccessTime(id);
    auto response = createResponse(Status::CODE_200, "");
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);
    return response;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::muxerStatus(std::shared_ptr<IncomingRequest> request) {
    auto body = request->readBodyToString().operator std::string();
    //json data = parse(body);
    std::cout << body << std::endl;

    auto response = createResponse(Status::CODE_200, "");
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);
    return response;
}