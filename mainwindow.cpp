#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QDirIterator>
#include <QInputDialog>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include <QFileInfo>
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QMap>
#include <QStyle>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setStretchLastSection(false);

    setWindowIcon(QIcon(":/app_icon.png"));
    setWindowTitle("Nafuda");
    ui->stackedWidget->setCurrentIndex(0);
    ui->selectedListWidget->setFrameShape(QFrame::NoFrame);
    ui->lblStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    statusPathLabel = new QLabel(this);
    statusPathLabel->setStyleSheet("padding-left: 5px; color: #555;");
    ui->statusbar->addWidget(statusPathLabel);

    QList<int> sizes;
    sizes << 300 << 600 << 300;
    ui->splitter->setSizes(sizes);

    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 2);
    ui->splitter->setStretchFactor(2, 1);

    ui->listWelcomeRecent->setFocusPolicy(Qt::NoFocus);
    ui->listWelcomeRecent->setCursor(Qt::PointingHandCursor);

    QSettings settings("Nafuda", "Settings");
    recentFiles = settings.value("recentFiles").toStringList();
    updateRecentMenu();

    settings.beginGroup("Presets");
    QStringList keys = settings.childKeys();
    if (keys.isEmpty()) {
        settings.endGroup();
        QString oldTemplate = settings.value("template").toString();
        if (oldTemplate.isEmpty()) oldTemplate = defaultTemplate;
        presets.insert("Default", oldTemplate);
        currentPresetName = "Default";
    } else {
        for (const QString &key : keys) {
            presets.insert(key, settings.value(key).toString());
        }
        settings.endGroup();
        currentPresetName = settings.value("currentPresetName", "Default").toString();
        if (!presets.contains(currentPresetName) && !presets.isEmpty()) {
            currentPresetName = presets.firstKey();
        }
    }
    contentTemplate = presets.value(currentPresetName, defaultTemplate);

    connect(ui->btnWelcomeOpen, &QPushButton::clicked, this, &MainWindow::openFolder);
    connect(ui->listWelcomeRecent, &QListWidget::itemClicked, this, &MainWindow::onWelcomeListClicked);

    connect(ui->btnSelectAll, &QPushButton::clicked, this, &MainWindow::selectAllFiles);
    connect(ui->btnDeselectAll, &QPushButton::clicked, this, &MainWindow::deselectAllFiles);

    connect(ui->actionOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(ui->actionTemplateSettings, &QAction::triggered, this, &MainWindow::openTemplateOptions);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->actionCheckUpdates, &QAction::triggered, this, &MainWindow::checkUpdate);
    connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->actionClearRecent, &QAction::triggered, this, &MainWindow::clearRecentList);

    connect(ui->treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &MainWindow::onTreeItemChanged);
    connect(ui->treeWidget, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentItemChanged);

    connect(ui->btnCopyTree, &QPushButton::clicked, this, &MainWindow::copyDirectoryTree);
    connect(ui->btnCopyContent, &QPushButton::clicked, this, &MainWindow::copyFileContent);
    connect(ui->btnCopyFull, &QPushButton::clicked, this, &MainWindow::copyFullContext);

    netManager = new QNetworkAccessManager(this);
    connect(netManager, &QNetworkAccessManager::finished, this, &MainWindow::onUpdateResult);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::setAllChildCheckState(QTreeWidgetItem *item, Qt::CheckState state) {
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        child->setCheckState(0, state);

        if (child->childCount() > 0) {
            setAllChildCheckState(child, state);
        } else {
            updateFileList(child);
        }
    }
}

void MainWindow::selectAllFiles() {
    if (ui->treeWidget->topLevelItemCount() == 0) return;
    ui->treeWidget->blockSignals(true);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *root = ui->treeWidget->topLevelItem(i);
        root->setCheckState(0, Qt::Checked);
        setAllChildCheckState(root, Qt::Checked);
    }
    ui->treeWidget->blockSignals(false);

    ui->selectedListWidget->clear();
    QDir rootDir(currentRootDir);
    QTreeWidgetItemIterator it(ui->treeWidget);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked && (*it)->childCount() == 0) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            ui->selectedListWidget->addItem(rootDir.relativeFilePath(path));
        }
        ++it;
    }
}

