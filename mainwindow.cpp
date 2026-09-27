#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "stratagemdata/stratagemdata.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    //===============================

    setWindowIcon(QIcon(":/app/asdvo-wbep7-001.ico"));
    setWindowTitle("绝地潜兵搓球训练器");
    //====================================
    QHBoxLayout *h = new QHBoxLayout(ui->TimeAttack_arrowsContainer);
       h->setAlignment(Qt::AlignCenter);
       h->setSpacing(10);

    initAll();

    // 让主窗口能够接收键盘事件
    setFocusPolicy(Qt::StrongFocus);

    // 禁止列表控件抢夺键盘焦点（方向键就不会被它吃掉）
    ui->listWidget_2->setFocusPolicy(Qt::NoFocus);
    // 如果你还有其他列表（比如 listWidget），也一并禁止
    ui->listWidget->setFocusPolicy(Qt::NoFocus);
    // 注册所有模式的钩子
    registerModeHandlers();

    // 连左侧菜单 → 模式切换
    connect(ui->listWidget, &QListWidget::currentRowChanged,
            this, &MainWindow::onModeMenuClicked);

    // 默认进入练习模式
    switchToMode(GameModeType::Practice);
    connect(ui->TimeAttack_startButton, &QPushButton::clicked, this, [this]() {
    int count = ui->TimeAttack_countSpinBox->value();
    if (count > 0) startTimeAttack(count);
    });
    AllConnections();
    createTimer();
    createIlluminateTimer();


    qDebug() << "资源测试：" << QFile::exists(":/Icon/Stratagem/cqc_20.svg");
 }
void MainWindow::createTimer()
{
    m_dictationTimer = new QTimer(this);
    m_dictationTimer->setInterval(50);   // 50ms × 100 = 5 秒
    connect(m_dictationTimer, &QTimer::timeout, this, [this]() {
        m_dictationRemaining--;
        ui->Dictation_timerBar->setValue(m_dictationRemaining);
        if (m_dictationRemaining <= 0) {
            stopDictationTimer();
            finishDictation();
        }
    });
}
void MainWindow::AllConnections()
{
    connect(ui->listWidget_2, &QListWidget::currentItemChanged,
            this, &MainWindow::onStratagemSelected);

    // ============ 结算页：再来一局 ============
    connect(ui->TimeAttack_retryButton, &QPushButton::clicked, this, [this]() {
        if (m_timeAttackTotal > 0) {
            startTimeAttack(m_timeAttackTotal);   // 用同样的数量重开
        }
    });
    // ============ 结算页：返回设置 ============
    connect(ui->TimeAttack_backButton, &QPushButton::clicked, this, [this]() {
        showTimeAttackSetup();
    });
    connect(ui->Infinite_resultBackButton, &QPushButton::clicked, this, [this]() {
        switchToMode(GameModeType::Endless);
    });
    connect(ui->Dictation_resultBackButton, &QPushButton::clicked, this, [this]() {
        switchToMode(GameModeType::Dictation);
    });
    connect(ui->Illuminate_resultBackButton, &QPushButton::clicked, this, [this]() {
        switchToMode(GameModeType::Illuminate);
    });

    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);
}
void MainWindow::onModeMenuClicked(int row)
{
    switch (row) {
        case 0: switchToMode(GameModeType::Practice);   break;
        case 1: switchToMode(GameModeType::TimeAttack); break;
        case 2: switchToMode(GameModeType::Endless);    break;
        case 3: switchToMode(GameModeType::Dictation);  break;
        case 4: switchToMode(GameModeType::Illuminate); break;
        default: break;
    }
}

void MainWindow::switchToMode(GameModeType mode)
{
    // 通知旧模式退出
    if (m_modeHandlers.contains(m_currentMode)) {
        auto &old = m_modeHandlers[m_currentMode];
        if (old.onExit) old.onExit();
    }
    clearAllArrows();               //切换前清除所有箭头

    m_currentMode = mode;
    transitionTo(SM_KeyboardInput_Idle);
    m_currentInput.clear();

    // 通知新模式进入
    if (m_modeHandlers.contains(mode)) {
        auto &h = m_modeHandlers[mode];
        if (h.onEnter) h.onEnter();
    }
     this->setFocus();
}
void MainWindow::initAll()
{
    // 加载 JSON
        QString jsonPath = ":/data/stratagems.json";
        if (loadFromJson(jsonPath)) {
            populateList();  // 填充第一页左侧列表
        } else {
            qDebug() << "⚠️ JSON 加载失败，请检查文件路径";
        }
        ui->listWidget->setCurrentRow(0);
        SM_KeyboardInput_NowStep = SM_KeyboardInput_Idle;

        ui->statusLabel->setText("按下 Ctrl 开始搓球");
        this->setFocus();  // 确保主窗口获得焦点
}

