//
// Created by cyy on 2023/1/6.
//

#include "server.h"

void HlsSchedule::createTask(const char *url) {

    std::cout << "url: " << url << std::endl;
}

void define_options(cmdline::parser *parser) {
    parser->add<std::string>("source", 's', "media source", true, "rtsp://127.0.0.1:8554/stream");
    parser->add<std::string>("dir", 'd', "segment file dir", false, ".");
    parser->add<std::string>("ext", 'x', "extends options, json", false, "{}");
}

int main(int argv, char *args[]) {
    cmdline::parser parser;
    define_options(&parser);

    httpCli::global_init();

//    parser.parse_check(argv, args);
//    auto url = parser.get<std::string>("source");
//    auto dir = parser.get<std::string>("dir");
//    auto ext = parser.get<std::string>("ext");

    std::string ext = R"({"serverPort":"8080","url":"/hls/muxer/status"})";

//    std::string url = "D:/Pitbull-Give-Me-Everything.mp4";
    std::string url = "rtsp://192.168.2.46:8554/test1";
    std::string dir = "D:/ts";

#ifdef linux
    dir = "./ts";
#endif

    std::cout << "HlsMuxer args, source: " << url;
    std::cout << ", ext: " << ext;
    std::cout << ", dir: " << dir << std::endl;

    nlohmann::json ext_args = nullptr; // parse(ext);

    auto hlsMuxer = new HlsMuxer(url.c_str(), dir.c_str(), &ext_args);
    hlsMuxer->start();

    httpCli::cleanup();
    return 0;
}