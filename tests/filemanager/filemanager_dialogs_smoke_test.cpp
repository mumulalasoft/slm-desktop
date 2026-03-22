#include <QtTest>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QLibraryInfo>
#include <memory>

class FileManagerHostRootStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int width READ width WRITE setWidth)
    Q_PROPERTY(int height READ height WRITE setHeight)
    Q_PROPERTY(QString contextEntryName READ contextEntryName WRITE setContextEntryName)
    Q_PROPERTY(QString renameInputText READ renameInputText WRITE setRenameInputText)
    Q_PROPERTY(QString renameDialogError READ renameDialogError WRITE setRenameDialogError)
    Q_PROPERTY(QVariantMap propertiesEntry READ propertiesEntry WRITE setPropertiesEntry)
    Q_PROPERTY(QVariantMap propertiesStat READ propertiesStat WRITE setPropertiesStat)
    Q_PROPERTY(bool propertiesShowDeviceUsage READ propertiesShowDeviceUsage WRITE setPropertiesShowDeviceUsage)
    Q_PROPERTY(QVariantList propertiesOpenWithApps READ propertiesOpenWithApps WRITE setPropertiesOpenWithApps)
    Q_PROPERTY(int propertiesOpenWithCurrentIndex READ propertiesOpenWithCurrentIndex WRITE setPropertiesOpenWithCurrentIndex)
    Q_PROPERTY(QString propertiesOpenWithName READ propertiesOpenWithName WRITE setPropertiesOpenWithName)
    Q_PROPERTY(int propertiesTabIndex READ propertiesTabIndex WRITE setPropertiesTabIndex)

public:
    explicit FileManagerHostRootStub(QObject *parent = nullptr) : QObject(parent) {}

    int width() const { return m_width; }
    void setWidth(int value) { m_width = value; }

    int height() const { return m_height; }
    void setHeight(int value) { m_height = value; }

    QString contextEntryName() const { return m_contextEntryName; }
    void setContextEntryName(const QString &value) { m_contextEntryName = value; }

    QString renameInputText() const { return m_renameInputText; }
    void setRenameInputText(const QString &value) { m_renameInputText = value; }

    QString renameDialogError() const { return m_renameDialogError; }
    void setRenameDialogError(const QString &value) { m_renameDialogError = value; }

    QVariantMap propertiesEntry() const { return m_propertiesEntry; }
    void setPropertiesEntry(const QVariantMap &value) { m_propertiesEntry = value; }

    QVariantMap propertiesStat() const { return m_propertiesStat; }
    void setPropertiesStat(const QVariantMap &value) { m_propertiesStat = value; }

    bool propertiesShowDeviceUsage() const { return m_propertiesShowDeviceUsage; }
    void setPropertiesShowDeviceUsage(bool value) { m_propertiesShowDeviceUsage = value; }

    QVariantList propertiesOpenWithApps() const { return m_propertiesOpenWithApps; }
    void setPropertiesOpenWithApps(const QVariantList &value) { m_propertiesOpenWithApps = value; }

    int propertiesOpenWithCurrentIndex() const { return m_propertiesOpenWithCurrentIndex; }
    void setPropertiesOpenWithCurrentIndex(int value) { m_propertiesOpenWithCurrentIndex = value; }

    QString propertiesOpenWithName() const { return m_propertiesOpenWithName; }
    void setPropertiesOpenWithName(const QString &value) { m_propertiesOpenWithName = value; }

    int propertiesTabIndex() const { return m_propertiesTabIndex; }
    void setPropertiesTabIndex(int value) { m_propertiesTabIndex = value; }

    Q_INVOKABLE void applyRenameContextEntry() { ++m_applyRenameCallCount; }
    Q_INVOKABLE QString formatStorageBytes(const QVariant &) const { return QStringLiteral("1 KB"); }
    Q_INVOKABLE QString fileTypeDisplay(const QVariant &, const QVariant &) const { return QStringLiteral("File"); }
    Q_INVOKABLE QString formatDateTimeHuman(const QVariant &) const { return QStringLiteral("2026-03-11 12:00"); }
    Q_INVOKABLE QString locationDisplay(const QVariant &, const QVariant &) const { return QStringLiteral("~/"); }
    Q_INVOKABLE void applyPropertiesOpenWithSelection(int index) { m_lastOpenWithSelection = index; }

    int applyRenameCallCount() const { return m_applyRenameCallCount; }
    int lastOpenWithSelection() const { return m_lastOpenWithSelection; }

