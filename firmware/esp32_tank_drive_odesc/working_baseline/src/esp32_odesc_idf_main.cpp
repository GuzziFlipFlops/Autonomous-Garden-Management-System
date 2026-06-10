#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "tank-drive";

static constexpr const char *AP_SSID = "ODESC-MOTOR";

static constexpr uart_port_t ODRIVE_UART = UART_NUM_2;
static constexpr int UART_BAUD = 115200;
static constexpr int UART_RX_PIN = 17;  // ESP32 RX <- ODESC GPIO1 TX
static constexpr int UART_TX_PIN = 16;  // ESP32 TX -> ODESC GPIO2 RX
static constexpr int AXIS_STATE_IDLE = 1;
static constexpr int AXIS_STATE_SENSORLESS_CONTROL = 5;
static constexpr int CONTROL_MODE_VELOCITY = 2;
static constexpr int INPUT_MODE_PASSTHROUGH = 1;
static constexpr int MOTOR = 0;
static constexpr float DEFAULT_ODRIVE_SPEED_TURNS_PER_SEC = 5.0f;
static constexpr float DEFAULT_ODRIVE_MAX_SPEED_TURNS_PER_SEC = 15.0f;
static constexpr float DEFAULT_ODRIVE_CURRENT_LIMIT_A = 10.0f;
static constexpr float SENSORLESS_RAMP_CURRENT_A = 4.0f;
static constexpr float SENSORLESS_RAMP_VEL_ERAD_S = 219.91f;
static constexpr float SENSORLESS_RAMP_ACCEL_ERAD_S2 = 80.0f;
static constexpr float SENSORLESS_RAMP_TIME_S = 0.8f;

static constexpr int LEFT_PWM_FWD_PIN = 14;
static constexpr int LEFT_PWM_REV_PIN = 12;
static constexpr int RIGHT_PWM_FWD_PIN = 26;
static constexpr int RIGHT_PWM_REV_PIN = 27;

static constexpr adc_channel_t LEFT_CURRENT_A_CH = ADC_CHANNEL_6;   // GPIO34
static constexpr adc_channel_t LEFT_CURRENT_B_CH = ADC_CHANNEL_7;   // GPIO35
static constexpr adc_channel_t RIGHT_CURRENT_A_CH = ADC_CHANNEL_4;  // GPIO32
static constexpr adc_channel_t RIGHT_CURRENT_B_CH = ADC_CHANNEL_5;  // GPIO33

static constexpr ledc_channel_t LEFT_PWM_FWD_CH = LEDC_CHANNEL_0;
static constexpr ledc_channel_t LEFT_PWM_REV_CH = LEDC_CHANNEL_1;
static constexpr ledc_channel_t RIGHT_PWM_FWD_CH = LEDC_CHANNEL_2;
static constexpr ledc_channel_t RIGHT_PWM_REV_CH = LEDC_CHANNEL_3;

static constexpr int TANK_PWM_FREQ_HZ = 20000;
static constexpr int TANK_PWM_MAX_DUTY = 1023;
static constexpr float DEFAULT_SPEED_PERCENT = 35.0f;
static constexpr float RAMP_PERCENT_PER_SEC = 70.0f;
static constexpr TickType_t SERVICE_TICKS = pdMS_TO_TICKS(20);

enum class OdriveCommand {
    None,
    Start,
    Stop,
    SetSpeed,
};

