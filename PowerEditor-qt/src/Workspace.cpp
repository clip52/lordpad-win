#include "Workspace.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSettings>

namespace {
constexpr int kCurrentVersion = 1;
constexpr int kMaxRecent = 8;
constexpr const char* kSettingsOrg = "clip52";
constexpr const char* kSettingsApp = "notepadpp-qt";
constexpr const char* kRecentKey = "Workspace/Recent";

// Serializa um WorkspaceFile em QJsonObject.
QJsonObject fileToJson(const WorkspaceFile& f) {
    QJsonObject o;
    o.insert(QStringLiteral("path"), f.path);
    o.insert(QStringLiteral("lexer"), f.lexer);
    o.insert(QStringLiteral("line"), f.line);
    o.insert(QStringLiteral("active"), f.active);
    return o;
}

// Reverso: lê WorkspaceFile a partir de QJsonObject (tolerante a campos ausentes).
WorkspaceFile fileFromJson(const QJsonObject& o) {
    WorkspaceFile f;
    f.path   = o.value(QStringLiteral("path")).toString();
    f.lexer  = o.value(QStringLiteral("lexer")).toString();
    f.line   = o.value(QStringLiteral("line")).toInt(0);
    f.active = o.value(QStringLiteral("active")).toBool(false);
    return f;
}
} // namespace

Workspace::Workspace(QObject* parent)
    : QObject(parent)
{
}

bool Workspace::load(const QString& path) {
    m_lastError.clear();

    if (path.isEmpty()) {
        m_lastError = tr("Caminho do workspace vazio.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Não foi possível abrir \"%1\": %2")
                          .arg(path, file.errorString());
        return false;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError) {
        m_lastError = tr("JSON inválido em \"%1\": %2")
                          .arg(path, err.errorString());
        return false;
    }
    if (!doc.isObject()) {
        m_lastError = tr("Formato de workspace inválido em \"%1\" (raiz deve ser objeto).")
                          .arg(path);
        return false;
    }

    const QJsonObject root = doc.object();

    WorkspaceData d;
    d.version = root.value(QStringLiteral("version")).toInt(kCurrentVersion);
    if (d.version < 1 || d.version > kCurrentVersion) {
        m_lastError = tr("Versão de workspace não suportada: %1").arg(d.version);
        return false;
    }

    d.name = root.value(QStringLiteral("name")).toString();

    // rootFolders
    const QJsonArray folders = root.value(QStringLiteral("rootFolders")).toArray();
    d.rootFolders.reserve(folders.size());
    for (const QJsonValue& v : folders) {
        if (v.isString()) d.rootFolders << v.toString();
    }

    // openFiles
    const QJsonArray files = root.value(QStringLiteral("openFiles")).toArray();
    d.openFiles.reserve(files.size());
    for (const QJsonValue& v : files) {
        if (v.isObject()) d.openFiles << fileFromJson(v.toObject());
    }

    // Datas (ISO 8601 com ms)
    d.createdAt = QDateTime::fromString(
        root.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    d.updatedAt = QDateTime::fromString(
        root.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);

    m_data = d;
    m_currentPath = path;

    addRecent(path);
    emit loaded(m_data);
    return true;
}

bool Workspace::save(const QString& path) {
    m_lastError.clear();

    const QString target = path.isEmpty() ? m_currentPath : path;
    if (target.isEmpty()) {
        m_lastError = tr("Nenhum caminho informado para salvar o workspace.");
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!m_data.createdAt.isValid()) {
        m_data.createdAt = now;
    }
    m_data.updatedAt = now;
    if (m_data.version <= 0) m_data.version = kCurrentVersion;

    QJsonObject root;
    root.insert(QStringLiteral("version"), m_data.version);
    root.insert(QStringLiteral("name"), m_data.name);

    QJsonArray folders;
    for (const QString& f : m_data.rootFolders) folders.append(f);
    root.insert(QStringLiteral("rootFolders"), folders);

    QJsonArray files;
    for (const WorkspaceFile& f : m_data.openFiles) files.append(fileToJson(f));
    root.insert(QStringLiteral("openFiles"), files);

    root.insert(QStringLiteral("createdAt"),
                m_data.createdAt.toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("updatedAt"),
                m_data.updatedAt.toString(Qt::ISODateWithMs));

    const QJsonDocument doc(root);

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_lastError = tr("Não foi possível gravar \"%1\": %2")
                          .arg(target, file.errorString());
        return false;
    }

    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        m_lastError = tr("Falha ao escrever \"%1\": %2")
                          .arg(target, file.errorString());
        file.close();
        return false;
    }
    file.close();

    m_currentPath = target;
    addRecent(target);
    emit saved(m_data);
    return true;
}

QString Workspace::currentPath() const {
    return m_currentPath;
}

QString Workspace::lastError() const {
    return m_lastError;
}

const WorkspaceData& Workspace::data() const {
    return m_data;
}

WorkspaceData& Workspace::mutableData() {
    return m_data;
}

QStringList Workspace::recentWorkspaces() {
    QSettings s(kSettingsOrg, kSettingsApp);
    QStringList list = s.value(kRecentKey).toStringList();

    // Remove entradas que deixaram de existir no disco (mantém a ordem).
    QStringList alive;
    alive.reserve(list.size());
    for (const QString& p : list) {
        if (QFileInfo::exists(p)) alive << p;
    }
    return alive;
}

void Workspace::addRecent(const QString& path) {
    if (path.isEmpty()) return;

    QSettings s(kSettingsOrg, kSettingsApp);
    QStringList list = s.value(kRecentKey).toStringList();

    // Move para o topo, evitando duplicatas (case-sensitive em Linux).
    list.removeAll(path);
    list.prepend(path);

    while (list.size() > kMaxRecent) list.removeLast();

    s.setValue(kRecentKey, list);
}

void Workspace::clearRecent() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.remove(kRecentKey);
}
