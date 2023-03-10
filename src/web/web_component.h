//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_WEB_COMPONENT_H
#define VIDEOPLAYER_WEB_COMPONENT_H


#include "oatpp/web/server/AsyncHttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"

#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "oatpp/core/macro/component.hpp"

#include "static_files_manager.h"

/**
 *  Class which creates and holds Application components and registers components in oatpp::base::Environment
 *  Order of components initialization is from top to bottom
 */
class WebComponent {
private:
    int port;
public:
    std::shared_ptr<oatpp::network::ServerConnectionProvider> serverConnectionProvider;

    std::shared_ptr<oatpp::web::server::interceptor::RequestInterceptor> _requestInterceptor;

public:
    explicit WebComponent(int port, oatpp::web::server::interceptor::RequestInterceptor *requestInterceptor) {
        this->port = port;
        this->_requestInterceptor = std::shared_ptr<oatpp::web::server::interceptor::RequestInterceptor>(requestInterceptor);
        this->init();
    }

    void init() {
        this->serverConnectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", static_cast<v_uint16>(this->port), oatpp::network::Address::IP_4});
        this->serverConnectionHandler.getObject()->addRequestInterceptor(this->_requestInterceptor);
    }

    void addRequestInterceptor(oatpp::web::server::interceptor::RequestInterceptor *requestInterceptor) {
        this->_requestInterceptor = std::shared_ptr<oatpp::web::server::interceptor::RequestInterceptor>(requestInterceptor);
    }

    /**
     * Create Async Executor
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::async::Executor>, executor)([] {
        return std::make_shared<oatpp::async::Executor>(
                4 /* Data-Processing threads */,
                1 /* I/O threads */,
                1 /* Timer threads */
        );
    }());

    /**
     *  Create ConnectionProvider component which listens on the port
     */
    /*OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, serverConnectionProvider)([this] {
        *//* non_blocking connections should be used with AsyncHttpConnectionHandler for AsyncIO *//*
        return oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", static_cast<v_uint16>(this->port), oatpp::network::Address::IP_4});
    }());*/

    /**
     *  Create Router component
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, httpRouter)([] {
        return oatpp::web::server::HttpRouter::createShared();
    }());

    /**
     *  Create ConnectionHandler component which uses Router component to route requests
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::AsyncHttpConnectionHandler>, serverConnectionHandler)([] {
        OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router); // get Router component
        OATPP_COMPONENT(std::shared_ptr<oatpp::async::Executor>, executor); // get Async executor component

        auto connectionHandler = oatpp::web::server::AsyncHttpConnectionHandler::createShared(router, executor);
        return connectionHandler;
    }());

    /**
     *  Create ObjectMapper component to serialize/deserialize DTOs in Contoller's API
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)([] {
        auto serializerConfig = oatpp::parser::json::mapping::Serializer::Config::createShared();
        auto deserializerConfig = oatpp::parser::json::mapping::Deserializer::Config::createShared();
        deserializerConfig->allowUnknownFields = false;
        auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared(serializerConfig, deserializerConfig);
        return objectMapper;
    }());

    OATPP_CREATE_COMPONENT(std::shared_ptr<StaticFilesManager>, staticFilesManager)([] {
        return std::make_shared<StaticFilesManager>(WEB_STATIC_FOLDER /* path to '<this-repo>/Media-Stream/video' folder. Put full, absolute path here */) ;
    }());

    /*OATPP_CREATE_COMPONENT(std::shared_ptr<Playlist>, livePlayList)([] {
        auto playlist = Playlist::parseFromFile(EXAMPLE_MEDIA_FOLDER "/playlist_live.m3u8" *//* path to '<this-repo>/Media-Stream/video/playlist_live.m3u8' file. Put full, absolute path here *//*);
        return std::make_shared<Playlist>(playlist);
    }());*/

};

#endif //VIDEOPLAYER_WEB_COMPONENT_H
