//
// Created by cyy on 2023/1/6.
//

#include <iostream>
#include <vector>

#include "status_mgr.h"
#include "hls.h"

void define_options(cmdline::parser *parser) {
    parser->add<std::string>("source", 's', "media source", true, "rtsp://127.0.0.1:8554/stream");
    parser->add<std::string>("dir", 'd', "target dir", false, ".");
    parser->add<std::string>("ext", 'e', "extends options, json", false, "{}");
    parser->add<bool>("standalone", 'a', "stand-alone", false, true);
    parser->add<bool>("mute", 'm', "no audio", false, false);
}

std::map<std::string, std::any>* parse_extra_args(std::string& args) {
    nlohmann::json data = parse(args);
    if (data.is_null() || data.empty()) {
        return {};
    }
    auto map = new std::map<std::string, std::any>;
    if (data.contains("serverPort")) {
        int serverPort = data.at("serverPort").get<int>();
        map->emplace("serverPort", serverPort);
    }
    if (data.contains("url")) {
        auto url = data.at("url").get<std::string>();
        map->emplace("url", url);
    }

    return map;
}


static bool check_extend_args(const std::map<std::string, std::any> * j) {
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
}


int main(int argv, char *args[]) {
    cmdline::parser parser;
    define_options(&parser);

    httpCli::global_init();

    parser.parse_check(argv, args);
    auto url = parser.get<std::string>("source");
    auto dir = parser.get<std::string>("dir");
    auto ext = parser.get<std::string>("ext");
    auto standalone = parser.get<bool>("standalone");
    auto mute = parser.get<bool>("mute");

    if (url.length() == 0) {
        url = "rtsp://192.168.2.46:8554/test2";
    }
    if (dir.length() == 0) {
        dir = "D:/ts";
#ifdef linux
        dir = "./ts";
#endif
    }

    sm_log("HlsMuxer args, source: {}, ext: {}, target dir: {}, standalone: {}", url, ext, dir, standalone);

    std::map<std::string, std::any> *extraArgs = parse_extra_args(ext);
    if (!standalone && !check_extend_args(extraArgs)) {
        nlohmann::json json = (const nlohmann::basic_json<> &) extraArgs;
        sm_error("Execute hls failed; Invalid args: {}", json.dump());
        return -1;
    }

    CmdArgs cmdArgs = {
            .source = url,
            .dest = dir,
            .extraArgs = extraArgs,
            .standalone = standalone,
            .mute = mute
    };

    StatusManager statusManager;
    auto hlsMuxer = new HlsMuxer(&cmdArgs);
    hlsMuxer->statusManager = &statusManager;
    hlsMuxer->start();

    httpCli::cleanup();
    return 0;
}
