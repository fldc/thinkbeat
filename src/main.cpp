#include <QGuiApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include "backend.h"
#include "theme.h"
#include "translator.h"

namespace {
int signalFd = -1;
void quitSignal(int) {
    const int saved = errno;
    const char byte = 1;
    if (signalFd >= 0) { const auto ignored = ::write(signalFd, &byte, 1); (void)ignored; }
    errno = saved;
}
}

int main(int argc, char **argv) {
    bool check = false, headless = false;
    for (int i = 1; i < argc; ++i) {
        check |= QByteArray(argv[i]) == "--check";
        headless |= QByteArray(argv[i]) == "--headless";
    }
    if (check || headless) qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("thinkbeat");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("thinkbeat");
    app.setWindowIcon(QIcon::fromTheme("thinkbeat"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Pulse the ThinkPad lid LED to system playback audio.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"check", "Check LED permissions and recorder availability without changing anything."},
                       {"json", "Output the check result as JSON."},
                       {"preview", "Capture playback and show the pulse without touching the LED."},
                       {"start", "Start listening immediately."},
                       {"headless", "Listen without a window; stop with Ctrl+C or SIGTERM."}});
    parser.process(app);
    const QString ledPath = "/sys/class/leds/tpacpi::lid_logo_dot";
    if (check) {
        const bool exists = QFileInfo::exists(ledPath + "/brightness");
        const bool writable = ::access(qPrintable(ledPath + "/brightness"), W_OK) == 0 &&
                              ::access(qPrintable(ledPath + "/trigger"), W_OK) == 0;
        const bool recorder = !QStandardPaths::findExecutable("parec").isEmpty();
        const bool ok = exists && writable && recorder;
        if (parser.isSet("json")) {
            const auto json = QJsonDocument(QJsonObject{{"ok", ok}, {"ledFound", exists}, {"ledWritable", writable},
                                                       {"recorderFound", recorder}, {"ledPath", ledPath}}).toJson(QJsonDocument::Compact);
            std::puts(json.constData());
        } else std::printf("Lid LED: %s\nLED writable: %s\nparec: %s\n", exists ? "found" : "missing", writable ? "yes" : "no (run bin/enable-led)", recorder ? "found" : "missing");
        if (!ok) std::fputs("ThinkBeat is not ready. Check LED access and the parec installation.\n", stderr);
        return ok ? 0 : 1;
    }
    if (parser.isSet("json")) { std::fputs("--json requires --check\n", stderr); return 1; }
    // Two writers cannot safely save and restore the same LED state.
    QLockFile lock(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/thinkbeat-led.lock");
    if (!parser.isSet("preview") && !lock.tryLock(0)) {
        std::fputs("Another ThinkBeat instance owns the lid LED. Use --preview for another window.\n", stderr);
        return 1;
    }
    int signalPipe[2];
    if (::pipe2(signalPipe, O_NONBLOCK | O_CLOEXEC) != 0) { std::perror("signal pipe"); return 1; }
    signalFd = signalPipe[1];
    QSocketNotifier notifier(signalPipe[0], QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
    struct sigaction action{};
    action.sa_handler = quitSignal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend(parser.isSet("preview"));
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    if (!headless) {
        engine.load(QUrl("qrc:/Main.qml"));
        if (engine.rootObjects().isEmpty()) return 1;
    }
    bool failed = false;
    if (headless) QObject::connect(&backend, &Backend::changed, &app, [&] {
        if (!backend.error().isEmpty()) {
            std::fprintf(stderr, "%s\n", qPrintable(backend.error()));
            failed = true;
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
        }
    });
    if (parser.isSet("start") || headless) QTimer::singleShot(0, &backend, &Backend::start);
    app.exec();
    backend.stop();
    signalFd = -1;
    ::close(signalPipe[0]);
    ::close(signalPipe[1]);
    return failed ? 1 : 0;
}
