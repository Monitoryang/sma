#include "WebHook.h"
#include "OnceToken.h"
#include "Config.h"

#include "Poco/URI.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/TaskManager.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/ThreadPool.h"

#include <string>
#include <functional>

namespace Hook {
#define HOOK_FIELD "hook."
    const std::string kOnExceptionHappen = HOOK_FIELD "on_exception_happen";

   /* static eap::onceToken token([]() {
        eap::configInstance().setString(kOnExceptionHappen, "http://127.0.0.1/index/api/on_exception_happen");
    });*/
} // namespace Hook

void installWebHook()
{
    
}

void unInstallWebHook()
{

}