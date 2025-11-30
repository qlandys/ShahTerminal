// Lightweight client that runs orderbook_backend.exe and feeds DomWidget.

#pragma once

#include "DomWidget.h"
#include "PrintsWidget.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

class LadderClient : public QObject {
    Q_OBJECT

public:
    explicit LadderClient(const QString &backendPath,
                          const QString &symbol,
                          int levels,
                          DomWidget *dom,
                          QObject *parent = nullptr,
                          class PrintsWidget *prints = nullptr);

    void restart(const QString &symbol, int levels);
    void stop();
    bool isRunning() const;

private slots:
    void handleReadyRead();
    void handleErrorOccurred(QProcess::ProcessError error);
    void handleFinished(int exitCode, QProcess::ExitStatus status);
    void handleWatchdogTimeout();

signals:
    void statusMessage(const QString &message);
    void pingUpdated(int milliseconds);

private:
    void emitStatus(const QString &msg);
    void processLine(const QByteArray &line);
    void armWatchdog();

    QString m_backendPath;
    QString m_symbol;
    int m_levels;
    QProcess m_process;
    QByteArray m_buffer;
    DomWidget *m_dom;
    bool m_initialCenterSent = false;
    class PrintsWidget *m_prints;
    QVector<double> m_lastPrices;
    double m_lastTickSize = 0.0;
    QVector<PrintItem> m_printBuffer;
    QTimer m_watchdogTimer;
    qint64 m_lastUpdateMs = 0;
    const int m_watchdogIntervalMs = 15000;
};
