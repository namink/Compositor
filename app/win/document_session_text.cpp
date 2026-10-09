#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/placement.hpp"
#include "document_session.hpp"

// Editable text layers, ported from the macOS app's `TypeTool.swift`: the rasterized PNG is the
// display and export fallback, while the `text` metadata keeps the layer editable so it round-trips
// with the macOS app. Rendering uses Qt where the macOS app uses CoreText, so a font may lay out
// differently, but the box, padding and alignment rules are the same.

namespace compositor::appwin {
namespace {

constexpr double kPadding = 12.0;

[[nodiscard]] render::RgbaSurface to_surface(const QImage& image) {
    const int width = image.width();
    const int height = image.height();
    render::RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        std::memcpy(surface.data() + surface.offset(0, y), image.constScanLine(y),
                    static_cast<std::size_t>(width) * 4U);
    }
    return surface;
}

[[nodiscard]] QString layer_name(const QString& content) {
    const QString flattened = content.simplified();
    return flattened.isEmpty() ? QStringLiteral("Text") : flattened.left(40);
}

}  // namespace

bool DocumentSession::rasterize_text(const nlohmann::json& style, render::RgbaSurface& surface, QString& error) {
    const QString content = QString::fromStdString(style.value("content", std::string()));
    if (content.trimmed().isEmpty()) {
        error = QStringLiteral("Enter some text first.");
        return false;
    }
    const QString font_name = QString::fromStdString(style.value("fontName", std::string("Helvetica")));
    const double font_size = style.value("fontSize", 72.0);
    const QString alignment = QString::fromStdString(style.value("alignment", std::string("Left")));
    const double tracking = style.value("tracking", 0.0);
    const double leading = style.value("leading", 0.0);
    const double line_height = leading > 0.0 ? leading : font_size * 1.2;

    bool has_box = false;
    double box_width = 0.0;
    double box_height = 0.0;
    if (style.contains("boxSize") && style["boxSize"].is_object()) {
        box_width = style["boxSize"].value("width", 0.0);
        box_height = style["boxSize"].value("height", 0.0);
        has_box = box_width >= 16.0 && box_height >= 16.0;
    }

    QFont font(font_name);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(font_size))));
    if (tracking != 0.0) {
        font.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    }
    QTextDocument document;
    document.setDefaultFont(font);
    document.setPlainText(content);
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    option.setAlignment(alignment == QStringLiteral("Center")  ? Qt::AlignHCenter
                        : alignment == QStringLiteral("Right") ? Qt::AlignRight
                                                               : Qt::AlignLeft);
    document.setDefaultTextOption(option);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(line_height, QTextBlockFormat::FixedHeight);
        QTextCursor cursor(block);
        cursor.setBlockFormat(format);
    }
    // Per-letter color and font runs (macOS's `colorRuns`/`fontRuns`), in UTF-16 offsets.
    const auto apply_runs = [&document](const nlohmann::json& runs, bool color) {
        if (!runs.is_array()) {
            return;
        }
        for (const nlohmann::json& run : runs) {
            if (!run.is_object()) {
                continue;
            }
            const int location = run.value("location", 0);
            const int length = run.value("length", 0);
            if (length <= 0 || location < 0) {
                continue;
            }
            QTextCursor cursor(&document);
            cursor.setPosition(location);
            cursor.setPosition(location + length, QTextCursor::KeepAnchor);
            QTextCharFormat format;
            if (color) {
                format.setForeground(
                    QColor::fromRgbF(run.value("red", 0.0), run.value("green", 0.0), run.value("blue", 0.0)));
            } else {
                format.setFontFamilies({QString::fromStdString(run.value("fontName", std::string("Helvetica")))});
            }
            cursor.mergeCharFormat(format);
        }
    };
    if (style.contains("colorRuns")) {
        apply_runs(style.at("colorRuns"), true);
    }
    if (style.contains("fontRuns")) {
        apply_runs(style.at("fontRuns"), false);
    }
    if (has_box) {
        document.setTextWidth(std::max(1.0, box_width - 2.0 * kPadding));
    }
    const QSizeF measured = document.size();

    int width = 0;
    int height = 0;
    if (has_box) {
        width = static_cast<int>(std::ceil(box_width));
        height = static_cast<int>(std::ceil(box_height));
    } else {
        width = static_cast<int>(std::ceil(std::max(16.0, measured.width() + 2.0 * kPadding + font_size * 0.1)));
        height = static_cast<int>(std::ceil(std::max(16.0, std::max(measured.height(), line_height) + 2.0 * kPadding)));
    }
    if (width < 1 || height < 1 || width > model::DocumentLimits::kMaxSide ||
        height > model::DocumentLimits::kMaxSide ||
        static_cast<std::int64_t>(width) * height > model::DocumentLimits::kMaxSurfacePixels) {
        error = QStringLiteral("That text is too large.");
        return false;
    }

    QImage image(width, height, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, QColor::fromRgbF(style.value("red", 0.0), style.value("green", 0.0),
                                                              style.value("blue", 0.0), 1.0));
    painter.translate(kPadding, kPadding);
    document.documentLayout()->draw(&painter, context);
    painter.end();
    surface = to_surface(image);
    return true;
}

