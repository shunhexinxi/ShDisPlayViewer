#pragma once

#include "modules/zqbs/FlightRouteTypes.h"

namespace Zqxxxt {

class FlightRouteXmlStore
{
public:
    static bool load(const QString &filePath, FlightRouteDocument &document,
                     QString &errorMessage);
    static bool save(const QString &filePath, const FlightRouteDocument &document,
                     QString &errorMessage);
};

}
