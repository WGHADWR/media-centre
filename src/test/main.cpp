//
// Created by smzhkj on 23-2-28.
//

#include <iostream>
#include <map>
#include "../util/encrypt.h"
#include "../util/util.h"

using namespace std;

int main() {
    MD5Digest md5Encode("123456");
    cout << md5Encode << endl;
    cout << md5("123456") << endl;

    auto str = R"({ "serverPort": 9000, "url": "/hls/send" })";
    auto data = parse(str);

    for (auto e : data.items()) {
        cout << e.key() << ": " << e.value() << endl;
    }

    std::map<std::string, std::any> extraMap;
    for (auto e : data.items()) {
        any val;
        if (e.value().is_number_integer()) {
            val = e.value().get<int>();
        } else {
            val = e.value().get<string>();
        }
        extraMap.emplace(e.key().c_str(), val);
    }

    for (auto e : extraMap) {
        std::cout << e.first << ": ";
        if (e.second.type() == typeid(int)) {
            std::cout << std::any_cast<int>(e.second);
        } else {
            std::cout << std::any_cast<std::string>(e.second);
        }
        std::cout << std::endl;
    }

    return 0;
}