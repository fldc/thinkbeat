#include <QtTest>
#include <QtEndian>
#include <QTemporaryDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include "backend.h"
#include "pulse.h"
#include "led.h"
#include "theme.h"
#include "translator.h"

class ThinkBeatTests : public QObject {
    Q_OBJECT
    static void write(const QString &path, const QByteArray &data) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    }
    static QByteArray read(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll().trimmed();
    }
    static void fakeLed(const QString &path) {
        write(path + "/brightness", "17\n");
        write(path + "/max_brightness", "255\n");
        write(path + "/trigger", "none [timer]\n");
    }
    static QByteArray pcm(qint16 value) {
        QByteArray bytes(Pulse::WindowBytes, '\0');
        for (int i = 0; i < bytes.size(); i += 2) qToLittleEndian<qint16>(value, bytes.data() + i);
        return bytes;
    }
private slots:
    void silenceIsDark() {
        Pulse pulse;
        for (int i = 0; i < 100; ++i) pulse.processWindow(pcm(0));
        QCOMPARE(pulse.level, 0.0);
        QVERIFY(!pulse.lit);
    }
    void onsetPulsesAndSilenceReleases() {
        Pulse pulse;
        pulse.processWindow(pcm(12000));
        QVERIFY(pulse.lit);
        QVERIFY(pulse.level > .9);
        for (int i = 0; i < 4; ++i) pulse.processWindow(pcm(0));
        QVERIFY(!pulse.lit);
    }
    void sustainedSoundBlinksRatherThanStayingOn() {
        Pulse pulse;
        int rises = 0;
        bool previous = false;
        for (int i = 0; i < 100; ++i) {
            pulse.processWindow(pcm(6000));
            if (pulse.lit && !previous) ++rises;
            previous = pulse.lit;
        }
        QVERIFY(rises >= 5);
        QVERIFY(rises <= 12);
    }
    void sensitivityAndNoiseFloor() {
        Pulse low, high;
        low.sensitivity = .25;
        high.sensitivity = 8;
        low.processWindow(pcm(100));
        high.processWindow(pcm(100));
        QVERIFY(!low.lit);
        QVERIFY(high.lit);
        QVERIFY(high.level > low.level);
        high.reset();
        QCOMPARE(high.level, 0.0);
        QVERIFY(!high.lit);
    }
    void signedSamplesAndMalformedWindows() {
        Pulse positive, negative;
        positive.processWindow(pcm(10000));
        negative.processWindow(pcm(-10000));
        QCOMPARE(positive.level, negative.level);
        positive.processWindow(QByteArray(1, 'x'));
        QCOMPARE(positive.level, negative.level);
        negative.processWindow(pcm(-32768));
        QCOMPARE(negative.level, 1.0);
    }
    void ledRestoresBrightnessAndTrigger() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        QString error;
        {
            Led led(dir.path());
            QVERIFY2(led.acquire(&error), qPrintable(error));
            QCOMPARE(read(dir.filePath("trigger")), QByteArray("none"));
            QVERIFY(led.set(true, &error));
            QCOMPARE(read(dir.filePath("brightness")), QByteArray("255"));
            QVERIFY(led.set(false, &error));
            QCOMPARE(read(dir.filePath("brightness")), QByteArray("0"));
        }
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("17"));
        QCOMPARE(read(dir.filePath("trigger")), QByteArray("timer"));
    }
    void missingLedIsActionable() {
        QTemporaryDir dir;
        Led led(dir.path());
        QString error;
        QVERIFY(!led.acquire(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!led.acquired());
    }
    void invalidBrightnessDoesNotChangeTrigger() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        // A directory fails even when tests happen to run with root privileges.
        QVERIFY(QFile::remove(dir.filePath("brightness")));
        QVERIFY(QDir().mkdir(dir.filePath("brightness")));
        Led led(dir.path());
        QString error;
        QVERIFY(!led.acquire(&error));
        QCOMPARE(read(dir.filePath("trigger")), QByteArray("none [timer]"));
    }
    void writeFailureStillRestoresTrigger() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        Led led(dir.path());
        QString error;
        QVERIFY(led.acquire(&error));
        QVERIFY(QFile::remove(dir.filePath("brightness")));
        QVERIFY(QDir().mkdir(dir.filePath("brightness")));
        QVERIFY(!led.set(true, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!led.restore(&error));
        QCOMPARE(read(dir.filePath("trigger")), QByteArray("timer"));
    }
    void missingRecorderDoesNotTouchLed() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        Backend backend(false, dir.path(), dir.filePath("no-such-recorder"));
        backend.start();
        QVERIFY(!backend.running());
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("17"));
    }
    void captureFailureRestoresLed() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        const QString recorder = dir.filePath("recorder");
        write(recorder, "#!/bin/sh\necho 'test server unavailable' >&2\nexit 1\n");
        QVERIFY(QFile::setPermissions(recorder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(false, dir.path(), recorder);
        backend.start();
        QTRY_VERIFY(!backend.running());
        QVERIFY(backend.error().contains("test server unavailable"));
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("17"));
        QCOMPARE(read(dir.filePath("trigger")), QByteArray("timer"));
    }
    void captureProcessesPartialSamplesAndRestores() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        write(dir.filePath("pcm"), pcm(10000));
        const QString recorder = dir.filePath("recorder");
        // Deliver a split sample to exercise framing across process reads.
        write(recorder, "#!/bin/sh\nhead -c 1 '" + dir.filePath("pcm").toUtf8() + "'\nsleep .05\ntail -c +2 '" + dir.filePath("pcm").toUtf8() + "'\nsleep 5\n");
        QVERIFY(QFile::setPermissions(recorder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(false, dir.path(), recorder);
        backend.start();
        QTRY_VERIFY(backend.lit());
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("255"));
        backend.stop();
        QVERIFY(!backend.running());
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("17"));
        QCOMPARE(read(dir.filePath("trigger")), QByteArray("timer"));
    }
    void watchdogTurnsLedOffAndRestores() {
        QTemporaryDir dir;
        fakeLed(dir.path());
        const QString recorder = dir.filePath("recorder");
        write(recorder, "#!/bin/sh\nexec sleep 20\n");
        QVERIFY(QFile::setPermissions(recorder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(false, dir.path(), recorder);
        backend.start();
        QTRY_VERIFY_WITH_TIMEOUT(!backend.running(), 4000);
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(read(dir.filePath("brightness")), QByteArray("17"));
    }
    void themeWatchesReplacement() {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("theme");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        write(dir.filePath("theme/colors.toml"), "accent = '#abcdef'\n");
        QTRY_COMPARE(theme.accent(), QString("#abcdef"));
        QVERIFY(QFile::remove(dir.filePath("theme/colors.toml")));
        write(dir.filePath("theme/colors.toml"), "accent = '#112233'\n");
        QTRY_COMPARE(theme.accent(), QString("#112233"));
    }
    void translatorFollowsLanguageAndFallsBack() {
        QTemporaryDir dir;
        Translator translator(dir.filePath("language"), ":/i18n");
        QCOMPARE(translator.language(), QString("en"));
        write(dir.filePath("language"), "sv\n");
        QTRY_COMPARE(translator.language(), QString("sv"));
        QCOMPARE(translator.translate("Main", "Sensitivity"), QString::fromUtf8("Känslighet"));
        QVERIFY(translator.translate("Main", "Unknown text").isNull());
        QVERIFY(QFile::remove(dir.filePath("language")));
        write(dir.filePath("language"), "da\n");
        QTRY_COMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("Main", "Start listening"), QString("Start lytning"));
    }
    void realQmlKeyboardAndTranslation() {
        QTemporaryDir dir;
        write(dir.filePath("language"), "en\n");
        write(dir.filePath("pcm"), pcm(10000));
        const QString recorder = dir.filePath("recorder");
        write(recorder, "#!/bin/sh\ncat '" + dir.filePath("pcm").toUtf8() + "'\nexec sleep 20\n");
        QVERIFY(QFile::setPermissions(recorder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Backend backend(true, dir.path(), recorder);
        Theme theme(dir.path());
        Translator translator(dir.filePath("language"));
        qGuiApp->installTranslator(&translator);
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings.append(error.toString());
        });
        connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(backend.lit());
        QVERIFY(backend.running());
        QTest::keyClick(window, Qt::Key_Question);
        QTRY_VERIFY(window->property("helpVisible").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->property("helpVisible").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!backend.running());
        write(dir.filePath("language"), "da\n");
        QTRY_COMPARE(translator.language(), QString("da"));
        auto button = window->findChild<QObject *>("toggleButton");
        QVERIFY(button);
        QTRY_COMPARE(button->property("text").toString(), QString("Start lytning"));
        QTest::qWait(100);
        if (!qEnvironmentVariableIsEmpty("THINKBEAT_TEST_SCREENSHOT"))
            QVERIFY(window->grabWindow().save(qEnvironmentVariable("THINKBEAT_TEST_SCREENSHOT") + ".da.png"));
        // Capture clean, synthetic content without opening the user's desktop.
        write(dir.filePath("language"), "en\n");
        QTRY_COMPARE(translator.language(), QString("en"));
        auto buttonItem = qobject_cast<QQuickItem *>(button);
        QVERIFY(buttonItem);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          buttonItem->mapToScene(QPointF(buttonItem->width() / 2, buttonItem->height() / 2)).toPoint());
        QTRY_VERIFY(backend.lit());
        QTest::qWait(100);
        if (!qEnvironmentVariableIsEmpty("THINKBEAT_TEST_SCREENSHOT")) {
            const QImage image = window->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(qEnvironmentVariable("THINKBEAT_TEST_SCREENSHOT")));
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        qGuiApp->removeTranslator(&translator);
    }
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    ThinkBeatTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "thinkbeat_tests.moc"
