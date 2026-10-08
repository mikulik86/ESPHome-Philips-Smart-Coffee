#pragma once

#include <deque>
#include <vector>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// How long a regular button press is held. Real presses on the display last roughly 50 to 250 ms.
#define SHORT_PRESS_DURATION 120
// How long a long press is held
#define LONG_PRESS_DURATION 3500
// Pause after each press, so the mainboard sees the button being released before the next one
#define PRESS_RELEASE_DURATION 100
// A press that could not start within this time (the display is not sending anything) is dropped
#define PRESS_START_TIMEOUT 1000
// Upper bound on queued presses
#define MAX_QUEUED_PRESSES 8

namespace esphome
{
    namespace philips_coffee_machine
    {
        /**
         * @brief Emulates button presses the way the display makes them.
         *
         * While a button is held, the display sends the button's message in place of every status request,
         * at its own polling rate. Presses are therefore not written to the mainboard directly: while one is
         * held, the bridge replaces every message from the display with the button's message. Writing a burst
         * of messages instead makes the mainboard treat the button as bouncing and lock it out for a while,
         * including the physical button on the display.
         */
        class PressQueue
        {
        public:
            /**
             * @brief Queues a button press
             *
             * @param message message the display sends while the button is held
             * @param duration how long the button is held in ms
             */
            void add(const std::vector<uint8_t> &message, uint32_t duration)
            {
                if (queue_.size() >= MAX_QUEUED_PRESSES)
                {
                    ESP_LOGW("philips_press", "Too many queued button presses, dropping one");
                    return;
                }
                queue_.push_back(Press{message, duration, millis()});
            }

            /**
             * @brief Whether a press is being held or waiting
             */
            bool busy() const
            {
                return !queue_.empty();
            }

            /**
             * @brief Advances the press timing. Called on every loop and before every display message.
             *
             * @param now current time in ms
             */
            void update(uint32_t now)
            {
                if (state_ == State::HOLDING && now - phase_start_ >= queue_.front().duration)
                {
                    state_ = State::RELEASING;
                    phase_start_ = now;
                }
                if (state_ == State::RELEASING && now - phase_start_ >= PRESS_RELEASE_DURATION)
                {
                    queue_.pop_front();
                    state_ = State::WAITING;
                    // The next press may only now start, so its start timeout begins now
                    if (!queue_.empty())
                        queue_.front().waiting_since = now;
                }
                // A press can only start with a message from the display. Drop presses that waited too long,
                // e.g. while the machine is off, instead of performing them unexpectedly later.
                while (state_ == State::WAITING && !queue_.empty() && now - queue_.front().waiting_since > PRESS_START_TIMEOUT)
                {
                    ESP_LOGW("philips_press", "Dropped a button press: the display is not sending anything");
                    queue_.pop_front();
                    if (!queue_.empty())
                        queue_.front().waiting_since = now;
                }
            }

            /**
             * @brief Called at the start of every message from the display
             *
             * @param now current time in ms
             * @return the message to send instead of the display's message, or nullptr to forward the display's message
             */
            const std::vector<uint8_t> *on_display_message(uint32_t now)
            {
                update(now);
                if (state_ == State::WAITING && !queue_.empty())
                {
                    state_ = State::HOLDING;
                    phase_start_ = now;
                }
                return state_ == State::HOLDING ? &queue_.front().message : nullptr;
            }

        private:
            struct Press
            {
                std::vector<uint8_t> message;
                uint32_t duration;
                uint32_t waiting_since;
            };

            enum class State
            {
                WAITING,
                HOLDING,
                RELEASING
            };

            std::deque<Press> queue_;
            State state_ = State::WAITING;
            uint32_t phase_start_ = 0;
        };

    } // namespace philips_coffee_machine
} // namespace esphome
