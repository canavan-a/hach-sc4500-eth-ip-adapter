#include <MessageRouter.h>
#include <utils/Buffer.h>

using eipScanner::SessionInfo;
using eipScanner::MessageRouter;
using namespace eipScanner::cip;
using namespace eipScanner::utils;

#include "config.h"

int main() {
    auto si = std::make_shared<SessionInfo>(config::ipAddress, 0xAF12);
    MessageRouter messageRouter;

    // class=0x04, instance=100, attribute=0x03 — same as your Python call
    auto response = messageRouter.sendRequest(si,
        ServiceCodes::GET_ATTRIBUTE_SINGLE,
        EPath(0x04, 100, 0x03),
        {});

    if (response.getGeneralStatusCode() == GeneralStatusCodes::SUCCESS) {
        Buffer buf(response.getData());
        CipReal f0, f1;
        buf >> f0 >> f1;
        printf("tag0=%.3f  tag1=%.3f\n", f0, f1);
    }
    return 0;
}
