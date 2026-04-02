#pragma once
#include "settingbinding.h"
#include <QDebug>

/**
 * @brief Simple in-memory implementation of SettingBinding for testing and demonstration.
 */
class MockBinding : public SettingBinding {
    Q_OBJECT
public:
    explicit MockBinding(const QVariant &initial, QObject *parent = nullptr)
        : SettingBinding(parent), m_value(initial) {}

    QVariant value() const override { return m_value; }
    void setValue(const QVariant &newValue) override {
        beginOperation();
        if (m_value != newValue) {
            m_value = newValue;
            emit valueChanged();
            
            // Simulate "Instant-apply" behavior
            qDebug() << "MockBinding: Applied value" << newValue;
        }
        endOperation(QStringLiteral("write"), true);
    }

private:
    QVariant m_value;
};
