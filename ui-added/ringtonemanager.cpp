#include "ringtonemanager.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtMath>

#include <algorithm>

namespace {
QString safeStem(QString stem)
{
    stem = stem.trimmed();
    stem.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), QStringLiteral("_"));
    return stem.isEmpty() ? QStringLiteral("自定义铃声") : stem.left(60);
}

void writeLittleEndian16(QDataStream &stream, quint16 value) { stream << value; }
void writeLittleEndian32(QDataStream &stream, quint32 value) { stream << value; }
}

bool RingtoneManager::initialize(QString *error)
{
    rootDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/ringtones");
    if (!QDir().mkpath(rootDir_ + QStringLiteral("/builtin"))
        || !QDir().mkpath(rootDir_ + QStringLiteral("/custom"))) {
        if (error) *error = QStringLiteral("无法创建铃声目录：") + rootDir_;
        return false;
    }
    return ensureBuiltInTones(error);
}

bool RingtoneManager::ensureBuiltInTones(QString *error)
{
    const QString builtIn = rootDir_ + QStringLiteral("/builtin/");
    if (!QFileInfo::exists(builtIn + QStringLiteral("classic.wav"))
        && !copyResource(QStringLiteral(":/audio/alarm.wav"),
                         builtIn + QStringLiteral("classic.wav"), error))
        return false;

    if (!QFileInfo::exists(builtIn + QStringLiteral("morning.wav"))
        && !writeTone(builtIn + QStringLiteral("morning.wav"),
                      {523.25, 659.25, 783.99, 659.25}, 310, 0.44, error))
        return false;
    if (!QFileInfo::exists(builtIn + QStringLiteral("bell.wav"))
        && !writeTone(builtIn + QStringLiteral("bell.wav"),
                      {880.0, 1174.66, 880.0, 1318.51}, 240, 0.52, error))
        return false;
    if (!QFileInfo::exists(builtIn + QStringLiteral("focus.wav"))
        && !writeTone(builtIn + QStringLiteral("focus.wav"),
                      {440.0, 554.37, 659.25, 554.37}, 420, 0.36, error))
        return false;
    return true;
}

QVector<RingtoneItem> RingtoneManager::items() const
{
    QVector<RingtoneItem> result = {
        {QStringLiteral("builtin:classic"), QStringLiteral("经典闹铃"),
         rootDir_ + QStringLiteral("/builtin/classic.wav"), true},
        {QStringLiteral("builtin:morning"), QStringLiteral("晨光和弦"),
         rootDir_ + QStringLiteral("/builtin/morning.wav"), true},
        {QStringLiteral("builtin:bell"), QStringLiteral("清脆铃音"),
         rootDir_ + QStringLiteral("/builtin/bell.wav"), true},
        {QStringLiteral("builtin:focus"), QStringLiteral("专注节拍"),
         rootDir_ + QStringLiteral("/builtin/focus.wav"), true}
    };

    QDir custom(rootDir_ + QStringLiteral("/custom"));
    const QStringList filters = {QStringLiteral("*.wav"), QStringLiteral("*.mp3"),
                                 QStringLiteral("*.m4a"), QStringLiteral("*.ogg"),
                                 QStringLiteral("*.flac"), QStringLiteral("*.wma")};
    const auto files = custom.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const auto &file : files) {
        result.append({QStringLiteral("custom:") + file.fileName(), file.completeBaseName(),
                       file.absoluteFilePath(), false});
    }
    return result;
}

RingtoneItem RingtoneManager::item(const QString &id) const
{
    for (const auto &candidate : items())
        if (candidate.id == id && QFileInfo::exists(candidate.path)) return candidate;
    return {};
}

