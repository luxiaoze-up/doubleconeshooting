#include "integrated_control/main_controller.h"
#include "common/device_interface.h"

#ifdef HAS_TANGO
#include "common/tango_device_wrapper.h"
#endif

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QMessageBox>
#include <QGroupBox>
#include <QButtonGroup>
#include <QGridLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QFont>
#include <QFrame>
#include <QDateTime>
#include <QtGlobal>
#include <QStyle>
#include <iostream>

MainControllerGUI::MainControllerGUI(bool simulation_mode, QWidget *parent)
    : QMainWindow(parent), 
      status_text_(nullptr), 
      monitor_timer_(nullptr),
      center_stacked_widget_(nullptr),
      vacuum_pressure_display_(nullptr),
      vacuum_target_input_(nullptr),
      vacuum_pump_status_label_(nullptr),
      vacuum_valve_status_label_(nullptr),
      six_dof_step_size_(0.1),
      large_stroke_step_size_(1.0) {
    
    setWindowTitle(simulation_mode ? "Double Cone Shooting Control System (SIMULATION)" : "Double Cone Shooting Control System");
    resize(1400, 900); // 默认尺寸更大，减少初始滚动需求
    
    // Connect to devices (Real or Mock)
    connectDevices(simulation_mode);
    
    // Setup UI
    setupUI();
    
    // Setup monitoring timer
    monitor_timer_ = new QTimer(this);
    connect(monitor_timer_, &QTimer::timeout, this, &MainControllerGUI::updateStatus);
    monitor_timer_->start(1000); // Update every second
}

void MainControllerGUI::connectDevices(bool simulation_mode) {
    try {
        if (simulation_mode) {
            // Use Mock Devices
            auto large_stroke = std::make_unique<Common::MockDevice>("large_stroke");
            large_stroke->initialize();
            large_stroke_ = std::move(large_stroke);

            auto six_dof = std::make_unique<Common::MockMultiAxisDevice>("six_dof", 6);
            six_dof->initialize();
            six_dof_ = std::move(six_dof);
            
            auto aux = std::make_unique<Common::MockMultiAxisDevice>("auxiliary_support", 5);
            aux->initialize();
            auxiliary_support_ = std::move(aux);

            auto backlight = std::make_unique<Common::MockMultiAxisDevice>("backlight_system", 6);
            backlight->initialize();
            backlight_system_ = std::move(backlight);
            
            auto vac = std::make_unique<Common::MockVacuumDevice>("vacuum");
            vac->initialize();
            vacuum_ = std::move(vac);

            auto interlock = std::make_unique<Common::MockInterlockService>("interlock");
            interlock->initialize();
            interlock_ = std::move(interlock);

            logMessage("Simulation mode: connected to mock devices");
        } else {
#ifdef HAS_TANGO
            // Use Real Tango Devices via Wrappers
            large_stroke_ = std::make_unique<Common::TangoMotionDevice>("sys/large_stroke/1");
            large_stroke_->initialize();

            six_dof_ = std::make_unique<Common::TangoMultiAxisDevice>("sys/six_dof/1");
            six_dof_->initialize();
            
            auxiliary_support_ = std::make_unique<Common::TangoMultiAxisDevice>("sys/auxiliary/1");
            auxiliary_support_->initialize();

            backlight_system_ = std::make_unique<Common::TangoMultiAxisDevice>("sys/backlight/1");
            backlight_system_->initialize();

            vacuum_ = std::make_unique<Common::TangoVacuumDevice>("sys/vacuum/1");
            vacuum_->initialize();

            interlock_ = std::make_unique<Common::TangoInterlockService>("sys/interlock/1");
            interlock_->initialize();
            
            logMessage("Successfully connected to all Tango devices");
#else
            throw std::runtime_error("Tango support not compiled. Please use --sim flag.");
#endif
        }
    } catch (const std::exception &e) {
        QString message = QString("Device connection failed: %1").arg(e.what());
        showError(message);
        logMessage(message);
    }
}