struct AppState {
    float speed_percent = DEFAULT_SPEED_PERCENT;
    float target_left = 0.0f;
    float target_right = 0.0f;
    float output_left = 0.0f;
    float output_right = 0.0f;
    bool emergency_stop = false;
    bool odrive_running = false;
    float odrive_speed = DEFAULT_ODRIVE_SPEED_TURNS_PER_SEC;
    int odrive_direction = 1;
    float odrive_signed_speed = 0.0f;
    float odrive_duration = 0.0f;
    float odrive_current_limit = DEFAULT_ODRIVE_CURRENT_LIMIT_A;
    float odrive_max_speed = DEFAULT_ODRIVE_MAX_SPEED_TURNS_PER_SEC;
    int64_t odrive_run_until_ms = 0;
    int64_t odrive_last_keepalive_ms = 0;
    int64_t estop_last_stop_ms = 0;
    OdriveCommand pending_odrive_command = OdriveCommand::None;
    int64_t last_ramp_ms = 0;
    int64_t last_adc_ms = 0;
    int left_current_a_raw = 0;
    int left_current_b_raw = 0;
    int right_current_a_raw = 0;
    int right_current_b_raw = 0;
    char last_action[96] = "boot";
};

static AppState state;
static SemaphoreHandle_t state_mutex;
static SemaphoreHandle_t uart_mutex;
static httpd_handle_t http_server;
static adc_oneshot_unit_handle_t adc1_handle;

static int64_t now_ms() {
    return esp_timer_get_time() / 1000;
}

static float clamp_float(float value, float low, float high) {
    return std::min(high, std::max(low, value));
}

static void init_odrive_uart() {
    uart_config_t config = {};
    config.baud_rate = UART_BAUD;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(ODRIVE_UART, 1024, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(ODRIVE_UART, &config));
    ESP_ERROR_CHECK(uart_set_pin(ODRIVE_UART, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "ODESC UART%d TX GPIO%d RX GPIO%d baud=%d", ODRIVE_UART, UART_TX_PIN, UART_RX_PIN, UART_BAUD);
}

static void odrive_write(const char *fmt, ...) {
    char command[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(command, sizeof(command), fmt, args);
    va_end(args);

    char line[208];
    snprintf(line, sizeof(line), "%s\n", command);
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
    uart_write_bytes(ODRIVE_UART, line, strlen(line));
    uart_wait_tx_done(ODRIVE_UART, pdMS_TO_TICKS(40));
    xSemaphoreGive(uart_mutex);
}

static void set_ledc_duty(ledc_channel_t channel, int duty) {
    duty = std::max(0, std::min(TANK_PWM_MAX_DUTY, duty));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
}

static void set_bts7960_pair(ledc_channel_t forward_channel, ledc_channel_t reverse_channel, float signed_percent) {
    signed_percent = clamp_float(signed_percent, -100.0f, 100.0f);
    int duty = static_cast<int>(std::round(std::abs(signed_percent) * TANK_PWM_MAX_DUTY / 100.0f));

    if (signed_percent > 0.0f) {
        set_ledc_duty(forward_channel, duty);
        set_ledc_duty(reverse_channel, 0);
    } else if (signed_percent < 0.0f) {
        set_ledc_duty(forward_channel, 0);
        set_ledc_duty(reverse_channel, duty);
    } else {
        set_ledc_duty(forward_channel, 0);
        set_ledc_duty(reverse_channel, 0);
    }
}

static void apply_drive_pwm(float left_percent, float right_percent) {
    set_bts7960_pair(LEFT_PWM_FWD_CH, LEFT_PWM_REV_CH, left_percent);
    set_bts7960_pair(RIGHT_PWM_FWD_CH, RIGHT_PWM_REV_CH, right_percent);
}

static void stop_drive_immediate() {
    apply_drive_pwm(0.0f, 0.0f);
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.target_left = 0.0f;
    state.target_right = 0.0f;
    state.output_left = 0.0f;
    state.output_right = 0.0f;
    snprintf(state.last_action, sizeof(state.last_action), "stopped");
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "stopped");
}

static void init_drive_io() {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num = LEDC_TIMER_0;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.freq_hz = TANK_PWM_FREQ_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    struct PwmPin {
        int gpio;
        ledc_channel_t channel;
    };
    const PwmPin pins[] = {
        {LEFT_PWM_FWD_PIN, LEFT_PWM_FWD_CH},
        {LEFT_PWM_REV_PIN, LEFT_PWM_REV_CH},
        {RIGHT_PWM_FWD_PIN, RIGHT_PWM_FWD_CH},
        {RIGHT_PWM_REV_PIN, RIGHT_PWM_REV_CH},
    };

    for (const PwmPin &pin : pins) {
        ledc_channel_config_t channel = {};
        channel.gpio_num = pin.gpio;
        channel.speed_mode = LEDC_LOW_SPEED_MODE;
        channel.channel = pin.channel;
        channel.timer_sel = LEDC_TIMER_0;
        channel.duty = 0;
        channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
    }

    adc_oneshot_unit_init_cfg_t adc_unit_config = {};
    adc_unit_config.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_config, &adc1_handle));

    adc_oneshot_chan_cfg_t adc_channel_config = {};
    adc_channel_config.atten = ADC_ATTEN_DB_12;
    adc_channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LEFT_CURRENT_A_CH, &adc_channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LEFT_CURRENT_B_CH, &adc_channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, RIGHT_CURRENT_A_CH, &adc_channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, RIGHT_CURRENT_B_CH, &adc_channel_config));

    stop_drive_immediate();
    ESP_LOGI(TAG, "PWM ready: left GPIO%d/%d right GPIO%d/%d", LEFT_PWM_FWD_PIN, LEFT_PWM_REV_PIN,
             RIGHT_PWM_FWD_PIN, RIGHT_PWM_REV_PIN);
}

