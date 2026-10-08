#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../commands.h"
#include "../press_queue.h"

namespace esphome
{
    namespace philips_coffee_machine
    {
        namespace philips_action_button
        {
            /**
             * @brief Executable actions. Select actions only select the type.
             * Make actions select the type and press play.
             *
             */
            enum Action
            {
                SELECT_COFFEE = 0,
                MAKE_COFFEE,
                SELECT_ESPRESSO,
                MAKE_ESPRESSO,
                SELECT_HOT_WATER,
                MAKE_HOT_WATER,
                SELECT_STEAM,
                MAKE_STEAM,
                SELECT_CAPPUCCINO,
                MAKE_CAPPUCCINO,
                SELECT_LATTE,
                MAKE_LATTE,
                SELECT_AMERICANO,
                MAKE_AMERICANO,
                SELECT_BEAN,
                SELECT_SIZE,
                SELECT_MILK,
                SELECT_AQUA_CLEAN,
                SELECT_CALC_CLEAN,
                PLAY_PAUSE,
            };

            /**
             * @brief Emulates (a) button press(es) the way the display makes them.
             *
             */
            class ActionButton : public button::Button, public Component
            {
            public:
                void dump_config() override;

                /**
                 * @brief Set the action used by this ActionButton.
                 *
                 * @param action Action to use
                 */
                void set_action(Action action)
                {
                    action_ = action;
                };

                /**
                 * @brief Reference to the queue through which presses reach the mainboard
                 *
                 * @param press_queue press queue of the controller
                 */
                void set_press_queue(PressQueue *press_queue)
                {
                    press_queue_ = press_queue;
                };

                /**
                 * @brief Sets the long press parameter on this button component.
                 *
                 * @param long_press True if a long press should be executed, false otherwise
                 */
                void set_long_press(bool long_press)
                {
                    should_long_press_ = long_press;
                }

            private:
                /**
                 * @brief Executes button press
                 *
                 */
                void press_action() override;

                /**
                 * @brief Queues the button press(es) for this action
                 *
                 * @param duration how long the (first) button is held in ms
                 */
                void perform_action(uint32_t duration);

                /// @brief Action used by this Button
                Action action_;
                /// @brief queue through which presses reach the mainboard
                PressQueue *press_queue_;
                /// @brief true if the button should be held for LONG_PRESS_DURATION
                bool should_long_press_ = false;
            };
        } // namespace philips_action_button
    }     // namespace philips_coffee_machine
} // namespace esphome