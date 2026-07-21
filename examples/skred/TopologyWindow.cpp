#include "TopologyWindow.h"

#include "VoiceTopology.h"
extern "C" {
#include "pikchr.h"
}

#include <skred/api.h>

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct SvgLabel {
    double x = 0.0;
    double y = 0.0;
    std::string anchor;
    std::string text;
};

std::string xmlDecode(std::string value) {
    struct Entity { const char *encoded; const char *plain; };
    static const Entity entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&#39;", "'"},
    };
    for (const Entity &entity : entities) {
        size_t at = 0;
        while ((at = value.find(entity.encoded, at)) != std::string::npos) {
            value.replace(at, std::strlen(entity.encoded), entity.plain);
            at += std::strlen(entity.plain);
        }
    }
    return value;
}

bool attribute(const std::string &tag, const char *name, std::string &value) {
    const std::string key = std::string(name) + "=\"";
    const size_t begin = tag.find(key);
    if (begin == std::string::npos) return false;
    const size_t valueBegin = begin + key.size();
    const size_t end = tag.find('"', valueBegin);
    if (end == std::string::npos) return false;
    value = tag.substr(valueBegin, end - valueBegin);
    return true;
}

std::vector<SvgLabel> parseLabels(const std::string &svg) {
    std::vector<SvgLabel> labels;
    size_t cursor = 0;
    while ((cursor = svg.find("<text ", cursor)) != std::string::npos) {
        const size_t tagEnd = svg.find('>', cursor);
        const size_t textEnd = tagEnd == std::string::npos ?
            std::string::npos : svg.find("</text>", tagEnd + 1);
        if (tagEnd == std::string::npos || textEnd == std::string::npos) break;
        const std::string tag = svg.substr(cursor, tagEnd - cursor + 1);
        std::string x, y, anchor;
        if (attribute(tag, "x", x) && attribute(tag, "y", y)) {
            SvgLabel label;
            label.x = std::strtod(x.c_str(), nullptr);
            label.y = std::strtod(y.c_str(), nullptr);
            label.anchor = attribute(tag, "text-anchor", anchor) ? anchor : "start";
            label.text = xmlDecode(svg.substr(tagEnd + 1, textEnd - tagEnd - 1));
            labels.push_back(std::move(label));
        }
        cursor = textEnd + 7;
    }
    return labels;
}

