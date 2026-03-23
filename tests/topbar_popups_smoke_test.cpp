#include <QtTest>

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QLibraryInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <memory>

namespace {
class ThemeIconControllerStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision CONSTANT)
public:
    explicit ThemeIconControllerStub(QObject *parent = nullptr) : QObject(parent) {}
    int revision() const { return 1; }
};

class UIPreferencesStub : public QObject
{
    Q_OBJECT
public:
    explicit UIPreferencesStub(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariant getPreference(const QString &, const QVariant &fallback = QVariant()) const
    {
        return fallback;
    }

    Q_INVOKABLE void setPreference(const QString &, const QVariant &) {}
    Q_INVOKABLE void setDockMotionPreset(const QString &) {}
    Q_INVOKABLE void setDockDropPulseEnabled(bool) {}
    Q_INVOKABLE void setDockAutoHideEnabled(bool) {}
};
}

class TopBarPopupsSmokeTest : public QObject
{
    Q_OBJECT

private:
    QQmlEngine m_engine;
    ThemeIconControllerStub m_themeIconController;
    UIPreferencesStub m_uiPreferences;
    QQuickWindow m_window;
    QString m_lastComponentError;

    QQuickItem *createTopbarComponent(const QString &fileName)
    {
        const QString importRoot = QString::fromUtf8(BUILD_QML_IMPORT_ROOT);
        const QString topbarDir = QString::fromUtf8(TOPBAR_QML_COMPONENT_DIR);
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            m_engine.addImportPath(qtQmlImportsPath);
        }
        m_engine.addImportPath(importRoot);
        m_engine.rootContext()->setContextProperty(QStringLiteral("ThemeIconController"), &m_themeIconController);
        m_engine.rootContext()->setContextProperty(QStringLiteral("UIPreferences"), &m_uiPreferences);

        QQmlComponent component(&m_engine, QUrl::fromLocalFile(topbarDir + QLatin1Char('/') + fileName));
        if (!component.isReady()) {
            m_lastComponentError = component.errorString();
            qWarning().noquote() << m_lastComponentError;
            return nullptr;
        }
        m_lastComponentError.clear();
        QObject *obj = component.create(m_engine.rootContext());
        auto *item = qobject_cast<QQuickItem *>(obj);
        if (!item) {
            delete obj;
            return nullptr;
        }
        item->setParentItem(m_window.contentItem());
        item->setPosition(QPointF(20, 20));
        item->setWidth(item->implicitWidth() > 0 ? item->implicitWidth() : 40);
        item->setHeight(item->implicitHeight() > 0 ? item->implicitHeight() : 30);
        return item;
    }

    QPoint clickPointFor(const QQuickItem *item) const
    {
        const QPointF scene = item->mapToScene(QPointF(item->width() * 0.5, item->height() * 0.5));
        return QPoint(int(scene.x()), int(scene.y()));
    }

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
        m_window.setWidth(900);
        m_window.setHeight(500);
        m_window.setColor(Qt::black);
        m_window.show();
        QTest::qWait(80);
    }

    void mainMenu_openCloseRepeatedly()
    {
        std::unique_ptr<QQuickItem> item(createTopbarComponent(QStringLiteral("TopBarMainMenuControl.qml")));
        if (!item) {
            if (m_lastComponentError.contains(QStringLiteral("QtQuick.Controls"), Qt::CaseInsensitive) ||
                m_lastComponentError.contains(QStringLiteral("QtQml.WorkerScript"), Qt::CaseInsensitive)) {
                QSKIP("Qt QML runtime modules are unavailable in this environment.");
            }
            QFAIL(qPrintable(QStringLiteral("Failed to create TopBarMainMenuControl: %1").arg(m_lastComponentError)));
        }

        for (int i = 0; i < 20; ++i) {
            QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, clickPointFor(item.get()));
            QTRY_VERIFY(item->property("popupOpen").toBool());

            QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, clickPointFor(item.get()));
            QTRY_VERIFY(!item->property("popupOpen").toBool());
            QTest::qWait(240); // debounce in control
        }
    }

    void screenshot_openCloseAndOutsideClose()
    {
        std::unique_ptr<QQuickItem> item(createTopbarComponent(QStringLiteral("TopBarScreenshotControl.qml")));
        if (!item) {
            if (m_lastComponentError.contains(QStringLiteral("QtQuick.Controls"), Qt::CaseInsensitive) ||
                m_lastComponentError.contains(QStringLiteral("QtQml.WorkerScript"), Qt::CaseInsensitive)) {
                QSKIP("Qt QML runtime modules are unavailable in this environment.");
            }
            QFAIL(qPrintable(QStringLiteral("Failed to create TopBarScreenshotControl: %1").arg(m_lastComponentError)));
        }

        QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, clickPointFor(item.get()));
        QTRY_VERIFY(item->property("popupOpen").toBool());

        // close by outside click
        QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, QPoint(4, m_window.height() - 4));
        QTRY_VERIFY(!item->property("popupOpen").toBool());
        QTest::qWait(240);

        for (int i = 0; i < 12; ++i) {
            QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, clickPointFor(item.get()));
            QTRY_VERIFY(item->property("popupOpen").toBool());

            QTest::mouseClick(&m_window, Qt::LeftButton, Qt::NoModifier, clickPointFor(item.get()));
            QTRY_VERIFY(!item->property("popupOpen").toBool());
            QTest::qWait(240);
        }
    }
};

QTEST_MAIN(TopBarPopupsSmokeTest)
#include "topbar_popups_smoke_test.moc"
