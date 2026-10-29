#include "pulse.h"
#include <QtEndian>
#include <algorithm>
#include <cmath>

void Pulse::reset() {
    level = m_average = 0;
    lit = false;
    m_hold = m_cooldown = 0;
}

void Pulse::processWindow(const QByteArray &pcm) {
    if (pcm.size() != WindowBytes)
        return;
    double sum = 0;
    for (int i = 0; i < pcm.size(); i += 2) {
        const double sample = qFromLittleEndian<qint16>(pcm.constData() + i) / 32768.0;
        sum += sample * sample;
    }
    const double rms = std::sqrt(sum / (pcm.size() / 2));
    const double gain = std::clamp(sensitivity, 0.25, 8.0);
    level = std::clamp(rms * gain * 4.0, 0.0, 1.0);
    if (m_hold > 0) --m_hold;
    if (m_cooldown > 0) --m_cooldown;
    // A moving floor favors transients. A slow fallback pulse keeps sustained
    // music visible, while a 100 ms refractory period limits LED writes.
    const double floor = 0.008 / gain;
    if (m_cooldown == 0 && rms > floor &&
        (rms > m_average * 1.35 || m_average > floor)) {
        m_hold = 3;
        m_cooldown = rms > m_average * 1.35 ? 5 : 12;
    }
    m_average += (rms - m_average) * 0.06;
    lit = m_hold > 0;
}
