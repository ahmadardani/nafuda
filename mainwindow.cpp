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
    sizes << 300 << 400 << 300;
    ui->splitter->setSizes(sizes);

connect(ui->btnWelcomeOpen, &QPushButton::clicked, this, &MainWindow::openFolder);

    contentTemplate = "File: {name}\n```\n{code}\n```\n";

    connect(ui->actionOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(ui->actionTemplateSettings, &QAction::triggered, this, &MainWindow::openTemplateOptions);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);

    connect(ui->treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &MainWindow::onTreeItemChanged);

    connect(ui->btnCopyTree, &QPushButton::clicked, this, &MainWindow::copyDirectoryTree);
    connect(ui->btnCopyContent, &QPushButton::clicked, this, &MainWindow::copyFileContent);
    connect(ui->btnCopyFull, &QPushButton::clicked, this, &MainWindow::copyFullContext);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Open Directory",
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        currentRootDir = dir;

        ui->stackedWidget->setCurrentIndex(1);

        ui->treeWidget->clear();
        ui->selectedListWidget->clear();
        ui->lblStatus->clear();

        QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
        rootItem->setText(0, QDir(dir).dirName());
        rootItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        rootItem->setData(0, Qt::UserRole, dir);

        populateTree(dir, rootItem);
        ui->treeWidget->expandItem(rootItem);
    }
}

void MainWindow::populateTree(const QString &path, QTreeWidgetItem *parentItem)
{
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst | QDir::Name);

    QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo &fileInfo : list) {
        if (fileInfo.fileName().startsWith(".")) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, fileInfo.fileName());
        item->setData(0, Qt::UserRole, fileInfo.filePath());

        if (fileInfo.isDir()) {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
            populateTree(fileInfo.filePath(), item);
        } else {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileIcon));
            item->setCheckState(0, Qt::Unchecked);
        }
    }
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QFileInfo info(path);

    if (info.isFile()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ui->codeViewer->setText(file.readAll());
        }
    }
}

void MainWindow::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (item->childCount() > 0) return;

    QString fullPath = item->data(0, Qt::UserRole).toString();
    QString relativePath = QDir(currentRootDir).relativeFilePath(fullPath);

    if (item->checkState(0) == Qt::Checked) {
        if (ui->selectedListWidget->findItems(relativePath, Qt::MatchExactly).isEmpty()) {
            ui->selectedListWidget->addItem(relativePath);
        }
    } else {
        QList<QListWidgetItem*> items = ui->selectedListWidget->findItems(relativePath, Qt::MatchExactly);
        for (auto *i : items) {
            delete ui->selectedListWidget->takeItem(ui->selectedListWidget->row(i));
        }
    }
}

QString MainWindow::generateAsciiTree(const QString &path, const QString &prefix)
{
    QString result;
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst | QDir::Name);

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo info = list.at(i);
        if (info.fileName().startsWith(".")) continue;

        bool isLast = (i == list.size() - 1);
        QString connector = isLast ? "└── " : "├── ";

        result += prefix + connector + info.fileName() + "\n";

        if (info.isDir()) {
            QString childPrefix = prefix + (isLast ? "    " : "│   ");
            result += generateAsciiTree(info.filePath(), childPrefix);
        }
    }
    return result;
}

void MainWindow::copyDirectoryTree()
{
    if (currentRootDir.isEmpty()) return;
    QString treeStr = "Project Structure:\n" + QDir(currentRootDir).dirName() + "\n";
    treeStr += generateAsciiTree(currentRootDir, "");

    QApplication::clipboard()->setText(treeStr);

    ui->lblStatus->setText("Directory Tree Copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::copyFileContent()
{
    if (ui->selectedListWidget->count() == 0) {
        ui->lblStatus->setText("⚠ No files selected!");
        QTimer::singleShot(3000, [this](){
            ui->lblStatus->clear();
        });
        return;
    }

    QString finalOutput = "";

    for(int i = 0; i < ui->selectedListWidget->count(); ++i) {
        QString relPath = ui->selectedListWidget->item(i)->text();
        QString fullPath = QDir(currentRootDir).filePath(relPath);

        QFile file(fullPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = file.readAll();
            QString entry = contentTemplate;
            entry.replace("{name}", relPath);
            entry.replace("{code}", content);
            finalOutput += entry + "\n";
        }
    }

    QApplication::clipboard()->setText(finalOutput);

    ui->lblStatus->setText("File Content Copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::copyFullContext()
{

    if (currentRootDir.isEmpty()) return;

if (ui->selectedListWidget->count() == 0) {
        ui->lblStatus->setText("⚠ No files selected for context!");

QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
        return;
    }

QString output = "Project Structure:\n" + QDir(currentRootDir).dirName() + "\n";
    output += generateAsciiTree(currentRootDir, "") + "\n\n";
    output += "File Contents:\n";

    for(int i = 0; i < ui->selectedListWidget->count(); ++i) {
        QString relPath = ui->selectedListWidget->item(i)->text();
        QString fullPath = QDir(currentRootDir).filePath(relPath);

        QFile file(fullPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = file.readAll();
            QString entry = contentTemplate;
            entry.replace("{name}", relPath);
            entry.replace("{code}", content);
            output += entry + "\n";
        }
    }

    QApplication::clipboard()->setText(output);

ui->lblStatus->setText("Full context copied!");
    QTimer::singleShot(3000, [this](){ ui->lblStatus->clear(); });
}

void MainWindow::openTemplateOptions()
{
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, "Template Settings",
                                                  "Set format using {name} and {code}:",
                                                  contentTemplate, &ok);
    if (ok && !text.isEmpty()) {
        contentTemplate = text;
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
        "<p style='color: #666;'>Version 0.1.0</p>"
        "<p>Designed to easily copy project structure and code context for LLMs.</p>"
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
