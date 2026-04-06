#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QListWidget>
#include <QFileSystemModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>
#include <QDateTime>
#include <QMap>
#include <QFileSystemWatcher>
#include <QIcon>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFolder();
    void openTemplateOptions();
    void openDataFilterOptions();
    void showAbout();

    void selectAllFiles();
    void deselectAllFiles();

    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void copyDirectoryTree();
    void copyFileContent();
    void copyFullContext();

    void checkUpdate();
    void onUpdateResult(QNetworkReply *reply);

    void openRecentProject();
    void onWelcomeListClicked(QListWidgetItem *item);
    void clearRecentList();

    void onProjectModified(const QString &path);
    void toggleDarkMode(bool checked);
    void refreshProject();

private:
    Ui::MainWindow *ui;
    QString currentRootDir;
    QString contentTemplate;
    const QString defaultTemplate = "File: {name}\n```\n{code}\n```\n";

    QMap<QString, QString> presets;
    QString currentPresetName;

    QLabel *statusPathLabel;
    QLabel *statusFilterLabel;

    QStringList recentFiles;
    const int maxRecentFiles = 10;

    QNetworkAccessManager *netManager;
    const QString currentVersion = "v0.6.0";

    QFileSystemWatcher *fileWatcher;
    QString currentFilePath;

    QIcon iconDir;
    QIcon iconFile;

    bool filterDataFiles;
    int maxDataLines;

    void populateTree(const QString &path, QTreeWidgetItem *parentItem, QStringList &watchDirs);
    void setAllChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void updateFileList(QTreeWidgetItem *item);
    QString generateAsciiTree(const QString &path, const QString &prefix);
    QString processFileContent(const QString &filePath);
    void updateFilterStatus();

    void loadProject(const QString &path);
    void addToRecent(const QString &path);
    void updateRecentMenu();
    QString getRelativeTime(const QDateTime &dt);
};

#endif
