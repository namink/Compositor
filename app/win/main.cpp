#include <QApplication>
#include <QIcon>
#include <cmath>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "document_session.hpp"
#include "main_window.hpp"
#include "theme.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Compositor"));
    QApplication::setApplicationName(QStringLiteral("Compositor"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));
    compositor::appwin::apply_dark_theme(app);

    // Headless self-check for CI and smoke runs: open a project, composite, report the size, exit.
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--open-check")) {
        compositor::appwin::DocumentSession session;
        QString error;
        if (!session.open(QString::fromLocal8Bit(argv[2]), error)) {
            std::fprintf(stderr, "FAIL: %s\n", error.toUtf8().constData());
            return 1;
        }
        // Exercise the edit paths headlessly: visibility, move, opacity, and a save/reload write-back.
        const auto* manifest = session.manifest();
        if (manifest != nullptr && !manifest->layers.empty()) {
            const std::string id = manifest->layers.front().id;
            const double origin_x = manifest->layers.front().transform.origin_x;
            if (!session.set_visibility(id, false, error) || !session.set_visibility(id, true, error) ||
                !session.move_layer(id, 3.0, 0.0, error) || !session.set_opacity(id, 0.5, error)) {
                std::fprintf(stderr, "FAIL edit: %s\n", error.toUtf8().constData());
                return 1;
            }
            compositor::appwin::BrushSettings brush;
            brush.radius = 2.0;
            brush.hardness = 1.0;
            brush.opacity = 1.0;
            brush.red = 1.0;
            session.set_brush(brush);
            if (!session.stroke_begin(id, 2.0, 2.0, error) || !session.stroke_move(5.0, 2.0, error)) {
                std::fprintf(stderr, "FAIL paint: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.stroke_end();
            const QString saved = QString::fromLocal8Bit(argv[2]) + QStringLiteral(".saved");
            if (!session.save(saved, error)) {
                std::fprintf(stderr, "FAIL save: %s\n", error.toUtf8().constData());
                return 1;
            }
            compositor::appwin::DocumentSession reloaded;
            const compositor::model::ProjectLayerRecord* record = nullptr;
            if (reloaded.open(saved, error)) {
                record = reloaded.layer(id);
            }
            if (record == nullptr || std::abs(record->transform.origin_x - (origin_x + 3.0)) > 1e-6 ||
                !record->opacity || std::abs(*record->opacity - 0.5) > 1e-6) {
                std::fprintf(stderr, "FAIL write-back\n");
                return 1;
            }
            session.select_all();
            if (!session.has_selection() || !session.fill_selection(id, error) || !session.undo(error)) {
                std::fprintf(stderr, "FAIL selection: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.select_rect(compositor::render::DocRect{0.0, 0.0, 2.0, 2.0},
                                compositor::render::CombineMode::replace);
            if (!session.crop_to_selection(error) || session.manifest()->width != 2 || !session.undo(error)) {
                std::fprintf(stderr, "FAIL crop: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clear_selection();
            if (!session.select_wand(id, 4.0, 2.0, 32, true, compositor::render::CombineMode::replace, error) ||
                !session.has_selection()) {
                std::fprintf(stderr, "FAIL wand: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.invert_selection();
            session.expand_selection(1);
            session.contract_selection(1);
            session.feather_selection(1.0);
            if (session.selection_outline().empty()) {
                std::fprintf(stderr, "FAIL outline\n");
                return 1;
            }
            session.clear_selection();
            if (!session.add_adjustment_layer(id, "Invert", error) || !session.undo(error)) {
                std::fprintf(stderr, "FAIL adjustment: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.apply_filter(id, nlohmann::json{{"kind", "Invert"}}, error) || !session.undo(error)) {
                std::fprintf(stderr, "FAIL filter: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.canvas_size(6, 6, 4, error) || session.manifest()->width != 6 || !session.undo(error) ||
                !session.image_size(8, 8, error) || session.manifest()->width != 8 || !session.undo(error) ||
                !session.trim(error) || !session.undo(error)) {
                std::fprintf(stderr, "FAIL canvas: %s\n", error.toUtf8().constData());
                return 1;
            }
            const int doc_w = session.manifest()->width;
            const int doc_h = session.manifest()->height;
            if (!session.fill_shape(id, compositor::render::from_rect({0, 0, 1, 1}, doc_w, doc_h), 0, 1, 0, 1.0,
                                    error) ||
                !session.undo(error) ||
                !session.draw_gradient(id, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, doc_w, doc_h, false, error) ||
                !session.undo(error)) {
                std::fprintf(stderr, "FAIL paint: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.heal_begin(id, 2.0, 2.0, error)) {
                std::fprintf(stderr, "FAIL heal: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.heal_move(3.0, 3.0);
            session.heal_end();
            compositor::appwin::BrushSettings blur_brush;
            blur_brush.blur = true;
            blur_brush.radius = 2.0;
            blur_brush.hardness = 0.5;
            blur_brush.opacity = 1.0;
            session.set_brush(blur_brush);
            if (!session.stroke_begin(id, 2.0, 2.0, error)) {
                std::fprintf(stderr, "FAIL blur: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.stroke_move(3.0, 3.0, error)) {
                std::fprintf(stderr, "FAIL blur move\n");
                return 1;
            }
            session.stroke_end();
            if (!session.add_guide(true, 2.0, error) || !session.clear_guides(error)) {
                std::fprintf(stderr, "FAIL guides: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.clone_set_source(id, 1.0, 1.0, error) || !session.clone_begin(id, 2.0, 2.0, error)) {
                std::fprintf(stderr, "FAIL clone: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clone_move(3.0, 3.0);
            session.clone_end();
            if (!session.flip_layer(id, true, error) || !session.rotate_layer_90(id, true, error) ||
                !session.reset_transform(id, error)) {
                std::fprintf(stderr, "FAIL transform: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.add_blank_layer(id, error) || !session.duplicate_layer(id, error) || !session.undo(error) ||
                !session.redo(error) || !session.delete_layer(id, error)) {
                std::fprintf(stderr, "FAIL layers: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.flatten(error)) {
                std::fprintf(stderr, "FAIL flatten: %s\n", error.toUtf8().constData());
                return 1;
            }
            const QString png =
                QString::fromLocal8Bit(argv[2]) + QStringLiteral("/images/6F1D3C2A-0B7E-4E8A-9C4D-2A1B3C4D5E6F.png");
            if (!session.import_image(png, std::string(), error) || session.manifest()->layers.size() < 2) {
                std::fprintf(stderr, "FAIL import: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.merge_layers(error) || session.manifest()->layers.size() > 1) {
                std::fprintf(stderr, "FAIL merge: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.undo(error)) {
                std::fprintf(stderr, "FAIL merge undo\n");
                return 1;
            }
            const std::size_t before = session.manifest()->layers.size();
            session.copy_layers();
            if (!session.paste_layers(error) || session.manifest()->layers.size() != before + 1) {
                std::fprintf(stderr, "FAIL paste: %s\n", error.toUtf8().constData());
                return 1;
            }
            const std::size_t layers = session.manifest()->layers.size();
            session.select_all();
            if (!session.copy_selection(error) || !session.paste(error) ||
                session.manifest()->layers.size() != layers + 1) {
                std::fprintf(stderr, "FAIL pixel paste: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clear_selection();
            std::vector<std::string> order;
            for (auto it = session.manifest()->layers.rbegin(); it != session.manifest()->layers.rend(); ++it) {
                order.push_back(it->id);
            }
            std::swap(order[0], order[1]);
            if (!session.reorder_layers(order, error) || session.manifest()->layers.back().id != order.front()) {
                std::fprintf(stderr, "FAIL reorder: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.add_folder(std::string(), error)) {
                std::fprintf(stderr, "FAIL folder: %s\n", error.toUtf8().constData());
                return 1;
            }
            const std::string folder = *session.manifest()->active_layer_id;
            std::vector<std::pair<std::string, std::optional<std::string>>> tree;
            std::string nested;
            for (auto it = session.manifest()->layers.rbegin(); it != session.manifest()->layers.rend(); ++it) {
                std::optional<std::string> parent;
                if (!it->is_group_layer() && nested.empty()) {
                    parent = folder;
                    nested = it->id;
                }
                tree.emplace_back(it->id, parent);
            }
            if (!session.set_layer_tree(tree, error)) {
                std::fprintf(stderr, "FAIL tree: %s\n", error.toUtf8().constData());
                return 1;
            }
            const compositor::model::ProjectLayerRecord* moved = session.layer(nested);
            if (moved == nullptr || !moved->parent_id || *moved->parent_id != folder) {
                std::fprintf(stderr, "FAIL tree parent\n");
                return 1;
            }
            nlohmann::json text;
            text["content"] = std::string("Hi");
            text["fontName"] = std::string("Helvetica");
            text["fontSize"] = 48.0;
            text["red"] = 0.0;
            text["green"] = 0.0;
            text["blue"] = 0.0;
            text["alignment"] = std::string("Left");
            text["tracking"] = 0.0;
            text["leading"] = 0.0;
            const std::size_t before_text = session.manifest()->layers.size();
            if (!session.add_text_layer(text, 8.0, 8.0, error) ||
                session.manifest()->layers.size() != before_text + 1) {
                std::fprintf(stderr, "FAIL text add: %s\n", error.toUtf8().constData());
                return 1;
            }
            const std::string text_id = *session.manifest()->active_layer_id;
            text["content"] = std::string("Hello, world");
            if (!session.update_text_layer(text_id, text, error)) {
                std::fprintf(stderr, "FAIL text edit: %s\n", error.toUtf8().constData());
                return 1;
            }
            const compositor::model::ProjectLayerRecord* text_layer = session.layer(text_id);
            if (text_layer == nullptr || !text_layer->text ||
                text_layer->text->value("content", std::string()) != "Hello, world") {
                std::fprintf(stderr, "FAIL text round-trip\n");
                return 1;
            }
            text["colorRuns"] =
                nlohmann::json::array({{{"location", 0}, {"length", 1}, {"red", 1.0}, {"green", 0.0}, {"blue", 0.0}}});
            text["fontRuns"] =
                nlohmann::json::array({{{"location", 0}, {"length", 2}, {"fontName", std::string("Courier New")}}});
            if (!session.update_text_layer(text_id, text, error)) {
                std::fprintf(stderr, "FAIL text runs: %s\n", error.toUtf8().constData());
                return 1;
            }
            compositor::render::ShapeStyle shape_style;
            shape_style.kind = compositor::render::ShapeKind::rectangle;
            shape_style.red = 1.0F;
            shape_style.corner_radius = 3.0;
            const std::size_t before_shape = session.manifest()->layers.size();
            if (!session.add_shape_layer(shape_style, 0.0, 0.0, 8, 8, error) ||
                session.manifest()->layers.size() != before_shape + 1) {
                std::fprintf(stderr, "FAIL shape add: %s\n", error.toUtf8().constData());
                return 1;
            }
            shape_style.corner_radius = 1.0;
            if (!session.update_shape_layer(*session.manifest()->active_layer_id, shape_style, error)) {
                std::fprintf(stderr, "FAIL shape edit: %s\n", error.toUtf8().constData());
                return 1;
            }
            const compositor::model::ProjectLayerRecord* shape_layer =
                session.layer(*session.manifest()->active_layer_id);
            if (shape_layer == nullptr || !shape_layer->shape ||
                shape_layer->shape->value("kind", std::string()) != "Rectangle") {
                std::fprintf(stderr, "FAIL shape round-trip\n");
                return 1;
            }
            nlohmann::json dither;
            dither["kind"] = std::string("Dither");
            dither["style"] = std::string("Atkinson");
            dither["levels"] = 2.0;
            if (!session.apply_filter(shape_layer->id, dither, error)) {
                std::fprintf(stderr, "FAIL dither: %s\n", error.toUtf8().constData());
                return 1;
            }
            nlohmann::json camera_raw;
            camera_raw["kind"] = std::string("Camera Raw");
            camera_raw["exposure"] = 0.5;
            camera_raw["temperature"] = 20.0;
            camera_raw["vibrance"] = 15.0;
            camera_raw["vignetteAmount"] = -30.0;
            camera_raw["detail"] = {{"sharpenAmount", 25.0}, {"sharpenRadius", 1.0}};
            camera_raw["optics"] = {{"vignetteAmount", -20.0}, {"purpleAmount", 10.0}};
            camera_raw["calibration"] = {{"shadowTint", 5.0}, {"redHue", 2.0}};
            if (!session.apply_filter(shape_layer->id, camera_raw, error)) {
                std::fprintf(stderr, "FAIL camera raw: %s\n", error.toUtf8().constData());
                return 1;
            }
            nlohmann::json layer_fx;
            layer_fx["stroke"] = {{"enabled", true}, {"red", 1.0}, {"green", 0.0}, {"blue", 0.0}, {"size", 2.0}};
            if (!session.set_effects(shape_layer->id, layer_fx, error)) {
                std::fprintf(stderr, "FAIL set effects: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.warp_begin(shape_layer->id, 2.0, 2.0, 1, error)) {
                std::fprintf(stderr, "FAIL warp begin: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.warp_move(3.0, 3.0);
            session.warp_end();
            compositor::render::Quad quad;
            if (!session.layer_quad(shape_layer->id, quad)) {
                std::fprintf(stderr, "FAIL distort quad\n");
                return 1;
            }
            quad.x[1] += 2.0;
            quad.y[2] += 2.0;
            if (!session.apply_distort(shape_layer->id, quad, error)) {
                std::fprintf(stderr, "FAIL distort: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.select_rect(compositor::render::DocRect{1.0, 1.0, 1.0, 1.0},
                                compositor::render::CombineMode::replace);
            const std::size_t before_float = session.manifest()->layers.size();
            if (!session.float_selection(error) || session.manifest()->layers.size() != before_float + 1) {
                std::fprintf(stderr, "FAIL float selection: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clear_selection();
            if (!session.auto_levels(0, error)) {
                std::fprintf(stderr, "FAIL auto levels: %s\n", error.toUtf8().constData());
                return 1;
            }
            const compositor::model::ProjectLayerRecord* levels_layer =
                session.layer(*session.manifest()->active_layer_id);
            if (levels_layer == nullptr || !levels_layer->adjustment ||
                levels_layer->adjustment->value("kind", std::string()) != "Levels") {
                std::fprintf(stderr, "FAIL auto levels kind\n");
                return 1;
            }
            std::string image_layer;
            for (const compositor::model::ProjectLayerRecord& candidate : session.manifest()->layers) {
                if (!candidate.is_group_layer() && !candidate.adjustment && candidate.image_file) {
                    image_layer = candidate.id;
                    break;
                }
            }
            if (!image_layer.empty()) {
                if (!session.load_layer_selection(image_layer, compositor::render::CombineMode::replace, error) ||
                    !session.has_selection()) {
                    std::fprintf(stderr, "FAIL load layer selection: %s\n", error.toUtf8().constData());
                    return 1;
                }
                session.clear_selection();
            }
            const int states = session.history_count();
            if (states < 1 || !session.jump_history(0, error) || session.history_current() != 0 ||
                !session.jump_history(states, error) || session.history_current() != states) {
                std::fprintf(stderr, "FAIL history jump: %s\n", error.toUtf8().constData());
                return 1;
            }
            if (!session.color_range_select(128, 128, 128, 200, false, compositor::render::CombineMode::replace,
                                            error) ||
                !session.has_selection()) {
                std::fprintf(stderr, "FAIL color range: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clear_selection();
            session.select_rect(compositor::render::DocRect{1.0, 1.0, 2.0, 2.0},
                                compositor::render::CombineMode::replace);
            if (!session.refine_selection_edges(2, error) || !session.has_selection()) {
                std::fprintf(stderr, "FAIL refine edge: %s\n", error.toUtf8().constData());
                return 1;
            }
            session.clear_selection();
            if (!session.create(8, 8, error) || session.manifest()->width != 8 || session.manifest()->height != 8 ||
                session.manifest()->layers.size() != 1) {
                std::fprintf(stderr, "FAIL new project: %s\n", error.toUtf8().constData());
                return 1;
            }
            const QString psd_path = QString::fromLocal8Bit(argv[2]) + QStringLiteral(".psd");
            if (!session.export_psd(psd_path, error)) {
                std::fprintf(stderr, "FAIL export psd: %s\n", error.toUtf8().constData());
                return 1;
            }
        }
        std::printf("OK %dx%d\n", session.image().width(), session.image().height());
        return 0;
    }

    compositor::appwin::MainWindow window;
    window.show();
    return app.exec();
}