static float ramp_toward(float current, float target, float max_delta) {
    if (current < target) {
        return std::min(target, current + max_delta);
    }
    if (current > target) {
        return std::max(target, current - max_delta);
    }
    return current;
}

static void set_drive_command(const char *command, float speed_percent) {
    speed_percent = clamp_float(speed_percent, 0.0f, 100.0f);

    float left = 0.0f;
    float right = 0.0f;
    if (strcmp(command, "forward") == 0) {
        left = speed_percent;
        right = speed_percent;
    } else if (strcmp(command, "down") == 0 || strcmp(command, "reverse") == 0 || strcmp(command, "back") == 0) {
        left = -speed_percent;
        right = -speed_percent;
    } else if (strcmp(command, "left") == 0) {
        left = -speed_percent;
        right = speed_percent;
    } else if (strcmp(command, "right") == 0) {
        left = speed_percent;
        right = -speed_percent;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (state.emergency_stop) {
        state.target_left = 0.0f;
        state.target_right = 0.0f;
        snprintf(state.last_action, sizeof(state.last_action), "emergency stop active");
        xSemaphoreGive(state_mutex);
        return;
    }
    state.speed_percent = speed_percent;
    state.target_left = left;
    state.target_right = right;
    snprintf(state.last_action, sizeof(state.last_action), "%s %.0f%%", command, speed_percent);
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "drive %s %.0f%% target L/R %.1f %.1f", command, speed_percent, left, right);
}

