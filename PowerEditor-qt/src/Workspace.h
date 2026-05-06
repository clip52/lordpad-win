#pragma once

// Workspace: persiste/abre arquivos .nppproj.json contendo a estrutura de
// pastas raiz, arquivos abertos, arquivo focado e metadados de tema/pack.
//
// O formato é JSON simples (versão 1), serializado via QJsonDocument.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>

struct WorkspaceFile {
    QString path;
    QString lexer;
    int line = 0;
    bool active = false;
};

struct WorkspaceData {
    int version = 1;
    QString name;
    QStringList rootFolders;
    QList<WorkspaceFile> openFiles;
    QDateTime createdAt;
    QDateTime updatedAt;
};

class Workspace : public QObject {
    Q_OBJECT
public:
    explicit Workspace(QObject* parent = nullptr);

    // Carrega de path absoluto. Retorna false em erro; lastError() detalha.
    bool load(const QString& path);

    // Salva no path atual ou no path passado (atualiza updatedAt automaticamente).
    // Se nenhum path estiver disponível, retorna false.
    bool save(const QString& path = {});

    QString currentPath() const;
    QString lastError() const;

    const WorkspaceData& data() const;
    WorkspaceData& mutableData();      // pra editar antes de save()

    // Recentes: persistido em QSettings("clip52","notepadpp-qt")
    // chave "Workspace/Recent" (QStringList, no máximo 8 entradas).
    static QStringList recentWorkspaces();
    static void addRecent(const QString& path);
    static void clearRecent();

signals:
    void loaded(const WorkspaceData& data);
    void saved(const WorkspaceData& data);

private:
    WorkspaceData m_data;
    QString m_currentPath;
    QString m_lastError;
};
