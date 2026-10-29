#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include "led.h"
#include "pulse.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool lit READ lit NOTIFY changed)
    Q_PROPERTY(double level READ level NOTIFY changed)
    Q_PROPERTY(double sensitivity READ sensitivity WRITE setSensitivity NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool preview READ preview CONSTANT)
public:
    explicit Backend(bool preview = false, const QString &ledDirectory = "/sys/class/leds/tpacpi::lid_logo_dot",
                     const QString &recorder = "parec", QObject *parent = nullptr);
    ~Backend() override;
    bool running() const { return m_running; }
    bool lit() const { return m_pulse.lit; }
    double level() const { return m_pulse.level; }
    double sensitivity() const { return m_pulse.sensitivity; }
    QString error() const { return m_error; }
    bool preview() const { return m_preview; }
    void setSensitivity(double value);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
signals:
    void changed();
private:
    void consume();
    void fail(const QString &message);
    Led m_led;
    Pulse m_pulse;
    QProcess m_audio;
    QTimer m_watchdog;
    QByteArray m_pending, m_stderr;
    QString m_error, m_recorder;
    bool m_preview = false, m_running = false, m_stopping = false;
};
