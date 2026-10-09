#pragma once
#include <optional>
#include <string>
#include <vector>

#include "compositor/model/canvas_guide.hpp"
#include "compositor/model/errors.hpp"
#include "compositor/model/layer_record.hpp"

namespace compositor::model {

/// The parsed `manifest.json` of a `.comp` package. Layers run bottom to top: the last draws on top.
struct ProjectManifest {
    std::string format = kFormatIdentifier;
    int version = kCurrentFormatVersion;
    std::string color_space = kColorSpace;
    /// Pixels per inch, 1–9600. Older version-1 projects omit it and default to 72.
    std::optional<double> resolution;
    std::string document_id;
    int width = 0;
    int height = 0;
    std::optional<std::string> active_layer_id;
    std::vector<ProjectLayerRecord> layers;
    /// Alignment guides. Missing on versions 1–7.
    std::optional<std::vector<CanvasGuide>> guides;
};

}  // namespace compositor::model
