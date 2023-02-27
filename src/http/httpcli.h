//
// Created by cyy on 2023/1/6.
//

#ifndef VIDEOPLAYER_HTTP_H
#define VIDEOPLAYER_HTTP_H

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "curl/curl.h"

namespace httpCli {

class RequestError : public std::exception {
private:
    std::string message;
public:
    explicit RequestError(std::string msg): std::exception(), message(std::move(msg)) {}
    const char* what() {
        return message.c_str();
    }
};

    typedef struct Response {
        int statusCode;
        const char* statusText;
        const char* body;
    } Response;

    static size_t write_response(void *ptr, size_t size, size_t nmemb, void *userData)
    {
        auto* pBuffer = (std::string*)userData;
        size_t length = size * nmemb;
        pBuffer->append((char*)ptr, length);
        return length;
    }

    static void global_init() {
        curl_global_init(CURL_GLOBAL_ALL);
    }

    static Response* _request(const char* url, const char* method, const char* data, const char* content_type, int timeout = 5) {
        auto curl = curl_easy_init();
        if (!curl) {
            return nullptr;
        }

        static std::string result;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, FALSE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "cache-control: no-cache");
        if (content_type) {
            char ct[255];
            strcat(ct, content_type);
            headers = curl_slist_append(headers, ct);
        }
        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        if (strcmp(method, "POST") == 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        auto code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            std::cerr << "Connect to " << url << " failed, response: " << code << ", " << curl_easy_strerror(code) << std::endl;
            return nullptr;
        }
        int http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        static auto res = new Response{
                .statusCode = http_code,
                .statusText = curl_easy_strerror(code),
                .body = result.c_str(),
        };
        return res;
    }

    static Response* get(const std::string& url, int timeout = 5) {
        static auto res = _request(url.c_str(), "GET", nullptr, nullptr, timeout);
        return res;
    }

    static Response* post(const char* url, const char* data, const char* contentType = "application/x-www-form-urlencoded", int timeout = 5) {
        static auto res = _request(url, "POST", data, contentType, timeout);
        return res;
    }

    static void cleanup() {
        curl_global_cleanup();
    }
}

#endif //VIDEOPLAYER_HTTP_H
