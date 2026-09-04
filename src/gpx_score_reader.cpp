#include "libgp_parser/gpx_score_reader.hpp"
#include "libgp_parser/gpx_xml.hpp"

#include <pugixml.hpp>

#include <string>

namespace libgp_parser {
    namespace {

        using namespace gpx_xml;

        void fill_metadata_from_score_node(const pugi::xml_node &score, SongMetadata &metadata) {
            metadata.title = direct_child_text(score, "Title");
            metadata.subtitle = direct_child_text(score, "SubTitle");
            metadata.artist = direct_child_text(score, "Artist");
            metadata.album = direct_child_text(score, "Album");
            metadata.words = direct_child_text(score, "Words");
            metadata.music = direct_child_text(score, "Music");
            metadata.words_and_music = direct_child_text(score, "WordsAndMusic");
            metadata.copyright = direct_child_text(score, "Copyright");
            metadata.tabber = direct_child_text(score, "Tabber");
            metadata.instructions = direct_child_text(score, "Instructions");
            metadata.notices = direct_child_text(score, "Notices");
        }

        pugi::xml_node find_score_node(const pugi::xml_document &doc) {
            const pugi::xml_node root = doc.document_element();
            if (!root) {
                return {};
            }
            return direct_child(root, "Score");
        }

    } // namespace

    ParseResult<SongMetadata> parse_gpx_score_metadata(const std::string_view xml) {
        pugi::xml_document doc;
        const pugi::xml_parse_result parsed =
            doc.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_auto);
        if (!parsed) {
            return ParseResult<SongMetadata>::failure(
                {.code = ParseErrorCode::Xml,
                 .message = gpx_xml::format_xml_parse_error(parsed, xml)});
        }

        const pugi::xml_node score = find_score_node(doc);
        if (!score) {
            return ParseResult<SongMetadata>::failure(
                {.code = ParseErrorCode::NotFound, .message = "Score element not found"});
        }

        SongMetadata metadata;
        fill_metadata_from_score_node(score, metadata);
        return ParseResult<SongMetadata>::success(std::move(metadata));
    }

} // namespace libgp_parser
