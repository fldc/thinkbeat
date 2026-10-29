#pragma once
#include <QString>

class Led {
public:
    explicit Led(const QString &directory = "/sys/class/leds/tpacpi::lid_logo_dot");
    ~Led();
    bool acquire(QString *error);
    bool set(bool on, QString *error);
    bool restore(QString *error = nullptr);
    bool acquired() const { return m_acquired; }
private:
    bool write(const QString &name, const QByteArray &value, QString *error);
    QString m_directory;
    QByteArray m_brightness, m_trigger;
    bool m_acquired = false;
    int m_max = 0;
    int m_last = -1;
};
