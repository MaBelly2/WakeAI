#pragma once

#include <QString>
#include <QVector>

struct RingtoneItem
{
    QString id;
    QString name;
    QString path;
    bool builtIn = false;
};

class RingtoneManager
{
public:
    bool initialize(QString *error = nullptr);
    QVector<RingtoneItem> items() const;
    RingtoneItem item(const QString &id) const;
    QString importFile(const QString &source, QString *error = nullptr);
    bool removeCustom(const QString &id, QString *error = nullptr);
    QString directory() const { return rootDir_; }

    static QString defaultId() { return QStringLiteral("builtin:classic"); }

private:
    bool ensureBuiltInTones(QString *error);
    static bool writeTone(const QString &path, const QVector<double> &notes,
                          int noteMs, double volume, QString *error);
    static bool copyResource(const QString &resource, const QString &target, QString *error);

    QString rootDir_;
};