void MainControllerGUI::setupUI() {
    // 应用工业科技风样式 - 深色主题 + 霓虹强调色
    setStyleSheet(R"(
        QMainWindow {
            background-color: #040a13;
            color: #d7dde8;
        }
        QWidget {
            background-color: transparent;
            color: #c3cede;
            font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
            font-size: 13px;
        }
        QGroupBox {
            background-color: #0b1524;
            border: 1px solid #1c3146;
            border-radius: 10px;
            margin-top: 20px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            margin-left: 12px;
            padding: 4px 14px;
            background-color: #08101c;
            border: 1px solid #1f6feb;
            border-radius: 6px;
            color: #29d6ff;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel {
            color: #8fa6c5;
            background: transparent;
        }
        QLabel[role="unit"] {
            color: #29d6ff;
            font-size: 12px;
            padding: 0 4px;
        }
        QLabel[statusRole="badge"] {
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 12px;
            background-color: #0f2238;
            border: 1px solid #1f6feb;
            color: #d7dde8;
        }
        QLabel[statusRole="badge"][statusLevel="warn"] {
            background-color: #342507;
            border-color: #d7a22a;
            color: #ffd38b;
        }
        QLabel[statusRole="badge"][statusLevel="error"] {
            background-color: #2f1114;
            border-color: #ff7b72;
            color: #ffb3ad;
        }
        QLabel[statusRole="dot"] {
            color: #38f0b4;
            font-weight: 600;
        }
        QLabel[statusRole="dot"][statusLevel="warn"] { color: #f2c95c; }
        QLabel[statusRole="dot"][statusLevel="error"] { color: #ff7b72; }
        QLabel[statusRole="inline"][statusLevel="ok"] { color: #39e072; }
        QLabel[statusRole="inline"][statusLevel="error"] { color: #ff7b72; }
        QLineEdit, QSpinBox, QDoubleSpinBox {
            background-color: #0c1724;
            border: 1px solid #1c3146;
            border-top: 2px solid #0b243c;
            border-radius: 6px;
            padding: 6px 10px;
            color: #f2f5ff;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #32e6c7;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1f6feb, stop:1 #32e6c7);
            border: 1px solid #23c9ff;
            border-bottom: 3px solid #0f4c92;
            border-radius: 8px;
            padding: 9px 22px;
            color: #ffffff;
            font-weight: 600;
            font-size: 13px;
            min-height: 36px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2a82ff, stop:1 #47ffd4);
            border-color: #47ffd4;
        }
        QPushButton:pressed {
            border-bottom: 1px solid #0f4c92;
            padding-top: 11px;
            padding-bottom: 7px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #143369, stop:1 #1f6feb);
        }
        QPushButton:disabled {
            background-color: #1b2536;
            border: 1px solid #2c3a52;
            color: #556176;
        }
        QTextEdit {
            background-color: #070e16;
            border: 1px solid #142338;
            border-radius: 6px;
            color: #61f2b0;
            font-family: "JetBrains Mono", "Consolas", monospace;
            font-size: 12px;
            padding: 10px;
        }
        QStackedWidget {
            background-color: #070e16;
            border: 1px solid #121c2b;
            border-radius: 10px;
        }
        QScrollBar:vertical {
            background-color: #0f1927;
            width: 12px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background-color: #1f6feb;
            border-radius: 6px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #32e6c7;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: none;
        }
    )");

    QWidget* central_widget = new QWidget;
    central_widget->setStyleSheet("background-color: #040a13;");
    setCentralWidget(central_widget);
    
    // 根布局：顶部状态栏 + 主区域 + 底部日志
    QVBoxLayout* root_layout = new QVBoxLayout(central_widget);
    root_layout->setSpacing(8);
    root_layout->setContentsMargins(8, 8, 8, 8);
    
    // === 顶部状态栏 ===
    root_layout->addWidget(createStatusBar());
    
    // === 主区域：侧边栏 + 中间内容 + 右侧监控 ===
    QWidget* main_container = new QWidget;
    main_container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    QHBoxLayout* main_layout = new QHBoxLayout(main_container);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(0, 0, 0, 0);

    // 1. Create Center Stacked Widget FIRST
    center_stacked_widget_ = new QStackedWidget;
    center_stacked_widget_->addWidget(createTargetPositioningPage());
    center_stacked_widget_->addWidget(createBacklightCharacterizationPage());
    center_stacked_widget_->addWidget(createAuxiliarySupportPage());
    center_stacked_widget_->addWidget(createVacuumControlPage());

    // 2. Left Navigation Sidebar
    main_layout->addWidget(createLeftSidebar());

    // 3. Center Content Area
    main_layout->addWidget(center_stacked_widget_, 1);

    // 4. Right Monitoring Panel
    main_layout->addWidget(createRightPanel());
    
    QScrollArea* main_scroll = new QScrollArea;
    main_scroll->setWidgetResizable(true);
    main_scroll->setFrameShape(QFrame::NoFrame);
    main_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    main_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    main_scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    main_scroll->setWidget(main_container);
    root_layout->addWidget(main_scroll, 1);
    
    // === 底部日志区 ===
    root_layout->addWidget(createLogPanel());
    root_layout->setStretch(0, 0); // 状态栏
    root_layout->setStretch(1, 5); // 主内容
    root_layout->setStretch(2, 1); // 日志
}

QWidget* MainControllerGUI::createStatusBar() {
    QWidget* bar = new QWidget;
    bar->setMinimumHeight(32);
    bar->setStyleSheet(R"(
        QWidget {
            background-color: #0e1724;
            border: 1px solid #1b2b3f;
            border-radius: 8px;
        }
        QLabel {
            color: #96a7c2;
            font-size: 12px;
            padding: 0 8px;
        }
        QLabel[statusRole="inline"] {
            padding: 3px 8px;
            border-radius: 10px;
            font-weight: 600;
            background-color: #0f2238;
            border: 1px solid #1f6feb;
        }
        QLabel[statusRole="inline"][statusLevel="ok"] {
            color: #37d086;
            border-color: #37d086;
        }
        QLabel[statusRole="inline"][statusLevel="warn"] {
            color: #f2c95c;
            border-color: #d7a22a;
            background-color: #342507;
        }
        QLabel[statusRole="inline"][statusLevel="error"] {
            color: #ff7b72;
            border-color: #ff7b72;
            background-color: #2f1114;
        }
    )");
    
    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(15, 0, 15, 0);
    layout->setSpacing(20);
    
    // 系统标题
    QLabel* title = new QLabel(QString::fromUtf8("双锥打靶控制系统"));
    title->setStyleSheet("color: #29d6ff; font-size: 18px; font-weight: 700; letter-spacing: 1px;");
    layout->addWidget(title);
    
    layout->addStretch();
    
    // 状态指示器
    auto addIndicator = [&](const QString& label, const QString& status, bool ok) {
        QLabel* l = new QLabel(label + ":");
        l->setStyleSheet("color: #8aa0c0;");
        layout->addWidget(l);
        
        QLabel* s = new QLabel(QString::fromUtf8(ok ? "● " : "○ ") + status);
        s->setProperty("statusRole", "inline");
        s->setProperty("statusLevel", ok ? "ok" : "error");
        layout->addWidget(s);
    };
    addIndicator(QString::fromUtf8("Tango"), QString::fromUtf8("已连接"), true);
    addIndicator(QString::fromUtf8("真空"), QString::fromUtf8("正常"), true);
    addIndicator(QString::fromUtf8("互锁"), QString::fromUtf8("正常"), true);
    
    // 时间显示
    QLabel* time_label = new QLabel();
    time_label->setStyleSheet("color: #7ea0c8; font-family: 'Consolas', monospace;");
    time_label->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    layout->addWidget(time_label);
    
    // 定时更新时间
    QTimer* timer = new QTimer(bar);
    connect(timer, &QTimer::timeout, [time_label]() {
        time_label->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    });
    timer->start(1000);
    
    return bar;
}

QWidget* MainControllerGUI::createLeftSidebar() {
    QWidget* sidebar = new QWidget;
    sidebar->setMinimumWidth(170);
    sidebar->setStyleSheet(R"(
        QWidget {
            background-color: #0b131f;
            border-right: 1px solid #1b2b3f;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 16, 12, 16);
    
    // 导航标题
    QLabel* nav_title = new QLabel(QString::fromUtf8("功能导航"));
    nav_title->setStyleSheet(R"(
        color: #7b8da8;
        font-size: 12px;
        font-weight: 600;
        padding: 4px 8px;
        text-transform: uppercase;
        letter-spacing: 1px;
    )");
    layout->addWidget(nav_title);

    QStringList buttons = {
        QString::fromUtf8("靶定位控制"), 
        QString::fromUtf8("背光表征"), 
        QString::fromUtf8("辅助支撑"),
        QString::fromUtf8("真空控制")
    };
    QButtonGroup* btn_group = new QButtonGroup(sidebar);
    
    for(int i=0; i<buttons.size(); ++i) {
        QPushButton* btn = new QPushButton(buttons[i]);
        btn->setCheckable(true);
        btn->setMinimumHeight(42);
        btn->setStyleSheet(R"(
            QPushButton { 
                background-color: transparent;
                border: none;
                border-radius: 6px;
                font-size: 13px;
                color: #d7dde8;
                text-align: left;
                padding: 8px 12px;
            }
            QPushButton:hover { 
                background-color: rgba(31, 239, 255, 0.12);
            }
            QPushButton:checked { 
                background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb, stop:1 #22d0c7);
                color: #ffffff;
                font-weight: 600;
            }
        )");
        
        layout->addWidget(btn);
        btn_group->addButton(btn, i);
        
        if(i == 0) btn->setChecked(true);
    }
    
    connect(btn_group, &QButtonGroup::idClicked, center_stacked_widget_, &QStackedWidget::setCurrentIndex);
    
    layout->addStretch();
    
    // 分隔线
    QFrame* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #1b2b3f; max-height: 1px;");
    layout->addWidget(line);
    
    // 版本信息
    QLabel* version = new QLabel(QString::fromUtf8("双锥打靶系统 v1.0"));
    version->setStyleSheet("color: #4b5c74; font-size: 10px; padding: 8px;");
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);
    
    return sidebar;
}

QWidget* MainControllerGUI::createTargetPositioningPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setSpacing(15);

    // --- 1. 靶定位六自由度位姿 (Display) ---
    QGroupBox* pose_group = new QGroupBox("靶定位六自由度位姿");
    QGridLayout* pose_layout = new QGridLayout;
    pose_layout->setHorizontalSpacing(4);
    pose_layout->setVerticalSpacing(4);
    pose_layout->setContentsMargins(6, 4, 6, 4);
    QStringList axes = {"X", "Y", "Z", "Xθ", "Yθ", "Zθ"};
    six_dof_position_displays_.resize(6);
    
    for(int i=0; i<6; ++i) {
        QString unit_text = (i < 3) ? QString::fromUtf8("mm") : QString::fromUtf8("°");
        pose_layout->addWidget(new QLabel(axes[i] + ":"), i/3, (i%3)*3);
        six_dof_position_displays_[i] = new QLineEdit("0");
        six_dof_position_displays_[i]->setReadOnly(true);
        six_dof_position_displays_[i]->setAlignment(Qt::AlignCenter);
        six_dof_position_displays_[i]->setMaximumWidth(90);
        pose_layout->addWidget(six_dof_position_displays_[i], i/3, (i%3)*3 + 1);
        QLabel* unit_label = new QLabel(unit_text);
        unit_label->setProperty("role", "unit");
        pose_layout->addWidget(unit_label, i/3, (i%3)*3 + 2);
    }
    pose_group->setLayout(pose_layout);
    layout->addWidget(pose_group);

    // --- 2. 靶定位六自由度位姿输入 (Input) ---
    QGroupBox* input_group = new QGroupBox("靶定位六自由度位姿输入");
    QGridLayout* input_layout = new QGridLayout;
    input_layout->setHorizontalSpacing(4);
    input_layout->setVerticalSpacing(4);
    input_layout->setContentsMargins(6, 4, 6, 4);
    
    six_dof_input_edits_.resize(6);
    for(int i=0; i<6; ++i) {
        QString unit_text = (i < 3) ? QString::fromUtf8("mm") : QString::fromUtf8("°");
        input_layout->addWidget(new QLabel(axes[i] + ":"), i/3, (i%3)*3);
        six_dof_input_edits_[i] = new QLineEdit("0");
        six_dof_input_edits_[i]->setAlignment(Qt::AlignCenter);
        six_dof_input_edits_[i]->setMaximumWidth(100);
        input_layout->addWidget(six_dof_input_edits_[i], i/3, (i%3)*3 + 1);
        QLabel* unit_label = new QLabel(unit_text);
        unit_label->setProperty("role", "unit");
        input_layout->addWidget(unit_label, i/3, (i%3)*3 + 2);
    }
    
    QHBoxLayout* input_btns = new QHBoxLayout;
    input_btns->setSpacing(20);
    input_btns->addStretch(1);
    
    QPushButton* btn_clear = new QPushButton(QString::fromUtf8("输入清零"));
    btn_clear->setMinimumWidth(110);
    connect(btn_clear, &QPushButton::clicked, this, &MainControllerGUI::onSixDofInputClear);
    input_btns->addWidget(btn_clear);
    
    QPushButton* btn_exec = new QPushButton(QString::fromUtf8("执行"));
    btn_exec->setMinimumWidth(110);
    connect(btn_exec, &QPushButton::clicked, this, &MainControllerGUI::onSixDofInputExecute);
    input_btns->addWidget(btn_exec);
    
    QPushButton* btn_zero = new QPushButton(QString::fromUtf8("回零"));
    btn_zero->setMinimumWidth(110);
    connect(btn_zero, &QPushButton::clicked, this, &MainControllerGUI::onSixDofInputZero);
    input_btns->addWidget(btn_zero);
    
    QPushButton* btn_rst = new QPushButton(QString::fromUtf8("复位"));
    btn_rst->setMinimumWidth(110);
    connect(btn_rst, &QPushButton::clicked, this, &MainControllerGUI::onSixDofReset);
    input_btns->addWidget(btn_rst);
    
    input_btns->addStretch(1);
    
    QVBoxLayout* input_group_layout = new QVBoxLayout;
    input_group_layout->addLayout(input_layout);
    input_group_layout->addLayout(input_btns);
    input_group->setLayout(input_group_layout);
    layout->addWidget(input_group);

    // --- 3. 大行程控制 (Large Stroke) ---
    QGroupBox* large_stroke_group = new QGroupBox(QString::fromUtf8("大行程控制"));
    QGridLayout* ls_layout = new QGridLayout;
    ls_layout->setHorizontalSpacing(6);
    ls_layout->setVerticalSpacing(8);
    ls_layout->setContentsMargins(8, 6, 8, 6);
    ls_layout->addWidget(new QLabel(QString::fromUtf8("当前位置:")), 0, 0);
    large_stroke_position_display_ = new QLineEdit("0");
    large_stroke_position_display_->setReadOnly(true);
    large_stroke_position_display_->setMaximumWidth(110);
    ls_layout->addWidget(large_stroke_position_display_, 0, 1);
    
    ls_layout->addWidget(new QLabel(QString::fromUtf8("运动状态:")), 0, 2);
    QLabel* ls_status = new QLabel(QString::fromUtf8("静止"));
    ls_status->setProperty("statusRole", "badge");
    ls_status->setProperty("statusLevel", "ok");
    ls_layout->addWidget(ls_status, 0, 3);
    
    ls_layout->addWidget(new QLabel(QString::fromUtf8("行程输入:")), 1, 0);
    QLineEdit* ls_input = new QLineEdit("0");
    ls_input->setMaximumWidth(120);
    ls_layout->addWidget(ls_input, 1, 1);
    
    QPushButton* btn_rel = new QPushButton(QString::fromUtf8("相对运动"));
    QPushButton* btn_abs = new QPushButton(QString::fromUtf8("绝对运动"));
    
    connect(btn_rel, &QPushButton::clicked, [this, ls_input](){
        moveLargeStroke(ls_input->text().toDouble());
    });
    connect(btn_abs, &QPushButton::clicked, [this, ls_input](){
        moveLargeStrokeAbsolute(ls_input->text().toDouble());
    });
    
    ls_layout->addWidget(btn_rel, 1, 2);
    ls_layout->addWidget(btn_abs, 1, 3);
    
    QPushButton* btn_stop = new QPushButton(QString::fromUtf8("停止"));
    QPushButton* btn_home = new QPushButton(QString::fromUtf8("回零"));
    QPushButton* btn_reset = new QPushButton(QString::fromUtf8("复位"));
    
    connect(btn_stop, &QPushButton::clicked, this, &MainControllerGUI::onLargeStrokeStop);
    connect(btn_home, &QPushButton::clicked, this, &MainControllerGUI::onLargeStrokeHome);
    connect(btn_reset, &QPushButton::clicked, this, &MainControllerGUI::onLargeStrokeReset);
    
    ls_layout->addWidget(btn_stop, 2, 1);
    ls_layout->addWidget(btn_home, 2, 2);
    ls_layout->addWidget(btn_reset, 2, 3);
    
    ls_layout->addWidget(new QLabel(QString::fromUtf8("闸板阀状态:")), 3, 0);
    QLabel* ls_valve = new QLabel(QString::fromUtf8("已关闭"));
    ls_valve->setProperty("statusRole", "badge");
    ls_valve->setProperty("statusLevel", "ok");
    ls_layout->addWidget(ls_valve, 3, 1);
    
    QPushButton* btn_valve_open = new QPushButton(QString::fromUtf8("闸板阀开"));
    QPushButton* btn_valve_close = new QPushButton(QString::fromUtf8("闸板阀关"));
    connect(btn_valve_open, &QPushButton::clicked, this, &MainControllerGUI::onValveOpen);
    connect(btn_valve_close, &QPushButton::clicked, this, &MainControllerGUI::onValveClose);
    ls_layout->addWidget(btn_valve_open, 3, 2);
    ls_layout->addWidget(btn_valve_close, 3, 3);
    
    large_stroke_group->setLayout(ls_layout);
    layout->addWidget(large_stroke_group);

    layout->addStretch();
    return page;
}

QWidget* MainControllerGUI::createAuxiliarySupportPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    auxiliary_position_displays_.resize(5);

    auto createSupportGroup = [this](QString title, int start_idx, int count) -> QGroupBox* {
        QGroupBox* group = new QGroupBox(title);
        QGridLayout* gl = new QGridLayout;
        gl->setHorizontalSpacing(6);
        gl->setVerticalSpacing(6);
        gl->setContentsMargins(8, 6, 8, 6);
        
        for(int i=0; i<count; ++i) {
            int axis_idx = start_idx + i;
            gl->addWidget(new QLabel(QString::fromUtf8("电机%1 当前位置:").arg(i+1)), i, 0);
            
            auxiliary_position_displays_[axis_idx] = new QLineEdit("0");
            auxiliary_position_displays_[axis_idx]->setReadOnly(true);
            auxiliary_position_displays_[axis_idx]->setMaximumWidth(90);
            gl->addWidget(auxiliary_position_displays_[axis_idx], i, 1);
            
            gl->addWidget(new QLabel(QString::fromUtf8("运动状态:")), i, 2);
            QLabel* status = new QLabel(QString::fromUtf8("静止"));
            status->setProperty("statusRole", "badge");
            status->setProperty("statusLevel", "ok");
            gl->addWidget(status, i, 3);
            
            gl->addWidget(new QLabel(QString::fromUtf8("行程输入:")), i, 4);
            QLineEdit* input = new QLineEdit("0");
            input->setMaximumWidth(100);
            gl->addWidget(input, i, 5);
            gl->addWidget(new QLabel("mm"), i, 6);
            
            QPushButton* btn_exec = new QPushButton(QString::fromUtf8("执行"));
            connect(btn_exec, &QPushButton::clicked, [this, axis_idx, input](){
                moveAuxiliaryAxis(axis_idx, input->text().toDouble());
            });
            gl->addWidget(btn_exec, i, 7);
            
            QPushButton* btn_stop = new QPushButton(QString::fromUtf8("停止"));
            connect(btn_stop, &QPushButton::clicked, [this, axis_idx](){
                onAuxiliaryStop(axis_idx);
            });
            gl->addWidget(btn_stop, i, 8);
            
            QPushButton* btn_reset = new QPushButton(QString::fromUtf8("复位"));
            connect(btn_reset, &QPushButton::clicked, [this, axis_idx](){
                onAuxiliaryReset(axis_idx);
            });
            gl->addWidget(btn_reset, i, 9);
        }
        
        group->setLayout(gl);
        return group;
    };

    // 1. Infrared Homogenization (2 motors: indices 0, 1)
    layout->addWidget(createSupportGroup(QString::fromUtf8("红外均化 (2个电机)"), 0, 2));
    
    // 2. Reflection Imaging (2 motors: indices 2, 3)
    layout->addWidget(createSupportGroup(QString::fromUtf8("反射成像 (2个电机)"), 2, 2));
    
    // 3. Target Support (1 motor: index 4)
    layout->addWidget(createSupportGroup(QString::fromUtf8("靶支撑 (1个电机)"), 4, 1));
    
    layout->addStretch();
    return page;
}

QWidget* MainControllerGUI::createBacklightCharacterizationPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    backlight_position_displays_.resize(6);

    QGroupBox* group = new QGroupBox(QString::fromUtf8("背光表征控制 (6个电机)"));
    QGridLayout* gl = new QGridLayout;
    gl->setHorizontalSpacing(6);
    gl->setVerticalSpacing(6);
    gl->setContentsMargins(8, 6, 8, 6);
    
    for(int i=0; i<6; ++i) {
        QString label = (i < 4) ? QString::fromUtf8("42型电机%1").arg(i+1) : QString::fromUtf8("57型电机%1").arg(i-3);
        gl->addWidget(new QLabel(label + QString::fromUtf8(" 当前位置:")), i, 0);
        
        backlight_position_displays_[i] = new QLineEdit("0");
        backlight_position_displays_[i]->setReadOnly(true);
        backlight_position_displays_[i]->setMaximumWidth(90);
        gl->addWidget(backlight_position_displays_[i], i, 1);
        
        gl->addWidget(new QLabel(QString::fromUtf8("运动状态:")), i, 2);
        QLabel* status = new QLabel(QString::fromUtf8("静止"));
        status->setProperty("statusRole", "badge");
        status->setProperty("statusLevel", "ok");
        gl->addWidget(status, i, 3);
        
        gl->addWidget(new QLabel(QString::fromUtf8("行程输入:")), i, 4);
        QLineEdit* input = new QLineEdit("0");
        input->setMaximumWidth(100);
        gl->addWidget(input, i, 5);
        gl->addWidget(new QLabel("mm"), i, 6);
        
        QPushButton* btn_exec = new QPushButton(QString::fromUtf8("执行"));
        connect(btn_exec, &QPushButton::clicked, [this, i, input](){
            moveBacklightAxis(i, input->text().toDouble());
        });
        gl->addWidget(btn_exec, i, 7);
        
        QPushButton* btn_stop = new QPushButton(QString::fromUtf8("停止"));
        connect(btn_stop, &QPushButton::clicked, [this, i](){
            onBacklightStop(i);
        });
        gl->addWidget(btn_stop, i, 8);
        
        QPushButton* btn_reset = new QPushButton(QString::fromUtf8("复位"));
        connect(btn_reset, &QPushButton::clicked, [this, i](){
            onBacklightReset(i);
        });
        gl->addWidget(btn_reset, i, 9);
    }
    
    group->setLayout(gl);
    layout->addWidget(group);
    
    layout->addStretch();
    return page;
}

QWidget* MainControllerGUI::createVacuumControlPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    
    // --- 状态区 ---
    QGroupBox* status_group = new QGroupBox(QString::fromUtf8("真空状态"));
    QGridLayout* st = new QGridLayout;
    st->setHorizontalSpacing(8);
    st->setVerticalSpacing(8);
    st->setContentsMargins(10, 10, 10, 10);
    
    st->addWidget(new QLabel(QString::fromUtf8("当前压力:")), 0, 0);
    vacuum_pressure_display_ = new QLineEdit("0.00");
    vacuum_pressure_display_->setReadOnly(true);
    vacuum_pressure_display_->setAlignment(Qt::AlignRight);
    vacuum_pressure_display_->setMaximumWidth(140);
    st->addWidget(vacuum_pressure_display_, 0, 1);
    QLabel* unit_pa = new QLabel("Pa");
    unit_pa->setProperty("role", "unit");
    st->addWidget(unit_pa, 0, 2);
    
    st->addWidget(new QLabel(QString::fromUtf8("目标压力:")), 1, 0);
    vacuum_target_input_ = new QLineEdit("0.00");
    vacuum_target_input_->setAlignment(Qt::AlignRight);
    vacuum_target_input_->setMaximumWidth(140);
    st->addWidget(vacuum_target_input_, 1, 1);
    QLabel* unit_pa2 = new QLabel("Pa");
    unit_pa2->setProperty("role", "unit");
    st->addWidget(unit_pa2, 1, 2);
    
    st->addWidget(new QLabel(QString::fromUtf8("泵状态:")), 2, 0);
    vacuum_pump_status_label_ = new QLabel(QString::fromUtf8("停止"));
    vacuum_pump_status_label_->setProperty("statusRole", "badge");
    vacuum_pump_status_label_->setProperty("statusLevel", "warn");
    st->addWidget(vacuum_pump_status_label_, 2, 1, 1, 2);
    
    st->addWidget(new QLabel(QString::fromUtf8("闸板阀门:")), 3, 0);
    vacuum_valve_status_label_ = new QLabel(QString::fromUtf8("已关闭"));
    vacuum_valve_status_label_->setProperty("statusRole", "badge");
    vacuum_valve_status_label_->setProperty("statusLevel", "warn");
    st->addWidget(vacuum_valve_status_label_, 3, 1, 1, 2);
    
    status_group->setLayout(st);
    layout->addWidget(status_group);
    
    // --- 控制区 ---
    QGroupBox* control_group = new QGroupBox(QString::fromUtf8("控制操作"));
    QGridLayout* ctl = new QGridLayout;
    ctl->setSpacing(10);
    ctl->setContentsMargins(10, 10, 10, 10);
    
    QPushButton* btn_start = new QPushButton(QString::fromUtf8("开始抽气"));
    QPushButton* btn_stop = new QPushButton(QString::fromUtf8("停止抽气"));
    QPushButton* btn_vent = new QPushButton(QString::fromUtf8("放气"));
    QPushButton* btn_set_target = new QPushButton(QString::fromUtf8("设置目标压力"));
    QPushButton* btn_valve_open = new QPushButton(QString::fromUtf8("闸板阀开"));
    QPushButton* btn_valve_close = new QPushButton(QString::fromUtf8("闸板阀关"));
    
    connect(btn_start, &QPushButton::clicked, this, &MainControllerGUI::onVacuumStartPumping);
    connect(btn_stop, &QPushButton::clicked, this, &MainControllerGUI::onVacuumStopPumping);
    connect(btn_vent, &QPushButton::clicked, this, &MainControllerGUI::onVacuumVent);
    connect(btn_valve_open, &QPushButton::clicked, this, &MainControllerGUI::onValveOpen);
    connect(btn_valve_close, &QPushButton::clicked, this, &MainControllerGUI::onValveClose);
    connect(btn_set_target, &QPushButton::clicked, [this]() {
        try {
            if (vacuum_) {
                double target = vacuum_target_input_ ? vacuum_target_input_->text().toDouble() : 0.0;
                vacuum_->setTargetPressure(target);
                logMessage(QString("[OK] 目标压力已设定为 %1 Pa").arg(target, 0, 'f', 2));
            }
        } catch (const std::exception &e) {
            logMessage(QString("设置目标压力失败: %1").arg(e.what()));
        }
    });
    
    ctl->addWidget(btn_start, 0, 0);
    ctl->addWidget(btn_stop, 0, 1);
    ctl->addWidget(btn_vent, 0, 2);
    ctl->addWidget(btn_set_target, 1, 0, 1, 2);
    ctl->addWidget(btn_valve_open, 2, 0);
    ctl->addWidget(btn_valve_close, 2, 1);
    ctl->setColumnStretch(3, 1);
    
    control_group->setLayout(ctl);
    layout->addWidget(control_group);
    
    layout->addStretch();
    return page;
}

QWidget* MainControllerGUI::createRightPanel() {
    QWidget* panel = new QWidget;
    panel->setMinimumWidth(260);
    panel->setStyleSheet("background-color: #0e1724; border-left: 1px solid #1b2b3f;");
    
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setSpacing(10);
    layout->setContentsMargins(12, 12, 12, 12);
    
    // --- 实时监控标题 ---
    QLabel* monitor_title = new QLabel(QString::fromUtf8("实时监控"));
    monitor_title->setStyleSheet(R"(
        color: #29d6ff;
        font-size: 15px;
        font-weight: 700;
        padding: 8px 0;
        border-bottom: 1px solid #1b2b3f;
    )");
    layout->addWidget(monitor_title);
    
    // --- 电机状态监控 ---
    QGroupBox* status_group = new QGroupBox(QString::fromUtf8("轴位置"));
    QGridLayout* grid = new QGridLayout;
    grid->setSpacing(6);
    grid->setContentsMargins(10, 15, 10, 10);
    
    // 电机行
    QStringList motors = {
        QString::fromUtf8("X  轴"),
        QString::fromUtf8("Y  轴"),
        QString::fromUtf8("Z  轴"),
        QString::fromUtf8("Rx 轴"),
        QString::fromUtf8("Ry 轴"),
        QString::fromUtf8("Rz 轴"),
        QString::fromUtf8("大行程")
    };
    
    for(int i=0; i<motors.size(); ++i) {
        QLabel* name = new QLabel(motors[i]);
        name->setStyleSheet("font-size: 12px; color: #a3b8da; font-family: 'Consolas', monospace;");
        grid->addWidget(name, i, 0);
        
        QLineEdit* pos = new QLineEdit("0.000");
        pos->setReadOnly(true);
        pos->setMinimumWidth(100);
        pos->setAlignment(Qt::AlignRight);
        grid->addWidget(pos, i, 1);
        
        QLabel* unit = new QLabel(i < 3 ? "mm" : (i < 6 ? "°" : "mm"));
        unit->setStyleSheet("color: #60708a; font-size: 11px;");
        grid->addWidget(unit, i, 2);
        
        QLabel* status = new QLabel(QString::fromUtf8("●"));
        status->setProperty("statusRole", "dot");
        status->setProperty("statusLevel", "ok");
        status->setAlignment(Qt::AlignCenter);
        grid->addWidget(status, i, 3);
    }
    
    status_group->setLayout(grid);
    layout->addWidget(status_group);
    
    // --- 系统状态 ---
    QGroupBox* sys_group = new QGroupBox(QString::fromUtf8("系统信息"));
    QVBoxLayout* sys_layout = new QVBoxLayout;
    sys_layout->setSpacing(8);
    sys_layout->setContentsMargins(10, 15, 10, 10);
    
    auto addStatusRow = [&](const QString& label, const QString& value, bool ok) {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* l = new QLabel(label);
        l->setStyleSheet("font-size: 12px; color: #8b949e;");
        row->addWidget(l);
        row->addStretch();
        QLabel* v = new QLabel(QString::fromUtf8(ok ? "● " : "○ ") + value);
        v->setProperty("statusRole", "inline");
        v->setProperty("statusLevel", ok ? "ok" : "error");
        row->addWidget(v);
        sys_layout->addLayout(row);
    };
    
    addStatusRow(QString::fromUtf8("真空系统"), QString::fromUtf8("正常"), true);
    addStatusRow(QString::fromUtf8("闸板阀门"), QString::fromUtf8("已关闭"), true);
    addStatusRow(QString::fromUtf8("安全互锁"), QString::fromUtf8("正常"), true);
    addStatusRow(QString::fromUtf8("设备通信"), QString::fromUtf8("已连接"), true);
    
    sys_group->setLayout(sys_layout);
    layout->addWidget(sys_group);
    
    layout->addStretch();
    
    // --- 紧急停止按钮 ---
    QPushButton* stop_all_btn = new QPushButton(QString::fromUtf8("紧急停止"));
    stop_all_btn->setMinimumHeight(48);
    stop_all_btn->setCursor(Qt::PointingHandCursor);
    stop_all_btn->setStyleSheet(R"(
        QPushButton {
            background-color: #da3633;
            color: #ffffff;
            font-weight: bold;
            font-size: 15px;
            border: none;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #f85149;
        }
        QPushButton:pressed {
            background-color: #b62324;
        }
    )");
    connect(stop_all_btn, &QPushButton::clicked, this, &MainControllerGUI::onEmergencyStop);
    layout->addWidget(stop_all_btn);
    
    return panel;
}

QWidget* MainControllerGUI::createLogPanel() {
    QWidget* log_widget = new QWidget;
    log_widget->setStyleSheet("background-color: #0e1724; border-top: 1px solid #1b2b3f;");
    log_widget->setMinimumHeight(160);
    log_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    
    QVBoxLayout* main_layout = new QVBoxLayout(log_widget);
    main_layout->setContentsMargins(12, 8, 12, 8);
    main_layout->setSpacing(6);
    
    // 日志标题栏
    QHBoxLayout* title_layout = new QHBoxLayout;
    QLabel* log_title = new QLabel(QString::fromUtf8("系统日志"));
    log_title->setStyleSheet("color: #29d6ff; font-size: 14px; font-weight: 700; padding: 4px 0;");
    title_layout->addWidget(log_title);
    title_layout->addStretch();
    
    QPushButton* clear_btn = new QPushButton(QString::fromUtf8("清空日志"));
    clear_btn->setMinimumHeight(28);
    clear_btn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #1b2b3f, stop:1 #223b52);
            border: 1px solid #2a85ff;
            border-radius: 6px;
            color: #d7dde8;
            font-size: 12px;
            font-weight: 600;
            padding: 4px 12px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #22314a, stop:1 #2e4c70);
            border-color: #47ffd4;
        }
        QPushButton:pressed {
            background: #1a2536;
            border-color: #1f6feb;
        }
    )");
    title_layout->addWidget(clear_btn);
    main_layout->addLayout(title_layout);
    
    // 日志内容
    status_text_ = new QTextEdit;
    status_text_->setReadOnly(true);
    connect(clear_btn, &QPushButton::clicked, status_text_, &QTextEdit::clear);
    
    main_layout->addWidget(status_text_);
    
    return log_widget;
}

