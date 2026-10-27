#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
    Q_PROPERTY(bool light READ light NOTIFY changed)
    Q_PROPERTY(int fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QVariantMap colors() const { return m_colors; }
    bool light() const { return m_light; }
    int fontSize() const { return m_fontSize; }
    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QVariantMap m_colors;
    bool m_light = false;
    int m_fontSize = 12;
    QString m_directory;
    QString m_accent = "#FFD60A";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