private:
    int m_width = 1200;
    int m_height = 800;
    QString m_contextEntryName = QStringLiteral("demo.txt");
    QString m_renameInputText = QStringLiteral("demo.txt");
    QString m_renameDialogError;
    QVariantMap m_propertiesEntry{
        {QStringLiteral("name"), QStringLiteral("demo.txt")},
        {QStringLiteral("iconName"), QStringLiteral("text-plain")}
    };
    QVariantMap m_propertiesStat{
        {QStringLiteral("isDir"), false},
        {QStringLiteral("size"), 1024},
        {QStringLiteral("mimeType"), QStringLiteral("text/plain")}
    };
    bool m_propertiesShowDeviceUsage = false;
    QVariantList m_propertiesOpenWithApps;
    int m_propertiesOpenWithCurrentIndex = -1;
    QString m_propertiesOpenWithName;
    int m_propertiesTabIndex = 0;
    int m_applyRenameCallCount = 0;
    int m_lastOpenWithSelection = -1;
};

class FileManagerDialogsSmokeTest : public QObject
{
    Q_OBJECT

private:
    QQmlEngine m_engine;
    QString m_lastError;
    bool m_importPathsConfigured = false;
    FileManagerHostRootStub m_hostRoot;

    static bool hasMissingQtQmlRuntime(const QString &error)
    {
        return error.contains(QStringLiteral("module \"QtQuick\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQuick.Controls\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQuick.Layouts\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQml.WorkerScript\" is not installed"), Qt::CaseInsensitive);
    }

    bool qmlRuntimeAvailable()
    {
        ensureImportPaths();
        const QString componentDir = QString::fromUtf8(FILEMANAGER_QML_COMPONENT_DIR);
        QQmlComponent probe(&m_engine, QUrl::fromLocalFile(componentDir + QLatin1String("/FileManagerDialogs.qml")));
        return probe.isReady() || !hasMissingQtQmlRuntime(probe.errorString());
    }

    void ensureImportPaths()
    {
        if (m_importPathsConfigured) {
            return;
        }
        const QString importRoot = QString::fromUtf8(BUILD_QML_IMPORT_ROOT);
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            m_engine.addImportPath(qtQmlImportsPath);
        }
        m_engine.addImportPath(importRoot);
        m_importPathsConfigured = true;
    }

    void verifyComponentLoads(const QString &fileName)
    {
        const QString componentDir = QString::fromUtf8(FILEMANAGER_QML_COMPONENT_DIR);
        ensureImportPaths();

        QQmlComponent component(&m_engine, QUrl::fromLocalFile(componentDir + QLatin1Char('/') + fileName));
        m_lastError = component.errorString();
        QVERIFY2(component.isReady(), qPrintable(m_lastError));
    }

    std::unique_ptr<QObject> createComponent(const QString &fileName,
                                             const QVariantMap &initialProps = {})
    {
        const QString componentDir = QString::fromUtf8(FILEMANAGER_QML_COMPONENT_DIR);
        ensureImportPaths();

        QQmlComponent component(&m_engine, QUrl::fromLocalFile(componentDir + QLatin1Char('/') + fileName));
        if (!component.isReady()) {
            m_lastError = component.errorString();
            return {};
        }
        QObject *obj = component.createWithInitialProperties(initialProps);
        if (!obj) {
            m_lastError = component.errorString();
            return {};
        }
        return std::unique_ptr<QObject>(obj);
    }

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    void critical_components_load()
    {
        if (!qmlRuntimeAvailable()) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }

        verifyComponentLoads(QStringLiteral("FileManagerDialogs.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerConnectServerDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerBatchOverlayPopup.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerMenus.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerSidebarContextMenu.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerClearRecentsDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerQuickPreviewDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerRenameDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerPropertiesDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerPropertiesDialogContent.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerCompressDialog.qml"));
        verifyComponentLoads(QStringLiteral("FileManagerOpenWithDialog.qml"));
    }

    void rename_dialog_create_and_open()
    {
        if (!qmlRuntimeAvailable()) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        auto obj = createComponent(QStringLiteral("FileManagerRenameDialog.qml"),
                                   {{QStringLiteral("hostRoot"), QVariant::fromValue(static_cast<QObject *>(&m_hostRoot))}});
        QVERIFY2(!!obj, qPrintable(m_lastError));

        const bool invoked = QMetaObject::invokeMethod(obj.get(),
                                                       "openAndFocus",
                                                       Q_ARG(QString, QStringLiteral("archive.tar.gz")),
                                                       Q_ARG(bool, false));
        QVERIFY(invoked);
        QCOMPARE(obj->property("originalName").toString(), QStringLiteral("archive.tar.gz"));
        QCOMPARE(obj->property("originalIsDir").toBool(), false);
    }

    void properties_dialog_create()
    {
        if (!qmlRuntimeAvailable()) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        auto obj = createComponent(QStringLiteral("FileManagerPropertiesDialog.qml"),
                                   {{QStringLiteral("hostRoot"), QVariant::fromValue(static_cast<QObject *>(&m_hostRoot))}});
        QVERIFY2(!!obj, qPrintable(m_lastError));
        QCOMPARE(obj->property("title").toString(), QStringLiteral("Properties"));
    }
};

QTEST_MAIN(FileManagerDialogsSmokeTest)
#include "filemanager_dialogs_smoke_test.moc"
