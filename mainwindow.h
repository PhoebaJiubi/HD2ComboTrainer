#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <Qt>
#include <QList>
#include <QLabel>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QElapsedTimer>
#include <QMap>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QTimer>
#include <QAbstractAnimation>
#include <functional>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "stratagemdata/stratagemdata.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void onStratagemSelected(QListWidgetItem *current, QListWidgetItem *previous);  // 点左侧战备，刷新右侧详情
    void onModeMenuClicked(int row);                                                 // 点左侧菜单，切换模式
    void on_TimeAttack_retryButton_clicked();                                        // 空壳
    void on_TimeAttack_backButton_clicked();                                         // 空壳
    void onSearchTextChanged(const QString &text);                                  //搜索函数
private:
    Ui::MainWindow *ui;

    // ---- 全局数据与初始化 ----
    QList<StratagemData> m_stratagems;      // 所有战备数据（从 JSON 加载）
    StratagemData m_currentStratagem;       // 当前正在练习的战备

    void initAll();                                                 // 启动初始化
    bool loadFromJson(const QString &filePath);                     // 读 JSON
    void populateList();                                            // 填左侧列表
    void updateDetails(const StratagemData &data);                  // 刷新练习页详情
    void buildArrows(QWidget *container, const QVector<Qt::Key> &seq); // 生成一排箭头

    // ---- 模式系统 ----
    enum class GameModeType {
        Practice,       // 练习
        TimeAttack,     // 计时随机
        Endless,        // 无限
        Dictation,      // 默写
        Illuminate      // 光能族干扰
    };

    struct ModeHandler {
        QString displayName;
        std::function<void()> onEnter;                              // 进模式
        std::function<void()> onExit;                               // 出模式
        std::function<void()> onActivate;                           // 按下 Ctrl
        std::function<void()> onDeactivate;                         // 松开 Ctrl
        std::function<void(const StratagemData&)> onLoadStratagem;  // 加载战备
        std::function<void()> onStratagemComplete;                  // 单个搓完
        std::function<void()> onEndInit;                            // 收尾
        std::function<void()> onProgress;                           // 输入正确一步
        std::function<void()> onInputError;                         // 输入错误
    };

    GameModeType m_currentMode = GameModeType::Practice;    // 当前模式
    QMap<GameModeType, ModeHandler> m_modeHandlers;         // 模式注册表

    void registerModeHandlers();                            // 注册所有模式钩子
    void switchToMode(GameModeType mode);                   // 切换模式

    // ---- 通用 UI 状态 ----
    QVector<QLabel*> m_arrowLabels;     // 当前屏幕上的箭头控件
    int m_currentStep;                  // 已输入步数
    bool m_ctrlHeld = false;            // Ctrl 是否按住
    QElapsedTimer m_timer;              // 练习模式计时器

    // ---- 核心状态机 ----
    typedef enum {
        SM_KeyboardInput_Idle,          // 空闲
        SM_KeyboardInput_Ready,         // Ctrl 已按，待输入
        SM_KeyboardInput_Inputting,     // 正在输入
        SM_KeyboardInput_End            // 收尾
    } SM_KeyboardInput_Step;

    SM_KeyboardInput_Step SM_KeyboardInput_NowStep = SM_KeyboardInput_Idle;
    QVector<Qt::Key> m_currentInput;    // 用户已输入的键

    void handleKeyEvent(QKeyEvent *event);                          // 核心状态机函数
    void transitionTo(SM_KeyboardInput_Step newState);              // 切状态
    void resetInput();                                              // 清输入、箭头变灰
    void updateArrowHighlight();                                    // 按进度高亮箭头
    void processDirectionKey(Qt::Key key);                          // 处理方向键

    // 未实现，待清理
    void updateArrowColors();
    void checkPrefixAndUpdateArrows();
    void showMatchResult(bool success, const QString &name = QString());

    // ---- TimeAttack 专用 ----
    QList<StratagemData> m_timeAttackQueue;     // 随机抽出的战备队列
    QList<double> m_timeAttackTimes;            // 每个战备的用时（秒）
    int m_timeAttackIndex = 0;                  // 当前第几个
    int m_timeAttackTotal = 0;                  // 总共几个
    int m_timeAttackErrors = 0;                 // 错误次数
    QElapsedTimer m_timeAttackStepTimer;        // 单步计时
    QElapsedTimer m_timeAttackTotalTimer;       // 总计时

    void startTimeAttack(int count);                                // 开始挑战
    void nextTimeAttackStratagem();                                 // 加载下一个
    void finishTimeAttack();                                        // 结算
    void showTimeAttackSetup();                                     // 切设置页
    void showTimeAttackPlaying();                                   // 切游戏页
    void showTimeAttackResult();                                    // 切结算页
    void refreshTimeAttackDetails(const StratagemData &data);       // 刷新游戏页详情

    // ---- 无限模式专用 ----
    QVector<Qt::Key> m_infiniteSequence;            // 500 个随机方向
    QWidget *m_infiniteArrowContainer = nullptr;    // 装箭头的超长容器
    int m_infiniteCombo = 0;                        // 当前连击数
    QElapsedTimer m_infiniteTimer;                  // 无限模式计时

    void generateInfiniteSequence(int length);                      // 生成随机序列
    void buildInfiniteArrows();                                     // 生成箭头条
    void scrollToInfiniteIndex(int index, bool animate);            // 滚动到指定位置
    void finishInfinite();                                          // 无限模式结算
    void clearAllArrows();                                          // 清空所有箭头

    // ---- 所有链接 ----
    void AllConnections();
    // ---- 默写模式 ----

    QTimer *m_dictationTimer = nullptr;     // 倒计时器
    int m_dictationRemaining = 100;         // 剩余进度（100 → 0）
    int m_dictationCorrectCount = 0;        // 累计答对数量

    void createTimer();                     // 创建倒计时器（只调一次）
    void startDictationTimer();             // 开始/重启倒计时
    void stopDictationTimer();              // 停止倒计时
    void resetDictationTimer();             // 重置进度到满格，继续跑
    void finishDictation();                 // 结算，切结算页
    void nextDictationStratagem();          // 抽下一题
    void buildQuestionMarks(QWidget *container, int count);              // 生成问好
    void revealArrowAt(int index);          // 将问好换成箭头


    // ---- 光能族干扰专用 ----
    QVector<Qt::Key> m_illuminateSequence;  // 当前显示的随机序列
    bool m_illuminateLocked = false;        // 序列是否已锁定
    int m_illuminateCorrectCount = 0;       // 累计答对数量
    QTimer *m_illuminateTimer = nullptr;    // 3 秒刷新定时器
    QElapsedTimer m_illuminateTimerElapsed; // 光能族：计时器（单题用时）
    QElapsedTimer m_illuminateElapsed;      // 单题用时

    void createIlluminateTimer();           // 创建刷新定时器
    void startIlluminateTimer();            // 启动刷新
    void stopIlluminateTimer();             // 停止刷新
    void generateIlluminateSequence();      // 生成随机序列并刷新箭头
    void refreshIlluminateArrows();         // 根据当前序列重建箭头
    void finishIlluminate(bool success);    // 结算，切结算页
};

#endif // MAINWINDOW_H
