#include "backend.h"
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

Backend::Backend(bool preview, const QString &directory, const QString &recorder, QObject *parent)
    : QObject(parent), m_led(directory), m_recorder(recorder), m_preview(preview) {
    connect(&m_audio, &QProcess::readyReadStandardOutput, this, &Backend::consume);
    connect(&m_audio, &QProcess::readyReadStandardError, this, [this] {
        m_stderr = (m_stderr + m_audio.readAllStandardError()).right(2048);
    });
    connect(&m_audio, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_stopping && error == QProcess::FailedToStart)
            fail(tr("Cannot start parec: %1").arg(m_audio.errorString()));
    });
    connect(&m_audio, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (!m_stopping && m_running) {
            m_stderr += m_audio.readAllStandardError();
            fail(tr("Playback capture stopped: %1").arg(QString::fromUtf8(m_stderr).trimmed()));
        }
    });
    m_watchdog.setInterval(2000);
    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        fail(tr("No playback data received. Check your PipeWire/PulseAudio server, then start again."));
    });
}

Backend::~Backend() { stop(); }
void Backend::setSensitivity(double value) {
    if (!std::isfinite(value)) return;
    m_pulse.sensitivity = std::clamp(value, .25, 8.0);
    emit changed();
}
void Backend::toggle() { if (m_running) stop(); else start(); }
void Backend::start() {
    if (m_running) return;
    m_error.clear();
    if (QStandardPaths::findExecutable(m_recorder).isEmpty()) {
        fail(tr("parec is missing. Install libpulse (or pulseaudio-utils on Debian/Ubuntu)."));
        return;
    }
    if (!m_preview && !m_led.acquire(&m_error)) {
        emit changed();
        return;
    }
    m_pulse.reset();
    m_pending.clear();
    m_stderr.clear();
    m_running = true;
    // The default monitor is playback audio, never the default microphone.
    m_audio.start(m_recorder, {"--device=@DEFAULT_MONITOR@", "--raw", "--format=s16le",
                              "--rate=48000", "--channels=1", "--latency-msec=30"});
    m_watchdog.start();
    emit changed();
}
void Backend::stop() {
    m_stopping = true;
    m_watchdog.stop();
    m_running = false;
    if (m_audio.state() != QProcess::NotRunning) {
        m_audio.terminate();
        if (!m_audio.waitForFinished(1000)) {
            m_audio.kill();
            m_audio.waitForFinished(1000);
        }
    }
    QString restoreError;
    if (!m_led.restore(&restoreError)) m_error = restoreError;
    m_pending.clear();
    m_pulse.reset();
    m_stopping = false;
    emit changed();
}
void Backend::fail(const QString &message) {
    m_error = message;
    stop();
}
void Backend::consume() {
    const QByteArray data = m_audio.readAllStandardOutput();
    if (!m_running || data.isEmpty()) return;
    m_watchdog.start();
    m_pending += data;
    // Bound latency after a stalled UI. Retain sample alignment when dropping.
    if (m_pending.size() > Pulse::WindowBytes * 10)
        m_pending.remove(0, ((m_pending.size() - Pulse::WindowBytes * 2) / Pulse::WindowBytes) * Pulse::WindowBytes);
    while (m_pending.size() >= Pulse::WindowBytes) {
        m_pulse.processWindow(m_pending.left(Pulse::WindowBytes));
        m_pending.remove(0, Pulse::WindowBytes);
        if (!m_preview && !m_led.set(m_pulse.lit, &m_error)) {
            const QString message = m_error;
            fail(message);
            return;
        }
    }
    emit changed();
}
