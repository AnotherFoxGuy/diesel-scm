#ifndef FILETABLEVIEW_H
#define FILETABLEVIEW_H

#include <QPoint>
#include <QTableView>

class FileTableView : public QTableView
{
    Q_OBJECT
public:
    explicit FileTableView(QWidget *parent = 0);

signals:
    void dragOutEvent();

private:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    QPoint dragStartPos;
};

#endif  // FILETABLEVIEW_H
