#ifndef __HB_NETWORK_H__
#define __HB_NETWORK_H__

namespace honeybase
{
class Network
{
public:
    static bool Startup(const unsigned listenPort);

    static void Shutdown();
};
}

#endif  //__HB_NETWORK_H__