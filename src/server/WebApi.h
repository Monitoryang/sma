#ifndef WEBAPI_H
#define WEBAPI_H

#include "WebApiBasic.h"

void installWebApi(const Poco::Net::ServerSocket& socket);

void unInstallWebApi();

#endif // !WEBAPI_H