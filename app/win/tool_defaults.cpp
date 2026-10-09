#include "tool_defaults.hpp"

#include <QSettings>

namespace compositor::appwin {
namespace {

[[nodiscard]] QString prefixed(const QString& key) {
    return QStringLiteral("tool/") + key;
}

}  // namespace

bool tool_bool(const QString& key, bool fallback) {
    return QSettings().value(prefixed(key), fallback).toBool();
}

int tool_int(const QString& key, int fallback) {
    return QSettings().value(prefixed(key), fallback).toInt();
}

double tool_double(const QString& key, double fallback) {
    return QSettings().value(prefixed(key), fallback).toDouble();
}

QString tool_string(const QString& key, const QString& fallback) {
    return QSettings().value(prefixed(key), fallback).toString();
}

void set_tool_bool(const QString& key, bool value) {
    QSettings().setValue(prefixed(key), value);
}

void set_tool_int(const QString& key, int value) {
    QSettings().setValue(prefixed(key), value);
}

void set_tool_double(const QString& key, double value) {
    QSettings().setValue(prefixed(key), value);
}

void set_tool_string(const QString& key, const QString& value) {
    QSettings().setValue(prefixed(key), value);
}

}  // namespace compositor::appwin
