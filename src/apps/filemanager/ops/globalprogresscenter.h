#pragma once

#include <QObject>
#include <QHash>

class GlobalProgressCenter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(bool indeterminate READ indeterminate NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool canPause READ canPause NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)

public:
    explicit GlobalProgressCenter(QObject *parent = nullptr);

    bool visible() const;
    QString source() const;
    QString title() const;
    QString detail() const;
    double progress() const;
    bool indeterminate() const;
    bool paused() const;
    bool canPause() const;
    bool canCancel() const;

    Q_INVOKABLE void showProgress(const QString &source,
                                  const QString &title,
                                  const QString &detail,
                                  double progress,
                                  bool indeterminate,
                                  bool canPause,
                                  bool paused,
                                  bool canCancel);
    Q_INVOKABLE void hideProgress(const QString &source);
    Q_INVOKABLE void requestPauseResume();
    Q_INVOKABLE void requestCancel();

signals:
    void changed();
    void pauseRequested(const QString &source);
    void resumeRequested(const QString &source);
    void cancelRequested(const QString &source);

private:
    struct Entry {
        QString source;
        QString title;
        QString detail;
        double progress = 0.0;
        bool indeterminate = false;
        bool paused = false;
        bool canPause = false;
        bool canCancel = false;
        qint64 updatedAtMs = 0;
    };

    void refreshActiveSource();

    QHash<QString, Entry> m_entries;
    QString m_activeSource;
};