void MainWindow::deselectAllFiles() {
    if (ui->treeWidget->topLevelItemCount() == 0) return;
    ui->treeWidget->blockSignals(true);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *root = ui->treeWidget->topLevelItem(i);
        root->setCheckState(0, Qt::Unchecked);
        setAllChildCheckState(root, Qt::Unchecked);
    }
    ui->treeWidget->blockSignals(false);
    ui->selectedListWidget->clear();
}

void MainWindow::openFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Open Directory", QDir::homePath());
    if (!dir.isEmpty()) {
        loadProject(dir);
    }
}

void MainWindow::loadProject(const QString &path) {
    currentRootDir = path;
    addToRecent(path);

    QDir dir(path);
    setWindowTitle(dir.dirName() + " - Nafuda");
    statusPathLabel->setText("Loaded: " + path);

    ui->stackedWidget->setCurrentIndex(1);
    ui->treeWidget->clear();
    ui->selectedListWidget->clear();
    ui->lblStatus->clear();
    ui->codeViewer->clear();
    ui->lblFileInfo->setText("Select a file to preview info");

    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
    rootItem->setText(0, dir.dirName());
    rootItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    rootItem->setData(0, Qt::UserRole, path);
    rootItem->setCheckState(0, Qt::Unchecked);

    populateTree(path, rootItem);
    ui->treeWidget->expandItem(rootItem);
}

void MainWindow::addToRecent(const QString &path) {
    for (int i = 0; i < recentFiles.size(); ++i) {
        QString entry = recentFiles[i];
        QString entryPath = entry.split('|').first();
        if (entryPath == path) {
            recentFiles.removeAt(i);
            break;
        }
    }

    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    recentFiles.prepend(path + "|" + timestamp);

    while (recentFiles.size() > maxRecentFiles) {
        recentFiles.removeLast();
    }

    QSettings settings("Nafuda", "Settings");
    settings.setValue("recentFiles", recentFiles);
    updateRecentMenu();
}

QString MainWindow::getRelativeTime(const QDateTime &dt) {
    qint64 diff = dt.secsTo(QDateTime::currentDateTime());

    if (diff < 60) return "Just now";
    if (diff < 3600) return QString("%1 mins ago").arg(diff / 60);
    if (diff < 86400) return QString("%1 hours ago").arg(diff / 3600);
    if (diff < 172800) return "Yesterday";
    return dt.toString("dd MMM yyyy");
}

void MainWindow::updateRecentMenu() {
    ui->menuOpenRecent->clear();
    ui->listWelcomeRecent->clear();

    if (recentFiles.isEmpty()) {
        QAction *emptyAction = ui->menuOpenRecent->addAction("No Recent Projects");
        emptyAction->setEnabled(false);

        ui->listWelcomeRecent->hide();
        ui->lblRecentTitle->hide();
    } else {
        ui->listWelcomeRecent->show();
        ui->lblRecentTitle->show();

        for (const QString &entry : recentFiles) {
            QStringList parts = entry.split('|');
            QString path = parts.first();
            QString timeStr = parts.value(1);
            QString relativeTime = "";

            if (!timeStr.isEmpty()) {
                QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
                relativeTime = getRelativeTime(dt);
            }

            QAction *action = ui->menuOpenRecent->addAction(path);
            action->setData(path);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentProject);

            QListWidgetItem *item = new QListWidgetItem();
            QString displayText = QDir(path).dirName();
            if (!relativeTime.isEmpty()) {
                displayText += "\n" + relativeTime;
            }
            item->setText(displayText);
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            item->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
            ui->listWelcomeRecent->addItem(item);
        }
    }

    ui->menuOpenRecent->addSeparator();
    ui->menuOpenRecent->addAction(ui->actionClearRecent);
}

void MainWindow::openRecentProject() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString path = action->data().toString();
        if (QDir(path).exists()) {
            loadProject(path);
        } else {
            QMessageBox::warning(this, "Error", "Directory does not exist anymore.");
            for(int i=0; i<recentFiles.size(); ++i) {
                if(recentFiles[i].startsWith(path + "|") || recentFiles[i] == path) {
                    recentFiles.removeAt(i);
                    break;
                }
            }
            QSettings settings("Nafuda", "Settings");
            settings.setValue("recentFiles", recentFiles);
            updateRecentMenu();
        }
    }
}

