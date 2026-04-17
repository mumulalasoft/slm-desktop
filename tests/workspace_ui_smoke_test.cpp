#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QLibraryInfo>
#include <memory>

// Mock objects to provide context to QML components
class WorkspaceHostStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool workspaceVisible READ workspaceVisible WRITE setWorkspaceVisible NOTIFY workspaceVisibleChanged)
    Q_PROPERTY(int activeSpace READ activeSpace WRITE setActiveSpace NOTIFY activeSpaceChanged)
    Q_PROPERTY(int spaceCount READ spaceCount WRITE setSpaceCount NOTIFY spaceCountChanged)
    Q_PROPERTY(QVariantList workspaceModel READ workspaceModel NOTIFY workspacesChanged)

public:
    explicit WorkspaceHostStub(QObject *parent = nullptr) : QObject(parent), m_workspaceVisible(false), m_activeSpace(1), m_spaceCount(2) {}

    bool workspaceVisible() const { return m_workspaceVisible; }
    void setWorkspaceVisible(bool v) { if (m_workspaceVisible != v) { m_workspaceVisible = v; emit workspaceVisibleChanged(); } }

    int activeSpace() const { return m_activeSpace; }
    void setActiveSpace(int v) { if (m_activeSpace != v) { m_activeSpace = v; emit activeSpaceChanged(); } }

    int spaceCount() const { return m_spaceCount; }
    void setSpaceCount(int v) { if (m_spaceCount != v) { m_spaceCount = v; emit spaceCountChanged(); } }

    QVariantList workspaceModel() const {
        QVariantList model;
        for (int i = 1; i <= m_spaceCount; ++i) {
            QVariantMap w;
            w["id"] = i;
            w["index"] = i;
            w["isActive"] = (i == m_activeSpace);
            w["isEmpty"] = true;
            w["isTrailingEmpty"] = (i == m_spaceCount);
            w["windowCount"] = 0;
            model << w;
        }
        return model;
    }

    Q_INVOKABLE void toggleWorkspace() { setWorkspaceVisible(!m_workspaceVisible); }
    Q_INVOKABLE void switchWorkspace(int index) { setActiveSpace(index); }
    Q_INVOKABLE bool moveWindowToWorkspace(const QString &, int) { return true; }

signals:
    void workspaceVisibleChanged();
    void activeSpaceChanged();
    void spaceCountChanged();
    void workspacesChanged();

private:
    bool m_workspaceVisible = false;
    int m_activeSpace = 1;
    int m_spaceCount = 2;
};

class WorkspaceUISmokeTest : public QObject
{
    Q_OBJECT

private:
    QQmlEngine m_engine;
    WorkspaceHostStub m_host;
    QString m_lastError;

    bool isEnvironmentImportIssue(const QString &error) const
    {
        const QString e = error.toLower();
        return e.contains(QStringLiteral("filemanagerbatch.js unavailable"))
               || e.contains(QStringLiteral("no such file or directory"))
               || e.contains(QStringLiteral("\"desktop_shell\" is ambiguous"));
    }

    void ensureImportPaths()
    {
        const char* importRoot = std::getenv("BUILD_QML_IMPORT_ROOT");
        if (importRoot) {
            m_engine.addImportPath(QString::fromUtf8(importRoot));
        }
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            m_engine.addImportPath(qtQmlImportsPath);
        }
    }

    bool verifyComponentLoads(const QString &fileName)
    {
        const char* componentDir = std::getenv("WORKSPACE_QML_COMPONENT_DIR");
        if (!componentDir) return false;

        ensureImportPaths();
        m_engine.rootContext()->setContextProperty("MultitaskingController", &m_host);
        m_engine.rootContext()->setContextProperty("SpacesManager", &m_host);
        
        // Mocking WindowThumbnailLayoutEngine if needed
        // m_engine.rootContext()->setContextProperty("WindowThumbnailLayoutEngine", ...);

        QQmlComponent component(&m_engine, QUrl::fromLocalFile(QString::fromUtf8(componentDir) + "/" + fileName));
        if (!component.isReady()) {
            m_lastError = component.errorString();
            return false;
        }
        return true;
    }

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    void testComponentsLoad()
    {
        if (!verifyComponentLoads("WorkspaceStrip.qml")) {
            if (isEnvironmentImportIssue(m_lastError)) {
                QSKIP(qPrintable(QStringLiteral("workspace qml import env incomplete: %1").arg(m_lastError)));
            }
            QFAIL(qPrintable(m_lastError));
        }
        QVERIFY2(verifyComponentLoads("WindowThumbnail.qml"), qPrintable(m_lastError));
        QVERIFY2(verifyComponentLoads("WorkspaceOverviewScene.qml"), qPrintable(m_lastError));
        QVERIFY2(verifyComponentLoads("WorkspaceOverlay.qml"), qPrintable(m_lastError));
    }
};

QTEST_MAIN(WorkspaceUISmokeTest)
#include "workspace_ui_smoke_test.moc"