MainWindow::~MainWindow()
{
    delete ui;
}
bool MainWindow::loadFromJson(const QString &filePath)//加载函数
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开 JSON 文件：" << filePath;
        return false;
    }

    QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON 解析错误：" << parseError.errorString();
        return false;
    }

    if (!doc.isArray()) {
        qDebug() << "JSON 根节点不是数组！";
        return false;
    }

    m_stratagems.clear();
    QJsonArray array = doc.array();

    for (const QJsonValue &val : array) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        StratagemData data;
        data.id = obj["id"].toString();
        data.name = obj["name"].toString();

        // 解析 type 字段（字符串转枚举）
        QString typeStr = obj["type"].toString("utility");
        if (typeStr == "offensive") data.type = StratagemType::Offensive;
        else if (typeStr == "supply") data.type = StratagemType::Supply;
        else if (typeStr == "defensive") data.type = StratagemType::Defensive;
        else data.type = StratagemType::Utility;

        data.iconPath = obj["icon_small"].toString();
        data.largeIconPath = obj["icon_large"].toString();

        // 解析 code 数组：["W","A","S","D"] → QVector<Qt::Key>
        QJsonArray codeArr = obj["code"].toArray();
        for (const QJsonValue &codeVal : codeArr) {
            QString dir = codeVal.toString();
            if (dir == "W") data.codeSequence.append(Qt::Key_W);
            else if (dir == "A") data.codeSequence.append(Qt::Key_A);
            else if (dir == "S") data.codeSequence.append(Qt::Key_S);
            else if (dir == "D") data.codeSequence.append(Qt::Key_D);
        }

        m_stratagems.append(data);
    }

    qDebug() << "从 JSON 加载了" << m_stratagems.size() << "个战备";
    return true;
}
void MainWindow::populateList()//添加函数
{
    ui->listWidget_2->clear();

    // 定义类型显示顺序
    QVector<StratagemType> typeOrder = {
        StratagemType::Utility,
        StratagemType::Offensive,
        StratagemType::Supply,
        StratagemType::Defensive

    };

    // 类型中文名映射
    auto typeDisplayName = [](StratagemType type) -> QString {
        switch(type) {
            case StratagemType::Utility:   return "黄战备";
            case StratagemType::Offensive: return "红战备";
            case StratagemType::Supply:    return "蓝战备";
            case StratagemType::Defensive: return "绿战备";

            default: return "未知";
        }
    };

    for (int i = 0; i < typeOrder.size(); ++i) {
        StratagemType currentType = typeOrder[i];

        // ----- 1. 添加分组标题 -----
        QListWidgetItem *headerItem = new QListWidgetItem(ui->listWidget_2);  // 选定控件
        headerItem->setText(typeDisplayName(currentType));
        headerItem->setFlags(headerItem->flags() & ~Qt::ItemIsSelectable);
        headerItem->setTextAlignment(Qt::AlignCenter);
        QFont headerFont = headerItem->font();
        headerFont.setBold(true);
        headerFont.setPointSize(12);
        headerItem->setFont(headerFont);
        headerItem->setBackground(QColor(60, 60, 80));
        ui->listWidget_2->addItem(headerItem);

        // ----- 2. 添加该类型的所有战备项 -----
        for (const StratagemData &data : m_stratagems) {
            if (data.type == currentType) {
                QListWidgetItem *item = new QListWidgetItem(ui->listWidget_2);
                if (!data.iconPath.isEmpty() && QFile::exists(data.iconPath)) {
                    item->setIcon(QIcon(data.iconPath));
                }
                item->setText(data.name);
                item->setData(Qt::UserRole, data.id);
                ui->listWidget_2->addItem(item);
            }
        }

        // ----- 3. 添加分割线（除最后一组） -----
        if (i < typeOrder.size() - 1) {
            QFrame *line = new QFrame(ui->listWidget_2);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            line->setStyleSheet("background-color: #4a4a6a; height: 2px;");

            QListWidgetItem *separatorItem = new QListWidgetItem(ui->listWidget_2);
            separatorItem->setFlags(separatorItem->flags() & ~Qt::ItemIsSelectable);
            separatorItem->setSizeHint(QSize(0, 4));
            ui->listWidget_2->setItemWidget(separatorItem, line);
            ui->listWidget_2->addItem(separatorItem);
        }
    }

    // 默认选中第一个战备项（跳过标题行）
    for (int i = 0; i < ui->listWidget_2->count(); ++i) {
        QListWidgetItem *item = ui->listWidget_2->item(i);
        if (item->data(Qt::UserRole).toString().isEmpty()) continue;
        ui->listWidget_2->setCurrentItem(item);
        break;
    }
}
void MainWindow::updateDetails(const StratagemData &data)
{
    // ----- 1. 大图标 -----
    if (!data.largeIconPath.isEmpty() && QFile::exists(data.largeIconPath)) {
        QPixmap pix(data.largeIconPath);
        ui->iconLabel->setPixmap(pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->iconLabel->setText("🖼️");
        ui->iconLabel->setAlignment(Qt::AlignCenter);
        ui->iconLabel->setStyleSheet("font-size: 40px;");
    }

    // ----- 2. 战备名称 -----
    ui->nameLabel->setText(data.name);
    QFont nameFont = ui->nameLabel->font();
    nameFont.setPointSize(18);          // 字号调大，比如 28
    nameFont.setBold(true);             // 可选：加粗
    ui->nameLabel->setFont(nameFont);
    ui->nameLabel->setAlignment(Qt::AlignCenter);  // 居中（如果没居中）

    // ----- 方向序列（箭头） -----
        buildArrows(ui->arrowsContainer, data.codeSequence);
       }








void MainWindow::onStratagemSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current) return;
    QString id = current->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    for (const StratagemData &data : m_stratagems) {
        if (data.id == id) {
            m_currentStratagem = data;
            updateDetails(data);
            resetInput();
            transitionTo(SM_KeyboardInput_Idle);
            ui->statusLabel->setText("按下 Ctrl 开始搓球");
            break;
        }
    }
    ui->lastTimeLabel->setText("上次用时：--");
}
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    handleKeyEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    handleKeyEvent(event);
}
void MainWindow::handleKeyEvent(QKeyEvent *event)
{
    Qt::Key key = static_cast<Qt::Key>(event->key());
       bool isPress   = (event->type() == QEvent::KeyPress);
       bool isRelease = (event->type() == QEvent::KeyRelease);

       // 追踪 Ctrl 是否按下
       if (key == Qt::Key_Control) {
           if (isPress)   m_ctrlHeld = true;
           if (isRelease) m_ctrlHeld = false;
       }


    // ---------- 1. 先映射物理方向键到 WASD ----------
    Qt::Key dirKey = key;
    switch (key) {
    case Qt::Key_Up:    dirKey = Qt::Key_W; break;
    case Qt::Key_Down:  dirKey = Qt::Key_S; break;
    case Qt::Key_Left:  dirKey = Qt::Key_A; break;
    case Qt::Key_Right: dirKey = Qt::Key_D; break;
    default: break;
    }

    // ---------- 2. 状态机主循环 ----------
    switch (SM_KeyboardInput_NowStep) {

    // ==================== 空闲状态 ====================
    case SM_KeyboardInput_Idle:
        if (key == Qt::Key_Control && isPress) {
            transitionTo(SM_KeyboardInput_Ready);
            ui->statusLabel->setText("激活，请输入方向键 (W/A/S/D)");
            resetInput();
            m_timer.start();   // 开始计时
        }
        // 在空闲状态下按方向键，提示用户先按Ctrl
        else if (isPress && (dirKey == Qt::Key_W || dirKey == Qt::Key_A ||
                             dirKey == Qt::Key_S || dirKey == Qt::Key_D)) {
            ui->statusLabel->setText("请先按住 Ctrl");
            return; // 阻止事件继续传递
        }
        break;

    // ==================== 准备状态（Ctrl 已按下） ====================
    case SM_KeyboardInput_Ready:{
        qDebug()<<"SM_KeyboardInput_Ready";
        auto &h = m_modeHandlers[m_currentMode];
        if (key == Qt::Key_Control && isRelease) {
            transitionTo(SM_KeyboardInput_Idle);
            if (h.onDeactivate) h.onDeactivate();
            ui->statusLabel->setText("已停用，按下 Ctrl 重新激活");
        }
        else if (isPress && (dirKey == Qt::Key_W || dirKey == Qt::Key_A ||
                             dirKey == Qt::Key_S || dirKey == Qt::Key_D)) {
            // 第一个方向键 → 进入输入中
            transitionTo(SM_KeyboardInput_Inputting);
            processDirectionKey(dirKey);
        }
        else if (isPress && key != Qt::Key_Control) {
            // 按了其他键（非 Ctrl、非方向键）→ 提示
            ui->statusLabel->setText("请输入方向键 (W/A/S/D)");

        }
        break;
}
    // ==================== 输入中状态 ====================
    case SM_KeyboardInput_Inputting:{
        qDebug()<<"进入SM_KeyboardInput_Inputting";

        if (key == Qt::Key_Control && isRelease) {
            // Ctrl 释放 → 检查匹配并回到 Idle
            auto &h = m_modeHandlers[m_currentMode];
            if (!m_currentInput.isEmpty()) {
                bool matched = false;
                StratagemData matchedData;
                for (const StratagemData &data : m_stratagems) {
                    if (data.codeSequence == m_currentInput) {
                        matched = true;
                        matchedData = data;
                        break;
                    }
                    if (h.onDeactivate) h.onDeactivate();
                }

            }
            transitionTo(SM_KeyboardInput_Idle);
            ui->statusLabel->setText("已停用，按下 Ctrl 重新激活");
        }
        else if (isPress && (dirKey == Qt::Key_W || dirKey == Qt::Key_A ||
                             dirKey == Qt::Key_S || dirKey == Qt::Key_D)) {
            // 继续输入方向键
            processDirectionKey(dirKey);

        }
        else if (isPress && key != Qt::Key_Control) {
            // 按了其他键 → 忽略，但提示
            ui->statusLabel->setText("继续输入方向键...");
        }
        break;
}
    // ==================== 结束状态 ====================
    case SM_KeyboardInput_End: {
        qDebug()<<"进入SM_KeyboardInput_End";

        auto &h = m_modeHandlers[m_currentMode];

        // 统一收尾
        resetInput();
        if (h.onEndInit) h.onEndInit();

        // 判断 Ctrl 是否还按着
        if (m_ctrlHeld) {
            // 还按着 → 直接回到 Ready，等待下一个方向键
            transitionTo(SM_KeyboardInput_Ready);
            if (h.onActivate) h.onActivate();   // 重新触发激活逻辑（比如重启单步计时）
        } else {
            // 松开了 → 回到 Idle
            transitionTo(SM_KeyboardInput_Idle);
        }
        break;
    }
    default:
        break;
    }

    // ---------- 3. 其他按键交给父类（如 Tab、退格等） ----------
    // 注意：只有未被处理的关键按键才传递给父类
    if (key != Qt::Key_Control &&
        !(key >= Qt::Key_W && key <= Qt::Key_D) &&  // 粗略判断 WASD
        !(key >= Qt::Key_Up && key <= Qt::Key_Right)) {
        QMainWindow::keyPressEvent(event);
    }
}
// ---------- 状态机辅助函数实现 ----------
void MainWindow::transitionTo(SM_KeyboardInput_Step newState)
{
    SM_KeyboardInput_NowStep = newState;
}

