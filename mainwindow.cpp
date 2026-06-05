#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <chrono>

using namespace std::chrono;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // UI界面初始化
    initialUI();

    highlighter = new syntaxhighlighter(codeInput->document());  // 高亮器
    // errorMarker = new ErrorMarker(codeInput);  // 词法错误提示器
    syntaxErrorMarker = new SyntaxErrorMarker(codeInput);  // 语法错误提示器

    // 创建分析线程
    analysisThread = new QThread(this);
    worker = new analyzer();
    worker->moveToThread(analysisThread);
    // analysisThread->start();
    // qDebug() << "线程是否运行？" << analysisThread->isRunning();

    // 创建分析定时器（用于延迟分析，避免频繁触发）
    analysisTimer = new QTimer(this);
    analysisTimer->setSingleShot(true);
    analysisTimer->setInterval(500); // 500毫秒延迟

    auto startLexer = high_resolution_clock::now();
    lexer = new LexicalAnalyzer(this);
    auto endLexer = high_resolution_clock::now();
    auto lexerDuration = duration_cast<milliseconds>(endLexer - startLexer).count();
    qDebug() << "Lexer 构造耗时: " << lexerDuration << " ms\n";

    auto startParser = high_resolution_clock::now();
    parse = new parser();
    auto endParser = high_resolution_clock::now();
    auto parserDuration = duration_cast<milliseconds>(endParser - startParser).count();
    qDebug() << "Parser 构造耗时: " << parserDuration << " ms\n";

    // 连接信号槽
    connect(codeInput, &QTextEdit::textChanged, this, &MainWindow::onTextChanged);  // 文本变化
    connect(analysisTimer, &QTimer::timeout, this, &MainWindow::scheduleAnalysis);  // 定时器结束，启动分析
    connect(worker, &analyzer::tokensReady, this, &MainWindow::updateTokenDisplay); // 词法分析结束， 更新结果展示
    connect(worker, &analyzer::tokensReady, highlighter, &syntaxhighlighter::updateTokens);  // 词法分析结束，更新高亮
    connect(worker, &analyzer::parseFinish, this, &MainWindow::updateParseResult);  // 语法分析结束，更新语法分析展示
    connect(worker, &analyzer::analysisFinished, analysisThread, &QThread::quit);   // 分析结束，线程结束
}

MainWindow::~MainWindow()
{
    analysisThread->quit();
    analysisThread->wait();
    delete worker;
    delete ui;
}

void MainWindow::onTextChanged() {
    // qDebug() << "检测到代码变化！";
    analysisTimer->start();
}

void MainWindow::scheduleAnalysis() {
    QString currentCode = codeInput->toPlainText();

    // 如果文本没有变化，直接跳过分析
    if(currentCode == lastAnalyzedCode) {
        // qDebug() << "文本未变化，跳过分析";
        return;
    }

    lastAnalyzedCode = currentCode;  // 记录当前文本

    if(currentCode.isEmpty()) return;  // 没有代码分析 直接返回

    // qDebug() << "定时器触发，准备调用 analyzeCode";
    if (analysisThread->isRunning()) {
        analysisThread->quit();
        analysisThread->wait();
    }
    analysisThread->start();  // 启动线程
    // qDebug() << "线程状态 - 运行中:" << analysisThread->isRunning()
    //          << "活跃:" << analysisThread->isFinished();
    QString code = codeInput->toPlainText();
    if (code.isEmpty()) {
        return;
    }

    // 启动分析
    bool success = QMetaObject::invokeMethod(worker, "analyzeCode", Qt::QueuedConnection, Q_ARG(QString, code));
    // qDebug() << "invokeMethod 是否成功？" << success;
}

void MainWindow::updateTokenDisplay(const QVector<Token> &tokens){
    displayToken(tokens);
}

void MainWindow::updateParseResult(const ParseResult &result){
    displayParseResult(result);
}


void MainWindow::initialUI(){
    setWindowTitle("编译器前端");

    // 创建主部件和主布局
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // 1. 创建代码输入框
    codeInput = new QTextEdit(this);
    codeInput->setPlaceholderText("请输入源代码...");
    mainLayout->addWidget(codeInput);

    // 2. 创建按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    // analyzeButton = new QPushButton("开始分析", this);
    clearButton = new QPushButton("清除", this);
    // buttonLayout->addWidget(analyzeButton);
    buttonLayout->addWidget(clearButton);
    mainLayout->addLayout(buttonLayout);

    // 3. 创建标签页控件（词法、语法、错误）
    tabWidget = new QTabWidget(this);
    lexerOutput = new QTextEdit(this);
    parserOutput = new QTextEdit(this);

    tabWidget->addTab(lexerOutput, "词法分析");
    tabWidget->addTab(parserOutput, "语法分析");

    // 设置只读
    lexerOutput->setReadOnly(true);
    parserOutput->setReadOnly(true);

    mainLayout->addWidget(tabWidget);

    // 设置 central widget
    setCentralWidget(central);

    // 信号与槽连接
    // connect(analyzeButton, &QPushButton::clicked, this, &MainWindow::onAnalyzeClicked);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
}


