#include "led.h"
#include <QFile>
#include <QCoreApplication>
#include <QRegularExpression>

Led::Led(const QString &directory) : m_directory(directory) {}
Led::~Led() { restore(); }

bool Led::write(const QString &name, const QByteArray &value, QString *error) {
    QFile file(m_directory + '/' + name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        file.write(value + '\n') != value.size() + 1 || !file.flush()) {
        if (error) *error = QCoreApplication::translate("Led", "Cannot write %1: %2. Run bin/enable-led from the project directory.")
                                .arg(file.fileName(), file.errorString());
        return false;
    }
    return true;
}

bool Led::acquire(QString *error) {
    if (m_acquired) return true;
    auto read = [this](const QString &name) {
        QFile file(m_directory + '/' + name);
        return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
    };
    m_brightness = read("brightness");
    bool maxOk = false, brightnessOk = false;
    m_max = read("max_brightness").toInt(&maxOk);
    const int original = m_brightness.toInt(&brightnessOk);
    const auto match = QRegularExpression("\\[([^]]+)\\]").match(QString::fromUtf8(read("trigger")));
    if (!maxOk || m_max <= 0 || !brightnessOk || original < 0 || original > m_max || !match.hasMatch()) {
        if (error) *error = QCoreApplication::translate("Led", "ThinkPad lid LED is missing or unreadable.");
        return false;
    }
    m_trigger = match.captured(1).toUtf8();
    if (!write("trigger", "none", error)) return false;
    m_acquired = true;
    m_last = -1;
    if (!set(false, error)) {
        restore();
        return false;
    }
    return true;
}

bool Led::set(bool on, QString *error) {
    if (!m_acquired) return false;
    const int value = on ? m_max : 0;
    if (m_last == value) return true;
    if (!write("brightness", QByteArray::number(value), error)) return false;
    m_last = value;
    return true;
}

bool Led::restore(QString *error) {
    if (!m_acquired) return true;
    // Restore brightness before the trigger, which may take control immediately.
    const bool brightness = write("brightness", m_brightness, error);
    const bool trigger = write("trigger", m_trigger, error);
    m_acquired = false;
    return brightness && trigger;
}