QString RingtoneManager::importFile(const QString &source, QString *error)
{
    const QFileInfo input(source);
    if (!input.exists() || !input.isFile()) {
        if (error) *error = QStringLiteral("选择的音频文件不存在");
        return {};
    }
    if (input.size() > 50LL * 1024LL * 1024LL) {
        if (error) *error = QStringLiteral("单个铃声不能超过 50 MB");
        return {};
    }
    const QStringList allowed = {QStringLiteral("wav"), QStringLiteral("mp3"),
                                 QStringLiteral("m4a"), QStringLiteral("ogg"),
                                 QStringLiteral("flac"), QStringLiteral("wma")};
    const QString suffix = input.suffix().toLower();
    if (!allowed.contains(suffix)) {
        if (error) *error = QStringLiteral("请选择 WAV、MP3、M4A、OGG、FLAC 或 WMA 文件");
        return {};
    }

    const QString stem = safeStem(input.completeBaseName());
    QDir custom(rootDir_ + QStringLiteral("/custom"));
    QString name = stem + QStringLiteral(".") + suffix;
    for (int i = 2; QFileInfo::exists(custom.filePath(name)); ++i)
        name = stem + QStringLiteral(" (") + QString::number(i) + QStringLiteral(").") + suffix;
    const QString target = custom.filePath(name);
    if (!QFile::copy(source, target)) {
        if (error) *error = QStringLiteral("无法复制铃声到应用目录");
        return {};
    }
    return QStringLiteral("custom:") + name;
}

bool RingtoneManager::removeCustom(const QString &id, QString *error)
{
    if (!id.startsWith(QStringLiteral("custom:"))) {
        if (error) *error = QStringLiteral("内置铃声不能删除");
        return false;
    }
    const QString fileName = id.mid(QStringLiteral("custom:").size());
    if (fileName.contains('/') || fileName.contains('\\')) {
        if (error) *error = QStringLiteral("铃声标识无效");
        return false;
    }
    const QString path = QDir(rootDir_ + QStringLiteral("/custom")).filePath(fileName);
    if (!QFileInfo::exists(path)) return true;
    if (!QFile::remove(path)) {
        if (error) *error = QStringLiteral("铃声正在使用或没有删除权限");
        return false;
    }
    return true;
}

bool RingtoneManager::writeTone(const QString &path, const QVector<double> &notes,
                                int noteMs, double volume, QString *error)
{
    constexpr int sampleRate = 44100;
    constexpr int channels = 1;
    QVector<qint16> samples;
    const int samplesPerNote = sampleRate * noteMs / 1000;
    for (double frequency : notes) {
        for (int i = 0; i < samplesPerNote; ++i) {
            const double t = double(i) / sampleRate;
            const double attack = qMin(1.0, i / (sampleRate * 0.018));
            const double release = qMin(1.0, (samplesPerNote - i) / (sampleRate * 0.12));
            const double envelope = qMin(attack, release);
            constexpr double pi = 3.14159265358979323846;
            const double wave = qSin(2.0 * pi * frequency * t)
                              + 0.22 * qSin(4.0 * pi * frequency * t);
            samples.append(qint16(qBound(-32767.0, wave * envelope * volume * 24000.0, 32767.0)));
        }
        const int oldSize = samples.size();
        samples.resize(oldSize + sampleRate / 18);
        std::fill(samples.begin() + oldSize, samples.end(), qint16(0));
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("无法生成内置铃声：") + file.errorString();
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataBytes = quint32(samples.size() * int(sizeof(qint16)));
    out.writeRawData("RIFF", 4); writeLittleEndian32(out, 36 + dataBytes);
    out.writeRawData("WAVEfmt ", 8); writeLittleEndian32(out, 16);
    writeLittleEndian16(out, 1); writeLittleEndian16(out, channels);
    writeLittleEndian32(out, sampleRate);
    writeLittleEndian32(out, sampleRate * channels * 16 / 8);
    writeLittleEndian16(out, channels * 16 / 8); writeLittleEndian16(out, 16);
    out.writeRawData("data", 4); writeLittleEndian32(out, dataBytes);
    for (qint16 sample : samples) out << sample;
    return out.status() == QDataStream::Ok;
}

bool RingtoneManager::copyResource(const QString &resource, const QString &target, QString *error)
{
    QFile input(resource);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法读取内置铃声资源");
        return false;
    }
    QFile output(target);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("无法保存内置铃声：") + output.errorString();
        return false;
    }
    if (output.write(input.readAll()) < 0) {
        if (error) *error = QStringLiteral("写入内置铃声失败");
        return false;
    }
    return true;
}