void MainWindow::onAnalyzeClicked()
{
    // 清空之前的输出
    lexerOutput->clear();
    parserOutput->clear();

    QString code = codeInput->toPlainText();
    if (code.isEmpty()) {
        return;
    }

    lexerOutput->setPlainText("正在进行词法分析...");
    auto start = high_resolution_clock::now();
    // 执行词法分析
    QVector<Token> tokens = lexer->analyze(code);
    auto end = high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_ms = end - start;
    qDebug() << "词法分析耗时: " << duration_ms.count() << " ms\n";

    displayToken(tokens);

    // 如果词法分析成功，开始语法分析
    if (!tokens.isEmpty()) {
        parserOutput->clear(); // 清空parser输出区域
        parserOutput->append("开始语法分析...\n");

        // 执行语法分析 - 输出会通过信号槽自动显示在parserOutput中

        auto start = high_resolution_clock::now();
        ParseResult parseResult = parse->parse(tokens);
        auto end = high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration_ms = end - start;
        qDebug() << "语法分析耗时: " << duration_ms.count() << " ms\n";

        displayParseResult(parseResult);

    } else {
        qDebug() << "词法分析失败：未识别到有效的token。";
    }

}

// Token -> 字符串
QString toString(TokenType type) {
    switch(type) {
    case TokenType::UNKNOWN: return "UNKNOWN";
    case TokenType::IDENTIFIER: return "IDENTIFIER";
    case TokenType::INTEGER: return "INTEGER";
    case TokenType::DECIMAL: return "DECIMAL";
    case TokenType::OPERATOR: return "OPERATOR";
    case TokenType::KEYWORD: return "KEYWORD";
    case TokenType::DELIMITER: return "DELIMITER";
    case TokenType::END_OF_INPUT: return "END_OF_INPUT";
    case TokenType::COMMENT: return "COMMENT";
    case TokenType::INT: return "INT";
    case TokenType::CHAR: return "CHAR";
    case TokenType::DOUBLE: return "DOUBLE";
    case TokenType::FLOAT: return "FLOAT";
    case TokenType::TRUE: return "TRUE";
    case TokenType::FALSE: return "FALSE";
    case TokenType::AND: return "AND";
    case TokenType::OR: return "OR";
    case TokenType::BOOL: return "BOOL";
    case TokenType::IF: return "IF";
    case TokenType::ELSE: return "ELSE";
    case TokenType::WHILE: return "WHILE";
    default: return "UNKNOWN";
    }
}

// 显示词法分析的结果
void MainWindow::displayToken(const QVector<Token>& tokens) {
    // 清空原有内容
    lexerOutput->clear();

    QString html;

    // 表格样式（与displayParseResult保持一致）
    html += "<style>";
    html += "table { border-collapse: collapse; width: 100%; font-family: 'Consolas', monospace; }";
    html += "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
    html += "th { background-color: #4CAF50; color: white; }";
    html += "tr:nth-child(even) { background-color: #f2f2f2; }";
    html += ".header { color: #2E8B57; font-weight: bold; font-size: 16px; margin-bottom: 10px; }";
    html += "</style>";

    html += "<div class='header'>词法分析结果</div>";

    // 创建表格
    html += "<table>";
    html += "<tr><th>类型</th><th>值</th><th>行号</th><th>列号</th></tr>";

    // 遍历所有token并添加到HTML表格中
    QVector<Token> errorTokens;
    for (const Token& token : tokens) {
        QString typeName = toString(token.type);
        QString rowColor = "";

        if (token.type == TokenType::UNKNOWN) {
            rowColor = "style='background-color: #ffebee;'";  // 错误token用浅红色背景
            errorTokens.append(token);
        }

        html += QString("<tr %1>"
                        "<td>%2</td>"
                        "<td>%3</td>"
                        "<td>%4</td>"
                        "<td>%5</td>"
                        "</tr>")
                    .arg(rowColor)
                    .arg(typeName)
                    .arg(token.value.toHtmlEscaped())
                    .arg(token.line)
                    .arg(token.column);
    }
    // errorMarker->markErrors(errorTokens);

    html += "</table>";

    // 添加统计信息
    html += "<br><div class='header'>统计信息</div>";
    html += QString("<p>共识别到 <b>%1</b> 个词法单元</p>").arg(tokens.size());

    // 检查是否有错误token
    int errorCount = std::count_if(tokens.begin(), tokens.end(),
                                   [](const Token& t) { return t.type == TokenType::UNKNOWN; });

    if (errorCount > 0) {
        html += QString("<p style='color: red;'>⚠️ 发现 <b>%1</b> 个词法错误</p>").arg(errorCount);
    } else {
        html += "<p style='color: green;'>✅ 词法分析成功，未发现错误</p>";
    }

    lexerOutput->setHtml(html);

    // 定位到底部
    QTextCursor cursor = lexerOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    lexerOutput->setTextCursor(cursor);
}


