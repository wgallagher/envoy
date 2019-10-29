#include "extensions/filters/udp/udp_proxy/udp_proxy_filter.h"

#include "envoy/config/filter/udp/udp_proxy/v2alpha/udp_proxy.pb.validate.h"

#include "test/mocks/upstream/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::InSequence;

namespace Envoy {
namespace Extensions {
namespace UdpFilters {
namespace UdpProxy {
namespace {

class UdpProxyFilterTest : public testing::Test {
public:
  struct TestSession {
    TestSession(Event::MockDispatcher& dispatcher)
        : idle_timer_(new Event::MockTimer(&dispatcher)) {}

    Event::MockTimer* idle_timer_;
  };
  using TestSessionPtr = std::unique_ptr<TestSession>;

  UdpProxyFilterTest() {
    // Disable strict mock warnings.
    EXPECT_CALL(callbacks_, udpListener()).Times(AtLeast(0));
  }

  ~UdpProxyFilterTest() {
    EXPECT_CALL(callbacks_.udp_listener_, onDestroy());
  }

  void setup(const std::string& yaml) {
    envoy::config::filter::udp::udp_proxy::v2alpha::UdpProxyConfig config;
    TestUtility::loadFromYamlAndValidate(yaml, config);
    config_ = std::make_shared<UdpProxyFilterConfig>(cluster_manager_, time_system_, config);
    filter_ = std::make_unique<UdpProxyFilter>(callbacks_, config_);
  }

  void recvData(const std::string& peer_address, const std::string& local_address,
                                    const std::string& buffer) {
    Network::UdpRecvData data;
    data.addresses_.peer_ = Network::Utility::parseInternetAddressAndPort(peer_address);
    data.addresses_.local_ = Network::Utility::parseInternetAddressAndPort(local_address);
    data.buffer_ = std::make_unique<Buffer::OwnedImpl>(buffer);
    data.receive_time_ = MonotonicTime(std::chrono::seconds(0));
    filter_->onData(data);
  }

  void expectSessionCreate() {
    EXPECT_CALL(cluster_manager_, get(_));
    test_sessions_.emplace_back(std::make_unique<TestSession>(callbacks_.udp_listener_.dispatcher_));
  }

  Upstream::MockClusterManager cluster_manager_;
  MockTimeSystem time_system_;
  UdpProxyFilterConfigSharedPtr config_;
  Network::MockUdpReadFilterCallbacks callbacks_;
  std::unique_ptr<UdpProxyFilter> filter_;
  std::vector<TestSessionPtr> test_sessions_;
};

// fixfix
TEST_F(UdpProxyFilterTest, BasicFlow) {
  InSequence s;

  setup(R"EOF(
cluster: fake_cluster
  )EOF");

  expectSessionCreate();
  recvData("10.0.0.1:1000", "10.0.0.2:80", "hello");
}

}
}
}
}
}
