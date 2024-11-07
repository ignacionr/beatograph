#pragma once

#include <string>
#include <vector>

#include <tinyxml2.h>

namespace rss {
    struct feed {

        struct item {
            std::string title;
            std::string link;
            std::string description;
            std::string enclosure;
        };

        void operator()(std::string_view partial_contents) {
            contents += partial_contents;
            tinyxml2::XMLDocument doc;
            doc.Parse(contents.c_str());
            if (doc.Error()) {
                // the document might be incomplete, so we ignore the error
            }
            else {
                (*this)(doc);
            }
        }
        void operator()(tinyxml2::XMLDocument const &doc) {
            auto root = doc.FirstChildElement("rss");
            if (root) {
                auto channel = root->FirstChildElement("channel");
                if (channel) {
                    auto title = channel->FirstChildElement("title");
                    if (title) {
                        feed_title = title->GetText();
                    }
                    auto link = channel->FirstChildElement("link");
                    if (link) {
                        feed_link = link->GetText();
                    }
                    auto description = channel->FirstChildElement("description");
                    if (description) {
                        feed_description = description->GetText();
                    }
                    auto image = channel->FirstChildElement("image");
                    if (image) {
                        auto url = image->FirstChildElement("url");
                        if (url) {
                            feed_image_url = url->GetText();
                        }
                    }
                    auto xml_item = channel->FirstChildElement("item");
                    while (xml_item) {
                        item new_item;
                        title = xml_item->FirstChildElement("title");
                        if (title) {
                            new_item.title = title->GetText();
                        }
                        link = xml_item->FirstChildElement("link");
                        if (link) {
                            new_item.link = link->GetText();
                        }
                        description = xml_item->FirstChildElement("description");
                        if (description && !description->NoChildren()) {
                            new_item.description = description->GetText();
                        }
                        auto enclosure = xml_item->FirstChildElement("enclosure");
                        if (enclosure) {
                            new_item.enclosure = enclosure->Attribute("url");
                        }
                        items.emplace_back(std::move(new_item));
                        xml_item = xml_item->NextSiblingElement("item");
                    }
                }
            }
        }

        std::string contents;
        std::string feed_title;
        std::string feed_link;
        std::string feed_description;
        std::string feed_image_url;
        std::vector<item> items;
    };
}