class TopologyView : public Fl_Widget {
public:
    TopologyView(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    bool setSvg(std::string svg, int sourceWidth, int sourceHeight) {
        std::unique_ptr<Fl_SVG_Image> image(
            new Fl_SVG_Image("voice-topology", svg.c_str()));
        if (image->fail() != 0) return false;
        svg_ = std::move(svg);
        image_ = std::move(image);
        labels_ = parseLabels(svg_);
        sourceWidth_ = sourceWidth;
        sourceHeight_ = sourceHeight;
        scaled_.reset();
        scaledWidth_ = 0;
        scaledHeight_ = 0;
        redraw();
        return true;
    }

private:
    void draw() override {
        fl_push_clip(x(), y(), w(), h());
        fl_color(fl_rgb_color(9, 17, 23));
        fl_rectf(x(), y(), w(), h());
        if (!image_ || sourceWidth_ <= 0 || sourceHeight_ <= 0) {
            fl_pop_clip();
            return;
        }

        constexpr int margin = 24;
        const int availableWidth = std::max(1, w() - margin * 2);
        const int availableHeight = std::max(1, h() - margin * 2);
        const double scale = std::min({
            static_cast<double>(availableWidth) / sourceWidth_,
            static_cast<double>(availableHeight) / sourceHeight_,
            3.0});
        const int drawWidth = std::max(1, static_cast<int>(sourceWidth_ * scale));
        const int drawHeight = std::max(1, static_cast<int>(sourceHeight_ * scale));
        const int drawX = x() + (w() - drawWidth) / 2;
        const int drawY = y() + (h() - drawHeight) / 2;
        if (!scaled_ || scaledWidth_ != drawWidth || scaledHeight_ != drawHeight) {
            scaled_.reset(image_->copy(drawWidth, drawHeight));
            scaledWidth_ = drawWidth;
            scaledHeight_ = drawHeight;
        }
        if (scaled_) scaled_->draw(drawX, drawY);

        const int fontSize = std::clamp(static_cast<int>(14.0 * scale), 10, 24);
        fl_font(FL_COURIER_BOLD, fontSize);
        fl_color(fl_rgb_color(224, 245, 250));
        for (const SvgLabel &label : labels_) {
            int labelX = drawX + static_cast<int>(label.x * scale);
            const int labelY = drawY + static_cast<int>(label.y * scale) +
                               fontSize / 3;
            const int textWidth = static_cast<int>(fl_width(label.text.c_str()));
            if (label.anchor == "middle") labelX -= textWidth / 2;
            else if (label.anchor == "end") labelX -= textWidth;
            fl_draw(label.text.c_str(), labelX, labelY);
        }
        fl_pop_clip();
    }

    std::string svg_;
    std::unique_ptr<Fl_SVG_Image> image_;
    std::unique_ptr<Fl_Image> scaled_;
    std::vector<SvgLabel> labels_;
    int sourceWidth_ = 0;
    int sourceHeight_ = 0;
    int scaledWidth_ = 0;
    int scaledHeight_ = 0;
};

Fl_Double_Window *gWindow = nullptr;
TopologyView *gView = nullptr;

void closeWindow(Fl_Widget *widget, void *) {
    widget->hide();
}

void setError(char *buffer, size_t capacity, const std::string &message) {
    if (!buffer || capacity == 0) return;
    std::strncpy(buffer, message.c_str(), capacity - 1);
    buffer[capacity - 1] = '\0';
}

} // namespace

extern "C" int topology_show_voice(int voice, int depth, char *error,
                                     size_t errorCapacity) {
    if (voice < 0 || depth < 0) {
        setError(error, errorCapacity, "voice and depth must be non-negative");
        return -1;
    }
    const char *graph = skred_voice_graph(voice, 1, depth);
    if (!graph || !*graph) {
        setError(error, errorCapacity, "Skred returned no voice graph");
        return -1;
    }
    const std::string graphCopy(graph);

    std::string source;
    std::string conversionError;
    int rootVoice = -1;
    if (!repl_voice_graph_to_pikchr(graphCopy.c_str(), source,
                                    rootVoice, conversionError)) {
        setError(error, errorCapacity, conversionError);
        return -1;
    }

    int width = 0;
    int height = 0;
    char *rendered = pikchr(source.c_str(), "voice-topology",
                            PIKCHR_DARK_MODE | PIKCHR_PLAINTEXT_ERRORS,
                            &width, &height);
    std::unique_ptr<char, decltype(&std::free)> renderedOwner(rendered, &std::free);
    if (!rendered || width <= 0 || height <= 0) {
        setError(error, errorCapacity,
                 rendered ? std::string(rendered) : "Pikchr rendering failed");
        return -1;
    }

    if (!gWindow) {
        gWindow = new Fl_Double_Window(820, 560, "Voice topology");
        gView = new TopologyView(0, 0, gWindow->w(), gWindow->h());
        gWindow->resizable(gView);
        gWindow->callback(closeWindow);
        gWindow->end();
    }
    if (!gView->setSvg(rendered, width, height)) {
        setError(error, errorCapacity, "FLTK could not load Pikchr SVG");
        return -1;
    }
    const std::string title = "Voice " + std::to_string(rootVoice) + " topology";
    gWindow->copy_label(title.c_str());
    gWindow->show();
    if (error && errorCapacity) error[0] = '\0';
    return 0;
}

extern "C" void topology_hide(void) {
    if (gWindow) gWindow->hide();
}
