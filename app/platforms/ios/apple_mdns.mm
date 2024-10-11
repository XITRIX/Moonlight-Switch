#include <GameStreamClient.hpp>
#import <Foundation/Foundation.h>
#include <netinet/in.h>
#include <arpa/inet.h>

std::vector<Host> foundHosts;
std::function<void(void)> _callback;

@interface MDNSManager : NSObject <NSNetServiceBrowserDelegate, NSNetServiceDelegate>

- (id) init;
- (void) searchForHosts;
- (void) stopSearching;
- (void) forgetHosts;

@end

@implementation MDNSManager {
    NSNetServiceBrowser* mDNSBrowser;
    NSMutableArray* services;
    BOOL scanActive;
    BOOL timerPending;
}

static NSString* NV_SERVICE_TYPE = @"_nvstream._tcp";

- (id) init {
    self = [super init];

    scanActive = FALSE;

    mDNSBrowser = [[NSNetServiceBrowser alloc] init];
    [mDNSBrowser setDelegate:self];

    services = [[NSMutableArray alloc] init];

    return self;
}

- (void) searchForHosts {
    if (scanActive) {
        return;
    }

    brls::Logger::info("Starting mDNS discovery");
    scanActive = TRUE;

    if (!timerPending) {
        timerPending = TRUE;

        // Just invoke the timer callback to save a little code
        [self startSearchTimerCallback:nil];
    }
}

- (void) stopSearching {
    if (!scanActive) {
        return;
    }

    brls::Logger::info("Stopping mDNS discovery");
    scanActive = FALSE;
    [mDNSBrowser stop];
}

- (void) forgetHosts {
    [services removeAllObjects];
}

+ (NSString*)sockAddrToString:(NSData*)addrData {
    char addrStr[INET6_ADDRSTRLEN];
    struct sockaddr* addr = (struct sockaddr*)[addrData bytes];
    if (addr->sa_family == AF_INET) {
        inet_ntop(addr->sa_family, &((struct sockaddr_in*)addr)->sin_addr, addrStr, sizeof(addrStr));
        unsigned short port = ntohs(((struct sockaddr_in*)addr)->sin_port);
        return [NSString stringWithFormat: @"%s:%u", addrStr, port];
    }
    else {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        inet_ntop(addr->sa_family, &sin6->sin6_addr, addrStr, sizeof(addrStr));
        unsigned short port = ntohs(((struct sockaddr_in6*)addr)->sin6_port);
        if (sin6->sin6_scope_id != 0) {
            // Link-local addresses with scope IDs are special
            return [NSString stringWithFormat: @"[%s%%%u]:%u", addrStr, sin6->sin6_scope_id, port];
        }
        else {
            return [NSString stringWithFormat: @"[%s]:%u", addrStr, port];
        }
    }
}

+ (BOOL)isAddress:(uint8_t*)address inSubnet:(uint8_t*)subnet netmask:(int)bits {
    for (int i = 0; i < bits; i++) {
        uint8_t mask = 1 << (i % 8);
        if ((address[i / 8] & mask) != (subnet[i / 8] & mask)) {
            return NO;
        }
    }
    return YES;
}

+ (BOOL)isLocalIpv6Address:(NSData*)addrData {
    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)[addrData bytes];
    if (sin6->sin6_family != AF_INET6) {
        return NO;
    }

    uint8_t* addrBytes = sin6->sin6_addr.s6_addr;
    uint8_t prefix[2];

    // fe80::/10
    prefix[0] = 0xfe;
    prefix[1] = 0x80;
    if ([MDNSManager isAddress:addrBytes inSubnet:prefix netmask:10]) {
        // Link-local
        return YES;
    }

    // fec0::/10
    prefix[0] = 0xfe;
    prefix[1] = 0xc0;
    if ([MDNSManager isAddress:addrBytes inSubnet:prefix netmask:10]) {
        // Site local
        return YES;
    }

    // fc00::/7
    prefix[0] = 0xfc;
    prefix[1] = 0x00;
    if ([MDNSManager isAddress:addrBytes inSubnet:prefix netmask:7]) {
        // ULA
        return YES;
    }

    return NO;
}

+ (NSString*)getBestIpv6Address:(NSArray<NSData*>*)addresses {
    for (NSData* addrData in addresses) {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)[addrData bytes];
        if (sin6->sin6_family != AF_INET6) {
            continue;
        }

        if ([MDNSManager isLocalIpv6Address:addrData]) {
            // Skip non-global addresses
            continue;
        }

        uint8_t* addrBytes = sin6->sin6_addr.s6_addr;
        uint8_t prefix[2];

        // 2002::/16
        prefix[0] = 0x20;
        prefix[1] = 0x02;
        if ([MDNSManager isAddress:addrBytes inSubnet:prefix netmask:16]) {
//            brls::Logger::info("Ignoring 6to4 address: {}", [MDNSManager sockAddrToString:addrData]);
            continue;
        }

        // 2001::/32
        prefix[0] = 0x20;
        prefix[1] = 0x01;
        if ([MDNSManager isAddress:addrBytes inSubnet:prefix netmask:32]) {
//            brls::Logger::info("Ignoring Teredo address: {}", [MDNSManager sockAddrToString:addrData]);
            continue;
        }

        return [MDNSManager sockAddrToString:addrData];
    }

    return nil;
}

