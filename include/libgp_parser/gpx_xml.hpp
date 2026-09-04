#pragma once

#include <pugixml.hpp>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

/// XML helpers for score.gpif (equivalent to GPXDocumentReader getChildNode*).
namespace libgp_parser::gpx_xml {

    /// Builds an XML error message with pugixml offset and a short snippet.
    [[nodiscard]] inline std::string format_xml_parse_error(const pugi::xml_parse_result &parsed,
                                                            std::string_view xml) {
        std::string message = std::string("XML parse error: ") + parsed.description();
        message += " at offset ";
        message += std::to_string(parsed.offset);
        if (!xml.empty() && parsed.offset >= 0) {
            const auto offset = static_cast<std::size_t>(parsed.offset);
            if (offset < xml.size()) {
                constexpr std::size_t kRadius = 24;
                const std::size_t begin = offset > kRadius ? offset - kRadius : 0;
                const std::size_t end = std::min(xml.size(), offset + kRadius);
                message += " near \"";
                for (std::size_t i = begin; i < end; ++i) {
                    const char ch = xml[i];
                    if (ch == '\n' || ch == '\r' || ch == '\t') {
                        message.push_back(' ');
                    } else if (static_cast<unsigned char>(ch) < 0x20 ||
                               static_cast<unsigned char>(ch) == 0x7f) {
                        message.push_back('.');
                    } else {
                        message.push_back(ch);
                    }
                }
                message += "\"";
            }
        }
        return message;
    }

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

    [[nodiscard]] inline std::vector<int> parse_int_list(std::string_view text,
                                                         std::string_view delimiters) {
        std::vector<int> values;
        std::size_t start = 0;
        while (start < text.size()) {
            const std::size_t end = text.find_first_of(delimiters, start);
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

    [[nodiscard]] inline std::vector<int> parse_int_list(std::string_view text) {
        return parse_int_list(text, " \t\r\n");
    }

    [[nodiscard]] inline std::vector<int> child_int_list(const pugi::xml_node &parent,
                                                         std::string_view name,
                                                         std::string_view delimiters = " \t\r\n") {
        return parse_int_list(direct_child_text(parent, name), delimiters);
    }

    [[nodiscard]] inline bool child_bool(const pugi::xml_node &parent, std::string_view name) {
        return direct_child_text(parent, name) == "true";
    }

    [[nodiscard]] inline bool attribute_bool(const pugi::xml_node &node, std::string_view name) {
        for (pugi::xml_attribute attr : node.attributes()) {
            if (std::string_view(attr.name()) == name) {
                return std::string_view(attr.value()) == "true";
            }
        }
        return false;
    }

    [[nodiscard]] inline std::string attribute_string(const pugi::xml_node &node,
                                                      std::string_view name) {
        for (pugi::xml_attribute attr : node.attributes()) {
            if (std::string_view(attr.name()) == name) {
                return attr.value();
            }
        }
        return {};
    }

} // namespace libgp_parser::gpx_xml
