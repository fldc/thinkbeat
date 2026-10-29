#pragma once
#include <QByteArray>

// Fixed 20 ms windows make detection independent of pipe read boundaries.
class Pulse {
public:
    static constexpr int WindowBytes = 960 * 2;
    double sensitivity = 1.0;
    double level = 0;
    bool lit = false;
    void reset();
    void processWindow(const QByteArray &pcm);
private:
    double m_average = 0;
    int m_hold = 0;
    int m_cooldown = 0;
};
