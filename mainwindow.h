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

    void reloadCurrentFile(const QString &path);
    void toggleDarkMode(bool checked);

private:
    Ui::MainWindow *ui;
    QString currentRootDir;
    QString contentTemplate;
    const QString defaultTemplate = "File: {name}\n```\n{code}\n```\n";

    QMap<QString, QString> presets;
    QString currentPresetName;

    QLabel *statusPathLabel;

    QStringList recentFiles;
    const int maxRecentFiles = 10;

    QNetworkAccessManager *netManager;
    const QString currentVersion = "v0.5.0";

    QFileSystemWatcher *fileWatcher;
    QString currentFilePath;

    void populateTree(const QString &path, QTreeWidgetItem *parentItem);
    void setAllChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void updateFileList(QTreeWidgetItem *item);
    QString generateAsciiTree(const QString &path, const QString &prefix);

    void loadProject(const QString &path);
    void addToRecent(const QString &path);
    void updateRecentMenu();
    QString getRelativeTime(const QDateTime &dt);
};

#endif
