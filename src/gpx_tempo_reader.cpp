#include "libgp_parser/gpx_tempo_reader.hpp"
#include "libgp_parser/gpx_xml.hpp"
#include "libgp_parser/tempo.hpp"

#include <pugixml.hpp>

#include <optional>
#include <string>

namespace libgp_parser {
    namespace {

        using namespace gpx_xml;

        pugi::xml_node find_master_track(const pugi::xml_document &doc) {
            const pugi::xml_node root = doc.document_element();
            if (!root) {
                return {};
            }
            return direct_child(root, "MasterTrack");
        }

        std::optional<int> parse_tempo_bpm_from_value(const std::string &value_text) {
            if (value_text.empty()) {
                return std::nullopt;
            }

            const std::vector<int> parts = parse_int_list(value_text);
            if (parts.size() >= 2) {
                return apply_gpx_tempo_modifier(parts[0], parts[1]);
            }
            if (parts.size() == 1) {
                // GP6: <Value>120.000000</Value> (single BPM, no note-value modifier)
                return parts[0];
            }

            return std::nullopt;
        }

        std::optional<int> parse_tempo_from_automation(const pugi::xml_node &automation) {
            if (std::string(automation.child_value("Type")) != "Tempo") {
                return std::nullopt;
            }

            return parse_tempo_bpm_from_value(direct_child_text(automation, "Value"));
        }

    } // namespace

    ParseResult<int> parse_gpx_initial_tempo(const std::string_view xml) {
        pugi::xml_document doc;
        const pugi::xml_parse_result parsed =
            doc.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parsed) {
            return ParseResult<int>::failure(
                {.code = ParseErrorCode::Xml,
                 .message = std::string("XML parse error: ") + parsed.description()});
        }

        const pugi::xml_node master = find_master_track(doc);
        if (!master) {
            return ParseResult<int>::failure(
                {.code = ParseErrorCode::NotFound, .message = "MasterTrack element not found"});
        }

        const pugi::xml_node automations = direct_child(master, "Automations");
        if (!automations) {
            return ParseResult<int>::failure(
                {.code = ParseErrorCode::NotFound, .message = "No Tempo automation found"});
        }

        // Same rule as GPXDocument.getAutomation("Tempo", 0): latest automation with
        // bar <= 0.
        constexpr int k_first_bar = 0;
        std::optional<int> best_bar;
        std::optional<int> best_bpm;

        for (pugi::xml_node node : automations.children("Automation")) {
            const auto bpm = parse_tempo_from_automation(node);
            if (!bpm.has_value()) {
                continue;
            }

            const int bar = child_int(node, "Bar", 0);
            if (bar > k_first_bar) {
                continue;
            }
            if (!best_bar.has_value() || bar > best_bar.value()) {
                best_bar = bar;
                best_bpm = bpm;
            }
        }

        if (!best_bpm.has_value()) {
            return ParseResult<int>::failure(
                {.code = ParseErrorCode::NotFound, .message = "No Tempo automation found"});
        }

        return ParseResult<int>::success(best_bpm.value());
    }

} // namespace libgp_parser
