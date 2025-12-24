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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(QIcon(":/app_icon.png"));
    ui->stackedWidget->setCurrentIndex(0);
    ui->selectedListWidget->setFrameShape(QFrame::NoFrame);
    ui->lblStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QList<int> sizes;
    sizes << 300 << 600 << 300;
    ui->splitter->setSizes(sizes);

    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 2);
    ui->splitter->setStretchFactor(2, 1);

    QSettings settings("Nafuda", "Settings");
    contentTemplate = settings.value("template", "File: {name}\n```\n{code}\n```\n").toString();

    connect(ui->btnWelcomeOpen, &QPushButton::clicked, this, &MainWindow::openFolder);
    connect(ui->btnSelectAll, &QPushButton::clicked, this, &MainWindow::selectAllFiles);
    connect(ui->btnDeselectAll, &QPushButton::clicked, this, &MainWindow::deselectAllFiles);

    connect(ui->actionOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(ui->actionTemplateSettings, &QAction::triggered, this, &MainWindow::openTemplateOptions);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);

    connect(ui->treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &MainWindow::onTreeItemChanged);
    connect(ui->treeWidget, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentItemChanged);

    connect(ui->btnCopyTree, &QPushButton::clicked, this, &MainWindow::copyDirectoryTree);
    connect(ui->btnCopyContent, &QPushButton::clicked, this, &MainWindow::copyFileContent);
    connect(ui->btnCopyFull, &QPushButton::clicked, this, &MainWindow::copyFullContext);
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
        currentRootDir = dir;
        ui->stackedWidget->setCurrentIndex(1);
        ui->treeWidget->clear();
        ui->selectedListWidget->clear();
        ui->lblStatus->clear();
        ui->codeViewer->clear();
        ui->lblFileInfo->setText("Select a file to preview info");

        QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
        rootItem->setText(0, QDir(dir).dirName());
        rootItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        rootItem->setData(0, Qt::UserRole, dir);
        rootItem->setCheckState(0, Qt::Unchecked);

        populateTree(dir, rootItem);
        ui->treeWidget->expandItem(rootItem);
    }
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
    bool ok;
    QString t = QInputDialog::getMultiLineText(this, "Template", "Format ({name}, {code}):", contentTemplate, &ok);
    if (ok && !t.isEmpty()) {
        contentTemplate = t;
        QSettings settings("Nafuda", "Settings");
        settings.setValue("template", contentTemplate);
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
        "<p style='color: #666;'>Version 0.2.0</p>"
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