- (void)netServiceDidResolveAddress:(NSNetService *)service {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSArray<NSData*>* addresses = [service addresses];

        for (NSData* addrData in addresses) {
//            brls::Logger::info("Resolved address: {} -> {}", [service hostName], [MDNSManager sockAddrToString: addrData]);
        }

        Host host;
        NSString* tempIp;

        // First, look for an IPv4 record for the local address
        for (NSData* addrData in addresses) {
            struct sockaddr_in* sin = (struct sockaddr_in*)[addrData bytes];
            if (sin->sin_family != AF_INET) {
                continue;
            }

            tempIp = [MDNSManager sockAddrToString:addrData];
//            brls::Logger::info("Local address chosen: {} -> {}", [service hostName], host.localAddress);
            break;
        }

        if (tempIp == nil) {
            // If we didn't find an IPv4 record, look for a local IPv6 record
            for (NSData* addrData in addresses) {
                if ([MDNSManager isLocalIpv6Address:addrData]) {
                    tempIp = [MDNSManager sockAddrToString:addrData];
//                    brls::Logger::info("Local address chosen: {} -> {}", [service hostName], host.localAddress);
                    break;
                }
            }
        }

//        host.ipv6Address = [MDNSManager getBestIpv6Address:addresses];
//        brls::Logger::info("IPv6 address chosen: {} -> {}", [service hostName], host.ipv6Address);

//        host.activeAddress = host.localAddress;
        auto hostAddress = [tempIp componentsSeparatedByString:@":"];
        NSString* ipAddress = hostAddress[0];
        NSString* port = hostAddress[1];

        auto hostName = [service.hostName stringByReplacingOccurrencesOfString:@".local." withString:@""];

        host.address = std::string([ipAddress UTF8String]);
        host.hostname = std::string([hostName UTF8String]);
        foundHosts.push_back(host);

        _callback();
    });
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict {
//    brls::Logger::warning("Did not resolve address for: {}\n{}", sender, [errorDict description]);

    // Schedule a retry in 2 seconds
    [NSTimer scheduledTimerWithTimeInterval:2.0
                                     target:self
                                   selector:@selector(retryResolveTimerCallback:)
                                   userInfo:sender
                                    repeats:NO];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
//    brls::Logger::debug("Found service: {}", aNetService);

    if (![services containsObject:aNetService]) {
        brls::Logger::info("Found new host: {}", [aNetService.name cStringUsingEncoding:NSUTF8StringEncoding]);
        [aNetService setDelegate:self];
        [aNetService resolveWithTimeout:5];
        [services addObject:aNetService];
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
//    brls::Logger::info("Removing service: {}", aNetService);
    [services removeObject:aNetService];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didNotSearch:(NSDictionary *)errorDict {
//    brls::Logger::warning("Did not perform search: \n{}", [errorDict description]);

    // We'll schedule a retry in startSearchTimerCallback
}

- (void)startSearchTimerCallback:(NSTimer *)timer {
    // Check if we've been stopped since this was queued
    if (!scanActive) {
        timerPending = FALSE;
        return;
    }

    brls::Logger::debug("Restarting mDNS search");
    [mDNSBrowser stop];
    [mDNSBrowser searchForServicesOfType:NV_SERVICE_TYPE inDomain:@""];

    // Search again in 5 seconds. We need to do this because
    // we want more aggressive querying than Bonjour will normally
    // do for when we're at the hosts screen. This also covers scenarios
    // where discovery didn't work, like if WiFi was disabled.
    [NSTimer scheduledTimerWithTimeInterval:5.0
                                     target:self
                                   selector:@selector(startSearchTimerCallback:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)retryResolveTimerCallback:(NSTimer *)timer {
    // Check if we've been stopped since this was queued
    if (!scanActive) {
        return;
    }

    NSNetService* service = timer.userInfo;
//    brls::Logger::info("Retrying mDNS resolution for {}", service);

    if (service.hostName == nil) {
        [service setDelegate:self];
        [service resolveWithTimeout:5];
    }
}

@end

MDNSManager* mDNSManager = [[MDNSManager alloc] init];

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void darwin_mdns_start(ServerCallback<std::vector<Host>>& callback) {
    _callback = [callback]() {
        brls::sync([callback] {
            callback(GSResult<std::vector<Host>>::success(foundHosts));
        });
    };
    [mDNSManager searchForHosts];
}

void darwin_mdns_stop() {
    [mDNSManager stopSearching];
    [mDNSManager forgetHosts];
    foundHosts.clear();
}