void MainControllerGUI::updateStatus() {
    try {
        if (large_stroke_) {
            double pos = large_stroke_->getCurrentPosition();
            large_stroke_position_display_->setText(QString::number(pos, 'f', 2));
        }
        
        if (six_dof_) {
            std::vector<double> pos_array = six_dof_->getCurrentPosition();
            for (size_t i = 0; i < 6 && i < pos_array.size(); ++i) {
                six_dof_position_displays_[i]->setText(QString::number(pos_array[i], 'f', 3));
            }
        }
        
        if (auxiliary_support_) {
            std::vector<double> pos_array = auxiliary_support_->getCurrentPosition();
            for (size_t i = 0; i < 5 && i < pos_array.size(); ++i) {
                if(auxiliary_position_displays_[i])
                    auxiliary_position_displays_[i]->setText(QString::number(pos_array[i], 'f', 3));
            }
        }

        if (backlight_system_) {
            std::vector<double> pos_array = backlight_system_->getCurrentPosition();
            for (size_t i = 0; i < 6 && i < pos_array.size(); ++i) {
                if(backlight_position_displays_[i])
                    backlight_position_displays_[i]->setText(QString::number(pos_array[i], 'f', 3));
            }
        }
        
        if (vacuum_) {
            double pressure = vacuum_->getPressure();
            if (vacuum_pressure_display_) {
                vacuum_pressure_display_->setText(QString::number(pressure, 'f', 2));
            }
            bool pumping = false;
            bool valve_open = false;
            double target = 0.0;
            try { pumping = vacuum_->isPumping(); } catch (...) {}
            try { valve_open = vacuum_->isGateValveOpen(); } catch (...) {}
            try { target = vacuum_->getTargetPressure(); } catch (...) {}
            
            if (vacuum_target_input_ && !vacuum_target_input_->hasFocus()) {
                vacuum_target_input_->setText(QString::number(target, 'f', 2));
            }
            
            auto updateBadge = [](QLabel* label, bool ok, const QString& text) {
                if (!label) return;
                label->setText(text);
                label->setProperty("statusLevel", ok ? "ok" : "warn");
                label->style()->unpolish(label);
                label->style()->polish(label);
                label->update();
            };
            updateBadge(vacuum_pump_status_label_, pumping, pumping ? QString::fromUtf8("运行") : QString::fromUtf8("停止"));
            updateBadge(vacuum_valve_status_label_, valve_open, valve_open ? QString::fromUtf8("已打开") : QString::fromUtf8("已关闭"));
        }
        
    } catch (const std::exception &e) {
        QString error_msg = "Status update failed: ";
        error_msg += e.what();
        logMessage(error_msg);
    }
}

