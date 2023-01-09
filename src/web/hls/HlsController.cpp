//
// Created by cyy on 2023/1/5.
//

#include "HlsController.h"

std::shared_ptr<HlsController::OutgoingResponse> HlsController::list() {
    auto data = "{}";
    auto response = createResponse(Status::CODE_200, data);
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);
    return response;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::open(std::shared_ptr<HlsController::IncomingRequest> request) {
    auto body = request->readBodyToString().operator std::string();
    json data = parse(body);
    auto url = data["url"];
    if (url.is_null()) {
        auto response = createResponse(Status::CODE_400, "Parameter url cannot be null");
        return response;
    }
    auto source = url.get<std::string>();
    auto streamId = Encrypt::md5(source);

    std::string hlsCmd = "hlsMedia";

#if defined(WIN32) || defined(__WIN32) || defined(__WIN32__)
    hlsCmd = "hlsMedia.exe";
//#elif defined(__linux__)
#else
#endif

    nlohmann::json extArgs = {
            { "url", "/hls/muxer/status" }
    };
    //std::stringstream value;
    auto value = (char*) this->serverConnectionProvider->getProperty("port").getData();
    std::string port = value;
    extArgs["serverPort"] = port;

    hlsCmd.append(" -u ").append(url);
    hlsCmd.append(" -d ").append("D:/ts");
    hlsCmd.append(" -x ").append(extArgs.dump());
    std::cout << "Start hls muxer, command: " << hlsCmd << std::endl;
    int ret = std::system(hlsCmd.c_str());
    if (ret != 0) {
        auto response = createResponse(Status::CODE_500, "Open hls process failed");
        return response;
    }

    json result = {
            { "id", streamId },
    };
    auto response = createResponse(Status::CODE_200, result.dump());
    response->putHeader(Header::CONTENT_TYPE, HTTP_MIME_TYPE_JSON);

    return response;
}

std::shared_ptr<HlsController::OutgoingResponse> HlsController::heartbeat(std::shared_ptr<HlsController::IncomingRequest> request) {
    auto id = request->getQueryParameter("id").operator std::string();
    if (id.empty()) {
        auto response = createResponse(Status::CODE_400, "Parameter id cannot be null");
        return response;
    }
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