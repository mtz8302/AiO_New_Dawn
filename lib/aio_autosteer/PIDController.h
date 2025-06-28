#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

class PIDController {
private:
    float kp;
    float output;
    float outputLimit;
    
public:
    PIDController() : kp(1.0f), output(0.0f), outputLimit(100.0f) {}
    
    void setKp(float gain) { kp = gain; }
    void setOutputLimit(float limit) { outputLimit = limit; }
    
    float compute(float setpoint, float actual) {
        float error = setpoint - actual;
        output = kp * error;
        
        // Limit output
        if (output > outputLimit) output = outputLimit;
        if (output < -outputLimit) output = -outputLimit;
        
        return output;
    }
    
    float getOutput() const { return output; }
};

#endif