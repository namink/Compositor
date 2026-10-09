#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "canvas_view.hpp"
#include "main_window.hpp"

// Building and re-reading the Layers tree, split out so `main_window.cpp` stays within the size limit.

namespace compositor::appwin {

void MainWindow::on_layers_reordered() {
    if (reordering_layers_ || populating_layers_) {
        return;
    }
    reordering_layers_ = true;
    std::vector<std::pair<std::string, std::optional<std::string>>> order;
    std::function<void(QTreeWidgetItem*, std::optional<std::string>)> collect =
        [&](QTreeWidgetItem* parent, std::optional<std::string> parent_id) {
            const int count = parent == nullptr ? layers_->topLevelItemCount() : parent->childCount();
            for (int i = 0; i < count; ++i) {
                QTreeWidgetItem* item = parent == nullptr ? layers_->topLevelItem(i) : parent->child(i);
                const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
                order.emplace_back(id, parent_id);
                collect(item, id);
            }
        };
    collect(nullptr, std::nullopt);
    QString error;
    if (!session_->set_layer_tree(order, error)) {
        report(error);
        refresh_layers();
    } else {
        canvas_->updateImage(session_->image());
    }
    reordering_layers_ = false;
}

void MainWindow::refresh_layers() {
    populating_layers_ = true;
    layers_->clear();
    const model::ProjectManifest* manifest = session_->manifest();
    if (manifest != nullptr) {
        std::map<std::string, std::vector<const model::ProjectLayerRecord*>> children;
        for (const model::ProjectLayerRecord& record : manifest->layers) {
            children[record.parent_id.value_or(std::string())].push_back(&record);
        }
        std::function<void(QTreeWidgetItem*, const std::string&)> add = [&](QTreeWidgetItem* parent,
                                                                            const std::string& key) {
            const auto found = children.find(key);
            if (found == children.end()) {
                return;
            }
            for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
                const model::ProjectLayerRecord& record = **it;
                QString label = QString::fromStdString(record.name);
                if (record.is_group_layer()) {
                    label += QStringLiteral("\nFolder");
                } else if (record.adjustment) {
                    label += QStringLiteral("\nAdjustment");
                } else {
                    label += QStringLiteral("\n%1 x %2 px")
                                 .arg(static_cast<int>(record.transform.width))
                                 .arg(static_cast<int>(record.transform.height));
                }
                auto* item = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(layers_);
                item->setText(0, label);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(0, record.is_visible ? Qt::Checked : Qt::Unchecked);
                item->setData(0, Qt::UserRole, QString::fromStdString(record.id));
                if (!record.is_group_layer() && !record.adjustment) {
                    const QImage thumbnail = session_->layer_thumbnail(record.id, 36);
                    if (!thumbnail.isNull()) {
                        item->setIcon(0, QIcon(QPixmap::fromImage(thumbnail)));
                    }
                }
                add(item, record.id);
            }
        };
        add(nullptr, std::string());
        layers_->expandAll();
    }
    populating_layers_ = false;
    if (layer_count_ != nullptr) {
        layer_count_->setText(QString::number(manifest != nullptr ? manifest->layers.size() : 0));
    }
    if (layers_->topLevelItemCount() > 0) {
        layers_->setCurrentItem(layers_->topLevelItem(0));
    } else {
        on_layer_selected();
    }
}

}  // namespace compositor::appwin