void MainWindow::resetInput()
{
    m_currentInput.clear();


        // 默写模式：恢复成问号，不显示灰色箭头
        if (m_currentMode == GameModeType::Dictation) {
            for (QLabel *lbl : m_arrowLabels) {
                if (!lbl) continue;
                lbl->setPixmap(QPixmap());   // 清掉 pixmap
                lbl->setText("?");
                lbl->setStyleSheet(
                    "color: #5a7a9a;"
                    "font-size: 32px;"
                    "font-weight: bold;"
                    "background: transparent;"
                    "border: none;"
                );
            }
            return;
        }
        // 光能族干扰：不需要重置，直接返回
           if (m_currentMode == GameModeType::Illuminate) {
               return;
           }

        // 其他模式：恢复成灰色箭头
    // 重置所有箭头为灰色图片
    for (int i = 0; i < m_arrowLabels.size(); ++i) {
        QLabel *label = m_arrowLabels[i];
        if (i >= m_currentStratagem.codeSequence.size()) break;
        Qt::Key dir = m_currentStratagem.codeSequence[i];
        QString baseName;
        QString path;
        switch(dir) {
            case Qt::Key_W: path = ":/arrows/Arrows/Up_nosuc.png"; break;
            case Qt::Key_A: path = ":/arrows/Arrows/Left_nosuc.png"; break;
            case Qt::Key_S: path = ":/arrows/Arrows/Down_nosuc.png"; break;
            case Qt::Key_D: path = ":/arrows/Arrows/Right_nosuc.png"; break;
            default: continue;
        }

        QPixmap pix;
        if (pix.load(path)) {
            label->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MainWindow::updateArrowHighlight()
{

    const auto &target = m_currentStratagem.codeSequence;
    for (int i = 0; i < m_arrowLabels.size(); ++i) {
        if (i >= target.size()) break;
        QLabel *label = m_arrowLabels[i];
        Qt::Key dir = target[i];
        QString baseName;
        switch(dir) {
            case Qt::Key_W: baseName = "Up"; break;
            case Qt::Key_A: baseName = "Left"; break;
            case Qt::Key_S: baseName = "Down"; break;
            case Qt::Key_D: baseName = "Right"; break;
            default: continue;
        }
        QString suffix = (i < m_currentInput.size()) ? "suc" : "nosuc";
        QString path = QString(":/arrows/Arrows/%1_%2.png").arg(baseName, suffix);
        QPixmap pix;
        if (pix.load(path))
            label->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MainWindow::processDirectionKey(Qt::Key key)
{
    if (m_currentMode == GameModeType::Illuminate && !m_illuminateLocked) {
           m_illuminateLocked = true;
           stopIlluminateTimer();
           ui->Illuminate_hintLabel->setText("已锁定，请继续输入...");
       }
    // 打印当前目标序列
       QString targetStr;
       for (Qt::Key k : m_currentStratagem.codeSequence) {
           switch(k) {
               case Qt::Key_W: targetStr += "W "; break;
               case Qt::Key_A: targetStr += "A "; break;
               case Qt::Key_S: targetStr += "S "; break;
               case Qt::Key_D: targetStr += "D "; break;
           }
       }
       qDebug() << "目标序列:" << targetStr;

       m_currentInput.append(key);

       // 打印用户输入
       QString inputStr;
       for (Qt::Key k : m_currentInput) {
           switch(k) {
               case Qt::Key_W: inputStr += "W "; break;
               case Qt::Key_A: inputStr += "A "; break;
               case Qt::Key_S: inputStr += "S "; break;
               case Qt::Key_D: inputStr += "D "; break;
           }
       }
       qDebug() << "用户输入:" << inputStr;






    const auto &target = m_currentStratagem.codeSequence;

    // 前缀检查
    bool isPrefix = (target.size() >= m_currentInput.size());
    if (isPrefix) {
        for (int i = 0; i < m_currentInput.size(); ++i) {
            if (target[i] != m_currentInput[i]) { isPrefix = false; break; }
        }
    }

    auto &h = m_modeHandlers[m_currentMode];

    // 出错
    if (!isPrefix) {
        if (h.onInputError) h.onInputError();

        // 统一重置
        m_currentInput.clear();
        transitionTo(SM_KeyboardInput_Idle);

        return;
    }


    // 正确：更新箭头
    if (m_currentMode == GameModeType::Dictation) {
        // 默写模式：只揭晓刚输入的那一个，其他保持问号
        revealArrowAt(m_currentInput.size() - 1);
    } else {
        // 其他模式：全部按进度更新
        updateArrowHighlight();
    }
     if (h.onProgress) h.onProgress();
    // 全部完成 → 先让模式记录，再进 End 状态
    if (m_currentInput.size() == target.size()) {
        if (h.onStratagemComplete) h.onStratagemComplete();
        transitionTo(SM_KeyboardInput_End);   // 统一进入 End
    }
}
void MainWindow::registerModeHandlers()
{
    // ========== 练习模式 ==========
    m_modeHandlers[GameModeType::Practice] = {
        "练习模式",                                                     // ① displayName

        [this]() {
            ui->stackedWidget->setCurrentIndex(0);
            ui->statusLabel->setText("按下 Ctrl 开始搓球");

            // 从左侧列表恢复当前战备
            QListWidgetItem *cur = ui->listWidget_2->currentItem();
            if (cur) {
                QString id = cur->data(Qt::UserRole).toString();
                for (const StratagemData &data : m_stratagems) {
                    if (data.id == id) {
                        m_currentStratagem = data;
                        updateDetails(data);   // 内部调 buildArrows 重建箭头
                        break;
                    }
                }
            }
        },

        [this]() { },                                                  // ③ onExit

        [this]() {                                                     // ④ onActivate
            m_timer.start();
            ui->statusLabel->setText("激活，请输入方向键 (W/A/S/D)");
        },

        [this]() {                                                     // onDeactivate
            ui->TimeAttack_stateLabel->setText("已停用");
        },

        [this](const StratagemData &data) {                            // ⑥ onLoadStratagem
            updateDetails(data);
        },

        [this]() {                                                     // ⑦ onStratagemComplete
            double sec = m_timer.isValid() ? m_timer.elapsed() / 1000.0 : 0;
            ui->statusLabel->setText(
                QString("%1 释放成功！用时 %2 秒")
                    .arg(m_currentStratagem.name).arg(sec, 0, 'f', 2));

            ui->lastTimeLabel->setText(QString("上次用时：%1 秒").arg(sec, 0, 'f', 2));
        },

        [this]() { },                                                  // ⑧ onEndInit
        [this]() { },                                                  //  onProgress
        [this]() {                                                     // ⑨ onInputError
            ui->statusLabel->setText("输入错误，请重新输入");
            resetInput();
        }
    };


    // ========== 计时随机模式 ==========
    m_modeHandlers[GameModeType::TimeAttack] = {
        "计时随机模式",                                                 // ① displayName

        [this]() {                                                     // ② onEnter
            ui->stackedWidget->setCurrentIndex(1);
            showTimeAttackSetup();
        },

        [this]() { },                                                  // ③ onExit

        [this]() {                                                     // ④ onActivate
            m_timeAttackStepTimer.start();
            ui->TimeAttack_stateLabel->setText("激活，请输入方向键");
        },

        [this]() {                                                     // onDeactivate
            // 已经输了一些方向 → 直接结算
            if (m_currentInput.size() > 0) {
                finishInfinite();
            } else {
                // 还没输入任何东西就松开 Ctrl → 只显示已停用
                ui->Infinite_hintLabel->setText("已停用");
            }
        },

        [this](const StratagemData &data) {                            // ⑥ onLoadStratagem
            refreshTimeAttackDetails(data);
        },

        [this]() {                                                     // ⑦ onStratagemComplete
            double stepSec = m_timeAttackStepTimer.elapsed() / 1000.0;
            m_timeAttackTimes.append(stepSec);
            m_timeAttackIndex++;
        },

        [this]() {                                                     // ⑧ onEndInit
            if (m_timeAttackIndex >= m_timeAttackTotal) {
                finishTimeAttack();
            } else {
                nextTimeAttackStratagem();
            }
        },
        [this]() { },                                                  // onProgres
        [this]() {                                                     // ⑨ onInputError
            m_timeAttackErrors++;
            ui->TimeAttack_stateLabel->setText("❌ 输入错误");
            resetInput();
        }
    };
    // ========== 无限模式 ==========
    m_modeHandlers[GameModeType::Endless] = {
        "无限模式",

        [this]() {
            ui->stackedWidget->setCurrentIndex(2);      // 外层切到 page_3
            ui->Infinite_pages->setCurrentIndex(0);      // ★ 内层切到游戏页
            m_infiniteCombo = 0;
            m_currentInput.clear();
            ui->Infinite_comboLabel->setText("连击: 0");
            ui->Infinite_hintLabel->setText("按住 Ctrl 开始，按错即结束");
            generateInfiniteSequence(500);
            buildInfiniteArrows();
            m_currentStratagem.id = "endless";
            m_currentStratagem.name = "无限模式";
            m_currentStratagem.codeSequence = m_infiniteSequence;
            m_infiniteTimer.start();
        },

        [this]() {
        m_currentStratagem = StratagemData();
        },                                                  // onExit

        [this]() {                                                     // onActivate
            ui->Infinite_hintLabel->setText("输入中...");
        },

        [this]() {                                                     // onDeactivate
            if (m_currentInput.size() > 0) {
                finishInfinite();
            } else {
                ui->Infinite_hintLabel->setText("已停用");
            }
        },

        [this](const StratagemData &) { },                             // onLoadStratagem（不用）

        [this]() { },                                                  // onStratagemComplete（不会触发）

        [this]() { },                                                  // onEndInit

        [this]() {                                                     // onProgress
            m_infiniteCombo = m_currentInput.size();
            ui->Infinite_comboLabel->setText(
                QString("连击: %1").arg(m_infiniteCombo));
            scrollToInfiniteIndex(m_infiniteCombo, true);
        },

        [this]() {                                                     // onInputError
            finishInfinite();
        }
    };
    // ========== 默写模式 ==========
    m_modeHandlers[GameModeType::Dictation] = {
        "默写模式",                                                     // ① displayName

        [this]() {                                                     // ② onEnter
            ui->stackedWidget->setCurrentIndex(3);   // 切到 Dictation_page
            ui->Dictation_pages->setCurrentIndex(0); // 内层切到游戏页
            m_dictationCorrectCount = 0;
            nextDictationStratagem();                // 抽第一题
            startDictationTimer();                   // 开始倒计时
        },

        [this]() {                                                     // ③ onExit
            stopDictationTimer();
        },

        [this]() { },                                                  // ④ onActivate

        [this]() { },                                                  // ⑤ onDeactivate

        [this](const StratagemData &) { },                             // ⑥ onLoadStratagem

        [this]() {                                                     // ⑦ onStratagemComplete
            // 玩家搓完了 → 揭晓答案（箭头全变绿）
            updateArrowHighlight();
            m_dictationCorrectCount++;

            // 短暂停顿后进入下一题
            QTimer::singleShot(600, this, [this]() {
                nextDictationStratagem();
            });
        },

        [this]() { },                                                  // ⑧ onEndInit

        [this]() { },                                                  // ⑨ onProgress

        [this]() {                                                     // ⑩ onInputError
            finishDictation();                                         // 输错了直接结算
        }
    };

    // ========== 光能族干扰模式 ==========
    m_modeHandlers[GameModeType::Illuminate] = {
        "光能族干扰模式",

        [this]() {                                                     // ② onEnter
            ui->stackedWidget->setCurrentIndex(4);
            ui->Illuminate_page->setCurrentIndex(0);
            m_illuminateCorrectCount = 0;
            m_illuminateLocked = false;
            m_currentInput.clear();
            transitionTo(SM_KeyboardInput_Idle);

            ui->Illuminate_hintLabel->setText("序列变化中，按下正确箭头锁定");

            generateIlluminateSequence();
            startIlluminateTimer();
            m_illuminateElapsed.start();      // ★ 启动计时
        },

        [this]() {                                                     // ③ onExit
            stopIlluminateTimer();
            m_currentStratagem = StratagemData();
        },

        [this]() { },                                                  // ④ onActivate

        [this]() { },                                                  // ⑤ onDeactivate

        [this](const StratagemData &) { },                             // ⑥ onLoadStratagem

        [this]() {                                                     // ⑦ onStratagemComplete
            finishIlluminate(true);          // 搓对
        },
        [this]() { },                                                  // ⑧ onEndInit

        [this]() { },                                                  // ⑨ onProgress

        [this]() {                                                     // ⑩ onInputError
            finishIlluminate(false);
        }
    };
}
// ==================== 切页 ====================
void MainWindow::showTimeAttackSetup()
{
    ui->TimeAttack_pages->setCurrentWidget(ui->TimeAttack_page_SetUp);
}

void MainWindow::showTimeAttackPlaying()
{
    ui->TimeAttack_pages->setCurrentWidget(ui->TimeAttack_page_Play);
}

void MainWindow::showTimeAttackResult()
{
    ui->TimeAttack_pages->setCurrentWidget(ui->TimeAttack_page_Result);  // 或 Result
}

// ==================== 开始挑战 ====================
void MainWindow::startTimeAttack(int count)
{
    if (m_stratagems.isEmpty()) return;

    m_timeAttackQueue.clear();
    m_timeAttackTimes.clear();
    m_timeAttackIndex = 0;
    m_timeAttackTotal = count;
    m_timeAttackErrors = 0;

    // 随机抽 count 个战备
    for (int i = 0; i < count; ++i) {
        int r = QRandomGenerator::global()->bounded(m_stratagems.size());
        m_timeAttackQueue.append(m_stratagems[r]);
    }

    m_timeAttackTotalTimer.start();

    showTimeAttackPlaying();
    nextTimeAttackStratagem();
}

// ==================== 加载下一个 ====================
void MainWindow::nextTimeAttackStratagem()
{
    if (m_timeAttackIndex >= m_timeAttackTotal) {
        finishTimeAttack();
        return;
    }

    m_currentStratagem = m_timeAttackQueue[m_timeAttackIndex];
    m_currentInput.clear();

    // 通过钩子刷新 UI
    auto &h = m_modeHandlers[m_currentMode];
    if (h.onLoadStratagem) h.onLoadStratagem(m_currentStratagem);

    // 进度条
    ui->progressBar->setMaximum(m_timeAttackTotal);
    ui->progressBar->setValue(m_timeAttackIndex);

    // 单步计时重置
    m_timeAttackStepTimer.start();
}

// ==================== 结束 ====================
void MainWindow::finishTimeAttack()
{
    // ---------- 1. 总用时 ----------
    double totalSec = m_timeAttackTotalTimer.elapsed() / 1000.0;

    // ---------- 2. 平均用时 ----------
    double avgSec = 0;
    if (!m_timeAttackTimes.isEmpty()) {
        for (double t : m_timeAttackTimes) avgSec += t;
        avgSec /= m_timeAttackTimes.size();
    }

    // ---------- 3. 填三个卡片 ----------
    ui->TimeAttack_totalValueLabel->setText(QString("%1s").arg(totalSec, 0, 'f', 2));
    ui->TimeAttack_avgValueLabel->setText(QString("%1s").arg(avgSec, 0, 'f', 2));
    ui->TimeAttack_errorValueLabel->setText(QString::number(m_timeAttackErrors));

    // ---------- 4. 明细列表 ----------
    ui->TimeAttack_detailList->clear();

    for (int i = 0; i < m_timeAttackTimes.size(); ++i) {
        QWidget *row = new QWidget();
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(12, 6, 12, 6);
        h->setSpacing(8);

        QLabel *nameLbl = new QLabel(
            QString("%1.  %2")
                .arg(i + 1, 2, 10, QChar('0'))
                .arg(m_timeAttackQueue[i].name));
        nameLbl->setStyleSheet("color: #c0c0d0; font-family: 'Microsoft YaHei'; font-size: 15px;");

        QLabel *timeLbl = new QLabel(
            QString("%1s").arg(m_timeAttackTimes[i], 0, 'f', 2));
        timeLbl->setStyleSheet("color: #8fb0c8; font-family: 'Microsoft YaHei'; font-size: 15px; font-weight: bold;");
        timeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        h->addWidget(nameLbl);
        h->addStretch();
        h->addWidget(timeLbl);

        QListWidgetItem *item = new QListWidgetItem(ui->TimeAttack_detailList);
        item->setSizeHint(row->sizeHint());

        ui->TimeAttack_detailList->addItem(item);
        ui->TimeAttack_detailList->setItemWidget(item, row);
    }

    // ---------- 5. 切到结算页 ----------
    showTimeAttackResult();
}

// ==================== 刷新 Play 页 ====================
void MainWindow::refreshTimeAttackDetails(const StratagemData &data)
{
    // 图标
    if (!data.largeIconPath.isEmpty() && QFile::exists(data.largeIconPath)) {
        QPixmap pix(data.largeIconPath);
        ui->TimeAttack_iconLabel->setPixmap(pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 名称
    ui->TimeAttack_nameLabel->setText(data.name);

    // 箭头 —— 用通用函数
    buildArrows(ui->TimeAttack_arrowsContainer, data.codeSequence);

    // 状态
    ui->TimeAttack_stateLabel->setText("按住 Ctrl 开始");
}
void MainWindow::buildArrows(QWidget *container, const QVector<Qt::Key> &seq)
{
    // 1. 获取或创建布局
    QLayout *layout = container->layout();
    if (!layout) {
        QHBoxLayout *h = new QHBoxLayout(container);
        h->setAlignment(Qt::AlignCenter);
        h->setSpacing(10);
        layout = h;
    }

    // 2. 清空旧的箭头
    qDeleteAll(m_arrowLabels);
    m_arrowLabels.clear();

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();   // ★ 补这行
        delete child;
    }

    // 3. 生成新的箭头
    for (Qt::Key key : seq) {
        QLabel *arrowLabel = new QLabel(container);
        QString path;
        switch(key) {
            case Qt::Key_W: path = ":/arrows/Arrows/Up_nosuc.png"; break;
            case Qt::Key_A: path = ":/arrows/Arrows/Left_nosuc.png"; break;
            case Qt::Key_S: path = ":/arrows/Arrows/Down_nosuc.png"; break;
            case Qt::Key_D: path = ":/arrows/Arrows/Right_nosuc.png"; break;
            default: continue;
        }
        QPixmap pix;
        if (pix.load(path)) {
            arrowLabel->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            arrowLabel->setText("?");
        }
        arrowLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(arrowLabel);

        // 关键：存进 m_arrowLabels
        m_arrowLabels.append(arrowLabel);
    }
}

void MainWindow::on_TimeAttack_retryButton_clicked()
{

}


void MainWindow::on_TimeAttack_backButton_clicked()
{

}
// ==================== 生成随机长序列 ====================
void MainWindow::generateInfiniteSequence(int length)
{
    m_infiniteSequence.clear();
    for (int i = 0; i < length; ++i) {
        int r = QRandomGenerator::global()->bounded(4);
        switch (r) {
            case 0: m_infiniteSequence.append(Qt::Key_W); break;
            case 1: m_infiniteSequence.append(Qt::Key_A); break;
            case 2: m_infiniteSequence.append(Qt::Key_S); break;
            case 3: m_infiniteSequence.append(Qt::Key_D); break;
        }
    }
}

// ==================== 构建箭头容器 ====================
void MainWindow::buildInfiniteArrows()
{
    const int arrowSize = 40;
    const int spacing = 10;
    const int step = arrowSize + spacing;

    // 清空旧的
    qDeleteAll(m_arrowLabels);
    m_arrowLabels.clear();
    if (m_infiniteArrowContainer) {
        m_infiniteArrowContainer->deleteLater();
        m_infiniteArrowContainer = nullptr;
    }

    // 创建容器
    m_infiniteArrowContainer = new QWidget();
    m_infiniteArrowContainer->setStyleSheet("background: transparent;");
    m_infiniteArrowContainer->setFixedSize(
        m_infiniteSequence.size() * step,
        arrowSize + 10
    );

    // 逐个生成箭头
    for (int i = 0; i < m_infiniteSequence.size(); ++i) {
        Qt::Key key = m_infiniteSequence[i];
        QLabel *lbl = new QLabel(m_infiniteArrowContainer);
        lbl->setGeometry(i * step, 0, arrowSize, arrowSize);

        QString path;
        switch (key) {
            case Qt::Key_W: path = ":/arrows/Arrows/Up_nosuc.png"; break;
            case Qt::Key_A: path = ":/arrows/Arrows/Left_nosuc.png"; break;
            case Qt::Key_S: path = ":/arrows/Arrows/Down_nosuc.png"; break;
            case Qt::Key_D: path = ":/arrows/Arrows/Right_nosuc.png"; break;
            default: continue;
        }
        QPixmap pix;
        if (pix.load(path)) {
            lbl->setPixmap(pix.scaled(arrowSize, arrowSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        lbl->setAlignment(Qt::AlignCenter);
        m_arrowLabels.append(lbl);
    }

    // 塞进 QScrollArea
    ui->Infinite_scrollArea->setWidget(m_infiniteArrowContainer);

    // 滚动到开头
    QTimer::singleShot(0, this, [this]() {
        scrollToInfiniteIndex(0, false);
    });
}

// ==================== 滚动到指定索引 ====================
void MainWindow::scrollToInfiniteIndex(int index, bool animate)
{
    if (!ui->Infinite_scrollArea || !m_infiniteArrowContainer) return;
    if (index < 0 || index >= m_arrowLabels.size()) return;

    const int arrowSize = 40;
    const int spacing = 10;
    const int step = arrowSize + spacing;

    int viewportW = ui->Infinite_scrollArea->viewport()->width();
    int targetCenter = index * step + arrowSize / 2;
    int targetScroll = targetCenter - viewportW / 2;

    int maxScroll = m_infiniteArrowContainer->width() - viewportW;
    targetScroll = qBound(0, targetScroll, maxScroll);

    QScrollBar *bar = ui->Infinite_scrollArea->horizontalScrollBar();

    if (!animate) {
        bar->setValue(targetScroll);
        return;
    }

    //动画代码
    QPropertyAnimation *anim = new QPropertyAnimation(bar, "value");
    anim->setDuration(220);
    anim->setStartValue(bar->value());
    anim->setEndValue(targetScroll);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ==================== 结束无限模式 ====================
void MainWindow::finishInfinite()
{
    ui->Infinite_comboValueLabel->setText(QString::number(m_infiniteCombo));

    m_currentInput.clear();
    transitionTo(SM_KeyboardInput_Idle);

    // 内层切到结算页
    ui->Infinite_pages->setCurrentWidget(ui->Infinite_page_Result);
}
void MainWindow::clearAllArrows()
{
    // 清练习模式和 TimeAttack 的箭头（布局管理的）
    QWidget *containers[] = {
        ui->arrowsContainer,
        ui->TimeAttack_arrowsContainer,
        ui->Dictation_arrowsContainer,
        ui->Illuminate_arrowsContainer
    };    for (QWidget *c : containers) {
        if (c && c->layout()) {
            QLayoutItem *child;
            while ((child = c->layout()->takeAt(0)) != nullptr) {
                if (child->widget()) delete child->widget();
                delete child;
            }
        }
    }

    // 清无限模式的箭头容器
    if (m_infiniteArrowContainer) {
        m_infiniteArrowContainer->deleteLater();
        m_infiniteArrowContainer = nullptr;
    }

    m_arrowLabels.clear();
}


void MainWindow::startDictationTimer()
{
    m_dictationRemaining = 100;
    ui->Dictation_timerBar->setValue(100);
    m_dictationTimer->start();
}

void MainWindow::stopDictationTimer()
{
    m_dictationTimer->stop();
}

void MainWindow::resetDictationTimer()
{
    m_dictationRemaining = 100;
    ui->Dictation_timerBar->setValue(100);
    // timer 不停，继续跑
}

void MainWindow::finishDictation()
{
    stopDictationTimer();
    m_currentInput.clear();
    transitionTo(SM_KeyboardInput_Idle);
    ui->Dictation_correctValueLabel->setText(QString::number(m_dictationCorrectCount));
    ui->Dictation_pages->setCurrentIndex(1);   // 切结算页
}

void MainWindow::nextDictationStratagem()
{
    // 从所有战备里随机抽一个
    if (m_stratagems.isEmpty()) return;
    int r = QRandomGenerator::global()->bounded(m_stratagems.size());
    m_currentStratagem = m_stratagems[r];

    // 刷新图标和名称
    if (!m_currentStratagem.largeIconPath.isEmpty() &&
        QFile::exists(m_currentStratagem.largeIconPath)) {
        QPixmap pix(m_currentStratagem.largeIconPath);
        ui->Dictation_iconLabel->setPixmap(
            pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    ui->Dictation_nameLabel->setText(m_currentStratagem.name);
    // 生成灰色占位箭头
    buildQuestionMarks(ui->Dictation_arrowsContainer, m_currentStratagem.codeSequence.size());
    // 重置状态
    m_currentInput.clear();
    transitionTo(SM_KeyboardInput_Idle);
    resetDictationTimer();
}
void MainWindow::buildQuestionMarks(QWidget *container, int count)
{
    // 1. 获取或创建布局
    QLayout *layout = container->layout();
    if (!layout) {
        QHBoxLayout *h = new QHBoxLayout(container);
        h->setAlignment(Qt::AlignCenter);
        h->setSpacing(10);
        layout = h;
    }

    // 2. 清空旧的
    qDeleteAll(m_arrowLabels);
    m_arrowLabels.clear();
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    // 3. 生成 count 个问号
    for (int i = 0; i < count; ++i) {
        QLabel *lbl = new QLabel(container);
        lbl->setText("?");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedSize(40, 40);
        lbl->setStyleSheet(
            "color: #5a7a9a;"
            "font-size: 32px;"
            "font-weight: bold;"
            "background: transparent;"
            "border: none;"
        );
        layout->addWidget(lbl);
        m_arrowLabels.append(lbl);
    }
}
void MainWindow::revealArrowAt(int index)
{
    if (index < 0 || index >= m_arrowLabels.size()) return;
    if (index >= m_currentStratagem.codeSequence.size()) return;

    QLabel *lbl = m_arrowLabels[index];
    Qt::Key key = m_currentStratagem.codeSequence[index];

    QString baseName;
    switch (key) {
        case Qt::Key_W: baseName = "Up"; break;
        case Qt::Key_A: baseName = "Left"; break;
        case Qt::Key_S: baseName = "Down"; break;
        case Qt::Key_D: baseName = "Right"; break;
        default: return;
    }

    QString path = QString(":/arrows/Arrows/%1_suc.png").arg(baseName);
    QPixmap pix;
    if (pix.load(path)) {
        lbl->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        lbl->setText("");   // 清掉问号
    }
    lbl->setStyleSheet("background: transparent; border: none;");
}
void MainWindow::createIlluminateTimer()
{
    m_illuminateTimer = new QTimer(this);
    m_illuminateTimer->setInterval(1000);   // 3秒刷新一次
    connect(m_illuminateTimer, &QTimer::timeout, this, [this]() {
        if (!m_illuminateLocked) {
            generateIlluminateSequence();
        }
    });
}

void MainWindow::startIlluminateTimer()
{
    m_illuminateTimer->start();
}

void MainWindow::stopIlluminateTimer()
{
    m_illuminateTimer->stop();
}

void MainWindow::generateIlluminateSequence()
{

    int len = QRandomGenerator::global()->bounded(3, 8);
    m_illuminateSequence.clear();
    for (int i = 0; i < len; ++i) {
        int r = QRandomGenerator::global()->bounded(4);
        switch (r) {
            case 0: m_illuminateSequence.append(Qt::Key_W); break;
            case 1: m_illuminateSequence.append(Qt::Key_A); break;
            case 2: m_illuminateSequence.append(Qt::Key_S); break;
            case 3: m_illuminateSequence.append(Qt::Key_D); break;
        }
    }

    // 同步到 m_currentStratagem，让通用高亮函数能用
    m_currentStratagem.codeSequence = m_illuminateSequence;
    m_currentInput.clear();

    refreshIlluminateArrows();
}

void MainWindow::refreshIlluminateArrows()
{
    // 1. 先生成箭头（内部会清空 Illuminate 容器）
    buildArrows(ui->Illuminate_arrowsContainer, m_illuminateSequence);

    // 2. 再覆盖对齐（此时布局已存在，setAlignment 不会被覆盖）
    QLayout *layout = ui->Illuminate_arrowsContainer->layout();
    if (layout) {
        layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
}
void MainWindow::finishIlluminate(bool success)
{
    stopIlluminateTimer();
    m_illuminateLocked = false;
    m_currentInput.clear();
    transitionTo(SM_KeyboardInput_Idle);

    // 显示对错
    if(success)
    {
        ui->Illuminate_feedbackLabel->setText("正确");
        ui->Illuminate_feedbackLabel->setStyleSheet(
                "color: #66ff66;"
                "font-family: 'Microsoft YaHei';"
                "font-size: 40px;"
                "font-weight: bold;"
                "background: transparent;"
                "border: none;"
                "qproperty-alignment: AlignCenter;"
        );

    }
    else
    {
        ui->Illuminate_feedbackLabel->setText("错误");
        ui->Illuminate_feedbackLabel->setStyleSheet(
                "color: #ff4444;"
                "font-family: 'Microsoft YaHei';"
                "font-size: 40px;"
                "font-weight: bold;"
                "background: transparent;"
                "border: none;"
                "qproperty-alignment: AlignCenter;"
        );
    }
    ui->Illuminate_feedbackLabel->setProperty("result", success ? "success" : "fail");
    ui->Illuminate_feedbackLabel->style()->polish(ui->Illuminate_feedbackLabel);

    // 用时
    double sec = m_illuminateElapsed.isValid()
                 ? m_illuminateElapsed.elapsed() / 1000.0 : 0;
    ui->Illuminate_correctValueLabel->setText(QString("%1s").arg(sec, 0, 'f', 2));

    ui->Illuminate_page->setCurrentIndex(1);
}
void MainWindow::onSearchTextChanged(const QString &text)
{
    QString keyword = text.trimmed();

    // 第一遍：决定每个 item 是否显示
    for (int i = 0; i < ui->listWidget_2->count(); ++i) {
        QListWidgetItem *item = ui->listWidget_2->item(i);
        QString id = item->data(Qt::UserRole).toString();

        if (id.isEmpty()) {
            // 分组标题或分割线，先标记为待定，稍后处理
            item->setHidden(true);
            continue;
        }

        // 匹配规则：关键词为空 → 全部显示；否则名称或 id 包含关键词
        bool match = keyword.isEmpty()
                     || item->text().contains(keyword, Qt::CaseInsensitive)
                     || id.contains(keyword, Qt::CaseInsensitive);

        item->setHidden(!match);
    }

    // 第二遍：处理分组标题 —— 如果该分组下没有任何可见战备，隐藏标题和后面的分隔线
    bool groupHasVisible = false;
    for (int i = ui->listWidget_2->count() - 1; i >= 0; --i) {
        QListWidgetItem *item = ui->listWidget_2->item(i);
        QString id = item->data(Qt::UserRole).toString();

        if (id.isEmpty()) {
            // 标题或分割线：由下面的逻辑决定
            // 判断"下一个可见项"是否是战备
            bool nextIsVisibleStratagem = false;
            for (int j = i + 1; j < ui->listWidget_2->count(); ++j) {
                QListWidgetItem *next = ui->listWidget_2->item(j);
                if (next->data(Qt::UserRole).toString().isEmpty()) {
                    if (!next->isHidden()) { /* 分割线可见，继续 */ }
                    continue;
                }
                if (!next->isHidden()) { nextIsVisibleStratagem = true; }
                break; // 只看最近的一个战备项
            }
            item->setHidden(!nextIsVisibleStratagem);
        }
    }

    // 如果有关键词且过滤后没有任何结果，显示一条"无结果"提示（可选）
    // ...
}
