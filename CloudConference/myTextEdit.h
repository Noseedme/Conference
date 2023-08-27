#ifndef MYTEXTEDIT_H
#define MYTEXTEDIT_H

#include <QObject>
#include <QWidget>
#include <QPlainTextEdit>
#include <QCompleter>//实现文字自动填充功能
#include <QStringList>
//pair将一对值（可以是不同的数据类型）组合成一个值，分别可以以其函数first和second访问
#include <QPair>
#include <QVector>

class Completer: public QCompleter//完成符
{
    Q_OBJECT
public:
    explicit Completer(QWidget *parent= nullptr);
};


class MyTextEdit : public QWidget
{
    Q_OBJECT
public:
    explicit MyTextEdit(QWidget *parent = nullptr);
    QString toPlainText();//转为纯文本
    void setPlainText(QString);
    void setPlaceholderText(QString);//占位符
    void setCompleter(QStringList );//设置完成符

private:
    QPlainTextEdit *edit;
    Completer *completer;
    QVector<QPair<int, int> > ipspan;

    QString textUnderCursor();//光标下的text
    bool eventFilter(QObject *, QEvent *);//事件过滤

signals:

private slots:
    void changeCompletion(QString);
public slots:
    void complete();
};

#endif // MYTEXTEDIT_H
