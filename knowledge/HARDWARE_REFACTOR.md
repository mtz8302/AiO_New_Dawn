⏺ Hardware Management Refactoring Plan

  Phase 1: Analysis and Documentation

  Goal: Understand current hardware usage and define clear ownership

  1. Create Hardware Ownership Matrix
    - Document every pin used in the system
    - Identify which class currently configures each pin
    - Identify which class actually uses each pin
    - Note any pins accessed by multiple classes
    - Document shared resources (ADC, PWM, I2C, SPI)
  2. Identify Conflicts
    - List pins configured in multiple places
    - Note timing dependencies
    - Document any dynamic reconfiguration needs

  Phase 2: Refactor HardwareManager

  Goal: Transform HardwareManager into a hardware definition service

  1. Remove Pin Initialization
    - Comment out all pinMode() calls in HardwareManager::initializePins()
    - Keep pin definitions and getter methods
    - Keep shared resource management (PWM frequency, etc.)
  2. Add Documentation
    - Document which class owns each pin
    - Add ownership comments to each pin definition
  3. Create Pin Groups
  // Pin definitions grouped by owner
  // ADProcessor pins
  static constexpr uint8_t WAS_SENSOR_PIN = A15;
  static constexpr uint8_t KICKOUT_A_PIN = A12;

  // MotorDriver pins  
  static constexpr uint8_t PWM1_PIN = 3;
  static constexpr uint8_t PWM2_PIN = 4;

  Phase 3: Update Pin Owners

  Goal: Each module initializes its own pins

  1. ADProcessor
    - Already initializes its pins - verify complete ownership
    - Remove any redundant initialization
  2. Motor Drivers
    - PWMMotorDriver: Initialize PWM1/PWM2/SLEEP pins
    - DanfossMotorDriver: Initialize its specific pins
    - KeyaCANDriver: Initialize any CAN-related pins
  3. EncoderProcessor
    - Initialize KICKOUT_A/KICKOUT_D when encoder enabled
    - Properly release pins when disabled
  4. Other Modules
    - SerialManager: Own serial pins
    - I2CManager: Own I2C pins
    - CANManager: Own CAN pins

  Phase 4: Shared Resource Coordination

  Goal: Manage resources used by multiple modules

  1. ADC Configuration
    - Create ADC configuration registry
    - Allow modules to request ADC settings
    - Coordinate conflicts (e.g., resolution, averaging)
  2. PWM Management
    - Keep centralized PWM frequency setting
    - Add PWM channel allocation
  3. Interrupt Management
    - Track which pins have interrupts attached
    - Prevent conflicts on shared interrupt pins

  Phase 5: Testing and Validation

  Goal: Ensure refactoring doesn't break functionality

  1. Unit Testing
    - Test each module initializes correctly
    - Verify no pin conflicts
    - Test dynamic reconfiguration (encoder ↔ pressure)
  2. Integration Testing
    - Full system startup
    - Mode switching
    - All motor types
    - All sensor configurations
  3. Documentation Update
    - Update system architecture docs
    - Create hardware ownership diagram
    - Document any breaking changes

  Implementation Order

  1. Phase 1 first (analysis) - 1-2 hours
  2. Phase 2 & 3 together - 2-3 hours
  3. Phase 4 - 1-2 hours
  4. Phase 5 - 1-2 hours

  Risks and Mitigation

  - Risk: Breaking existing functionality
    - Mitigation: Extensive testing, gradual rollout
  - Risk: Missing pin initializations
    - Mitigation: Comprehensive ownership matrix
  - Risk: Timing issues during startup
    - Mitigation: Document and enforce init order

  Benefits After Implementation

  - Clear ownership prevents conflicts
  - Easier to debug hardware issues
  - More modular and testable code
  - Easier to port to different hardware
  - Dynamic reconfiguration becomes reliable