void MainWindow::onWelcomeListClicked(QListWidgetItem *item) {
    QString path = item->data(Qt::UserRole).toString();
    if (QDir(path).exists()) {
        loadProject(path);
    } else {
        QMessageBox::warning(this, "Error", "Directory does not exist anymore.");
        for(int i=0; i<recentFiles.size(); ++i) {
            if(recentFiles[i].startsWith(path + "|") || recentFiles[i] == path) {
                recentFiles.removeAt(i);
                break;
            }
        }
        QSettings settings("Nafuda", "Settings");
        settings.setValue("recentFiles", recentFiles);
        updateRecentMenu();
    }
}

void MainWindow::clearRecentList() {
    recentFiles.clear();
    QSettings settings("Nafuda", "Settings");
    settings.setValue("recentFiles", recentFiles);
    updateRecentMenu();
}

void MainWindow::populateTree(const QString &path, QTreeWidgetItem *parentItem) {
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &info : dir.entryInfoList()) {
        if (info.fileName().startsWith(".")) continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, info.fileName());
        item->setData(0, Qt::UserRole, info.filePath());
        item->setCheckState(0, Qt::Unchecked);

        if (info.isDir()) {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
            populateTree(info.filePath(), item);
        } else {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column) {
    QString path = item->data(0, Qt::UserRole).toString();
    QFileInfo info(path);

    if (info.isFile()) {
        double sizeInKB = info.size() / 1024.0;
        QString fileInfoText = QString("<b>File:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Size:</b> %2 KB &nbsp;&nbsp;|&nbsp;&nbsp; <b>Format:</b> %3")
                                   .arg(info.fileName())
                                   .arg(QString::number(sizeInKB, 'f', 2))
                                   .arg(info.suffix().toUpper() + " File");

        ui->lblFileInfo->setTextFormat(Qt::RichText);
        ui->lblFileInfo->setText(fileInfoText);

        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ui->codeViewer->setText(file.readAll());
        }
    } else {
        ui->lblFileInfo->setText("Folder: " + info.fileName());
        ui->codeViewer->clear();
    }
}

void MainWindow::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
    if (current) {
        onTreeItemClicked(current, 0);
    }
}

void MainWindow::onTreeItemChanged(QTreeWidgetItem *item, int column) {
    if (column != 0) return;
    ui->treeWidget->blockSignals(true);

    if (item->childCount() > 0) {
        setAllChildCheckState(item, item->checkState(0));
    } else {
        updateFileList(item);
    }

    ui->treeWidget->blockSignals(false);
}

void MainWindow::updateFileList(QTreeWidgetItem *item) {
    if (currentRootDir.isEmpty()) return;
    QString fullPath = item->data(0, Qt::UserRole).toString();
    QString relPath = QDir(currentRootDir).relativeFilePath(fullPath);

    if (item->checkState(0) == Qt::Checked) {
        if (ui->selectedListWidget->findItems(relPath, Qt::MatchExactly).isEmpty()) {
            ui->selectedListWidget->addItem(relPath);
        }
    } else {
        auto items = ui->selectedListWidget->findItems(relPath, Qt::MatchExactly);
        for (auto *i : items) delete ui->selectedListWidget->takeItem(ui->selectedListWidget->row(i));
    }
}

QString MainWindow::generateAsciiTree(const QString &path, const QString &prefix) {
    QString res; QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    auto list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].fileName().startsWith(".")) continue;
        bool last = (i == list.size() - 1);
        res += prefix + (last ? "└── " : "├── ") + list[i].fileName() + "\n";
        if (list[i].isDir()) res += generateAsciiTree(list[i].filePath(), prefix + (last ? "    " : "│   "));
    }
    return res;
}

