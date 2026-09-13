#include "presets.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

namespace {

QString presetPath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/photo/presets.json");
}

QJsonObject loadRaw() {
    QFile f(presetPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

void writeRaw(const QJsonObject &data) {
    const QString path = presetPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
}

} // namespace

namespace presets {

QStringList names() {
    QStringList out = loadRaw().keys();
    std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) {
        return a.toLower() < b.toLower();
    });
    return out;
}

void save(const QString &name, const Adjustments &adj) {
    const QString n = name.trimmed();
    if (n.isEmpty())
        return;
    QJsonObject data = loadRaw();
    QJsonObject rec;
    for (const auto &k : kColorKeys)
        rec.insert(k, adj.get(k));
    data.insert(n, rec);
    writeRaw(data);
}

bool apply(const QString &name, Adjustments &adj) {
    const QJsonValue v = loadRaw().value(name);
    if (!v.isObject())
        return false;
    const QJsonObject rec = v.toObject();
    for (const auto &k : kColorKeys) {
        if (rec.contains(k))
            adj.set(k, rec.value(k).toDouble());
    }
    return true;
}

void remove(const QString &name) {
    QJsonObject data = loadRaw();
    if (!data.contains(name))
        return;
    data.remove(name);
    writeRaw(data);
}

}
