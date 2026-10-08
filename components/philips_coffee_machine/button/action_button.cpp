#include "esphome/core/log.h"
#include "action_button.h"

namespace esphome
{
    namespace philips_coffee_machine
    {
        namespace philips_action_button
        {

            static const char *const TAG = "philips-action-button";

            void ActionButton::dump_config()
            {
                LOG_BUTTON("", "Philips Action Button", this);
            }

            void ActionButton::press_action()
            {
                perform_action(should_long_press_ ? LONG_PRESS_DURATION : SHORT_PRESS_DURATION);
            }

            void ActionButton::perform_action(uint32_t duration)
            {
                // Presses are queued and performed one after another, with the button released in between,
                // so make actions press the beverage button and then play/pause like a person would.
                auto action = action_;
                // Coffee
                if (action == SELECT_COFFEE || action == MAKE_COFFEE)
                {
                    press_queue_->add(command_press_1, duration);
                    if (action == SELECT_COFFEE)
                        return;
                    action = PLAY_PAUSE;
                }

                // Espresso
                if (action == SELECT_ESPRESSO || action == MAKE_ESPRESSO)
                {
                    press_queue_->add(command_press_2, duration);
                    if (action == SELECT_ESPRESSO)
                        return;
                    action = PLAY_PAUSE;
                }

                // Hot water
                if (action == SELECT_HOT_WATER || action == MAKE_HOT_WATER)
                {
                    press_queue_->add(command_press_3, duration);
                    if (action == SELECT_HOT_WATER)
                        return;
                    action = PLAY_PAUSE;
                }

#ifdef PHILIPS_EP2220
                // Steam
                if (action == SELECT_STEAM || action == MAKE_STEAM)
                {
                    press_queue_->add(command_press_4, duration);
                    if (action == SELECT_STEAM)
                        return;
                    action = PLAY_PAUSE;
                }
#endif
#ifdef PHILIPS_EP2235
                // Cappuccino
                if (action == SELECT_CAPPUCCINO || action == MAKE_CAPPUCCINO)
                {
                    press_queue_->add(command_press_4, duration);
                    if (action == SELECT_CAPPUCCINO)
                        return;
                    action = PLAY_PAUSE;
                }
#endif
#ifdef PHILIPS_EP3243
                // Latte
                if (action == SELECT_LATTE || action == MAKE_LATTE)
                {
                    press_queue_->add(command_press_4, duration);
                    if (action == SELECT_LATTE)
                        return;
                    action = PLAY_PAUSE;
                }

                // Americano
                if (action == SELECT_AMERICANO || action == MAKE_AMERICANO)
                {
                    press_queue_->add(command_press_5, duration);
                    if (action == SELECT_AMERICANO)
                        return;
                    action = PLAY_PAUSE;
                }

                // Cappuccino
                if (action == SELECT_CAPPUCCINO || action == MAKE_CAPPUCCINO)
                {
                    press_queue_->add(command_press_6, duration);
                    if (action == SELECT_CAPPUCCINO)
                        return;
                    action = PLAY_PAUSE;
                }
#endif
                // press/play or subsequent press/play
                if (action == PLAY_PAUSE)
                    press_queue_->add(command_press_play_pause, duration);
                else if (action == SELECT_BEAN)
                    // bean button
                    press_queue_->add(command_press_bean, duration);
                else if (action == SELECT_SIZE)
                    // size button
                    press_queue_->add(command_press_size, duration);
#if defined(PHILIPS_EP3243)
                else if (action == SELECT_MILK)
                    // milk button
                    press_queue_->add(command_press_milk, duration);
#endif
                else if (action == SELECT_AQUA_CLEAN)
                    // aqua clean button
                    press_queue_->add(command_press_aqua_clean, duration);
                else if (action == SELECT_CALC_CLEAN)
                    // calc clean button
                    press_queue_->add(command_press_calc_clean, duration);
                else
                    ESP_LOGE(TAG, "Invalid Action provided!");
            }
        } // namespace philips_action_button
    }     // namespace philips_coffee_machine
} // namespace esphome
