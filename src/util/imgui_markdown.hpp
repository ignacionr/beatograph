#pragma once // Use #pragma once instead of traditional include guards

// Standard Library Includes (Alphabetical Order)
#include <functional> // For std::function
#include <memory>
#include <string>
#include <unordered_map> // For the renderer map
#include <vector>


// Third-party Includes
#include <imgui/imgui.h> // Include ImGui after standard library

// Sundown Includes (C Libraries) - Wrap in extern "C"
extern "C" {
    #include <html.h>     // We use its callback structure as a base & for html_renderopt
    #include <markdown.h>
}

// --- Helper Struct for Parsed Elements ---

// Enum to represent different Markdown element types (lowercase name)
enum class markdown_element_type {
    TEXT,
    HEADER,
    LIST_ITEM_BULLET, // Unordered list item
    LIST_ITEM_ORDERED, // Ordered list item (basic implementation)
    BOLD_START,       // Marker for start of bold text
    BOLD_END,         // Marker for end of bold text
    ITALIC_START,     // Marker for start of italic text
    ITALIC_END,       // Marker for end of italic text
    HRULE,            // Horizontal rule
    CODE_BLOCK,       // Code block
    // Add more types as needed (e.g., LINK, IMAGE, QUOTE)
};

// Structure to hold information about a single parsed Markdown element (lowercase name)
struct markdown_element {
    markdown_element_type type{};
    std::string text{};
    int level{0};
    long long item_number{0};

    // Constructor using the lowercase enum type
    markdown_element(markdown_element_type t, std::string txt = "", int lvl = 0, long long num = 0)
        : type{t}, text{std::move(txt)}, level{lvl}, item_number{num} {}
};

// --- Main Renderer Struct (lowercase name) ---

struct imgui_markdown_renderer {
public: // Public members first
    // Fonts (optional, manage externally or provide defaults)
    ImFont* boldFont{nullptr};
    ImFont* italicFont{nullptr};
    ImFont* headerFonts[6]{nullptr}; // Aggregate initialization
    ImFont* codeFont{nullptr};

    // Default constructor
    imgui_markdown_renderer() = default;

    // Parses the Markdown string and populates the internal element list.
    void parse(const std::string& markdown_text) {
        elements.clear(); // Clear previous elements
        current_list_level = {0};
        current_ordered_list_number = {0};
        is_in_ordered_list = {false};

        struct buf *output_buffer{bufnew(64)};
        struct sd_callbacks callbacks{};
        // **FIXED**: Create an options struct and pass its address
        struct html_renderopt options{}; // Create default HTML options struct
        // Initialize callbacks struct using standard HTML renderer setup
        sdhtml_renderer(&callbacks, &options, 0); // Pass address of options

        // Assign our static methods to the Sundown callbacks struct
        callbacks.blockcode = cb_blockcode;
        callbacks.paragraph = cb_paragraph;
        callbacks.header = cb_header;
        callbacks.hrule = cb_hrule;
        callbacks.list = cb_list;
        callbacks.listitem = cb_listitem;
        callbacks.emphasis = cb_emphasis;
        callbacks.double_emphasis = cb_double_emphasis;
        callbacks.normal_text = cb_normal_text;
        // Add other callbacks here...

        unsigned int extensions{MKDEXT_FENCED_CODE | MKDEXT_NO_INTRA_EMPHASIS};
        // Pass 'this' instance as the opaque user data pointer
        struct sd_markdown *markdown{sd_markdown_new(extensions, 16, &callbacks, this)};

        if (markdown) {
            sd_markdown_render(output_buffer, reinterpret_cast<const uint8_t*>(markdown_text.data()), markdown_text.size(), markdown);
            sd_markdown_free(markdown);
        } else {
            elements.emplace_back(markdown_element_type::TEXT, "Error: Could not initialize Markdown parser.");
        }
        bufrelease(output_buffer);
    }

    // Renders the previously parsed Markdown elements using ImGui.
    void render_parsed() {
        bool need_spacing{false};
        for (size_t i{0}; i < elements.size(); ++i) {
            const auto& el{elements[i]};
            if (need_spacing) {
                 if (!(el.type == markdown_element_type::LIST_ITEM_BULLET || el.type == markdown_element_type::LIST_ITEM_ORDERED)) {
                      ImGui::Spacing();
                 }
                 need_spacing = false;
            }
            // Find and call the appropriate renderer lambda from the map
            auto it = element_renderers.find(el.type);
            if (it != element_renderers.end()) {
                it->second(this, el, i, need_spacing);
            }
        }
    }

private: // Private members last
    std::vector<markdown_element> elements{};
    int current_list_level{0};
    long long current_ordered_list_number{0};
    bool is_in_ordered_list{false};

    // Type alias for the renderer function stored in the map
    using renderer_func = std::function<void(imgui_markdown_renderer*, const markdown_element&, size_t, bool&)>;

    // Declare static map inside the class (defined/initialized outside)
    inline static const std::unordered_map<markdown_element_type, renderer_func> element_renderers;

