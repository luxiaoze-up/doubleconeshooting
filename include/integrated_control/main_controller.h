#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include <QMainWindow>
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimer>
#include <QDateTime>
#include <QStackedWidget> // Added
#include <memory>
#include <vector>
#include "common/device_interface.h"

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QTabWidget;

class MainControllerGUI : public QMainWindow {
    Q_OBJECT

private:
    // Device interfaces (Abstract)
    std::unique_ptr<Common::IMotionDevice> large_stroke_; // Renamed from target_transport_
    std::unique_ptr<Common::IMultiAxisDevice> six_dof_;
    std::unique_ptr<Common::IMultiAxisDevice> auxiliary_support_; // New: 5 axes
    std::unique_ptr<Common::IMultiAxisDevice> backlight_system_;  // New: 6 axes
    std::unique_ptr<Common::IVacuumDevice> vacuum_;
    std::unique_ptr<Common::IInterlockService> interlock_;
    
    // GUI components
    QTextEdit* status_text_;
    QTimer* monitor_timer_;
    QStackedWidget* center_stacked_widget_;
    
    // Control displays
    std::vector<QLineEdit*> six_dof_position_displays_;
    std::vector<QLineEdit*> six_dof_input_edits_;  // 六自由度位姿输入框
    std::vector<QLineEdit*> auxiliary_position_displays_; // New
    std::vector<QLineEdit*> backlight_position_displays_; // New
    QLineEdit* large_stroke_position_display_; // Renamed
    QLineEdit* vacuum_pressure_display_;
    QLineEdit* vacuum_target_input_;
    QLabel* vacuum_pump_status_label_;
    QLabel* vacuum_valve_status_label_;
    
    // Control parameters
    double six_dof_step_size_;
    double large_stroke_step_size_; // Renamed
    
public:
    explicit MainControllerGUI(bool simulation_mode = false, QWidget *parent = nullptr);
    virtual ~MainControllerGUI() = default;

private slots:
    void updateStatus();
    void onSixDofMove(const QString& axis, bool positive);
    void onSixDofStop();
    void onSixDofReset();
    
    // Large Stroke Slots
    void onLargeStrokeMove(bool forward);
    void onLargeStrokeStop();
    void onLargeStrokeReset();
    
    void onVacuumStartPumping();
    void onVacuumStopPumping();
    void onVacuumVent();
    void onEmergencyStop();
    void onInterlockToggle();

private:
    void connectDevices(bool simulation_mode);
    void setupUI();
    
    // UI creation methods
    QWidget* createStatusBar();
    QWidget* createLeftSidebar();
    QWidget* createTargetPositioningPage();
    QWidget* createAuxiliarySupportPage();
    QWidget* createBacklightCharacterizationPage();
    QWidget* createVacuumControlPage();
    QWidget* createRightPanel();
    QWidget* createLogPanel();

    // Legacy methods (keeping for now or removing if replaced)
    QWidget* createIRSubsystemTab();
    QWidget* createVacuumSubsystemTab();
    QWidget* createControlTab();
    
    void logMessage(const QString& message);
    void showError(const QString& error);
    void updateDeviceStatus();
    bool checkInterlockStatus();
    
    // Movement methods
    void moveSixDofAxis(int axis, double increment);
    void moveLargeStroke(double increment);
    void moveLargeStrokeAbsolute(double position);
    void moveAuxiliaryAxis(int axis, double increment);
    void moveBacklightAxis(int axis, double increment);
    
    // Six DOF input actions
    void onSixDofInputClear();
    void onSixDofInputExecute();
    void onSixDofInputZero();
    
    // Large stroke actions
    void onLargeStrokeHome();
    void onValveOpen();
    void onValveClose();
    
    // Auxiliary support stop/reset
    void onAuxiliaryStop(int axis);
    void onAuxiliaryReset(int axis);
    
    // Backlight characterization stop/reset
    void onBacklightStop(int axis);
    void onBacklightReset(int axis);
};

#endif // MAIN_CONTROLLER_H
