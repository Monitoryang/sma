#include "WebApi.h"

void installWebApi(const Poco::Net::ServerSocket& socket)
{
	eap::WebApiBasic::instance()->apiRegist("/hello");

	eap::WebApiBasic::instance()->start(socket);
}

void unInstallWebApi()
{
	eap::WebApiBasic::instance()->stop();
}