#pragma once

#include "envoy/config/filter/udp/udp_proxy/v2alpha/udp_proxy.pb.h"
#include "envoy/event/file_event.h"
#include "envoy/event/timer.h"
#include "envoy/network/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/network/utility.h"

#include "absl/container/flat_hash_set.h"

namespace Envoy {
namespace Extensions {
namespace UdpFilters {
namespace UdpProxy {

class UdpProxyFilterConfig {
public:
  UdpProxyFilterConfig(Upstream::ClusterManager& cluster_manager, TimeSource& time_source,
                       const envoy::config::filter::udp::udp_proxy::v2alpha::UdpProxyConfig& config)
      : cluster_manager_(cluster_manager), time_source_(time_source), config_(config) {}

  Upstream::ThreadLocalCluster* getCluster() const {
    return cluster_manager_.get(config_.cluster());
  }
  TimeSource& timeSource() const { return time_source_; }

private:
  Upstream::ClusterManager& cluster_manager_;
  TimeSource& time_source_;
  const envoy::config::filter::udp::udp_proxy::v2alpha::UdpProxyConfig config_;
};

using UdpProxyFilterConfigSharedPtr = std::shared_ptr<const UdpProxyFilterConfig>;

class UdpProxyFilter : public Network::UdpListenerReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  UdpProxyFilter(Network::UdpReadFilterCallbacks& callbacks,
                 const UdpProxyFilterConfigSharedPtr& config)
      : UdpListenerReadFilter(callbacks), config_(config) {}

  // Network::UdpListenerReadFilter
  void onData(Network::UdpRecvData& data) override;

private:
  class ActiveSession : public Network::UdpPacketProcessor {
  public:
    ActiveSession(UdpProxyFilter& parent, Network::UdpRecvData::LocalPeerAddresses&& addresses,
                  const Upstream::HostConstSharedPtr& host);
    const Network::UdpRecvData::LocalPeerAddresses& addresses() { return addresses_; }
    void write(const Buffer::Instance& buffer);

  private:
    void onIdleTimer();
    void onReadReady();

    // Network::UdpPacketProcessor
    void processPacket(Network::Address::InstanceConstSharedPtr local_address,
                       Network::Address::InstanceConstSharedPtr peer_address,
                       Buffer::InstancePtr buffer, MonotonicTime receive_time) override;
    uint64_t maxPacketSize() const override {
      // TODO(mattklein123): Support configurable/jumbo frames when proxying to upstream.
      // Eventually we will want to support some type of PROXY header when doing L4 QUIC
      // forwarding.
      return Network::MAX_UDP_PACKET_SIZE;
    }

    UdpProxyFilter& parent_;
    const Network::UdpRecvData::LocalPeerAddresses addresses_;
    const Upstream::HostConstSharedPtr host_;
    const Event::TimerPtr idle_timer_;
    const Network::IoHandlePtr io_handle_;
    const Event::FileEventPtr socket_event_;
  };

  using ActiveSessionPtr = std::unique_ptr<ActiveSession>;

  struct HeterogeneousActiveSessionHash {
    // Specifying is_transparent indicates to the library infrastructure that
    // type-conversions should not be applied when calling find(), but instead
    // pass the actual types of the contained and searched-for objects directly to
    // these functors. See
    // https://en.cppreference.com/w/cpp/utility/functional/less_void for an
    // official reference, and https://abseil.io/tips/144 for a description of
    // using it in the context of absl.
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    size_t operator()(const Network::UdpRecvData::LocalPeerAddresses& value) const {
      return absl::Hash<const Network::UdpRecvData::LocalPeerAddresses>()(value);
    }
    size_t operator()(const ActiveSessionPtr& value) const {
      return absl::Hash<const Network::UdpRecvData::LocalPeerAddresses>()(value->addresses());
    }
  };

  struct HeterogeneousActiveSessionEqual {
    // See description for HeterogeneousActiveSessionHash::is_transparent.
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    bool operator()(const ActiveSessionPtr& lhs,
                    const Network::UdpRecvData::LocalPeerAddresses& rhs) const {
      return lhs->addresses() == rhs;
    }
    bool operator()(const ActiveSessionPtr& lhs, const ActiveSessionPtr& rhs) const {
      return lhs->addresses() == rhs->addresses();
    }
  };

  const UdpProxyFilterConfigSharedPtr config_;
  absl::flat_hash_set<ActiveSessionPtr, HeterogeneousActiveSessionHash,
                      HeterogeneousActiveSessionEqual>
      sessions_;
};

} // namespace UdpProxy
} // namespace UdpFilters
} // namespace Extensions
} // namespace Envoy
