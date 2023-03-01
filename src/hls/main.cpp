//
// Created by cyy on 2023/1/6.
//

#include <iostream>
#include <vector>

#include "hls.h"

void define_options(cmdline::parser *parser) {
    parser->add<std::string>("source", 's', "media source", true, "rtsp://127.0.0.1:8554/stream");
    parser->add<std::string>("dir", 'd', "target dir", false, ".");
    parser->add<std::string>("ext", 'x', "extends options, json", false, "{}");
}

std::map<std::string, std::any>* parse_extra_args(std::string& args) {
    nlohmann::json data = parse(args);
    if (data.empty()) {
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

int main(int argv, char *args[]) {
    cmdline::parser parser;
    define_options(&parser);

    httpCli::global_init();

    parser.parse_check(argv, args);
    auto url = parser.get<std::string>("source");
    auto dir = parser.get<std::string>("dir");
    auto ext = parser.get<std::string>("ext");

    if (url.length() == 0) {
        url = "rtsp://192.168.2.46:8554/test2";
    }
    if (dir.length() == 0) {
        dir = "D:/ts";
#ifdef linux
        dir = "./ts";
#endif
    }

    sm_log("HlsMuxer args, source: {}, ext: {}, target dir: {}", url, ext, dir);

    /*nlohmann::json ext_args = nullptr; // parse(ext);
    if (!ext.empty()) {
        ext_args = parse(ext);
    }*/
    std::map<std::string, std::any> *extra = parse_extra_args(ext);

    auto hlsMuxer = new HlsMuxer(url.c_str(), dir.c_str(), extra);
    hlsMuxer->start();

    httpCli::cleanup();
    return 0;
}
