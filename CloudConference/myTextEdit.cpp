#include "myTextEdit.h"
#include <QVBoxLayout>//垂直布局
#include <QStringListModel>
#include <QDebug>
#include <QAbstractItemView>
#include <QScrollBar>

Completer::Completer(QWidget *parent): QCompleter{parent}{}


MyTextEdit::MyTextEdit(QWidget *parent) : QWidget{parent}{
    QVBoxLayout *layout = new QVBoxLayout(this);
    //设置外边距为0
    layout->setContentsMargins(0, 0, 0, 0);
    edit = new QPlainTextEdit();
    edit->setPlaceholderText(QString::fromUtf8("输入@可以向对应的IP发送数据"));
    layout->addWidget(edit);
    completer = nullptr;
    connect(edit,SIGNAL(textChanged()), this, SLOT(complete()));
    edit->installEventFilter(this);
}


QString MyTextEdit::toPlainText()//获取内容
{
    return edit->toPlainText();
}


void MyTextEdit::setPlainText(QString str)//设置内容
{
    edit->setPlainText(str);
}


void MyTextEdit::setPlaceholderText(QString str)
{
    edit->setPlaceholderText(str);
}



void MyTextEdit::setCompleter(QStringList stringlist)
{
    if(completer == nullptr)
    {
        completer = new Completer(this);
        completer->setWidget(this);
        completer->setCompletionMode(QCompleter::PopupCompletion);//显示匹配的自动完成项
        completer->setCaseSensitivity(Qt::CaseInsensitive);//不区分大小写
        connect(completer, SIGNAL(activated(QString)), this, SLOT(changeCompletion(QString)));
    }
    else
    {
        delete completer->model();
    }
    QStringListModel * model = new QStringListModel(stringlist, this);
    completer->setModel(model);
}


QString MyTextEdit::textUnderCursor()
{
    QTextCursor tc = edit->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}


bool MyTextEdit::eventFilter(QObject *obj, QEvent *event)
{
    //事件的发送者(obj)是否为edit
    if(obj == edit)
    {
        if(event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
            QTextCursor tc = edit->textCursor();
            int p = tc.position();//获取光标当前位置
            int i;
            for(i = 0; i < ipspan.size(); i++)
            {
                if( (keyevent->key() == Qt::Key_Backspace && p > ipspan[i].first && p <= ipspan[i].second ) || (keyevent->key() == Qt::Key_Delete && p >= ipspan[i].first && p < ipspan[i].second) )
                {
                    //光标的位置移动到当前IP范围的起始位置
                    tc.setPosition(ipspan[i].first, QTextCursor::MoveAnchor);
                    if(p == ipspan[i].second)
                    {
                        tc.setPosition(ipspan[i].second, QTextCursor::KeepAnchor);
                    }
                    else
                    {
                        tc.setPosition(ipspan[i].second + 1, QTextCursor::KeepAnchor);
                    }
                    //移除文本
                    tc.removeSelectedText();
                    ipspan.removeAt(i);
                    return true;//表示事件以处理
                }
                else if(p >= ipspan[i].first && p <= ipspan[i].second)
                {
                    QTextCursor tc = edit->textCursor();
                    tc.setPosition(ipspan[i].second);
                    edit->setTextCursor(tc);
                }
            }
        }
    }
    //果事件不是由edit对象发送的，或者事件不是按键事件，则调用父类的eventFilter()函数处理事件
    return QWidget::eventFilter(obj, event);
}


void MyTextEdit::changeCompletion(QString text)
{
    QTextCursor tc = edit->textCursor();
    //text的长度减去自动完成器completer的补全前缀长度
    int len = text.size() - completer->completionPrefix().size();
    tc.movePosition(QTextCursor::EndOfWord);//光标移动到当前单词的末尾
    tc.insertText(text.right(len));
    edit->setTextCursor(tc);
    completer->popup()->hide();//隐藏自动完成器completer的弹出窗口

    QString str = edit->toPlainText();
    int pos = str.size() - 1;
    //在str中从末尾开始向前找到字符@
    while(str.at(pos) != '@') pos--;

    tc.clearSelection();
    tc.setPosition(pos, QTextCursor::MoveAnchor);
    tc.setPosition(str.size(), QTextCursor::KeepAnchor);
        // tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, str.size() - pos);

    QTextCharFormat fmt = tc.charFormat();//赋予字符格式
    QTextCharFormat fmt_back = fmt;
    fmt.setForeground(QBrush(Qt::white));
    fmt.setBackground(QBrush(QColor(0, 160, 233)));
    tc.setCharFormat(fmt);
    tc.clearSelection();
    tc.setCharFormat(fmt_back);

    tc.insertText(" ");//插入空格
    edit->setTextCursor(tc);

    ipspan.push_back(QPair<int, int>(pos, str.size()+1));

}


void MyTextEdit::complete()
{
    if(edit->toPlainText().size() == 0 || completer == nullptr) return;
    QChar tail =  edit->toPlainText().at(edit->toPlainText().size()-1);
    if(tail == '@')
    {
        //将completer的补全前缀设置为字符@
        completer->setCompletionPrefix(tail);
        //completer的弹出窗口QAbstractItemView
        QAbstractItemView *view = completer->popup();
        //将view的当前索引设置为completer的补全模型的索引(0, 0)
        view->setCurrentIndex(completer->completionModel()->index(0, 0));
        QRect cr = edit->cursorRect();//获取矩形位置1
        QScrollBar *bar = completer->popup()->verticalScrollBar();//添加滚动条
        cr.setWidth(completer->popup()->sizeHintForColumn(0) + bar->sizeHint().width());
        completer->complete(cr);
    }
}