// 显示语法分析结果
void MainWindow::displayParseResult(const ParseResult& parseResult) {
    parserOutput->clear();

    QString html;

    // 表格样式
    html += "<style>";
    html += "table { border-collapse: collapse; width: 100%; font-family: 'Consolas', monospace; }";
    html += "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
    html += "th { background-color: #4CAF50; color: white; }";
    html += "tr:nth-child(even) { background-color: #f2f2f2; }";
    html += ".error-row { background-color: #ffebee !important; }";
    html += ".header { color: #2E8B57; font-weight: bold; font-size: 16px; margin-bottom: 10px; }";
    html += "</style>";

    html += "<div class='header'>语法分析步骤表</div>";

    html += "<table>";
    html += "<tr><th>步骤</th><th>状态栈</th><th>符号栈</th><th>剩余输入</th><th>动作</th></tr>";

    // 标记语法错误
    if (!parseResult.success) {
        if (parseResult.errorLine != -1) {
            // 默认标记错误位置前后3个字符
            int markLength = 3;
            QString errorContext = getErrorContext(parseResult.errorLine, parseResult.errorColumn);

            syntaxErrorMarker->markError(
                parseResult.errorLine,
                parseResult.errorColumn,
                markLength,
                parseResult.errorMessage + "\n上下文: " + errorContext
                );
        }
    } else {
        syntaxErrorMarker->clearMarks();
    }


    for (const ParseStep& step : parseResult.steps) {
        QString rowClass = step.isError ? "error-row" : "";
        html += QString("<tr class='%1'>").arg(rowClass);
        html += QString("<td>%1</td>").arg(step.stepNumber);
        html += QString("<td>%1</td>").arg(step.stateStack.toHtmlEscaped());
        html += QString("<td>%1</td>").arg(step.symbolStack.toHtmlEscaped());
        html += QString("<td>%1</td>").arg(step.remainingInput.toHtmlEscaped());
        html += QString("<td>%1</td>").arg(step.action.toHtmlEscaped());
        html += "</tr>";
    }

    html += "</table>";

    // 添加结果总结
    html += "<br><div class='header'>分析结果</div>";
    if (parseResult.success) {
        html += "<p style='color: green; font-weight: bold;'>✅ 语法分析成功！</p>";
    } else {
        html += "<p style='color: red; font-weight: bold;'>❌ 语法分析失败！</p>";
        if (!parseResult.errorMessage.isEmpty()) {
            html += QString("<p style='color: red;'>错误信息: %1</p>").arg(parseResult.errorMessage.toHtmlEscaped());
        }
        if (parseResult.errorLine != -1) {
            html += QString("<p style='color: red;'>错误位置: 行 %1, 列 %2</p>")
                        .arg(parseResult.errorLine)
                        .arg(parseResult.errorColumn);
        }
    }

    parserOutput->setHtml(html);

    // 定位到底部
    QTextCursor cursor = parserOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    parserOutput->setTextCursor(cursor);
}

// 辅助函数：获取错误位置的上下文
QString MainWindow::getErrorContext(int line, int column) {
    QTextCursor cursor(codeInput->document());
    cursor.movePosition(QTextCursor::Start);

    // 定位到错误行
    for (int i = 1; i < line; ++i) {
        cursor.movePosition(QTextCursor::Down);
    }

    // 获取当前行文本
    QString lineText = cursor.block().text();

    // 截取错误位置前后的文本（防止越界）
    int startPos = qMax(0, column - 10);
    int endPos = qMin(lineText.length(), column + 10);
    return lineText.mid(startPos, endPos - startPos);
}


void MainWindow::onClearClicked()
{
    codeInput->clear();
    lexerOutput->clear();
    parserOutput->clear();
    // errorMarker->clearMarks();
    syntaxErrorMarker->clearMarks();
    highlighter->clear();
}
