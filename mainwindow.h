#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThread>
#include <QTimer>
#include "lexer.h"
#include "parser.h"
#include "analyzer.h"
// #include "errormarker.h"
#include "syntaxerrormarker.h"
#include "syntaxhighlighter.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initialUI();  // 设置UI界面

    void displayToken(const QVector<Token>& tokens);  // 显示词法分析结果
    void displayParseResult(const ParseResult& parseResult);  // 显示语法分析结果

    QString getErrorContext(int line, int column);  // 辅助函数：获取错误位置上下文

private slots:
    void onAnalyzeClicked();  // 分析槽函数
    void onClearClicked();    // 清空槽函数

    void scheduleAnalysis();
    void onTextChanged();  // 文本改变槽函数
    void updateTokenDisplay(const QVector<Token> &tokens);  // 更新词法分析结果
    void updateParseResult(const ParseResult &result);  // 更新语法分析结果

private:
    Ui::MainWindow *ui;
    QTextEdit *codeInput;           // 源代码输入框
    QPushButton *analyzeButton;     // 启动分析按钮
    QPushButton *clearButton;       // 清空按钮
    QTextEdit *lexerOutput;         // 词法分析结果显示区
    QTextEdit *parserOutput;        // 语法分析结果显示区
    QTabWidget *tabWidget;          // 多结果展示容器(词法分析 + 语法分析)

    LexicalAnalyzer* lexer;         // 词法分析器
    parser* parse;                  // 语法分析器

    QThread *analysisThread;        // 分析线程
    analyzer *worker;               // 线程分析器
    QTimer *analysisTimer;          // 防抖动定时器
    // ErrorMarker *errorMarker;       // 词法错误标记器
    SyntaxErrorMarker *syntaxErrorMarker;  // 语法错误标记器

    syntaxhighlighter *highlighter;  // 高亮器

    QString lastAnalyzedCode;  // 用于判断文本是否变化，防止频繁刷新

};
#endif // MAINWINDOW_H
