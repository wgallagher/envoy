#pragma once

#include "envoy/config/core/v3alpha/config_source.pb.h"

#include "common/protobuf/protobuf.h"

// Convenience macro for downgrading a message and obtaining a reference.
#define API_DOWNGRADE(msg) (*Config::VersionConverter::downgrade(msg)->msg_)

namespace Envoy {
namespace Config {

// An instance of a dynamic message from a DynamicMessageFactory.
struct DynamicMessage {
  // The dynamic message factory must outlive the message.
  Protobuf::DynamicMessageFactory dynamic_msg_factory_;

  // Dynamic message.
  std::unique_ptr<Protobuf::Message> msg_;
};

using DynamicMessagePtr = std::unique_ptr<DynamicMessage>;

class VersionConverter {
public:
  /**
   * Upgrade a message from an earlier to later version of the Envoy API. This
   * performs a simple wire-level reinterpretation of the fields. As a result of
   * shadow protos, earlier deprecated fields such as foo are materialized as
   * hidden_envoy_deprecated_foo.
   *
   * This should be used when you have wire input (e.g. bootstrap, xDS, some
   * opaque config) that might be at any supported version and want to upgrade
   * to Envoy's internal latest API usage.
   *
   * @param prev_message previous version message input.
   * @param next_message next version message to generate.
   */
  static void upgrade(const Protobuf::Message& prev_message, Protobuf::Message& next_message);

  /**
   * Downgrade a message to the previous version. If no previous version exists,
   * the given message is copied in the return value. This is not super
   * efficient, most uses are expected to be tests and performance agnostic
   * code.
   *
   * This is used primarily in tests, to allow tests to internally use the
   * latest supported API but ensure that earlier versions are used on the wire.
   *
   * @param message message input.
   * @return DynamicMessagePtr with the downgraded message (and associated
   *         factory state).
   */
  static DynamicMessagePtr downgrade(const Protobuf::Message& message);

  /**
   * Reinterpret an Envoy internal API message at v3 based on a given transport API
   * version. This will downgrade() to an earlier version or scrub the shadow
   * deprecated fields in the existing one.
   *
   * This is typically used when Envoy is generating a wire message from some
   * internally generated message, e.g. DiscoveryRequest, and we want to ensure
   * it matches a specific API version. For example, a v3 DiscoveryRequest must
   * have any deprecated v2 fields removed (they only exist because of
   * shadowing) and a v2 DiscoveryRequest needs to have type
   * envoy.api.v2.DiscoveryRequest to ensure JSON representations have the
   * correct field names (after renames/deprecations are reversed).
   *
   * @param message message input.
   * @param api_version target API version.
   * @return DynamicMessagePtr with the reinterpreted message (and associated
   *         factory state).
   */
  static DynamicMessagePtr reinterpret(const Protobuf::Message& message,
                                       envoy::config::core::v3alpha::ApiVersion api_version);
};

class VersionUtil {
public:
  // Some helpers for working with earlier message version deprecated fields.
  static bool hasHiddenEnvoyDeprecated(const Protobuf::Message& message);
  static void scrubHiddenEnvoyDeprecated(Protobuf::Message& message);
};

} // namespace Config
} // namespace Envoy