    // --- Sundown Callback Declarations ---
    // Helper to cast opaque pointer back to our class instance
    static imgui_markdown_renderer* GetInstance(void *opaque) {
        return static_cast<imgui_markdown_renderer*>(opaque);
    }
    // Block callbacks
    static void cb_blockcode(struct buf *ob, const struct buf *text, const struct buf *lang, void *opaque);
    static void cb_paragraph(struct buf *ob, const struct buf *text, void *opaque);
    static void cb_header(struct buf *ob, const struct buf *text, int level, void *opaque);
    static void cb_hrule(struct buf *ob, void *opaque);
    static void cb_list(struct buf *ob, const struct buf *text, int flags, void *opaque);
    static void cb_listitem(struct buf *ob, const struct buf *text, int flags, void *opaque);
    // Span callbacks - Ensure signatures match sd_callbacks
    static int cb_double_emphasis(struct buf *ob, const struct buf *text, void *opaque); // text is const
    static int cb_emphasis(struct buf *ob, const struct buf *text, void *opaque); // text is const
    static void cb_normal_text(struct buf *ob, const struct buf *text, void *opaque);

}; // ========= End of struct imgui_markdown_renderer Definition =========


// --- Sundown Callback Definitions ---
// These are defined outside the class for clarity and to ensure the class is fully defined first.

inline void imgui_markdown_renderer::cb_blockcode(struct buf * /*ob*/, const struct buf *text, const struct buf * /*lang*/, void *opaque) {
    auto* instance{GetInstance(opaque)};
    if (text && text->size > 0) {
        size_t len{text->size};
        if (len > 0 && text->data[len - 1] == '\n') { len--; }
        if (len > 0 && text->data[len - 1] == '\r') { len--; }
        instance->elements.emplace_back(markdown_element_type::CODE_BLOCK, std::string{reinterpret_cast<const char*>(text->data), len});
    }
}

inline void imgui_markdown_renderer::cb_paragraph(struct buf * /*ob*/, const struct buf * /*text*/, void *opaque) {
    auto* instance{GetInstance(opaque)};
     if (!instance->elements.empty() && instance->elements.back().type != markdown_element_type::HRULE) {
          instance->elements.emplace_back(markdown_element_type::TEXT, ""); // Signal potential paragraph break
     }
}

inline void imgui_markdown_renderer::cb_header(struct buf * /*ob*/, const struct buf *text, int level, void *opaque) {
    auto* instance{GetInstance(opaque)};
    if (text && text->size > 0) {
        instance->elements.emplace_back(markdown_element_type::HEADER, std::string{reinterpret_cast<const char*>(text->data), text->size}, level);
    }
}

inline void imgui_markdown_renderer::cb_hrule(struct buf * /*ob*/, void *opaque) {
    GetInstance(opaque)->elements.emplace_back(markdown_element_type::HRULE);
}

inline void imgui_markdown_renderer::cb_list(struct buf * /*ob*/, const struct buf * /*text*/, int flags, void *opaque) {
    auto* instance{GetInstance(opaque)};
    if (flags & MKD_LIST_ORDERED) {
        instance->is_in_ordered_list = true;
        instance->current_ordered_list_number = 1;
    } else {
         instance->is_in_ordered_list = false;
    }
}

// Added flags parameter name back for clarity, though unused
inline void imgui_markdown_renderer::cb_listitem(struct buf * /*ob*/, const struct buf *text, int /*flags*/, void *opaque) {
    auto* instance{GetInstance(opaque)};
    if (instance->is_in_ordered_list) {
         instance->elements.emplace_back(markdown_element_type::LIST_ITEM_ORDERED, "", instance->current_list_level, instance->current_ordered_list_number++);
    } else {
         instance->elements.emplace_back(markdown_element_type::LIST_ITEM_BULLET, "", instance->current_list_level);
    }
    if (text && text->size > 0) {
         instance->elements.emplace_back(markdown_element_type::TEXT, std::string{reinterpret_cast<const char*>(text->data), text->size});
    }
}

inline int imgui_markdown_renderer::cb_double_emphasis(struct buf * /*ob*/, const struct buf *text, void *opaque) {
    auto* instance{GetInstance(opaque)};
    instance->elements.emplace_back(markdown_element_type::BOLD_START);
    if (text && text->size > 0) {
        instance->elements.emplace_back(markdown_element_type::TEXT, std::string{reinterpret_cast<const char*>(text->data), text->size});
    }
    instance->elements.emplace_back(markdown_element_type::BOLD_END);
    return 1; // Handled
}

inline int imgui_markdown_renderer::cb_emphasis(struct buf * /*ob*/, const struct buf *text, void *opaque) {
    auto* instance{GetInstance(opaque)};
    instance->elements.emplace_back(markdown_element_type::ITALIC_START);
    if (text && text->size > 0) {
        instance->elements.emplace_back(markdown_element_type::TEXT, std::string{reinterpret_cast<const char*>(text->data), text->size});
    }
    instance->elements.emplace_back(markdown_element_type::ITALIC_END);
    return 1; // Handled
}

