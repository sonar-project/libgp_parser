#pragma once

#include <string>
#include <utility>
#include <vector>

namespace libgp_parser {

    inline constexpr int kDefaultPercussionBank = 128;
    inline constexpr int kDefaultBank = 0;
    inline constexpr int kDefaultProgram = 25;
    inline constexpr int kDefaultVolume = 127;
    inline constexpr int kDefaultBalance = 64;

    inline constexpr const char *kGmChannel1Key = "gm-channel-1";
    inline constexpr const char *kGmChannel2Key = "gm-channel-2";

    /// Mixer channel parameter (TGChannelParameter).
    struct ChannelParameter {
        std::string key;
        std::string value;

        [[nodiscard]] bool operator==(const ChannelParameter &) const = default;
    };

    /// GM mixer channel (TGChannel).
    /// @par TuxGuitar source common/TuxGuitar-lib/.../TGChannel.java.
    struct Channel {
        int channel_id{0};
        int bank{kDefaultBank};
        int program{kDefaultProgram};
        int volume{kDefaultVolume};
        int balance{kDefaultBalance};
        int chorus{0};
        int reverb{0};
        int phaser{0};
        int tremolo{0};
        std::string name;
        std::vector<ChannelParameter> parameters;

        [[nodiscard]] bool is_percussion() const noexcept {
            return bank == kDefaultPercussionBank;
        }

        [[nodiscard]] bool operator==(const Channel &) const = default;
    };

    [[nodiscard]] inline const ChannelParameter *find_parameter(const Channel &channel,
                                                                const char *key) {
        for (const ChannelParameter &parameter : channel.parameters) {
            if (parameter.key == key) {
                return &parameter;
            }
        }
        return nullptr;
    }

    inline void set_parameter(Channel &channel, std::string key, std::string value) {
        for (ChannelParameter &parameter : channel.parameters) {
            if (parameter.key == key) {
                parameter.value = std::move(value);
                return;
            }
        }
        channel.parameters.push_back(
            ChannelParameter{.key = std::move(key), .value = std::move(value)});
    }

} // namespace libgp_parser
