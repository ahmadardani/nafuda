#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QFileSystemModel>

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

private:
    Ui::MainWindow *ui;
    QString currentRootDir;
    QString contentTemplate;

    void populateTree(const QString &path, QTreeWidgetItem *parentItem);
    void setAllChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void updateFileList(QTreeWidgetItem *item);
    QString generateAsciiTree(const QString &path, const QString &prefix);
};

#endif
