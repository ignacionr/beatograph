#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <sstream>
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
            std::string image_url;
            std::chrono::system_clock::time_point updated;
        };

        feed(std::function<std::string(std::string_view)> system_runner) : system_runner_(system_runner) {}

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
            if (auto root = doc.FirstChildElement("rss"); root) {
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
                    else {
                        auto itunes_image = channel->FirstChildElement("itunes:image");
                        if (itunes_image) {
                            feed_image_url = itunes_image->Attribute("href");
                        }
                    }
                    for (
                        auto xml_item = channel->FirstChildElement("item");
                        xml_item;
                        xml_item = xml_item->NextSiblingElement("item")) 
                    {
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
                        // if there is no direct enclosure, we can try to get one if the link is to youtube
                        else if (new_item.link.find("youtube.com") != std::string::npos) {
                            new_item.enclosure = new_item.link;
                        }
                        // look for an itunes:image tag
                        auto itunes_image = xml_item->FirstChildElement("itunes:image");
                        if (itunes_image) {
                            new_item.image_url = itunes_image->Attribute("href");
                        }
                        // obtain the updated time from a tag similar to
                        // <pubDate>Tue, 14 Apr 2020 18:16:11 +0000</pubDate>
                        if (auto pub_date = xml_item->FirstChildElement("pubDate"); pub_date) {
                            std::istringstream pub_date_istr{pub_date->GetText()};
                            pub_date_istr >> std::chrono::parse("%a, %d %b %Y %T %z", new_item.updated);
                        }
                        items.emplace_back(std::move(new_item));
                    }
                }
            }
            else if (auto root_feed = doc.FirstChildElement("feed"); root_feed) {
                auto title = root_feed->FirstChildElement("title");
                if (title) {
                    feed_title = title->GetText();
                }
                auto link = root_feed->FirstChildElement("link");
                if (link) {
                    feed_link = link->Attribute("href");
                }
                auto description = root_feed->FirstChildElement("description");
                if (description) {
                    feed_description = description->GetText();
                }
                auto image = root_feed->FirstChildElement("image");
                if (image) {
                    auto url = image->FirstChildElement("url");
                    if (url) {
                        feed_image_url = url->GetText();
                    }
                }
                else if (auto itunes_image = root_feed->FirstChildElement("itunes:image"); itunes_image) {
                    feed_image_url = itunes_image->Attribute("href");
                }
                else if (auto image_el = root_feed->FirstChildElement("media:thumbnail"); image_el) {
                    feed_image_url = image_el->Attribute("url");
                }
                
                for (
                    auto xml_item = root_feed->FirstChildElement("entry"); 
                    xml_item; 
                    xml_item = xml_item->NextSiblingElement("entry")) 
                {
                    item new_item;
                    title = xml_item->FirstChildElement("title");
                    if (title) {
                        new_item.title = title->GetText();
                    }
                    link = xml_item->FirstChildElement("link");
                    if (link) {
                        new_item.link = link->Attribute("href");
                    }
                    description = xml_item->FirstChildElement("summary");
                    if (description) {
                        new_item.description = description->GetText();
                    }
                    // look for a media:group tag
                    if (auto media_group = xml_item->FirstChildElement("media:group"); media_group) {
                        if (auto media_content = media_group->FirstChildElement("media:content"); media_content) {
                            new_item.enclosure = media_content->Attribute("url");
                        }
                        // look for a media:thumbnail tag
                        if (auto media_thumbnail = media_group->FirstChildElement("media:thumbnail"); media_thumbnail) {
                            new_item.image_url = media_thumbnail->Attribute("url");
                            // if the feed doesn't have an image, asign the first thumbnail found
                            if (feed_image_url.empty()) {
                                feed_image_url = new_item.image_url;
                            }
                        }
                    }
                    // get the updated time from an object similar to
                    // <updated>2024-12-07T06:49:08+00:00</updated>
                    if (auto updated = xml_item->FirstChildElement("updated"); updated) {
                        std::istringstream updated_istr{updated->GetText()};
                        updated_istr >> std::chrono::parse("%FT%T%Ez", new_item.updated);
                    }
                    items.emplace_back(std::move(new_item));
                }
            }
        }

        std::string contents;
        std::string feed_title;
        std::string feed_link;
        std::string feed_description;
        std::string feed_image_url;
        std::vector<item> items;
        std::function<std::string(std::string_view)> system_runner_;
    };
}