void MainWindow::copyFullContext() {
    if (currentRootDir.isEmpty()) return;
    if (ui->selectedListWidget->count() == 0) {
        ui->lblStatus->setText("⚠ No files selected for context!");
        QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
        return;
    }
    QString out = "Project Structure:\n" + QDir(currentRootDir).dirName() + "\n" + generateAsciiTree(currentRootDir, "") + "\n\nFile Contents:\n";
    for(int i = 0; i < ui->selectedListWidget->count(); ++i) {
        QString rel = ui->selectedListWidget->item(i)->text();
        QFile f(QDir(currentRootDir).filePath(rel));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString entry = contentTemplate;
            entry.replace("{name}", rel).replace("{code}", f.readAll());
            out += entry + "\n";
        }
    }
    QApplication::clipboard()->setText(out);
    ui->lblStatus->setText("Full Context Copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::copyDirectoryTree() {
    if (currentRootDir.isEmpty()) return;
    QApplication::clipboard()->setText(generateAsciiTree(currentRootDir, ""));
    ui->lblStatus->setText("Directory Tree Copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::copyFileContent() {
    if (ui->selectedListWidget->count() == 0) {
        ui->lblStatus->setText("⚠ No files selected!");
        QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
        return;
    }
    QString out;
    for(int i = 0; i < ui->selectedListWidget->count(); ++i) {
        QString rel = ui->selectedListWidget->item(i)->text();
        QFile f(QDir(currentRootDir).filePath(rel));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString entry = contentTemplate;
            entry.replace("{name}", rel).replace("{code}", f.readAll());
            out += entry + "\n";
        }
    }
    QApplication::clipboard()->setText(out);
    ui->lblStatus->setText("Content Copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::openTemplateOptions() {
    QDialog dlg(this);
    dlg.setWindowTitle("Template Settings");
    dlg.resize(450, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    QHBoxLayout *topLayout = new QHBoxLayout();
    QComboBox *cmbPresets = new QComboBox(&dlg);
    cmbPresets->addItems(presets.keys());
    cmbPresets->setCurrentText(currentPresetName);

    QToolButton *btnAdd = new QToolButton(&dlg);
    btnAdd->setText("+");
    btnAdd->setToolTip("Create New Preset");

    QToolButton *btnRename = new QToolButton(&dlg);
    btnRename->setText(QString::fromUtf8("\u270E"));
    btnRename->setToolTip("Rename Preset");

    QToolButton *btnDel = new QToolButton(&dlg);
    btnDel->setIcon(qApp->style()->standardIcon(QStyle::SP_TrashIcon));
    btnDel->setToolTip("Delete Selected Preset");

    topLayout->addWidget(new QLabel("Preset:"));
    topLayout->addWidget(cmbPresets, 1);
    topLayout->addWidget(btnAdd);
    topLayout->addWidget(btnRename);
    topLayout->addWidget(btnDel);
    mainLayout->addLayout(topLayout);

    mainLayout->addWidget(new QLabel("Template Content ({name}, {code}):", &dlg));

    QPlainTextEdit *edit = new QPlainTextEdit(&dlg);
    QMap<QString, QString> tempPresets = presets;
    edit->setPlainText(tempPresets.value(currentPresetName, defaultTemplate));
    mainLayout->addWidget(edit);

    connect(cmbPresets, &QComboBox::currentTextChanged, [&, edit](const QString &text){
        if (!text.isEmpty()) {
            edit->setPlainText(tempPresets.value(text, defaultTemplate));
        }
    });

    connect(edit, &QPlainTextEdit::textChanged, [&, edit, cmbPresets](){
        tempPresets[cmbPresets->currentText()] = edit->toPlainText();
    });

    connect(btnAdd, &QToolButton::clicked, [&, cmbPresets, edit](){
        bool ok;
        QString text = QInputDialog::getText(&dlg, "New Preset", "Preset Name:", QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            if (tempPresets.contains(text)) {
                QMessageBox::warning(&dlg, "Error", "Preset name already exists.");
                return;
            }
            tempPresets.insert(text, defaultTemplate);
            cmbPresets->addItem(text);
            cmbPresets->setCurrentText(text);
        }
    });

    connect(btnRename, &QToolButton::clicked, [&, cmbPresets](){
        QString currentName = cmbPresets->currentText();
        bool ok;
        QString text = QInputDialog::getText(&dlg, "Rename Preset", "New Name:", QLineEdit::Normal, currentName, &ok);
        if (ok && !text.isEmpty() && text != currentName) {
            if (tempPresets.contains(text)) {
                QMessageBox::warning(&dlg, "Error", "Preset name already exists.");
                return;
            }
            QString content = tempPresets.take(currentName);
            tempPresets.insert(text, content);

            int idx = cmbPresets->currentIndex();
            cmbPresets->setItemText(idx, text);
        }
    });

    connect(btnDel, &QToolButton::clicked, [&, cmbPresets](){
        if (cmbPresets->count() <= 1) {
            QMessageBox::warning(&dlg, "Warning", "Cannot delete the last preset.");
            return;
        }
        QString toRemove = cmbPresets->currentText();
        int ret = QMessageBox::question(&dlg, "Delete Preset", "Delete preset '" + toRemove + "'?", QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            tempPresets.remove(toRemove);
            cmbPresets->removeItem(cmbPresets->currentIndex());
        }
    });

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnRestoreContent = new QPushButton("Restore Default Content", &dlg);
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);

    bottomLayout->addWidget(btnRestoreContent);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnBox);
    mainLayout->addLayout(bottomLayout);

    connect(btnRestoreContent, &QPushButton::clicked, [edit, this](){
        edit->setPlainText(defaultTemplate);
    });

    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        presets = tempPresets;
        currentPresetName = cmbPresets->currentText();
        contentTemplate = presets.value(currentPresetName, defaultTemplate);

        QSettings settings("Nafuda", "Settings");
        settings.beginGroup("Presets");
        settings.remove("");
        for(auto it = presets.begin(); it != presets.end(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();
        settings.setValue("currentPresetName", currentPresetName);
    }
}

void MainWindow::showAbout()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About Nafuda");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);

    QString aboutText =
        "<h3 style='margin-bottom: 0px;'>Nafuda</h3>"
        "<p style='color: #666;'>Version " + currentVersion + "</p>"
                           "<p>A lightweight tool for copying project structure and providing code context for LLMs.</p>"
                           "<p>Developed with Qt & C++.</p>"
                           "<p><a href=\"https://github.com/ahmadardani/nafuda\">GitHub Repository</a></p>"
                           "<p style='font-size: small;'>&copy; 2025 Ahmad Ardani</p>";

    msgBox.setText(aboutText);

    QPixmap iconPixmap(":/app_icon.png");
    if (!iconPixmap.isNull()) {
        msgBox.setIconPixmap(iconPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    msgBox.exec();
}

void MainWindow::checkUpdate() {
    QNetworkRequest req(QUrl("https://api.github.com/repos/ahmadardani/nafuda/releases/latest"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "NafudaApp");
    netManager->get(req);
}

void MainWindow::onUpdateResult(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        QString remoteVersion = obj["tag_name"].toString();

        if (!remoteVersion.isEmpty()) {

            QString cleanRemote = remoteVersion;
            if (cleanRemote.startsWith("v") || cleanRemote.startsWith("V")) {
                cleanRemote.remove(0, 1);
            }

            QString cleanLocal = currentVersion;
            if (cleanLocal.startsWith("v") || cleanLocal.startsWith("V")) {
                cleanLocal.remove(0, 1);
            }

            if (cleanRemote != cleanLocal) {
                QMessageBox msgBox;
                msgBox.setWindowTitle("Update Available");
                msgBox.setText("<b>New Version Available!</b>");
                msgBox.setInformativeText("Version " + remoteVersion + " is available. Would you like to download it now?");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::Yes);
                msgBox.setIcon(QMessageBox::Information);

                if (msgBox.exec() == QMessageBox::Yes) {
                    QDesktopServices::openUrl(QUrl("https://github.com/ahmadardani/nafuda/releases/latest"));
                }
            } else {
                QMessageBox::information(this, "No Updates",
                                         "You are using the latest version (" + currentVersion + ").");
            }
        }
    } else {
        QMessageBox::warning(this, "Check Failed", "Could not check for updates.\nPlease check your internet connection.");
    }
    reply->deleteLater();
}
