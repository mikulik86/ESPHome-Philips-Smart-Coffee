#include "esphome/core/log.h"
#include "philips_coffee_machine.h"

// Bytes moved per read; the loop keeps reading until nothing is waiting
#define FORWARD_CHUNK_SIZE size_t(64)
// Upper bound on chunks per direction and loop, so a flooded bus cannot starve other components
#define MAX_FORWARD_CHUNKS 8

namespace esphome
{
    namespace philips_coffee_machine
    {

        static const char *TAG = "philips_coffee_machine";

        void PhilipsCoffeeMachine::setup()
        {
            power_pin_->setup();
            power_pin_->pin_mode(gpio::FLAG_OUTPUT);
            power_pin_->digital_write(initial_pin_state_);

            // By default the ESP32 uart driver holds received bytes until a whole message has arrived
            // (rx full threshold or rx timeout), which delays every message by its own length on each hop.
            // The display reacts to delays that small. Handing over every byte lets it be forwarded at once.
            // This is a no-op on platforms without an rx full threshold, such as the ESP8266.
            if (low_latency_rx_)
            {
                display_uart_component_->set_rx_full_threshold(1);
                mainboard_uart_component_->set_rx_full_threshold(1);
            }
        }

        void PhilipsCoffeeMachine::loop()
        {
            uint32_t loop_start = micros();
            if (bridge_stats_)
            {
                uint32_t now = millis();
                if (stats_loops_ > 0)
                    stats_max_gap_ = std::max(stats_max_gap_, now - stats_last_loop_);
                stats_last_loop_ = now;
                stats_loops_++;
                stats_max_display_waiting_ = std::max(stats_max_display_waiting_, size_t(display_uart_.available()));
                stats_max_mainboard_waiting_ = std::max(stats_max_mainboard_waiting_, size_t(mainboard_uart_.available()));
            }

            // Forward bytes as soon as they are available instead of waiting for complete messages.
            // Everything that has arrived is passed on in this loop, so nothing queues up between loops.
            forward_display_to_mainboard_();
            forward_mainboard_to_display_();

            // Publish power state if required as long as the display is requesting messages
            if (millis() - last_message_from_display_time_ > POWER_STATE_TIMEOUT)
            {
#ifdef USE_SWITCH
                // Update power switches
                for (philips_power_switch::Power *power_switch : power_switches_)
                    power_switch->update_state(false);
#endif

#ifdef USE_TEXT_SENSOR
                // Update status sensors
                for (philips_status_sensor::StatusSensor *status_sensor : status_sensors_)
                    status_sensor->set_state_off();
#endif
            }
            else
            {
#ifdef USE_SWITCH
                // Update power switches
                for (philips_power_switch::Power *power_switch : power_switches_)
                    power_switch->update_state(true);
#endif
            }

            if (bridge_stats_)
            {
                stats_max_loop_us_ = std::max(stats_max_loop_us_, micros() - loop_start);
                if (millis() - stats_start_ >= BRIDGE_STATS_INTERVAL)
                    log_bridge_stats_();
            }
        }

        void PhilipsCoffeeMachine::forward_display_to_mainboard_()
        {
            size_t available = display_uart_.available();
            if (available == 0)
                return;
            last_message_from_display_time_ = millis();

            // Check if a action button is currently performing a long press
            bool long_pressing = false;
#ifdef USE_BUTTON
            for (philips_action_button::ActionButton *button : action_buttons_)
            {
                if (button->is_long_pressing())
                {
                    long_pressing = true;
                    break;
                }
            }
#endif

            uint8_t buffer[FORWARD_CHUNK_SIZE];
            for (int chunk = 0; chunk < MAX_FORWARD_CHUNKS && available > 0; chunk++)
            {
                size_t size = std::min(available, FORWARD_CHUNK_SIZE);
                display_uart_.read_array(buffer, size);

                // Drop whole display frames while an action button is injecting a long press.
                // The decision is made at each frame header so frames are never cut in half.
                size_t forward = 0;
                for (size_t i = 0; i < size; i++)
                {
                    if (buffer[i] == message_header[0])
                        drop_display_frame_ = long_pressing;
                    if (!drop_display_frame_)
                        buffer[forward++] = buffer[i];
                }
                if (forward > 0)
                    mainboard_uart_.write_array(buffer, forward);

                available = display_uart_.available();
            }
        }