bool DocumentSession::add_text_layer(const nlohmann::json& style, double origin_x, double origin_y, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    render::RgbaSurface surface;
    if (!rasterize_text(style, surface, error)) {
        return false;
    }
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = layer_name(QString::fromStdString(style.value("content", std::string()))).toStdString();
    record.is_visible = true;
    record.transform.origin_x = origin_x;
    record.transform.origin_y = origin_y;
    record.transform.width = surface.width();
    record.transform.height = surface.height();
    record.image_file = record.id + ".png";
    record.text = style;
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (active) {
        for (const model::ProjectLayerRecord& other : snapshot_->manifest.layers) {
            if (other.id == *active) {
                record.parent_id = other.is_group_layer() ? other.id : other.parent_id;
                break;
            }
        }
    }
    try {
        model::ImageAsset asset;
        asset.width = surface.width();
        asset.height = surface.height();
        asset.png = io::encode_png(surface);
        snapshot_->images[record.id] = std::move(asset);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    const std::string new_id = record.id;
    auto insert_at = snapshot_->manifest.layers.end();
    if (active) {
        for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
            if (it->id == *active) {
                insert_at = it + 1;
                break;
            }
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = new_id;
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::update_text_layer(const std::string& id, const nlohmann::json& style, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = nullptr;
    for (model::ProjectLayerRecord& candidate : snapshot_->manifest.layers) {
        if (candidate.id == id) {
            record = &candidate;
            break;
        }
    }
    if (record == nullptr || !record->text) {
        error = QStringLiteral("Select a text layer to edit.");
        return false;
    }
    render::RgbaSurface surface;
    if (!rasterize_text(style, surface, error)) {
        return false;
    }
    const auto asset = snapshot_->images.find(id);
    const double old_width = asset != snapshot_->images.end() ? asset->second.width : record->transform.width;
    const double old_height = asset != snapshot_->images.end() ? asset->second.height : record->transform.height;
    push_history();
    // Keep the transformed upper-left corner and the user's scale, rotation and flips.
    double anchor_x = 0.0;
    double anchor_y = 0.0;
    render::layer_pixel_to_document(record->transform, static_cast<int>(old_width), static_cast<int>(old_height), 0.0,
                                    0.0, anchor_x, anchor_y);
    if (old_width > 0.0 && old_height > 0.0) {
        record->transform.width = static_cast<double>(surface.width()) * record->transform.width / old_width;
        record->transform.height = static_cast<double>(surface.height()) * record->transform.height / old_height;
    } else {
        record->transform.width = surface.width();
        record->transform.height = surface.height();
    }
    double moved_x = 0.0;
    double moved_y = 0.0;
    render::layer_pixel_to_document(record->transform, surface.width(), surface.height(), 0.0, 0.0, moved_x, moved_y);
    record->transform.origin_x += anchor_x - moved_x;
    record->transform.origin_y += anchor_y - moved_y;
    record->text = style;
    try {
        model::ImageAsset replacement;
        replacement.width = surface.width();
        replacement.height = surface.height();
        replacement.png = io::encode_png(surface);
        snapshot_->images[id] = std::move(replacement);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    invalidate_surfaces();
    return recomposite(error);
}

}  // namespace compositor::appwin
