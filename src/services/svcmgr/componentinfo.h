#pragma once

#include <QString>
#include <QVariantMap>

struct ComponentInfo {
    QString name;           // friendly name, e.g. "envd"
    QString unitName;       // systemd unit, e.g. "slm-envd.service"
    QString displayName;    // label for UI, e.g. "Environment Service"
    QString description;

    // Runtime state (filled by SvcManagerService).
    QString  status;        // active | inactive | failed | activating | unknown
    qint64   pid       = -1;
    qint64   since     = 0; // epoch ms
    double   cpuPct    = 0.0;
    qint64   memKb     = 0;
    int      restartCount = 0;
    int      lastExitCode = 0;

    QVariantMap toVariantMap() const
    {
        return {
            { QStringLiteral("name"),         name         },
            { QStringLiteral("unitName"),      unitName     },
            { QStringLiteral("displayName"),   displayName  },
            { QStringLiteral("description"),   description  },
            { QStringLiteral("status"),        status       },
            { QStringLiteral("pid"),           pid          },
            { QStringLiteral("since"),         since        },
            { QStringLiteral("cpuPct"),        cpuPct       },
            { QStringLiteral("memKb"),         memKb        },
            { QStringLiteral("restartCount"),  restartCount },
            { QStringLiteral("lastExitCode"),  lastExitCode },
        };
    }
};