        void PhilipsCoffeeMachine::forward_mainboard_to_display_()
        {
            uint8_t buffer[FORWARD_CHUNK_SIZE];
            size_t available = mainboard_uart_.available();
            for (int chunk = 0; chunk < MAX_FORWARD_CHUNKS && available > 0; chunk++)
            {
                size_t size = std::min(available, FORWARD_CHUNK_SIZE);
                mainboard_uart_.read_array(buffer, size);

                // Pass the bytes on first, then parse them
                display_uart_.write_array(buffer, size);
                for (size_t i = 0; i < size; i++)
                    feed_mainboard_byte_(buffer[i]);

                available = mainboard_uart_.available();
            }
        }

        void PhilipsCoffeeMachine::feed_mainboard_byte_(uint8_t byte)
        {
            // A header byte always starts a new frame: payload and checksum bytes are 6-bit values
            if (byte == message_header[0])
                mainboard_frame_length_ = 0;
            else if (mainboard_frame_length_ == 0 || mainboard_frame_length_ >= MAINBOARD_FRAME_SIZE)
                return;

            mainboard_frame_[mainboard_frame_length_++] = byte;

            if (mainboard_frame_length_ == 2 && byte != message_header[1])
                mainboard_frame_length_ = 0;
            else if (mainboard_frame_length_ == MAINBOARD_FRAME_SIZE)
                process_mainboard_frame_();
        }

        void PhilipsCoffeeMachine::process_mainboard_frame_()
        {
            // Only process duplicate messages (crude checksum alternative)
            // TODO: figure out how the checksum is calculated and only parse valid messages
            if (std::equal(mainboard_frame_ + 17, mainboard_frame_ + 19, std::begin(last_mainboard_message_checksum_)))
            {
                last_message_from_mainboard_time_ = millis();
#ifdef USE_TEXT_SENSOR
                // Update status sensors
                for (philips_status_sensor::StatusSensor *status_sensor : status_sensors_)
                    status_sensor->update_status(mainboard_frame_);

#ifdef USE_NUMBER
                // Update beverage settings
                for (philips_beverage_setting::BeverageSetting *beverage_setting : beverage_settings_)
                    beverage_setting->update_status(mainboard_frame_);
#endif
#endif
            }
            // retain last checksum for comparison with next checksum
            std::copy_n(mainboard_frame_ + 17, 2, last_mainboard_message_checksum_);
        }

        void PhilipsCoffeeMachine::log_bridge_stats_()
        {
            uint32_t now = millis();
            ESP_LOGI(TAG, "Bridge: %u loops in %u ms, longest gap between loops %u ms, longest loop %u us, most bytes waiting: display %u, mainboard %u",
                     (unsigned)stats_loops_, (unsigned)(now - stats_start_), (unsigned)stats_max_gap_, (unsigned)stats_max_loop_us_,
                     (unsigned)stats_max_display_waiting_, (unsigned)stats_max_mainboard_waiting_);
            stats_start_ = now;
            stats_loops_ = 0;
            stats_max_gap_ = 0;
            stats_max_loop_us_ = 0;
            stats_max_display_waiting_ = 0;
            stats_max_mainboard_waiting_ = 0;
        }

        void PhilipsCoffeeMachine::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Philips Coffee Machine");
            ESP_LOGCONFIG(TAG, "  Low latency rx: %s", YESNO(low_latency_rx_));
            ESP_LOGCONFIG(TAG, "  Bridge stats: %s", YESNO(bridge_stats_));
            // The UART settings are checked when the configuration is validated (FINAL_VALIDATE_SCHEMA)
        }

    } // namespace philips_coffee_machine
} // namespace esphome
