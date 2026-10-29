#include "translator.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

Translator::Translator(const QString &languageFile, const QString &catalogDirectory, QObject *parent)
    : QTranslator(parent), m_file(languageFile.isEmpty() ? QDir::homePath() + "/.config/omarchy/language" : languageFile), m_catalogs(catalogDirectory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    reload();
}
QString Translator::translate(const char *, const char *source, const char *, int) const {
    return m_strings.value(QString::fromUtf8(source));
}
void Translator::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty()) m_watcher.removePaths(paths);
    if (QFileInfo::exists(m_file)) m_watcher.addPath(m_file);
    const QString directory = QFileInfo(m_file).absolutePath();
    if (QFileInfo::exists(directory)) m_watcher.addPath(directory);
    QString language = "en";
    QFile file(m_file);
    if (file.open(QIODevice::ReadOnly)) {
        const QString code = QString::fromUtf8(file.readLine()).trimmed();
        if (QRegularExpression("^[a-z]{2,3}(-[A-Za-z]{2})?$").match(code).hasMatch()) language = code;
    }
    if (language == m_language) return;
    m_language = language;
    m_strings.clear();
    QStringList codes{"en", language.section('-', 0, 0), language};
    codes.removeDuplicates();
    for (const auto &code : codes) {
        QFile catalog(m_catalogs + '/' + code + ".json");
        if (!catalog.open(QIODevice::ReadOnly)) continue;
        const auto strings = QJsonDocument::fromJson(catalog.readAll()).object();
        for (auto it = strings.begin(); it != strings.end(); ++it)
            if (it.value().isString() && !it.value().toString().isEmpty()) m_strings.insert(it.key(), it.value().toString());
    }
    QString localeName = language;
    QFile registry(qEnvironmentVariable("OMARCHY_PATH") + "/default/i18n/locales.json");
    if (registry.open(QIODevice::ReadOnly)) {
        const QString name = QJsonDocument::fromJson(registry.readAll()).object().value(language).toObject().value("formatLocale").toString();
        if (!name.isEmpty()) localeName = name;
    }
    const QLocale locale(localeName);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());
    emit changed();
}
