// src/BackendBridge.h
#ifndef BACKENDBRIDGE_H
#define BACKENDBRIDGE_H

#include <QObject>
#include <QVariantList>
#include <QTimer>

class BackendBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BackendBridge(QObject *parent = nullptr);
    QVariantList devices() const { return m_devices; }

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void setThrottle(const QString &ip, int kbps);
    Q_INVOKABLE void toggleSpoof(const QString &ip, bool enabled);

signals:
    void devicesChanged();

private slots:
    void updateStats();

private:
    QVariantList m_devices;
    QTimer *m_statsTimer;
};

#endif // BACKENDBRIDGE_H

// src/BackendBridge.cpp
#include "BackendBridge.h"
#include <QDebug>

BackendBridge::BackendBridge(QObject *parent) : QObject(parent)
{
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &BackendBridge::updateStats);
    m_statsTimer->start(1000); // 1 second updates
}

void BackendBridge::startScan() {
    qDebug() << "Starting network scan...";
    // Dummy device for UI testing
    QVariantMap dev;
    dev["ip"] = "192.168.1.15";
    dev["mac"] = "AA:BB:CC:DD:EE:FF";
    dev["hostname"] = "User's Phone";
    dev["downSpeed"] = "1.2 Mbps";
    dev["upSpeed"] = "0.5 Mbps";
    dev["throttle"] = 1000;
    dev["isSpoofing"] = false;
    
    m_devices.clear();
    m_devices.append(dev);
    emit devicesChanged();
}

void BackendBridge::setThrottle(const QString &ip, int kbps) {
    qDebug() << "Setting throttle for" << ip << "to" << kbps << "kbps";
}

void BackendBridge::toggleSpoof(const QString &ip, bool enabled) {
    qDebug() << (enabled ? "Starting" : "Stopping") << "spoof for" << ip;
}

void BackendBridge::updateStats() {
    // Logic to poll the C++ engine for real-time speeds
    emit devicesChanged();
}