inline void imgui_markdown_renderer::cb_normal_text(struct buf * /*ob*/, const struct buf *text, void *opaque) {
    auto* instance{GetInstance(opaque)};
    if (text && text->size > 0) {
        instance->elements.emplace_back(markdown_element_type::TEXT, std::string{reinterpret_cast<const char*>(text->data), text->size});
    }
}


// --- Define and initialize the static map outside the class definition ---
// Use 'inline' to allow definition in header file (C++17 onwards)
inline const std::unordered_map<markdown_element_type, imgui_markdown_renderer::renderer_func> imgui_markdown_renderer::element_renderers {
    { markdown_element_type::HEADER,
        [](imgui_markdown_renderer* self, const markdown_element& el, size_t /*idx*/, bool& spacing) {
        ImFont* targetFont = (el.level >= 1 && el.level <= 6 && self->headerFonts[el.level - 1])
                                ? self->headerFonts[el.level - 1]
                                : nullptr;
        if (targetFont) ImGui::PushFont(targetFont);
        ImGui::TextWrapped("%s", el.text.c_str());
        if (targetFont) ImGui::PopFont();
        ImGui::Separator();
        spacing = true;
    }},

    { markdown_element_type::TEXT,
        [](imgui_markdown_renderer* self, const markdown_element& el, size_t idx, bool& spacing) {
        // Now accessing self->elements is safe as the class is fully defined
        if (!el.text.empty()) {
            bool end_of_paragraph = (idx + 1 == self->elements.size() ||
                                    self->elements[idx+1].type == markdown_element_type::HEADER ||
                                    self->elements[idx+1].type == markdown_element_type::HRULE ||
                                    self->elements[idx+1].type == markdown_element_type::LIST_ITEM_BULLET ||
                                    self->elements[idx+1].type == markdown_element_type::LIST_ITEM_ORDERED ||
                                    self->elements[idx+1].type == markdown_element_type::CODE_BLOCK);

            ImGui::TextWrapped("%s", el.text.c_str());

            if (end_of_paragraph) {
                spacing = true;
            } else {
                if (idx + 1 < self->elements.size() && self->elements[idx+1].type == markdown_element_type::TEXT && !el.text.empty() && el.text.back() != '\n') {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(" ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
            }
        } else if (idx > 0) {
            spacing = true;
        }
    }},

    { markdown_element_type::LIST_ITEM_BULLET,
        [](imgui_markdown_renderer* /*self*/, const markdown_element& /*el*/, size_t /*idx*/, bool& /*spacing*/) {
        ImGui::Bullet();
        ImGui::SameLine();
    }},

    { markdown_element_type::LIST_ITEM_ORDERED,
        [](imgui_markdown_renderer* /*self*/, const markdown_element& el, size_t /*idx*/, bool& /*spacing*/) {
        ImGui::Text("%lld.", el.item_number);
        ImGui::SameLine();
    }},

    { markdown_element_type::BOLD_START,
        [](imgui_markdown_renderer* self, const markdown_element& /*el*/, size_t /*idx*/, bool& /*spacing*/) {
        if (self->boldFont) {
             ImGui::PushFont(self->boldFont);
        } else {
             // **FIXED**: Calculate color multiplication correctly and pass both args
             ImVec4 baseColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
             ImVec4 boldColor = ImVec4{baseColor.x * 1.2f, baseColor.y * 1.2f, baseColor.z * 1.2f, baseColor.w};
             ImGui::PushStyleColor(ImGuiCol_Text, boldColor);
        }
    }},

    { markdown_element_type::BOLD_END,
        [](imgui_markdown_renderer* self, const markdown_element& /*el*/, size_t /*idx*/, bool& /*spacing*/) {
        if (self->boldFont) {
            ImGui::PopFont();
        } else {
            ImGui::PopStyleColor();
        }
    }},

    { markdown_element_type::ITALIC_START,
        [](imgui_markdown_renderer* self, const markdown_element& /*el*/, size_t /*idx*/, bool& /*spacing*/) {
        if (self->italicFont) ImGui::PushFont(self->italicFont);
    }},

    { markdown_element_type::ITALIC_END,
        [](imgui_markdown_renderer* self, const markdown_element& /*el*/, size_t /*idx*/, bool& /*spacing*/) {
        if (self->italicFont) ImGui::PopFont();
    }},

    { markdown_element_type::HRULE,
        [](imgui_markdown_renderer* /*self*/, const markdown_element& /*el*/, size_t /*idx*/, bool& spacing) {
        ImGui::Separator();
        spacing = true;
    }},

    { markdown_element_type::CODE_BLOCK,
        [](imgui_markdown_renderer* self, const markdown_element& el, size_t /*idx*/, bool& spacing) {
        if (self->codeFont) ImGui::PushFont(self->codeFont);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.8f, 0.85f, 1.0f, 1.0f});
        ImGui::TextUnformatted(el.text.c_str());
        ImGui::PopStyleColor();
        if (self->codeFont) ImGui::PopFont();
        spacing = true;
    }}
    // Add other element types and their lambdas here...
};

