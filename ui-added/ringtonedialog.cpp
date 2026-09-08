#include "ringtonedialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>

RingtoneDialog::RingtoneDialog(RingtoneManager &manager, const QString &selectedId,
                               double volume, QWidget *parent)
    : QDialog(parent), manager_(manager)
{
    setWindowTitle(QStringLiteral("选择闹钟铃声"));
    setMinimumSize(520, 560);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("铃声库"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto *caption = new QLabel(QStringLiteral("内置铃声可直接使用，也可以导入自己的音乐。WAV 最稳定。"), this);
    caption->setObjectName(QStringLiteral("mutedLabel"));
    caption->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(caption);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("ringtoneList"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list_, 1);

    auto *volumeRow = new QHBoxLayout;
    auto *volumeLabel = new QLabel(QStringLiteral("音量"), this);
    volume_ = new QSlider(Qt::Horizontal, this);
    volume_->setRange(0, 100);
    volume_->setValue(qRound(qBound(0.0, volume, 1.0) * 100));
    volumeRow->addWidget(volumeLabel);
    volumeRow->addWidget(volume_, 1);
    layout->addLayout(volumeRow);

    auto *actions = new QHBoxLayout;
    preview_ = new QPushButton(QStringLiteral("试听"), this);
    auto *import = new QPushButton(QStringLiteral("导入音频"), this);
    remove_ = new QPushButton(QStringLiteral("删除自定义"), this);
    preview_->setProperty("uiRole", QStringLiteral("softButton"));
    import->setProperty("uiRole", QStringLiteral("secondaryButton"));
    remove_->setProperty("uiRole", QStringLiteral("ghostButton"));
    actions->addWidget(preview_);
    actions->addWidget(import);
    actions->addWidget(remove_);
    actions->addStretch();
    layout->addLayout(actions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("使用此铃声"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("uiRole", QStringLiteral("primaryButton"));
    buttons->button(QDialogButtonBox::Cancel)->setProperty("uiRole", QStringLiteral("secondaryButton"));
    layout->addWidget(buttons);

    player_.setAudioOutput(&audio_);
    audio_.setVolume(volume_->value() / 100.0);
    reload(selectedId);

    connect(list_, &QListWidget::itemSelectionChanged, this, &RingtoneDialog::updateButtons);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this] { togglePreview(); });
    connect(preview_, &QPushButton::clicked, this, &RingtoneDialog::togglePreview);
    connect(import, &QPushButton::clicked, this, &RingtoneDialog::importRingtone);
    connect(remove_, &QPushButton::clicked, this, &RingtoneDialog::deleteSelected);
    connect(volume_, &QSlider::valueChanged, this, [this](int value) { audio_.setVolume(value / 100.0); });
    connect(&player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        preview_->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("停止试听") : QStringLiteral("试听"));
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

RingtoneDialog::~RingtoneDialog() { player_.stop(); }

QString RingtoneDialog::selectedId() const
{
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString() : RingtoneManager::defaultId();
}

double RingtoneDialog::volume() const { return volume_->value() / 100.0; }

void RingtoneDialog::reload(const QString &selectId)
{
    list_->clear();
    int selectRow = 0;
    const auto all = manager_.items();
    for (int i = 0; i < all.size(); ++i) {
        const auto &ringtone = all[i];
        auto *entry = new QListWidgetItem(
            (ringtone.builtIn ? QStringLiteral("内置  ·  ") : QStringLiteral("我的  ·  ")) + ringtone.name,
            list_);
        entry->setData(Qt::UserRole, ringtone.id);
        entry->setToolTip(ringtone.path);
        if (ringtone.id == selectId) selectRow = i;
    }
    if (list_->count()) list_->setCurrentRow(selectRow);
    updateButtons();
}

void RingtoneDialog::togglePreview()
{
    if (player_.playbackState() == QMediaPlayer::PlayingState) {
        player_.stop();
        return;
    }
    const auto ringtone = manager_.item(selectedId());
    if (ringtone.path.isEmpty()) return;
    player_.setSource(QUrl::fromLocalFile(ringtone.path));
    player_.play();
}

void RingtoneDialog::importRingtone()
{
    const QString source = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入铃声"), {},
        QStringLiteral("音频 (*.wav *.mp3 *.m4a *.ogg *.flac *.wma)"));
    if (source.isEmpty()) return;
    QString error;
    const QString id = manager_.importFile(source, &error);
    if (id.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    reload(id);
}

void RingtoneDialog::deleteSelected()
{
    const auto current = manager_.item(selectedId());
    if (current.builtIn) return;
    if (QMessageBox::question(this, QStringLiteral("删除铃声"),
                              QStringLiteral("从 WakeAI 的铃声库删除“%1”？\n原始文件不会受影响。").arg(current.name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    player_.stop();
    QString error;
    if (!manager_.removeCustom(current.id, &error)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    reload(RingtoneManager::defaultId());
}

void RingtoneDialog::updateButtons()
{
    const auto current = manager_.item(selectedId());
    const bool valid = !current.path.isEmpty();
    preview_->setEnabled(valid);
    remove_->setEnabled(valid && !current.builtIn);
}
