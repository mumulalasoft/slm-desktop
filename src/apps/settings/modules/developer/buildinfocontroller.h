#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// BuildInfoController — read-only snapshot of build and runtime information.
//
// Exposed to QML as the "BuildInfo" context property.
// All properties are computed once at startup; none are writable.
//
class BuildInfoController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString shellVersion    READ shellVersion    CONSTANT)
    Q_PROPERTY(QString qtVersion       READ qtVersion       CONSTANT)
    Q_PROPERTY(QString buildType       READ buildType       CONSTANT)
    Q_PROPERTY(QString compilerInfo    READ compilerInfo    CONSTANT)
    Q_PROPERTY(QString sessionType     READ sessionType     CONSTANT)
    Q_PROPERTY(QString displayServer   READ displayServer   CONSTANT)
    Q_PROPERTY(QString glRenderer      READ glRenderer      CONSTANT)
    Q_PROPERTY(QString kernelVersion   READ kernelVersion   CONSTANT)
    Q_PROPERTY(QString machineArch     READ machineArch     CONSTANT)
    Q_PROPERTY(QVariantList sections   READ sections        CONSTANT)

public:
    explicit BuildInfoController(QObject *parent = nullptr);

    QString shellVersion()  const { return m_shellVersion;  }
    QString qtVersion()     const { return m_qtVersion;     }
    QString buildType()     const { return m_buildType;     }
    QString compilerInfo()  const { return m_compilerInfo;  }
    QString sessionType()   const { return m_sessionType;   }
    QString displayServer() const { return m_displayServer; }
    QString glRenderer()    const { return m_glRenderer;    }
    QString kernelVersion() const { return m_kernelVersion; }
    QString machineArch()   const { return m_machineArch;   }

    // Grouped list for the QML table view.
    // Each entry: {group: string, key: string, value: string}
    QVariantList sections() const { return m_sections; }

    Q_INVOKABLE QString allAsText() const;

private:
    void build();
    void addSection(const QString &group, const QString &key, const QString &value);

    QString m_shellVersion;
    QString m_qtVersion;
    QString m_buildType;
    QString m_compilerInfo;
    QString m_sessionType;
    QString m_displayServer;
    QString m_glRenderer;
    QString m_kernelVersion;
    QString m_machineArch;
    QVariantList m_sections;
};
