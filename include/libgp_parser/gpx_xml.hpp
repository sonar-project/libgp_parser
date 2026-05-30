#pragma once

#include <pugixml.hpp>

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

/// XML helpers for score.gpif (equivalent to GPXDocumentReader getChildNode*).
namespace libgp_parser::gpx_xml {

    /// Parses an integer from a bounded string view (no null terminator required).
    [[nodiscard]] inline std::optional<int> parse_int(std::string_view text) noexcept {
        if (text.empty()) {
            return std::nullopt;
        }
        int value{};
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ptr == text.data()) {
            return std::nullopt;
        }
        if (ec == std::errc::result_out_of_range) {
            return value;
        }
        if (ec != std::errc{}) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] inline int parse_int_or(std::string_view text, const int default_value) noexcept {
        return parse_int(text).value_or(default_value);
    }

    /// Finds a direct child element by name.
    /// @par TuxGuitar source GPXDocumentReader.getChildNode().
    /// @par Brief First matching child or empty node.
    /// @par Visibility public (internal use, in header).
    [[nodiscard]] inline pugi::xml_node direct_child(const pugi::xml_node &parent,
                                                     std::string_view name) {
        for (pugi::xml_node child : parent.children()) {
            if (std::string_view(child.name()) == name) {
                return child;
            }
        }
        return {};
    }

    /// Reads text of a direct child element.
    /// @par TuxGuitar source getChildNodeContent().
    /// @par Brief Empty string when missing.
    /// @par Visibility public (internal use).
    [[nodiscard]] inline std::string direct_child_text(const pugi::xml_node &parent,
                                                       std::string_view name) {
        const pugi::xml_node child = direct_child(parent, name);
        return (child != nullptr) ? child.child_value() : std::string{};
    }

    [[nodiscard]] inline int attribute_int(const pugi::xml_node &node, std::string_view name,
                                           int default_value = 0) {
        for (pugi::xml_attribute attr : node.attributes()) {
            if (std::string_view(attr.name()) == name) {
                const std::string_view value{attr.value()};
                if (value.empty()) {
                    return default_value;
                }
                return parse_int(value).value_or(0);
            }
        }
        return default_value;
    }

    [[nodiscard]] inline int child_int(const pugi::xml_node &parent, std::string_view name,
                                       int default_value = 0) {
        const std::string text = direct_child_text(parent, name);
        if (text.empty()) {
            return default_value;
        }
        return parse_int(text).value_or(0);
    }

    [[nodiscard]] inline std::vector<int> parse_int_list(std::string_view text) {
        std::vector<int> values;
        std::size_t start = 0;
        while (start < text.size()) {
            const std::size_t end = text.find_first_of(" \t\r\n", start);
            const std::size_t token_end = (end == std::string_view::npos) ? text.size() : end;
            if (token_end > start) {
                const auto token = text.substr(start, token_end - start);
                values.push_back(parse_int_or(token, 0));
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        return values;
    }

} // namespace libgp_parser::gpx_xml