static void poll_current_sense(int64_t now) {
    int left_a = 0;
    int left_b = 0;
    int right_a = 0;
    int right_b = 0;
    if (adc1_handle) {
        adc_oneshot_read(adc1_handle, LEFT_CURRENT_A_CH, &left_a);
        adc_oneshot_read(adc1_handle, LEFT_CURRENT_B_CH, &left_b);
        adc_oneshot_read(adc1_handle, RIGHT_CURRENT_A_CH, &right_a);
        adc_oneshot_read(adc1_handle, RIGHT_CURRENT_B_CH, &right_b);
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.left_current_a_raw = left_a;
    state.left_current_b_raw = left_b;
    state.right_current_a_raw = right_a;
    state.right_current_b_raw = right_b;
    state.last_adc_ms = now;
    xSemaphoreGive(state_mutex);
}

static void send_odrive_stop_commands() {
    odrive_write("w axis0.requested_state %d", AXIS_STATE_IDLE);
    odrive_write("w axis0.controller.input_vel 0");
    odrive_write("v %d 0 0", MOTOR);
}

static void configure_odrive_runtime(float max_speed, float current_limit) {
    odrive_write("w config.enable_uart 1");
    odrive_write("w config.uart_baudrate %d", UART_BAUD);
    odrive_write("w config.brake_resistance 0");
    odrive_write("w axis0.config.enable_step_dir 0");
    odrive_write("w axis0.config.enable_watchdog 0");
    odrive_write("w axis0.motor.config.current_lim %.3f", current_limit);
    odrive_write("w axis0.motor.config.direction 1");
    odrive_write("w axis0.controller.config.control_mode %d", CONTROL_MODE_VELOCITY);
    odrive_write("w axis0.controller.config.input_mode %d", INPUT_MODE_PASSTHROUGH);
    odrive_write("w axis0.controller.config.vel_limit %.3f", max_speed);
    odrive_write("w axis0.config.sensorless_ramp.current %.3f", SENSORLESS_RAMP_CURRENT_A);
    odrive_write("w axis0.config.sensorless_ramp.vel %.3f", SENSORLESS_RAMP_VEL_ERAD_S);
    odrive_write("w axis0.config.sensorless_ramp.accel %.3f", SENSORLESS_RAMP_ACCEL_ERAD_S2);
    odrive_write("w axis0.config.sensorless_ramp.ramp_time %.3f", SENSORLESS_RAMP_TIME_S);
    odrive_write("w axis0.config.sensorless_ramp.finish_on_vel 1");
    odrive_write("w axis0.config.sensorless_ramp.finish_on_distance 0");
    odrive_write("w axis0.config.sensorless_ramp.finish_on_enc_idx 0");
}

static void clear_odrive_errors() {
    odrive_write("w axis0.requested_state %d", AXIS_STATE_IDLE);
    odrive_write("w axis0.error 0");
    odrive_write("w axis0.motor.error 0");
    odrive_write("w axis0.controller.error 0");
    odrive_write("w axis0.sensorless_estimator.error 0");
}

static void request_odrive_start(float speed, int direction, float duration, float current_limit, float max_speed) {
    direction = direction >= 0 ? 1 : -1;
    max_speed = clamp_float(max_speed, 0.5f, 80.0f);
    current_limit = clamp_float(current_limit, 1.0f, 60.0f);
    speed = clamp_float(std::abs(speed), 0.0f, max_speed);

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (!state.emergency_stop) {
        state.odrive_speed = speed;
        state.odrive_direction = direction;
        state.odrive_signed_speed = speed * direction;
        state.odrive_duration = std::max(0.0f, duration);
        state.odrive_current_limit = current_limit;
        state.odrive_max_speed = max_speed;
        state.pending_odrive_command = OdriveCommand::Start;
        snprintf(state.last_action, sizeof(state.last_action), "ODESC start %.2f turn/s", state.odrive_signed_speed);
    } else {
        snprintf(state.last_action, sizeof(state.last_action), "emergency stop active");
    }
    xSemaphoreGive(state_mutex);
}

static void request_odrive_speed(float speed, int direction) {
    direction = direction >= 0 ? 1 : -1;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (!state.emergency_stop) {
        speed = clamp_float(std::abs(speed), 0.0f, state.odrive_max_speed);
        state.odrive_speed = speed;
        state.odrive_direction = direction;
        state.odrive_signed_speed = speed * direction;
        state.pending_odrive_command = OdriveCommand::SetSpeed;
        snprintf(state.last_action, sizeof(state.last_action), "ODESC speed %.2f turn/s", state.odrive_signed_speed);
    } else {
        snprintf(state.last_action, sizeof(state.last_action), "emergency stop active");
    }
    xSemaphoreGive(state_mutex);
}

static void request_odrive_stop() {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.pending_odrive_command = OdriveCommand::Stop;
    state.odrive_running = false;
    state.odrive_signed_speed = 0.0f;
    state.odrive_run_until_ms = 0;
    snprintf(state.last_action, sizeof(state.last_action), "ODESC stop requested");
    xSemaphoreGive(state_mutex);
}

static void activate_emergency_stop() {
    stop_drive_immediate();
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.emergency_stop = true;
    state.odrive_running = false;
    state.odrive_signed_speed = 0.0f;
    state.odrive_run_until_ms = 0;
    state.pending_odrive_command = OdriveCommand::Stop;
    state.estop_last_stop_ms = 0;
    snprintf(state.last_action, sizeof(state.last_action), "emergency stop active");
    xSemaphoreGive(state_mutex);
}

static void enable_outputs() {
    stop_drive_immediate();
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.emergency_stop = false;
    state.pending_odrive_command = OdriveCommand::None;
    snprintf(state.last_action, sizeof(state.last_action), "enabled");
    xSemaphoreGive(state_mutex);
}

static void service_task(void *) {
    while (true) {
        AppState snapshot;
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        snapshot = state;
        xSemaphoreGive(state_mutex);

        int64_t now = now_ms();
        int64_t elapsed_ms = snapshot.last_ramp_ms > 0 ? now - snapshot.last_ramp_ms : 20;
        float max_delta = RAMP_PERCENT_PER_SEC * std::max<int64_t>(1, elapsed_ms) / 1000.0f;
        float left = snapshot.emergency_stop ? 0.0f : ramp_toward(snapshot.output_left, snapshot.target_left, max_delta);
        float right = snapshot.emergency_stop ? 0.0f : ramp_toward(snapshot.output_right, snapshot.target_right, max_delta);

        apply_drive_pwm(left, right);

        xSemaphoreTake(state_mutex, portMAX_DELAY);
        state.output_left = left;
        state.output_right = right;
        state.last_ramp_ms = now;
        xSemaphoreGive(state_mutex);

        if (now - snapshot.last_adc_ms >= 250) {
            poll_current_sense(now);
        }

        vTaskDelay(SERVICE_TICKS);
    }
}

static void odrive_task(void *) {
    while (true) {
        AppState snapshot;
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        snapshot = state;
        state.pending_odrive_command = OdriveCommand::None;
        xSemaphoreGive(state_mutex);

        int64_t now = now_ms();

        if (snapshot.emergency_stop) {
            if (now - snapshot.estop_last_stop_ms >= 250) {
                send_odrive_stop_commands();
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.estop_last_stop_ms = now_ms();
                state.odrive_running = false;
                state.odrive_signed_speed = 0.0f;
                state.odrive_run_until_ms = 0;
                xSemaphoreGive(state_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (snapshot.pending_odrive_command == OdriveCommand::Start) {
            clear_odrive_errors();
            configure_odrive_runtime(snapshot.odrive_max_speed, snapshot.odrive_current_limit);
            odrive_write("w axis0.controller.input_vel %.3f", snapshot.odrive_signed_speed);
            odrive_write("w axis0.requested_state %d", AXIS_STATE_SENSORLESS_CONTROL);
            odrive_write("v %d %.3f 0", MOTOR, snapshot.odrive_signed_speed);

            xSemaphoreTake(state_mutex, portMAX_DELAY);
            state.odrive_running = true;
            state.odrive_last_keepalive_ms = now_ms();
            state.odrive_run_until_ms = snapshot.odrive_duration > 0.0f
                                            ? now_ms() + static_cast<int64_t>(snapshot.odrive_duration * 1000.0f)
                                            : 0;
            xSemaphoreGive(state_mutex);
        } else if (snapshot.pending_odrive_command == OdriveCommand::SetSpeed) {
            odrive_write("w axis0.controller.input_vel %.3f", snapshot.odrive_signed_speed);
            odrive_write("v %d %.3f 0", MOTOR, snapshot.odrive_signed_speed);
        } else if (snapshot.pending_odrive_command == OdriveCommand::Stop) {
            send_odrive_stop_commands();
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            state.odrive_running = false;
            state.odrive_signed_speed = 0.0f;
            state.odrive_run_until_ms = 0;
            xSemaphoreGive(state_mutex);
        }

        xSemaphoreTake(state_mutex, portMAX_DELAY);
        snapshot = state;
        xSemaphoreGive(state_mutex);
        now = now_ms();

        if (snapshot.odrive_running && snapshot.odrive_run_until_ms > 0 && now >= snapshot.odrive_run_until_ms) {
            send_odrive_stop_commands();
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            state.odrive_running = false;
            state.odrive_signed_speed = 0.0f;
            state.odrive_run_until_ms = 0;
            snprintf(state.last_action, sizeof(state.last_action), "ODESC timed stop");
            xSemaphoreGive(state_mutex);
        } else if (snapshot.odrive_running && now - snapshot.odrive_last_keepalive_ms >= 250) {
            odrive_write("w axis0.controller.input_vel %.3f", snapshot.odrive_signed_speed);
            odrive_write("u %d", MOTOR);
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            state.odrive_last_keepalive_ms = now_ms();
            xSemaphoreGive(state_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static float query_float(httpd_req_t *req, const char *key, float fallback) {
    char query[160] = "";
    char value[32] = "";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, key, value, sizeof(value)) == ESP_OK) {
        return strtof(value, nullptr);
    }
    return fallback;
}

static void query_text(httpd_req_t *req, const char *key, char *out, size_t out_size, const char *fallback) {
    if (out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s", fallback);
    char query[160] = "";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, key, out, out_size);
    }
}

static esp_err_t send_status_json(httpd_req_t *req) {
    AppState s;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    s = state;
    xSemaphoreGive(state_mutex);

    char body[1200];
    snprintf(body, sizeof(body),
             "{\"speed\":%.1f,\"target_left\":%.1f,\"target_right\":%.1f,"
             "\"output_left\":%.1f,\"output_right\":%.1f,"
             "\"emergency_stop\":%s,\"odrive_running\":%s,"
             "\"odrive_speed\":%.3f,\"odrive_direction\":%d,\"odrive_signed_speed\":%.3f,"
             "\"odrive_duration\":%.3f,\"odrive_current_limit\":%.3f,\"odrive_max_speed\":%.3f,"
             "\"left_current_a_raw\":%d,\"left_current_b_raw\":%d,"
             "\"right_current_a_raw\":%d,\"right_current_b_raw\":%d,"
             "\"last_action\":\"%s\"}",
             s.speed_percent, s.target_left, s.target_right, s.output_left, s.output_right,
             s.emergency_stop ? "true" : "false", s.odrive_running ? "true" : "false",
             s.odrive_speed, s.odrive_direction, s.odrive_signed_speed,
             s.odrive_duration, s.odrive_current_limit, s.odrive_max_speed,
             s.left_current_a_raw, s.left_current_b_raw, s.right_current_a_raw, s.right_current_b_raw,
             s.last_action);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

static const char INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tank Drive Test</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:#eef3f4;color:#152028}
header{background:#152028;color:white;padding:12px 14px;display:grid;grid-template-columns:1fr auto;gap:10px;align-items:center}
h1{font-size:20px;margin:0}
main{max-width:560px;margin:0 auto;padding:14px}
label{display:grid;gap:8px;font-size:14px;color:#44535c;margin-bottom:12px}
input,select{width:100%;font-size:17px;padding:10px;border:1px solid #b8c3ca;border-radius:6px;background:white}
input[type=range]{width:100%;padding:0}
button{width:100%;min-height:58px;border:0;border-radius:6px;font-size:17px;font-weight:bold;background:#1e6f5c;color:white;touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none;-webkit-tap-highlight-color:transparent}
button.stop{background:#b42318}
button.secondary{background:#324651}
button.estop{background:#b42318;min-width:136px}
button.enable{background:#1e6f5c;min-width:136px}
.grid,.buttons{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}
.pad{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:12px auto 16px}
.pad button{min-height:68px}
.empty{visibility:hidden}
.status{background:white;border:1px solid #d7e0e5;border-radius:6px;padding:10px;font-family:monospace;font-size:13px;white-space:pre-wrap;user-select:text}
.bar{display:grid;grid-template-columns:1fr 72px;gap:10px;align-items:center}
h2{font-size:16px;margin:18px 0 10px}
@media(max-width:520px){header,.grid,.buttons{grid-template-columns:1fr}.estop,.enable{min-width:0}}
</style>
</head>
<body>
<header><h1>Tank Drive</h1><button id="estopBtn" class="estop" onclick="toggleEstop()">Emergency Stop</button></header>
<main>
<h2>Drivetrain</h2>
<label>Drive Speed
  <div class="bar"><input id="speed" type="range" min="0" max="100" step="1" value="35"><output id="speedOut">35%</output></div>
</label>
<section class="pad">
  <div class="empty"></div><button data-cmd="forward">Forward</button><div class="empty"></div>
  <button data-cmd="left">Left</button><button class="stop" id="stopBtn">Stop</button><button data-cmd="right">Right</button>
  <div class="empty"></div><button data-cmd="down">Down</button><div class="empty"></div>
</section>
<h2>ODESC</h2>
<section class="grid">
  <label>Speed, turn/s<input id="odriveSpeed" type="number" min="0" max="80" step="0.1" value="5.0"></label>
  <label>Direction<select id="odriveDir"><option value="1">Forward</option><option value="-1">Reverse</option></select></label>
  <label>Run Time, seconds<input id="odriveDuration" type="number" min="0" max="3600" step="0.1" value="0"></label>
  <label>Current Limit, A<input id="odriveCurrent" type="number" min="1" max="60" step="0.1" value="10.0"></label>
  <label>Max Speed, turn/s<input id="odriveMax" type="number" min="0.5" max="80" step="0.1" value="15.0"></label>
</section>
<section class="buttons">
  <button onclick="startOdrive()">Start ODESC</button>
  <button class="secondary" onclick="setOdriveSpeed()">Apply Speed</button>
  <button class="stop" onclick="stopOdrive()">Stop ODESC</button>
</section>
<div id="status" class="status">loading...</div>
</main>
<script>
function val(id){return document.getElementById(id).value}
function setStatus(s){
  const b=document.getElementById('estopBtn');
  b.textContent=s.emergency_stop?'Enable':'Emergency Stop';
  b.className=s.emergency_stop?'enable':'estop';
  document.getElementById('status').textContent =
    'emergency_stop: '+s.emergency_stop+'\n'+
    'target L/R: '+s.target_left+' / '+s.target_right+' %\n'+
    'output L/R: '+s.output_left+' / '+s.output_right+' %\n'+
    'ODESC running: '+s.odrive_running+'\n'+
    'ODESC target: '+s.odrive_signed_speed+' turn/s\n'+
    'left ADC: '+s.left_current_a_raw+' / '+s.left_current_b_raw+'\n'+
    'right ADC: '+s.right_current_a_raw+' / '+s.right_current_b_raw+'\n'+
    'last: '+s.last_action;
}
function api(path){return fetch(path,{cache:'no-store'}).then(r=>r.json()).then(setStatus).catch(e=>{document.getElementById('status').textContent='request failed: '+e})}
function drive(cmd){api('/api/drive?cmd='+cmd+'&speed='+val('speed'))}
function stopNow(){api('/api/stop')}
function toggleEstop(){api(document.getElementById('estopBtn').textContent==='Enable'?'/api/enable':'/api/estop')}
function startOdrive(){api('/api/odrive_start?speed='+val('odriveSpeed')+'&dir='+val('odriveDir')+'&duration='+val('odriveDuration')+'&current='+val('odriveCurrent')+'&max='+val('odriveMax'))}
function setOdriveSpeed(){api('/api/odrive_speed?speed='+val('odriveSpeed')+'&dir='+val('odriveDir'))}
function stopOdrive(){api('/api/odrive_stop')}
function refresh(){api('/api/status')}
const slider=document.getElementById('speed');
slider.addEventListener('input',()=>{document.getElementById('speedOut').textContent=slider.value+'%'});
document.addEventListener('contextmenu', e=>e.preventDefault());
document.addEventListener('selectstart', e=>e.preventDefault());
document.querySelectorAll('[data-cmd]').forEach(btn=>{
  const cmd=btn.dataset.cmd;
  btn.addEventListener('pointerdown', e=>{e.preventDefault(); btn.setPointerCapture(e.pointerId); drive(cmd)});
  btn.addEventListener('pointerup', e=>{e.preventDefault(); if(btn.hasPointerCapture(e.pointerId))btn.releasePointerCapture(e.pointerId); stopNow()});
  btn.addEventListener('pointercancel', e=>{e.preventDefault(); stopNow()});
  btn.addEventListener('lostpointercapture', ()=>stopNow());
});
document.getElementById('stopBtn').addEventListener('pointerdown', e=>{e.preventDefault(); stopNow()});
setInterval(refresh, 1000);
refresh();
</script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_handler(httpd_req_t *req) {
    const char *uri = req->uri;
    if (strncmp(uri, "/api/estop", 10) == 0) {
        activate_emergency_stop();
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/enable", 11) == 0) {
        enable_outputs();
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/drive", 10) == 0) {
        char command[24];
        query_text(req, "cmd", command, sizeof(command), "stop");
        set_drive_command(command, query_float(req, "speed", state.speed_percent));
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/stop", 9) == 0) {
        stop_drive_immediate();
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/odrive_start", 17) == 0) {
        request_odrive_start(query_float(req, "speed", state.odrive_speed),
                             static_cast<int>(query_float(req, "dir", state.odrive_direction)),
                             query_float(req, "duration", state.odrive_duration),
                             query_float(req, "current", state.odrive_current_limit),
                             query_float(req, "max", state.odrive_max_speed));
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/odrive_speed", 17) == 0) {
        request_odrive_speed(query_float(req, "speed", state.odrive_speed),
                             static_cast<int>(query_float(req, "dir", state.odrive_direction)));
        return send_status_json(req);
    }
    if (strncmp(uri, "/api/odrive_stop", 16) == 0) {
        request_odrive_stop();
        return send_status_json(req);
    }
    return send_status_json(req);
}

static void start_web_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    httpd_uri_t index_uri = {};
    index_uri.uri = "/";
    index_uri.method = HTTP_GET;
    index_uri.handler = index_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &index_uri));

    httpd_uri_t api_uri = {};
    api_uri.uri = "/api/*";
    api_uri.method = HTTP_GET;
    api_uri.handler = api_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &api_uri));

    ESP_LOGI(TAG, "HTTP server started");
}

static void start_wifi_ap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t wifi_config = {};
    snprintf(reinterpret_cast<char *>(wifi_config.ap.ssid), sizeof(wifi_config.ap.ssid), "%s", AP_SSID);
    wifi_config.ap.ssid_len = strlen(AP_SSID);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP active SSID=%s IP=192.168.4.1", AP_SSID);
}

extern "C" void app_main(void) {
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    state_mutex = xSemaphoreCreateMutex();
    uart_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "boot heap=%lu", static_cast<unsigned long>(esp_get_free_heap_size()));

    init_drive_io();
    init_odrive_uart();
    start_wifi_ap();
    start_web_server();
    xTaskCreatePinnedToCore(service_task, "drive_service", 4096, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(odrive_task, "odrive_service", 4096, nullptr, 4, nullptr, 1);
}
