void SixDofDevice::sixMoveZero() {
    check_state("sixMoveZero");
    log_event("Move to zero started");

    if (limit_fault_latched_.load()) {
        Tango::Except::throw_exception(
            "LIMIT_FAULT_LATCHED",
            "Limit fault is latched; please run reset before moving.",
            "SixDofDevice::sixMoveZero");
    }
    
    if (sim_mode_) {
        axis_pos_.fill(0.0);
        dire_pos_.fill(0.0);
        six_freedom_pose_.fill(0.0);
        lim_org_state_.fill(0);  // At origin
        result_value_ = 0;
    } else {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot move to zero in real mode.",
                "SixDofDevice::sixMoveZero");
        }
        try {
            // 循环调用每个轴的带参 moveZero 命令
            INFO_STREAM << "[DEBUG] sixMoveZero: sending moveZero to motion controller for all 6 axes" << endl;
            for (int axis = 0; axis < NUM_AXES; ++axis) {
                Tango::DeviceData data_in;
                data_in << static_cast<Tango::DevShort>(axis);
                motion->command_inout("moveZero", data_in);
                INFO_STREAM << "[DEBUG] sixMoveZero: axis " << axis << " moveZero command sent" << endl;
            }
            set_state(Tango::MOVING);
            INFO_STREAM << "[DEBUG] sixMoveZero: all axes moveZero commands sent successfully" << endl;
        } catch (Tango::DevFailed &e) {
            result_value_ = 1;
            ERROR_STREAM << "[DEBUG] sixMoveZero: moveZero command failed - " << e.errors[0].desc << endl;
            Tango::Except::re_throw_exception(e, "API_ProxyError", 
                "Failed to move to zero", "SixDofDevice::sixMoveZero");
        }
    }
    log_event("Move to zero completed");
}