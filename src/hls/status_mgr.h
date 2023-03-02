//
// Created by smzhkj on 23-3-3.
//

#ifndef MEDIACENTRE_STATUS_MGR_H
#define MEDIACENTRE_STATUS_MGR_H

#include <string>
#include <map>
#include <any>
#include <nlohmann/json.hpp>

#include "../util/util.h"
#include "../http/httpcli.h"

class StatusManager {

public:
    nlohmann::json send(uint32_t action, std::string& streamId, const std::map<std::string, std::any>* extra_args);
};


#endif //MEDIACENTRE_STATUS_MGR_H
