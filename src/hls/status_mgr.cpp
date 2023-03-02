//
// Created by smzhkj on 23-3-3.
//

#include "status_mgr.h"

/*static bool check_extend_args(const std::map<std::string, std::any> * j) {
    if (!j) {
        return false;
    }

    if (j->find("serverPort")  == j->end() || j->find("url")  == j->end()) {
        return false;
    }
    try {
        auto port =   j->at("serverPort");
        // auto url = (*j)["url"].get<std::string>();
        std::any_cast<int>(port);
    } catch (std::exception& e) {
        return false;
    }
    return true;
}*/

nlohmann::json StatusManager::send(uint32_t action, std::string& streamId, const std::map<std::string, std::any>* extra_args) {
    /*if (!check_extend_args(extra_args)) {
        nlohmann::json json = (const nlohmann::basic_json<> &) extra_args;
        sm_error("Hls send status failed; invalid args: {}", json.dump());
        throw "Send status failed; Invalid extra parameters";
    }*/

    auto pid = getpid();
    nlohmann::json data;
    data["t"] = GET_TIME_NOW_MILLISEC;
    data["action"] = action;
    data["pid"] = pid;
    if (action == 1) {
        data["id"] = streamId;
    }
    std::string msg;
    const httpCli::Response *res = nullptr;
    try {
        auto port = std::any_cast<int>(extra_args->at("serverPort"));
        auto url = std::any_cast<std::string>(extra_args->at("url"));
        auto target = "http://127.0.0.1:" + std::to_string(port) + url + "?t=" + std::to_string(GET_TIME_NOW_MILLISEC);

        msg = "host: " + target + ", body: " + data.dump();
        // sm_log("Hls send status, {}", msg);
        res = httpCli::post(target.c_str(), data.dump().c_str(), "application/json");
        if (!res || res->statusCode != 200) {
            throw httpCli::RequestError("Send status failed");
        }
        data = parse(res->body);
        //sm_log("Send status success, {}", msg);
        //sm_log("Response: {}", res->body);

        return data;
    } catch (httpCli::RequestError& e) {
        /*sm_error("Send status error: {}", e.what());
        if (res) {
            sm_error(" status code: {}, {}, {}", res->statusCode, res->statusText, msg);
        }*/
        auto re = std::string("Send status error: ").append(e.what());
        throw re;
    } catch (std::exception& e) {
//        sm_error("Send status error; {}", e.what());
        auto re = std::string("Send status error: ").append(e.what());
        throw re;
    }
}