void MainControllerGUI::onLargeStrokeMove(bool forward) {
    logMessage(QString("Large stroke move: %1").arg(forward ? "forward" : "backward"));
}

void MainControllerGUI::onSixDofMove(const QString& axis, bool positive) {
    logMessage(QString("Six DOF move: %1 %2").arg(axis).arg(positive ? "+" : "-"));
}

bool MainControllerGUI::checkInterlockStatus() {
    try {
        if (interlock_) {
            return interlock_->isInterlockActive();
        }
    } catch (const std::exception &e) {
        logMessage(QString("Interlock check failed: %1").arg(e.what()));
    }
    return false;
}

void MainControllerGUI::moveSixDofAxis(int axis, double increment) {
    logMessage(QString("[DEBUG] moveSixDofAxis called: axis=%1, increment=%2").arg(axis).arg(increment));
    try {
        if (six_dof_ && axis >= 0 && axis < 6) {
            // Get current position
            std::vector<double> current_pos = six_dof_->getCurrentPosition();
            logMessage(QString("[DEBUG] SixDof getCurrentPosition returned %1 values").arg(current_pos.size()));
            
            if (current_pos.size() < 6) return;

            // Create new position
            std::vector<double> new_pos = current_pos;
            new_pos[axis] += increment;
            
            logMessage(QString("[DEBUG] SixDof sending moveToPosition for axis %1: %2 -> %3")
                .arg(axis).arg(current_pos[axis]).arg(new_pos[axis]));
            
            // Send move command
            six_dof_->moveToPosition(new_pos);
            
            logMessage(QString("[OK] Six DOF axis %1 moved by %2").arg(axis).arg(increment));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Six DOF move failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onSixDofStop() {
    logMessage("[DEBUG] onSixDofStop called");
    try {
        if (six_dof_) {
            six_dof_->stop();
            logMessage("[OK] Six DOF stopped");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Six DOF stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onSixDofReset() {
    logMessage("[DEBUG] onSixDofReset called");
    try {
        if (six_dof_) {
            six_dof_->reset();
            logMessage("[OK] Six DOF reset");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Six DOF reset failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::moveLargeStroke(double increment) {
    logMessage(QString("[DEBUG] moveLargeStroke called: increment=%1").arg(increment));
    try {
        if (large_stroke_) {
            double current_pos = large_stroke_->getCurrentPosition();
            logMessage(QString("[DEBUG] LargeStroke getCurrentPosition returned: %1").arg(current_pos));
            double new_pos = current_pos + increment;
            logMessage(QString("[DEBUG] LargeStroke sending moveToPosition: %1 -> %2").arg(current_pos).arg(new_pos));
            large_stroke_->moveToPosition(new_pos);
            logMessage(QString("[OK] Large stroke moved by %1 to %2").arg(increment).arg(new_pos));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Large stroke move failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onLargeStrokeStop() {
    logMessage("[DEBUG] onLargeStrokeStop called");
    try {
        if (large_stroke_) {
            large_stroke_->stop();
            logMessage("[OK] Large stroke stopped");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Large stroke stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onLargeStrokeReset() {
    logMessage("[DEBUG] onLargeStrokeReset called");
    try {
        if (large_stroke_) {
            large_stroke_->reset();
            logMessage("[OK] Large stroke reset");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Large stroke reset failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::moveLargeStrokeAbsolute(double position) {
    logMessage(QString("[DEBUG] moveLargeStrokeAbsolute called: position=%1").arg(position));
    try {
        if (large_stroke_) {
            logMessage(QString("[DEBUG] LargeStroke sending moveToPosition (absolute): %1").arg(position));
            large_stroke_->moveToPosition(position);
            logMessage(QString("[OK] Large stroke moved to absolute position %1").arg(position));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Large stroke absolute move failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onLargeStrokeHome() {
    logMessage("[DEBUG] onLargeStrokeHome called");
    try {
        if (large_stroke_) {
            logMessage("[DEBUG] LargeStroke sending moveToPosition(0) for homing");
            large_stroke_->moveToPosition(0);
            logMessage("[OK] Large stroke homing to zero position");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Large stroke home failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onValveOpen() {
    logMessage("[DEBUG] onValveOpen called");
    try {
        if (vacuum_) {
            vacuum_->openGateValve();
            logMessage("[OK] Gate valve open command sent");
        } else {
            logMessage("[WARN] Vacuum device not available");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Gate valve open failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onValveClose() {
    logMessage("[DEBUG] onValveClose called");
    try {
        if (vacuum_) {
            vacuum_->closeGateValve();
            logMessage("[OK] Gate valve close command sent");
        } else {
            logMessage("[WARN] Vacuum device not available");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Gate valve close failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onSixDofInputClear() {
    logMessage("[DEBUG] onSixDofInputClear called");
    for (int i = 0; i < 6; ++i) {
        if (six_dof_input_edits_[i]) {
            six_dof_input_edits_[i]->setText("0");
        }
    }
    logMessage("[OK] Six DOF input cleared");
}

void MainControllerGUI::onSixDofInputExecute() {
    logMessage("[DEBUG] onSixDofInputExecute called");
    try {
        if (six_dof_) {
            std::vector<double> target_pos(6);
            for (int i = 0; i < 6; ++i) {
                target_pos[i] = six_dof_input_edits_[i] ? six_dof_input_edits_[i]->text().toDouble() : 0.0;
                logMessage(QString("[DEBUG] Input axis %1: %2").arg(i).arg(target_pos[i]));
            }
            six_dof_->moveToPosition(target_pos);
            logMessage("[OK] Six DOF move to input position executed");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Six DOF input execute failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onSixDofInputZero() {
    logMessage("[DEBUG] onSixDofInputZero called");
    try {
        if (six_dof_) {
            std::vector<double> zero_pos(6, 0.0);
            logMessage("[DEBUG] Six DOF sending moveToPosition to all zeros");
            six_dof_->moveToPosition(zero_pos);
            logMessage("[OK] Six DOF homing to zero position");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Six DOF home failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::moveAuxiliaryAxis(int axis, double increment) {
    logMessage(QString("[DEBUG] moveAuxiliaryAxis called: axis=%1, increment=%2").arg(axis).arg(increment));
    try {
        if (auxiliary_support_) {
            std::vector<double> current_pos = auxiliary_support_->getCurrentPosition();
            logMessage(QString("[DEBUG] Auxiliary getCurrentPosition returned %1 values").arg(current_pos.size()));
            if (axis < static_cast<int>(current_pos.size())) {
                std::vector<double> new_pos = current_pos;
                new_pos[axis] += increment;
                logMessage(QString("[DEBUG] Auxiliary sending moveToPosition for axis %1: %2 -> %3")
                    .arg(axis).arg(current_pos[axis]).arg(new_pos[axis]));
                auxiliary_support_->moveToPosition(new_pos);
                logMessage(QString("[OK] Auxiliary axis %1 moved by %2").arg(axis).arg(increment));
            }
        }
    } catch (const std::exception &e) {
        logMessage(QString("Auxiliary move failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::moveBacklightAxis(int axis, double increment) {
    logMessage(QString("[DEBUG] moveBacklightAxis called: axis=%1, increment=%2").arg(axis).arg(increment));
    try {
        if (backlight_system_) {
            std::vector<double> current_pos = backlight_system_->getCurrentPosition();
            logMessage(QString("[DEBUG] Backlight getCurrentPosition returned %1 values").arg(current_pos.size()));
            if (axis < static_cast<int>(current_pos.size())) {
                std::vector<double> new_pos = current_pos;
                new_pos[axis] += increment;
                logMessage(QString("[DEBUG] Backlight sending moveToPosition for axis %1: %2 -> %3")
                    .arg(axis).arg(current_pos[axis]).arg(new_pos[axis]));
                backlight_system_->moveToPosition(new_pos);
                logMessage(QString("[OK] Backlight axis %1 moved by %2").arg(axis).arg(increment));
            }
        }
    } catch (const std::exception &e) {
        logMessage(QString("Backlight move failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::logMessage(const QString& message) {
    QString timestamped = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + ": " + message;
    // 输出到控制台
    std::cout << "[MainController] " << timestamped.toStdString() << std::endl;
    // 输出到GUI日志面板
    if (status_text_) {
        status_text_->append(timestamped);
    }
}

void MainControllerGUI::updateDeviceStatus() {
    updateStatus();
}

void MainControllerGUI::onVacuumStartPumping() {
    logMessage("[DEBUG] onVacuumStartPumping called");
    try {
        if (vacuum_) {
            vacuum_->startPumping();
            logMessage("[OK] Vacuum pumping started");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Vacuum start failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onVacuumStopPumping() {
    logMessage("[DEBUG] onVacuumStopPumping called");
    try {
        if (vacuum_) {
            vacuum_->stopPumping();
            logMessage("[OK] Vacuum pumping stopped");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Vacuum stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onVacuumVent() {
    logMessage("[DEBUG] onVacuumVent called");
    try {
        if (vacuum_) {
            vacuum_->vent();
            logMessage("[OK] Vacuum chamber vented");
        }
    } catch (const std::exception &e) {
        logMessage(QString("Vacuum vent failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onEmergencyStop() {
    logMessage("[DEBUG] onEmergencyStop called");
    try {
        if (interlock_) {
            interlock_->emergencyStop();
            logMessage("[ALERT] EMERGENCY STOP activated!");
        }
    } catch (const std::exception &e) {
        showError(QString("Emergency stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onInterlockToggle() {
    logMessage("[DEBUG] onInterlockToggle called");
    try {
        if (interlock_) {
            bool current_state = checkInterlockStatus();
            logMessage(QString("[DEBUG] Interlock current state: %1, toggling to: %2")
                .arg(current_state ? "active" : "inactive")
                .arg(!current_state ? "active" : "inactive"));
            interlock_->setInterlockActive(!current_state);
            logMessage(QString("[OK] Interlock %1").arg(!current_state ? "activated" : "deactivated"));
        }
    } catch (const std::exception &e) {
        showError(QString("Interlock toggle failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onAuxiliaryStop(int axis) {
    logMessage(QString("[DEBUG] onAuxiliaryStop called: axis=%1").arg(axis));
    try {
        if (auxiliary_support_) {
            auxiliary_support_->stop();
            logMessage(QString("[OK] Auxiliary axis %1 stopped").arg(axis));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Auxiliary stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onAuxiliaryReset(int axis) {
    logMessage(QString("[DEBUG] onAuxiliaryReset called: axis=%1").arg(axis));
    try {
        if (auxiliary_support_) {
            auxiliary_support_->reset();
            logMessage(QString("[OK] Auxiliary axis %1 reset").arg(axis));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Auxiliary reset failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onBacklightStop(int axis) {
    logMessage(QString("[DEBUG] onBacklightStop called: axis=%1").arg(axis));
    try {
        if (backlight_system_) {
            backlight_system_->stop();
            logMessage(QString("[OK] Backlight axis %1 stopped").arg(axis));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Backlight stop failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::onBacklightReset(int axis) {
    logMessage(QString("[DEBUG] onBacklightReset called: axis=%1").arg(axis));
    try {
        if (backlight_system_) {
            backlight_system_->reset();
            logMessage(QString("[OK] Backlight axis %1 reset").arg(axis));
        }
    } catch (const std::exception &e) {
        logMessage(QString("Backlight reset failed: %1").arg(e.what()));
    }
}

void MainControllerGUI::showError(const QString& error) {
    QMessageBox::critical(this, "Error", error);
    logMessage("ERROR: " + error);
}

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    
    // Check for simulation flag
    bool simulation_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--sim") {
            simulation_mode = true;
            break;
        }
    }

    MainControllerGUI window(simulation_mode);
    window.show();
    
    return app.exec();
}
